#include "bsx.h"

#include <ctype.h>
#include <dirent.h>

static void key_of(const char *name, char *out, size_t size) {
    const char *base = strrchr(name, '/');
    base = base ? base + 1 : name;
    size_t n = 0;
    for (; base[n] && base[n] != '.' && n + 1 < size; n++) out[n] = (char)tolower((u8)base[n]);
    out[n] = 0;
}

static void ix_rehash(ResIndex *x) {
    free(x->hash);
    x->hashSize = 64;
    while (x->hashSize < x->count * 2) x->hashSize *= 2;
    x->hash = bx_alloc(sizeof(int) * (size_t)x->hashSize);
    for (int i = 0; i < x->hashSize; i++) x->hash[i] = -1;
    for (int i = 0; i < x->count; i++) {
        char k[128];
        key_of(x->e[i].name, k, sizeof k);
        u32 h = bx_hash(k) & (u32)(x->hashSize - 1);
        while (x->hash[h] >= 0) h = (h + 1) & (u32)(x->hashSize - 1);
        x->hash[h] = i;
    }
}

static void ix_add(ResIndex *x, const char *name, int arc, u32 off, u32 size) {
    if (x->count == x->cap) {
        x->cap = x->cap ? x->cap * 2 : 256;
        x->e = bx_realloc(x->e, sizeof(BsaEntry) * (size_t)x->cap);
    }
    BsaEntry *e = &x->e[x->count++];
    e->name = bx_strdup(name);
    e->arc = arc;
    e->offset = off;
    e->size = size;
}

static void join(char *out, size_t size, char **stack, int depth, const char *name) {
    out[0] = 0;
    for (int i = 0; i < depth; i++) {
        strncat(out, stack[i], size - strlen(out) - 1);
        strncat(out, "/", size - strlen(out) - 1);
    }
    strncat(out, name, size - strlen(out) - 1);
}

static bool read_index(Archives *a, int arc, ResIndex *x) {
    FILE *f = a->f[arc];
    u8 h[16];
    if (fseek(f, 0, SEEK_SET) || fread(h, 1, 16, f) != 16 || memcmp(h, "BSArc", 5)) return false;
    int version = rd16(h + 8), count = rd16(h + 10);
    u32 index = rd32(h + 12);
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    if (index >= (u32)fileSize) return false;
    size_t n = (size_t)fileSize - index;
    u8 *d = bx_alloc(n + 1);
    fseek(f, (long)index, SEEK_SET);
    if (fread(d, 1, n, f) != n) {
        free(d);
        return false;
    }
    int start = x->count;
    char *stack[32];
    int depth = 0;
    char path[512];
    bool ok = false;
    if (version > 1 && (size_t)count * 12 <= n) {
        const u8 *names = d + (size_t)count * 12;
        size_t namesLen = n - (size_t)count * 12;
        ok = true;
        for (int i = 0; i < count && ok; i++) {
            u32 no = rd32(d + 12 * i), off = rd32(d + 12 * i + 4), size = rd32(d + 12 * i + 8);
            if (no >= namesLen || !memchr(names + no, 0, namesLen - no)) {
                ok = false;
                break;
            }
            const char *nm = (const char *)names + no;
            if (!*nm) ok = false;
            else if (nm[0] == '>') {
                if (depth < 32) stack[depth++] = bx_strdup(nm + 1);
            } else if (nm[0] == '<') {
                if (depth) free(stack[--depth]);
            } else if ((u64)off + size > (u64)fileSize) ok = false;
            else {
                join(path, sizeof path, stack, depth, nm);
                ix_add(x, path, arc, off, size);
            }
        }
        while (depth) free(stack[--depth]);
        if (!ok) {
            for (int i = start; i < x->count; i++) free(x->e[i].name);
            x->count = start;
        }
    }
    if (!ok && (size_t)count * 40 <= n) {
        ok = true;
        for (int i = 0; i < count && ok; i++) {
            const u8 *r = d + 40 * i;
            char nm[33];
            memcpy(nm, r, 32);
            nm[32] = 0;
            u32 off = rd32(r + 32), size = rd32(r + 36);
            if (!nm[0]) ok = false;
            else if (nm[0] == '>') {
                if (depth < 32) stack[depth++] = bx_strdup(nm + 1);
            } else if (nm[0] == '<') {
                if (depth) free(stack[--depth]);
            } else if ((u64)off + size > (u64)fileSize) ok = false;
            else {
                join(path, sizeof path, stack, depth, nm);
                ix_add(x, path, arc, off, size);
            }
        }
        while (depth) free(stack[--depth]);
        if (!ok) {
            for (int i = start; i < x->count; i++) free(x->e[i].name);
            x->count = start;
        }
    }
    free(d);
    return ok;
}

static int kind_of(const char *file) {
    char k[128];
    key_of(file, k, sizeof k);
    if (!strncmp(k, "graphics", 8)) return RX_GFX;
    if (!strncmp(k, "voice", 5)) return RX_VOICE;
    if (!strcmp(k, "bgm")) return RX_BGM;
    if (!strcmp(k, "bgs")) return RX_BGS;
    if (!strcmp(k, "se")) return RX_SE;
    if (!strcmp(k, "sesys")) return RX_SYS;
    if (!strcmp(k, "define") || !strcmp(k, "embed")) return RX_DEF;
    return -1;
}

static int name_cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

bool bx_archives_open(Archives *a, const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return false;
    char *names[256];
    int n = 0;
    struct dirent *de;
    while ((de = readdir(d)) && n < 256) {
        size_t l = strlen(de->d_name);
        if (l > 4 && bx_ieq(de->d_name + l - 4, ".bsa") && kind_of(de->d_name) >= 0) names[n++] = bx_strdup(de->d_name);
    }
    closedir(d);
    qsort(names, (size_t)n, sizeof *names, name_cmp);
    for (int i = 0; i < n; i++) {
        if (a->count < BX_ARCS) {
            char path[2048];
            snprintf(path, sizeof path, "%s/%s", dir, names[i]);
            FILE *f = fopen(path, "rb");
            if (f) {
                int arc = a->count++;
                a->f[arc] = f;
                a->path[arc] = bx_strdup(path);
                if (!read_index(a, arc, &a->ix[kind_of(names[i])])) {
                    fclose(f);
                    free(a->path[arc]);
                    a->f[arc] = NULL;
                    a->path[arc] = NULL;
                }
            }
        }
        free(names[i]);
    }
    for (int i = 0; i < RX_COUNT; i++) ix_rehash(&a->ix[i]);
    return a->ix[RX_GFX].count > 0;
}

void bx_archives_close(Archives *a) {
    for (int i = 0; i < a->count; i++) {
        if (a->f[i]) fclose(a->f[i]);
        free(a->path[i]);
    }
    for (int i = 0; i < RX_COUNT; i++) {
        for (int k = 0; k < a->ix[i].count; k++) free(a->ix[i].e[k].name);
        free(a->ix[i].e);
        free(a->ix[i].hash);
    }
    memset(a, 0, sizeof *a);
}

const BsaEntry *bx_res_find(Archives *a, int ix, const char *name) {
    ResIndex *x = &a->ix[ix];
    if (!x->hashSize) return NULL;
    char k[128];
    key_of(name, k, sizeof k);
    u32 h = bx_hash(k) & (u32)(x->hashSize - 1);
    while (x->hash[h] >= 0) {
        char k2[128];
        key_of(x->e[x->hash[h]].name, k2, sizeof k2);
        if (!strcmp(k, k2)) return &x->e[x->hash[h]];
        h = (h + 1) & (u32)(x->hashSize - 1);
    }
    return NULL;
}

static u8 *read_entry(Archives *a, const BsaEntry *e, size_t *size) {
    FILE *f = a->f[e->arc];
    if (!f) return NULL;
    u8 *d = bx_alloc((size_t)e->size + 1);
    if (fseek(f, (long)e->offset, SEEK_SET) || fread(d, 1, e->size, f) != e->size) {
        free(d);
        return NULL;
    }
    if (size) *size = e->size;
    return d;
}

static const BsaEntry *resolve(Archives *a, int ix, const char *name) {
    char cur[128];
    snprintf(cur, sizeof cur, "%s", name);
    for (int hop = 0; hop < 8; hop++) {
        const BsaEntry *e = bx_res_find(a, ix, cur);
        if (!e) return NULL;
        size_t l = strlen(e->name);
        if (!(l > 5 && bx_ieq(e->name + l - 5, ".link"))) return e;
        size_t n;
        u8 *d = read_entry(a, e, &n);
        if (!d) return NULL;
        size_t k = 0;
        while (k < n && k + 1 < sizeof cur && d[k] && d[k] != '\r' && d[k] != '\n') {
            cur[k] = (char)d[k];
            k++;
        }
        cur[k] = 0;
        free(d);
    }
    return NULL;
}

u8 *bx_res_read(Archives *a, int ix, const char *name, size_t *size) {
    const BsaEntry *e = resolve(a, ix, name);
    return e ? read_entry(a, e, size) : NULL;
}

bool bx_res_head(Archives *a, int ix, const char *name, u8 *out, size_t n) {
    const BsaEntry *e = resolve(a, ix, name);
    FILE *f = e ? a->f[e->arc] : NULL;
    return f && e->size >= n && !fseek(f, (long)e->offset, SEEK_SET) && fread(out, 1, n, f) == n;
}

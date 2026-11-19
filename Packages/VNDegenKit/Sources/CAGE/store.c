#include "age.h"

#include <dirent.h>
#include <strings.h>

size_t ag_lzss(const u8 *src, size_t srcLen, u8 *dst, size_t dstLen) {
    u8 win[4096] = { 0 };
    int pos = 0xfee;
    size_t i = 0, o = 0;
    while (i < srcLen && o < dstLen) {
        u8 flags = src[i++];
        for (int b = 0; b < 8 && o < dstLen; b++) {
            if (flags & (1 << b)) {
                if (i >= srcLen) return o;
                u8 c = src[i++];
                dst[o++] = c;
                win[pos] = c;
                pos = (pos + 1) & 0xfff;
            } else {
                if (i + 1 >= srcLen) return o;
                int lo = src[i], hi = src[i + 1];
                i += 2;
                int off = lo | (hi & 0xf0) << 4, len = (hi & 0x0f) + 3;
                for (int k = 0; k < len && o < dstLen; k++) {
                    u8 c = win[(off + k) & 0xfff];
                    dst[o++] = c;
                    win[pos] = c;
                    pos = (pos + 1) & 0xfff;
                }
            }
        }
    }
    return o;
}

static int cmp_name(const void *a, const void *b) { return strcasecmp(*(char *const *)a, *(char *const *)b); }

static char **list_dir(const char *dir, int *count) {
    char real[2048];
    *count = 0;
    if (ag_path_resolve(dir, real, sizeof real) <= 0) return NULL;
    DIR *d = opendir(real);
    if (!d) return NULL;
    char **names = NULL;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        names = ag_realloc(names, sizeof(char *) * (size_t)(n + 1));
        names[n++] = ag_strdup(e->d_name);
    }
    closedir(d);
    qsort(names, (size_t)n, sizeof(char *), cmp_name);
    *count = n;
    return names;
}

static const char *find_name(char **names, int count, const char *name) {
    char **hit = bsearch(&name, names, (size_t)count, sizeof(char *), cmp_name);
    return hit ? *hit : NULL;
}

static bool parse_directory(Catalog *c, const u8 *idx, size_t n, size_t *end) {
    if (n < 4) return false;
    size_t p = 0;
    int ac = (int)rd32(idx);
    p += 4;
    if (ac < 0 || p + (size_t)ac * 0x100 + 4 > n) return false;
    c->arcCount = ac;
    c->arcs = ag_alloc(sizeof(char *) * (size_t)(ac ? ac : 1));
    c->files = ag_alloc(sizeof(FILE *) * (size_t)(ac ? ac : 1));
    for (int i = 0; i < ac; i++, p += 0x100) {
        char nm[0x101];
        memcpy(nm, idx + p, 0x100);
        nm[0x100] = 0;
        c->arcs[i] = ag_strdup(nm);
    }
    int fc = (int)rd32(idx + p);
    p += 4;
    if (fc < 0 || p + (size_t)fc * 0x50 > n) return false;
    c->count = fc;
    c->e = ag_alloc(sizeof(CatEntry) * (size_t)(fc ? fc : 1));
    for (int i = 0; i < fc; i++, p += 0x50) {
        memcpy(c->e[i].name, idx + p, 63);
        c->e[i].arc = (i32)rd32(idx + p + 0x40);
        c->e[i].offset = rd32(idx + p + 0x48);
        c->e[i].size = rd32(idx + p + 0x4c);
    }
    *end = p;
    return true;
}

static bool load_catalog(Store *s, const char *path, bool append, char *err, size_t errSize) {
    size_t size;
    u8 *d = ag_read_file(path, &size);
    if (!d) { snprintf(err, errSize, "can't read %s", path); return false; }
    size_t off = append ? 0x114 : 0x134;
    if (size < off + 4) { free(d); snprintf(err, errSize, "%s is too short", path); return false; }
    u32 unpacked = rd32(d + off - 4), packed = rd32(d + off);
    if (off + 4 + packed > size) { free(d); snprintf(err, errSize, "%s is cut short", path); return false; }
    u8 *idx = ag_alloc(unpacked + 1);
    size_t got = ag_lzss(d + off + 4, packed, idx, unpacked);
    int sel = append ? (int)rd32(d + 0x108) : 0;
    if (sel < 0 || sel > 255) sel = 0;
    Catalog *c = &s->cat[sel];
    size_t end = 0;
    bool ok = parse_directory(c, idx, got, &end);
    if (ok && !append) {
        for (int i = 0; i < 6; i++) {
            s->bankSaved[i] = rd32(d + 0xf0 + i * 4);
            s->bankFull[i] = rd32(d + 0x114 + i * 4);
        }
        if (end + 4 <= got) {
            u32 meta = rd32(idx + end);
            size_t p = end + 4 + meta;
            if (p + 8 <= got) {
                u32 pairs = rd32(idx + p + 4);
                p += 8;
                for (u32 i = 0; i < pairs && p < got; i++) {
                    const char *k = (const char *)idx + p;
                    size_t kl = strnlen(k, got - p);
                    p += kl + 1;
                    if (p >= got) break;
                    const char *v = (const char *)idx + p;
                    size_t vl = strnlen(v, got - p);
                    p += vl + 1;
                    s->settings = ag_realloc(s->settings, sizeof(Setting) * (size_t)(s->settingCount + 1));
                    s->settings[s->settingCount].key = ag_strdup(k);
                    s->settings[s->settingCount].value = ag_strdup(v);
                    s->settingCount++;
                }
            }
        }
    }
    free(idx);
    free(d);
    if (!ok) snprintf(err, errSize, "%s has a broken directory", path);
    return ok;
}

bool store_open(Store *s, const char *dir, char *err, size_t errSize) {
    memset(s, 0, sizeof *s);
    s->dir = ag_strdup(dir);
    char *p = ag_fmt("%s/SYS4INI.BIN", dir);
    bool ok = load_catalog(s, p, false, err, errSize);
    free(p);
    if (!ok) return false;
    p = ag_fmt("%s/patch", dir);
    s->patchNames = list_dir(p, &s->patchCount);
    free(p);
    s->looseNames = list_dir(dir, &s->looseCount);
    for (int i = 0; i < s->looseCount; i++) {
        const char *n = s->looseNames[i];
        size_t l = strlen(n);
        if (l < 4 || strcasecmp(n + l - 4, ".AAI")) continue;
        char *q = ag_fmt("%s/%s", dir, n);
        char e2[256];
        if (!load_catalog(s, q, true, e2, sizeof e2)) ag_log(NULL, "%s", e2);
        free(q);
    }
    return true;
}

void store_close(Store *s) {
    for (int c = 0; c < 256; c++) {
        Catalog *k = &s->cat[c];
        for (int i = 0; i < k->arcCount; i++) {
            if (k->files[i]) fclose(k->files[i]);
            free(k->arcs[i]);
        }
        free(k->arcs);
        free(k->files);
        free(k->e);
    }
    for (int i = 0; i < s->settingCount; i++) {
        free(s->settings[i].key);
        free(s->settings[i].value);
    }
    free(s->settings);
    for (int i = 0; i < s->patchCount; i++) free(s->patchNames[i]);
    free(s->patchNames);
    for (int i = 0; i < s->looseCount; i++) free(s->looseNames[i]);
    free(s->looseNames);
    free(s->dir);
    memset(s, 0, sizeof *s);
}

const CatEntry *store_entry(Store *s, u32 id) {
    int sel = (int)(i32)id >> 24;
    if (sel < 0) return NULL;
    Catalog *c = &s->cat[sel];
    u32 i = id & 0xffffff;
    if (!c->e || i >= (u32)c->count) return NULL;
    return &c->e[i];
}

static u8 *read_path(const char *path, size_t *size) { return ag_read_file(path, size); }

static u8 *read_loose(Store *s, const char *name, size_t *size) {
    const char *hit = s->patchCount ? find_name(s->patchNames, s->patchCount, name) : NULL;
    if (hit) {
        char *p = ag_fmt("%s/patch/%s", s->dir, hit);
        u8 *d = read_path(p, size);
        free(p);
        if (d) return d;
    }
    hit = s->looseCount ? find_name(s->looseNames, s->looseCount, name) : NULL;
    if (hit) {
        char *p = ag_fmt("%s/%s", s->dir, hit);
        u8 *d = read_path(p, size);
        free(p);
        if (d) return d;
    }
    return NULL;
}

static u8 *read_archived(Store *s, Catalog *c, const CatEntry *e, size_t *size) {
    if (e->arc < 0 || e->arc >= c->arcCount) return NULL;
    if (!c->files[e->arc]) {
        char *p = ag_fmt("%s/%s", s->dir, c->arcs[e->arc]);
        char real[2048];
        if (ag_path_resolve(p, real, sizeof real) > 0) c->files[e->arc] = fopen(real, "rb");
        free(p);
        if (!c->files[e->arc]) return NULL;
    }
    FILE *f = c->files[e->arc];
    u8 *d = ag_alloc((size_t)e->size + 4);
    if (fseeko(f, (off_t)e->offset, SEEK_SET) != 0 || fread(d, 1, e->size, f) != e->size) { free(d); return NULL; }
    *size = e->size;
    return d;
}

u8 *store_read(Store *s, u32 id, size_t *size) {
    const CatEntry *e = store_entry(s, id);
    if (!e || !e->name[0] || !strcmp(e->name, "@")) return NULL;
    size_t sz = 0;
    u8 *d = read_loose(s, e->name, &sz);
    if (!d) d = read_archived(s, &s->cat[(i32)id >> 24], e, &sz);
    if (d && size) *size = sz;
    return d;
}

i32 store_find(Store *s, const char *name) {
    for (int c = 0; c < 256; c++)
        for (int i = 0; i < s->cat[c].count; i++)
            if (!strcasecmp(s->cat[c].e[i].name, name)) return (i32)((u32)c << 24 | (u32)i);
    return -1;
}

u8 *store_read_name(Store *s, const char *name, size_t *size) {
    size_t sz = 0;
    u8 *d = read_loose(s, name, &sz);
    if (!d) {
        i32 id = store_find(s, name);
        if (id >= 0) d = read_archived(s, &s->cat[id >> 24], store_entry(s, (u32)id), &sz);
    }
    if (d && size) *size = sz;
    return d;
}

const char *store_setting(Store *s, const char *key) {
    const char *v = NULL;
    for (int i = 0; i < s->settingCount; i++)
        if (!strcasecmp(s->settings[i].key, key)) v = s->settings[i].value;
    return v;
}

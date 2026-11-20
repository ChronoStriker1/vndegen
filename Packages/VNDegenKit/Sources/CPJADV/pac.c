#include "pj.h"

#include <ctype.h>
#include <dirent.h>

static u32 hash_name(const char *s) {
    u32 h = 2166136261u;
    for (; *s; s++) h = (h ^ (u32)tolower((u8)*s)) * 16777619u;
    return h;
}

static void rehash(Pac *a) {
    free(a->hash);
    a->hashSize = 256;
    while (a->hashSize < a->count * 2) a->hashSize *= 2;
    a->hash = pj_alloc(sizeof(int) * (size_t)a->hashSize);
    for (int i = 0; i < a->hashSize; i++) a->hash[i] = -1;
    for (int i = 0; i < a->count; i++) {
        u32 h = hash_name(a->e[i].name) & (u32)(a->hashSize - 1);
        while (a->hash[h] >= 0) h = (h + 1) & (u32)(a->hashSize - 1);
        a->hash[h] = i;
    }
}

const PacEntry *pj_pac_find(const Pac *a, const char *name) {
    if (!a->hashSize || !name || !*name) return NULL;
    u32 h = hash_name(name) & (u32)(a->hashSize - 1);
    while (a->hash[h] >= 0) {
        const PacEntry *e = &a->e[a->hash[h]];
        if (pj_ieq(e->name, name)) return e;
        h = (h + 1) & (u32)(a->hashSize - 1);
    }
    return NULL;
}

static bool add_archive(Pac *a, const char *path, PJ *log) {
    if (a->arcs == PJ_ARCS) return false;
    char real[2048];
    if (pj_path_resolve(path, real, sizeof real) < 1) return false;
    FILE *f = fopen(real, "rb");
    if (!f) return false;
    u8 hdr[16];
    if (fread(hdr, 1, 16, f) != 16 || memcmp(hdr, "GAMEDAT PAC", 11)) {
        fclose(f);
        return false;
    }
    int nameLen = hdr[11] == 'K' ? 16 : hdr[11] == '2' ? 32 : 0;
    u32 n = rd32(hdr + 12);
    if (!nameLen || n == 0 || n > 0xfffff) {
        fclose(f);
        return false;
    }
    size_t dirSize = (size_t)n * (size_t)(nameLen + 8);
    u8 *dir = pj_alloc(dirSize);
    if (fread(dir, 1, dirSize, f) != dirSize) {
        free(dir);
        fclose(f);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    u32 base = 16 + (u32)dirSize;
    int arc = a->arcs++;
    a->f[arc] = f;
    for (u32 i = 0; i < n; i++) {
        PacEntry e = {0};
        memcpy(e.name, dir + (size_t)i * (size_t)nameLen, (size_t)nameLen);
        e.name[nameLen < 32 ? nameLen : 32] = 0;
        const u8 *ix = dir + (size_t)n * (size_t)nameLen + (size_t)i * 8;
        e.arc = arc;
        e.offset = base + rd32(ix);
        e.size = rd32(ix + 4);
        if ((long)e.offset + (long)e.size > fileSize || !e.name[0]) continue;
        if (pj_pac_find(a, e.name)) continue;
        if (a->count == a->cap) {
            a->cap = a->cap ? a->cap * 2 : 512;
            a->e = pj_realloc(a->e, sizeof(PacEntry) * (size_t)a->cap);
        }
        a->e[a->count++] = e;
        if (a->count * 2 > a->hashSize) rehash(a);
        else {
            u32 h = hash_name(e.name) & (u32)(a->hashSize - 1);
            while (a->hash[h] >= 0) h = (h + 1) & (u32)(a->hashSize - 1);
            a->hash[h] = a->count - 1;
        }
    }
    free(dir);
    pj_log(log, "archive %s: %u entries", path, n);
    return true;
}

static bool opened(const Pac *a, const char *dir, const char *name, char opens[][64], int openCount) {
    (void)a;
    (void)dir;
    for (int i = 0; i < openCount; i++)
        if (pj_ieq(opens[i], name)) return true;
    return false;
}

bool pj_pac_open(Pac *a, const char *dir, PJ *log) {
    memset(a, 0, sizeof *a);
    char opens[PJ_ARCS * 2][64];
    int openCount = 0;
    char *path = pj_fmt("%s/archive.dat", dir);
    bool ok = add_archive(a, path, log);
    free(path);
    if (!ok) return false;
    snprintf(opens[openCount++], 64, "archive.dat");
    size_t size;
    u8 *ini = pj_pac_read(a, "archive.ini", &size);
    if (ini) {
        Ini c = {0};
        pj_ini_parse(&c, ini, size);
        for (int i = 0; i < c.count; i++) {
            if (!pj_ieq(c.key[i], "file") || opened(a, dir, c.val[i], opens, openCount)) continue;
            path = pj_fmt("%s/%s", dir, c.val[i]);
            if (add_archive(a, path, log) && openCount < PJ_ARCS * 2) snprintf(opens[openCount++], 64, "%s", c.val[i]);
            free(path);
        }
        pj_ini_free(&c);
        free(ini);
    }
    char real[2048];
    if (pj_path_resolve(dir, real, sizeof real) < 1) return true;
    DIR *d = opendir(real);
    if (!d) return true;
    char *names[64];
    int count = 0;
    for (struct dirent *de; (de = readdir(d)) && count < 64;) {
        size_t n = strlen(de->d_name);
        if (n < 5 || n >= 64 || !pj_ieq(de->d_name + n - 4, ".dat") || opened(a, dir, de->d_name, opens, openCount)) continue;
        names[count++] = pj_strdup(de->d_name);
    }
    closedir(d);
    for (int i = 0; i < count; i++)
        for (int j = i + 1; j < count; j++)
            if (strcmp(names[j], names[i]) < 0) {
                char *t = names[i];
                names[i] = names[j];
                names[j] = t;
            }
    for (int i = 0; i < count; i++) {
        path = pj_fmt("%s/%s", real, names[i]);
        add_archive(a, path, log);
        free(path);
        free(names[i]);
    }
    return true;
}

void pj_pac_close(Pac *a) {
    for (int i = 0; i < a->arcs; i++)
        if (a->f[i]) fclose(a->f[i]);
    free(a->e);
    free(a->hash);
    memset(a, 0, sizeof *a);
}

u8 *pj_pac_read(Pac *a, const char *name, size_t *size) {
    const PacEntry *e = pj_pac_find(a, name);
    if (!e) return NULL;
    u8 *d = pj_alloc((size_t)e->size + 1);
    FILE *f = a->f[e->arc];
    if (fseek(f, (long)e->offset, SEEK_SET) || fread(d, 1, e->size, f) != e->size) {
        free(d);
        return NULL;
    }
    if (pj_ieq(e->name + (strlen(e->name) > 12 ? strlen(e->name) - 12 : 0), "textdata.bin") && e->size >= 5 &&
        d[0] == 0x95 && d[1] == 0x6b && d[2] == 0x3c && d[3] == 0x9d && d[4] == 0x63) {
        u8 key = 0xc5;
        for (u32 i = 0; i < e->size; i++) {
            d[i] ^= key;
            key = (u8)(key + 0x5c);
        }
    }
    if (size) *size = e->size;
    return d;
}

const PacEntry *pj_pac_find_ext(const Pac *a, const char *base, const char *const *exts, char *found, size_t foundSize) {
    char name[80];
    for (int i = 0; exts[i]; i++) {
        snprintf(name, sizeof name, "%s%s", base, exts[i]);
        const PacEntry *e = pj_pac_find(a, name);
        if (e) {
            if (found) snprintf(found, foundSize, "%s", e->name);
            return e;
        }
    }
    return NULL;
}

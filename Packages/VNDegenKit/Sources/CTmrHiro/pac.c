#include "th.h"

#include <ctype.h>

static u32 name_hash(const char *s) {
    u32 h = 2166136261u;
    for (; *s; s++) h = (h ^ (u8)tolower((u8)*s)) * 16777619u;
    return h;
}

static void group_put(PacGroup *g, const char *name, u64 offset, u32 size, int file) {
    if (g->hashSize == 0) {
        g->hashSize = 1 << 12;
        g->hash = th_alloc(sizeof(int) * (size_t)g->hashSize);
        for (int i = 0; i < g->hashSize; i++) g->hash[i] = -1;
    }
    u32 h = name_hash(name) & (u32)(g->hashSize - 1);
    while (g->hash[h] >= 0) {
        PacEntry *e = &g->entries[g->hash[h]];
        if (th_ieq(e->name, name)) {
            e->offset = offset;
            e->size = size;
            e->file = file;
            return;
        }
        h = (h + 1) & (u32)(g->hashSize - 1);
    }
    if (g->count == g->cap) {
        g->cap = g->cap ? g->cap * 2 : 256;
        g->entries = th_realloc(g->entries, sizeof(PacEntry) * (size_t)g->cap);
    }
    g->entries[g->count] = (PacEntry){th_strdup(name), offset, size, file};
    g->hash[h] = g->count++;
    if (g->count * 2 > g->hashSize) {
        int old = g->hashSize;
        free(g->hash);
        g->hashSize = old * 2;
        g->hash = th_alloc(sizeof(int) * (size_t)g->hashSize);
        for (int i = 0; i < g->hashSize; i++) g->hash[i] = -1;
        for (int i = 0; i < g->count; i++) {
            u32 k = name_hash(g->entries[i].name) & (u32)(g->hashSize - 1);
            while (g->hash[k] >= 0) k = (k + 1) & (u32)(g->hashSize - 1);
            g->hash[k] = i;
        }
    }
}

static bool read_index(Store *s, int kind, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    u8 hdr[7];
    if (fread(hdr, 1, 7, f) != 7) { fclose(f); return false; }
    int count = rd16(hdr);
    int nameLen = hdr[2];
    u32 dataStart = rd32(hdr + 3);
    u32 v1 = 7 + (u32)(nameLen + 8) * (u32)count, v2 = v1 + 4u * (u32)count;
    int version = dataStart == v1 ? 1 : dataStart == v2 ? 2 : 0;
    if (!version || !nameLen || count <= 0) { fclose(f); return false; }
    size_t indexSize = dataStart - 7;
    u8 *idx = th_alloc(indexSize);
    if (fread(idx, 1, indexSize, f) != indexSize) { free(idx); fclose(f); return false; }
    int file = s->fileCount++;
    s->files = th_realloc(s->files, sizeof(PacFile) * (size_t)s->fileCount);
    s->files[file] = (PacFile){th_strdup(path), f};
    u8 *p = idx;
    char *name = th_alloc((size_t)nameLen + 1);
    for (int i = 0; i < count; i++) {
        memcpy(name, p, (size_t)nameLen);
        name[nameLen] = 0;
        p += nameLen;
        u64 off;
        u32 size;
        if (version == 1) {
            off = rd32(p);
            size = rd32(p + 4);
            p += 8;
        } else {
            off = (u64)rd32(p) | (u64)rd32(p + 4) << 32;
            size = rd32(p + 8);
            p += 12;
        }
        group_put(&s->groups[kind], name, off + dataStart, size, file);
    }
    free(name);
    free(idx);
    return true;
}

static const struct { const char *base; int kind; } archives[] = {
    {"Agrd", PAC_IMAGE}, {"Bgrd", PAC_IMAGE}, {"Cgrd", PAC_IMAGE}, {"Dgrd", PAC_IMAGE}, {"Egrd", PAC_IMAGE},
    {"Fgrd", PAC_IMAGE}, {"Ggrd", PAC_IMAGE}, {"Hgrd", PAC_IMAGE}, {"grd", PAC_IMAGE}, {"srp", PAC_SCRIPT},
    {"mus", PAC_BGM}, {"se", PAC_SE}, {"sse", PAC_SSE}, {"vic", PAC_VOICE}, {"mov", PAC_MOVIE},
};

bool store_open(Store *s, const char *dir, char *err, size_t errSize) {
    memset(s, 0, sizeof *s);
    for (size_t a = 0; a < sizeof archives / sizeof archives[0]; a++) {
        for (int n = 1; n <= 10; n++) {
            char path[2048], real[2048];
            if (n == 1) snprintf(path, sizeof path, "%s/%s.pac", dir, archives[a].base);
            else snprintf(path, sizeof path, "%s/%s_%d.pac", dir, archives[a].base, n);
            if (th_path_resolve(path, real, sizeof real) != 1) continue;
            read_index(s, archives[a].kind, real);
        }
    }
    if (!s->groups[PAC_SCRIPT].count || !s->groups[PAC_IMAGE].count) {
        if (err) snprintf(err, errSize, "No Tmr-Hiro srp.pac and grd archives in %s", dir);
        store_close(s);
        return false;
    }
    return true;
}

void store_close(Store *s) {
    for (int i = 0; i < s->fileCount; i++) {
        if (s->files[i].f) fclose(s->files[i].f);
        free(s->files[i].path);
    }
    free(s->files);
    for (int k = 0; k < PAC_KINDS; k++) {
        for (int i = 0; i < s->groups[k].count; i++) free(s->groups[k].entries[i].name);
        free(s->groups[k].entries);
        free(s->groups[k].hash);
    }
    memset(s, 0, sizeof *s);
}

const PacEntry *store_find(Store *s, int kind, const char *name) {
    PacGroup *g = &s->groups[kind];
    if (!g->hashSize) return NULL;
    u32 h = name_hash(name) & (u32)(g->hashSize - 1);
    while (g->hash[h] >= 0) {
        PacEntry *e = &g->entries[g->hash[h]];
        if (th_ieq(e->name, name)) return e;
        h = (h + 1) & (u32)(g->hashSize - 1);
    }
    return NULL;
}

u8 *store_read(Store *s, const PacEntry *e) {
    FILE *f = s->files[e->file].f;
    u8 *d = th_alloc((size_t)e->size + 1);
    if (fseeko(f, (off_t)e->offset, SEEK_SET) != 0 || fread(d, 1, e->size, f) != e->size) {
        free(d);
        return NULL;
    }
    return d;
}

u8 *store_load(Store *s, int kind, const char *name, size_t *size) {
    const PacEntry *e = store_find(s, kind, name);
    if (!e) return NULL;
    if (size) *size = e->size;
    return store_read(s, e);
}

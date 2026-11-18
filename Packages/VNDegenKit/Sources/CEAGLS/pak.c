#include "eagls.h"

#include <ctype.h>
#include <sys/types.h>

static const char idxKey[] = "1qaz2wsx3edc4rfv5tgb6yhn7ujm8ik,9ol.0p;/-@:^[]";

static u32 hash_name(const char *s) {
    u32 h = 2166136261u;
    for (; *s; s++) h = (h ^ (u8)tolower((u8)*s)) * 16777619u;
    return h;
}

static void build_hash(Pak *p) {
    p->hashSize = 1;
    while (p->hashSize < p->count * 2) p->hashSize <<= 1;
    p->hash = eg_alloc(sizeof(int) * (size_t)p->hashSize);
    for (int i = 0; i < p->hashSize; i++) p->hash[i] = -1;
    for (int i = 0; i < p->count; i++) {
        u32 h = hash_name(p->e[i].name) & (u32)(p->hashSize - 1);
        while (p->hash[h] >= 0) h = (h + 1) & (u32)(p->hashSize - 1);
        p->hash[h] = i;
    }
}

bool pak_open(Pak *p, const char *dir, const char *base) {
    memset(p, 0, sizeof *p);
    char path[2048], real[2048];
    snprintf(path, sizeof path, "%s/%s.idx", dir, base);
    size_t n;
    u8 *raw = eg_read_file(path, &n);
    if (!raw || n < 8) {
        free(raw);
        return false;
    }
    snprintf(path, sizeof path, "%s/%s.pak", dir, base);
    if (eg_path_resolve(path, real, sizeof real) < 0 || !(p->f = fopen(real, "rb"))) {
        free(raw);
        return false;
    }
    size_t len = n - 4;
    u32 seed;
    eg_rand_seed(&seed, (i32)rd32(raw + len));
    for (size_t i = 0; i < len; i++) raw[i] ^= (u8)idxKey[eg_rand(&seed) % (int)(sizeof idxKey - 1)];
    size_t entry = len / 10000;
    if (entry > 40) entry = 40;
    bool longOff = entry == 40;
    size_t nameLen = longOff ? 0x18 : 0x14;
    u64 first = rd32(raw + nameLen);
    for (size_t o = 0; o + entry <= len && raw[o]; o += entry) {
        char name[0x19] = {0};
        memcpy(name, raw + o, nameLen);
        u64 off;
        u32 size;
        if (longOff) {
            off = (u64)rd32(raw + o + nameLen) | (u64)rd32(raw + o + nameLen + 4) << 32;
            size = rd32(raw + o + nameLen + 8);
        } else {
            off = rd32(raw + o + nameLen);
            size = rd32(raw + o + nameLen + 4);
        }
        p->e = eg_realloc(p->e, sizeof(PakEntry) * (size_t)(p->count + 1));
        p->e[p->count].name = eg_strdup(name);
        p->e[p->count].offset = off - first;
        p->e[p->count].size = size;
        p->count++;
    }
    free(raw);
    build_hash(p);
    return p->count > 0;
}

void pak_close(Pak *p) {
    for (int i = 0; i < p->count; i++) free(p->e[i].name);
    free(p->e);
    free(p->hash);
    if (p->f) fclose(p->f);
    memset(p, 0, sizeof *p);
}

const PakEntry *pak_find(Pak *p, const char *name) {
    if (!p->count) return NULL;
    u32 h = hash_name(name) & (u32)(p->hashSize - 1);
    while (p->hash[h] >= 0) {
        if (eg_ieq(p->e[p->hash[h]].name, name)) return &p->e[p->hash[h]];
        h = (h + 1) & (u32)(p->hashSize - 1);
    }
    return NULL;
}

u8 *pak_read(Pak *p, const PakEntry *e, size_t *size) {
    u8 *d = eg_alloc((size_t)e->size + 1);
    if (fseeko(p->f, (off_t)e->offset, SEEK_SET) != 0 || fread(d, 1, e->size, p->f) != e->size) {
        free(d);
        return NULL;
    }
    if (size) *size = e->size;
    return d;
}

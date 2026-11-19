#include "ikura.h"
#include "ik_casepath.h"

static const char *const kindNames[AR_KINDS] = {"GRP", "SNR", "SE", "BGM", "DATA", "VOICE"};

static bool mpx_read(Store *s, int kind, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    u8 head[16];
    if (fread(head, 1, 16, f) != 16 || memcmp(head, "SM2MPX10", 8)) {
        fclose(f);
        return false;
    }
    u32 count = rd32(head + 8);
    if (count == 0 || count > 0xfffff || fseek(f, 0x20, SEEK_SET)) {
        fclose(f);
        return false;
    }
    u8 *idx = ik_alloc((size_t)count * 0x14);
    if (fread(idx, 0x14, count, f) != count) {
        free(idx);
        fclose(f);
        return false;
    }
    s->files = ik_realloc(s->files, sizeof(FILE *) * (size_t)(s->fileCount + 1));
    int fi = s->fileCount++;
    s->files[fi] = f;
    MpxEntry *list = s->entries[kind];
    int n = s->counts[kind];
    list = ik_realloc(list, sizeof(MpxEntry) * (size_t)(n + count));
    for (u32 i = 0; i < count; i++) {
        const u8 *e = idx + (size_t)i * 0x14;
        MpxEntry *m = &list[n + i];
        memset(m, 0, sizeof *m);
        memcpy(m->name, e, 12);
        ik_lower(m->name);
        m->offset = rd32(e + 12);
        m->size = rd32(e + 16);
        m->file = fi;
    }
    s->entries[kind] = list;
    s->counts[kind] = n + (int)count;
    free(idx);
    return true;
}

bool store_open(Store *s, const char *gameDir, char *err, size_t errSize) {
    memset(s, 0, sizeof *s);
    for (int k = 0; k < AR_KINDS; k++) {
        char want[2048], real[2048];
        for (int p = 9; p >= 0; p--) {
            snprintf(want, sizeof want, "%s/%s%d", gameDir, kindNames[k], p);
            if (ik_casepath_resolve(want, real, sizeof real) >= 0) mpx_read(s, k, real);
        }
        snprintf(want, sizeof want, "%s/%s", gameDir, kindNames[k]);
        if (ik_casepath_resolve(want, real, sizeof real) >= 0) mpx_read(s, k, real);
    }
    if (!s->counts[AR_SNR] || !s->counts[AR_GRP]) {
        if (err) snprintf(err, errSize, "This game's SNR and GRP archives are missing.");
        store_close(s);
        return false;
    }
    return true;
}

void store_close(Store *s) {
    for (int i = 0; i < s->fileCount; i++) fclose(s->files[i]);
    free(s->files);
    for (int k = 0; k < AR_KINDS; k++) free(s->entries[k]);
    memset(s, 0, sizeof *s);
}

u8 *store_load(Store *s, int kind, const char *name, size_t *size) {
    char key[64];
    snprintf(key, sizeof key, "%s", name);
    ik_lower(key);
    size_t keyLen = strlen(key);
    bool bare = !strchr(key, '.');
    MpxEntry *e = NULL;
    for (int i = 0; i < s->counts[kind] && !e; i++)
        if (!strcmp(s->entries[kind][i].name, key)) e = &s->entries[kind][i];
    for (int i = 0; i < s->counts[kind] && !e && bare; i++) {
        MpxEntry *c = &s->entries[kind][i];
        if (!strncmp(c->name, key, keyLen) && c->name[keyLen] == '.') e = c;
    }
    if (e) {
        FILE *f = s->files[e->file];
        u8 *d = ik_alloc((size_t)e->size + 1);
        if (fseek(f, (long)e->offset, SEEK_SET) || fread(d, 1, e->size, f) != e->size) {
            free(d);
            return NULL;
        }
        if (size) *size = e->size;
        return d;
    }
    return NULL;
}

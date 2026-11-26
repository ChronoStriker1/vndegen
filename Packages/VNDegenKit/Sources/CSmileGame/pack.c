#include "sg.h"

#include <ctype.h>

int stbi_zlib_decode_noheader_buffer(char *obuffer, int olen, const char *ibuffer, int ilen);

void pack_key(const char *name, char *out, size_t size) {
    size_t n = 0;
    const char *s = name;
    if (s[0] == '.' && (s[1] == '\\' || s[1] == '/')) s += 2;
    for (; *s && n + 1 < size; s++) {
        char c = *s == '\\' ? '/' : *s;
        if (c == '/' && n > 0 && out[n - 1] == '/') continue;
        out[n++] = (char)tolower((unsigned char)c);
    }
    out[n] = 0;
}

static int cmp_entry(const void *a, const void *b) { return strcmp(((const PackEntry *)a)->key, ((const PackEntry *)b)->key); }

static u32 le32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }

bool pack_open(Pack *p, const char *path, char *err, size_t errSize) {
    memset(p, 0, sizeof *p);
    p->f = fopen(path, "rb");
    if (!p->f) {
        snprintf(err, errSize, "can't open %s", path);
        return false;
    }
    u8 magic[8];
    if (fread(magic, 1, 8, p->f) != 8 || memcmp(magic, "YUKARPKG", 8) != 0) {
        snprintf(err, errSize, "%s is not a Smile Game Builder package", path);
        pack_close(p);
        return false;
    }
    fseek(p->f, 0, SEEK_END);
    long size = ftell(p->f);
    long tailSize = size < 65557 ? size : 65557;
    u8 *tail = sg_alloc((size_t)tailSize);
    fseek(p->f, size - tailSize, SEEK_SET);
    if (fread(tail, 1, (size_t)tailSize, p->f) != (size_t)tailSize) {
        free(tail);
        snprintf(err, errSize, "can't read %s", path);
        pack_close(p);
        return false;
    }
    long eocd = -1;
    for (long i = tailSize - 22; i >= 0; i--)
        if (le32(tail + i) == 0x06054b50) {
            eocd = i;
            break;
        }
    if (eocd < 0) {
        free(tail);
        snprintf(err, errSize, "%s has no central directory", path);
        pack_close(p);
        return false;
    }
    u32 count = le16(tail + eocd + 10), dirSize = le32(tail + eocd + 12), dirOffset = le32(tail + eocd + 16);
    free(tail);
    u8 *dir = sg_alloc(dirSize);
    fseek(p->f, dirOffset, SEEK_SET);
    if (fread(dir, 1, dirSize, p->f) != dirSize) {
        free(dir);
        snprintf(err, errSize, "can't read the directory of %s", path);
        pack_close(p);
        return false;
    }
    p->entries = sg_alloc(sizeof(PackEntry) * (count ? count : 1));
    u32 o = 0;
    for (u32 i = 0; i < count && o + 46 <= dirSize; i++) {
        const u8 *h = dir + o;
        if (le32(h) != 0x02014b50) break;
        u16 nameLen = le16(h + 28), extraLen = le16(h + 30), commentLen = le16(h + 32);
        if (o + 46 + nameLen > dirSize) break;
        char name[1024];
        size_t n = nameLen < sizeof name - 1 ? nameLen : sizeof name - 1;
        memcpy(name, h + 46, n);
        name[n] = 0;
        o += 46 + nameLen + extraLen + commentLen;
        if (n && (name[n - 1] == '\\' || name[n - 1] == '/')) continue;
        char key[1024];
        pack_key(name, key, sizeof key);
        PackEntry *e = &p->entries[p->count++];
        e->key = sg_strdup(key);
        e->method = le16(h + 10);
        e->csize = le32(h + 20);
        e->usize = le32(h + 24);
        e->offset = le32(h + 42);
    }
    free(dir);
    qsort(p->entries, (size_t)p->count, sizeof(PackEntry), cmp_entry);
    return true;
}

void pack_close(Pack *p) {
    if (p->f) fclose(p->f);
    for (int i = 0; i < p->count; i++) free(p->entries[i].key);
    free(p->entries);
    memset(p, 0, sizeof *p);
}

const PackEntry *pack_find(const Pack *p, const char *name) {
    char key[1024];
    pack_key(name, key, sizeof key);
    PackEntry probe = {.key = key};
    return bsearch(&probe, p->entries, (size_t)p->count, sizeof(PackEntry), cmp_entry);
}

u8 *pack_read(Pack *p, const char *name, size_t *size) {
    const PackEntry *e = pack_find(p, name);
    if (!e) return NULL;
    u8 local[30];
    fseek(p->f, e->offset, SEEK_SET);
    if (fread(local, 1, 30, p->f) != 30) return NULL;
    long data = (long)e->offset + 30 + le16(local + 26) + le16(local + 28);
    u8 *raw = sg_alloc(e->csize ? e->csize : 1);
    fseek(p->f, data, SEEK_SET);
    if (e->csize && fread(raw, 1, e->csize, p->f) != e->csize) {
        free(raw);
        return NULL;
    }
    if (e->method == 0) {
        *size = e->csize;
        return raw;
    }
    if (e->method != 8) {
        free(raw);
        return NULL;
    }
    u8 *out = sg_alloc((size_t)e->usize + 1);
    int got = stbi_zlib_decode_noheader_buffer((char *)out, (int)e->usize, (const char *)raw, (int)e->csize);
    free(raw);
    if (got != (int)e->usize) {
        free(out);
        return NULL;
    }
    *size = e->usize;
    return out;
}

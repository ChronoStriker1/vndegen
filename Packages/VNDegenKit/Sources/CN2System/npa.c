// Nitroplus .npa archives: "NPA\x01", two keys, flags, counts, then the index. Entry names are stored with a
// per-entry offset added to each byte. Layout from GARbro's ArcNPA.cs (MIT); see docs/n2system-cleanroom.md.
#include "n2.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

typedef struct NpaScheme {
    u8 nameKey;
    const u8 *table;
} NpaScheme;

static const u8 hanachirasuTable[256] = {
    0x59, 0x19, 0x06, 0xba, 0xb1, 0x69, 0x1d, 0x14, 0x36, 0xe1, 0x5a, 0xf0, 0x97, 0x7d, 0xad, 0xe6,
    0x1c, 0x37, 0xd2, 0xc0, 0xe3, 0xc5, 0xa5, 0x64, 0xa7, 0x7a, 0xd3, 0x2a, 0x87, 0x20, 0xc7, 0xca,
    0x4b, 0x50, 0x40, 0xd7, 0xb4, 0xb2, 0x9c, 0xac, 0x82, 0x32, 0x76, 0x9e, 0xda, 0x56, 0xc2, 0x6a,
    0xe4, 0x2d, 0xe5, 0xc6, 0x18, 0x0a, 0xed, 0x46, 0x15, 0xc3, 0x27, 0x67, 0xff, 0x78, 0x8a, 0xef,
    0x11, 0xa9, 0x0f, 0x10, 0xf7, 0x07, 0x2f, 0x3a, 0xcc, 0xa2, 0x63, 0x25, 0xeb, 0x0e, 0x9b, 0x08,
    0xd8, 0x3d, 0x95, 0x43, 0x1f, 0x71, 0xd9, 0x75, 0x62, 0x9d, 0x39, 0xa0, 0x1b, 0x47, 0x21, 0x9f,
    0x74, 0xdc, 0x4c, 0xe8, 0x34, 0x8e, 0x83, 0xd4, 0xb0, 0x3f, 0x6b, 0x4e, 0x8b, 0xfc, 0x5d, 0x66,
    0xf2, 0x22, 0x1a, 0x3e, 0x0c, 0x41, 0x03, 0x35, 0x42, 0x05, 0xe7, 0x91, 0x92, 0x85, 0xfb, 0xa1,
    0x65, 0x99, 0x94, 0xe0, 0x8f, 0x00, 0x7f, 0x4a, 0xe9, 0xb6, 0x57, 0xde, 0x26, 0x9a, 0x6f, 0xaa,
    0xfe, 0xf5, 0x24, 0x2e, 0x30, 0xdb, 0x58, 0x04, 0xec, 0x5f, 0x4d, 0x88, 0x4f, 0x5b, 0x31, 0xbc,
    0xbb, 0x6d, 0x54, 0xae, 0x09, 0x01, 0x28, 0xa8, 0xf3, 0x0d, 0xa3, 0x45, 0x49, 0x48, 0xcf, 0x84,
    0x3b, 0x68, 0xa4, 0xf1, 0xee, 0x2b, 0xfd, 0x7c, 0xa6, 0xd6, 0xe2, 0x5c, 0x12, 0xb8, 0xb3, 0x6c,
    0x8d, 0x89, 0xbf, 0xf9, 0xf8, 0x81, 0x77, 0x6e, 0x44, 0xab, 0xcd, 0x7b, 0xdd, 0xc1, 0x51, 0x96,
    0x55, 0xf6, 0x02, 0xbd, 0x60, 0x73, 0x23, 0x72, 0xaf, 0x33, 0xd1, 0xcb, 0xd5, 0xc8, 0xce, 0xf4,
    0x80, 0x8c, 0xfa, 0x13, 0xc9, 0xd0, 0x2c, 0x1e, 0x90, 0x29, 0x86, 0x38, 0xb9, 0x7e, 0x3c, 0xdf,
    0x5e, 0xb7, 0x79, 0xb5, 0x16, 0xc4, 0x52, 0x0b, 0x93, 0x98, 0x17, 0x61, 0xbe, 0xea, 0x53, 0x70,
};

static const NpaScheme schemes[] = {
    {33, hanachirasuTable},
};

static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

static u8 name_key(int index, int file, i32 arcKey) {
    int key = 0xfc * index;
    key -= arcKey >> 24;
    key -= arcKey >> 16;
    key -= arcKey >> 8;
    key -= arcKey & 0xff;
    key -= file >> 24;
    key -= file >> 16;
    key -= file >> 8;
    key -= file;
    return (u8)key;
}

static void decrypt(const NpaScheme *s, const NpaEntry *e, i32 arcKey, u8 *data, u32 size) {
    u32 key = ((u32)(u8)(s->nameKey - e->nameSum) * e->nameLen + (u32)arcKey) * e->unpackedSize;
    u32 len = 0x1000 + e->nameLen;
    if (len > size) len = size;
    for (u32 i = 0; i < len; i++) data[i] = (u8)(s->table[data[i]] - (u8)key - i);
}

static bool looks_plain(const u8 *p, size_t n) {
    if (n >= 4 && (!memcmp(p, "\x89PNG", 4) || !memcmp(p, "OggS", 4) || !memcmp(p, "\xff\xd8\xff", 3) ||
                   !memcmp(p, "BM", 2)))
        return true;
    if (n >= 2 && p[0] == 0x78 && ((p[0] << 8 | p[1]) % 31) == 0) return true;
    return n >= 1 && (p[0] == ';' || p[0] == '[' || p[0] == '/' || p[0] == '#' || p[0] == '\r' || p[0] == '\n');
}

static char *lower_dup(const char *s) {
    char *d = n2_strdup(s);
    for (char *p = d; *p; p++) {
        if (*p == '\\') *p = '/';
        else *p = (char)tolower((u8)*p);
    }
    return d;
}

Npa *npa_open(const char *path, char *err, size_t errSize) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, errSize, "can't open %s", path);
        return NULL;
    }
    u8 h[41];
    if (fread(h, 1, 41, f) != 41 || memcmp(h, "NPA\x01", 4) != 0) {
        snprintf(err, errSize, "%s is not an NPA archive", path);
        fclose(f);
        return NULL;
    }
    i32 key1 = (i32)rd32(h + 7), key2 = (i32)rd32(h + 11);
    bool compressed = h[15] != 0, encrypted = h[16] != 0;
    i32 total = (i32)rd32(h + 17);
    u32 dirSize = rd32(h + 37);
    if (total < 0 || dirSize > (64u << 20)) {
        snprintf(err, errSize, "%s has a damaged index", path);
        fclose(f);
        return NULL;
    }
    u8 *dir = n2_alloc(dirSize);
    if (fread(dir, 1, dirSize, f) != dirSize) {
        snprintf(err, errSize, "%s is cut short", path);
        free(dir);
        fclose(f);
        return NULL;
    }
    // Signed multiplication wraps as on the PC; do it unsigned to keep C defined.
    i32 arcKey = (i32)((u32)key1 * (u32)key2);
    Npa *a = n2_alloc(sizeof *a);
    a->path = n2_strdup(path);
    a->file = f;
    a->compressed = compressed;
    a->arcKey = arcKey;
    a->entries = n2_alloc(sizeof(NpaEntry) * (size_t)(total ? total : 1));
    size_t pos = 0;
    for (int i = 0; i < total; i++) {
        if (pos + 4 > dirSize) break;
        u32 n = rd32(dir + pos);
        if (pos + 4 + n + 17 > dirSize) break;
        u8 *raw = n2_alloc(n + 1);
        for (u32 x = 0; x < n; x++) raw[x] = (u8)(dir[pos + 4 + x] + name_key((int)x, i, arcKey));
        u8 type = dir[pos + 4 + n];
        const u8 *info = dir + pos + 5 + n;
        if (type != 1) {
            NpaEntry *e = &a->entries[a->count++];
            e->nameLen = n;
            for (u32 x = 0; x < n; x++) e->nameSum = (u8)(e->nameSum + raw[x]);
            e->name = n2_sjis_to_utf8(raw, n, NULL);
            for (char *p = e->name; *p; p++)
                if (*p == '\\') *p = '/';
            e->offset = 41 + dirSize + rd32(info + 4);
            e->size = rd32(info + 8);
            e->unpackedSize = rd32(info + 12);
        }
        free(raw);
        pos += 4 + n + 17;
    }
    free(dir);
    if (encrypted) {
        for (size_t s = 0; s < sizeof schemes / sizeof schemes[0] && !a->scheme; s++) {
            int tried = 0, plain = 0;
            for (int i = 0; i < a->count && tried < 4; i++) {
                const NpaEntry *e = &a->entries[i];
                if (e->size < 4) continue;
                u8 head[8] = {0};
                size_t n = e->size < sizeof head ? e->size : sizeof head;
                if (fseek(f, (long)e->offset, SEEK_SET) != 0 || fread(head, 1, n, f) != n) continue;
                decrypt(&schemes[s], e, arcKey, head, (u32)n);
                tried++;
                plain += looks_plain(head, n);
            }
            if (tried && plain == tried) a->scheme = &schemes[s];
        }
        if (!a->scheme) {
            snprintf(err, errSize, "%s is encrypted with a key this runtime doesn't know", path);
            npa_close(a);
            return NULL;
        }
    }
    for (int i = 0; i < a->count; i++) {
        char *k = lower_dup(a->entries[i].name);
        map_set(&a->index, k, &a->entries[i]);
        free(k);
    }
    return a;
}

void npa_close(Npa *a) {
    if (!a) return;
    fclose(a->file);
    for (int i = 0; i < a->count; i++) free(a->entries[i].name);
    free(a->entries);
    map_free(&a->index, NULL);
    free(a->path);
    free(a);
}

const NpaEntry *npa_find(Npa *a, const char *name) {
    char *k = lower_dup(name);
    const NpaEntry *e = map_get(&a->index, k);
    free(k);
    return e;
}

u8 *npa_read(Npa *a, const NpaEntry *e, size_t *size) {
    u8 *data = n2_alloc(e->size + 1);
    if (fseek(a->file, (long)e->offset, SEEK_SET) != 0 || fread(data, 1, e->size, a->file) != e->size) {
        free(data);
        return NULL;
    }
    if (a->scheme) decrypt(a->scheme, e, a->arcKey, data, e->size);
    if (a->compressed && e->size != e->unpackedSize) {
        u8 *out = n2_alloc(e->unpackedSize + 1);
        uLongf outLen = e->unpackedSize;
        if (uncompress(out, &outLen, data, e->size) != Z_OK) {
            free(out);
            free(data);
            return NULL;
        }
        free(data);
        *size = outLen;
        return out;
    }
    *size = e->size;
    return data;
}

// Nitroplus .npa archives: "NPA\x01", two keys, flags, counts, then the index. Entry names are stored with a
// per-entry offset added to each byte. Layout from GARbro's ArcNPA.cs (MIT); see docs/n2system-cleanroom.md.
//
// Only what the test games use is built: plain entries. Encrypted archives (Demonbane, Hanachirasu) are refused with
// a message, and zlib-packed entries are inflated.
#include "n2.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

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
    if (encrypted) {
        snprintf(err, errSize, "%s is encrypted, which this runtime doesn't read yet", path);
        fclose(f);
        return NULL;
    }
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

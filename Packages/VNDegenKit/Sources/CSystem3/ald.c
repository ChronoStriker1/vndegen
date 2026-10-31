// ALD link archives, as AliceSoft's linker NL4.EXE writes them.
//
// Layout, read from the files: the volume is in 256-byte sectors. Sector 0 holds 3-byte little
// endian sector numbers: the first is where the link map starts, the rest are where each entry of
// this volume starts. The link map holds one 3-byte record per link number: the volume (1 = A) and
// the entry index within that volume. Each entry starts with a header: u32 header size, u32 data
// size, an 8-byte time stamp, then the NUL-padded file name.
#include "s3.h"

static u32 u24(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16; }

static bool entry(const S3Archive *a, int vol, u32 index, S3Blob *out, const char **name) {
    const u8 *d = a->vol[vol];
    size_t size = a->volSize[vol];
    if (!d || index == 0 || size < 3) return false;
    size_t first = (size_t)u24(d) * 256;
    size_t slot = (size_t)index * 3;
    if (slot + 3 > first || slot + 3 > size) return false;
    size_t off = (size_t)u24(d + slot) * 256;
    if (!off || off + 16 > size) return false;
    u32 hs = s3_le32(d + off), ds = s3_le32(d + off + 4);
    if (hs > size - off || ds > size - off - hs) return false;
    out->data = d + off + hs;
    out->size = ds;
    if (name) *name = (const char *)d + off + 16;
    return true;
}

bool s3_archive_open(S3Archive *a, const char *dir, const char *prefix, char kind) {
    memset(a, 0, sizeof *a);
    for (int v = 0; v < 26; v++) {
        char name[128];
        snprintf(name, sizeof name, "%s%c%c.ALD", prefix, kind, 'A' + v);
        char *path = s3_find_file(dir, name);
        if (!path) continue;
        a->vol[v] = s3_read_file(path, &a->volSize[v]);
        free(path);
        if (a->vol[v]) a->disks = v + 1;
    }
    if (!a->disks) return false;
    // Every volume carries the same link map; read the first one present.
    for (int v = 0; v < a->disks; v++) {
        const u8 *d = a->vol[v];
        if (!d || a->volSize[v] < 512) continue;
        size_t mapStart = (size_t)u24(d) * 256, mapEnd = (size_t)u24(d + 3) * 256;
        if (mapStart + 3 > a->volSize[v]) continue;
        if (mapEnd <= mapStart || mapEnd > a->volSize[v]) continue;
        u32 n = (u32)((mapEnd - mapStart) / 3);
        while (n && d[mapStart + (n - 1) * 3] == 0) n--;
        a->count = n;
        a->mapDisk = s3_alloc(n + 1);
        a->mapIndex = s3_alloc((n + 1) * sizeof(u16));
        for (u32 i = 0; i < n; i++) {
            const u8 *r = d + mapStart + i * 3;
            a->mapDisk[i + 1] = r[0];
            a->mapIndex[i + 1] = s3_le16(r + 1);
        }
        break;
    }
    return a->count > 0;
}

void s3_archive_close(S3Archive *a) {
    for (int v = 0; v < 26; v++) free(a->vol[v]);
    free(a->mapDisk);
    free(a->mapIndex);
    memset(a, 0, sizeof *a);
}

bool s3_archive_get(const S3Archive *a, u32 n, S3Blob *out, const char **name) {
    if (n == 0 || n > a->count) return false;
    int disk = a->mapDisk[n];
    if (disk < 1 || disk > 26) return false;
    return entry(a, disk - 1, a->mapIndex[n], out, name);
}

// System 3.0 DAT archives (ADISK.DAT, ACG.dat, BCG.dat, ...), read from the files: 256-byte sectors, a table of
// u16 sector numbers where file i spans sectors table[i-1]-1 to table[i]-1, and file 1 a link map of (volume, file)
// byte pairs, one per link number. A link's file index counts the map as file 1, so index i is file i + 1.
bool s3_dat_open(S3Dat *a, const char *dir, const char *const *names, int count) {
    memset(a, 0, sizeof *a);
    for (int v = 0; v < count && v < 4; v++) {
        char *path = s3_find_file(dir, names[v]);
        if (!path) continue;
        a->vol[v] = s3_read_file(path, &a->volSize[v]);
        free(path);
    }
    if (!a->vol[0]) return false;
    S3Blob map;
    a->mapData = NULL;
    if (!s3_dat_file(a, 0, 1, &map)) return false;
    a->map = map.data;
    a->count = (u32)(map.size / 2);
    while (a->count && a->map[(a->count - 1) * 2] == 0) a->count--;
    return a->count > 0;
}

bool s3_dat_file(const S3Dat *a, int vol, u32 file, S3Blob *out) {
    if (vol < 0 || vol >= 4 || !a->vol[vol] || file == 0) return false;
    const u8 *d = a->vol[vol];
    size_t n = a->volSize[vol];
    if (n < 2) return false;
    size_t tableEnd = ((size_t)s3_le16(d) - 1) * 256;
    if ((size_t)file * 2 + 2 > tableEnd || tableEnd > n) return false;
    u32 s0 = s3_le16(d + (file - 1) * 2), s1 = s3_le16(d + file * 2);
    if (s0 == 0 || s1 <= s0) return false;
    size_t from = ((size_t)s0 - 1) * 256, to = ((size_t)s1 - 1) * 256;
    if (to > n) to = n;
    if (from >= to) return false;
    out->data = d + from;
    out->size = to - from;
    return true;
}

bool s3_dat_get(const S3Dat *a, u32 n, S3Blob *out) {
    if (n == 0 || n > a->count) return false;
    int vol = a->map[(n - 1) * 2], index = a->map[(n - 1) * 2 + 1];
    if (vol < 1 || index < 1) return false;
    return s3_dat_file(a, vol - 1, (u32)index + 1, out);
}

void s3_dat_close(S3Dat *a) {
    for (int v = 0; v < 4; v++) free(a->vol[v]);
    memset(a, 0, sizeof *a);
}

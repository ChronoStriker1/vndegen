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
    if (!d || index == 0) return false;
    u32 first = u24(d) * 256;
    size_t slot = (size_t)index * 3;
    if (slot + 3 > first) return false;
    size_t off = (size_t)u24(d + slot) * 256;
    if (!off || off + 16 > size) return false;
    u32 hs = s3_le32(d + off), ds = s3_le32(d + off + 4);
    if (off + hs > size || off + hs + ds > size) return false;
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

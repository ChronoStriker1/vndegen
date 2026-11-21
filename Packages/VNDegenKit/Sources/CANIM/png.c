#include "anim.h"

#include <zlib.h>

static void be32(u8 *p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static void chunk(u8 **out, size_t *len, const char *type, const u8 *data, size_t n) {
    *out = an_realloc(*out, *len + n + 12);
    u8 *p = *out + *len;
    be32(p, (u32)n);
    memcpy(p + 4, type, 4);
    if (n) memcpy(p + 8, data, n);
    u32 c = (u32)crc32(0, p + 4, (uInt)(n + 4));
    be32(p + 8 + n, c);
    *len += n + 12;
}

bool an_png_write(const char *path, const u32 *px, int w, int h) {
    size_t row = (size_t)w * 4 + 1, raw = row * (size_t)h;
    u8 *r = an_alloc(raw);
    for (int y = 0; y < h; y++) memcpy(r + (size_t)y * row + 1, px + (size_t)y * (size_t)w, (size_t)w * 4);
    uLongf zn = compressBound((uLong)raw);
    u8 *z = an_alloc(zn);
    bool ok = compress2(z, &zn, r, (uLong)raw, 9) == Z_OK;
    free(r);
    u8 *out = an_alloc(8);
    memcpy(out, "\x89PNG\r\n\x1a\n", 8);
    size_t len = 8;
    u8 ihdr[13] = {0};
    be32(ihdr, (u32)w);
    be32(ihdr + 4, (u32)h);
    ihdr[8] = 8;
    ihdr[9] = 6;
    chunk(&out, &len, "IHDR", ihdr, 13);
    chunk(&out, &len, "IDAT", z, zn);
    chunk(&out, &len, "IEND", NULL, 0);
    free(z);
    ok = ok && an_write_file(path, out, len);
    free(out);
    return ok;
}

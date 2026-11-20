#include "age.h"

Image *image_new(int w, int h) {
    Image *im = ag_alloc(sizeof *im);
    im->w = w;
    im->h = h;
    im->px = ag_alloc((size_t)(w > 0 ? w : 1) * (size_t)(h > 0 ? h : 1) * 4);
    return im;
}

void image_free(Image *im) {
    if (!im) return;
    free(im->px);
    free(im);
}

static u8 *section(const u8 *d, size_t size, size_t p, u32 unpacked, u32 packed) {
    if (p + packed > size || unpacked > 64u << 20) return NULL;
    u8 *out = ag_alloc(unpacked + 4);
    if (unpacked == packed) memcpy(out, d + p, packed);
    else ag_lzss(d + p, packed, out, unpacked);
    return out;
}

static Image *bmp_decode(const u8 *d, size_t size) {
    if (size < 0x36) return NULL;
    u32 off = rd32(d + 10);
    int w = (int)rd32(d + 18), h = (int)rd32(d + 22);
    int bpp = rd16(d + 28);
    bool topDown = h < 0;
    if (topDown) h = -h;
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || (bpp != 24 && bpp != 32 && bpp != 8)) return NULL;
    int stride = (w * bpp / 8 + 3) & ~3;
    if ((size_t)off + (size_t)stride * (size_t)h > size) return NULL;
    u32 pal[256] = { 0 };
    if (bpp == 8)
        for (int i = 0; i < 256 && 0x36 + i * 4 + 4 <= (int)off; i++) {
            const u8 *c = d + 0x36 + i * 4;
            pal[i] = (u32)c[2] | (u32)c[1] << 8 | (u32)c[0] << 16 | 0xff000000u;
        }
    bool anyAlpha = false;
    if (bpp == 32)
        for (int y = 0; y < h && !anyAlpha; y++)
            for (int x = 0; x < w; x++)
                if (d[off + (size_t)y * (size_t)stride + (size_t)x * 4 + 3]) { anyAlpha = true; break; }
    Image *im = image_new(w, h);
    for (int y = 0; y < h; y++) {
        const u8 *row = d + off + (size_t)(topDown ? y : h - 1 - y) * (size_t)stride;
        u32 *o = im->px + (size_t)y * (size_t)w;
        for (int x = 0; x < w; x++) {
            if (bpp == 8) { o[x] = pal[row[x]]; continue; }
            const u8 *q = row + x * (bpp / 8);
            u32 a = bpp == 32 && anyAlpha ? q[3] : 255;
            o[x] = (u32)q[2] | (u32)q[1] << 8 | (u32)q[0] << 16 | a << 24;
        }
    }
    return im;
}

Image *agf_decode(const u8 *d, size_t size) {
    if (size >= 2 && d[0] == 'B' && d[1] == 'M') return bmp_decode(d, size);
    if (size < 0x24) return NULL;
    int type = (int)rd32(d + 4);
    u32 hu = rd32(d + 0xc), hp = rd32(d + 0x14);
    u8 *hdr = section(d, size, 0x18, hu, hp);
    if (!hdr || hu < 0x36) { free(hdr); return NULL; }
    int w = (int)rd32(hdr + 0x14), h = (int)rd32(hdr + 0x18);
    int bpp = rd16(hdr + 0x1c + 2);
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) { free(hdr); return NULL; }
    u32 pal[256] = { 0 };
    if (bpp <= 8) {
        int n = 1 << bpp;
        for (int i = 0; i < n && 0x38 + i * 4 + 4 <= (int)hu; i++) {
            const u8 *c = hdr + 0x38 + i * 4;
            pal[i] = (u32)c[2] | (u32)c[1] << 8 | (u32)c[0] << 16;
        }
    }
    free(hdr);
    size_t p = 0x18 + hp;
    if (p + 12 > size) return NULL;
    u32 du = rd32(d + p + 4), dp = rd32(d + p + 8);
    p += 12;
    u8 *bmp = section(d, size, p, du, dp);
    if (!bmp) return NULL;
    p += dp;
    u8 *alpha = NULL;
    if (type == 2 && p + 0x24 <= size && !memcmp(d + p, "ACIF", 4)) {
        u32 au = rd32(d + p + 0x1c), ap = rd32(d + p + 0x20);
        alpha = section(d, size, p + 0x24, au, ap);
        if (alpha && au < (u32)(w * h)) { free(alpha); alpha = NULL; }
    }
    int stride = (w * bpp / 8 + 3) & ~3;
    if ((size_t)stride * (size_t)h > du) { free(bmp); free(alpha); return NULL; }
    Image *im = image_new(w, h);
    for (int y = 0; y < h; y++) {
        const u8 *row = bmp + (size_t)(h - 1 - y) * (size_t)stride;
        u32 *o = im->px + (size_t)y * (size_t)w;
        const u8 *a = alpha ? alpha + (size_t)y * (size_t)w : NULL;
        for (int x = 0; x < w; x++) {
            u32 c;
            if (bpp == 8) c = pal[row[x]];
            else if (bpp == 4) c = pal[(row[x >> 1] >> ((x & 1) ? 0 : 4)) & 0xf];
            else if (bpp == 1) c = pal[(row[x >> 3] >> (7 - (x & 7))) & 1];
            else {
                const u8 *q = row + x * (bpp / 8);
                c = (u32)q[2] | (u32)q[1] << 8 | (u32)q[0] << 16;
            }
            o[x] = c | (u32)(a ? a[x] : 255) << 24;
        }
    }
    free(bmp);
    free(alpha);
    return im;
}

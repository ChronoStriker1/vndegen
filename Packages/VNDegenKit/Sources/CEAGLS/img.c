#include "eagls.h"

Image *image_new(int w, int h) {
    Image *im = eg_alloc(sizeof *im);
    im->w = w;
    im->h = h;
    im->px = eg_alloc(sizeof(u32) * (size_t)w * (size_t)h);
    return im;
}

Image *image_copy(const Image *src) {
    Image *im = image_new(src->w, src->h);
    memcpy(im->px, src->px, sizeof(u32) * (size_t)src->w * (size_t)src->h);
    im->alpha = src->alpha;
    return im;
}

void image_free(Image *im) {
    if (!im) return;
    free(im->px);
    free(im);
}

Image *bmp_decode(const u8 *d, size_t n) {
    if (n < 54 || d[0] != 'B' || d[1] != 'M') return NULL;
    u32 off = rd32(d + 10);
    int w = (int)(i32)rd32(d + 18), h = (int)(i32)rd32(d + 22);
    int bpp = rd16(d + 28);
    u32 comp = rd32(d + 30);
    bool bottomUp = h > 0;
    if (h < 0) h = -h;
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384 || comp != 0) return NULL;
    if (bpp != 8 && bpp != 24 && bpp != 32) return NULL;
    size_t stride = ((size_t)w * (size_t)bpp / 8 + 3) & ~(size_t)3;
    if (off + stride * (size_t)h > n) return NULL;
    u32 pal[256] = {0};
    if (bpp == 8) {
        u32 hdr = rd32(d + 14);
        u32 colors = rd32(d + 46);
        if (!colors || colors > 256) colors = 256;
        const u8 *p = d + 14 + hdr;
        for (u32 i = 0; i < colors && p + 4 <= d + off; i++, p += 4) pal[i] = (u32)p[2] | (u32)p[1] << 8 | (u32)p[0] << 16 | 0xff000000u;
    }
    Image *im = image_new(w, h);
    im->alpha = bpp == 32;
    for (int y = 0; y < h; y++) {
        const u8 *row = d + off + stride * (size_t)(bottomUp ? h - 1 - y : y);
        u32 *o = im->px + (size_t)y * (size_t)w;
        if (bpp == 32)
            for (int x = 0; x < w; x++) o[x] = (u32)row[x * 4 + 2] | (u32)row[x * 4 + 1] << 8 | (u32)row[x * 4] << 16 | (u32)row[x * 4 + 3] << 24;
        else if (bpp == 24)
            for (int x = 0; x < w; x++) o[x] = (u32)row[x * 3 + 2] | (u32)row[x * 3 + 1] << 8 | (u32)row[x * 3] << 16 | 0xff000000u;
        else
            for (int x = 0; x < w; x++) o[x] = pal[row[x]];
    }
    return im;
}

Image *eg_image_load(EG *e, const char *name) {
    char buf[256];
    snprintf(buf, sizeof buf, "%s.bmp", name);
    size_t n = 0;
    u8 *d = NULL;
    const PakEntry *pe = pak_find(&e->pak[PK_CG], buf);
    if (pe) d = pak_read(&e->pak[PK_CG], pe, &n);
    if (!d) {
        char path[2048];
        snprintf(path, sizeof path, "%s/CG/%s", e->dataDir, buf);
        d = eg_read_file(path, &n);
    }
    if (!d) return NULL;
    Image *im = bmp_decode(d, n);
    free(d);
    if (!im) eg_log(e, "can't decode picture %s", name);
    return im;
}

#include "sa.h"

void sa_blit(u32 *dst, const Image *im, int x, int y) {
    if (!im) return;
    for (int j = 0; j < im->h; j++) {
        int dy = y + j;
        if (dy < 0 || dy >= SA_H) continue;
        const u32 *src = im->px + (size_t)j * im->w;
        u32 *d = dst + (size_t)dy * SA_W;
        for (int i = 0; i < im->w; i++) {
            int dx = x + i;
            if (dx < 0 || dx >= SA_W) continue;
            u32 p = src[i], a = p >> 24;
            if (!a) continue;
            if (a == 255) {
                d[dx] = p;
                continue;
            }
            u32 q = d[dx], ia = 255 - a;
            u32 r = ((p & 0xff) * a + (q & 0xff) * ia) / 255;
            u32 g = ((p >> 8 & 0xff) * a + (q >> 8 & 0xff) * ia) / 255;
            u32 b = ((p >> 16 & 0xff) * a + (q >> 16 & 0xff) * ia) / 255;
            d[dx] = 0xff000000u | b << 16 | g << 8 | r;
        }
    }
}

void sa_blit_alpha(u32 *dst, const Image *im, int x, int y, u32 alpha) {
    if (!im) return;
    for (int j = 0; j < im->h; j++) {
        int dy = y + j;
        if (dy < 0 || dy >= SA_H) continue;
        for (int i = 0; i < im->w; i++) {
            int dx = x + i;
            if (dx < 0 || dx >= SA_W) continue;
            u32 p = im->px[(size_t)j * im->w + i], a = (p >> 24) * alpha / 255;
            if (!a) continue;
            u32 q = dst[(size_t)dy * SA_W + dx], ia = 255 - a;
            u32 r = ((p & 0xff) * a + (q & 0xff) * ia) / 255;
            u32 g = ((p >> 8 & 0xff) * a + (q >> 8 & 0xff) * ia) / 255;
            u32 b = ((p >> 16 & 0xff) * a + (q >> 16 & 0xff) * ia) / 255;
            dst[(size_t)dy * SA_W + dx] = 0xff000000u | b << 16 | g << 8 | r;
        }
    }
}

void sa_blit_rect(u32 *dst, const Image *im, int sx, int sy, int w, int h, int x, int y) {
    if (!im) return;
    for (int j = 0; j < h; j++) {
        int dy = y + j, syy = sy + j;
        if (dy < 0 || dy >= SA_H || syy < 0 || syy >= im->h) continue;
        for (int i = 0; i < w; i++) {
            int dx = x + i, sxx = sx + i;
            if (dx < 0 || dx >= SA_W || sxx < 0 || sxx >= im->w) continue;
            u32 p = im->px[(size_t)syy * im->w + sxx], a = p >> 24;
            if (!a) continue;
            u32 *d = &dst[(size_t)dy * SA_W + dx];
            if (a == 255) {
                *d = p;
                continue;
            }
            u32 q = *d, ia = 255 - a;
            u32 r = ((p & 0xff) * a + (q & 0xff) * ia) / 255;
            u32 g = ((p >> 8 & 0xff) * a + (q >> 8 & 0xff) * ia) / 255;
            u32 b = ((p >> 16 & 0xff) * a + (q >> 16 & 0xff) * ia) / 255;
            *d = 0xff000000u | b << 16 | g << 8 | r;
        }
    }
}

void sa_fill(u32 *dst, int x, int y, int w, int h, u32 rgba) {
    u32 a = rgba >> 24;
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= SA_H) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= SA_W) continue;
            u32 q = dst[(size_t)j * SA_W + i], ia = 255 - a;
            u32 r = ((rgba & 0xff) * a + (q & 0xff) * ia) / 255;
            u32 g = ((rgba >> 8 & 0xff) * a + (q >> 8 & 0xff) * ia) / 255;
            u32 b = ((rgba >> 16 & 0xff) * a + (q >> 16 & 0xff) * ia) / 255;
            dst[(size_t)j * SA_W + i] = 0xff000000u | b << 16 | g << 8 | r;
        }
    }
}

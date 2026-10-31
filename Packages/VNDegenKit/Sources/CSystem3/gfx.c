// The drawing surface (the game's DIB), palettes, and presenting the view to the host.
#include "s3.h"

void s3_gfx_init(S3 *s, int w, int h, int depth) {
    free(s->dib.p8);
    free(s->dib.p32);
    memset(&s->dib, 0, sizeof s->dib);
    if (w <= 0 || h <= 0 || w > 4096 || h > 8192) { w = 640; h = 480; }
    s->dib.w = w;
    s->dib.h = h;
    s->dib.depth = depth == 8 ? 8 : 24;
    if (s->dib.depth == 8) s->dib.p8 = s3_alloc((size_t)w * h);
    else s->dib.p32 = s3_alloc((size_t)w * h * 4);
    if (s->viewW <= 0) { s->viewW = w < 640 ? w : 640; s->viewH = h < 480 ? h : 480; }
    s3_mark_dirty(s);
}

void s3_mark_dirty(S3 *s) { s->dirty = true; }

u32 s3_rgb(S3 *s, int color) {
    color &= 255;
    return (u32)s->pal[color][0] << 16 | (u32)s->pal[color][1] << 8 | s->pal[color][2];
}

bool s3_clip(S3 *s, int *x, int *y, int *w, int *h) {
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > s->dib.w) *w = s->dib.w - *x;
    if (*y + *h > s->dib.h) *h = s->dib.h - *y;
    return *w > 0 && *h > 0;
}

// CP: fills the 4-connected area of the start pixel's colour with `color`, scanning runs of a row at a time.
void s3_paint(S3 *s, int x, int y, int color) {
    int W = s->dib.w, H = s->dib.h;
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    bool p8 = s->dib.depth == 8;
    u32 from = p8 ? s->dib.p8[(size_t)y * W + x] : s->dib.p32[(size_t)y * W + x];
    u32 to = p8 ? (u32)(color & 255) : s3_rgb(s, color);
    if (from == to) return;
    #define PX(i) (p8 ? (u32)s->dib.p8[i] : s->dib.p32[i])
    size_t cap = 1024, n = 0;
    int *stack = s3_alloc(sizeof(int) * 2 * cap);
    stack[n * 2] = x; stack[n * 2 + 1] = y; n++;
    while (n) {
        n--;
        int sx = stack[n * 2], sy = stack[n * 2 + 1];
        size_t row = (size_t)sy * W;
        if (PX(row + sx) != from) continue;
        int l = sx, r = sx;
        while (l > 0 && PX(row + l - 1) == from) l--;
        while (r < W - 1 && PX(row + r + 1) == from) r++;
        for (int i = l; i <= r; i++) { if (p8) s->dib.p8[row + i] = (u8)to; else s->dib.p32[row + i] = to; }
        for (int dy = -1; dy <= 1; dy += 2) {
            int ny = sy + dy;
            if (ny < 0 || ny >= H) continue;
            size_t nrow = (size_t)ny * W;
            for (int i = l; i <= r; i++) {
                if (PX(nrow + i) != from || (i > l && PX(nrow + i - 1) == from)) continue; // one seed per run
                if (n == cap) { cap *= 2; stack = realloc(stack, sizeof(int) * 2 * cap); if (!stack) abort(); }
                stack[n * 2] = i; stack[n * 2 + 1] = ny; n++;
            }
        }
    }
    #undef PX
    free(stack);
    s3_mark_dirty(s);
}

void s3_fill(S3 *s, int x, int y, int w, int h, int color) {
    if (!s3_clip(s, &x, &y, &w, &h)) return;
    if (s->dib.depth == 8) {
        for (int j = 0; j < h; j++) memset(s->dib.p8 + (size_t)(y + j) * s->dib.w + x, color & 255, (size_t)w);
    } else {
        u32 c = s3_rgb(s, color);
        for (int j = 0; j < h; j++) {
            u32 *row = s->dib.p32 + (size_t)(y + j) * s->dib.w + x;
            for (int i = 0; i < w; i++) row[i] = c;
        }
    }
    s3_mark_dirty(s);
}

// Clips a copy against both the source and the destination.
static bool clip2(S3 *s, int *sx, int *sy, int *w, int *h, int *dx, int *dy) {
    if (*sx < 0) { *w += *sx; *dx -= *sx; *sx = 0; }
    if (*sy < 0) { *h += *sy; *dy -= *sy; *sy = 0; }
    if (*dx < 0) { *w += *dx; *sx -= *dx; *dx = 0; }
    if (*dy < 0) { *h += *dy; *sy -= *dy; *dy = 0; }
    int W = s->dib.w, H = s->dib.h;
    if (*sx + *w > W) *w = W - *sx;
    if (*dx + *w > W) *w = W - *dx;
    if (*sy + *h > H) *h = H - *sy;
    if (*dy + *h > H) *h = H - *dy;
    return *w > 0 && *h > 0;
}

void s3_copy(S3 *s, int sx, int sy, int w, int h, int dx, int dy) {
    if (!clip2(s, &sx, &sy, &w, &h, &dx, &dy)) return;
    int W = s->dib.w;
    bool up = dy > sy;
    for (int k = 0; k < h; k++) {
        int j = up ? h - 1 - k : k;
        if (s->dib.depth == 8) memmove(s->dib.p8 + (size_t)(dy + j) * W + dx, s->dib.p8 + (size_t)(sy + j) * W + sx, (size_t)w);
        else memmove(s->dib.p32 + (size_t)(dy + j) * W + dx, s->dib.p32 + (size_t)(sy + j) * W + sx, (size_t)w * 4);
    }
    s3_mark_dirty(s);
}

void s3_copy_key(S3 *s, int sx, int sy, int w, int h, int dx, int dy, int key) {
    if (!clip2(s, &sx, &sy, &w, &h, &dx, &dy)) return;
    int W = s->dib.w;
    bool up = dy > sy;
    if (s->dib.depth == 8) {
        u8 *tmp = s3_alloc((size_t)w);
        for (int k = 0; k < h; k++) {
            int j = up ? h - 1 - k : k;
            memcpy(tmp, s->dib.p8 + (size_t)(sy + j) * W + sx, (size_t)w);
            u8 *d = s->dib.p8 + (size_t)(dy + j) * W + dx;
            for (int i = 0; i < w; i++) if (tmp[i] != (u8)key) d[i] = tmp[i];
        }
        free(tmp);
    } else {
        u32 kc = s3_rgb(s, key);
        u32 *tmp = s3_alloc((size_t)w * 4);
        for (int k = 0; k < h; k++) {
            int j = up ? h - 1 - k : k;
            memcpy(tmp, s->dib.p32 + (size_t)(sy + j) * W + sx, (size_t)w * 4);
            u32 *d = s->dib.p32 + (size_t)(dy + j) * W + dx;
            for (int i = 0; i < w; i++) if (tmp[i] != kc) d[i] = tmp[i];
        }
        free(tmp);
    }
    s3_mark_dirty(s);
}

// Draws a decoded CG. key is a palette index left transparent, or -1. 16-bit CGs with alpha
// blend; PD darkens CGs drawn on a high-colour DIB.
void s3_draw_image(S3 *s, const S3Image *img, int x, int y, int key) {
    int W = s->dib.w, H = s->dib.h;
    for (int j = 0; j < img->h; j++) {
        int dy = y + j;
        if (dy < 0 || dy >= H) continue;
        for (int i = 0; i < img->w; i++) {
            int dx = x + i;
            if (dx < 0 || dx >= W) continue;
            size_t si = (size_t)j * img->w + i, di = (size_t)dy * W + dx;
            if (img->bpp == 8) {
                u8 c = img->pixels8[si];
                if (key >= 0 && c == key) continue;
                if (s->dib.depth == 8) s->dib.p8[di] = c;
                else {
                    int r = img->pal[c][0], g = img->pal[c][1], b = img->pal[c][2];
                    if (s->pdLevel < 255) { r = r * s->pdLevel / 255; g = g * s->pdLevel / 255; b = b * s->pdLevel / 255; }
                    s->dib.p32[di] = (u32)r << 16 | (u32)g << 8 | (u32)b;
                }
            } else {
                u16 v = img->pixels16[si];
                int r = (v >> 11) << 3, g = ((v >> 5) & 63) << 2, b = (v & 31) << 3;
                r |= r >> 5; g |= g >> 6; b |= b >> 5;
                if (s->pdLevel < 255) { r = r * s->pdLevel / 255; g = g * s->pdLevel / 255; b = b * s->pdLevel / 255; }
                if (s->dib.depth == 8) continue; // a high-colour CG on a 256-colour DIB has nowhere to go
                u32 *d = &s->dib.p32[di];
                if (img->alpha) {
                    int a = img->alpha[si];
                    if (a == 0) continue;
                    if (a < 255) {
                        int dr = (*d >> 16) & 255, dg = (*d >> 8) & 255, db = *d & 255;
                        r = dr + (r - dr) * a / 255; g = dg + (g - dg) * a / 255; b = db + (b - db) * a / 255;
                    }
                }
                *d = (u32)r << 16 | (u32)g << 8 | (u32)b;
            }
        }
    }
    s3_mark_dirty(s);
}

// Builds the host's frame from the view rectangle. The screen palette (8 bit) and the fade level
// apply here, so fades never touch the DIB.
void s3_gfx_present(S3 *s) {
    if (!s->dirty) return;
    s->dirty = false;
    int vw = s->viewW > 0 ? s->viewW : 640, vh = s->viewH > 0 ? s->viewH : 480;
    if (vw != s->screenW || vh != s->screenH || !s->screen) {
        free(s->screen);
        s->screen = s3_alloc((size_t)vw * vh * 4);
        s->screenW = vw;
        s->screenH = vh;
    }
    if (!s->flip) return;
    u32 lut[256];
    for (int i = 0; i < 256; i++) lut[i] = (u32)s->shownPal[i][0] << 16 | (u32)s->shownPal[i][1] << 8 | s->shownPal[i][2];
    int f = s->fadeLevel;
    for (int y = 0; y < vh; y++) {
        u32 *o = s->screen + (size_t)y * vw;
        int sy = s->viewY + y;
        for (int x = 0; x < vw; x++) {
            int sx = s->viewX + x;
            u32 c = 0;
            if (sx >= 0 && sy >= 0 && sx < s->dib.w && sy < s->dib.h) {
                size_t i = (size_t)sy * s->dib.w + sx;
                c = s->dib.depth == 8 ? lut[s->dib.p8[i]] : s->dib.p32[i];
            }
            if (f) {
                int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
                if (s->fadeWhite) { r += (255 - r) * f / 255; g += (255 - g) * f / 255; b += (255 - b) * f / 255; }
                else { r -= r * f / 255; g -= g * f / 255; b -= b * f / 255; }
                c = (u32)r << 16 | (u32)g << 8 | (u32)b;
            }
            o[x] = 0xff000000u | c;
        }
    }
    s->serial++;
}

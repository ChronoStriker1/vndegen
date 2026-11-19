#include "br.h"

static const char *const regionNames[4] = {"ff_00", "ff_01", "ff_02", "ff_03"};

static void load_sheet(BR *e, int r, int big) {
    FontSheet *s = &e->font[big][r];
    if (s->tried) return;
    s->tried = true;
    char rel[128];
    snprintf(rel, sizeof rel, "font/%s%s.bmp", regionNames[r], big ? "l" : "");
    size_t n;
    u8 *d = br_read_game(e, rel, &n);
    if (!d) {
        br_log(e, "font %s is missing", rel);
        return;
    }
    s->px = image_decode_gray(d, n, &s->w, &s->h);
    free(d);
}

static bool glyph(BR *e, const u8 *p, int big, const u8 **px, int *stride, int *cell, int *half) {
    int size = big ? 64 : 32;
    int r, idx;
    *half = 0;
    if (p[0] < 0x80) {
        r = 3;
        idx = p[0] - 0x20;
        *half = 1;
        if (idx < 0 || idx >= 96) return false;
    } else if (p[0] >= 0x81 && p[0] <= 0x8f) {
        r = 0;
        idx = (p[0] - 0x81) * 192 + (p[1] - 0x40);
    } else if (p[0] >= 0x90 && p[0] <= 0x9f) {
        r = 1;
        idx = (p[0] - 0x90) * 192 + (p[1] - 0x40);
    } else if (p[0] >= 0xe0 && p[0] <= 0xef) {
        r = 2;
        idx = (p[0] - 0xe0) * 192 + (p[1] - 0x40);
    } else return false;
    if (idx < 0) return false;
    load_sheet(e, r, big);
    FontSheet *s = &e->font[big][r];
    if (!s->px) return false;
    int cw = *half ? size / 2 : size;
    int perRow = s->w / cw;
    int gx = idx % perRow * cw, gy = idx / perRow * size;
    if (gy + size > s->h) return false;
    *px = s->px + (size_t)gy * s->w + gx;
    *stride = s->w;
    *cell = size;
    return true;
}

int font_char_width(const u8 *p, int size) { return p[0] < 0x80 ? (size + 1) / 2 : size; }

int font_text_width(const char *sjis, int size) {
    int w = 0;
    for (const u8 *p = (const u8 *)sjis; *p;) {
        w += font_char_width(p, size);
        p += br_sjis_len(p);
    }
    return w;
}

static inline void put(u32 *d, u32 color, int a) {
    if (a <= 0) return;
    if (a > 255) a = 255;
    int ia = 255 - a;
    u32 o = *d;
    u32 r = ((color & 0xff) * a + (o & 0xff) * ia) / 255;
    u32 g = ((color >> 8 & 0xff) * a + (o >> 8 & 0xff) * ia) / 255;
    u32 b = ((color >> 16 & 0xff) * a + (o >> 16 & 0xff) * ia) / 255;
    *d = r | g << 8 | b << 16 | 0xff000000u;
}

static int sample(const u8 *px, int stride, int cell, int cw, int size, int x, int y) {
    if (x < 0 || y < 0) return 0;
    int x0 = x * cell / size, x1 = (x + 1) * cell / size, y0 = y * cell / size, y1 = (y + 1) * cell / size;
    if (x1 <= x0) x1 = x0 + 1;
    if (y1 <= y0) y1 = y0 + 1;
    if (x0 >= cw || y0 >= cell) return 0;
    if (x1 > cw) x1 = cw;
    if (y1 > cell) y1 = cell;
    int sum = 0, n = 0, mx = 0;
    for (int sy = y0; sy < y1; sy++)
        for (int sx = x0; sx < x1; sx++) {
            int v = px[(size_t)sy * stride + sx];
            sum += v;
            if (v > mx) mx = v;
            n++;
        }
    int avg = sum / n;
    return (avg * 3 + mx) / 4;
}

void font_draw_char(BR *e, u32 *dst, int dw, int dh, const u8 *p, int x, int y, int size, u32 color, u32 edge, int edgeW, int alpha) {
    const u8 *px;
    int stride, cell, half;
    if (!glyph(e, p, size > 32, &px, &stride, &cell, &half)) return;
    int cw = half ? cell / 2 : cell;
    int w = half ? (size + 1) / 2 : size;
    if (edgeW > 0) {
        for (int yy = -edgeW; yy < size + edgeW; yy++) {
            int ty = y + yy;
            if (ty < 0 || ty >= dh) continue;
            for (int xx = -edgeW; xx < w + edgeW; xx++) {
                int tx = x + xx;
                if (tx < 0 || tx >= dw) continue;
                int m = 0;
                for (int oy = -edgeW; oy <= edgeW && m < 255; oy++)
                    for (int ox = -edgeW; ox <= edgeW; ox++) {
                        int v = sample(px, stride, cell, cw, size, xx + ox, yy + oy);
                        if (v > m) m = v;
                    }
                put(&dst[(size_t)ty * dw + tx], edge, m * alpha / 255);
            }
        }
    }
    for (int yy = 0; yy < size; yy++) {
        int ty = y + yy;
        if (ty < 0 || ty >= dh) continue;
        for (int xx = 0; xx < w; xx++) {
            int tx = x + xx;
            if (tx < 0 || tx >= dw) continue;
            int v = sample(px, stride, cell, cw, size, xx, yy);
            put(&dst[(size_t)ty * dw + tx], color, v * alpha / 255);
        }
    }
}

void font_draw(BR *e, u32 *dst, int dw, int dh, const char *sjis, int x, int y, int size, u32 color, u32 edge, int edgeW, int alpha) {
    u32 base = color;
    for (const u8 *p = (const u8 *)sjis; *p;) {
        if ((p[0] == '$' || p[0] == '#' || p[0] == '&') && p[1] && p[1] < 0x80) {
            if (p[0] == '$' && p[1] >= 'A' && p[1] <= 'K') color = color_code(e, 10 + (p[1] - 'A'));
            else if (p[0] == '$' && p[1] == 'N') color = base;
            p += 2;
            continue;
        }
        font_draw_char(e, dst, dw, dh, p, x, y, size, color, edge, edgeW, alpha);
        x += font_char_width(p, size);
        p += br_sjis_len(p);
    }
}

static inline void over(u32 *d, u32 color, int a) {
    if (a <= 0) return;
    if (a > 255) a = 255;
    u32 o = *d;
    int oa = (int)(o >> 24);
    int na = a + oa * (255 - a) / 255;
    if (na <= 0) return;
    u32 r = (u32)(((int)(color & 0xff) * a + (int)(o & 0xff) * oa * (255 - a) / 255) / na);
    u32 g = (u32)(((int)(color >> 8 & 0xff) * a + (int)(o >> 8 & 0xff) * oa * (255 - a) / 255) / na);
    u32 b = (u32)(((int)(color >> 16 & 0xff) * a + (int)(o >> 16 & 0xff) * oa * (255 - a) / 255) / na);
    *d = r | g << 8 | b << 16 | (u32)na << 24;
}

void font_draw_layer(BR *e, Image *L, const u8 *p, int x, int y, int size, u32 color, u32 edge, int edgeW) {
    const u8 *px;
    int stride, cell, half;
    if (!glyph(e, p, size > 32, &px, &stride, &cell, &half)) return;
    int cw = half ? cell / 2 : cell;
    int w = half ? (size + 1) / 2 : size;
    for (int yy = -edgeW; yy < size + edgeW; yy++) {
        int ty = y + yy;
        if (ty < 0 || ty >= L->h) continue;
        for (int xx = -edgeW; xx < w + edgeW; xx++) {
            int tx = x + xx;
            if (tx < 0 || tx >= L->w) continue;
            int v = sample(px, stride, cell, cw, size, xx, yy);
            int m = 0;
            if (edgeW)
                for (int oy = -edgeW; oy <= edgeW && m < 255; oy++)
                    for (int ox = -edgeW; ox <= edgeW; ox++) {
                        int s2 = sample(px, stride, cell, cw, size, xx + ox, yy + oy);
                        if (s2 > m) m = s2;
                    }
            u32 *d = &L->px[(size_t)ty * L->w + tx];
            if (m) over(d, edge, m);
            if (v) over(d, color, v);
        }
    }
}

void font_free(BR *e) {
    for (int b = 0; b < 2; b++)
        for (int r = 0; r < 4; r++) {
            free(e->font[b][r].px);
            memset(&e->font[b][r], 0, sizeof e->font[b][r]);
        }
}

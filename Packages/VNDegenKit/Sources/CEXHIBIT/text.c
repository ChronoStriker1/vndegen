#include "ex.h"

#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"

typedef struct Glyph {
    u32 cp;
    int size;
    int x0, y0, w, h, adv;
    u8 *bm;
} Glyph;

typedef struct Fonts {
    u8 *data;
    stbtt_fontinfo info;
    bool ok;
    Glyph *glyphs;
    int count, cap;
} Fonts;

static Fonts *fonts(EX *e) {
    if (e->fonts) return e->fonts;
    Fonts *f = ex_alloc(sizeof *f);
    e->fonts = f;
    const char *rel = e->fontFile ? e->fontFile : "res/NotoSans-Medium.ttf";
    char *path = ex_game_path(e, rel);
    size_t n;
    f->data = ex_read_file(path, &n);
    free(path);
    int off = f->data ? stbtt_GetFontOffsetForIndex(f->data, 0) : -1;
    f->ok = off >= 0 && stbtt_InitFont(&f->info, f->data, off);
    if (!f->ok) ex_log(e, "can't read the font %s", rel);
    return f;
}

void text_free(EX *e) {
    Fonts *f = e->fonts;
    if (!f) return;
    for (int i = 0; i < f->count; i++) free(f->glyphs[i].bm);
    free(f->glyphs);
    free(f->data);
    free(f);
    e->fonts = NULL;
}

static Glyph *glyph(Fonts *f, u32 cp, int size) {
    for (int i = f->count - 1; i >= 0; i--)
        if (f->glyphs[i].cp == cp && f->glyphs[i].size == size) return &f->glyphs[i];
    if (f->count == f->cap) {
        f->cap = f->cap ? f->cap * 2 : 256;
        f->glyphs = ex_realloc(f->glyphs, sizeof(Glyph) * (size_t)f->cap);
    }
    Glyph *g = &f->glyphs[f->count++];
    memset(g, 0, sizeof *g);
    g->cp = cp;
    g->size = size;
    float sc = stbtt_ScaleForMappingEmToPixels(&f->info, (float)size);
    int adv, lsb;
    stbtt_GetCodepointHMetrics(&f->info, (int)cp, &adv, &lsb);
    g->adv = (int)lroundf((float)adv * sc);
    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(&f->info, (int)cp, sc, sc, &x0, &y0, &x1, &y1);
    g->x0 = x0;
    g->y0 = y0;
    g->w = x1 - x0;
    g->h = y1 - y0;
    if (g->w > 0 && g->h > 0) {
        g->bm = ex_alloc((size_t)g->w * (size_t)g->h);
        stbtt_MakeCodepointBitmap(&f->info, g->bm, g->w, g->h, g->w, sc, sc, (int)cp);
    }
    return g;
}

int text_advance(EX *e, u32 cp, int size) {
    Fonts *f = fonts(e);
    if (!f->ok) return size / 2;
    if (cp == '\n') return 0;
    return glyph(f, cp, size)->adv;
}

int text_ascent(EX *e, int size) {
    Fonts *f = fonts(e);
    if (!f->ok) return size;
    int a, d, g;
    stbtt_GetFontVMetrics(&f->info, &a, &d, &g);
    return (int)lroundf((float)a * stbtt_ScaleForMappingEmToPixels(&f->info, (float)size));
}

int text_descent(EX *e, int size) {
    Fonts *f = fonts(e);
    if (!f->ok) return 0;
    int a, d, g;
    stbtt_GetFontVMetrics(&f->info, &a, &d, &g);
    return (int)lroundf((float)-d * stbtt_ScaleForMappingEmToPixels(&f->info, (float)size));
}

static void put(u32 *dst, int dw, int dh, const Glyph *g, int ox, int oy, u32 color, int alpha, bool italic) {
    for (int j = 0; j < g->h; j++) {
        int y = oy + j;
        if (y < 0 || y >= dh) continue;
        int shift = italic ? -(g->y0 + j) / 5 : 0;
        for (int i = 0; i < g->w; i++) {
            int x = ox + i + shift;
            if (x < 0 || x >= dw) continue;
            u32 a = (u32)g->bm[(size_t)j * (size_t)g->w + (size_t)i] * (u32)alpha / 255;
            if (!a) continue;
            u32 *d = &dst[(size_t)y * (size_t)dw + (size_t)x];
            u32 ia = 255 - a, dp = *d;
            u32 r = ((color & 0xff) * a + (dp & 0xff) * ia) / 255;
            u32 gg = ((color >> 8 & 0xff) * a + (dp >> 8 & 0xff) * ia) / 255;
            u32 b = ((color >> 16 & 0xff) * a + (dp >> 16 & 0xff) * ia) / 255;
            *d = r | gg << 8 | b << 16 | 0xff000000u;
        }
    }
}

void text_glyph(EX *e, u32 *dst, int dw, int dh, u32 cp, int x, int baseline, int size, u32 color, int alpha, bool italic) {
    Fonts *f = fonts(e);
    if (!f->ok || alpha <= 0) return;
    Glyph *g = glyph(f, cp, size);
    if (g->bm) put(dst, dw, dh, g, x + g->x0, baseline + g->y0, color, alpha, italic);
}

int text_width(EX *e, const u32 *cps, int n, int size) {
    int w = 0;
    for (int i = 0; i < n; i++) w += text_advance(e, cps[i], size);
    return w;
}

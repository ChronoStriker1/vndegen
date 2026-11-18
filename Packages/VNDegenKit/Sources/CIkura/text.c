#include "ikura.h"
#include "vnfont.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"

struct Glyph {
    u32 cp;
    int x0, y0, w, h, adv;
    u8 *bm;
};

static void font_free(Font *f) {
    for (int i = 0; i < f->count; i++) free(f->glyphs[i].bm);
    free(f->glyphs);
    free(f->info);
    free(f->data);
    memset(f, 0, sizeof *f);
}

void text_close(IK *t) {
    for (int i = 0; i < IK_FONTS; i++) font_free(&t->fonts[i]);
}

void font_set(IK *t, int slot, const char *name, int size, int shadow, bool bold) {
    if (slot < 0 || slot >= IK_FONTS) return;
    Font *f = &t->fonts[slot];
    font_free(f);
    f->size = size;
    f->shadow = shadow;
    vnfont_match m;
    if (!vnfont_resolve(name, bold, 0, &m) && !vnfont_resolve("Times New Roman", bold, 0, &m)) {
        ik_log(t, "no substitute for %s", name);
        return;
    }
    size_t n;
    f->data = ik_read_file(m.path, &n);
    stbtt_fontinfo *info = ik_alloc(sizeof *info);
    f->info = info;
    int off = f->data ? stbtt_GetFontOffsetForIndex(f->data, m.face) : -1;
    if (off < 0 || !stbtt_InitFont(info, f->data, off)) {
        ik_log(t, "can't read font %s", m.path);
        return;
    }
    f->scale = stbtt_ScaleForMappingEmToPixels(info, (float)size);
    int asc, desc, gap;
    stbtt_GetFontVMetrics(info, &asc, &desc, &gap);
    f->ascent = (int)((float)asc * f->scale + 0.5f);
    f->ok = true;
    ik_log(t, "font %d: %s %d -> %s", slot, name, size, m.path);
}

static Glyph *glyph(Font *f, u32 cp) {
    for (int i = 0; i < f->count; i++)
        if (f->glyphs[i].cp == cp) return &f->glyphs[i];
    if (f->count == f->cap) {
        f->cap = f->cap ? f->cap * 2 : 128;
        f->glyphs = ik_realloc(f->glyphs, sizeof(Glyph) * (size_t)f->cap);
    }
    Glyph *g = &f->glyphs[f->count++];
    memset(g, 0, sizeof *g);
    g->cp = cp;
    stbtt_fontinfo *info = f->info;
    int adv, lsb;
    stbtt_GetCodepointHMetrics(info, (int)cp, &adv, &lsb);
    g->adv = (int)((float)adv * f->scale + 0.5f);
    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(info, (int)cp, f->scale, f->scale, &x0, &y0, &x1, &y1);
    g->x0 = x0;
    g->y0 = y0;
    g->w = x1 - x0;
    g->h = y1 - y0;
    if (g->w > 0 && g->h > 0) {
        g->bm = ik_alloc((size_t)g->w * (size_t)g->h);
        stbtt_MakeCodepointBitmap(info, g->bm, g->w, g->h, g->w, f->scale, f->scale, (int)cp);
    }
    return g;
}

int text_advance(IK *t, int slot, u32 cp) {
    Font *f = &t->fonts[slot & (IK_FONTS - 1)];
    return f->ok ? glyph(f, cp)->adv : 0;
}

static void put(IK *t, const Glyph *g, int x, int y, u32 argb, const int clip[4]) {
    u32 ca = argb >> 24, cr = (argb >> 16) & 0xff, cg = (argb >> 8) & 0xff, cb = argb & 0xff;
    for (int j = 0; j < g->h; j++) {
        int py = y + g->y0 + j;
        if (py < clip[1] || py >= clip[3] || py < 0 || py >= t->H) continue;
        u32 *row = t->screen + (size_t)py * t->W;
        for (int i = 0; i < g->w; i++) {
            int px = x + g->x0 + i;
            if (px < clip[0] || px >= clip[2] || px < 0 || px >= t->W) continue;
            u32 a = g->bm[j * g->w + i] * ca / 255;
            if (!a) continue;
            u32 d = row[px], ia = 255 - a;
            row[px] = ((cr * a + (d & 0xff) * ia) / 255) | (((cg * a + ((d >> 8) & 0xff) * ia) / 255) << 8) |
                      (((cb * a + ((d >> 16) & 0xff) * ia) / 255) << 16) | 0xff000000u;
        }
    }
}

void text_draw(IK *t, int slot, u32 cp, int x, int y, u32 colour, u32 shadow, const int clip[4]) {
    Font *f = &t->fonts[slot & (IK_FONTS - 1)];
    if (!f->ok || cp == ' ') return;
    Glyph *g = glyph(f, cp);
    if (!g->bm) return;
    int by = y + f->ascent;
    if (f->shadow && shadow >> 24) put(t, g, x + f->shadow, by + f->shadow, shadow, clip);
    put(t, g, x, by, colour, clip);
}

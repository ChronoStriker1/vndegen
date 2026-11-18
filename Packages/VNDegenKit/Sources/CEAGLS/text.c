#include "eagls.h"
#include "vnfont.h"

#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"

typedef struct Glyph {
    u32 cp;
    int size;
    int x0, y0, w, h;
    u8 *bm;
} Glyph;

static struct {
    u8 *data;
    stbtt_fontinfo info;
    bool ok, tried;
    Glyph *glyphs;
    int count, cap;
} font;

void text_init(EG *e) {
    if (font.tried) return;
    font.tried = true;
    vnfont_match m;
    if (!vnfont_resolve("MS Gothic", 0, 0, &m)) {
        eg_log(e, "no font for the message window");
        return;
    }
    size_t size;
    font.data = eg_read_file(m.path, &size);
    int off = font.data ? stbtt_GetFontOffsetForIndex(font.data, m.face) : -1;
    font.ok = off >= 0 && stbtt_InitFont(&font.info, font.data, off);
    if (!font.ok) eg_log(e, "can't read font %s", m.path);
}

static bool half_width(u32 cp) { return cp < 0x80 || (cp >= 0xff61 && cp <= 0xff9f); }

static Glyph *glyph(u32 cp, int size) {
    for (int i = 0; i < font.count; i++)
        if (font.glyphs[i].cp == cp && font.glyphs[i].size == size) return &font.glyphs[i];
    if (font.count == font.cap) {
        font.cap = font.cap ? font.cap * 2 : 256;
        font.glyphs = eg_realloc(font.glyphs, sizeof(Glyph) * (size_t)font.cap);
    }
    Glyph *g = &font.glyphs[font.count++];
    memset(g, 0, sizeof *g);
    g->cp = cp;
    g->size = size;
    float sc = stbtt_ScaleForMappingEmToPixels(&font.info, (float)size);
    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(&font.info, (int)cp, sc, sc, &x0, &y0, &x1, &y1);
    g->x0 = x0;
    g->y0 = y0;
    g->w = x1 - x0;
    g->h = y1 - y0;
    if (g->w > 0 && g->h > 0) {
        g->bm = eg_alloc((size_t)g->w * (size_t)g->h);
        stbtt_MakeCodepointBitmap(&font.info, g->bm, g->w, g->h, g->w, sc, sc, (int)cp);
    }
    return g;
}

int text_advance(u32 cp, int size, int spacing) {
    int full = size * 8 + spacing * 10;
    return half_width(cp) ? full / 2 : full;
}

int text_width(const char *sjis, int size, int spacing) {
    int w = 0;
    for (const u8 *p = (const u8 *)sjis; *p;) {
        u32 cp;
        p += eg_sjis_decode(p, &cp);
        w += text_advance(cp, size, spacing);
    }
    return w / 8;
}

static void put_glyph(u32 *dst, int dw, int dh, Glyph *g, int ox, int oy, u32 color) {
    for (int j = 0; j < g->h; j++) {
        int y = oy + j;
        if (y < 0 || y >= dh) continue;
        for (int i = 0; i < g->w; i++) {
            int x = ox + i;
            if (x < 0 || x >= dw) continue;
            if (g->bm[(size_t)j * g->w + i] < 128) continue;
            dst[(size_t)y * dw + x] = color | 0xff000000u;
        }
    }
}

static void put_outline(u32 *dst, int dw, int dh, Glyph *g, int ox, int oy) {
    for (int j = -1; j <= g->h; j++)
        for (int i = -1; i <= g->w; i++) {
            int x = ox + i, y = oy + j;
            if (x < 0 || y < 0 || x >= dw || y >= dh) continue;
            bool on = false;
            for (int b = -1; b <= 1 && !on; b++)
                for (int a = -1; a <= 1 && !on; a++) {
                    int gi = i + a, gj = j + b;
                    if (gi >= 0 && gj >= 0 && gi < g->w && gj < g->h && g->bm[(size_t)gj * g->w + gi] >= 128) on = true;
                }
            if (on) dst[(size_t)y * dw + x] = 0xff010101u;
        }
}

int text_draw(EG *e, u32 *dst, int dw, int dh, const char *sjis, int x, int y, int size, int spacing, u32 color, bool outline) {
    if (!font.ok) text_init(e);
    if (!font.ok) return x;
    float sc = stbtt_ScaleForMappingEmToPixels(&font.info, (float)size);
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&font.info, &ascent, &descent, &gap);
    int baseline = y + (int)lroundf((float)ascent * sc);
    int fx = x * 8;
    for (const u8 *p = (const u8 *)sjis; *p;) {
        u32 cp;
        p += eg_sjis_decode(p, &cp);
        Glyph *g = glyph(cp, size);
        if (g->bm) {
            int gx = fx / 8 + g->x0;
            if (outline) put_outline(dst, dw, dh, g, gx, baseline + g->y0);
            put_glyph(dst, dw, dh, g, gx, baseline + g->y0, color);
        }
        fx += text_advance(cp, size, spacing);
    }
    return fx / 8;
}

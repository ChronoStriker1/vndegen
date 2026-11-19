#include "bsx.h"
#include "vnfont.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"

typedef struct Glyph {
    u32 cp;
    int size;
    int x0, y0, w, h, adv;
    u8 *bm;
} Glyph;

static struct {
    u8 *data;
    stbtt_fontinfo info;
    bool ok, tried;
    Glyph *glyphs;
    int count, cap;
    int *hash;
    int hashSize;
} font;

void bx_text_init(BX *b) {
    if (font.tried) return;
    font.tried = true;
    const char *face = bx_cfg_get(&b->cfg, "default font", "fontface");
    vnfont_match m;
    if (!vnfont_resolve(face ? face : "MS Gothic", 0, 0, &m) && !vnfont_resolve("MS Gothic", 0, 0, &m)) {
        bx_log(b, "no font for the message window");
        return;
    }
    size_t size;
    font.data = bx_read_file(m.path, &size);
    int off = font.data ? stbtt_GetFontOffsetForIndex(font.data, m.face) : -1;
    font.ok = off >= 0 && stbtt_InitFont(&font.info, font.data, off);
    if (!font.ok) bx_log(b, "can't read font %s", m.path);
}

static u32 gkey(u32 cp, int size) { return cp * 31u + (u32)size; }

static void rehash(void) {
    free(font.hash);
    font.hashSize = 1024;
    while (font.hashSize < font.count * 2) font.hashSize *= 2;
    font.hash = bx_alloc(sizeof(int) * (size_t)font.hashSize);
    for (int i = 0; i < font.hashSize; i++) font.hash[i] = -1;
    for (int i = 0; i < font.count; i++) {
        u32 h = gkey(font.glyphs[i].cp, font.glyphs[i].size) & (u32)(font.hashSize - 1);
        while (font.hash[h] >= 0) h = (h + 1) & (u32)(font.hashSize - 1);
        font.hash[h] = i;
    }
}

static Glyph *glyph(u32 cp, int size) {
    if (font.hashSize) {
        u32 h = gkey(cp, size) & (u32)(font.hashSize - 1);
        while (font.hash[h] >= 0) {
            Glyph *g = &font.glyphs[font.hash[h]];
            if (g->cp == cp && g->size == size) return g;
            h = (h + 1) & (u32)(font.hashSize - 1);
        }
    }
    if (font.count == font.cap) {
        font.cap = font.cap ? font.cap * 2 : 512;
        font.glyphs = bx_realloc(font.glyphs, sizeof(Glyph) * (size_t)font.cap);
    }
    Glyph *g = &font.glyphs[font.count++];
    memset(g, 0, sizeof *g);
    g->cp = cp;
    g->size = size;
    if (font.ok) {
        float sc = stbtt_ScaleForPixelHeight(&font.info, (float)size);
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&font.info, (int)cp, &adv, &lsb);
        g->adv = (int)(adv * sc + 0.5f);
        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&font.info, (int)cp, sc, sc, &x0, &y0, &x1, &y1);
        g->x0 = x0;
        g->y0 = y0;
        g->w = x1 - x0;
        g->h = y1 - y0;
        if (g->w > 0 && g->h > 0) {
            g->bm = bx_alloc((size_t)g->w * (size_t)g->h);
            stbtt_MakeCodepointBitmap(&font.info, g->bm, g->w, g->h, g->w, sc, sc, (int)cp);
        }
    } else g->adv = size / 2;
    if (font.count * 2 > font.hashSize) rehash();
    else {
        u32 h = gkey(cp, size) & (u32)(font.hashSize - 1);
        while (font.hash[h] >= 0) h = (h + 1) & (u32)(font.hashSize - 1);
        font.hash[h] = font.count - 1;
    }
    return g;
}

static int ascent(int size) {
    if (!font.ok) return size;
    int a, d, l;
    stbtt_GetFontVMetrics(&font.info, &a, &d, &l);
    float sc = stbtt_ScaleForPixelHeight(&font.info, (float)size);
    return (int)(a * sc + 0.5f);
}

int bx_text_width(const char *s, int size) {
    int w = 0;
    while (*s) {
        u32 cp;
        s += bx_utf8_decode(s, &cp);
        if (cp == '\n') continue;
        w += glyph(cp, size)->adv;
    }
    return w;
}

static void put_glyph(u32 *dst, int dw, int dh, const Glyph *g, int x, int y, u32 color) {
    u32 ca = color >> 24;
    for (int j = 0; j < g->h; j++) {
        int py = y + j;
        if (py < 0 || py >= dh) continue;
        for (int i = 0; i < g->w; i++) {
            int px = x + i;
            if (px < 0 || px >= dw) continue;
            u32 a = g->bm[(size_t)j * g->w + i] * ca / 255;
            if (!a) continue;
            u32 *d = &dst[(size_t)py * dw + px];
            u32 ia = 255 - a;
            u32 rb = ((color & 0xff00ff) * a + (*d & 0xff00ff) * ia) >> 8 & 0xff00ff;
            u32 gg = ((color & 0x00ff00) * a + (*d & 0x00ff00) * ia) >> 8 & 0x00ff00;
            *d = rb | gg | 0xff000000u;
        }
    }
}

int bx_text_draw(BX *b, u32 *dst, int dw, int dh, const char *s, int x, int y, int size, u32 color, int edge, int shadow) {
    (void)b;
    int base = y + ascent(size);
    int cx = x;
    u32 dark = (color & 0xff000000u) | 0x000000;
    while (*s) {
        u32 cp;
        s += bx_utf8_decode(s, &cp);
        if (cp == '\n') continue;
        Glyph *g = glyph(cp, size);
        if (g->bm) {
            int gx = cx + g->x0, gy = base + g->y0;
            if (shadow) put_glyph(dst, dw, dh, g, gx + shadow, gy + shadow, (dark & 0xff000000u) / 2 & 0xff000000u);
            for (int oy = -edge; oy <= edge; oy++)
                for (int ox = -edge; ox <= edge; ox++)
                    if (ox || oy) put_glyph(dst, dw, dh, g, gx + ox, gy + oy, dark);
            put_glyph(dst, dw, dh, g, gx, gy, color);
        }
        cx += g->adv;
    }
    return cx - x;
}

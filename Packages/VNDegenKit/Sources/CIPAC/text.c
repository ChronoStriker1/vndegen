#include "ipac.h"
#include "vnfont.h"

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
    int ascent;
} font;

void text_init(IP *t) {
    if (font.tried) return;
    font.tried = true;
    vnfont_match m;
    if (!vnfont_resolve("ＭＳ ゴシック", 0, 0, &m) && !vnfont_resolve("MS Gothic", 0, 0, &m) && !vnfont_resolve("MS PGothic", 0, 0, &m)) {
        ip_log(t, "no substitute for MS Gothic");
        return;
    }
    size_t size;
    font.data = ip_read_file(m.path, &size);
    int off = font.data ? stbtt_GetFontOffsetForIndex(font.data, m.face) : -1;
    font.ok = off >= 0 && stbtt_InitFont(&font.info, font.data, off);
    if (!font.ok) ip_log(t, "can't read font %s", m.path);
}

void text_close(void) {
    for (int i = 0; i < font.count; i++) free(font.glyphs[i].bm);
    free(font.glyphs);
    free(font.data);
    memset(&font, 0, sizeof font);
}

static Glyph *glyph(u32 cp, int size) {
    for (int i = 0; i < font.count; i++)
        if (font.glyphs[i].cp == cp && font.glyphs[i].size == size) return &font.glyphs[i];
    if (font.count == font.cap) {
        font.cap = font.cap ? font.cap * 2 : 256;
        font.glyphs = ip_realloc(font.glyphs, sizeof(Glyph) * (size_t)font.cap);
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
        g->bm = ip_alloc((size_t)g->w * (size_t)g->h);
        stbtt_MakeCodepointBitmap(&font.info, g->bm, g->w, g->h, g->w, sc, sc, (int)cp);
    }
    return g;
}

static int baseline(int size) {
    int asc, desc, gap;
    stbtt_GetFontVMetrics(&font.info, &asc, &desc, &gap);
    float sc = stbtt_ScaleForMappingEmToPixels(&font.info, (float)size);
    return (int)((float)asc * sc + 0.5f);
}

static void put(u32 *dst, int dw, int dh, const Glyph *g, int x, int y, u32 color) {
    for (int j = 0; j < g->h; j++) {
        int py = y + g->y0 + j;
        if (py < 0 || py >= dh) continue;
        for (int i = 0; i < g->w; i++) {
            int px = x + g->x0 + i;
            if (px < 0 || px >= dw) continue;
            u32 a = g->bm[j * g->w + i] * (color >> 24) / 255;
            if (!a) continue;
            u32 *d = &dst[(size_t)py * dw + px], ia = 255 - a;
            u32 r = ((color & 0xff) * a + (*d & 0xff) * ia) / 255;
            u32 gg = (((color >> 8) & 0xff) * a + ((*d >> 8) & 0xff) * ia) / 255;
            u32 b = (((color >> 16) & 0xff) * a + ((*d >> 16) & 0xff) * ia) / 255;
            *d = r | gg << 8 | b << 16 | 0xff000000u;
        }
    }
}

void text_glyph(u32 *dst, int dw, int dh, u32 cp, int x, int y, int size, u32 color, u32 shadow) {
    if (!font.ok || cp == ' ' || cp == 0x3000) return;
    Glyph *g = glyph(cp, size);
    if (!g->bm) return;
    int by = y + baseline(size);
    if (shadow >> 24) put(dst, dw, dh, g, x + 1, by + 1, shadow);
    put(dst, dw, dh, g, x, by, color);
}

bool text_wide(u32 cp) { return !(cp < 0x80 || (cp >= 0xff61 && cp <= 0xff9f)); }

int text_width(const u32 *cps, int n, int pitch) {
    int w = 0;
    for (int i = 0; i < n; i++) w += text_wide(cps[i]) ? pitch : pitch / 2;
    return w;
}

void text_line(u32 *dst, int dw, int dh, const u32 *cps, int n, int x, int y, int size, int pitch, u32 color, u32 shadow) {
    for (int i = 0; i < n; i++) {
        int adv = text_wide(cps[i]) ? pitch : pitch / 2;
        int gx = x + (adv - (text_wide(cps[i]) ? size : size / 2)) / 2;
        text_glyph(dst, dw, dh, cps[i], gx, y, size, color, shadow);
        x += adv;
    }
}

u32 *text_decode(const char *sjis, int *len) {
    size_t n = strlen(sjis);
    u32 *out = ip_alloc(sizeof(u32) * (n + 1));
    int k = 0;
    for (const u8 *p = (const u8 *)sjis; *p;) {
        u32 cp;
        p += ip_sjis_decode(p, &cp);
        out[k++] = cp;
    }
    *len = k;
    return out;
}

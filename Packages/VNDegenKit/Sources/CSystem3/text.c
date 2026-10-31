// Glyph drawing with stb_truetype. The games were laid out for MS Gothic, a monospaced font, so
// every full-width character takes a square cell of the font size and every half-width character
// half of it, whatever the substitute font's own advances are.
#include "s3.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "third_party/stb_truetype.h"
#pragma GCC diagnostic pop

#define CACHE_SIZE 1024

typedef struct Glyph { u32 cp; int size; u8 half; u8 used; int w, h, ox, oy; u8 *bits; } Glyph;

typedef struct S3Font {
    u8 *data;
    stbtt_fontinfo info;
    Glyph cache[CACHE_SIZE];
} S3Font;

bool s3_font_open(S3 *s, const char *path, int index) {
    size_t n;
    u8 *d = path ? s3_read_file(path, &n) : NULL;
    if (!d) return false;
    S3Font *f = s3_alloc(sizeof *f);
    int off = stbtt_GetFontOffsetForIndex(d, index);
    if (off < 0 || !stbtt_InitFont(&f->info, d, off)) { free(d); free(f); return false; }
    f->data = d;
    s->font = f;
    return true;
}

void s3_font_close(S3 *s) {
    if (!s->font) return;
    for (int i = 0; i < CACHE_SIZE; i++) free(s->font->cache[i].bits);
    free(s->font->data);
    free(s->font);
    s->font = NULL;
}

static Glyph *glyph(S3Font *f, u32 cp, int size, bool half) {
    u32 h = (cp * 2654435761u) ^ (u32)size * 40503u ^ (half ? 0x5bd1e995u : 0);
    Glyph *g = &f->cache[h % CACHE_SIZE];
    if (g->used && g->cp == cp && g->size == size && g->half == half) return g;
    free(g->bits);
    memset(g, 0, sizeof *g);
    g->cp = cp; g->size = size; g->half = half; g->used = 1;
    int gi = stbtt_FindGlyphIndex(&f->info, (int)cp);
    if (!gi) return g;
    int asc, desc, gap;
    stbtt_GetFontVMetrics(&f->info, &asc, &desc, &gap);
    float scale = stbtt_ScaleForPixelHeight(&f->info, (float)size);
    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(&f->info, gi, scale, scale, &x0, &y0, &x1, &y1);
    int w = x1 - x0, hh = y1 - y0;
    if (w <= 0 || hh <= 0) return g;
    g->bits = s3_alloc((size_t)w * hh);
    stbtt_MakeGlyphBitmap(&f->info, g->bits, w, hh, w, scale, scale, gi);
    // Centre the glyph in its cell horizontally; place it on the font's baseline.
    int adv, lsb;
    stbtt_GetGlyphHMetrics(&f->info, gi, &adv, &lsb);
    int cell = half ? size / 2 : size;
    int advPx = (int)(adv * scale + 0.5f);
    g->ox = x0 + (cell - advPx) / 2;
    g->oy = (int)(asc * scale + 0.5f) + y0;
    g->w = w; g->h = hh;
    return g;
}

// GAIJI.DAT, read from the files of the System 3.0 games: u16 count, u16 cell size (16), 30 zero bytes, then per
// character a u16 JIS code and 16 rows of u16 pixels, leftmost pixel in the top bit. The codes are in JIS rows 0x76
// and 0x77, which Shift-JIS writes as EB 9F to EC 9E: the games' save dates, AM, PM and such.
typedef struct S3Gaiji { u8 lead, trail; u16 rows[16]; } S3Gaiji;

void s3_gaiji_load(S3 *s) {
    char *path = s3_find_file(s->gameDir, "GAIJI.DAT");
    size_t n;
    u8 *d = path ? s3_read_file(path, &n) : NULL;
    free(path);
    if (!d) return;
    int count = n >= 34 && s3_le16(d + 2) == 16 ? s3_le16(d) : 0;
    if (count > (int)((n - 34) / 34)) count = (int)((n - 34) / 34);
    s->gaiji = s3_alloc(sizeof(S3Gaiji) * (size_t)(count + 1));
    for (int i = 0; i < count; i++) {
        const u8 *e = d + 34 + (size_t)i * 34;
        int row = e[1], col = e[0];
        S3Gaiji *g = &s->gaiji[s->gaijiCount++];
        g->lead = (u8)((row + 1) / 2 + (row <= 0x5e ? 0x70 : 0xb0));
        g->trail = (u8)(row & 1 ? col + (col < 0x60 ? 0x1f : 0x20) : col + 0x7e);
        for (int y = 0; y < 16; y++) g->rows[y] = s3_le16(e + 2 + y * 2);
    }
    free(d);
}

int s3_gaiji_find(S3 *s, const u8 *sjis) {
    for (int i = 0; i < s->gaijiCount; i++) if (s->gaiji[i].lead == sjis[0] && s->gaiji[i].trail == sjis[1]) return i;
    return -1;
}

// A GAIJI.DAT character, scaled from its 16 by 16 cell to the font size.
static void draw_gaiji(S3 *s, const S3Gaiji *g, int x, int y, int size, int color) {
    u32 rgb = s3_rgb(s, color);
    for (int j = 0; j < size; j++)
        for (int i = 0; i < size; i++) {
            if (!(g->rows[j * 16 / size] & (0x8000 >> (i * 16 / size)))) continue;
            int dx = x + i, dy = y + j;
            if (dx < 0 || dy < 0 || dx >= s->dib.w || dy >= s->dib.h) continue;
            size_t di = (size_t)dy * s->dib.w + dx;
            if (s->dib.depth == 8) s->dib.p8[di] = (u8)color; else s->dib.p32[di] = rgb;
        }
    s3_mark_dirty(s);
}

int s3_draw_char(S3 *s, u32 cp, bool half, int x, int y, int size, int color) {
    int adv = half ? size / 2 : size;
    if (cp >= S3_GAIJI) {
        if (cp - S3_GAIJI < (u32)s->gaijiCount) draw_gaiji(s, &s->gaiji[cp - S3_GAIJI], x, y, size, color);
        return adv;
    }
    if (!s->font || cp == ' ' || cp == 0x3000) return adv;
    Glyph *g = glyph(s->font, cp, size, half);
    if (!g->bits) return adv;
    u32 rgb = s3_rgb(s, color);
    int W = s->dib.w, H = s->dib.h;
    for (int j = 0; j < g->h; j++) {
        int dy = y + g->oy + j;
        if (dy < 0 || dy >= H) continue;
        for (int i = 0; i < g->w; i++) {
            int dx = x + g->ox + i;
            if (dx < 0 || dx >= W) continue;
            int a = g->bits[j * g->w + i];
            size_t di = (size_t)dy * W + dx;
            if (s->dib.depth == 8) {
                if (a >= 128) s->dib.p8[di] = (u8)color;
            } else if (a) {
                u32 d = s->dib.p32[di];
                int r = (d >> 16) & 255, gg = (d >> 8) & 255, b = d & 255;
                int R = (rgb >> 16) & 255, G = (rgb >> 8) & 255, B = rgb & 255;
                r += (R - r) * a / 255; gg += (G - gg) * a / 255; b += (B - b) * a / 255;
                s->dib.p32[di] = (u32)r << 16 | (u32)gg << 8 | (u32)b;
            }
        }
    }
    s3_mark_dirty(s);
    return adv;
}

#include "anim.h"
#include "vnfont.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"

typedef struct Glyph {
    u32 cp;
    int size;
    int x0, y0, w, h, adv;
    float fadv;
    u8 *bm;
} Glyph;

struct Font {
    u8 *data;
    stbtt_fontinfo info;
    bool ok;
    Glyph *glyphs;
    int count, cap;
    int *hash;
    int hashSize;
    Font *fallback;
};

static Font *font_open(AN *a, const vnfont_match *m) {
    Font *f = an_alloc(sizeof *f);
    size_t size;
    f->data = an_read_file(m->path, &size);
    int off = f->data ? stbtt_GetFontOffsetForIndex(f->data, m->face) : -1;
    f->ok = off >= 0 && stbtt_InitFont(&f->info, f->data, off);
    if (!f->ok) an_log(a, "can't read font %s", m->path);
    return f;
}

bool an_text_init(AN *a) {
    vnfont_match m, jp;
    bool hasJp = vnfont_resolve("MS Gothic", 0, 0, &jp);
    if (!vnfont_resolve("Arial", 0, 0, &m) && !hasJp) {
        a->font = an_alloc(sizeof(Font));
        an_log(a, "no font for the message window");
        return false;
    }
    if (!vnfont_resolve("Arial", 0, 0, &m)) m = jp;
    a->font = font_open(a, &m);
    if (hasJp && strcmp(jp.path, m.path)) a->font->fallback = font_open(a, &jp);
    return a->font->ok;
}

static void font_free(Font *f) {
    if (!f) return;
    for (int i = 0; i < f->count; i++) free(f->glyphs[i].bm);
    free(f->glyphs);
    free(f->hash);
    free(f->data);
    font_free(f->fallback);
    free(f);
}

void an_text_free(AN *a) {
    font_free(a->font);
    a->font = NULL;
}

static u32 gkey(u32 cp, int size) { return cp * 31u + (u32)size * 2654435761u; }

static void rehash(Font *f) {
    free(f->hash);
    f->hashSize = 1024;
    while (f->hashSize < f->count * 2) f->hashSize *= 2;
    f->hash = an_alloc(sizeof(int) * (size_t)f->hashSize);
    for (int i = 0; i < f->hashSize; i++) f->hash[i] = -1;
    for (int i = 0; i < f->count; i++) {
        u32 h = gkey(f->glyphs[i].cp, f->glyphs[i].size) & (u32)(f->hashSize - 1);
        while (f->hash[h] >= 0) h = (h + 1) & (u32)(f->hashSize - 1);
        f->hash[h] = i;
    }
}

static Glyph *glyph(Font *f, u32 cp, int size) {
    if (f->fallback && f->fallback->ok && cp > 0x7f && f->ok && !stbtt_FindGlyphIndex(&f->info, (int)cp) &&
        stbtt_FindGlyphIndex(&f->fallback->info, (int)cp))
        return glyph(f->fallback, cp, size);
    if (f->hashSize) {
        u32 h = gkey(cp, size) & (u32)(f->hashSize - 1);
        while (f->hash[h] >= 0) {
            Glyph *g = &f->glyphs[f->hash[h]];
            if (g->cp == cp && g->size == size) return g;
            h = (h + 1) & (u32)(f->hashSize - 1);
        }
    }
    if (f->count == f->cap) {
        f->cap = f->cap ? f->cap * 2 : 512;
        f->glyphs = an_realloc(f->glyphs, sizeof(Glyph) * (size_t)f->cap);
    }
    Glyph *g = &f->glyphs[f->count++];
    memset(g, 0, sizeof *g);
    g->cp = cp;
    g->size = size;
    if (f->ok) {
        float sc = stbtt_ScaleForMappingEmToPixels(&f->info, (float)size);
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&f->info, (int)cp, &adv, &lsb);
        g->fadv = adv * sc;
        g->adv = (int)ceilf(g->fadv);
        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&f->info, (int)cp, sc, sc, &x0, &y0, &x1, &y1);
        g->x0 = x0;
        g->y0 = y0;
        g->w = x1 - x0;
        g->h = y1 - y0;
        if (g->w > 0 && g->h > 0) {
            g->bm = an_alloc((size_t)g->w * (size_t)g->h);
            stbtt_MakeCodepointBitmap(&f->info, g->bm, g->w, g->h, g->w, sc, sc, (int)cp);
        }
    } else g->fadv = (float)(g->adv = size / 2);
    if (f->count * 2 > f->hashSize) rehash(f);
    else {
        u32 h = gkey(cp, size) & (u32)(f->hashSize - 1);
        while (f->hash[h] >= 0) h = (h + 1) & (u32)(f->hashSize - 1);
        f->hash[h] = f->count - 1;
    }
    return g;
}

static void put_glyph(u32 *dst, int dw, int dh, const Glyph *g, int x, int y, u32 color) {
    u32 ca = color >> 24;
    for (int j = 0; j < g->h; j++) {
        int py = y + j;
        if (py < 0 || py >= dh) continue;
        for (int i = 0; i < g->w; i++) {
            int px = x + i;
            if (px < 0 || px >= dw) continue;
            u32 al = g->bm[(size_t)j * (size_t)g->w + (size_t)i] * ca / 255;
            if (!al) continue;
            u32 *d = &dst[(size_t)py * (size_t)dw + (size_t)px];
            u32 ia = 255 - al;
            u32 rb = ((color & 0xff00ffu) * al + (*d & 0xff00ffu) * ia) >> 8 & 0xff00ffu;
            u32 gg = ((color & 0x00ff00u) * al + (*d & 0x00ff00u) * ia) >> 8 & 0x00ff00u;
            *d = rb | gg | 0xff000000u;
        }
    }
}

static int word_width(Font *f, const char *s, int size) {
    int w = 0;
    while (*s && *s != ' ' && !(s[0] == '@' && s[1] == 'n') && *s != '\n') {
        u32 cp;
        s += an_utf8_decode(s, &cp);
        w += glyph(f, cp, size)->adv;
    }
    return w;
}

int an_text_width(AN *a, const char *s, int size) {
    int w = 0;
    while (*s) {
        u32 cp;
        s += an_utf8_decode(s, &cp);
        w += glyph(a->font, cp, size)->adv;
    }
    return w;
}

int an_text_draw(AN *a, int x, int baseline, const char *s, int size, u32 color, u32 shadow, int maxWidth, int lineHeight) {
    Font *f = a->font;
    if (!f) return 0;
    int base = baseline;
    int cx = x, lines = 1;
    float fx = (float)x;
    while (*s) {
        if ((s[0] == '@' && s[1] == 'n') || s[0] == '\n') {
            s += s[0] == '@' ? 2 : 1;
            cx = x;
            fx = (float)x;
            base += lineHeight;
            lines++;
            continue;
        }
        if (*s == ' ' && maxWidth > 0 && cx > x) {
            Glyph *sp = glyph(f, ' ', size);
            if (cx + sp->adv + word_width(f, s + 1, size) > x + maxWidth) {
                s++;
                cx = x;
                fx = (float)x;
                base += lineHeight;
                lines++;
                continue;
            }
        }
        u32 cp;
        s += an_utf8_decode(s, &cp);
        Glyph *g = glyph(f, cp, size);
        if (g->bm) {
            int gx = cx + g->x0, gy = base + g->y0;
            if (shadow >> 24) {
                put_glyph(a->fb, a->W, a->H, g, gx + 1, gy + 1, shadow);
                put_glyph(a->fb, a->W, a->H, g, gx + 2, gy + 2, shadow);
            }
            put_glyph(a->fb, a->W, a->H, g, gx, gy, color);
        }
        if (a->fractionalText) {
            fx += g->fadv;
            cx = (int)lroundf(fx);
        } else cx += g->adv;
    }
    return lines;
}

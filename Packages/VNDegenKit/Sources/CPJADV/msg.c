#include "pj.h"
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
    bool ok, embolden;
    Glyph *glyphs;
    int count, cap;
    int *hash;
    int hashSize;
} font;

static void font_reset(void) {
    for (int i = 0; i < font.count; i++) free(font.glyphs[i].bm);
    free(font.glyphs);
    free(font.hash);
    free(font.data);
    memset(&font, 0, sizeof font);
}

void pj_font_init(PJ *p) {
    font_reset();
    static const char *const en[] = {"Arial", "Meiryo UI", "MS Gothic", NULL};
    static const char *const ja[] = {"Meiryo UI", "Meiryo", "MS UI Gothic", "MS Gothic", NULL};
    static const char *const cn[] = {"SimSun", "NSimSun", "Meiryo UI", "MS Gothic", NULL};
    const char *const *names = p->lang == VNPJADV_LANG_CHINESE ? cn : p->lang == VNPJADV_LANG_JAPANESE ? ja : en;
    vnfont_match m;
    bool found = false;
    for (int i = 0; names[i] && !found; i++) found = vnfont_resolve(names[i], 1, 0, &m) != 0;
    if (!found) {
        pj_log(p, "no font for the message window");
        return;
    }
    size_t size;
    font.data = pj_read_file(m.path, &size);
    int off = font.data ? stbtt_GetFontOffsetForIndex(font.data, m.face) : -1;
    font.ok = off >= 0 && stbtt_InitFont(&font.info, font.data, off);
    font.embolden = m.embolden != 0;
    if (!font.ok) pj_log(p, "can't read font %s", m.path);
    else pj_log(p, "message font %s", m.path);
}

static u32 gkey(u32 cp, int size) { return cp * 31u + (u32)size * 7919u; }

static void rehash(void) {
    free(font.hash);
    font.hashSize = 1024;
    while (font.hashSize < font.count * 2) font.hashSize *= 2;
    font.hash = pj_alloc(sizeof(int) * (size_t)font.hashSize);
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
        font.glyphs = pj_realloc(font.glyphs, sizeof(Glyph) * (size_t)font.cap);
    }
    Glyph *g = &font.glyphs[font.count++];
    memset(g, 0, sizeof *g);
    g->cp = cp;
    g->size = size;
    if (font.ok) {
        float sc = stbtt_ScaleForPixelHeight(&font.info, (float)size * 1.15f);
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
            g->bm = pj_alloc((size_t)g->w * (size_t)g->h);
            stbtt_MakeCodepointBitmap(&font.info, g->bm, g->w, g->h, g->w, sc, sc, (int)cp);
        }
    } else
        g->adv = cp < 0x2e80 ? size / 2 : size;
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
    float sc = stbtt_ScaleForPixelHeight(&font.info, (float)size * 1.15f);
    return (int)(a * sc + 0.5f);
}

int pj_char_width(u32 cp, int size) { return glyph(cp, size)->adv + (font.embolden ? 1 : 0); }

int pj_text_width(const char *s, int n, int size) {
    int w = 0;
    for (int i = 0; *s && (n < 0 || i < n); i++) {
        u32 cp;
        s += pj_utf8_decode(s, &cp);
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

static u32 rgb_to_px(u32 rgb) { return 0xff000000u | (rgb & 0xff) << 16 | (rgb & 0xff00) | (rgb >> 16 & 0xff); }

int pj_text_draw(u32 *dst, int dw, int dh, const char *s, int n, int x, int y, int size, u32 color, u32 edge) {
    int base = y + ascent(size);
    int cx = x;
    u32 fg = rgb_to_px(color), ed = rgb_to_px(edge);
    int e = size >= 30 ? 2 : 1;
    for (int i = 0; *s && (n < 0 || i < n); i++) {
        u32 cp;
        s += pj_utf8_decode(s, &cp);
        Glyph *g = glyph(cp, size);
        if (g->bm) {
            int gx = cx + g->x0, gy = base + g->y0;
            if (edge != 0xffffffffu)
                for (int oy = -e; oy <= e; oy++)
                    for (int ox = -e; ox <= e; ox++)
                        if (ox || oy) put_glyph(dst, dw, dh, g, gx + ox, gy + oy, ed);
            if (edge != 0xffffffffu && font.embolden)
                for (int oy = -e; oy <= e; oy++)
                    if (oy) put_glyph(dst, dw, dh, g, gx + e + 1, gy + oy, ed);
            put_glyph(dst, dw, dh, g, gx, gy, fg);
            if (font.embolden) put_glyph(dst, dw, dh, g, gx + 1, gy, fg);
        }
        cx += g->adv + (font.embolden ? 1 : 0);
    }
    return cx - x;
}

void pj_msg_init(PJ *p) {
    p->win[0] = (MsgWin){91, 793, 418, 38, 38, 38, 38, 0xffffff, 0x8050a0, 0, 0};
    p->win[1] = (MsgWin){91, 848, 1755, 189, 39, 39, 50, 0xffffff, 0x8050a0, 0, 0};
}

static int wrap(const char *s, int size, int width, const char **lines, int *counts, int max) {
    int n = 0;
    const char *line = s;
    int chars = 0, w = 0;
    const char *lastSpace = NULL;
    int lastSpaceChars = 0;
    for (const char *q = s; *q;) {
        u32 cp;
        int k = pj_utf8_decode(q, &cp);
        int cw = pj_char_width(cp, size);
        if (cp == '\n' || (w + cw > width && chars > 0)) {
            if (n == max) return n;
            if (cp != '\n' && cp != ' ' && lastSpace && lastSpace > line) {
                lines[n] = line;
                counts[n++] = lastSpaceChars;
                line = lastSpace + 1;
                q = line;
            } else {
                lines[n] = line;
                counts[n++] = chars;
                line = cp == '\n' || cp == ' ' ? q + k : q;
                q = line;
            }
            chars = 0;
            w = 0;
            lastSpace = NULL;
            continue;
        }
        if (cp == ' ') {
            lastSpace = q;
            lastSpaceChars = chars;
        }
        chars++;
        w += cw;
        q += k;
    }
    if (chars && n < max) {
        lines[n] = line;
        counts[n++] = chars;
    }
    return n;
}

void pj_msg_tick(PJ *p, int ms) {
    if (!p->msgText || p->msgShownChars >= p->msgTotalChars) return;
    p->msgT += ms;
    int per = 20;
    int want = p->msgT / per;
    if (want > p->msgTotalChars) want = p->msgTotalChars;
    if (want != p->msgShownChars) {
        p->msgShownChars = want;
        p->dirty = true;
    }
}

void pj_msg_draw(PJ *p, u32 *dst) {
    if (!p->msgShown || !p->msgText) return;
    if (p->msgFrameFile && p->msgFrameLoaded != p->msgFrameFile) {
        pj_sprite_load(p, &p->msgFrame, pj_script_name(&p->sc, p->msgFrameFile) ? pj_script_name(&p->sc, p->msgFrameFile) : "");
        p->msgFrameLoaded = p->msgFrameFile;
    }
    Layer *f = &p->msgFrame;
    if (f->used && f->img) pj_blit(dst, PJ_W, PJ_H, f->img, f->sx, f->sy, f->sw, f->sh, f->x, f->y, f->alpha, 100, 0, 0);
    MsgWin *nw = &p->win[0], *tw = &p->win[1];
    if (p->msgName && *p->msgName) pj_text_draw(dst, PJ_W, PJ_H, p->msgName, -1, nw->x + nw->indent, nw->y, nw->ch, nw->color, nw->edge);
    const char *lines[8];
    int counts[8];
    int n = wrap(p->msgText, tw->ch, tw->w - tw->indent, lines, counts, 8);
    int left = p->msgShownChars;
    for (int i = 0; i < n && left > 0; i++) {
        int c = counts[i] < left ? counts[i] : left;
        pj_text_draw(dst, PJ_W, PJ_H, lines[i], c, tw->x + (i ? tw->indent : 0), tw->y + i * tw->pitch, tw->ch, tw->color, tw->edge);
        left -= counts[i];
    }
}

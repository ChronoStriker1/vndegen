// Fonts and text drawing. Fonts come from the shared Windows font pack (vnfont), which the host initializes.
#include "text.h"
#include "vnfont.h"
#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"

typedef struct YFont {
    char path[vnfont_path_max];
    int face;
    u8 *data;
    stbtt_fontinfo info;
    bool ok;
} YFont;

enum { FONT_CACHE = 8 };
static YFont fonts[FONT_CACHE];
static int fontCount;

static YFont *font_for(const char *path, int face) {
    for (int i = 0; i < fontCount; i++) if (fonts[i].face == face && !strcmp(fonts[i].path, path)) return &fonts[i];
    if (fontCount == FONT_CACHE) return fonts[0].ok ? &fonts[0] : NULL;
    YFont *f = &fonts[fontCount++];
    snprintf(f->path, sizeof f->path, "%s", path);
    f->face = face;
    size_t size;
    f->data = yr_read_file(path, &size);
    int off = f->data ? stbtt_GetFontOffsetForIndex(f->data, face) : -1;
    f->ok = off >= 0 && stbtt_InitFont(&f->info, f->data, off);
    return f;
}

static YFont *slot_font(YR *y, const YFontSlot *s, bool *embolden) {
    vnfont_match m;
    char *name = s->name ? yr_to_utf8(y->enc, s->name) : NULL;
    bool found = name && vnfont_resolve(name, s->bold, 0, &m);
    free(name);
    if (!found) found = vnfont_resolve("MS Gothic", s->bold, 0, &m);
    if (!found) return NULL;
    *embolden = s->bold && m.embolden;
    YFont *f = font_for(m.path, m.face);
    return f && f->ok ? f : NULL;
}

void yr_text_open(YR *y) {
    YText *t = y->txt = yr_alloc(sizeof(YText));
    for (int i = 0; i < YR_FONT_SLOTS; i++) {
        YFontSlot *s = &t->slots[i];
        s->sx = s->sy = 24;
        s->color = s->color2 = 0xffffff;
        s->aa = true;
    }
}

void yr_text_close(YR *y) {
    if (!y->txt) return;
    for (int i = 0; i < YR_FONT_SLOTS; i++) free(y->txt->slots[i].name);
    free(y->txt);
    y->txt = NULL;
}

YFontSlot *yr_font_slot(YR *y, int bno) {
    if (bno < 0 || bno >= YR_FONT_SLOTS) bno = 0;
    return &y->txt->slots[bno];
}

static float em_scale(YFont *f, int size) { return stbtt_ScaleForMappingEmToPixels(&f->info, (float)size); }

int yr_text_width(YR *y, int bno, const char *s) {
    YFontSlot *slot = yr_font_slot(y, bno);
    bool bold;
    YFont *f = slot_font(y, slot, &bold);
    if (!f) return 0;
    float sc = em_scale(f, slot->sy), sx = slot->sy ? sc * (float)slot->sx / (float)slot->sy : sc;
    float w = 0;
    for (const u8 *p = (const u8 *)s; *p;) {
        u32 cp;
        p += yr_char_decode(y->enc, p, &cp);
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&f->info, (int)cp, &adv, &lsb);
        w += adv * sx;
    }
    return (int)ceilf(w);
}

static void put(YLayer *l, int x, int y, u32 rgb, u32 a, int drawmode) {
    if (x < 0 || y < 0 || x >= l->w || y >= l->h || !a) return;
    u32 *d = &l->px[(size_t)y * l->w + x];
    u32 da = *d >> 24;
    if (drawmode == 0 || !da) { *d = rgb | a << 24; return; }
    // Straight-alpha "over".
    u32 oa = a + yr_mul8(da, 255 - a);
    u32 out = 0;
    for (int ch = 0; ch < 3; ch++) {
        u32 sc = (rgb >> (8 * ch)) & 0xff, dc = (*d >> (8 * ch)) & 0xff;
        u32 v = (sc * a + dc * yr_mul8(da, 255 - a)) / (oa ? oa : 1);
        out |= (v > 255 ? 255 : v) << (8 * ch);
    }
    *d = out | oa << 24;
}

static u32 rgb_of(i64 c) { return (u32)((c >> 16) & 0xff) | (u32)(c & 0xff00) | (u32)((c & 0xff) << 16); }

void yr_text_draw(YR *y, YLayer *l, int bno, const char *s, int x, int y0) {
    YFontSlot *slot = yr_font_slot(y, bno);
    if (!l->px || l->w <= 0) yr_layer_pixels(l, y->width, y->height);
    bool embolden = false;
    YFont *f = slot_font(y, slot, &embolden);
    if (!f) { yr_log(y, "text: no font\n"); return; }
    float sc = em_scale(f, slot->sy), sx = slot->sy ? sc * (float)slot->sx / (float)slot->sy : sc;
    int asc, desc, gap;
    stbtt_GetFontVMetrics(&f->info, &asc, &desc, &gap);
    float baseline = y0 + asc * sc;
    float pen = (float)x;
    u32 top = rgb_of(slot->color), bottom = rgb_of(slot->color2);
    int cell = slot->sy > 0 ? slot->sy : 1;
    bool bold = slot->bold;
    for (const u8 *p = (const u8 *)s; *p;) {
        u32 cp;
        p += yr_char_decode(y->enc, p, &cp);
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&f->info, (int)cp, &adv, &lsb);
        int bx0, by0, bx1, by1;
        float shift = pen - floorf(pen);
        stbtt_GetCodepointBitmapBoxSubpixel(&f->info, (int)cp, sx, sc, shift, 0, &bx0, &by0, &bx1, &by1);
        int gw = bx1 - bx0, gh = by1 - by0;
        if (gw > 0 && gh > 0) {
            u8 *bm = yr_alloc((size_t)gw * (size_t)gh);
            stbtt_MakeCodepointBitmapSubpixel(&f->info, bm, gw, gh, gw, sx, sc, shift, 0, (int)cp);
            int ox = (int)floorf(pen) + bx0, oy = (int)floorf(baseline) + by0;
            for (int j = 0; j < gh; j++) {
                int row = oy + j - y0;
                // Gradation from COLOR at the top of the cell to COLOR2 at its bottom.
                u32 rgb = top;
                if (top != bottom) {
                    int k = row <= 0 ? 0 : row >= cell ? 256 : row * 256 / cell, out = 0;
                    for (int ch = 0; ch < 3; ch++) {
                        int a = (top >> (8 * ch)) & 0xff, b = (bottom >> (8 * ch)) & 0xff;
                        out |= (a + (b - a) * k / 256) << (8 * ch);
                    }
                    rgb = (u32)out;
                }
                for (int i = 0; i < gw; i++) {
                    u32 a = bm[(size_t)j * gw + i];
                    if (!slot->aa) a = a >= 128 ? 255 : 0;
                    if (!a) continue;
                    put(l, ox + i, oy + j, rgb, a, 1);
                    if (bold || embolden) put(l, ox + i + 1, oy + j, rgb, a, 1);
                }
            }
            free(bm);
        }
        pen += adv * sx + (bold ? 1 : 0);
    }
}

// ---- FONT and FONTINFO ----

static const char *const fontList[] = { "MS Gothic", "MS PGothic", "MS Mincho", "MS PMincho", "Meiryo" };

int yr_cmd_font(YCall *c, int id) {
    YR *y = c->y;
    int bno = (int)yc_int_or(c, "BNO", 0);
    YFontSlot *s = yr_font_slot(y, bno);
    if (id == C_FONT) {
        if (yc_has(c, "NAME")) { free(s->name); s->name = yc_str(c, "NAME"); }
        i64 v;
        if (yc_int(c, "SX", &v)) s->sx = (int)v;
        if (yc_int(c, "SY", &v)) s->sy = (int)v;
        if (yc_int(c, "COLOR", &v)) { s->color = v; if (!yc_has(c, "COLOR2")) s->color2 = v; }
        if (yc_int(c, "COLOR2", &v)) s->color2 = v;
        if (yc_int(c, "BOLD", &v)) s->bold = v != 0;
        if (yc_int(c, "ANTIALIASING", &v)) s->aa = v != 0;
        if (yc_int(c, "DRAWMODE", &v)) s->drawmode = (int)v;
        return YR_NEXT;
    }
    YRef out;
    if (!yc_ref(c, "LET", &out)) return YR_NEXT;
    YVal r = yv_int(0);
    int count = (int)(sizeof fontList / sizeof fontList[0]);
    if (yc_int_or(c, "NUM", 0)) r = yv_int(count);
    else if (yc_int_or(c, "NAME", 0)) {
        i64 no;
        if (yc_int(c, "NO", &no)) {
            const char *n = no >= 1 && no <= count ? fontList[no - 1] : "";
            r = yv_str(n);
        } else r = yv_str(s->name ? s->name : "");
    } else if (yc_int_or(c, "LANG", 0)) r = yv_int(128);
    else if (yc_int_or(c, "SX", 0)) r = yv_int(s->sx);
    else if (yc_int_or(c, "SY", 0)) r = yv_int(s->sy);
    else if (yc_int_or(c, "COLOR", 0)) r = yv_int(s->color);
    else if (yc_int_or(c, "COLOR2", 0)) r = yv_int(s->color2);
    else if (yc_int_or(c, "BOLD", 0)) r = yv_int(s->bold);
    else if (yc_int_or(c, "ANTIALIASING", 0)) r = yv_int(s->aa);
    else if (yc_int_or(c, "DRAWMODE", 0)) r = yv_int(s->drawmode);
    else if (yc_int_or(c, "X", 0)) {
        char *str = yc_str(c, "SETSTR");
        r = yv_int(yr_text_width(y, bno, str ? str : ""));
        free(str);
    } else if (yc_int_or(c, "Y", 0)) r = yv_int(s->sy);
    else yc_unknown(c, "FONTINFO form");
    yr_ref_set(y, c->t, &out, &r);
    yv_free(&r);
    return YR_NEXT;
}

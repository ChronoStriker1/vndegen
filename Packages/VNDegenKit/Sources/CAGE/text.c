#include "age.h"
#include "vnfont.h"

#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"

typedef struct Font {
    char path[1024];
    int face;
    u8 *data;
    stbtt_fontinfo info;
    bool ok;
} Font;

static Font fonts[8];
static int nfonts;

static Font *font_load(const char *name, int bold) {
    vnfont_match m;
    if (!vnfont_resolve(name, bold, 0, &m) && !vnfont_resolve("MS Gothic", bold, 0, &m)) return NULL;
    for (int i = 0; i < nfonts; i++)
        if (!strcmp(fonts[i].path, m.path) && fonts[i].face == m.face) return fonts[i].ok ? &fonts[i] : NULL;
    if (nfonts >= 8) return fonts[0].ok ? &fonts[0] : NULL;
    Font *f = &fonts[nfonts++];
    snprintf(f->path, sizeof f->path, "%s", m.path);
    f->face = m.face;
    size_t n = 0;
    FILE *fp = fopen(m.path, "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        n = (size_t)ftell(fp);
        fseek(fp, 0, SEEK_SET);
        f->data = ag_alloc(n);
        if (fread(f->data, 1, n, fp) != n) { free(f->data); f->data = NULL; }
        fclose(fp);
    }
    int off = f->data ? stbtt_GetFontOffsetForIndex(f->data, m.face) : -1;
    f->ok = off >= 0 && stbtt_InitFont(&f->info, f->data, off);
    return f->ok ? f : NULL;
}

typedef struct Mask {
    int w, h, x0, y0;
    int adv;
    u8 *c;
} Mask;

static bool glyph_mask(Font *f, u32 cp, int size, bool bold, Mask *out) {
    memset(out, 0, sizeof *out);
    float sc = stbtt_ScaleForMappingEmToPixels(&f->info, (float)size);
    int aw, lsb;
    stbtt_GetCodepointHMetrics(&f->info, (int)cp, &aw, &lsb);
    out->adv = (int)floorf((float)aw * sc + 0.5f) + (bold ? 1 : 0);
    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(&f->info, (int)cp, sc, sc, &x0, &y0, &x1, &y1);
    int w = x1 - x0, h = y1 - y0;
    if (w <= 0 || h <= 0) return true;
    u8 *b = ag_alloc((size_t)w * (size_t)h);
    stbtt_MakeCodepointBitmap(&f->info, b, w, h, w, sc, sc, (int)cp);
    if (bold) {
        u8 *e = ag_alloc((size_t)(w + 1) * (size_t)h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x <= w; x++) {
                int a = x < w ? b[y * w + x] : 0, p = x > 0 ? b[y * w + x - 1] : 0;
                int v = a + p;
                e[y * (w + 1) + x] = (u8)(v > 255 ? 255 : v);
            }
        free(b);
        b = e;
        w++;
    }
    out->w = w;
    out->h = h;
    out->x0 = x0;
    out->y0 = y0;
    out->c = b;
    return true;
}

static int font_ascent(Font *f, int size) {
    float sc = stbtt_ScaleForMappingEmToPixels(&f->info, (float)size);
    int a, d, gap;
    stbtt_GetFontVMetrics(&f->info, &a, &d, &gap);
    int t = 0, os2 = (int)stbtt__find_table(f->info.data, (u32)f->info.fontstart, "OS/2");
    if (os2) {
        t = ttSHORT(f->info.data + os2 + 74);
        if (t > 0) a = t;
    }
    return (int)floorf((float)a * sc + 0.5f);
}

static void blit_mask(Image *im, const Mask *m, int x, int y, u32 rgb) {
    for (int yy = 0; yy < m->h; yy++) {
        int ty = y + yy;
        if (ty < 0 || ty >= im->h) continue;
        for (int xx = 0; xx < m->w; xx++) {
            int tx = x + xx;
            if (tx < 0 || tx >= im->w) continue;
            int a = m->c[yy * m->w + xx];
            a = (a * 16 / 255) * 255 / 16;
            if (!a) continue;
            u32 *p = &im->px[(size_t)ty * (size_t)im->w + (size_t)tx];
            u32 d = *p;
            int da = (int)(d >> 24);
            if (a == 255 || da == 0) {
                *p = (rgb & 0xffffff) | (u32)(da > a ? da : a) << 24;
                if (da == 0) *p = (rgb & 0xffffff) | (u32)a << 24;
                continue;
            }
            int na = da > a ? da : a;
            u32 o = 0;
            for (int k = 0; k < 24; k += 8) {
                int dc = (int)(d >> k & 255), sc = (int)(rgb >> k & 255);
                o |= (u32)((dc * (255 - a) + sc * a) / 255) << k;
            }
            *p = o | (u32)na << 24;
        }
    }
}

static u32 rgb_of(i32 c) { u32 v = (u32)c; return (v >> 16 & 255) | (v & 0xff00) | (v & 255) << 16; }

static void draw_mask(Text *t, Image *im, const Mask *m, int x, int y) {
    if (!m->c) return;
    u32 col = rgb_of(t->color), ef = rgb_of(t->effColor);
    if (t->effMode == 1) blit_mask(im, m, x + t->effX, y + t->effY, ef);
    else if (t->effMode == 3 && (t->effX || t->effY)) {
        double step = 360.0 / (sqrt((double)(t->effX + t->effY)) * 8.0);
        for (double ang = 0; ang < 360.0; ang += step) {
            double a = ang * 3.14159265358979 / 180.0;
            int ox = (int)lround(cos(a) * t->effX), oy = (int)lround(sin(a) * t->effY);
            blit_mask(im, m, x + ox, y + oy, ef);
        }
    }
    blit_mask(im, m, x, y, col);
    if (t->effMode == 2) {
        Mask q = *m;
        q.c = ag_alloc((size_t)m->w * (size_t)m->h);
        for (int i = 0; i < m->w * m->h; i++) q.c[i] = (u8)(m->c[i] >> 2);
        blit_mask(im, &q, x, y, col);
        free(q.c);
    }
}

static bool closing_class(u32 cp) {
    return cp == 0x3001 || cp == 0x3002 || cp == 0xff0c || cp == 0xff0e || cp == 0x300d || cp == 0x300f || cp == 0xff09 ||
           cp == 0x3011 || cp == 0xff3d || cp == 0xff5d || cp == 0x3009 || cp == 0x300b;
}

static bool opening_class(u32 cp) {
    return cp == 0x300c || cp == 0x300e || cp == 0xff08 || cp == 0x3010 || cp == 0xff3b || cp == 0xff5b || cp == 0x3008 ||
           cp == 0x300a;
}

typedef struct Pen {
    Text *t;
    Font *f;
    int ascent;
    int full;
} Pen;

static bool pen_init(AG *g, Pen *p) {
    Text *t = &g->text;
    p->t = t;
    p->f = font_load(t->face[0] ? t->face : "MS Gothic", t->bold);
    if (!p->f) return false;
    p->ascent = font_ascent(p->f, t->size);
    p->full = t->size + (t->bold ? 1 : 0);
    return true;
}

static int glyph(AG *g, Pen *p, Image *im, const u8 *s, int x, int y, bool draw, int *adv) {
    u32 cp;
    int len = ag_sjis_decode(s, &cp);
    Text *t = p->t;
    Mask m;
    glyph_mask(p->f, cp, t->size, t->bold != 0, &m);
    int a;
    int off = 0;
    if (t->aspect) {
        a = len == 1 ? p->full / 2 : p->full;
        if (m.c) {
            int bbw = m.w;
            if (len == 1) off = (int)(t->size / 4.0 + 0.5) - (int)(bbw * 0.5 + 0.5) - m.x0;
            else if (closing_class(cp)) off = 0;
            else if (opening_class(cp)) off = (p->full - 2) - m.x0 - bbw;
            else off = (int)(t->size / 2.0 + 0.5) - (int)(bbw * 0.5 + 0.5) - m.x0;
        }
    } else a = m.adv;
    if (draw && im && m.c) draw_mask(t, im, &m, x + m.x0 + off, y + p->ascent + m.y0);
    free(m.c);
    *adv = a;
    (void)g;
    return len;
}

void text_draw_string(AG *g, Image *im, int x, int y, const char *s) {
    Pen p;
    if (!pen_init(g, &p)) return;
    const u8 *q = (const u8 *)s;
    while (*q) {
        int a;
        q += glyph(g, &p, im, q, x, y, true, &a);
        x += a;
    }
}

static int string_width(AG *g, const char *s) {
    Pen p;
    if (!pen_init(g, &p)) return 0;
    int w = 0;
    for (const u8 *q = (const u8 *)s; *q;) {
        int a;
        q += glyph(g, &p, NULL, q, 0, 0, false, &a);
        w += a;
    }
    return w;
}

static Layout *layout(AG *g, i32 slot) {
    if (slot == 0) slot = g->text.cur;
    if (slot < 0 || slot > 9) return NULL;
    return &g->text.lay[slot];
}

static i32 layout_index(AG *g, i32 slot) { return slot == 0 ? g->text.cur : slot; }

void text_init(AG *g) {
    Text *t = &g->text;
    snprintf(t->face, sizeof t->face, "%s", store_setting(&g->st, "FONT") ? store_setting(&g->st, "FONT") : "MS Gothic");
    t->size = 24;
    t->color = 0xffffff;
    t->effColor = 0x606060;
    t->leading = 8;
    t->rubySize = 12;
    t->delay = 5;
    t->cur = 1;
    t->recording = true;
    t->winAlpha = 8;
    t->autoTime[0] = 1000;
    t->autoTime[1] = 2000;
    for (int i = 0; i < 10; i++) { t->lay[i].indHandle = -1; t->lay[i].boundR = 0x7fffffff; t->lay[i].boundB = 0x7fffffff; }
}

void text_close(AG *g) {
    Text *t = &g->text;
    for (int i = 0; i < 10; i++) free(t->lay[i].rec);
    hist_clear(g);
    free(t->rec);
    free(t->ent);
}

static void rec_push(Layout *L, GlyphRec r) {
    if (L->nrec >= L->cap) {
        L->cap = L->cap ? L->cap * 2 : 64;
        L->rec = ag_realloc(L->rec, sizeof(GlyphRec) * (size_t)L->cap);
    }
    L->rec[L->nrec++] = r;
}

static void publish(AG *g, int li, int idx) {
    Layout *L = &g->text.lay[li];
    if (idx >= L->capacity) return;
    GlyphRec *r = &L->rec[idx];
    Obj *o = gfx_obj(g, L->first + idx, true);
    o->slot = 0x14 + li;
    o->sl = r->l;
    o->st = r->t;
    o->sr = r->r;
    o->sb = r->b;
    o->pos.x = (float)(L->ox + r->l);
    o->pos.y = (float)(L->oy + r->t);
    o->visible = true;
    o->live = false;
}

bool text_revealing(AG *g) {
    for (int i = 1; i < 10; i++) {
        Layout *L = &g->text.lay[i];
        if (L->shown < L->nrec && L->shown < L->capacity) return true;
    }
    return false;
}

void text_finish(AG *g) {
    for (int i = 1; i < 10; i++) {
        Layout *L = &g->text.lay[i];
        while (L->shown < L->nrec && L->shown < L->capacity) publish(g, i, L->shown++);
    }
}

static void indicator_place(AG *g) {
    Text *t = &g->text;
    if (!t->indActive) return;
    Layout *L = &t->lay[t->indLayout];
    if (!L->indConfigured || L->indHandle < 0) return;
    Obj *o = gfx_obj(g, L->indHandle, true);
    int frames = L->indFrames > 0 ? L->indFrames : 1;
    int f = L->indPeriod > 0 ? (int)((g->now - t->indStart) / (u32)L->indPeriod) % frames : 0;
    int cols = L->indCols > 0 ? L->indCols : 1;
    o->slot = L->indSurf;
    o->sl = L->indSX + (f % cols) * L->indW;
    o->st = L->indSY + (f / cols) * L->indH;
    o->sr = o->sl + L->indW;
    o->sb = o->st + L->indH;
    int x = L->ox + L->indX, y = L->oy + L->indY;
    if (t->followLast) { x += L->cx; y += L->cy; }
    o->pos.x = (float)x;
    o->pos.y = (float)y;
    o->visible = true;
}

void text_tick(AG *g) {
    Text *t = &g->text;
    for (int i = 1; i < 10; i++) {
        Layout *L = &t->lay[i];
        while (L->shown < L->nrec && L->shown < L->capacity && (i32)(g->now - L->nextAt) >= 0) {
            bool chain = L->rec[L->shown].chain == 1;
            publish(g, i, L->shown++);
            if (!chain) L->nextAt += (u32)(t->delay > 0 ? t->delay : 0);
            if (t->delay <= 0) continue;
        }
    }
    indicator_place(g);
}

static void indicator_stop(AG *g) {
    Text *t = &g->text;
    if (!t->indActive) return;
    Layout *L = &t->lay[t->indLayout];
    if (L->indHandle >= 0) {
        Obj *o = gfx_obj(g, L->indHandle, false);
        if (o) o->visible = false;
    }
    t->indActive = false;
}

static int op_define(AG *g) {
    i32 li = layout_index(g, arg_i(g, 1));
    Layout *L = layout(g, arg_i(g, 1));
    if (!L) return OP_NEXT;
    L->defined = true;
    L->w = arg_i(g, 2);
    L->h = arg_i(g, 3);
    L->ox = arg_i(g, 4);
    L->oy = arg_i(g, 5);
    if (L->w > 0 && L->h > 0) gfx_set_surface(g, 0x14 + li, image_new(L->w, L->h), -1, true);
    hist_boundary(g, li);
    return OP_NEXT;
}

static void layout_clear(AG *g, i32 li) {
    Layout *L = &g->text.lay[li];
    Image *im = gfx_surface(g, 0x14 + li);
    if (im) memset(im->px, 0, sizeof(u32) * (size_t)im->w * (size_t)im->h);
    L->nrec = 0;
    L->shown = 0;
    L->cx = L->resetX;
    L->cy = L->resetY;
}

static int op_reset(AG *g) {
    Frame *f = &g->fr[g->top];
    f->t1 = script_table_index(f->sc->t1, f->sc->t1Count, f->pc);
    readtext_mark(g, f->sc->id, f->t1, f->sc->t1Count);
    for (int i = 0; i < AG_SURFACES; i++) memcpy(g->gfx.surf[i].snap, g->gfx.surf[i].rec, sizeof g->gfx.surf[i].rec);
    i32 li = layout_index(g, arg_i(g, 1));
    if (li < 0 || li > 9) return OP_NEXT;
    layout_clear(g, li);
    hist_boundary(g, li);
    if (li == g->text.indLayout) indicator_stop(g);
    return OP_NEXT;
}

static int op_reset_cursor(AG *g) {
    Layout *L = layout(g, arg_i(g, 1));
    if (L) { L->resetX = arg_i(g, 2); L->resetY = arg_i(g, 3); }
    return OP_NEXT;
}

static int op_cursor(AG *g) {
    Layout *L = layout(g, arg_i(g, 1));
    if (L) { L->cx = arg_i(g, 2); L->cy = arg_i(g, 3); }
    return OP_NEXT;
}

static int op_origin(AG *g) {
    Layout *L = layout(g, arg_i(g, 1));
    if (L) { L->ox = arg_i(g, 2); L->oy = arg_i(g, 3); }
    return OP_NEXT;
}

static int op_bounds(AG *g) {
    Layout *L = layout(g, arg_i(g, 1));
    if (L) { L->boundR = arg_i(g, 2); L->boundB = arg_i(g, 3); }
    return OP_NEXT;
}

static int op_range(AG *g) {
    Layout *L = layout(g, arg_i(g, 1));
    if (L) { L->first = arg_i(g, 2); L->capacity = arg_i(g, 3); }
    return OP_NEXT;
}

static int op_ind_handle(AG *g) {
    Layout *L = layout(g, arg_i(g, 1));
    if (L) L->indHandle = arg_i(g, 2);
    return OP_NEXT;
}

static int op_ind_grid(AG *g) {
    Layout *L = layout(g, arg_i(g, 1));
    if (!L) return OP_NEXT;
    L->indX = arg_i(g, 2);
    L->indY = arg_i(g, 3);
    L->indSurf = arg_i(g, 4);
    L->indSX = arg_i(g, 5);
    L->indSY = arg_i(g, 6);
    L->indW = arg_i(g, 7);
    L->indH = arg_i(g, 8);
    L->indCols = arg_i(g, 9);
    L->indFrames = arg_i(g, 10);
    L->indPeriod = arg_i(g, 11);
    L->indConfigured = true;
    return OP_NEXT;
}

static void show_into(AG *g, i32 li, const char *s, bool record, bool wrap) {
    Layout *L = &g->text.lay[li];
    Image *im = gfx_surface(g, 0x14 + li);
    Pen p;
    if (!pen_init(g, &p)) return;
    Text *t = &g->text;
    bool start = L->shown >= L->nrec;
    char row[1024];
    int rn = 0;
    i32 rowL = L->cx, rowT = L->cy;
    for (const u8 *q = (const u8 *)s; *q;) {
        int a;
        u32 cp;
        int len = ag_sjis_decode(q, &cp);
        glyph(g, &p, NULL, q, 0, 0, false, &a);
        int b = L->cy + t->size + (t->effMode ? t->effY : 0);
        if (wrap && L->cy + t->size > L->boundB && L->boundB != 0x7fffffff) break;
        if (wrap && L->cx + a > L->boundR && L->boundR != 0x7fffffff && !closing_class(cp)) {
            if (record && rn) { row[rn] = 0; hist_text(g, li, rowL, rowT, L->cx, rowT + t->size, row); }
            if (record) hist_break(g, li);
            rn = 0;
            L->cx = L->resetX;
            L->cy += t->size + t->leading;
            rowL = L->cx;
            rowT = L->cy;
            b = L->cy + t->size + (t->effMode ? t->effY : 0);
        }
        if (rn + len < (int)sizeof row - 1) { memcpy(row + rn, q, (size_t)len); rn += len; }
        q += glyph(g, &p, im, q, L->cx, L->cy, true, &a);
        GlyphRec r = { 0, L->cx, L->cy, L->cx + a, b };
        rec_push(L, r);
        L->cx += a;
    }
    if (record && rn) { row[rn] = 0; hist_text(g, li, rowL, rowT, L->cx, rowT + t->size, row); }
    if (start) L->nextAt = g->now;
    if (t->delay <= 0 || g->in.skip || !record) {
        while (L->shown < L->nrec && L->shown < L->capacity) publish(g, li, L->shown++);
    }
}

static void show(AG *g, i32 slot, const char *s) {
    i32 li = layout_index(g, slot);
    if (li < 1 || li > 9) return;
    show_into(g, li, s, true, true);
}

void text_render_records(AG *g, i32 li, int first, bool reflow, i32 flags, i32 col, i32 eff) {
    Text *t = &g->text;
    if (li < 1 || li > 9 || first < 0 || first >= t->nrec) return;
    Layout *L = &t->lay[li];
    i32 sc = t->color, se = t->effColor, ss = t->size;
    bool rec = t->recording;
    t->recording = false;
    if (!reflow) {
        for (i32 h = L->first; h < L->first + L->capacity; h++) gfx_erase(g, h);
        layout_clear(g, li);
    }
    for (int i = first; i < t->nrec; i++) {
        HistRec *r = &t->rec[i];
        if (i > first && (r->d[10] & HF_GROUP)) break;
        if ((r->d[10] & HF_FILTERED) && !(flags & 1)) continue;
        if (r->d[10] & (HF_META | HF_VOICE)) continue;
        if (r->d[10] & HF_BREAK) {
            if (reflow) { L->cx = L->resetX; L->cy += t->size + t->leading; }
            continue;
        }
        if (!reflow) {
            L->cx = r->d[1];
            L->cy = r->d[2];
            t->size = r->d[9] > 0 ? r->d[9] : ss;
            t->color = (i32)(((u32)r->d[5] >> 16 & 255) | ((u32)r->d[5] & 0xff00) | ((u32)r->d[5] & 255) << 16);
            t->effColor = (i32)(((u32)r->d[6] >> 16 & 255) | ((u32)r->d[6] & 0xff00) | ((u32)r->d[6] & 255) << 16);
        }
        if (flags & 2) { t->color = col; t->effColor = eff; }
        show_into(g, li, r->text ? r->text : "", false, false);
        t->color = sc;
        t->effColor = se;
        t->size = ss;
    }
    if (flags & 4) for (i32 h = L->first; h < L->first + L->capacity; h++) { Obj *o = gfx_obj(g, h, false); if (o) o->visible = false; }
    t->recording = rec;
}

static int op_show(AG *g) {
    show(g, arg_i(g, 1), arg_s(g, 2));
    if (text_revealing(g)) { g->wait = W_TEXT; return OP_WAIT; }
    return OP_NEXT;
}

static int op_ruby(AG *g) {
    show(g, arg_i(g, 1), arg_s(g, 2));
    if (text_revealing(g)) { g->wait = W_TEXT; return OP_WAIT; }
    return OP_NEXT;
}

static int op_newline(AG *g) {
    Layout *L = layout(g, arg_i(g, 1));
    if (!L) return OP_NEXT;
    hist_break(g, layout_index(g, arg_i(g, 1)));
    L->cx = L->resetX;
    L->cy += g->text.size + g->text.leading;
    return OP_NEXT;
}

static int op_wait(AG *g) {
    i32 li = layout_index(g, arg_i(g, 1));
    Text *t = &g->text;
    if (li >= 1 && li <= 9) {
        t->indLayout = li;
        if (!t->indActive) t->indStart = g->now;
        t->indActive = true;
        indicator_place(g);
    }
    g->in.waitStart = g->now;
    g->wait = W_INPUT;
    return OP_WAIT;
}

static int op_publish(AG *g) {
    i32 li = layout_index(g, arg_i(g, 1));
    if (li < 1 || li > 9) return OP_NEXT;
    Layout *L = &g->text.lay[li];
    for (int i = 0; i < L->shown; i++) publish(g, li, i);
    return OP_NEXT;
}

static int op_size(AG *g) {
    i32 s = arg_i(g, 1);
    if (s == 32 || s == 33) s = 31;
    else if (s & 1) s--;
    g->text.size = s;
    return OP_NEXT;
}

static int op_color(AG *g) { g->text.color = arg_i(g, 1); return OP_NEXT; }
static int op_eff_color(AG *g) { g->text.effColor = arg_i(g, 1); return OP_NEXT; }
static int op_eff_mode(AG *g) { g->text.effMode = arg_i(g, 1); return OP_NEXT; }
static int op_eff_offset(AG *g) { g->text.effX = arg_i(g, 1); g->text.effY = arg_i(g, 2); return OP_NEXT; }
static int op_face(AG *g) { snprintf(g->text.face, sizeof g->text.face, "%s", arg_s(g, 1)); return OP_NEXT; }
static int op_bold(AG *g) { g->text.bold = arg_i(g, 1) != 0; return OP_NEXT; }
static int op_leading(AG *g) { g->text.leading = arg_i(g, 1); return OP_NEXT; }
static int op_ruby_size(AG *g) { g->text.rubySize = arg_i(g, 1); return OP_NEXT; }
static int op_aspect(AG *g) { g->text.aspect = arg_i(g, 1); return OP_NEXT; }
static int op_delay(AG *g) { g->text.delay = arg_i(g, 1); return OP_NEXT; }
static int op_get_delay(AG *g) { arg_set_i(g, 1, g->text.delay); return OP_NEXT; }
static int op_follow(AG *g) { g->text.followLast = arg_i(g, 1) != 0; return OP_NEXT; }
static int op_ind_enable(AG *g) { if (!arg_i(g, 1)) indicator_stop(g); return OP_NEXT; }
static int op_current(AG *g) { i32 v = arg_i(g, 1); if (v >= 1 && v <= 9) g->text.cur = v; return OP_NEXT; }
static int op_recording(AG *g) { g->text.recording = arg_i(g, 1) != 0; return OP_NEXT; }
static int op_win_alpha_get(AG *g) { arg_set_i(g, 1, g->text.winAlpha); return OP_NEXT; }
static int op_win_alpha_set(AG *g) { g->text.winAlpha = arg_i(g, 1); return OP_NEXT; }

static int op_draw_string(AG *g) {
    Image *im = gfx_surface(g, arg_i(g, 1));
    if (im) text_draw_string(g, im, arg_i(g, 2), arg_i(g, 3), arg_s(g, 4));
    return OP_NEXT;
}

static int op_draw_int(AG *g) {
    Image *im = gfx_surface(g, arg_i(g, 1));
    if (!im) return OP_NEXT;
    i32 x = arg_i(g, 2), y = arg_i(g, 3), v = arg_i(g, 4), width = arg_i(g, 5), flags = arg_i(g, 6);
    char digits[32], out[64];
    snprintf(digits, sizeof digits, "%lld", (long long)(v < 0 ? -(i64)v : v));
    char sign = 0;
    if (v < 0) sign = '-';
    else if (v > 0 && (flags & 8)) sign = '+';
    else if (v == 0 && (flags & 16)) sign = '+';
    if (v == 0 && (flags & 32)) { snprintf(digits, sizeof digits, "-"); sign = 0; }
    int n = (int)strlen(digits) + (sign ? 1 : 0);
    int o = 0;
    if (sign) out[o++] = sign;
    if ((flags & 1) && n < width) {
        for (int i = 0; i < width - n; i++) out[o++] = '0';
        n = width;
    }
    snprintf(out + o, sizeof out - (size_t)o, "%s", digits);
    int omitted = width > n ? width - n : 0;
    Text *t = &g->text;
    int full = t->size + (t->bold ? 1 : 0);
    bool ascii = (flags & 0x10000) != 0;
    int cell = ascii ? full / 2 : full;
    if (flags & 4) {
    } else if (flags & 2) x += omitted * cell / 2;
    else x += omitted * cell;
    if (ascii) text_draw_string(g, im, x, y, out);
    else {
        char wide[128];
        int k = 0;
        for (const char *q = out; *q && k < 120; q++) {
            u16 w2 = *q == '-' ? 0x817c : *q == '+' ? 0x817b : (u16)(0x824f + (*q - '0'));
            wide[k++] = (char)(w2 >> 8);
            wide[k++] = (char)(w2 & 255);
        }
        wide[k] = 0;
        text_draw_string(g, im, x, y, wide);
    }
    return OP_NEXT;
}

static int op_font_find(AG *g) {
    vnfont_match m;
    const char *n = arg_s(g, 2);
    if (*n == '@') n++;
    arg_set_i(g, 1, vnfont_resolve(n, 0, 0, &m) ? 0 : -1);
    return OP_NEXT;
}

static int op_font_count(AG *g) { arg_set_i(g, 1, 1); return OP_NEXT; }

static int op_font_name(AG *g) {
    arg_set_s(g, 1, arg_i(g, 2) == 0 ? g->text.face : "");
    return OP_NEXT;
}






static int op_nop(AG *g) { (void)g; return OP_NEXT; }

static int op_auto_time_get(AG *g) {
    i32 s = arg_i(g, 1);
    arg_set_i(g, 2, s == 0 || s == 1 ? g->text.autoTime[s] : 0);
    return OP_NEXT;
}

static int op_auto_time_set(AG *g) {
    i32 s = arg_i(g, 1);
    if (s == 0 || s == 1) g->text.autoTime[s] = arg_i(g, 2);
    return OP_NEXT;
}

static int op_numeric_style(AG *g) {
    i32 i = arg_i(g, 1);
    if (i < 0 || i > 10) return OP_NEXT;
    i32 *s = g->text.tabStyle[i];
    for (int k = 0; k < 5; k++) s[k] = arg_i(g, k + 2);
    s[5] = 0;
    s[6] = 0;
    return OP_NEXT;
}

static int op_numeric_style2(AG *g) {
    i32 i = arg_i(g, 1);
    if (i < 0 || i > 10) return OP_NEXT;
    i32 *s = g->text.tabStyle[i];
    for (int k = 0; k < 7; k++) s[k] = arg_i(g, k + 2);
    return OP_NEXT;
}

static int op_numeric_glyphs(AG *g) {
    i32 base = arg_i(g, 1), st = arg_i(g, 2), v = arg_i(g, 3), x = arg_i(g, 4), y = arg_i(g, 5), cap = arg_i(g, 6);
    i32 flags = arg_i(g, 7);
    if (st < 0 || st > 10 || cap <= 0) return OP_NEXT;
    i32 *s = g->text.tabStyle[st];
    for (int i = 0; i < cap; i++) {
        Obj *o = gfx_obj(g, base + i, false);
        if (o) { i32 h = o->handle; Obj *q = o; (void)q; gfx_erase(g, h); }
    }
    char a[32];
    snprintf(a, sizeof a, "%d", v < 0 ? -v : v);
    int n = (int)strlen(a);
    if ((flags & 1) && n < cap) {
        char b[32];
        int pad = cap - n;
        for (int i = 0; i < pad; i++) b[i] = '0';
        memcpy(b + pad, a, (size_t)n + 1);
        memcpy(a, b, sizeof b);
        n = cap;
    }
    if (n > cap) { memmove(a, a + (n - cap), (size_t)cap + 1); n = cap; }
    int start = (flags & 4) ? 0 : (flags & 2) ? (cap - n) / 2 : cap - n;
    for (int i = 0; i < n; i++) {
        Obj *o = gfx_obj(g, base + i, true);
        int d = a[i] - '0';
        o->slot = s[0];
        o->sl = s[1] + d * (s[3] + s[5]);
        o->st = s[2];
        o->sr = o->sl + s[3];
        o->sb = o->st + s[4];
        o->pos.x = (float)(x + (start + i) * (s[3] + s[6]));
        o->pos.y = (float)y;
        o->visible = true;
        o->live = false;
    }
    return OP_NEXT;
}

void text_register(void) {
    vm_register_draw(0x6e, op_show);
    vm_register_draw(0x6f, op_newline);
    vm_register_draw(0x70, op_define);
    vm_register_draw(0x71, op_reset);
    vm_register_draw(0x72, op_wait);
    vm_register_draw(0x75, op_size);
    vm_register_draw(0x76, op_color);
    vm_register_draw(0x77, op_eff_color);
    vm_register_draw(0x78, op_eff_mode);
    vm_register_draw(0x79, op_reset_cursor);
    vm_register_draw(0x7a, op_cursor);
    vm_register(0x7f, op_get_delay);
    vm_register_draw(0x80, op_current);
    vm_register_draw(0x8b, op_leading);
    vm_register(0x131, op_win_alpha_get);
    vm_register_draw(0x13a, op_numeric_style);
    vm_register_draw(0x141, op_win_alpha_set);
    vm_register_draw(0x196, op_ruby);
    vm_register_draw(0x197, op_ruby_size);
    vm_register_draw(0x198, op_origin);
    vm_register_draw(0x1a4, op_eff_offset);
    vm_register_draw(0x1a5, op_face);
    vm_register_draw(0x1b1, op_follow);
    vm_register_draw(0x1b5, op_delay);
    vm_register(0x1b8, op_auto_time_get);
    vm_register_draw(0x1b9, op_auto_time_set);
    vm_register_draw(0x1bb, op_recording);
    vm_register_draw(0x1c1, op_bounds);
    vm_register_draw(0x1ce, op_ind_enable);
    vm_register_draw(0x204, op_draw_string);
    vm_register_draw(0x205, op_draw_int);
    vm_register_draw(0x20a, op_publish);
    vm_register_draw(0x212, op_ind_handle);
    vm_register_draw(0x213, op_range);
    vm_register_draw(0x23b, op_numeric_glyphs);
    vm_register_draw(0x2bc, op_ind_grid);
    vm_register_draw(0x2bd, op_bold);
    vm_register_draw(0x2da, op_numeric_style2);
    vm_register_draw(0x2db, op_aspect);
    vm_register(0x2dc, op_font_count);
    vm_register(0x2dd, op_font_name);
    vm_register(0x2de, op_font_find);
    (void)string_width;
}

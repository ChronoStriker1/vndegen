// CG, CGACT, CGEND and CGINFO, from the manual's CG pages.
#include "gfx.h"
#include "text.h"
#include <math.h>

// A numeric keyword; "X-=2" and the like store their operator in the argument's type, as LET does.
static bool num(YCall *c, const char *key, double cur, double *out) {
    const YArg *a = yc_arg(c, key);
    if (!a || !a->size) return false;
    YVal v = yr_eval(c->y, c->t, a->data, a->size);
    double x = yv_as_flt(&v);
    yv_free(&v);
    switch (a->type >> 8) {
    case 1: x = cur + x; break;
    case 2: x = cur - x; break;
    case 3: x = cur * x; break;
    case 4: x = x != 0 ? cur / x : cur; break;
    case 5: x = x != 0 ? fmod(cur, x) : cur; break;
    }
    *out = x;
    return true;
}

static bool inum(YCall *c, const char *key, int *field) {
    double v;
    if (!num(c, key, *field, &v)) return false;
    *field = (int)(v < 0 ? ceil(v - 0.5) : floor(v + 0.5));
    return true;
}

static bool dnum(YCall *c, const char *key, double *field) { return num(c, key, *field, field); }

static void set_str(char **field, char *value) {
    free(*field);
    *field = value;
}

static int cg(YCall *c) {
    YR *y = c->y;
    char *id = yc_str(c, "ID");
    if (!id) return YR_NEXT;
    bool hasFile = yc_has(c, "FILE");
    size_t n = strlen(id);
    bool set = n && id[n - 1] == '/';
    YLayer *l = yr_layer(y, id, true, set);
    if (!l) { free(id); return YR_NEXT; }
    if (hasFile && !l->isSet) {
        char *file = yc_str(c, "FILE");
        yr_layer_load(y, l, file ? file : "");
        free(file);
    }
    i64 e;
    if (yc_int(c, "E", &e)) l->e = e != 0;
    inum(c, "A", &l->a);
    inum(c, "X", &l->x);
    inum(c, "Y", &l->y);
    inum(c, "Z", &l->z);
    inum(c, "MODE", &l->mode);
    inum(c, "SX", &l->sx);
    inum(c, "SY", &l->sy);
    inum(c, "T", &l->t);
    inum(c, "TA", &l->ta);
    if (yc_has(c, "TID")) set_str(&l->tid, yc_str(c, "TID"));
    if (yc_has(c, "MID")) set_str(&l->mid, yc_str(c, "MID"));
    double lint = (double)l->lint, lint2 = (double)l->lint2;
    if (num(c, "LINT", lint, &lint)) l->lint = (i64)lint;
    if (num(c, "LINT2", lint2, &lint2)) l->lint2 = (i64)lint2;
    if (yc_has(c, "F")) l->f = yc_int_or(c, "F", 0) != 0;
    bool fset = false;
    fset |= dnum(c, "FX", &l->fx);
    fset |= dnum(c, "FY", &l->fy);
    fset |= dnum(c, "FBX", &l->fbx);
    fset |= dnum(c, "FBY", &l->fby);
    fset |= dnum(c, "FCX", &l->fcx);
    fset |= dnum(c, "FCY", &l->fcy);
    fset |= dnum(c, "FRX", &l->frx);
    fset |= dnum(c, "FRY", &l->fry);
    fset |= dnum(c, "FRZ", &l->frz);
    if (fset && !yc_has(c, "F")) l->f = true;
    y->gfx->dirty = true;
    free(id);
    return YR_NEXT;
}

static int cgend(YCall *c) {
    char *id = yc_str(c, "ID");
    if (!id) return YR_NEXT;
    YLayer *l = yr_layer(c->y, id, false, false);
    if (l && l->parent) yr_layer_remove(c->y, l);
    free(id);
    return YR_NEXT;
}

static u32 pixel_at(const YLayer *l, int x, int y) {
    if (!l->px || x < 0 || y < 0 || x >= l->w || y >= l->h) return 0;
    return l->px[(size_t)y * (size_t)l->w + (size_t)x];
}

static i64 rgb_of(u32 p) { return (i64)((p & 0xff) << 16 | (p & 0xff00) | ((p >> 16) & 0xff)); }

static int cginfo(YCall *c) {
    YR *y = c->y;
    YRef out;
    if (!yc_ref(c, "LET", &out)) return YR_NEXT;
    char *id = yc_str(c, "ID");
    YLayer *l = id ? yr_layer(y, id, false, false) : NULL;
    YVal r = yv_int(0);
    if (yc_int_or(c, "EXIST", 0)) r = yv_int(l != NULL);
    else if (!l) r = yv_int(0);
    else if (yc_int_or(c, "E", 0)) r = yv_int(l->e);
    else if (yc_int_or(c, "A", 0)) r = yv_int(l->a);
    else if (yc_int_or(c, "X", 0)) r = yv_int(l->x);
    else if (yc_int_or(c, "Y", 0)) r = yv_int(l->y);
    else if (yc_int_or(c, "Z", 0)) r = yv_int(l->z);
    else if (yc_int_or(c, "SX", 0)) r = yv_int(l->isSet ? l->sx : l->w);
    else if (yc_int_or(c, "SY", 0)) r = yv_int(l->isSet ? l->sy : l->h);
    else if (yc_int_or(c, "MODE", 0)) r = yv_int(l->mode);
    else if (yc_int_or(c, "T", 0)) r = yv_int(l->t);
    else if (yc_int_or(c, "TA", 0)) r = yv_int(l->ta);
    else if (yc_int_or(c, "LINT", 0)) r = yv_int(l->lint);
    else if (yc_int_or(c, "LINT2", 0)) r = yv_int(l->lint2);
    else if (yc_int_or(c, "TEX", 0)) r = yv_int(0);
    else if (yc_int_or(c, "TID", 0)) r = yv_str(l->tid ? l->tid : "");
    else if (yc_int_or(c, "COLOR", 0)) {
        u32 p = pixel_at(l, (int)yc_int_or(c, "SET", 0), (int)yc_int_or(c, "SET2", 0));
        r = yv_int(rgb_of(p));
    } else if (yc_int_or(c, "ONMOUSE", 0) || yc_int_or(c, "ONMOUSE2", 0) || yc_int_or(c, "ONMOUSE3", 0)) {
        int ox, oy;
        yr_layer_origin(l, &ox, &oy);
        int mx = (int)yc_int_or(c, "SET", y->mouseX) - ox, my = (int)yc_int_or(c, "SET2", y->mouseY) - oy;
        bool shown = true;
        for (const YLayer *p = l; p; p = p->parent) if (!p->e) shown = false;
        if (!shown || l->isSet || mx < 0 || my < 0 || mx >= l->w || my >= l->h) r = yv_int(-1);
        else {
            u32 p = pixel_at(l, mx, my);
            if (yc_int_or(c, "ONMOUSE", 0)) r = yv_int(rgb_of(p));
            else if (yc_int_or(c, "ONMOUSE2", 0)) r = yv_int(p >> 24);
            else r = yv_int((i64)(p >> 24) << 24 | rgb_of(p));
        }
    } else yc_unknown(c, "CGINFO form");
    yr_ref_set(y, c->t, &out, &r);
    yv_free(&r);
    free(id);
    return YR_NEXT;
}

// ---- CGACT ----

typedef struct Rect { int x0, y0, x1, y1; } Rect;

// The area X, Y to X2, Y2 when given, else the whole layer, clipped to it.
static Rect area(YCall *c, const YLayer *l) {
    Rect r = { 0, 0, l->w, l->h };
    i64 v;
    if (yc_int(c, "X", &v)) r.x0 = (int)v;
    if (yc_int(c, "Y", &v)) r.y0 = (int)v;
    if (yc_int(c, "X2", &v)) r.x1 = (int)v;
    if (yc_int(c, "Y2", &v)) r.y1 = (int)v;
    if (r.x0 < 0) r.x0 = 0;
    if (r.y0 < 0) r.y0 = 0;
    if (r.x1 > l->w) r.x1 = l->w;
    if (r.y1 > l->h) r.y1 = l->h;
    if (r.x1 < r.x0) r.x1 = r.x0;
    if (r.y1 < r.y0) r.y1 = r.y0;
    return r;
}

static void copy_attrs(YLayer *d, const YLayer *s) {
    d->e = s->e;
    d->a = s->a;
    d->x = s->x;
    d->y = s->y;
    d->z = s->z;
    d->mode = s->mode;
    d->f = s->f;
    d->fx = s->fx; d->fy = s->fy; d->fbx = s->fbx; d->fby = s->fby; d->fcx = s->fcx; d->fcy = s->fcy;
    d->frx = s->frx; d->fry = s->fry; d->frz = s->frz;
}

// The source as an image: an image layer's own pixels, or a set rendered as it shows.
static uint32_t *source_pixels(YR *y, YLayer *l, int *w, int *h, bool *owned) {
    if (!l->isSet) { *w = l->w; *h = l->h; *owned = false; return l->px; }
    *w = l->sx > 0 ? l->sx : y->width;
    *h = l->sy > 0 ? l->sy : y->height;
    *owned = true;
    return yr_render_set(y, l, *w, *h);
}

// The layer that receives a result: ID2 when given (made when missing), else the source itself.
static YLayer *target(YCall *c, YLayer *src) {
    char *id2 = yc_str(c, "ID2");
    if (!id2) return src;
    YLayer *d = yr_layer(c->y, id2, true, false);
    free(id2);
    return d && !d->isSet ? d : src;
}

static void scale(const uint32_t *s, int sw, int sh, uint32_t *d, int dw, int dh, bool bilinear) {
    for (int Y = 0; Y < dh; Y++)
        for (int X = 0; X < dw; X++) {
            if (!bilinear || sw <= 1 || sh <= 1) {
                int sx = (int)((long)X * sw / dw), sy = (int)((long)Y * sh / dh);
                d[(size_t)Y * dw + X] = s[(size_t)sy * sw + sx];
                continue;
            }
            double fx = (X + 0.5) * sw / dw - 0.5, fy = (Y + 0.5) * sh / dh - 0.5;
            if (fx < 0) fx = 0;
            if (fy < 0) fy = 0;
            int ix = (int)fx, iy = (int)fy;
            if (ix >= sw - 1) ix = sw - 2;
            if (iy >= sh - 1) iy = sh - 2;
            double ax = fx - ix, ay = fy - iy;
            double acc[4] = { 0 };
            double wa = 0;
            for (int q = 0; q < 4; q++) {
                u32 p = s[(size_t)(iy + (q >> 1)) * sw + ix + (q & 1)];
                double w = ((q & 1) ? ax : 1 - ax) * ((q >> 1) ? ay : 1 - ay), a = (p >> 24) * w;
                acc[0] += (p & 0xff) * a;
                acc[1] += ((p >> 8) & 0xff) * a;
                acc[2] += ((p >> 16) & 0xff) * a;
                acc[3] += a;
                wa += w;
            }
            u32 out = 0;
            if (acc[3] > 0) {
                out = (u32)(acc[0] / acc[3] + 0.5) | (u32)(acc[1] / acc[3] + 0.5) << 8 | (u32)(acc[2] / acc[3] + 0.5) << 16 |
                      (u32)(acc[3] / (wa > 0 ? wa : 1) + 0.5) << 24;
            }
            d[(size_t)Y * dw + X] = out;
        }
}

static void per_pixel(YLayer *l, Rect r, void (*fn)(u32 *p, i64 set, i64 set2), i64 set, i64 set2) {
    for (int Y = r.y0; Y < r.y1; Y++)
        for (int X = r.x0; X < r.x1; X++) fn(&l->px[(size_t)Y * l->w + X], set, set2);
}

static u32 clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : (u32)v; }

static void fx_paint_rgb(u32 *p, i64 c, i64 n) { (void)n; *p = (*p & 0xff000000u) | (u32)((c >> 16) & 0xff) | (u32)(c & 0xff00) | (u32)((c & 0xff) << 16); }
static void fx_paint_alpha(u32 *p, i64 c, i64 n) { (void)n; *p = (*p & 0xffffffu) | (u32)(c & 0xff) << 24; }
static void fx_paint_argb(u32 *p, i64 c, i64 n) { (void)n; *p = (u32)((c >> 16) & 0xff) | (u32)(c & 0xff00) | (u32)((c & 0xff) << 16) | (u32)((c >> 24) & 0xff) << 24; }
static void fx_reverse(u32 *p, i64 a, i64 b) { (void)a; (void)b; *p = (*p & 0xff000000u) | (~*p & 0xffffffu); }

static void fx_akarusa(u32 *p, i64 v, i64 n) {
    // Brightness: 256 leaves the image as it is, 0 is black, 512 white.
    (void)n;
    int r = *p & 0xff, g = (*p >> 8) & 0xff, b = (*p >> 16) & 0xff;
    if (v <= 256) { r = r * (int)v / 256; g = g * (int)v / 256; b = b * (int)v / 256; }
    else { int k = (int)v - 256; r += (255 - r) * k / 256; g += (255 - g) * k / 256; b += (255 - b) * k / 256; }
    *p = (*p & 0xff000000u) | clamp8(r) | clamp8(g) << 8 | clamp8(b) << 16;
}

static void fx_meido(u32 *p, i64 c, i64 n) {
    // Per-channel lightness: 0x80 in a channel leaves it, 0xff doubles it, 0 makes it black.
    (void)n;
    int r = *p & 0xff, g = (*p >> 8) & 0xff, b = (*p >> 16) & 0xff;
    int kr = (int)((c >> 16) & 0xff), kg = (int)((c >> 8) & 0xff), kb = (int)(c & 0xff);
    *p = (*p & 0xff000000u) | clamp8(r * kr / 128) | clamp8(g * kg / 128) << 8 | clamp8(b * kb / 128) << 16;
}

static void fx_saido(u32 *p, i64 v, i64 n) {
    // Saturation: 256 leaves the image as it is, 0 is gray.
    (void)n;
    int r = *p & 0xff, g = (*p >> 8) & 0xff, b = (*p >> 16) & 0xff;
    int l = (r * 77 + g * 151 + b * 28) >> 8;
    r = l + (r - l) * (int)v / 256;
    g = l + (g - l) * (int)v / 256;
    b = l + (b - l) * (int)v / 256;
    *p = (*p & 0xff000000u) | clamp8(r) | clamp8(g) << 8 | clamp8(b) << 16;
}

static void fx_mono(u32 *p, i64 t, i64 n) {
    (void)n;
    int l = ((*p & 0xff) * 77 + ((*p >> 8) & 0xff) * 151 + ((*p >> 16) & 0xff) * 28) >> 8;
    u32 v = l >= t ? 255 : 0;
    *p = (*p & 0xff000000u) | v | v << 8 | v << 16;
}

static void merge(YLayer *src, YLayer *dst, bool bothAlpha) {
    // src goes on top of dst at src's position relative to dst; the result stays in dst.
    if (!src->px || !dst->px) return;
    int ox = src->x - dst->x, oy = src->y - dst->y;
    int ka = src->a > 256 ? 256 : src->a;
    for (int Y = 0; Y < src->h; Y++) {
        int dy = Y + oy;
        if (dy < 0 || dy >= dst->h) continue;
        for (int X = 0; X < src->w; X++) {
            int dx = X + ox;
            if (dx < 0 || dx >= dst->w) continue;
            u32 s = src->px[(size_t)Y * src->w + X], *d = &dst->px[(size_t)dy * dst->w + dx];
            u32 sa = ((s >> 24) * (u32)ka) >> 8;
            if (!sa) continue;
            u32 da = *d >> 24;
            if (bothAlpha) da = (da * (u32)(dst->a > 256 ? 256 : dst->a)) >> 8;
            u32 oa = sa + yr_mul8(da, 255 - sa);
            if (!oa) { *d = 0; continue; }
            u32 rgb = 0;
            for (int ch = 0; ch < 3; ch++) {
                u32 sc = (s >> (8 * ch)) & 0xff, dc = (*d >> (8 * ch)) & 0xff;
                u32 v = (sc * sa + dc * yr_mul8(da, 255 - sa)) / oa;
                rgb |= (v > 255 ? 255 : v) << (8 * ch);
            }
            *d = rgb | oa << 24;
        }
    }
    if (bothAlpha) dst->a = 256;
}

static int cgact(YCall *c) {
    YR *y = c->y;
    char *id = yc_str(c, "ID");
    if (!id) return YR_NEXT;
    YLayer *l = yr_layer(y, id, false, false);
    free(id);
    y->gfx->dirty = true;
    if (yc_int_or(c, "TEXT", 0)) {
        if (!l) {   // drawing text makes the layer
            char *nid = yc_str(c, "ID");
            l = yr_layer(y, nid, true, false);
            free(nid);
        }
        if (l && !l->isSet) {
            char *s = yc_str(c, "SETSTR");
            yr_text_draw(y, l, (int)yc_int_or(c, "BNO", 0), s ? s : "", (int)yc_int_or(c, "X", 0), (int)yc_int_or(c, "Y", 0));
            free(s);
        }
        return YR_NEXT;
    }
    if (!l) { yc_unknown(c, "CGACT on a missing layer"); return YR_NEXT; }
    if (yc_int_or(c, "COPY", 0) || yc_int_or(c, "COPY2", 0) || yc_int_or(c, "TEXCOPY", 0)) {
        bool attrs = yc_int_or(c, "COPY", 0) != 0;
        int sw, sh;
        bool owned;
        uint32_t *src = source_pixels(y, l, &sw, &sh, &owned);
        YLayer tmp = *l;
        tmp.w = sw;
        tmp.h = sh;
        Rect r = area(c, &tmp);
        YLayer *d = target(c, l);
        int w = r.x1 - r.x0, h = r.y1 - r.y0;
        uint32_t *px = yr_alloc(sizeof(uint32_t) * (size_t)(w > 0 ? w : 1) * (size_t)(h > 0 ? h : 1));
        for (int Y = 0; Y < h; Y++) memcpy(px + (size_t)Y * w, src + (size_t)(Y + r.y0) * sw + r.x0, sizeof(uint32_t) * (size_t)w);
        if (owned) free(src);
        if (d != l || !l->isSet) {
            free(d->px);
            d->px = px;
            d->w = w;
            d->h = h;
            yr_layer_check_opaque(d);
            if (attrs && d != l) copy_attrs(d, l);
        } else free(px);
        return YR_NEXT;
    }
    if (yc_int_or(c, "SWAP", 0) || yc_int_or(c, "SWAP2", 0)) {
        YLayer *d = target(c, l);
        if (d != l) {
            uint32_t *p = d->px; int w = d->w, h = d->h; bool o = d->opaque;
            d->px = l->px; d->w = l->w; d->h = l->h; d->opaque = l->opaque;
            l->px = p; l->w = w; l->h = h; l->opaque = o;
            if (yc_int_or(c, "SWAP", 0)) { YLayer t = *d; copy_attrs(d, l); copy_attrs(l, &t); }
        }
        return YR_NEXT;
    }
    if (yc_int_or(c, "SIZE", 0) || yc_int_or(c, "SIZE2", 0) || yc_int_or(c, "SIZE3", 0) || yc_int_or(c, "SIZE4", 0) ||
        yc_int_or(c, "SIZE5", 0)) {
        int sw, sh;
        bool owned;
        uint32_t *src = source_pixels(y, l, &sw, &sh, &owned);
        int dw = (int)yc_int_or(c, "SX", sw), dh = (int)yc_int_or(c, "SY", sh);
        YLayer *d = target(c, l);
        if (dw > 0 && dh > 0 && sw > 0 && sh > 0 && src) {
            uint32_t *px = yr_alloc(sizeof(uint32_t) * (size_t)dw * (size_t)dh);
            scale(src, sw, sh, px, dw, dh, !yc_int_or(c, "SIZE", 0));
            if (d->isSet) free(px);
            else {
                free(d->px);
                d->px = px;
                d->w = dw;
                d->h = dh;
                yr_layer_check_opaque(d);
                if (d != l) copy_attrs(d, l);
            }
        }
        if (owned) free(src);
        return YR_NEXT;
    }
    if (l->isSet) { yc_unknown(c, "CGACT on a layer set"); return YR_NEXT; }
    if (yc_int_or(c, "RESIZE", 0)) {
        int left = (int)yc_int_or(c, "X", 0), top = (int)yc_int_or(c, "Y", 0);
        int right = (int)yc_int_or(c, "X2", 0), bottom = (int)yc_int_or(c, "Y2", 0);
        int nw = l->w + left + right, nh = l->h + top + bottom;
        if (nw < 0) nw = 0;
        if (nh < 0) nh = 0;
        uint32_t *px = yr_alloc(sizeof(uint32_t) * (size_t)(nw ? nw : 1) * (size_t)(nh ? nh : 1));
        for (int Y = 0; Y < nh; Y++)
            for (int X = 0; X < nw; X++) {
                int sx = X - left, sy = Y - top;
                if (sx >= 0 && sy >= 0 && sx < l->w && sy < l->h) px[(size_t)Y * nw + X] = l->px[(size_t)sy * l->w + sx];
            }
        free(l->px);
        l->px = px;
        l->w = nw;
        l->h = nh;
        l->x -= left;
        l->y -= top;
        yr_layer_check_opaque(l);
        return YR_NEXT;
    }
    if (yc_int_or(c, "MERGE", 0) || yc_int_or(c, "MERGE2", 0)) {
        YLayer *d = target(c, l);
        if (d != l) { merge(l, d, yc_int_or(c, "MERGE2", 0) != 0); yr_layer_check_opaque(d); }
        return YR_NEXT;
    }
    // Effects that work pixel by pixel, on the area, into ID2 when given.
    YLayer *d = target(c, l);
    if (d != l) {
        free(d->px);
        d->px = yr_alloc(sizeof(uint32_t) * (size_t)(l->w ? l->w : 1) * (size_t)(l->h ? l->h : 1));
        memcpy(d->px, l->px, sizeof(uint32_t) * (size_t)l->w * (size_t)l->h);
        d->w = l->w;
        d->h = l->h;
        copy_attrs(d, l);
    }
    Rect r = area(c, d);
    i64 set = yc_int_or(c, "SET", 0), set2 = yc_int_or(c, "SET2", 0);
    if (yc_int_or(c, "RECTPAINT", 0)) per_pixel(d, r, fx_paint_rgb, set, 0);
    else if (yc_int_or(c, "RECTPAINT2", 0)) per_pixel(d, r, fx_paint_alpha, set, 0);
    else if (yc_int_or(c, "RECTPAINT3", 0)) per_pixel(d, r, fx_paint_argb, set, 0);
    else if (yc_int_or(c, "REVERSE", 0)) per_pixel(d, r, fx_reverse, 0, 0);
    else if (yc_int_or(c, "AKARUSA", 0)) per_pixel(d, r, fx_akarusa, set, 0);
    else if (yc_int_or(c, "MEIDO", 0)) per_pixel(d, r, fx_meido, set, 0);
    else if (yc_int_or(c, "SAIDO", 0)) per_pixel(d, r, fx_saido, set, 0);
    else if (yc_int_or(c, "MONO", 0)) per_pixel(d, r, fx_mono, set, 0);
    else if (yc_int_or(c, "VERTICAL", 0)) {
        for (int Y = 0; Y < d->h / 2; Y++)
            for (int X = 0; X < d->w; X++) {
                u32 *a = &d->px[(size_t)Y * d->w + X], *b = &d->px[(size_t)(d->h - 1 - Y) * d->w + X], t = *a;
                *a = *b;
                *b = t;
            }
    } else if (yc_int_or(c, "HORIZONTAL", 0)) {
        for (int Y = 0; Y < d->h; Y++)
            for (int X = 0; X < d->w / 2; X++) {
                u32 *a = &d->px[(size_t)Y * d->w + X], *b = &d->px[(size_t)Y * d->w + d->w - 1 - X], t = *a;
                *a = *b;
                *b = t;
            }
    } else if (yc_int_or(c, "MOSAIC", 0) && set > 1) {
        int s = (int)set;
        for (int Y = r.y0; Y < r.y1; Y += s)
            for (int X = r.x0; X < r.x1; X += s) {
                u32 p = d->px[(size_t)Y * d->w + X];
                for (int j = Y; j < Y + s && j < r.y1; j++)
                    for (int i = X; i < X + s && i < r.x1; i++) d->px[(size_t)j * d->w + i] = p;
            }
    } else if (yc_int_or(c, "BLUR", 0) && set > 0) {
        // A box blur of radius SET, horizontal then vertical, on straight colors weighted by alpha.
        int rad = (int)set;
        uint32_t *tmp = yr_alloc(sizeof(uint32_t) * (size_t)d->w * (size_t)d->h);
        for (int pass = 0; pass < 2; pass++) {
            uint32_t *s = pass ? tmp : d->px, *o = pass ? d->px : tmp;
            for (int Y = 0; Y < d->h; Y++)
                for (int X = 0; X < d->w; X++) {
                    u32 acc[4] = { 0 }, cnt = 0;
                    for (int k = -rad; k <= rad; k++) {
                        int xx = pass ? X : X + k, yy = pass ? Y + k : Y;
                        if (xx < 0 || yy < 0 || xx >= d->w || yy >= d->h) continue;
                        u32 p = s[(size_t)yy * d->w + xx], a = p >> 24;
                        acc[0] += (p & 0xff) * a; acc[1] += ((p >> 8) & 0xff) * a; acc[2] += ((p >> 16) & 0xff) * a; acc[3] += a;
                        cnt++;
                    }
                    u32 a = cnt ? acc[3] / cnt : 0;
                    o[(size_t)Y * d->w + X] = acc[3] ? (acc[0] / acc[3]) | (acc[1] / acc[3]) << 8 | (acc[2] / acc[3]) << 16 | a << 24 : 0;
                }
        }
        free(tmp);
    } else {
        yc_unknown(c, "CGACT form");
    }
    (void)set2;
    yr_layer_check_opaque(d);
    return YR_NEXT;
}

int yr_cmd_cg(YCall *c, int id) {
    switch (id) {
    case C_CG: return cg(c);
    case C_CGEND: return cgend(c);
    case C_CGINFO: return cginfo(c);
    case C_CGACT: return cgact(c);
    }
    return YR_NEXT;
}

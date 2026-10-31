// The compositor: every visible object, in priority order, drawn on the CPU into the 800x600 screen. Zoom and
// rotation turn around the object's center. DrawTransition masks an object by a grayscale image.
#include "n2.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct DrawList {
    Obj **items;
    int count, cap;
} DrawList;

static bool flag(N2 *n, const char *name) {
    Value *v = var_get(n, name);
    return v && v_truthy(v);
}

static void gather(N2 *n, Obj *o, DrawList *l, bool textHidden) {
    for (int i = 0; i < o->childCount; i++) {
        Obj *c = o->children[i];
        if (c->erased && c->kind != OBJ_TEXT) continue;
        if (textHidden && c->hideable) continue;
        if (!obj_state_visible(c)) continue;
        if (c->kind == OBJ_TEXTURE || c->kind == OBJ_COLOR || c->kind == OBJ_TEXT || c->kind == OBJ_SCROLLBAR ||
            c->kind == OBJ_BACKLOG) {
            if (!(c->kind == OBJ_TEXT && c->erased)) {
                if (l->count == l->cap) {
                    l->cap = l->cap ? l->cap * 2 : 64;
                    l->items = n2_grow(l->items, sizeof(Obj *) * (size_t)l->cap);
                }
                l->items[l->count++] = c;
            }
        }
        gather(n, c, l, textHidden);
    }
}

static int by_prio(const void *a, const void *b) {
    const Obj *x = *(Obj *const *)a, *y = *(Obj *const *)b;
    if (x->prio != y->prio) return x->prio < y->prio ? -1 : 1;
    return x->serial < y->serial ? -1 : x->serial > y->serial;
}

static inline u32 blend_px(u32 d, u32 s, u32 alpha, int mode) {
    // s is premultiplied; alpha (0-256) scales it.
    u32 sr = (s & 0xff) * alpha >> 8, sg = ((s >> 8) & 0xff) * alpha >> 8, sb = ((s >> 16) & 0xff) * alpha >> 8,
        sa = (s >> 24) * alpha >> 8;
    u32 dr = d & 0xff, dg = (d >> 8) & 0xff, db = (d >> 16) & 0xff;
    switch (mode) {
    case 1:  // add
        dr = dr + sr > 255 ? 255 : dr + sr;
        dg = dg + sg > 255 ? 255 : dg + sg;
        db = db + sb > 255 ? 255 : db + sb;
        break;
    case 2:  // subtract
        dr = dr > sr ? dr - sr : 0;
        dg = dg > sg ? dg - sg : 0;
        db = db > sb ? db - sb : 0;
        break;
    case 3: {  // multiply: d * (s + (1 - sa))
        u32 ia = 255 - sa;
        dr = dr * (sr + ia) / 255;
        dg = dg * (sg + ia) / 255;
        db = db * (sb + ia) / 255;
        break;
    }
    default: {
        u32 ia = 255 - sa;
        dr = sr + (dr * ia + 127) / 255;
        dg = sg + (dg * ia + 127) / 255;
        db = sb + (db * ia + 127) / 255;
        break;
    }
    }
    return dr | dg << 8 | db << 16 | 0xff000000u;
}

static u32 mask_alpha(Obj *o, int sx, int sy, u32 alpha) {
    Anim *an = &o->anims[ANIM_TRANSITION];
    if (o->transition < 0) return alpha;
    double t = o->transition / 1000.0;
    double m = 0;
    if (an->maskImage) {
        Image *mi = an->maskImage;
        int mx = sx * mi->w / N2_W, my = sy * mi->h / N2_H;
        if (mx < 0) mx = 0;
        if (my < 0) my = 0;
        if (mx >= mi->w) mx = mi->w - 1;
        if (my >= mi->h) my = mi->h - 1;
        m = (mi->px[(size_t)my * mi->w + mx] & 0xff) / 255.0;
    } else {
        return (u32)(alpha * t);
    }
    double f = an->feather / 1000.0;
    if (f < 0.001) f = 0.001;
    // The progress sweeps the mask from dark to light, with a soft edge `f` wide.
    double v = (t * (1 + f) - m) / f;
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    return (u32)(alpha * v);
}

static void draw_color(N2 *n, Obj *o, double x, double y) {
    u32 alpha = (u32)(o->opacity * 256 / 1000);
    if (!alpha) return;
    int x0 = (int)x, y0 = (int)y, x1 = x0 + o->w, y1 = y0 + o->h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > N2_W) x1 = N2_W;
    if (y1 > N2_H) y1 = N2_H;
    u32 s = (o->color >> 16 & 0xff) | (o->color & 0xff00) | (o->color & 0xff) << 16 | 0xff000000u;
    for (int py = y0; py < y1; py++) {
        u32 *row = n->screen + (size_t)py * N2_W;
        for (int px = x0; px < x1; px++) row[px] = blend_px(row[px], s, mask_alpha(o, px, py, alpha), o->blend);
    }
}

static inline u32 sample(const Image *img, double u, double v) {
    int x = (int)floor(u), y = (int)floor(v);
    if (x < 0 || y < 0 || x >= img->w || y >= img->h) return 0;
    return img->px[(size_t)y * img->w + x];
}

void n2_blit(N2 *n, Obj *o, const Image *img, double x, double y, u32 alpha) {
    if (!img || !alpha) return;
    double sx = o->zoomX / 1000.0, sy = o->zoomY / 1000.0;
    // A turn around the X or Y axis shows as a squash on the screen.
    sx *= cos(o->rotY * M_PI / 180);
    sy *= cos(o->rotX * M_PI / 180);
    double rz = -o->rotZ * M_PI / 180;
    bool simple = fabs(sx - 1) < 1e-6 && fabs(sy - 1) < 1e-6 && fabs(rz) < 1e-9;
    if (simple) {
        int ix = (int)lround(x), iy = (int)lround(y);
        int x0 = ix < 0 ? 0 : ix, y0 = iy < 0 ? 0 : iy;
        int x1 = ix + img->w > N2_W ? N2_W : ix + img->w, y1 = iy + img->h > N2_H ? N2_H : iy + img->h;
        bool masked = o->transition >= 0;
        for (int py = y0; py < y1; py++) {
            u32 *row = n->screen + (size_t)py * N2_W;
            const u32 *src = img->px + (size_t)(py - iy) * img->w - ix;
            if (!masked && alpha >= 256 && o->blend == 0) {
                for (int px = x0; px < x1; px++) {
                    u32 s = src[px];
                    u32 sa = s >> 24;
                    if (sa == 255) row[px] = s;
                    else if (sa) row[px] = blend_px(row[px], s, 256, 0);
                }
            } else {
                for (int px = x0; px < x1; px++) {
                    u32 s = src[px];
                    if (!(s >> 24) && o->blend != 3) continue;
                    row[px] = blend_px(row[px], s, masked ? mask_alpha(o, px, py, alpha) : alpha, o->blend);
                }
            }
        }
        return;
    }
    // General case: inverse-map each screen pixel of the transformed bounds.
    double cx = x + img->w / 2.0, cy = y + img->h / 2.0;
    double c = cos(rz), s = sin(rz);
    double hw = img->w * fabs(sx) / 2, hh = img->h * fabs(sy) / 2;
    double ex = fabs(hw * c) + fabs(hh * s), ey = fabs(hw * s) + fabs(hh * c);
    int x0 = (int)floor(cx - ex), x1 = (int)ceil(cx + ex), y0 = (int)floor(cy - ey), y1 = (int)ceil(cy + ey);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > N2_W) x1 = N2_W;
    if (y1 > N2_H) y1 = N2_H;
    if (fabs(sx) < 1e-6 || fabs(sy) < 1e-6) return;
    for (int py = y0; py < y1; py++) {
        u32 *row = n->screen + (size_t)py * N2_W;
        for (int px = x0; px < x1; px++) {
            double dx = px + 0.5 - cx, dy = py + 0.5 - cy;
            double u = (dx * c + dy * s) / sx + img->w / 2.0, v = (-dx * s + dy * c) / sy + img->h / 2.0;
            u32 p = sample(img, u, v);
            if (!(p >> 24)) continue;
            row[px] = blend_px(row[px], p, mask_alpha(o, px, py, alpha), o->blend);
        }
    }
}

static void draw_obj(N2 *n, Obj *o) {
    double x, y;
    obj_screen_pos(o, &x, &y);
    u32 alpha = (u32)(fmax(0, fmin(1000, o->opacity)) * 256 / 1000);
    switch (o->kind) {
    case OBJ_COLOR:
        if (o->centerX) x = (N2_W - o->w) / 2.0;
        if (o->centerY) y = (N2_H - o->h) / 2.0;
        draw_color(n, o, x, y);
        break;
    case OBJ_TEXTURE:
        if (!o->image) break;
        if (o->centerX) x = (N2_W - o->image->w) / 2.0;
        if (o->centerY) y = (N2_H - o->image->h) / 2.0;
        n2_blit(n, o, o->image, x, y, alpha);
        break;
    case OBJ_TEXT: text_draw(n, o, n->screen, (int)x, (int)y, o->opacity); break;
    case OBJ_SCROLLBAR:
        if (o->image) {
            double kx = o->sbX1 + (o->sbX2 - o->sbX1) * o->sbValue, ky = o->sbY1 + (o->sbY2 - o->sbY1) * o->sbValue;
            n2_blit(n, o, o->image, kx - o->image->w / 2.0, ky - o->image->h / 2.0, alpha);
        }
        break;
    case OBJ_BACKLOG: backlog_draw(n, o); break;
    }
}

void n2_render_to(N2 *n, u32 *dst) {
    u32 *saved = n->screen;
    n->screen = dst;
    for (int i = 0; i < N2_W * N2_H; i++) dst[i] = 0xff000000u;
    DrawList l = {0};
    bool hidden = flag(n, "$SYSTEM_text_erase");
    gather(n, &n->root, &l, hidden);
    gather(n, &n->sysRoot, &l, false);
    qsort(l.items, (size_t)l.count, sizeof(Obj *), by_prio);
    for (int i = 0; i < l.count; i++) draw_obj(n, l.items[i]);
    free(l.items);
    n->screen = saved;
}

void n2_render(N2 *n) {
    if (!n->dirty) return;
    n2_render_to(n, n->screen);
    n->dirty = false;
    n->frameSerial++;
}

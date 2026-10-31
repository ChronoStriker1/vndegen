// CG layers and the compositor. Layers hold straight-alpha pixels; sets composite into premultiplied buffers.
#include "gfx.h"
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#include "third_party/stb_image.h"

static YLayer *layer_new(YGfx *g, YLayer *parent, const char *name, bool set) {
    YLayer *l = yr_alloc(sizeof(YLayer));
    l->name = yr_strdup(name);
    l->parent = parent;
    l->isSet = set;
    l->e = true;
    l->a = 256;
    l->ta = 0;
    l->fbx = l->fby = 10000;
    l->fcx = l->fcy = 5000;
    l->order = g->order++;
    if (parent) {
        if (parent->nkids == parent->kidCap) {
            parent->kidCap = parent->kidCap ? parent->kidCap * 2 : 8;
            parent->kids = yr_realloc(parent->kids, sizeof(YLayer *) * (size_t)parent->kidCap);
        }
        parent->kids[parent->nkids++] = l;
    }
    return l;
}

void yr_layer_free(YLayer *l) {
    for (int i = 0; i < l->nkids; i++) yr_layer_free(l->kids[i]);
    free(l->kids);
    free(l->name);
    free(l->px);
    free(l->tid);
    free(l->mid);
    free(l->file);
    free(l);
}

void yr_layer_remove(YR *y, YLayer *l) {
    YLayer *p = l->parent;
    if (!p) return;
    for (int i = 0; i < p->nkids; i++)
        if (p->kids[i] == l) {
            memmove(&p->kids[i], &p->kids[i + 1], sizeof(YLayer *) * (size_t)(p->nkids - i - 1));
            p->nkids--;
            break;
        }
    if (y->gfx->cur == l) y->gfx->cur = y->gfx->root;
    yr_layer_free(l);
    y->gfx->dirty = true;
}

void yr_gfx_open(YR *y) {
    YGfx *g = y->gfx = yr_alloc(sizeof(YGfx));
    g->root = layer_new(g, NULL, ":", true);
    YLayer *w0 = layer_new(g, g->root, "W0", true);
    w0->sx = y->width;
    w0->sy = y->height;
    g->cur = w0;
    g->dirty = true;
}

void yr_gfx_close(YR *y) {
    if (!y->gfx) return;
    yr_layer_free(y->gfx->root);
    free(y->gfx);
    y->gfx = NULL;
}

static YLayer *child(YLayer *p, const char *name, size_t n) {
    for (int i = 0; i < p->nkids; i++)
        if (strlen(p->kids[i]->name) == n && !strncasecmp(p->kids[i]->name, name, n)) return p->kids[i];
    return NULL;
}

YLayer *yr_layer(YR *y, const char *id, bool create, bool set) {
    YGfx *g = y->gfx;
    if (!id) return NULL;
    YLayer *l = g->cur;
    const char *p = id;
    if (*p == ':') { l = g->root; p++; }
    size_t len = strlen(p);
    bool wantSet = set || (len && p[len - 1] == '/');
    if (!*p) return l;
    while (*p) {
        if (*p == '/') { p++; continue; }
        const char *e = strchr(p, '/');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        bool last = !e || !e[1];
        if (n == 1 && p[0] == '.') { p += n; continue; }
        if (n == 2 && p[0] == '.' && p[1] == '.') { if (l->parent) l = l->parent; p += n; continue; }
        YLayer *c = child(l, p, n);
        if (!c) {
            if (!create) return NULL;
            char name[256];
            snprintf(name, sizeof name, "%.*s", (int)(n < 255 ? n : 255), p);
            c = layer_new(g, l, name, last ? wantSet : true);
            g->dirty = true;
        }
        l = c;
        p += n;
    }
    return l;
}

void yr_layer_pixels(YLayer *l, int w, int h) {
    free(l->px);
    l->w = w > 0 ? w : 0;
    l->h = h > 0 ? h : 0;
    l->px = yr_alloc(sizeof(uint32_t) * (size_t)(l->w ? l->w : 1) * (size_t)(l->h ? l->h : 1));
    l->opaque = false;
}

void yr_layer_check_opaque(YLayer *l) {
    size_t n = (size_t)l->w * (size_t)l->h;
    l->opaque = n > 0;
    for (size_t i = 0; i < n; i++) if ((l->px[i] >> 24) != 0xff) { l->opaque = false; break; }
}

static const char *const imageExts[] = { ".png", ".bmp", ".jpg", ".gif", NULL };

bool yr_layer_load(YR *y, YLayer *l, const char *file) {
    if (!file || !*file) {
        // FILE="" makes a 1x1 black image.
        yr_layer_pixels(l, 1, 1);
        l->px[0] = 0xff000000u;
        l->opaque = true;
        free(l->file);
        l->file = yr_strdup("");
        return true;
    }
    YFile f;
    if (!yr_fs_read(y, file, imageExts, &f)) {
        yr_log(y, "CG: file not found: %s\n", file);
        return false;
    }
    int w, h, comp;
    unsigned char *img = stbi_load_from_memory(f.data, (int)f.size, &w, &h, &comp, 4);
    yr_file_free(&f);
    if (!img) {
        yr_log(y, "CG: cannot decode %s\n", file);
        return false;
    }
    free(l->px);
    l->px = (uint32_t *)img;   // stb_image allocates with malloc
    l->w = w;
    l->h = h;
    free(l->file);
    l->file = yr_strdup(file);
    yr_layer_check_opaque(l);
    return true;
}

void yr_layer_origin(const YLayer *l, int *x, int *y_) {
    int x0 = 0, y0 = 0;
    for (const YLayer *p = l; p && p->parent; p = p->parent) {
        if (!strcasecmp(p->name, "W0") && p->parent && !p->parent->parent) break;
        x0 += p->x;
        y0 += p->y;
    }
    *x = x0;
    *y_ = y0;
}

// ---- compositing ----

typedef struct Target {
    uint32_t *px;     // premultiplied RGBA
    int w, h;
    int cx0, cy0, cx1, cy1;   // clip
} Target;

#define mul8 yr_mul8

static void blend_px(uint32_t *d, u32 sr, u32 sg, u32 sb, u32 sa, int mode) {
    // sr..sb are premultiplied by sa; all 0..255.
    u32 v = *d, dr = v & 0xff, dg = (v >> 8) & 0xff, db = (v >> 16) & 0xff, da = v >> 24;
    u32 inv = 255 - sa;
    u32 r, g, b;
    switch (mode) {
    case 1:   // add
        r = dr + sr; g = dg + sg; b = db + sb;
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        break;
    case 2:   // subtract
        r = dr > sr ? dr - sr : 0; g = dg > sg ? dg - sg : 0; b = db > sb ? db - sb : 0;
        break;
    case 3:   // multiply
        r = mul8(dr, inv) + mul8(dr, sr); g = mul8(dg, inv) + mul8(dg, sg); b = mul8(db, inv) + mul8(db, sb);
        break;
    case 5:   // screen
        r = dr + mul8(sr, 255 - dr); g = dg + mul8(sg, 255 - dg); b = db + mul8(sb, 255 - db);
        break;
    default:
        r = sr + mul8(dr, inv); g = sg + mul8(dg, inv); b = sb + mul8(db, inv);
        break;
    }
    u32 a = sa + mul8(da, inv);
    *d = (r > 255 ? 255 : r) | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b) << 16 | (a > 255 ? 255 : a) << 24;
}

// The transition factor (0..256) of a layer pixel from its rule image.
static int rule_factor(const YLayer *rule, int lx, int ly, int t, int ta) {
    if (!rule || !rule->px || rule->w <= 0 || rule->h <= 0) return t >= 256 ? 256 : t <= 0 ? 0 : t;
    int rx = lx * rule->w / (rule->w ? rule->w : 1), ry = ly;
    if (rx >= rule->w) rx = rule->w - 1;
    if (ry >= rule->h) ry = ry % rule->h;
    if (rx < 0) rx = 0;
    if (ry < 0) ry = 0;
    u32 p = rule->px[(size_t)ry * (size_t)rule->w + (size_t)rx];
    int r = (int)((p & 0xff) * 77 + ((p >> 8) & 0xff) * 151 + ((p >> 16) & 0xff) * 28) >> 8;
    if (ta <= 0) return (t * 256 / 256 > r) ? 256 : 0;
    int threshold = t * (255 + ta) / 256;
    int f = (threshold - r) * 256 / ta;
    return f < 0 ? 0 : f > 256 ? 256 : f;
}

static void draw_image(YR *y, Target *dst, const YLayer *l, int ox, int oy, int k) {
    if (!l->px || l->w <= 0 || l->h <= 0 || k <= 0) return;
    const YLayer *rule = l->tid && *l->tid ? yr_layer(y, l->tid, false, false) : NULL;
    bool useRule = rule || (l->t > 0 && l->t < 256 && l->tid && *l->tid);
    if (l->f && (l->fbx != 10000 || l->fby != 10000 || l->frz != 0 || l->fx != 0 || l->fy != 0)) {
        // Scale and rotation about the center, then the offset; sampled bilinearly from the destination side.
        double sx = l->fbx / 10000.0, sy = l->fby / 10000.0;
        if (sx == 0 || sy == 0) return;
        double cxl = l->fcx / 10000.0 * l->w, cyl = l->fcy / 10000.0 * l->h;
        double ang = l->frz / 100.0 * M_PI / 180.0, cs = cos(ang), sn = sin(ang);
        double px0 = ox + cxl + l->fx, py0 = oy + cyl + l->fy;
        double minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
        double corners[4][2] = { { 0, 0 }, { l->w, 0 }, { 0, l->h }, { l->w, l->h } };
        for (int i = 0; i < 4; i++) {
            double ux = (corners[i][0] - cxl) * sx, uy = (corners[i][1] - cyl) * sy;
            double X = px0 + ux * cs - uy * sn, Y = py0 + ux * sn + uy * cs;
            if (X < minx) minx = X;
            if (X > maxx) maxx = X;
            if (Y < miny) miny = Y;
            if (Y > maxy) maxy = Y;
        }
        int x0 = (int)floor(minx), x1 = (int)ceil(maxx), y0 = (int)floor(miny), y1 = (int)ceil(maxy);
        if (x0 < dst->cx0) x0 = dst->cx0;
        if (y0 < dst->cy0) y0 = dst->cy0;
        if (x1 > dst->cx1) x1 = dst->cx1;
        if (y1 > dst->cy1) y1 = dst->cy1;
        for (int Y = y0; Y < y1; Y++)
            for (int X = x0; X < x1; X++) {
                double dx = X + 0.5 - px0, dy = Y + 0.5 - py0;
                double ux = (dx * cs + dy * sn) / sx + cxl - 0.5, uy = (-dx * sn + dy * cs) / sy + cyl - 0.5;
                if (ux < -0.5 || uy < -0.5 || ux > l->w - 0.5 || uy > l->h - 0.5) continue;
                int ix = (int)floor(ux), iy = (int)floor(uy);
                double fx = ux - ix, fy = uy - iy;
                double acc[4] = { 0, 0, 0, 0 };
                for (int q = 0; q < 4; q++) {
                    int qx = ix + (q & 1), qy = iy + (q >> 1);
                    if (qx < 0) qx = 0;
                    if (qy < 0) qy = 0;
                    if (qx >= l->w) qx = l->w - 1;
                    if (qy >= l->h) qy = l->h - 1;
                    double wgt = ((q & 1) ? fx : 1 - fx) * ((q >> 1) ? fy : 1 - fy);
                    u32 p = l->px[(size_t)qy * (size_t)l->w + (size_t)qx];
                    double a = (p >> 24) * wgt;
                    acc[0] += (p & 0xff) * a;
                    acc[1] += ((p >> 8) & 0xff) * a;
                    acc[2] += ((p >> 16) & 0xff) * a;
                    acc[3] += a;
                }
                u32 sa = (u32)(acc[3] * k / 256 + 0.5);
                if (!sa) continue;
                double m = (double)k / 256 / 255;
                blend_px(&dst->px[(size_t)Y * (size_t)dst->w + (size_t)X], (u32)(acc[0] * m + 0.5), (u32)(acc[1] * m + 0.5),
                         (u32)(acc[2] * m + 0.5), sa > 255 ? 255 : sa, l->mode);
            }
        return;
    }
    int x0 = ox, y0 = oy, x1 = ox + l->w, y1 = oy + l->h;
    if (x0 < dst->cx0) x0 = dst->cx0;
    if (y0 < dst->cy0) y0 = dst->cy0;
    if (x1 > dst->cx1) x1 = dst->cx1;
    if (y1 > dst->cy1) y1 = dst->cy1;
    if (x0 >= x1 || y0 >= y1) return;
    bool copy = l->opaque && k >= 256 && l->mode == 0 && !useRule;
    for (int Y = y0; Y < y1; Y++) {
        const uint32_t *s = l->px + (size_t)(Y - oy) * (size_t)l->w + (size_t)(x0 - ox);
        uint32_t *d = dst->px + (size_t)Y * (size_t)dst->w + (size_t)x0;
        if (copy) { memcpy(d, s, sizeof(uint32_t) * (size_t)(x1 - x0)); continue; }
        for (int X = x0; X < x1; X++, s++, d++) {
            u32 p = *s, a = p >> 24;
            if (!a) continue;
            int kk = k;
            if (useRule) kk = kk * rule_factor(rule, X - ox, Y - oy, l->t, l->ta) / 256;
            if (kk < 256) a = (a * (u32)kk) >> 8;
            if (!a) continue;
            if (a == 255 && l->mode == 0) { *d = p | 0xff000000u; continue; }
            blend_px(d, mul8(p & 0xff, a), mul8((p >> 8) & 0xff, a), mul8((p >> 16) & 0xff, a), a, l->mode);
        }
    }
}

// Blends a premultiplied buffer onto the target.
static void draw_buffer(Target *dst, const uint32_t *src, int w, int h, int ox, int oy, int k, int mode) {
    int x0 = ox, y0 = oy, x1 = ox + w, y1 = oy + h;
    if (x0 < dst->cx0) x0 = dst->cx0;
    if (y0 < dst->cy0) y0 = dst->cy0;
    if (x1 > dst->cx1) x1 = dst->cx1;
    if (y1 > dst->cy1) y1 = dst->cy1;
    for (int Y = y0; Y < y1; Y++) {
        const uint32_t *s = src + (size_t)(Y - oy) * (size_t)w + (size_t)(x0 - ox);
        uint32_t *d = dst->px + (size_t)Y * (size_t)dst->w + (size_t)x0;
        for (int X = x0; X < x1; X++, s++, d++) {
            u32 p = *s;
            if (!(p >> 24)) continue;
            u32 r = p & 0xff, g = (p >> 8) & 0xff, b = (p >> 16) & 0xff, a = p >> 24;
            if (k < 256) { r = (r * (u32)k) >> 8; g = (g * (u32)k) >> 8; b = (b * (u32)k) >> 8; a = (a * (u32)k) >> 8; }
            blend_px(d, r, g, b, a, mode);
        }
    }
}

static int by_z(const void *a, const void *b) {
    const YLayer *x = *(YLayer *const *)a, *z = *(YLayer *const *)b;
    if (x->z != z->z) return x->z < z->z ? -1 : 1;
    return x->order < z->order ? -1 : x->order > z->order;
}

static void draw_set(YR *y, Target *dst, const YLayer *set, int ox, int oy, int k);

static void draw_kids(YR *y, Target *dst, const YLayer *set, int ox, int oy, int k) {
    if (!set->nkids) return;
    YLayer **sorted = yr_alloc(sizeof(YLayer *) * (size_t)set->nkids);
    memcpy(sorted, set->kids, sizeof(YLayer *) * (size_t)set->nkids);
    qsort(sorted, (size_t)set->nkids, sizeof(YLayer *), by_z);
    for (int i = 0; i < set->nkids; i++) {
        const YLayer *c = sorted[i];
        if (!c->e || c->a <= 0) continue;
        int kk = k * (c->a > 256 ? 256 : c->a) / 256;
        if (c->isSet) draw_set(y, dst, c, ox + c->x, oy + c->y, kk);
        else draw_image(y, dst, c, ox + c->x, oy + c->y, kk);
    }
    free(sorted);
}

static void draw_set(YR *y, Target *dst, const YLayer *set, int ox, int oy, int k) {
    if (k >= 256 && set->mode == 0) {
        // Drawn straight into the target, clipped to the set's size.
        Target t = *dst;
        if (set->sx > 0 && set->sy > 0) {
            if (ox > t.cx0) t.cx0 = ox;
            if (oy > t.cy0) t.cy0 = oy;
            if (ox + set->sx < t.cx1) t.cx1 = ox + set->sx;
            if (oy + set->sy < t.cy1) t.cy1 = oy + set->sy;
            if (t.cx0 >= t.cx1 || t.cy0 >= t.cy1) return;
        }
        draw_kids(y, &t, set, ox, oy, 256);
        return;
    }
    // Translucent or blended sets composite their contents first, as one image.
    int w = set->sx > 0 ? set->sx : dst->w, h = set->sy > 0 ? set->sy : dst->h;
    int bx = set->sx > 0 ? ox : 0, by = set->sy > 0 ? oy : 0;
    uint32_t *buf = yr_alloc(sizeof(uint32_t) * (size_t)w * (size_t)h);
    Target t = { buf, w, h, 0, 0, w, h };
    draw_kids(y, &t, set, ox - bx, oy - by, 256);
    draw_buffer(dst, buf, w, h, bx, by, k, set->mode);
    free(buf);
}

uint32_t *yr_render_set(YR *y, YLayer *set, int w, int h) {
    uint32_t *buf = yr_alloc(sizeof(uint32_t) * (size_t)(w > 0 ? w : 1) * (size_t)(h > 0 ? h : 1));
    Target t = { buf, w, h, 0, 0, w, h };
    if (set->isSet) draw_kids(y, &t, set, 0, 0, 256);
    else draw_image(y, &t, set, 0, 0, 256);
    // Back to straight alpha.
    for (size_t i = 0; i < (size_t)w * (size_t)h; i++) {
        u32 p = buf[i], a = p >> 24;
        if (a && a < 255) {
            u32 r = (p & 0xff) * 255 / a, g = ((p >> 8) & 0xff) * 255 / a, b = ((p >> 16) & 0xff) * 255 / a;
            buf[i] = (r > 255 ? 255 : r) | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b) << 16 | a << 24;
        }
    }
    return buf;
}

void yr_gfx_present(YR *y) {
    YGfx *g = y->gfx;
    if (!g || !g->dirty) return;
    g->dirty = false;
    YLayer *w0 = yr_layer(y, ":/W0", false, true);
    size_t n = (size_t)y->width * (size_t)y->height;
    for (size_t i = 0; i < n; i++) y->screen[i] = 0xff000000u;
    if (w0 && w0->e) {
        Target t = { y->screen, y->width, y->height, 0, 0, y->width, y->height };
        draw_kids(y, &t, w0, 0, 0, w0->a > 256 ? 256 : w0->a);
    }
    for (size_t i = 0; i < n; i++) y->screen[i] |= 0xff000000u;
    y->serial++;
}

static void dump(YR *y, const YLayer *l, int depth) {
    yr_log(y, "%*s%s%s z=%d e=%d a=%d x=%d y=%d size=%dx%d%s%s mode=%d\n", depth * 2, "", l->name, l->isSet ? "/" : "", l->z,
           l->e, l->a, l->x, l->y, l->isSet ? l->sx : l->w, l->isSet ? l->sy : l->h, l->file ? " file=" : "",
           l->file ? l->file : "", l->mode);
    for (int i = 0; i < l->nkids; i++) dump(y, l->kids[i], depth + 1);
}

void yr_gfx_dump(YR *y) {
    if (y->gfx) dump(y, y->gfx->root, 0);
}

#include "eagls.h"

static inline u32 blend_px(u32 d, u32 s, u32 a) {
    if (a >= 255) return s | 0xff000000u;
    if (!a) return d;
    u32 ia = 255 - a;
    u32 r = ((s & 0xff) * a + (d & 0xff) * ia) / 255;
    u32 g = (((s >> 8) & 0xff) * a + ((d >> 8) & 0xff) * ia) / 255;
    u32 b = (((s >> 16) & 0xff) * a + ((d >> 16) & 0xff) * ia) / 255;
    return r | g << 8 | b << 16 | 0xff000000u;
}

void fill_rect(u32 *dst, int dw, int dh, int x, int y, int w, int h, u32 rgba) {
    u32 a = rgba >> 24;
    for (int j = y < 0 ? 0 : y; j < y + h && j < dh; j++)
        for (int i = x < 0 ? 0 : x; i < x + w && i < dw; i++) dst[(size_t)j * dw + i] = blend_px(dst[(size_t)j * dw + i], rgba & 0xffffff, a);
}

void blit_part(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int w, int h, int dx, int dy, int alpha) {
    if (!im || alpha <= 0) return;
    if (sx < 0) { dx -= sx; w += sx; sx = 0; }
    if (sy < 0) { dy -= sy; h += sy; sy = 0; }
    if (sx + w > im->w) w = im->w - sx;
    if (sy + h > im->h) h = im->h - sy;
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (dx + w > dw) w = dw - dx;
    if (dy + h > dh) h = dh - dy;
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) {
        const u32 *s = im->px + (size_t)(sy + j) * im->w + sx;
        u32 *d = dst + (size_t)(dy + j) * dw + dx;
        if (!im->alpha && alpha >= 255) {
            memcpy(d, s, sizeof(u32) * (size_t)w);
            continue;
        }
        for (int i = 0; i < w; i++) {
            u32 a = im->alpha ? (s[i] >> 24) * (u32)alpha / 255 : (u32)alpha;
            d[i] = blend_px(d[i], s[i] & 0xffffff, a);
        }
    }
}

void blit(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, int alpha) {
    if (im) blit_part(dst, dw, dh, im, 0, 0, im->w, im->h, dx, dy, alpha);
}

void blit_scaled(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int ddw, int ddh, int alpha) {
    if (!im || alpha <= 0 || sw <= 0 || sh <= 0 || ddw <= 0 || ddh <= 0) return;
    if (sw == ddw && sh == ddh) {
        blit_part(dst, dw, dh, im, sx, sy, sw, sh, dx, dy, alpha);
        return;
    }
    for (int j = 0; j < ddh; j++) {
        int y = dy + j;
        if (y < 0 || y >= dh) continue;
        int syy = sy + (int)((i64)j * sh / ddh);
        if (syy < 0 || syy >= im->h) continue;
        const u32 *s = im->px + (size_t)syy * im->w;
        u32 *d = dst + (size_t)y * dw;
        for (int i = 0; i < ddw; i++) {
            int x = dx + i;
            if (x < 0 || x >= dw) continue;
            int sxx = sx + (int)((i64)i * sw / ddw);
            if (sxx < 0 || sxx >= im->w) continue;
            u32 p = s[sxx];
            u32 a = im->alpha ? (p >> 24) * (u32)alpha / 255 : (u32)alpha;
            d[x] = blend_px(d[x], p & 0xffffff, a);
        }
    }
}

void gfx_init(EG *e) {
    size_t n = (size_t)e->W * (size_t)e->H;
    e->screen = eg_alloc(sizeof(u32) * n);
    e->shown = eg_alloc(sizeof(u32) * n);
    e->from = eg_alloc(sizeof(u32) * n);
    e->work = eg_alloc(sizeof(u32) * n);
    for (size_t i = 0; i < n; i++) e->shown[i] = e->screen[i] = 0xff000000u;
}

void gfx_close(EG *e) {
    for (int i = 0; i < EG_PLANES; i++) image_free(e->plane[i]);
    image_free(e->transMask);
    free(e->screen);
    free(e->shown);
    free(e->from);
    free(e->work);
}

void gfx_free_plane(EG *e, int p) {
    if (p < 0 || p >= EG_PLANES) return;
    image_free(e->plane[p]);
    e->plane[p] = NULL;
    e->planeName[p][0] = 0;
}

void gfx_load(EG *e, const char *name, int p) {
    if (p < 0 || p >= EG_PLANES) return;
    Image *im = eg_image_load(e, name);
    if (!im) eg_log(e, "missing picture %s", name);
    image_free(e->plane[p]);
    e->plane[p] = im;
    snprintf(e->planeName[p], sizeof e->planeName[p], "%s", name);
}

void gfx_layer_reset(EG *e, int l) {
    if (l < 0 || l >= EG_PLANES) return;
    memset(&e->layer[l], 0, sizeof e->layer[l]);
}

void gfx_set_layer(EG *e, int l, int plane, const int *r, int alpha) {
    Layer *ly = &e->layer[l];
    ly->vis = true;
    ly->plane = plane;
    ly->dx = r[0];
    ly->dy = r[1];
    ly->dw = r[2];
    ly->dh = r[3];
    ly->sx = r[4];
    ly->sy = r[5];
    ly->sw = r[6];
    ly->sh = r[7];
    ly->alpha = clampi(alpha, 0, 255);
}

void gfx_copy(EG *e, int dst, int dx, int dy, int w, int h, int src, int sx, int sy) {
    if (dst < 0 || dst >= EG_PLANES || src < 0 || src >= EG_PLANES) return;
    Image *d = e->plane[dst], *s = e->plane[src];
    if (!d || !s) return;
    for (int j = 0; j < h; j++) {
        int y = dy + j, yy = sy + j;
        if (y < 0 || y >= d->h || yy < 0 || yy >= s->h) continue;
        for (int i = 0; i < w; i++) {
            int x = dx + i, xx = sx + i;
            if (x < 0 || x >= d->w || xx < 0 || xx >= s->w) continue;
            d->px[(size_t)y * d->w + x] = s->px[(size_t)yy * s->w + xx];
        }
    }
}

void gfx_copy_mask(EG *e, int dst, int dx, int dy, int w, int h, int src, int sx, int sy, int maskPlane, int color) {
    if (dst < 0 || dst >= EG_PLANES || src < 0 || src >= EG_PLANES || maskPlane < 0 || maskPlane >= EG_PLANES) return;
    Image *d = e->plane[dst], *s = e->plane[src], *m = e->plane[maskPlane];
    if (!d || !s || !m) return;
    for (int j = 0; j < h; j++) {
        int y = dy + j, yy = sy + j;
        if (y < 0 || y >= d->h || yy < 0 || yy >= s->h || yy >= m->h) continue;
        for (int i = 0; i < w; i++) {
            int x = dx + i, xx = sx + i;
            if (x < 0 || x >= d->w || xx < 0 || xx >= s->w || xx >= m->w) continue;
            if ((int)(m->px[(size_t)yy * m->w + xx] & 0xff) == color) d->px[(size_t)y * d->w + x] = s->px[(size_t)yy * s->w + xx];
        }
    }
}

int gfx_color_at(EG *e, int plane, int x, int y) {
    if (plane < 0 || plane >= EG_PLANES || !e->plane[plane]) return 0;
    Image *m = e->plane[plane];
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    u32 p = m->px[(size_t)y * m->w + x];
    if (m->alpha && !(p >> 24)) return 0;
    return (int)(p & 0xff);
}

void gfx_compose(EG *e, u32 *dst) {
    size_t n = (size_t)e->W * (size_t)e->H;
    for (size_t i = 0; i < n; i++) dst[i] = 0xff000000u;
    for (int l = 0; l < EG_PLANES; l++) {
        Layer *ly = &e->layer[l];
        if (!ly->vis || ly->plane < 0 || ly->plane >= EG_PLANES) continue;
        Image *im = e->plane[ly->plane];
        if (!im) continue;
        blit_scaled(dst, e->W, e->H, im, ly->sx, ly->sy, ly->sw, ly->sh, ly->dx, ly->dy, ly->dw, ly->dh, ly->alpha);
    }
}

void gfx_present(EG *e) {
    if (e->transActive) {
        e->transActive = false;
        image_free(e->transMask);
        e->transMask = NULL;
    }
    gfx_compose(e, e->shown);
    e->serial++;
}

void gfx_transition(EG *e, int effect, const char *mask, int ms, bool reverse) {
    if (e->transActive) memcpy(e->from, e->screen, sizeof(u32) * (size_t)e->W * (size_t)e->H);
    else memcpy(e->from, e->shown, sizeof(u32) * (size_t)e->W * (size_t)e->H);
    image_free(e->transMask);
    e->transMask = NULL;
    bool skip = e->skipMode || e->keyDown[0x11];
    if (effect == 0 || ms <= 0 || skip) {
        e->transActive = false;
        gfx_present(e);
        return;
    }
    if (mask && *mask && strcmp(mask, "0")) {
        e->transMask = eg_image_load(e, mask);
        if (!e->transMask) eg_log(e, "missing transition mask %s", mask);
    }
    e->transEffect = effect;
    e->transReverse = reverse;
    e->transStart = e->now;
    e->transDur = (u32)ms;
    e->transActive = true;
}

bool gfx_transition_done(EG *e) {
    if (!e->transActive) return true;
    if (e->click && !e->transMask) {
        gfx_present(e);
        return true;
    }
    if (e->now - e->transStart >= e->transDur) {
        gfx_present(e);
        return true;
    }
    return false;
}

static void mask_blend(EG *e, u32 *out, const u32 *a, const u32 *b, int p256) {
    Image *m = e->transMask;
    int W = e->W, H = e->H;
    int edge = 32;
    int level = p256 * (256 + edge) / 256;
    for (int y = 0; y < H; y++) {
        int my = m->h == H ? y : (int)((i64)y * m->h / H);
        for (int x = 0; x < W; x++) {
            int mx = m->w == W ? x : (int)((i64)x * m->w / W);
            int v = (int)(m->px[(size_t)my * m->w + mx] & 0xff);
            if (e->transReverse) v = 255 - v;
            int t = level - v;
            int al = t <= 0 ? 0 : t >= edge ? 255 : t * 255 / edge;
            size_t i = (size_t)y * W + x;
            out[i] = blend_px(a[i], b[i] & 0xffffff, (u32)al);
        }
    }
}

void gfx_frame(EG *e) {
    size_t n = (size_t)e->W * (size_t)e->H;
    if (e->transActive) {
        gfx_compose(e, e->work);
        u32 el = e->now - e->transStart;
        int p = e->transDur ? (int)((u64)el * 256 / e->transDur) : 256;
        if (p > 256) p = 256;
        if (e->transMask) mask_blend(e, e->screen, e->from, e->work, p);
        else
            for (size_t i = 0; i < n; i++) e->screen[i] = blend_px(e->from[i], e->work[i] & 0xffffff, (u32)(p * 255 / 256));
    } else if (movie_any(e)) {
        gfx_compose(e, e->shown);
        memcpy(e->screen, e->shown, sizeof(u32) * n);
    } else memcpy(e->screen, e->shown, sizeof(u32) * n);
    msg_draw(e, e->screen);
    choice_draw(e, e->screen);
    win_draw(e, e->screen);
    ui_draw(e, e->screen);
    e->serial++;
}

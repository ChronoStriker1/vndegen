#include "anim.h"

void an_gfx_fill(u32 *dst, int w, int h, u32 color) {
    size_t n = (size_t)w * (size_t)h;
    for (size_t i = 0; i < n; i++) dst[i] = color;
}

static inline u32 blend(u32 d, u32 s, u32 a) {
    if (a >= 255) return s | 0xff000000u;
    if (!a) return d;
    u32 ia = 255 - a;
    u32 rb = (((s & 0xff00ffu) * a + (d & 0xff00ffu) * ia) >> 8) & 0xff00ffu;
    u32 g = (((s & 0xff00u) * a + (d & 0xff00u) * ia) >> 8) & 0xff00u;
    return rb | g | 0xff000000u;
}

void an_gfx_blit(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int alpha) {
    if (!im || alpha <= 0) return;
    int x0 = sx > im->x0 ? sx : im->x0;
    int y0 = sy > im->y0 ? sy : im->y0;
    int x1 = sx + sw < im->x0 + im->cw ? sx + sw : im->x0 + im->cw;
    int y1 = sy + sh < im->y0 + im->ch ? sy + sh : im->y0 + im->ch;
    dx += x0 - sx;
    dy += y0 - sy;
    if (dx < 0) {
        x0 -= dx;
        dx = 0;
    }
    if (dy < 0) {
        y0 -= dy;
        dy = 0;
    }
    if (dx + (x1 - x0) > dw) x1 = x0 + dw - dx;
    if (dy + (y1 - y0) > dh) y1 = y0 + dh - dy;
    if (x1 <= x0 || y1 <= y0) return;
    for (int y = y0; y < y1; y++) {
        const u32 *s = im->px + (size_t)(y - im->y0) * (size_t)im->cw + (size_t)(x0 - im->x0);
        u32 *d = dst + (size_t)(dy + y - y0) * (size_t)dw + (size_t)dx;
        for (int x = 0; x < x1 - x0; x++) {
            u32 a = (s[x] >> 24) * (u32)alpha / 255;
            if (a) d[x] = blend(d[x], s[x], a);
        }
    }
}

static void blit_scaled(u32 *dst, int dw, int dh, const Image *im, double cx, double cy, double scale, int alpha) {
    if (!im || alpha <= 0 || scale <= 0) return;
    double inv = 1.0 / scale;
    int x0 = (int)(cx + im->x0 * scale), y0 = (int)(cy + im->y0 * scale);
    int x1 = (int)(cx + (im->x0 + im->cw) * scale) + 1, y1 = (int)(cy + (im->y0 + im->ch) * scale) + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > dw) x1 = dw;
    if (y1 > dh) y1 = dh;
    for (int y = y0; y < y1; y++) {
        int sy = (int)((y - cy) * inv);
        if (sy < im->y0 || sy >= im->y0 + im->ch) continue;
        const u32 *row = im->px + (size_t)(sy - im->y0) * (size_t)im->cw;
        u32 *d = dst + (size_t)y * (size_t)dw;
        for (int x = x0; x < x1; x++) {
            int sx = (int)((x - cx) * inv);
            if (sx < im->x0 || sx >= im->x0 + im->cw) continue;
            u32 s = row[sx - im->x0];
            u32 a = (s >> 24) * (u32)alpha / 255;
            if (a) d[x] = blend(d[x], s, a);
        }
    }
}

static int lerp(int from, int to, u32 start, u32 time, u32 now) {
    if (!time || now - start >= time) return to;
    return from + (int)((i64)(to - from) * (i64)(now - start) / (i64)time);
}

static void draw_scene(AN *a, u32 *fb) {
    Scene *s = &a->scene;
    an_gfx_fill(fb, a->W, a->H, 0xff000000u);
    an_emote_prepare(a);
    Image *bg = an_image_load(a, s->bg);
    if (bg) {
        int x = lerp(s->bgFromX, s->bgX, s->bgMoveStart, s->bgMoveTime, a->now);
        int y = lerp(s->bgFromY, s->bgY, s->bgMoveStart, s->bgMoveTime, a->now);
        if (s->bgScale > 0 && s->bgScale != 1000) blit_scaled(fb, a->W, a->H, bg, -x * s->bgScale / 1000.0, -y * s->bgScale / 1000.0, s->bgScale / 1000.0, 255);
        else an_gfx_blit(fb, a->W, a->H, bg, x, y, a->W, a->H, 0, 0, 255);
    }
    if (an_movie_looping(a)) an_movie_draw(a, fb);
    for (int i = 0; i < AN_LAYERS; i++) {
        Layer *l = &s->layer[i];
        if (l->kind == LK_NONE) continue;
        int x = lerp(l->fromX, l->toX, l->moveStart, l->moveTime, a->now);
        int y = lerp(l->fromY, l->toY, l->moveStart, l->moveTime, a->now);
        int alpha = lerp(l->fromAlpha, l->toAlpha, l->fadeStart, l->fadeTime, a->now);
        if (l->kind == LK_EMOTE) {
            an_emote_draw(a, i, l, x, y, alpha);
            continue;
        }
        Image *im = an_image_load(a, l->name);
        if (!im) continue;
        double scale = l->scale > 0 ? l->scale / 1000.0 : 1.0;
        if (scale != 1.0)
            blit_scaled(fb, a->W, a->H, im, x, y, scale, alpha);
        else
            an_gfx_blit(fb, a->W, a->H, im, 0, 0, im->w, im->h, x, y, alpha);
    }
}

void an_gfx_snapshot(AN *a) { memcpy(a->prev, a->fb, sizeof(u32) * (size_t)a->W * (size_t)a->H); }

void an_gfx_begin_transition(AN *a, int kind, u32 ms, u32 color) {
    an_gfx_snapshot(a);
    a->transKind = kind;
    a->transStart = a->now;
    a->transTime = ms;
    a->transColor = color;
}

bool an_gfx_transition_done(AN *a) { return !a->transTime || a->now - a->transStart >= a->transTime; }

static void apply_transition(AN *a) {
    if (an_gfx_transition_done(a)) {
        a->transTime = 0;
        return;
    }
    u32 t = (a->now - a->transStart) * 255u / a->transTime;
    size_t n = (size_t)a->W * (size_t)a->H;
    if (a->transKind == 0x28) {
        u32 half = 128;
        for (size_t i = 0; i < n; i++) {
            u32 c = a->transColor | 0xff000000u;
            if (t < half) a->fb[i] = blend(a->prev[i], c, t * 2);
            else a->fb[i] = blend(c, a->fb[i], (t - half) * 2 > 255 ? 255 : (t - half) * 2);
        }
        return;
    }
    for (size_t i = 0; i < n; i++) a->fb[i] = blend(a->prev[i], a->fb[i], t);
}

void an_gfx_render(AN *a) {
    if (a->screen == SCR_FLOW) {
        an_flow_render(a);
        an_screens_render(a);
        a->serial++;
        return;
    }
    if (a->phase == PH_TITLE) {
        an_title_render(a);
        an_screens_render(a);
        a->serial++;
        return;
    }
    if (a->th.wait == W_MOVIE) {
        an_movie_draw(a, a->fb);
        a->serial++;
        return;
    }
    draw_scene(a, a->fb);
    apply_transition(a);
    an_window_render(a);
    an_ui_render(a);
    an_screens_render(a);
    a->serial++;
}

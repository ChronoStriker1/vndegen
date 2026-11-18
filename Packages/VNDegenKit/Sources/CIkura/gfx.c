#include "ikura.h"

void gfx_init(IK *t) {
    t->screen = ik_alloc(sizeof(u32) * (size_t)t->W * (size_t)t->H);
    for (int i = 0; i < t->W * t->H; i++) t->screen[i] = 0xff000000u;
}

void gfx_close(IK *t) {
    free(t->screen);
    for (int i = 0; i < t->imageCount; i++) image_free(t->images[i]);
    free(t->images);
    free(t->layers);
}

static void step(Anim *a, u32 now, int *out, int n) {
    if (!a->on) return;
    u32 el = now - a->start;
    if (el >= a->dur) {
        for (int k = 0; k < n; k++) out[k] = a->to[k];
        a->on = false;
        return;
    }
    for (int k = 0; k < n; k++) out[k] = a->from[k] + (int)((i64)(a->to[k] - a->from[k]) * el / a->dur);
}

void gfx_update(IK *t) {
    for (int i = 0; i < t->layerCount; i++) {
        Layer *l = &t->layers[i];
        if (!l->made) continue;
        if (l->fade.on) {
            step(&l->fade, t->now, l->argb, 4);
            t->dirty = true;
        }
        if (l->move.on) {
            int xy[2] = {l->x, l->y};
            step(&l->move, t->now, xy, 2);
            l->x = xy[0];
            l->y = xy[1];
            t->dirty = true;
        }
    }
}

bool gfx_animating(IK *t) {
    for (int i = 0; i < t->layerCount; i++)
        if (t->layers[i].made && (t->layers[i].fade.on || t->layers[i].move.on)) return true;
    return false;
}

static void draw_layer(IK *t, const Layer *l) {
    if (l->img < 0 || l->img >= t->imageCount || !t->images[l->img] || l->argb[0] <= 0) return;
    const Image *im = t->images[l->img];
    u32 ca = (u32)clampi(l->argb[0], 0, 255), cr = (u32)clampi(l->argb[1], 0, 255), cg = (u32)clampi(l->argb[2], 0, 255),
        cb = (u32)clampi(l->argb[3], 0, 255);
    int w = l->w, h = l->h;
    if (l->sx + w > im->w) w = im->w - l->sx;
    if (l->sy + h > im->h) h = im->h - l->sy;
    for (int y = 0; y < h; y++) {
        int dy = l->y + y, syy = l->sy + y;
        if (dy < 0 || dy >= t->H || syy < 0) continue;
        const u32 *srow = im->px + (size_t)syy * im->w;
        u32 *drow = t->screen + (size_t)dy * t->W;
        for (int x = 0; x < w; x++) {
            int dx = l->x + x, sxx = l->sx + x;
            if (dx < 0 || dx >= t->W || sxx < 0) continue;
            u32 s = srow[sxx];
            u32 a = (s >> 24) * ca / 255;
            if (!a) continue;
            u32 r = (s & 0xff) * cr / 255, g = ((s >> 8) & 0xff) * cg / 255, b = ((s >> 16) & 0xff) * cb / 255;
            u32 d = drow[dx], ia = 255 - a;
            drow[dx] = ((r * a + (d & 0xff) * ia) / 255) | (((g * a + ((d >> 8) & 0xff) * ia) / 255) << 8) |
                       (((b * a + ((d >> 16) & 0xff) * ia) / 255) << 16) | 0xff000000u;
        }
    }
}

void gfx_compose(IK *t) {
    for (int i = 0; i < t->W * t->H; i++) t->screen[i] = 0xff000000u;
    for (int i = 0; i < t->layerCount; i++)
        if (t->layers[i].made) draw_layer(t, &t->layers[i]);
    t->serial++;
}

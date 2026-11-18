#include "ikura.h"

void gfx_init(IK *t) {
    t->buttons = ik_alloc(sizeof(Button) * IK_BUTTONS);
    t->hover = t->pressed = -1;
    t->screen = ik_alloc(sizeof(u32) * (size_t)t->W * (size_t)t->H);
    for (int i = 0; i < t->W * t->H; i++) t->screen[i] = 0xff000000u;
}

void gfx_close(IK *t) {
    free(t->screen);
    for (int i = 0; i < t->imageCount; i++) image_free(t->images[i]);
    free(t->images);
    free(t->layers);
    free(t->buttons);
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

static const int fullColour[4] = {255, 255, 255, 255};

void gfx_blit_min(IK *t, const int argb[4], int img, int sx, int sy, int w, int h, int x0, int y0, u32 minAlpha) {
    if (img < 0 || img >= t->imageCount || !t->images[img] || argb[0] <= 0) return;
    const Image *im = t->images[img];
    u32 ca = (u32)clampi(argb[0], 0, 255), cr = (u32)clampi(argb[1], 0, 255), cg = (u32)clampi(argb[2], 0, 255),
        cb = (u32)clampi(argb[3], 0, 255);
    if (sx + w > im->w) w = im->w - sx;
    if (sy + h > im->h) h = im->h - sy;
    for (int y = 0; y < h; y++) {
        int dy = y0 + y, syy = sy + y;
        if (dy < 0 || dy >= t->H || syy < 0) continue;
        const u32 *srow = im->px + (size_t)syy * im->w;
        u32 *drow = t->screen + (size_t)dy * t->W;
        for (int x = 0; x < w; x++) {
            int dx = x0 + x, sxx = sx + x;
            if (dx < 0 || dx >= t->W || sxx < 0) continue;
            u32 s = srow[sxx];
            if ((s >> 24) < minAlpha) continue;
            u32 a = (s >> 24) * ca / 255;
            if (!a) continue;
            u32 r = (s & 0xff) * cr / 255, g = ((s >> 8) & 0xff) * cg / 255, b = ((s >> 16) & 0xff) * cb / 255;
            u32 d = drow[dx], ia = 255 - a;
            drow[dx] = ((r * a + (d & 0xff) * ia) / 255) | (((g * a + ((d >> 8) & 0xff) * ia) / 255) << 8) |
                       (((b * a + ((d >> 16) & 0xff) * ia) / 255) << 16) | 0xff000000u;
        }
    }
}

void gfx_blit(IK *t, const int argb[4], int img, int sx, int sy, int w, int h, int x0, int y0) {
    gfx_blit_min(t, argb, img, sx, sy, w, h, x0, y0, 0);
}

static void fill(IK *t, const Layer *l) {
    u32 a = (u32)clampi(l->argb[0], 0, 255);
    if (!a) return;
    u32 r = (u32)clampi(l->argb[1], 0, 255), g = (u32)clampi(l->argb[2], 0, 255), b = (u32)clampi(l->argb[3], 0, 255);
    int x0 = clampi(l->x, 0, t->W), y0 = clampi(l->y, 0, t->H), x1 = clampi(l->x + l->w, 0, t->W), y1 = clampi(l->y + l->h, 0, t->H);
    for (int y = y0; y < y1; y++) {
        u32 *row = t->screen + (size_t)y * t->W;
        for (int x = x0; x < x1; x++) {
            u32 d = row[x], ia = 255 - a;
            row[x] = ((r * a + (d & 0xff) * ia) / 255) | (((g * a + ((d >> 8) & 0xff) * ia) / 255) << 8) |
                     (((b * a + ((d >> 16) & 0xff) * ia) / 255) << 16) | 0xff000000u;
        }
    }
}

static int button_state(IK *t, int id, const Button *b) {
    if (!b->enabled) return 3;
    if (id == t->pressed && id == t->hover) return 2;
    if (id == t->hover) return 1;
    return 0;
}

void gfx_compose(IK *t) {
    for (int i = 0; i < t->W * t->H; i++) t->screen[i] = 0xff000000u;
    for (int i = 0; i < t->layerCount; i++) {
        Layer *l = &t->layers[i];
        if (!l->made) continue;
        if (l->img >= 0) gfx_blit(t, l->argb, l->img, l->sx, l->sy, l->w, l->h, l->x, l->y);
        else fill(t, l);
        if (l->mode) {
            for (int k = 0; k < IK_BUTTONS; k++) {
                Button *b = &t->buttons[k];
                if (!b->made || !b->shown || b->layer != i) continue;
                ButtonState *s = &b->state[button_state(t, k, b)];
                gfx_blit(t, fullColour, s->img, s->sx, s->sy, b->w, b->h, l->x + b->x, l->y + b->y);
            }
        }
        msg_draw(t, i);
    }
    t->serial++;
}

int button_at(IK *t, int x, int y) {
    for (int k = IK_BUTTONS - 1; k >= 0; k--) {
        Button *b = &t->buttons[k];
        if (!b->made || !b->shown || b->layer < 0 || b->layer >= t->layerCount) continue;
        Layer *l = &t->layers[b->layer];
        if (!l->made || !l->mode) continue;
        int bx = l->x + b->x, by = l->y + b->y;
        if (x >= bx && y >= by && x < bx + b->w && y < by + b->h) return k;
    }
    return -1;
}

void buttons_input(IK *t) {
    int h = button_at(t, t->mx, t->my);
    if (h != t->hover) {
        t->hover = h;
        t->dirty = true;
    }
    if (t->lDown && t->pressed < 0 && h >= 0) t->pressed = h;
    if (!t->lDown && t->pressed >= 0) {
        t->dirty = true;
        t->pressed = -1;
    }
    if (t->wait != W_INPUT && t->wait != W_MSG) return;
    if (t->click && h >= 0 && t->buttons[h].enabled && t->buttons[h].bound) {
        Button *b = &t->buttons[h];
        t->click = false;
        ik_log(t, "button %d chose label %d", h, b->label);
        vm_branch(t, b->sc, b->label, b->call);
    } else if (t->rclick && t->cancelBound) {
        t->rclick = false;
        vm_branch(t, t->cancelSc, t->cancelLabel, t->cancelCall);
    }
}

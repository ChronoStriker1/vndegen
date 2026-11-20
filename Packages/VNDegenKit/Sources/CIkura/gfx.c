#include "ikura.h"

#include <math.h>

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
    for (int i = 0; i < t->layerCount; i++) image_free(t->layers[i].surf);
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

void layer_move(IK *t, Layer *l, int ms, int x, int y) {
    l->move.from[0] = l->x;
    l->move.from[1] = l->y;
    l->move.to[0] = x;
    l->move.to[1] = y;
    l->move.start = t->now;
    l->move.dur = (u32)(ms > 0 ? ms : 0);
    l->move.on = ms > 0;
    if (ms <= 0) {
        l->x = x;
        l->y = y;
    }
}

bool gfx_animating(IK *t) {
    for (int i = 0; i < t->layerCount; i++)
        if (t->layers[i].made && (t->layers[i].fade.on || t->layers[i].move.on)) return true;
    return false;
}

static const int fullColour[4] = {255, 255, 255, 255};

static inline u32 over(u32 d, u32 r, u32 g, u32 b, u32 a) {
    u32 da = d >> 24;
    if (a == 255 || !da) return r | g << 8 | b << 16 | a << 24;
    u32 ia = 255 - a, w = da * ia / 255, oa = a + w;
    return (r * a + (d & 0xff) * w) / oa | ((g * a + ((d >> 8) & 0xff) * w) / oa) << 8 |
           ((b * a + ((d >> 16) & 0xff) * w) / oa) << 16 | oa << 24;
}

void image_blit(Image *dst, const Image *src, int sx, int sy, int w, int h, int x0, int y0, const int argb[4], u32 minAlpha,
                bool copy) {
    if (!dst || !src) return;
    u32 ca = (u32)clampi(argb[0], 0, 255), cr = (u32)clampi(argb[1], 0, 255), cg = (u32)clampi(argb[2], 0, 255),
        cb = (u32)clampi(argb[3], 0, 255);
    if (!copy && !ca) return;
    if (sx + w > src->w) w = src->w - sx;
    if (sy + h > src->h) h = src->h - sy;
    for (int y = 0; y < h; y++) {
        int dy = y0 + y, syy = sy + y;
        if (dy < 0 || dy >= dst->h || syy < 0) continue;
        const u32 *srow = src->px + (size_t)syy * src->w;
        u32 *drow = dst->px + (size_t)dy * dst->w;
        for (int x = 0; x < w; x++) {
            int dx = x0 + x, sxx = sx + x;
            if (dx < 0 || dx >= dst->w || sxx < 0) continue;
            u32 p = srow[sxx];
            if (copy) {
                drow[dx] = p;
                continue;
            }
            if ((p >> 24) < minAlpha) continue;
            u32 a = (p >> 24) * ca / 255;
            if (!a) continue;
            drow[dx] = over(drow[dx], (p & 0xff) * cr / 255, ((p >> 8) & 0xff) * cg / 255, ((p >> 16) & 0xff) * cb / 255, a);
        }
    }
}

void gfx_blit_min(IK *t, const int argb[4], int img, int sx, int sy, int w, int h, int x0, int y0, u32 minAlpha) {
    if (img < 0 || img >= t->imageCount || !t->images[img]) return;
    Image screen = {.w = t->W, .h = t->H, .px = t->screen};
    image_blit(&screen, t->images[img], sx, sy, w, h, x0, y0, argb, minAlpha, false);
}

Image *layer_surface(IK *t, Layer *l) {
    if (l->surf) return l->surf;
    l->surf = image_new(l->w, l->h);
    memset(l->surf->px, 0, sizeof(u32) * (size_t)l->w * (size_t)l->h);
    if (l->img >= 0 && l->img < t->imageCount && t->images[l->img])
        image_blit(l->surf, t->images[l->img], l->sx, l->sy, l->w, l->h, 0, 0, fullColour, 0, true);
    return l->surf;
}

void layer_restore(IK *t, Layer *l, int x, int y, int w, int h) {
    Image *s = layer_surface(t, l);
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= s->h) continue;
        for (int i = x; i < x + w; i++)
            if (i >= 0 && i < s->w) s->px[(size_t)j * s->w + i] = 0;
    }
    if (l->img >= 0 && l->img < t->imageCount && t->images[l->img])
        image_blit(s, t->images[l->img], l->sx + x, l->sy + y, w, h, x, y, fullColour, 0, true);
}

void gfx_blit(IK *t, const int argb[4], int img, int sx, int sy, int w, int h, int x0, int y0) {
    gfx_blit_min(t, argb, img, sx, sy, w, h, x0, y0, 0);
}

static void rotated(IK *t, const Layer *l) {
    if (l->img >= t->imageCount || !t->images[l->img]) return;
    const Image *im = t->images[l->img];
    float c = cosf(l->rot), s = sinf(l->rot), hx = (float)l->w / 2, hy = (float)l->h / 2;
    u32 ca = (u32)clampi(l->argb[0], 0, 255);
    for (int y = 0; y < l->h; y++) {
        int dy = l->y + y;
        if (dy < 0 || dy >= t->H) continue;
        for (int x = 0; x < l->w; x++) {
            int dx = l->x + x;
            if (dx < 0 || dx >= t->W) continue;
            float ox = (float)x + 0.5f - hx, oy = (float)y + 0.5f - hy;
            int sx = (int)floorf(ox * c + oy * s + hx), sy = (int)floorf(-ox * s + oy * c + hy);
            if (sx < 0 || sy < 0 || sx >= l->w || sy >= l->h) continue;
            sx += l->sx;
            sy += l->sy;
            if (sx < 0 || sy < 0 || sx >= im->w || sy >= im->h) continue;
            u32 p = im->px[(size_t)sy * im->w + sx], a = (p >> 24) * ca / 255;
            if (!a) continue;
            u32 *o = &t->screen[(size_t)dy * t->W + dx], d = *o, ia = 255 - a;
            *o = (((p & 0xff) * a + (d & 0xff) * ia) / 255) | ((((p >> 8) & 0xff) * a + ((d >> 8) & 0xff) * ia) / 255) << 8 |
                 ((((p >> 16) & 0xff) * a + ((d >> 16) & 0xff) * ia) / 255) << 16 | 0xff000000u;
        }
    }
}

static void fill(IK *t, const Layer *l) {
    u32 a = (u32)clampi(l->argb[0], 0, 255) * (l->fillArgb >> 24) / 255;
    if (!a) return;
    u32 r = (u32)clampi(l->argb[1], 0, 255) * (l->fillArgb >> 16 & 0xff) / 255,
        g = (u32)clampi(l->argb[2], 0, 255) * (l->fillArgb >> 8 & 0xff) / 255,
        b = (u32)clampi(l->argb[3], 0, 255) * (l->fillArgb & 0xff) / 255;
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

static void scroll_thumb(const Button *b, int *pos, int *len) {
    int range = b->max + b->scroll.page;
    *len = range > 0 ? b->h * b->scroll.page / range : b->h;
    if (*len < 24) *len = 24;
    if (*len > b->h) *len = b->h;
    *pos = b->max > 0 ? (b->h - *len) * clampi(b->value, 0, b->max) / b->max : 0;
}

static void draw_scrollbar(IK *t, int id, const Button *b, const Layer *l) {
    const Scroll *s = &b->scroll;
    int img = s->img;
    if (img < 0 || img >= t->imageCount || !t->images[img] || b->max <= 0) return;
    const Image *im = t->images[img];
    int st = id == t->pressed ? 2 : id == t->hover ? 1 : 0;
    const Cell *top = s->cells[st] >= 0 && s->cells[st] < im->cellCount ? &im->cells[s->cells[st]] : NULL;
    const Cell *bot = s->cells[3 + st] >= 0 && s->cells[3 + st] < im->cellCount ? &im->cells[s->cells[3 + st]] : NULL;
    const Cell *mid = s->cells[6 + st] >= 0 && s->cells[6 + st] < im->cellCount ? &im->cells[s->cells[6 + st]] : NULL;
    if (!top || !bot || !mid || mid->h <= 0) return;
    int pos, len;
    scroll_thumb(b, &pos, &len);
    int x = l->x + b->x, y = l->y + b->y + pos;
    gfx_blit(t, fullColour, img, top->sx, top->sy, top->w, top->h, x, y);
    for (int yy = top->h; yy < len - bot->h; yy += mid->h) {
        int h = len - bot->h - yy < mid->h ? len - bot->h - yy : mid->h;
        gfx_blit(t, fullColour, img, mid->sx, mid->sy, mid->w, h, x, y + yy);
    }
    gfx_blit(t, fullColour, img, bot->sx, bot->sy, bot->w, bot->h, x, y + len - bot->h);
}

static int scroll_value(IK *t, const Button *b) {
    int pos, len;
    scroll_thumb(b, &pos, &len);
    const Layer *l = &t->layers[b->layer];
    int travel = b->h - len;
    if (travel <= 0) return 0;
    return clampi(((t->my - l->y - b->y - len / 2) * b->max + travel / 2) / travel, 0, b->max);
}

void scrollbar_exec(IK *t, u16 op, const u8 *a, int n) {
    u32 id = ik_value(t, rd32(a));
    if (id >= IK_BUTTONS) return;
    Scroll *s = &t->buttons[id].scroll;
    if (op == 0x0712 && n >= 8) s->img = (int)ik_value(t, rd32(a + 4));
    else if (op == 0x0714 && n >= 8) s->page = (int)ik_value(t, rd32(a + 4));
    else if (op == 0x071e && n >= 44) {
        s->img = (int)ik_value(t, rd32(a + 4));
        for (int k = 0; k < 9; k++) s->cells[k] = (int)ik_value(t, rd32(a + 8 + 4 * k));
    }
    t->dirty = true;
}

static int button_state(IK *t, int id, const Button *b) {
    if (!b->enabled) return 3;
    if (b->mode == 2) return 2;
    if (id == t->pressed && id == t->hover) return 2;
    if (id == t->hover) return 1;
    return 0;
}

bool layer_drawn(const IK *t, int i) {
    const Layer *l = &t->layers[i];
    return l->made && l->ready && l->shown;
}

bool buttons_live(const IK *t) { return t->wait == W_INPUT || t->wait == W_MSG; }

void gfx_compose(IK *t) {
    for (int i = 0; i < t->W * t->H; i++) t->screen[i] = 0xff000000u;
    dun_render(t);
    bool live = buttons_live(t);
    for (int i = 0; i < t->layerCount; i++) {
        Layer *l = &t->layers[i];
        if (!layer_drawn(t, i)) continue;
        if (gan_drawn(t, i)) gan_draw(t, i);
        else if (l->rot != 0 && !l->surf && l->img >= 0) rotated(t, l);
        else if (l->surf) {
            Image screen = {.w = t->W, .h = t->H, .px = t->screen};
            image_blit(&screen, l->surf, 0, 0, l->w, l->h, l->x, l->y, l->argb, 0, false);
        } else if (l->img >= 0) gfx_blit(t, l->argb, l->img, l->sx, l->sy, l->w, l->h, l->x, l->y);
        else if (l->fill) fill(t, l);
        if (live) {
            for (int k = 0; k < IK_BUTTONS; k++) {
                Button *b = &t->buttons[k];
                if (!b->made || !b->shown || b->layer != i) continue;
                if (b->type == 7) {
                    draw_scrollbar(t, k, b, l);
                    continue;
                }
                if (b->type == 4) {
                    ButtonState *bg = &b->state[b->enabled ? 0 : 3], *fg = &b->state[k == t->hover ? 2 : 1];
                    gfx_blit(t, fullColour, bg->img, bg->sx, bg->sy, b->w, b->h, l->x + b->x, l->y + b->y);
                    int fw = b->max > 0 ? b->w * clampi(b->value, 0, b->max) / b->max : 0;
                    if (b->enabled) gfx_blit(t, fullColour, fg->img, fg->sx, fg->sy, fw, b->h, l->x + b->x, l->y + b->y);
                    continue;
                }
                ButtonState *s = &b->state[button_state(t, k, b)];
                gfx_blit(t, fullColour, s->img, s->sx, s->sy, b->w, b->h, l->x + b->x, l->y + b->y);
            }
        }
        if (live && t->hover >= 0) {
            ButtonTip *tip = &t->buttons[t->hover].tip;
            if (tip->on && tip->layer == i && t->buttons[t->hover].enabled)
                gfx_blit(t, fullColour, tip->img, tip->sx, tip->sy, tip->w, tip->h, l->x + tip->x, l->y + tip->y);
        }
        msg_draw(t, i);
    }
    movie_draw(t);
    t->serial++;
}

int button_at(IK *t, int x, int y) {
    for (int k = IK_BUTTONS - 1; k >= 0; k--) {
        Button *b = &t->buttons[k];
        if (!b->made || !b->shown || b->layer < 0 || b->layer >= t->layerCount) continue;
        if (!layer_drawn(t, b->layer)) continue;
        Layer *l = &t->layers[b->layer];
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
    if (t->lDown && t->pressed >= 0 && t->buttons[t->pressed].type == 7 && t->buttons[t->pressed].bound) {
        Button *b = &t->buttons[t->pressed];
        int v = scroll_value(t, b);
        if (v != b->value) {
            b->value = v;
            if (b->var >= 0 && b->var < t->varCount) t->vars[b->var] = v;
            vm_branch(t, b->sc, b->label, b->call);
            return;
        }
    }
    if (t->click && h >= 0 && t->buttons[h].enabled && t->buttons[h].bound) {
        Button *b = &t->buttons[h];
        t->click = false;
        if (b->type == 3) {
            int on = b->mode != 2;
            b->mode = on ? 2 : 1;
            if (b->var >= 0 && b->var < t->varCount) t->vars[b->var] = on;
            t->lastToggle = h;
        }
        if (b->type == 7) {
            b->value = scroll_value(t, b);
            if (b->var >= 0 && b->var < t->varCount) t->vars[b->var] = b->value;
        }
        if (b->type == 4 && b->w > 0) {
            Layer *bl = &t->layers[b->layer];
            b->value = clampi((t->mx - bl->x - b->x) * b->max / b->w, 0, b->max);
            if (b->var >= 0 && b->var < t->varCount) t->vars[b->var] = b->value;
        }
        ik_log(t, "button %d chose label %d", h, b->label);
        vm_branch(t, b->sc, b->label, b->call);
    } else if (t->rclick && t->cancelBound) {
        t->rclick = false;
        vm_branch(t, t->cancelSc, t->cancelLabel, t->cancelCall);
    } else {
        for (int k = 0; k < 256; k++) {
            if (!t->keyPress[k] || !t->keyButton[k]) continue;
            int id = t->keyButton[k] - 1;
            if (id == -2 && t->cancelBound) {
                t->keyPress[k] = false;
                vm_branch(t, t->cancelSc, t->cancelLabel, t->cancelCall);
                break;
            }
            if (id >= 0 && id < IK_BUTTONS && t->buttons[id].made && t->buttons[id].enabled && t->buttons[id].bound) {
                t->keyPress[k] = false;
                vm_branch(t, t->buttons[id].sc, t->buttons[id].label, t->buttons[id].call);
                break;
            }
        }
    }
}

#include "pj.h"

static bool referenced(PJ *p, const Image *im) {
    for (int i = 0; i < PJ_LAYERS + PJ_SUBS; i++)
        if (p->layer[i].used && p->layer[i].img == im) return true;
    for (int i = 0; i < PJ_BUTTONS; i++)
        if (p->btn[i].used && p->btn[i].spr.img == im) return true;
    for (int i = 0; i < PJ_SLIDERS; i++)
        if (p->slider[i].used && p->slider[i].knob.img == im) return true;
    if (p->msgFrame.used && p->msgFrame.img == im) return true;
    return p->game && pj_game_uses(p->game, im);
}

Image *pj_image(PJ *p, const char *name) {
    int n = (int)(sizeof p->cache / sizeof p->cache[0]);
    for (int i = 0; i < n; i++)
        if (p->cache[i].img && pj_ieq(p->cache[i].name, name)) {
            p->cache[i].used = ++p->cacheTick;
            return p->cache[i].img;
        }
    size_t size;
    u8 *data = pj_pac_read(&p->pac, name, &size);
    if (!data) {
        static const char *const exts[] = {".EPA", NULL};
        char found[40];
        if (pj_pac_find_ext(&p->pac, name, exts, found, sizeof found)) data = pj_pac_read(&p->pac, found, &size);
    }
    if (!data) {
        pj_log(p, "missing picture %s", name);
        return NULL;
    }
    Image *im = pj_epa_decode(data, size);
    free(data);
    if (!im) {
        pj_log(p, "can't decode picture %s", name);
        return NULL;
    }
    size_t budget = (size_t)160 << 20, bytes = (size_t)im->w * (size_t)im->h * 4;
    for (int i = 0; i < n; i++)
        if (p->cache[i].img) bytes += (size_t)p->cache[i].img->w * (size_t)p->cache[i].img->h * 4;
    while (bytes > budget) {
        int victim = -1;
        u32 best = ~0u;
        for (int i = 0; i < n; i++)
            if (p->cache[i].img && p->cache[i].used < best && !referenced(p, p->cache[i].img)) {
                best = p->cache[i].used;
                victim = i;
            }
        if (victim < 0) break;
        bytes -= (size_t)p->cache[victim].img->w * (size_t)p->cache[victim].img->h * 4;
        pj_image_free(p->cache[victim].img);
        p->cache[victim].img = NULL;
    }
    int slot = -1;
    for (int i = 0; i < n && slot < 0; i++)
        if (!p->cache[i].img) slot = i;
    if (slot < 0) {
        pj_log(p, "picture cache full, not caching %s", name);
        return im;
    }
    snprintf(p->cache[slot].name, sizeof p->cache[slot].name, "%s", name);
    p->cache[slot].img = im;
    p->cache[slot].used = ++p->cacheTick;
    return im;
}

Layer *pj_layer(PJ *p, int id) {
    if (id >= 0 && id < PJ_LAYERS) return &p->layer[id];
    if (id >= PJ_SUB_BASE && id < PJ_SUB_BASE + PJ_SUBS) return &p->layer[PJ_LAYERS + id - PJ_SUB_BASE];
    return NULL;
}

void pj_sprite_free(Layer *l) {
    for (int i = 0; i < l->anmCount; i++) pj_anm_free(&l->anm[i]);
    memset(l, 0, sizeof *l);
}

void pj_track_start(Layer *l, int t, u32 pc, u32 end) {
    Track *tr = &l->tr[t];
    memset(tr, 0, sizeof *tr);
    tr->live = true;
    tr->pc = pc;
    tr->end = end;
    tr->op = -1;
}

void pj_anm1_start(Layer *l, const Anm *a) {
    for (int i = 0; i < PJ_TRACKS; i++) l->tr[i].live = false;
    pj_track_start(l, 0, a->start, a->end);
    l->animating = true;
}

static const char *lang_src(PJ *p, const Def *d) {
    int i = p->lang == VNPJADV_LANG_JAPANESE ? 1 : p->lang == VNPJADV_LANG_CHINESE ? 2 : 0;
    if (d->src[i][0]) return d->src[i];
    for (int k = 0; k < 3; k++)
        if (d->src[k][0]) return d->src[k];
    return NULL;
}

bool pj_sprite_load(PJ *p, Layer *l, const char *name) {
    pj_sprite_free(l);
    snprintf(l->name, sizeof l->name, "%s", name);
    l->used = true;
    l->shown = true;
    l->alpha = 255;
    l->scale = 100;
    l->btnState = -1;
    char defName[48];
    snprintf(defName, sizeof defName, "%s.def", name);
    size_t size;
    u8 *data = pj_pac_read(&p->pac, defName, &size);
    if (data) {
        l->hasDef = pj_def_parse(&l->def, name, data, size);
        free(data);
    }
    if (l->hasDef) {
        Def *d = &l->def;
        const char *src = lang_src(p, d);
        l->img = src ? pj_image(p, src) : NULL;
        l->x = d->x;
        l->y = d->y;
        l->alpha = clampi(d->alpha, 0, 255);
        l->scale = d->scale;
        if (!d->show) l->shown = false;
        if (d->hasRect) {
            l->sx = d->rx;
            l->sy = d->ry;
            l->sw = d->rw;
            l->sh = d->rh;
        } else if (l->img) {
            l->sw = l->img->w;
            l->sh = l->img->h;
        } else {
            l->sw = d->xsize;
            l->sh = d->ysize;
        }
        for (int i = 0; i < d->animeCount && l->anmCount < 2; i++) {
            u8 *ad = pj_pac_read(&p->pac, d->anime[i], &size);
            if (!ad) continue;
            if (pj_anm_parse(&l->anm[l->anmCount], ad, size)) {
                if (l->anm[l->anmCount].kind == ANM_ONE) pj_anm1_start(l, &l->anm[l->anmCount]);
                l->anmCount++;
            } else
                free(ad);
        }
        for (int i = 0; i < l->anmCount; i++)
            if (l->anm[i].kind == ANM_TWO && l->anm[i].type == 0) {
                pj_layer_set_state(p, l, 0);
                break;
            }
        return true;
    }
    l->img = pj_image(p, name);
    if (!l->img) {
        l->used = false;
        return false;
    }
    l->sw = l->img->w;
    l->sh = l->img->h;
    return true;
}

bool pj_layer_load(PJ *p, int id, const char *name) {
    Layer *l = pj_layer(p, id);
    if (!l) return false;
    bool ok = pj_sprite_load(p, l, name);
    l->id = id;
    l->z = ++p->zNext;
    l->parent = -1;
    if (!ok) pj_log(p, "layer %d: can't load %s", id, name);
    return ok;
}

void pj_layer_clear(PJ *p, int id) {
    Layer *l = pj_layer(p, id);
    if (l) pj_sprite_free(l);
}

void pj_layer_set_state(PJ *p, Layer *l, int state) {
    if (state < 0 || state > 5 || l->btnState == state) return;
    l->btnState = state;
    for (int i = 0; i < l->anmCount; i++) {
        Anm *a = &l->anm[i];
        if (a->kind != ANM_TWO) continue;
        u32 pc = a->state[state];
        while (pc + 4 <= a->size) {
            i32 op = (i32)rd32(a->data + pc);
            if (op == 0) break;
            if (op == 0x1a && pc + 12 <= a->size) {
                l->sx = (i32)rd32(a->data + pc + 4);
                l->sy = (i32)rd32(a->data + pc + 8);
                pc += 12;
            } else if (op == 0x10) {
                if (a->sound[0]) {
                    const char *s = a->sound;
                    while (*s == ' ') s++;
                    pj_play(p, CH_SE, 15, s, false);
                }
                pc += 8;
            } else {
                pj_log(p, "%s: ANM2 op %x", l->name, op);
                break;
            }
        }
    }
}

static void tween_value(int *cur, int *done, int total, int t, int dur) {
    int want = dur <= 0 || t >= dur ? total : (int)((i64)total * t / dur);
    *cur += want - *done;
    *done = want;
}

void pj_track_tick(Layer *l, const Anm *a, int ti, int ms, u32 *rng) {
    Track *tr = &l->tr[ti];
    int guard = 0;
    while (tr->live && guard++ < 256) {
        if (tr->op >= 0) {
            int step = ms;
            if (tr->t + step > tr->dur) step = tr->dur - tr->t;
            tr->t += step;
            ms -= step;
            if (tr->op == 7) {
                tween_value(&l->ox, &tr->done0, tr->a0, tr->t, tr->dur);
                tween_value(&l->oy, &tr->done1, tr->a1, tr->t, tr->dur);
            } else if (tr->op == 8)
                tween_value(&l->scale, &tr->done0, tr->a0, tr->t, tr->dur);
            else if (tr->op == 0xc) {
                tween_value(&l->alpha, &tr->done0, tr->a0, tr->t, tr->dur);
            }
            if (tr->t < tr->dur) return;
            tr->op = -1;
            if (ms <= 0 && tr->dur > 0) {
                if (tr->pc >= tr->end) tr->live = false;
                return;
            }
        }
        if (tr->pc + 4 > a->size || tr->pc >= tr->end) {
            tr->live = false;
            return;
        }
        const u8 *d = a->data;
        u32 pc = tr->pc;
        i32 op = (i32)rd32(d + pc);
        i32 x = pc + 8 <= a->size ? (i32)rd32(d + pc + 4) : 0;
        i32 y = pc + 12 <= a->size ? (i32)rd32(d + pc + 8) : 0;
        i32 z = pc + 16 <= a->size ? (i32)rd32(d + pc + 12) : 0;
        switch (op) {
        case 0:
        case 1:
            tr->live = false;
            return;
        case 2:
            tr->pc = a->start + (u32)x;
            break;
        case 4:
            tr->pc = pc + 12;
            *rng = *rng * 1103515245u + 12345u;
            if ((int)((*rng >> 16) % 100) < x) tr->pc = a->start + (u32)y;
            break;
        case 5:
            tr->pc = pc + 8;
            tr->op = 5;
            tr->t = 0;
            tr->dur = x;
            break;
        case 7:
            tr->pc = pc + 16;
            tr->op = 7;
            tr->t = 0;
            tr->dur = z;
            tr->a0 = x;
            tr->a1 = y;
            tr->done0 = tr->done1 = 0;
            break;
        case 8:
        case 0xc:
            tr->pc = pc + 12;
            tr->op = op;
            tr->t = 0;
            tr->dur = y;
            tr->a0 = x;
            tr->done0 = 0;
            break;
        case 0x1a:
            tr->pc = pc + 12;
            l->sx = x;
            l->sy = y;
            break;
        case 0x1d: {
            tr->pc = pc + (u32)x;
            for (int k = 0; k < PJ_TRACKS; k++)
                if (!l->tr[k].live) {
                    pj_track_start(l, k, pc + 8, pc + (u32)x);
                    break;
                }
            break;
        }
        default:
            tr->live = false;
            return;
        }
    }
}

bool pj_sprite_tick(PJ *p, Layer *l, int ms) {
    if (!l->used || !l->animating) return false;
    const Anm *a = NULL;
    for (int k = 0; k < l->anmCount; k++)
        if (l->anm[k].kind == ANM_ONE) a = &l->anm[k];
    bool any = false;
    if (a)
        for (int k = 0; k < PJ_TRACKS; k++) {
            if (l->tr[k].live) pj_track_tick(l, a, k, ms, &p->rng);
            any |= l->tr[k].live;
        }
    l->animating = any;
    return true;
}

void pj_layers_tick(PJ *p, int ms) {
    for (int i = 0; i < PJ_LAYERS + PJ_SUBS; i++)
        if (pj_sprite_tick(p, &p->layer[i], ms)) p->dirty = true;
    if (p->tweenCount && p->groupMs >= 0) {
        p->groupT += ms;
        if (p->groupT > p->groupMs) p->groupT = p->groupMs;
        for (int i = 0; i < p->tweenCount; i++) {
            Tween *t = &p->tween[i];
            Layer *l = pj_layer(p, t->layer);
            if (!l || !l->used) continue;
            if (t->kind == TW_ALPHA) tween_value(&l->alpha, &t->done0, t->d0, p->groupT, p->groupMs);
            else if (t->kind == TW_MOVE) {
                tween_value(&l->ox, &t->done0, t->d0, p->groupT, p->groupMs);
                tween_value(&l->oy, &t->done1, t->d1, p->groupT, p->groupMs);
            } else if (t->kind == TW_SCALE)
                tween_value(&l->scale, &t->done0, t->d0, p->groupT, p->groupMs);
        }
        p->dirty = true;
        if (p->groupT >= p->groupMs) {
            p->tweenCount = 0;
            p->groupMs = -1;
        }
    }
}

void pj_slider_place(PJ *p, Slider *s) {
    int range = s->args[14], lo = s->args[15], hi = s->args[16];
    int off = hi > lo ? (clampi(s->value, lo, hi) - lo) * range / (hi - lo) : 0;
    s->knob.x = s->args[8] + s->dx + off;
    s->knob.y = s->args[9] + s->dy;
    s->knob.ox = s->knob.oy = 0;
    p->dirty = true;
}

static inline u32 blend(u32 d, u32 s, u32 a) {
    if (a >= 255) return s | 0xff000000u;
    u32 ia = 255 - a;
    u32 rb = ((s & 0xff00ff) * a + (d & 0xff00ff) * ia) >> 8 & 0xff00ff;
    u32 g = ((s & 0x00ff00) * a + (d & 0x00ff00) * ia) >> 8 & 0x00ff00;
    return rb | g | 0xff000000u;
}

void pj_blit(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int alpha, int scale, int cx, int cy) {
    if (!im || alpha <= 0 || scale <= 0) return;
    if (alpha > 255) alpha = 255;
    if (sx < 0) {
        sw += sx;
        dx -= sx;
        sx = 0;
    }
    if (sy < 0) {
        sh += sy;
        dy -= sy;
        sy = 0;
    }
    if (sx + sw > im->w) sw = im->w - sx;
    if (sy + sh > im->h) sh = im->h - sy;
    if (sw <= 0 || sh <= 0) return;
    if (scale == 100) {
        for (int y = 0; y < sh; y++) {
            int ty = dy + y;
            if (ty < 0 || ty >= dh) continue;
            const u32 *s = im->px + (size_t)(sy + y) * (size_t)im->w + sx;
            u32 *d = dst + (size_t)ty * (size_t)dw;
            int x0 = dx < 0 ? -dx : 0, x1 = dx + sw > dw ? dw - dx : sw;
            for (int x = x0; x < x1; x++) {
                u32 c = s[x];
                u32 a = (c >> 24) * (u32)alpha / 255;
                if (a) d[dx + x] = blend(d[dx + x], c, a);
            }
        }
        return;
    }
    int ow = sw * scale / 100, oh = sh * scale / 100;
    int ox = dx + cx - cx * scale / 100, oy = dy + cy - cy * scale / 100;
    for (int y = 0; y < oh; y++) {
        int ty = oy + y;
        if (ty < 0 || ty >= dh) continue;
        int syy = sy + y * 100 / scale;
        if (syy >= sy + sh) continue;
        const u32 *s = im->px + (size_t)syy * (size_t)im->w;
        u32 *d = dst + (size_t)ty * (size_t)dw;
        for (int x = 0; x < ow; x++) {
            int tx = ox + x;
            if (tx < 0 || tx >= dw) continue;
            int sxx = sx + x * 100 / scale;
            if (sxx >= sx + sw) continue;
            u32 c = s[sxx];
            u32 a = (c >> 24) * (u32)alpha / 255;
            if (a) d[tx] = blend(d[tx], c, a);
        }
    }
}

static void draw_sprite(PJ *p, u32 *dst, const Layer *l, int px, int py) {
    if (!l->img) return;
    int x = l->x + l->ox + px, y = l->y + l->oy + py;
    pj_blit(dst, PJ_W, PJ_H, l->img, l->sx, l->sy, l->sw, l->sh, x, y, l->alpha, l->scale, l->hasDef ? l->def.cx : 0,
            l->hasDef ? l->def.cy : 0);
    (void)p;
}

static int zcmp(const void *a, const void *b) {
    const Layer *x = *(const Layer *const *)a, *y = *(const Layer *const *)b;
    return x->z < y->z ? -1 : x->z > y->z;
}

void pj_compose(PJ *p, u32 *dst) {
    for (size_t i = 0; i < (size_t)PJ_W * PJ_H; i++) dst[i] = 0xff000000u;
    const Layer *list[PJ_LAYERS + PJ_SUBS + PJ_BUTTONS + PJ_SLIDERS];
    int n = 0;
    for (int i = 0; i < PJ_LAYERS + PJ_SUBS; i++)
        if (p->layer[i].used && p->layer[i].shown && p->layer[i].parent < 0) list[n++] = &p->layer[i];
    for (int i = 0; i < PJ_BUTTONS; i++)
        if (p->btn[i].used && p->btn[i].spr.used && p->btn[i].spr.shown) list[n++] = &p->btn[i].spr;
    for (int i = 0; i < PJ_SLIDERS; i++)
        if (p->slider[i].used && p->slider[i].shown && p->slider[i].knob.used) list[n++] = &p->slider[i].knob;
    qsort(list, (size_t)n, sizeof list[0], zcmp);
    for (int i = 0; i < n; i++) {
        const Layer *l = list[i];
        draw_sprite(p, dst, l, 0, 0);
        for (int k = 0; k < PJ_LAYERS + PJ_SUBS && l->id >= 0; k++) {
            const Layer *c = &p->layer[k];
            if (c->used && c->shown && c->parent == l->id && c != l) draw_sprite(p, dst, c, l->x + l->ox, l->y + l->oy);
        }
    }
    pj_msg_draw(p, dst);
    if (p->game) pj_game_draw(p, p->game, dst);
}

void pj_freeze(PJ *p) {
    if (p->frozen) return;
    memcpy(p->from, p->shown, (size_t)PJ_W * PJ_H * 4);
    p->frozen = true;
    p->transMs = 0;
}

void pj_commit(PJ *p, int ms) {
    if (!p->frozen) {
        p->dirty = true;
        return;
    }
    p->frozen = false;
    p->transMs = ms > 0 ? ms : 0;
    p->transT = 0;
    p->dirty = true;
}

void pj_frame_tick(PJ *p, int ms) {
    if (p->transMs) {
        p->transT += ms;
        if (p->transT >= p->transMs) p->transMs = 0;
        p->dirty = true;
    }
    if (!p->dirty || p->frozen) return;
    p->dirty = false;
    pj_compose(p, p->screen);
    if (p->transMs) {
        u32 a = (u32)(p->transT * 255 / p->transMs);
        for (size_t i = 0; i < (size_t)PJ_W * PJ_H; i++) p->shown[i] = blend(p->from[i], p->screen[i], a);
    } else
        memcpy(p->shown, p->screen, (size_t)PJ_W * PJ_H * 4);
    p->serial++;
}

#include "ex.h"

static float ease(float t, int kind) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    switch (kind) {
    case 1: return t * t;
    case 2:
    case 5: return 1 - (1 - t) * (1 - t);
    case 3:
    case 4: return t * t * (3 - 2 * t);
    default: return t;
    }
}

void anim_start(EX *e, Anim *a, float from, float to, int ms, int kind) {
    a->from = from;
    a->to = to;
    a->start = e->now;
    a->dur = (u32)(ms > 0 ? ms : 0);
    a->ease = kind;
    a->active = ms > 0;
}

float anim_value(EX *e, Anim *a, float cur) {
    if (!a->active) return cur;
    u32 t = e->now - a->start;
    if (t >= a->dur) {
        a->active = false;
        return a->to;
    }
    return a->from + (a->to - a->from) * ease((float)t / (float)a->dur, a->ease);
}

static Image *cg_image(EX *e, int id) {
    char rel[64];
    snprintf(rel, sizeof rel, "res/g/ev/%02d/%04d.gyu", id / 100, id);
    return image_load(e, rel);
}

static u32 bgr(u32 c) { return (c >> 16 & 0xff) | (c & 0xff00) | (c & 0xff) << 16; }

static void cam_reset(Camera *c) {
    memset(c, 0, sizeof *c);
    c->w = EX_W;
    c->h = EX_H;
}

static void cam_update(EX *e, Camera *c) {
    if (c->ax.active) c->x = anim_value(e, &c->ax, c->x);
    if (c->ay.active) c->y = anim_value(e, &c->ay, c->y);
    if (c->aw.active) c->w = anim_value(e, &c->aw, c->w);
    if (c->ah.active) c->h = anim_value(e, &c->ah, c->h);
}

static bool cam_busy(Camera *c) { return c->ax.active || c->ay.active || c->aw.active || c->ah.active; }

void gfx_camera(EX *e, float x, float y, float w, float h, int ms, int kind) {
    Camera *c = &e->cam;
    if (c->w <= 0) cam_reset(c);
    if (ms <= 0) {
        cam_reset(c);
        c->x = x;
        c->y = y;
        c->w = w;
        c->h = h;
        return;
    }
    anim_start(e, &c->ax, c->x, x, ms, kind);
    anim_start(e, &c->ay, c->y, y, ms, kind);
    anim_start(e, &c->aw, c->w, w, ms, kind);
    anim_start(e, &c->ah, c->h, h, ms, kind);
}

void gfx_draw_cg(EX *e, int id, int ms) {
    image_free(e->cgPrev);
    e->cgPrev = NULL;
    if (ms > 0) {
        e->cgPrev = e->cg;
        e->cgPrevColor = e->cgColor;
        e->camPrev = e->cam;
        anim_start(e, &e->cgFade, 0, 255, ms, 0);
    } else {
        image_free(e->cg);
        e->cgFade.active = false;
    }
    e->cg = NULL;
    e->cgColor = 0xff000000u;
    if (id < 0) {
        e->cgColor = bgr((u32)id & 0xffffff) | 0xff000000u;
        return;
    }
    e->cg = cg_image(e, id);
}

static Billboard *bb_find(EX *e, int id, bool create) {
    for (int i = 0; i < EX_BILLBOARDS; i++)
        if (e->bb[i].used && e->bb[i].id == id) return &e->bb[i];
    if (!create) return NULL;
    for (int i = 0; i < EX_BILLBOARDS; i++)
        if (!e->bb[i].used) {
            memset(&e->bb[i], 0, sizeof e->bb[i]);
            e->bb[i].used = true;
            e->bb[i].id = id;
            return &e->bb[i];
        }
    return NULL;
}

static void bb_clear(Billboard *b) {
    if (b->ownImg) image_free(b->img);
    image_free(b->mask);
    memset(b, 0, sizeof *b);
}

static void snapshot(EX *e, Billboard *self);

int gfx_billboard(EX *e, char *args) {
    char *f[48];
    int n = split_csv(args, f, 48);
    if (n < 11) return 0;
    int kind = field_int(f[0]), id = field_int(f[1]);
    Billboard *b = bb_find(e, id, false);
    if (b) bb_clear(b);
    b = bb_find(e, id, true);
    if (!b) return 0;
    b->kind = kind;
    b->x = field_int(f[4]);
    b->y = field_int(f[5]);
    b->w = field_int(f[6]);
    b->h = field_int(f[7]);
    int ms = field_int(f[8]), flags = field_int(f[9]);
    switch (kind) {
    case 0:
        b->img = cg_image(e, field_int(f[2]));
        b->ownImg = true;
        break;
    case 1:
        b->color = bgr((u32)field_int(f[3]));
        break;
    case 2:
        b->img = n > 20 ? image_load(e, f[20]) : NULL;
        b->ownImg = true;
        break;
    case 3:
        snapshot(e, b);
        break;
    default:
        ex_log(e, "billboard kind %d", kind);
        break;
    }
    if (flags & 0x200) {
        b->a = 0;
        return 0;
    }
    float full = 255.0f * (float)field_int(f[10]) / 16.0f;
    if (full > 255 || full <= 0) full = 255;
    if (ms > 0 && (flags & 8) && kind != 3) {
        b->a = 0;
        anim_start(e, &b->alpha, 0, full, ms, 0);
        return ms;
    }
    b->a = full;
    return 0;
}

int gfx_remove_billboard(EX *e, char *args) {
    char *f[8];
    int n = split_csv(args, f, 8);
    if (n < 3) return 0;
    Billboard *b = bb_find(e, field_int(f[0]), false);
    if (!b) return 0;
    int ms = field_int(f[1]), mode = field_int(f[2]);
    if (ms <= 0 || mode == 256 || mode == 0) {
        bb_clear(b);
        return 0;
    }
    if (n > 3 && strcmp(f[3], "*")) {
        image_free(b->mask);
        b->mask = image_load(e, f[3]);
        b->masked = b->mask != NULL;
    } else b->masked = false;
    b->removing = true;
    anim_start(e, &b->alpha, b->a, 0, ms, 0);
    return ms;
}

int gfx_faceup(EX *e, char *args) {
    char *f[40];
    int n = split_csv(args, f, 40);
    if (n < 26) return 0;
    int id = field_int(f[12]);
    Billboard *b = bb_find(e, id, false);
    if (b) bb_clear(b);
    b = bb_find(e, id, true);
    if (!b) return 0;
    const char *src = f[18];
    if (strchr(src, '.') || strchr(src, '\\')) {
        b->kind = 2;
        b->img = image_load(e, src);
    } else {
        b->kind = 0;
        b->img = cg_image(e, field_int(src));
    }
    b->ownImg = true;
    int ms = field_int(f[24]);
    if (strcmp(f[25], "*") && strcmp(f[25], "-1")) {
        b->mask = image_load(e, f[25]);
        b->masked = b->mask != NULL;
    }
    if (ms <= 0) {
        b->a = 255;
        return 0;
    }
    b->a = 0;
    anim_start(e, &b->alpha, 0, 255, ms, 0);
    return ms;
}

bool gfx_busy(EX *e) {
    for (int i = 0; i < EX_BILLBOARDS; i++) {
        Billboard *b = &e->bb[i];
        if (b->used && (b->alpha.active || b->mx.active || b->my.active)) return true;
    }
    for (int i = 0; i < e->view.layerCount; i++) {
        Layer *l = &e->view.layers[i];
        if (l->mx.active || l->my.active || l->fade.active) return true;
    }
    return e->cgFade.active || cam_busy(&e->cam) || chara_busy(e);
}

static void move_target(EX *e, int id, float x, float y, int ms, int kind, bool rel) {
    Billboard *b = bb_find(e, id, false);
    if (b) {
        float tx = rel ? (float)b->x + x : x, ty = rel ? (float)b->y + y : y;
        if (ms <= 0) {
            b->x = (int)tx;
            b->y = (int)ty;
            b->mx.active = b->my.active = false;
        } else {
            anim_start(e, &b->mx, (float)b->x, tx, ms, kind);
            anim_start(e, &b->my, (float)b->y, ty, ms, kind);
        }
        return;
    }
    Actor *a = chara_actor(e, id);
    if (a) {
        float tx = rel ? a->x + x : x, ty = rel ? a->y + y : y;
        if (ms <= 0) {
            a->x = tx;
            a->y = ty;
            a->mx.active = a->my.active = false;
        } else {
            anim_start(e, &a->mx, a->x, tx, ms, kind);
            anim_start(e, &a->my, a->y, ty, ms, kind);
        }
        return;
    }
    Layer *l = view_layer(e, id);
    if (l) {
        float tx = rel ? (float)l->x + x : x, ty = rel ? (float)l->y + y : y;
        if (ms <= 0) {
            l->x = (int)tx;
            l->y = (int)ty;
            l->mx.active = l->my.active = false;
        } else {
            anim_start(e, &l->mx, (float)l->x, tx, ms, kind);
            anim_start(e, &l->my, (float)l->y, ty, ms, kind);
        }
    }
}

void gfx_move(EX *e, char *args) {
    char *f[40];
    int n = split_csv(args, f, 40);
    if (n < 4) return;
    int ms = field_int(f[1]);
    if (ms < 0) return;
    bool rel = field_int(f[3]) != 0;
    for (int i = 4; i + 3 < n; i += 4) {
        int id = field_int(f[i]);
        if (id < 0) continue;
        move_target(e, id, (float)field_int(f[i + 1]), (float)field_int(f[i + 2]), ms, field_int(f[i + 3]), rel);
    }
}

void gfx_fade(EX *e, char *args) {
    char *f[40];
    int n = split_csv(args, f, 40);
    for (int i = 2; i + 3 < n; i += 4) {
        int id = field_int(f[i]);
        if (id < 0) continue;
        float to = 255.0f * (float)field_int(f[i + 1]) / 16.0f;
        if (to > 255) to = 255;
        int ms = field_int(f[i + 2]);
        Billboard *b = bb_find(e, id, false);
        if (b) {
            if (ms <= 0) {
                b->a = to;
                b->alpha.active = false;
            } else anim_start(e, &b->alpha, b->a, to, ms, 0);
        }
        Actor *a = chara_actor(e, id);
        if (a) {
            if (ms <= 0) {
                a->a = to;
                a->fade.active = false;
            } else anim_start(e, &a->fade, a->a, to, ms, 0);
        }
        Layer *l = view_layer(e, id);
        if (l) {
            if (ms <= 0) {
                l->a = to;
                l->fade.active = false;
            } else anim_start(e, &l->fade, l->a, to, ms, 0);
        }
    }
}

static void draw_bb(EX *e, u32 *dst, Billboard *b) {
    (void)e;
    int a = (int)b->a;
    if (a <= 0) return;
    if (b->kind == 1) {
        fill(dst, EX_W, EX_H, b->x, b->y, b->w > 0 ? b->w : EX_W, b->h > 0 ? b->h : EX_H, b->color, a);
        return;
    }
    if (!b->img) return;
    if (b->masked && b->mask && a < 255) {
        int cut = 255 - a;
        const Image *im = b->img, *m = b->mask;
        for (int y = 0; y < EX_H; y++) {
            int sy = y - b->y;
            if (sy < 0 || sy >= im->h) continue;
            const u32 *s = im->px + (size_t)sy * (size_t)im->w;
            int my = m->h > 0 ? (int)((long)y * m->h / EX_H) : 0;
            const u32 *mk = m->px + (size_t)my * (size_t)m->w;
            u32 *d = dst + (size_t)y * EX_W;
            for (int x = 0; x < EX_W; x++) {
                int sx = x - b->x;
                if (sx < 0 || sx >= im->w) continue;
                int level = (int)(mk[(long)x * m->w / EX_W] & 0xff);
                int v = (level - cut) * 8 + 128;
                if (v <= 0) continue;
                if (v > 255) v = 255;
                u32 sa = im->alpha ? (s[sx] >> 24) * (u32)v / 255 : (u32)v;
                u32 sp = s[sx], dp = d[x], ia = 255 - sa;
                u32 r = ((sp & 0xff) * sa + (dp & 0xff) * ia) / 255;
                u32 g = ((sp >> 8 & 0xff) * sa + (dp >> 8 & 0xff) * ia) / 255;
                u32 bl = ((sp >> 16 & 0xff) * sa + (dp >> 16 & 0xff) * ia) / 255;
                d[x] = r | g << 8 | bl << 16 | 0xff000000u;
            }
        }
        return;
    }
    blit(dst, EX_W, EX_H, b->img, b->x, b->y, a);
}

static void draw_cg(EX *e, u32 *dst, Image *cg, u32 color, Camera *c, int alpha) {
    if (alpha >= 255) {
        u32 col = color | 0xff000000u;
        for (size_t i = 0; i < (size_t)EX_W * EX_H; i++) dst[i] = col;
    } else fill(dst, EX_W, EX_H, 0, 0, EX_W, EX_H, color, alpha);
    if (!cg) return;
    if (c->w == EX_W && c->h == EX_H && c->x == 0 && c->y == 0) blit(dst, EX_W, EX_H, cg, 0, 0, alpha);
    else blit_scaled(dst, EX_W, EX_H, cg, c->x, c->y, c->w, c->h, 0, 0, EX_W, EX_H, alpha);
}

static void compose(EX *e, u32 *dst, int below) {
    if (e->cgPrev || e->cgFade.active) {
        draw_cg(e, dst, e->cgPrev, e->cgPrevColor, &e->cam, 255);
        draw_cg(e, dst, e->cg, e->cgColor, &e->cam, (int)anim_value(e, &e->cgFade, 255));
    } else draw_cg(e, dst, e->cg, e->cgColor, &e->cam, 255);
    chara_draw(e, dst);
    int order[EX_BILLBOARDS], n = 0;
    for (int i = 0; i < EX_BILLBOARDS; i++)
        if (e->bb[i].used && (below < 0 || e->bb[i].id < below)) order[n++] = i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && e->bb[order[j]].id < e->bb[order[j - 1]].id; j--) {
            int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    bool viewDrawn = false;
    for (int i = 0; i < n; i++) {
        Billboard *b = &e->bb[order[i]];
        if (!viewDrawn && b->id >= 300) {
            view_draw(e, dst);
            viewDrawn = true;
        }
        draw_bb(e, dst, b);
    }
    if (!viewDrawn) view_draw(e, dst);
    if (below >= 0) return;
    msg_draw(e, dst);
    for (int z = 0; z < 4; z++) portrait_draw(e, dst, e->portraitZ[z]);
}

static void snapshot(EX *e, Billboard *self) {
    Image *im = image_new(EX_W, EX_H);
    self->used = false;
    compose(e, im->px, self->id);
    self->used = true;
    self->img = im;
    self->ownImg = true;
}

void gfx_frame(EX *e) {
    cam_update(e, &e->cam);
    if (e->cgPrev && !e->cgFade.active) {
        image_free(e->cgPrev);
        e->cgPrev = NULL;
    }
    if (e->cgFade.active) anim_value(e, &e->cgFade, 255);
    for (int i = 0; i < EX_BILLBOARDS; i++) {
        Billboard *b = &e->bb[i];
        if (!b->used) continue;
        if (b->alpha.active) b->a = anim_value(e, &b->alpha, b->a);
        if (b->mx.active) b->x = (int)anim_value(e, &b->mx, (float)b->x);
        if (b->my.active) b->y = (int)anim_value(e, &b->my, (float)b->y);
        if (b->removing && !b->alpha.active) bb_clear(b);
    }
    compose(e, e->screen, -1);
    e->serial++;
}

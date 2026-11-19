#include "ex.h"

static float ease(float t, int kind) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    switch (kind) {
    case 1: return t * t;
    case 2:
    case 5: return 1 - (1 - t) * (1 - t);
    case 3: return t * t * (3 - 2 * t);
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

static void snapshot(EX *e, Billboard *self, int below);

void gfx_draw_cg(EX *e, int id, int ms) {
    (void)ms;
    image_free(e->cg);
    e->cg = NULL;
    e->cgColor = 0xff000000u;
    if (id < 0) {
        e->cgColor = (u32)id & 0xffffff;
        e->cgColor = (e->cgColor >> 16 & 0xff) | (e->cgColor & 0xff00) | (e->cgColor & 0xff) << 16;
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

static u32 bgr(u32 c) { return (c >> 16 & 0xff) | (c & 0xff00) | (c & 0xff) << 16; }

int gfx_billboard(EX *e, char *args) {
    char *f[48];
    int n = split_csv(args, f, 48);
    if (n < 9) return 0;
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
    int ms = field_int(f[8]);
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
        snapshot(e, b, n > 20 ? field_int(f[20]) : -1);
        break;
    default:
        ex_log(e, "billboard kind %d", kind);
        break;
    }
    if (ms > 0 && kind != 3) {
        b->a = 0;
        anim_start(e, &b->alpha, 0, 255, ms, 0);
        return ms;
    }
    b->a = 255;
    b->alpha.active = false;
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
    if (n > 3 && strcmp(f[3], "*")) b->mask = image_load(e, f[3]);
    b->removing = true;
    anim_start(e, &b->alpha, b->a, 0, ms, 0);
    return ms;
}

bool gfx_busy(EX *e) {
    for (int i = 0; i < EX_BILLBOARDS; i++)
        if (e->bb[i].used && e->bb[i].alpha.active) return true;
    for (int i = 0; i < e->view.layerCount; i++) {
        Layer *l = &e->view.layers[i];
        if (l->mx.active || l->my.active || l->fade.active) return true;
    }
    return false;
}

static void draw_bb(EX *e, u32 *dst, Billboard *b) {
    int a = (int)b->a;
    if (a <= 0) return;
    if (b->kind == 1) {
        fill(dst, EX_W, EX_H, b->x, b->y, b->w > 0 ? b->w : EX_W, b->h > 0 ? b->h : EX_H, b->color, a);
        return;
    }
    if (!b->img) return;
    if (b->mask && b->removing) {
        int cut = 255 - a;
        const Image *im = b->img, *m = b->mask;
        for (int y = 0; y < EX_H && y < im->h; y++) {
            const u32 *s = im->px + (size_t)y * (size_t)im->w;
            const u32 *mk = m->px + (size_t)(y < m->h ? y : m->h - 1) * (size_t)m->w;
            u32 *d = dst + (size_t)y * EX_W;
            for (int x = 0; x < EX_W && x < im->w; x++) {
                int level = (int)(mk[x < m->w ? x : m->w - 1] & 0xff);
                int v = (level - cut) * 8 + 128;
                if (v <= 0) continue;
                if (v > 255) v = 255;
                u32 sa = im->alpha ? (s[x] >> 24) * (u32)v / 255 : (u32)v;
                u32 sp = s[x], dp = d[x], ia = 255 - sa;
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

static void compose(EX *e, u32 *dst, int below) {
    for (size_t i = 0; i < (size_t)EX_W * EX_H; i++) dst[i] = e->cgColor;
    if (e->cg) blit(dst, EX_W, EX_H, e->cg, 0, 0, 255);
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
}

static void snapshot(EX *e, Billboard *self, int below) {
    (void)below;
    Image *im = image_new(EX_W, EX_H);
    self->used = false;
    compose(e, im->px, self->id);
    self->used = true;
    self->img = im;
    self->ownImg = true;
}

void gfx_frame(EX *e) {
    for (int i = 0; i < EX_BILLBOARDS; i++) {
        Billboard *b = &e->bb[i];
        if (!b->used) continue;
        if (b->alpha.active) b->a = anim_value(e, &b->alpha, b->a);
        if (b->removing && !b->alpha.active) bb_clear(b);
    }
    compose(e, e->screen, -1);
    e->serial++;
}

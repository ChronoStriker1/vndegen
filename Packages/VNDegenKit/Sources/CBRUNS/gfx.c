#include "br.h"

float anim_value(BR *e, Anim *a) {
    if (!a->dur || e->now >= a->start + a->dur) return a->to;
    if (e->now <= a->start) return a->from;
    float t = (float)(e->now - a->start) / (float)a->dur;
    return a->from + (a->to - a->from) * t;
}

void anim_set(BR *e, Anim *a, float from, float to, int ms) {
    a->from = from;
    a->to = to;
    a->start = e->now;
    a->dur = ms > 0 ? (u32)ms : 0;
}

bool anim_busy(BR *e, Anim *a) { return a->dur && e->now < a->start + a->dur; }

Obj *obj_get(BR *e, int kind, int id) {
    if (id < 0 || id >= BR_OBJS) return NULL;
    return e->obj[kind][id];
}

static void obj_free(Obj *o) {
    if (!o) return;
    free(o->text);
    free(o);
}

Obj *obj_create(BR *e, int kind, int id, int parts, int layer, int x, int y, int z, int alpha, int flag) {
    if (id < 0 || id >= BR_OBJS) return NULL;
    obj_free(e->obj[kind][id]);
    Obj *o = br_alloc(sizeof *o);
    o->kind = kind;
    o->id = id;
    o->parts = parts;
    o->layer = layer;
    o->x = x;
    o->y = y;
    o->z = z;
    o->alpha = alpha;
    o->flag = flag;
    o->event = -1;
    o->order = ++e->objOrder;
    e->obj[kind][id] = o;
    return o;
}

void obj_delete(BR *e, int kind, int id) {
    if (id < 0 || id >= BR_OBJS) return;
    obj_free(e->obj[kind][id]);
    e->obj[kind][id] = NULL;
}

void obj_clear(BR *e, int kind) {
    for (int i = 0; i < BR_OBJS; i++) obj_delete(e, kind, i);
}

static const char *obj_text(BR *e, Obj *o, Parts *p) {
    if (o->text) return o->text;
    return p && p->text ? p->text : "";
}

static Image *obj_image(BR *e, Obj *o, Parts *p) {
    int state = o->disabled ? 3 : o->pressed ? 2 : o->hover ? 1 : 0;
    return parts_image(e, p, state, e->now);
}

bool obj_hit(BR *e, Obj *o, int mx, int my) {
    if (o->layer <= 0 || o->hidden || o->alpha <= 0) return false;
    Parts *p = parts_get(e, o->parts);
    if (!p) return false;
    if (p->type == P_TEXT) {
        const char *t = obj_text(e, o, p);
        int size = o->fontSize ? o->fontSize : p->fontSize;
        return mx >= o->x && my >= o->y && mx < o->x + font_text_width(t, size) && my < o->y + size;
    }
    Image *im = parts_image(e, p, 0, e->now);
    if (!im) return false;
    int x0, y0, x1, y1;
    image_bbox(im, &x0, &y0, &x1, &y1);
    bool hit = mx >= o->x + x0 && my >= o->y + y0 && mx < o->x + x1 && my < o->y + y1;
    image_free(im);
    return hit;
}

static void obj_draw(BR *e, u32 *dst, Obj *o) {
    if (o->layer <= 0 || o->hidden || o->alpha <= 0) return;
    Parts *p = parts_get(e, o->parts);
    if (!p) return;
    if (p->type == P_TEXT) {
        int size = o->fontSize ? o->fontSize : p->fontSize;
        u32 color = o->hasColor ? o->color : p->color;
        font_draw(e, dst, BR_W, BR_H, obj_text(e, o, p), o->x, o->y, size, color, 0, 0, o->alpha);
        return;
    }
    Image *im = obj_image(e, o, p);
    if (!im) return;
    blit(dst, BR_W, BR_H, im, 0, 0, -1, -1, o->x, o->y, o->alpha);
    image_free(im);
}

static int obj_cmp(const void *a, const void *b) {
    const Obj *x = *(Obj *const *)a, *y = *(Obj *const *)b;
    if (x->layer != y->layer) return x->layer - y->layer;
    if (x->z != y->z) return y->z - x->z;
    if (x->kind != y->kind) return x->kind - y->kind;
    return x->order - y->order;
}

int obj_sorted(BR *e, Obj **out, int max) {
    int n = 0;
    for (int k = 0; k < OBJ_KINDS; k++)
        for (int i = 0; i < BR_OBJS && n < max; i++)
            if (e->obj[k][i]) out[n++] = e->obj[k][i];
    qsort(out, (size_t)n, sizeof *out, obj_cmp);
    return n;
}

void bg_set(BR *e, int num, int ms) {
    image_free(e->bgPrev);
    e->bgPrev = NULL;
    if (ms > 0) {
        e->bgPrev = image_new(BR_W, BR_H);
        if (e->bg) blit(e->bgPrev->px, BR_W, BR_H, e->bg, 0, 0, -1, -1, 0, 0, 255);
        else
            for (int i = 0; i < BR_W * BR_H; i++) e->bgPrev->px[i] = 0;
    }
    image_free(e->bg);
    e->bg = num >= 0 ? image_storage(e, num) : NULL;
    e->bgNum = num;
    anim_set(e, &e->bgFade, 0, 255, ms);
}

bool bg_busy(BR *e) { return anim_busy(e, &e->bgFade); }

void overlay_set(BR *e, u32 rgb, int alpha, int ms) {
    float cur = anim_value(e, &e->overlay);
    e->overlayColor = rgb | 0xff000000u;
    anim_set(e, &e->overlay, cur, (float)alpha, ms);
}

bool overlay_busy(BR *e) { return anim_busy(e, &e->overlay); }

void gfx_frame(BR *e) {
    u32 *dst = e->screen;
    for (int i = 0; i < BR_W * BR_H; i++) dst[i] = 0xff000000u;
    layers_draw(e, dst);
    if (e->bgPrev && bg_busy(e)) {
        blit(dst, BR_W, BR_H, e->bgPrev, 0, 0, -1, -1, 0, 0, 255);
        blit(dst, BR_W, BR_H, e->bg, 0, 0, -1, -1, 0, 0, (int)anim_value(e, &e->bgFade));
    } else {
        if (e->bgPrev) {
            image_free(e->bgPrev);
            e->bgPrev = NULL;
        }
        blit(dst, BR_W, BR_H, e->bg, 0, 0, -1, -1, 0, 0, 255);
    }
    static Obj *list[4096];
    int n = obj_sorted(e, list, 4096);
    for (int i = 0; i < n; i++) obj_draw(e, dst, list[i]);
    msg_draw(e, dst);
    menu_draw(e, dst);
    int oa = (int)anim_value(e, &e->overlay);
    if (oa > 0) fill_rect(dst, BR_W, BR_H, 0, 0, BR_W, BR_H, e->overlayColor, oa);
    e->serial++;
}

void gfx_free(BR *e) {
    layers_free(e);
    for (int k = 0; k < OBJ_KINDS; k++) obj_clear(e, k);
    image_free(e->bg);
    image_free(e->bgPrev);
    e->bg = e->bgPrev = NULL;
}

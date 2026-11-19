#include "bsx.h"

#include <ctype.h>

void bx_gfx_init(BX *b) {
    size_t n = (size_t)b->W * (size_t)b->H;
    b->screen = bx_alloc(n * 4);
    b->front = bx_alloc(n * 4);
    b->from = bx_alloc(n * 4);
    b->to = bx_alloc(n * 4);
    for (size_t i = 0; i < n; i++) b->screen[i] = b->front[i] = 0xff000000u;
    b->shownDirty = true;
}

void bx_gfx_close(BX *b) {
    free(b->screen);
    free(b->front);
    free(b->from);
    free(b->to);
    bx_image_free(b->transMask);
    bx_image_cache_clear(b);
}

static inline u32 mix(u32 d, u32 s, u32 a) {
    if (a >= 255) return s | 0xff000000u;
    u32 ia = 255 - a;
    u32 rb = ((s & 0xff00ff) * a + (d & 0xff00ff) * ia) >> 8 & 0xff00ff;
    u32 g = ((s & 0x00ff00) * a + (d & 0x00ff00) * ia) >> 8 & 0x00ff00;
    return rb | g | 0xff000000u;
}

void bx_blit_part(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int w, int h, int dx, int dy, int alpha) {
    if (!im || alpha <= 0) return;
    if (sx < 0) { w += sx; dx -= sx; sx = 0; }
    if (sy < 0) { h += sy; dy -= sy; sy = 0; }
    if (sx + w > im->w) w = im->w - sx;
    if (sy + h > im->h) h = im->h - sy;
    if (dx < 0) { w += dx; sx -= dx; dx = 0; }
    if (dy < 0) { h += dy; sy -= dy; dy = 0; }
    if (dx + w > dw) w = dw - dx;
    if (dy + h > dh) h = dh - dy;
    for (int y = 0; y < h; y++) {
        const u32 *s = im->px + (size_t)(sy + y) * im->w + sx;
        u32 *d = dst + (size_t)(dy + y) * dw + dx;
        for (int x = 0; x < w; x++) {
            u32 a = (s[x] >> 24) * (u32)alpha / 255;
            if (a) d[x] = mix(d[x], s[x], a);
        }
    }
}

void bx_blit(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, int alpha) {
    if (im) bx_blit_part(dst, dw, dh, im, 0, 0, im->w, im->h, dx, dy, alpha);
}

void bx_fill(u32 *dst, int dw, int dh, int x, int y, int w, int h, u32 rgba) {
    u32 a = rgba >> 24;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > dw) w = dw - x;
    if (y + h > dh) h = dh - y;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            u32 *d = &dst[(size_t)(y + j) * dw + x + i];
            *d = mix(*d, rgba & 0xffffff, a);
        }
}

static bool bustup_parts(const char *name, char *body, char *face) {
    size_t n = strlen(name);
    if (n != 12 || tolower((u8)name[0]) != 'b' || tolower((u8)name[1]) != 'u') return false;
    snprintf(body, 64, "%.10s", name);
    snprintf(face, 64, "%.6s%.2s1%c%.2s", name, name + 6, name[9], name + 10);
    return true;
}

static int char_id(const char *name) {
    if (strlen(name) >= 4 && tolower((u8)name[0]) == 'b' && tolower((u8)name[1]) == 'u') return atoi((char[3]){name[2], name[3], 0});
    return 0;
}

static Image *face_image(BX *b, const char *name) {
    char body[64], face[64];
    if (!bustup_parts(name, body, face)) return NULL;
    if (bx_res_find(&b->arc, RX_GFX, face)) return bx_image_load(b, face);
    char alt[64];
    snprintf(alt, sizeof alt, "%.6s001%c%.2s", name, name[9], name + 10);
    if (bx_res_find(&b->arc, RX_GFX, alt)) return bx_image_load(b, alt);
    snprintf(alt, sizeof alt, "%.5s0001%c%.2s", name, name[9], name + 10);
    if (bx_res_find(&b->arc, RX_GFX, alt)) return bx_image_load(b, alt);
    return NULL;
}

static void draw_obj(BX *b, const Obj *o, u32 *dst) {
    char body[64], face[64];
    if (o->kind == OK_CHAR && bustup_parts(o->name, body, face)) {
        Image *im = bx_image_load(b, body);
        if (im) bx_blit(dst, b->W, b->H, im, im->x + o->x, im->y + o->y, 255);
        Image *f = face_image(b, o->name);
        if (f) bx_blit(dst, b->W, b->H, f, f->x + o->x, f->y + o->y, 255);
        return;
    }
    Image *im = bx_image_load(b, o->name);
    if (im) bx_blit(dst, b->W, b->H, im, im->x + o->x, im->y + o->y, 255);
    if (o->face[0]) {
        Image *f = bx_image_load(b, o->face);
        if (f) bx_blit(dst, b->W, b->H, f, f->x + o->x, f->y + o->y, 255);
    }
}

static int order_cmp(const void *pa, const void *pb) {
    const Obj *a = *(const Obj *const *)pa, *b = *(const Obj *const *)pb;
    int ka = a->kind == OK_BG ? 0 : a->kind == OK_CHAR ? 1 : a->kind == OK_OVERLAY ? 2 : 3;
    int kb = b->kind == OK_BG ? 0 : b->kind == OK_CHAR ? 1 : b->kind == OK_OVERLAY ? 2 : 3;
    if (ka != kb) return ka - kb;
    return a->seq - b->seq;
}

void bx_compose(BX *b, const Scene *s, u32 *dst) {
    size_t n = (size_t)b->W * (size_t)b->H;
    for (size_t i = 0; i < n; i++) dst[i] = 0xff000000u;
    const Obj *list[BX_OBJS];
    int count = 0;
    for (int i = 0; i < BX_OBJS; i++)
        if (s->obj[i].used) list[count++] = &s->obj[i];
    qsort(list, (size_t)count, sizeof *list, order_cmp);
    for (int i = 0; i < count; i++) draw_obj(b, list[i], dst);
}

static Obj *find_in(Scene *s, int kind, int id, const char *name) {
    for (int i = 0; i < BX_OBJS; i++) {
        Obj *o = &s->obj[i];
        if (!o->used || o->kind != kind) continue;
        if (kind == OK_OVERLAY || kind == OK_OBJ ? bx_ieq(o->name, name) : o->id == id) return o;
    }
    return NULL;
}

Obj *bx_obj_find(BX *b, int kind, int id) { return find_in(&b->scene, kind, id, ""); }

Obj *bx_obj_put(BX *b, int kind, int id, const char *name, int x, int y) {
    Scene *s = &b->scene;
    if (kind == OK_BG) {
        for (int i = 0; i < BX_OBJS; i++)
            if (s->obj[i].used && s->obj[i].kind != OK_OBJ) s->obj[i].used = false;
    }
    if (kind == OK_CHAR) id = char_id(name);
    Obj *o = find_in(s, kind, id, name);
    if (!o)
        for (int i = 0; i < BX_OBJS && !o; i++)
            if (!s->obj[i].used) o = &s->obj[i];
    if (!o) {
        bx_log(b, "too many pictures on screen");
        return NULL;
    }
    memset(o, 0, sizeof *o);
    o->used = true;
    o->kind = kind;
    o->id = id;
    snprintf(o->name, sizeof o->name, "%s", name);
    o->x = x;
    o->y = y;
    o->seq = ++s->seq;
    return o;
}

void bx_obj_remove(BX *b, Obj *o) {
    (void)b;
    if (o) o->used = false;
}

void bx_obj_face(BX *b, Obj *o, const char *name) {
    (void)b;
    if (o) snprintf(o->face, sizeof o->face, "%s", name);
}

bool bx_obj_move(BX *b, int kind, int id, int x, int y, int ms) {
    bool any = false;
    Scene *scenes[2] = {&b->scene, &b->shown};
    for (int k = 0; k < 2; k++) {
        Obj *o = find_in(scenes[k], kind, id, "");
        if (!o) continue;
        any = true;
        o->ax0 = o->x;
        o->ay0 = o->y;
        o->ax1 = x;
        o->ay1 = y;
        o->animStart = b->now;
        o->animDur = (u32)(ms > 0 ? ms : 0);
        o->anim = ms > 0;
        if (!o->anim) {
            o->x = x;
            o->y = y;
        }
    }
    b->shownDirty = true;
    return any;
}

void bx_anim_update(BX *b) {
    Scene *scenes[2] = {&b->scene, &b->shown};
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < BX_OBJS; i++) {
            Obj *o = &scenes[k]->obj[i];
            if (!o->used || !o->anim) continue;
            u32 t = b->now - o->animStart;
            if (t >= o->animDur) {
                o->x = o->ax1;
                o->y = o->ay1;
                o->anim = 0;
            } else {
                o->x = o->ax0 + (int)((i64)(o->ax1 - o->ax0) * t / o->animDur);
                o->y = o->ay0 + (int)((i64)(o->ay1 - o->ay0) * t / o->animDur);
            }
            b->shownDirty = true;
        }
}

void bx_transition(BX *b, int type, int ticks, const char *mask) {
    size_t n = (size_t)b->W * (size_t)b->H;
    memcpy(b->from, b->front, n * 4);
    b->shown = b->scene;
    bx_compose(b, &b->shown, b->to);
    b->shownDirty = false;
    bx_image_free(b->transMask);
    b->transMask = NULL;
    if (mask && *mask) {
        Image *m = bx_image_load(b, mask);
        if (m) {
            b->transMask = bx_image_new(m->w, m->h);
            memcpy(b->transMask->px, m->px, sizeof(u32) * (size_t)m->w * (size_t)m->h);
        }
    }
    b->transType = type;
    b->transStart = b->now;
    b->transDur = (u32)(ticks > 0 ? ticks : 0) * 100;
    if (!b->transDur || (!b->transMask && type == 0)) {
        memcpy(b->front, b->to, n * 4);
        b->transActive = false;
        return;
    }
    b->transActive = true;
}

void bx_flash(BX *b, int r, int g, int bl, int ticks) {
    b->flashColor = (u32)(r & 255) | (u32)(g & 255) << 8 | (u32)(bl & 255) << 16;
    b->flashStart = b->now;
    b->flashDur = (u32)(ticks > 0 ? ticks : 1) * 100;
}

static void trans_frame(BX *b) {
    u32 t = b->now - b->transStart;
    size_t n = (size_t)b->W * (size_t)b->H;
    if (t >= b->transDur || b->skipMode) {
        memcpy(b->front, b->to, n * 4);
        b->transActive = false;
        return;
    }
    int p = (int)((u64)t * 256 / b->transDur);
    if (b->transMask) {
        Image *m = b->transMask;
        bool reverse = b->transType == 3;
        for (int y = 0; y < b->H; y++) {
            int my = y * m->h / b->H;
            for (int x = 0; x < b->W; x++) {
                int mx = x * m->w / b->W;
                int v = (int)(m->px[(size_t)my * m->w + mx] & 0xff);
                if (reverse) v = 255 - v;
                int a = (p * 2 - v) * 4;
                size_t i = (size_t)y * b->W + x;
                b->front[i] = mix(b->from[i], b->to[i], (u32)clampi(a, 0, 255));
            }
        }
        return;
    }
    u32 a = (u32)clampi(p, 0, 255);
    for (size_t i = 0; i < n; i++) b->front[i] = mix(b->from[i], b->to[i], a);
}

void bx_gfx_frame(BX *b) {
    size_t n = (size_t)b->W * (size_t)b->H;
    if (b->transActive) trans_frame(b);
    else if (b->shownDirty) {
        bx_compose(b, &b->shown, b->front);
        b->shownDirty = false;
    }
    memcpy(b->screen, b->front, n * 4);
    if (b->flashDur) {
        u32 t = b->now - b->flashStart;
        if (t >= b->flashDur) b->flashDur = 0;
        else {
            u32 a = 255 - (u32)((u64)t * 255 / b->flashDur);
            for (size_t i = 0; i < n; i++) b->screen[i] = mix(b->screen[i], b->flashColor, a);
        }
    }
    if (b->phase != PH_NONE) bx_phase_draw(b, b->screen);
    else {
        bx_msg_draw(b, b->screen);
        bx_choice_draw(b, b->screen);
        bx_ui_draw(b, b->screen);
    }
    b->serial++;
}

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

void bx_blit_multiply(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, int alpha) {
    if (!im || alpha <= 0) return;
    for (int y = 0; y < im->h; y++) {
        int py = dy + y;
        if (py < 0 || py >= dh) continue;
        for (int x = 0; x < im->w; x++) {
            int px = dx + x;
            if (px < 0 || px >= dw) continue;
            u32 s = im->px[(size_t)y * im->w + x];
            u32 a = (s >> 24) * (u32)alpha / 255;
            if (!a) continue;
            u32 *d = &dst[(size_t)py * dw + px];
            u32 m = 0xff000000u;
            for (int k = 0; k < 24; k += 8) m |= (((*d >> k) & 0xff) * ((s >> k) & 0xff) / 255) << k;
            *d = mix(*d, m & 0xffffff, a);
        }
    }
}

static int blend_channel(char mode, int s, int c) {
    switch (mode) {
    case '*': return s * c / 255;
    case '+': return s + c > 255 ? 255 : s + c;
    case '-': return s - c < 0 ? 0 : s - c;
    case '^': return c >= 255 ? 255 : clampi(s * 255 / (255 - c), 0, 255);
    case '/': return c <= 0 ? 0 : 255 - clampi((255 - s) * 255 / c, 0, 255);
    case '&': return s + c - 255 < 0 ? 0 : s + c - 255;
    default: return s;
    }
}

static void blit_tinted(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, int alpha, char mode, u32 color) {
    u32 strength = color >> 24;
    for (int y = 0; y < im->h; y++) {
        int py = dy + y;
        if (py < 0 || py >= dh) continue;
        for (int x = 0; x < im->w; x++) {
            int px = dx + x;
            if (px < 0 || px >= dw) continue;
            u32 s = im->px[(size_t)y * im->w + x];
            u32 a = (s >> 24) * (u32)alpha / 255;
            if (!a) continue;
            u32 t = 0;
            for (int k = 0; k < 24; k += 8) t |= (u32)blend_channel(mode, (int)(s >> k & 0xff), (int)(color >> (16 - k) & 0xff)) << k;
            u32 *d = &dst[(size_t)py * dw + px];
            *d = mix(*d, mix(s, t, strength) & 0xffffff, a);
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

static bool face_name(BX *b, const char *name, char *out) {
    char body[64];
    if (!bustup_parts(name, body, out)) return false;
    if (bx_res_find(&b->arc, RX_GFX, out)) return true;
    snprintf(out, 64, "%.6s001%c%.2s", name, name[9], name + 10);
    if (bx_res_find(&b->arc, RX_GFX, out)) return true;
    snprintf(out, 64, "%.5s0001%c%.2s", name, name[9], name + 10);
    return bx_res_find(&b->arc, RX_GFX, out) != NULL;
}

int bx_eye_frame(BX *b, const Obj *o) {
    int pattern = bx_cfg_int(&b->cfg, "bustup setting", "eye pattern", 0);
    if (pattern < 2) return -1;
    char *end;
    const char *iv = bx_cfg_get(&b->cfg, "bustup setting", "eye anime interval");
    u32 cycle = (u32)((iv ? strtod(iv, &end) : 30.0) * 100.0);
    if (cycle < 1000) cycle = 1000;
    u32 t = (b->now + (u32)o->charNo * 1733u) % cycle;
    u32 step = 60;
    return t < step * (u32)(pattern - 1) ? 1 + (int)(t / step) : 0;
}

static void stripped(BX *b, const char *name, char *out) {
    snprintf(out, 64, "%s", name);
    if (!b->stripLevel || strlen(name) != 12) return;
    int table[8] = {0}, n = bx_cfg_ints(&b->cfg, "system button", "simple strip table", table, 8);
    if (b->stripLevel >= n) return;
    char alt[64], body[64], face[64];
    snprintf(alt, sizeof alt, "%s", name);
    for (int wear = clampi(table[b->stripLevel], 0, 9); wear > name[5] - '0'; wear--) {
        alt[5] = (char)('0' + wear);
        if (bustup_parts(alt, body, face) && bx_res_find(&b->arc, RX_GFX, body)) {
            snprintf(out, 64, "%s", alt);
            return;
        }
    }
}

int bx_view_level(BX *b) { return b->viewLevel ? b->viewLevel : b->viewButtons ? 1 : 0; }

static int view_offset(BX *b, const char *name) {
    if (!bx_view_level(b) || strlen(name) != 12 || (name[4] != 'u' && name[4] != 'U')) return 0;
    char key[64];
    snprintf(key, sizeof key, "fixed position%.2s%c", name + 2, name[9]);
    int pos[3] = {0, 0, 0};
    if (bx_cfg_ints(&b->cfg, "free-sight screen", key, pos, 3) < 3) bx_cfg_ints(&b->cfg, "free-sight screen", "fixed position", pos, 3);
    int v = pos[clampi(bx_view_level(b) - 1, 0, 2)];
    return v ? -(v - b->H) : 0;
}

static void draw_obj(BX *b, const Obj *o, u32 *dst, const char *tint) {
    char body[64], face[64];
    int x = o->x + o->kx, y = o->y + o->ky, a = o->alpha;
    if (a <= 0) return;
    if (o->kind == OK_CHAR && bustup_parts(o->name, body, face)) {
        char shown[64];
        stripped(b, o->name, shown);
        bustup_parts(shown, body, face);
        y += view_offset(b, o->name);
        char fn[64], part[80];
        bool hasFace = face_name(b, shown, fn) || face_name(b, o->name, fn);
        int eye = hasFace ? bx_eye_frame(b, o) : -1;
        Image *eyes = NULL;
        u8 head[0x12];
        if (eye >= 0 && bx_res_head(&b->arc, RX_GFX, fn, head, sizeof head) && !memcmp(head, "BSS-Composition", 15) && head[0x11] >= 2) {
            snprintf(part, sizeof part, "%s#1", fn);
            eyes = bx_image_load(b, part);
            snprintf(part, sizeof part, "%s#0", fn);
        } else snprintf(part, sizeof part, "%s", fn);
        u32 color = tint ? (u32)strtoul(tint + 1, NULL, 0) : 0;
        int pattern = bx_cfg_int(&b->cfg, "bustup setting", "eye pattern", 1);
        Image *parts[3] = {bx_image_load(b, body), hasFace ? bx_image_load(b, part) : NULL, eyes};
        for (int i = 0; i < 3; i++) {
            if (!parts[i]) continue;
            Image *im = parts[i], frame;
            if (i == 2) {
                frame = *im;
                frame.h = im->h / pattern;
                frame.px = im->px + (size_t)clampi(eye, 0, pattern - 1) * (size_t)frame.h * (size_t)im->w;
                im = &frame;
            }
            if (color >> 24) blit_tinted(dst, b->W, b->H, im, im->x + x, im->y + y, a, tint[0], color);
            else bx_blit(dst, b->W, b->H, im, im->x + x, im->y + y, a);
        }
        return;
    }
    Image *im = bx_image_load(b, o->name);
    if (im && o->kind == OK_OBJ && bx_cfg_get(&b->cfg, "button object", o->name)) {
        int h = im->h / 2;
        bx_blit_part(dst, b->W, b->H, im, 0, o->id == b->objHover ? h : 0, im->w, h, im->x + x, im->y + y, a);
        return;
    }
    if (im && o->blend == 1) bx_blit_multiply(dst, b->W, b->H, im, im->x + x, im->y + y, a);
    else if (im) bx_blit(dst, b->W, b->H, im, im->x + x, im->y + y, a);
    if (o->face[0]) {
        Image *f = bx_image_load(b, o->face);
        if (f) bx_blit(dst, b->W, b->H, f, f->x + x, f->y + y, a);
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
    const char *tint = count && list[0]->kind == OK_BG ? bx_cfg_get(&b->cfg, "bu color adjustment", list[0]->name) : NULL;
    if (tint && !strchr("*+-^/&", tint[0])) tint = NULL;
    for (int i = 0; i < count; i++) draw_obj(b, list[i], dst, tint);
}

static bool numbered(int kind) { return kind != OK_BG; }

static Obj *find_named(Scene *s, int kind, const char *name) {
    int no = kind == OK_CHAR ? char_id(name) : 0;
    for (int i = 0; i < BX_OBJS; i++) {
        Obj *o = &s->obj[i];
        if (!o->used || o->kind != kind) continue;
        if (kind == OK_BG) return o;
        if (kind == OK_CHAR ? o->charNo == no : bx_ieq(o->name, name)) return o;
    }
    return NULL;
}

static Obj *find_id(Scene *s, int id) {
    for (int i = 0; i < BX_OBJS; i++)
        if (s->obj[i].used && numbered(s->obj[i].kind) && s->obj[i].id == id) return &s->obj[i];
    return NULL;
}

Obj *bx_obj_find(BX *b, int kind, int id) {
    for (int i = 0; i < BX_OBJS; i++) {
        Obj *o = &b->scene.obj[i];
        if (o->used && o->kind == kind && (kind == OK_BG || o->id == id)) return o;
    }
    return NULL;
}

Obj *bx_obj_by_id(BX *b, int id) { return find_id(&b->scene, id); }

Obj *bx_obj_put(BX *b, int kind, int id, const char *name, int x, int y) {
    (void)id;
    Scene *s = &b->scene;
    if (kind == OK_BG) {
        for (int i = 0; i < BX_OBJS; i++)
            s->obj[i].used = false;
        b->stripLevel = 0;
        b->viewLevel = 0;
    }
    Obj *o = find_named(s, kind, name);
    int keepId = o ? o->id : 0;
    if (!o)
        for (int i = 0; i < BX_OBJS && !o; i++)
            if (!s->obj[i].used) o = &s->obj[i];
    if (!o) {
        bx_log(b, "too many pictures on screen");
        return NULL;
    }
    if (numbered(kind) && !keepId) {
        keepId = 1;
        while (find_id(s, keepId)) keepId++;
    }
    memset(o, 0, sizeof *o);
    o->used = true;
    o->kind = kind;
    o->id = keepId;
    o->charNo = kind == OK_CHAR ? char_id(name) : 0;
    o->alpha = 255;
    o->blend = b->nextBlend;
    b->nextBlend = 0;
    snprintf(o->name, sizeof o->name, "%s", name);
    bx_museum_seen(b, name);
    if (kind == OK_CHAR && strlen(name) >= 6 && name[5] >= '0' && name[5] <= '9') bx_museum_wear(b, o->charNo, name[5] - '0');
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
    bx_museum_seen(b, name);
    if (o) snprintf(o->face, sizeof o->face, "%s", name);
}

bool bx_obj_move(BX *b, int id, int x, int y, int ms) {
    bool any = false;
    Scene *scenes[2] = {&b->scene, &b->shown};
    for (int k = 0; k < 2; k++) {
        Obj *o = find_id(scenes[k], id);
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

static int attr(const char *tag, const char *name, int *a, int *bb) {
    char key[32];
    snprintf(key, sizeof key, "%s=\"", name);
    const char *p = strstr(tag, key);
    if (!p) return 0;
    p += strlen(key);
    char *e;
    *a = (int)strtol(p, &e, 10);
    if (bb) {
        while (*e == ' ') e++;
        *bb = (int)strtol(e, NULL, 10);
    }
    return 1;
}

static Anim *anim_get(BX *b, const char *name) {
    for (int i = 0; i < b->animCount; i++)
        if (bx_ieq(b->anims[i].name, name)) return &b->anims[i].anim;
    if (b->animCount >= 16) return NULL;
    char file[96];
    snprintf(file, sizeof file, "%s.txt", name);
    size_t n;
    u8 *d = bx_res_read(&b->arc, RX_DEF, file, &n);
    if (!d) {
        bx_log(b, "no animation %s", name);
        return NULL;
    }
    Anim *a = &b->anims[b->animCount].anim;
    memset(a, 0, sizeof *a);
    b->anims[b->animCount++].name = bx_strdup(name);
    const char *s = (const char *)d;
    const char *layer = strstr(s, "<layer");
    a->fps = 60;
    if (layer) {
        attr(layer, "fps", &a->fps, NULL);
        attr(layer, "begin", &a->begin, NULL);
        attr(layer, "end", &a->end, NULL);
        a->keep = strstr(layer, "takeover=\"true\"") == NULL;
    }
    int lastX = 0, lastY = 0, lastO = 100;
    for (const char *f = strstr(s, "<frame"); f; f = strstr(f + 1, "<frame")) {
        a->frames = bx_realloc(a->frames, sizeof(KeyFrame) * (size_t)(a->count + 1));
        KeyFrame *k = &a->frames[a->count++];
        k->no = 0;
        attr(f, "no", &k->no, NULL);
        if (!attr(f, "pos", &k->x, &k->y)) {
            k->x = lastX;
            k->y = lastY;
        }
        if (!attr(f, "opacity", &k->opacity, NULL)) k->opacity = lastO;
        lastX = k->x;
        lastY = k->y;
        lastO = k->opacity;
    }
    free(d);
    if (a->fps <= 0) a->fps = 60;
    return a;
}

void bx_obj_keyframes(BX *b, int id, const char *spec) {
    char name[64];
    snprintf(name, sizeof name, "%s", spec);
    char *comma = strchr(name, ',');
    int delay = 0;
    if (comma) {
        delay = atoi(comma + 1);
        *comma = 0;
    }
    Anim *a = anim_get(b, name);
    if (!a || !a->count) return;
    Scene *scenes[2] = {&b->scene, &b->shown};
    for (int k = 0; k < 2; k++) {
        Obj *o = find_id(scenes[k], id);
        if (!o) continue;
        o->key = a;
        o->keyDelay = delay;
        o->animStart = b->now;
        o->kx = a->frames[0].x;
        o->ky = a->frames[0].y;
        o->alpha = a->frames[0].opacity * 255 / 100;
    }
    b->shownDirty = true;
}

static bool keyframe_step(BX *b, Obj *o) {
    Anim *a = o->key;
    int frame = (int)((u64)(b->now - o->animStart) * (u64)a->fps / 1000) - o->keyDelay;
    int last = a->frames[a->count - 1].no;
    if (frame < 0) frame = 0;
    if (frame >= last) {
        KeyFrame *k = &a->frames[a->count - 1];
        o->kx = k->x;
        o->ky = k->y;
        o->alpha = k->opacity * 255 / 100;
        o->key = NULL;
        if (o->alpha <= 0 && !a->keep) o->used = false;
        return false;
    }
    int i = 0;
    while (i + 1 < a->count && a->frames[i + 1].no <= frame) i++;
    KeyFrame *k0 = &a->frames[i], *k1 = i + 1 < a->count ? &a->frames[i + 1] : k0;
    int span = k1->no - k0->no;
    int t = span > 0 ? (frame - k0->no) * 256 / span : 0;
    o->kx = k0->x + (k1->x - k0->x) * t / 256;
    o->ky = k0->y + (k1->y - k0->y) * t / 256;
    o->alpha = (k0->opacity + (k1->opacity - k0->opacity) * t / 256) * 255 / 100;
    return true;
}

bool bx_anim_busy(BX *b) {
    for (int i = 0; i < BX_OBJS; i++) {
        Obj *o = &b->shown.obj[i];
        if (o->used && (o->anim || o->key)) return true;
    }
    return false;
}

void bx_anim_update(BX *b) {
    Scene *scenes[2] = {&b->scene, &b->shown};
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < BX_OBJS; i++) {
            Obj *o = &scenes[k]->obj[i];
            if (!o->used) continue;
            if (o->key) {
                keyframe_step(b, o);
                b->shownDirty = true;
            }
            if (!o->anim) continue;
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

void bx_shake(BX *b, int x, int y) {
    b->shakeX = x;
    b->shakeY = y;
    b->shakeStart = b->now;
}

#define SHAKE_MS 500

static int shake_offset(u32 seed, int amp) {
    int range = amp * 3 / 2;
    if (range <= 0) return 0;
    seed = seed * 2654435761u ^ seed >> 13;
    return (int)(seed % (u32)(2 * range + 1)) - range;
}

static void shake_frame(BX *b) {
    u32 t = b->now - b->shakeStart;
    if (t >= SHAKE_MS || b->skipMode) {
        b->shakeX = b->shakeY = 0;
        return;
    }
    u32 step = (b->now / 33) * 2;
    int dx = shake_offset(step, b->shakeX), dy = shake_offset(step + 1, b->shakeY);
    for (int y = 0; y < b->H; y++) {
        int sy = y - dy;
        u32 *row = b->screen + (size_t)y * b->W;
        for (int x = 0; x < b->W; x++) {
            int sx = x - dx;
            row[x] = sx >= 0 && sx < b->W && sy >= 0 && sy < b->H ? b->front[(size_t)sy * b->W + sx] : 0xff000000u;
        }
    }
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
    if (bx_movie_draw(b, b->screen)) {
        b->serial++;
        return;
    }
    for (int i = 0; i < BX_OBJS && !b->transActive; i++) {
        const Obj *o = &b->shown.obj[i];
        int e = o->used && o->kind == OK_CHAR ? bx_eye_frame(b, o) : -1;
        if (e != b->eyeShown[i]) {
            b->eyeShown[i] = e;
            b->shownDirty = true;
        }
    }
    if (b->transActive) trans_frame(b);
    else if (b->shownDirty) {
        bx_compose(b, &b->shown, b->front);
        b->shownDirty = false;
    }
    memcpy(b->screen, b->front, n * 4);
    if (b->shakeX || b->shakeY) shake_frame(b);
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

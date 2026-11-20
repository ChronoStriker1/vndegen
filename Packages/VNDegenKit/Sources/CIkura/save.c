#include "ikura.h"

#include <math.h>
#include <time.h>

#define HEAD_SIZE 0x500
#define TABLE 0x14
#define GLOBAL_AT 0xdc
#define GLOBAL_END 0x114
#define TEXT_AT 0x0c
#define TEXT_END 0x10c
#define THUMB_AT 0x114
#define THUMB_W 112
#define THUMB_H 84
#define STATE_AT (THUMB_AT + THUMB_W * THUMB_H * 3)
#define NAME_LEN 12
#define SETTINGS 64
#define SETTINGS_USED 42
#define WIDE_RECORDS 23
#define WIDE_MIDDLE (4 * (6 + 9 * WIDE_RECORDS + 5))
#define GAN_STATE 32

static const i32 defaultSettings[SETTINGS_USED] = {1000, 1, 0, 500, 500, 0, 128, 0, 128, 0, 128, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                                   1,    1, 1, 1,   1,   1, 1, 1,   1, 1,   1, 1,   1, 1, 1, 1, 0, 0, 0, 0, 0};

typedef struct Buf {
    u8 *p;
    size_t n, cap;
} Buf;

static void put(Buf *b, const void *d, size_t n) {
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2 + 256;
        b->p = ik_realloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, d, n);
    b->n += n;
}

static void put32(Buf *b, u32 v) {
    u8 q[4];
    wr32(q, v);
    put(b, q, 4);
}

static void putf(Buf *b, float f) {
    u32 v;
    memcpy(&v, &f, 4);
    put32(b, v);
}

static void putname(Buf *b, const char *s) {
    char q[NAME_LEN] = {0};
    if (s) memcpy(q, s, strnlen(s, NAME_LEN));
    put(b, q, NAME_LEN);
}

typedef struct Rd {
    const u8 *p;
    size_t n, at;
    bool bad;
} Rd;

static u32 get32(Rd *r) {
    if (r->at + 4 > r->n) {
        r->bad = true;
        return 0;
    }
    u32 v = rd32(r->p + r->at);
    r->at += 4;
    return v;
}

static float getf(Rd *r) {
    u32 v = get32(r);
    float f;
    memcpy(&f, &v, 4);
    return f;
}

static void getname(Rd *r, char out[16]) {
    memset(out, 0, 16);
    if (r->at + NAME_LEN > r->n) {
        r->bad = true;
        return;
    }
    memcpy(out, r->p + r->at, NAME_LEN);
    r->at += NAME_LEN;
}

static u8 *read_save(IK *t, size_t *size) {
    u8 *d = t->save.path ? ik_read_file(t->save.path, size) : NULL;
    if (d && *size >= HEAD_SIZE) return d;
    free(d);
    *size = HEAD_SIZE;
    d = ik_alloc(HEAD_SIZE);
    memset(d, 0, HEAD_SIZE);
    wr32(d, (u32)t->save.slots);
    wr32(d + 8, GLOBAL_AT);
    wr32(d + 12, GLOBAL_END);
    return d;
}

static void write_save(IK *t, const u8 *d, size_t n) {
    if (!t->save.path) return;
    char *dir = ik_strdup(t->save.path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = 0;
        ik_mkdirs(dir);
    }
    free(dir);
    if (!ik_write_file(t->save.path, d, n)) ik_log(t, "can't write %s", t->save.path);
}

static void store_block(u8 **d, size_t *n, u32 headField, const u8 *blk, size_t len) {
    u32 at = rd32(*d + headField);
    u32 old = at && at + 4 <= *n ? (headField >= TABLE ? rd32(*d + at) : (u32)len) : 0;
    if (!at || old != len || at + len > *n) {
        at = (u32)*n;
        *d = ik_realloc(*d, *n + len);
        *n += len;
        wr32(*d + headField, at);
    }
    memcpy(*d + at, blk, len);
}

void settings_apply(IK *t) {
    snd_volume(t, CH_BGM, t->settings[5] ? 0 : t->settings[6]);
    snd_volume(t, CH_SE, t->settings[7] ? 0 : t->settings[8]);
    snd_volume(t, CH_VOICE, t->settings[9] ? 0 : t->settings[10]);
    t->msg.msPerChar = t->settings[4] / 10;
}

static void load_settings(IK *t) {
    for (int i = 0; i < SETTINGS; i++) t->settings[i] = i < SETTINGS_USED ? defaultSettings[i] : 0;
    size_t n;
    u8 *d = read_save(t, &n);
    u32 at = rd32(d + 4);
    if (at && at + SETTINGS_USED * 4 <= n)
        for (int i = 0; i < SETTINGS_USED; i++) t->settings[i] = (i32)rd32(d + at + 4 * i);
    free(d);
    settings_apply(t);
}

static void save_settings(IK *t) {
    size_t n;
    u8 *d = read_save(t, &n);
    u8 blk[SETTINGS * 4] = {0};
    for (int i = 0; i < SETTINGS_USED; i++) wr32(blk + 4 * i, (u32)t->settings[i]);
    store_block(&d, &n, 4, blk, sizeof blk);
    write_save(t, d, n);
    free(d);
}

static bool flag_get(IK *t, int i) { return i >= 0 && i < t->flagCount * 8 && (t->flags[i >> 3] >> (i & 7) & 1); }

static void flag_put(IK *t, int i, bool on) {
    if (i < 0 || i >= t->flagCount * 8) return;
    if (on) t->flags[i >> 3] |= (u8)(1 << (i & 7));
    else t->flags[i >> 3] &= (u8)~(1 << (i & 7));
}

static bool global_span(const u8 *d, size_t n, u32 *at, u32 *len) {
    u32 a = rd32(d + 8), e = rd32(d + 12);
    if (a < TABLE || e <= a || e > n) return false;
    *at = a;
    *len = e - a;
    return true;
}

static void save_global(IK *t, int f0, int fc, int v0, int vc) {
    size_t n;
    u8 *d = read_save(t, &n);
    u32 at, len;
    if (!global_span(d, n, &at, &len) || 16 + (fc + 7) / 8 + vc * 4 > (int)len) {
        free(d);
        return;
    }
    u8 *g = d + at;
    memset(g, 0, len);
    wr32(g, (u32)f0);
    wr32(g + 4, (u32)fc);
    wr32(g + 8, (u32)v0);
    wr32(g + 12, (u32)vc);
    u8 *bits = g + 16;
    for (int i = 0; i < fc; i++)
        if (flag_get(t, f0 + i)) bits[i >> 3] |= (u8)(1 << (i & 7));
    u8 *vars = bits + (fc + 7) / 8;
    for (int i = 0; i < vc; i++) wr32(vars + 4 * i, v0 + i < t->varCount ? (u32)t->vars[v0 + i] : 0);
    write_save(t, d, n);
    free(d);
}

static void load_global(IK *t) {
    size_t n;
    u8 *d = read_save(t, &n);
    u32 at, len;
    if (!global_span(d, n, &at, &len)) {
        free(d);
        return;
    }
    const u8 *g = d + at;
    int f0 = (int)rd32(g), fc = (int)rd32(g + 4), v0 = (int)rd32(g + 8), vc = (int)rd32(g + 12);
    if (fc >= 0 && vc >= 0 && 16 + (fc + 7) / 8 + vc * 4 <= (int)len) {
        const u8 *bits = g + 16;
        for (int i = 0; i < fc; i++) flag_put(t, f0 + i, bits[i >> 3] >> (i & 7) & 1);
        const u8 *vars = bits + (fc + 7) / 8;
        for (int i = 0; i < vc; i++)
            if (v0 + i >= 0 && v0 + i < t->varCount) t->vars[v0 + i] = (i32)rd32(vars + 4 * i);
    }
    free(d);
}

static void thumbnail(IK *t, u8 *out) {
    for (int y = 0; y < THUMB_H; y++)
        for (int x = 0; x < THUMB_W; x++) {
            int x0 = x * t->W / THUMB_W, x1 = (x + 1) * t->W / THUMB_W, y0 = y * t->H / THUMB_H, y1 = (y + 1) * t->H / THUMB_H;
            u32 r = 0, g = 0, b = 0, c = 0;
            for (int j = y0; j < y1; j++)
                for (int i = x0; i < x1; i++) {
                    u32 p = t->screen[(size_t)j * t->W + i];
                    r += p & 0xff;
                    g += p >> 8 & 0xff;
                    b += p >> 16 & 0xff;
                    c++;
                }
            u8 *o = out + (y * THUMB_W + x) * 3;
            o[0] = (u8)(b / c);
            o[1] = (u8)(g / c);
            o[2] = (u8)(r / c);
        }
}

static void put_layer(Buf *b, const Layer *l) {
    if (!l->made) {
        put32(b, 6);
        for (int i = 0; i < 10; i++) put32(b, 0);
        put32(b, 0);
        put32(b, 0xffffffffu);
        put32(b, 0);
        put32(b, 0);
        put32(b, 0);
        return;
    }
    put32(b, (l->shown ? 1u : 0u) | 2u | (l->textured ? 4u : 0u));
    put32(b, (u32)(l->quad ? l->tw : l->w));
    put32(b, (u32)(l->quad ? l->th : l->h));
    float x0 = (float)l->x, x1 = (float)(l->x + l->w), y0 = (float)l->y, y1 = (float)(l->y + l->h);
    putf(b, x0);
    putf(b, x0);
    putf(b, x1);
    putf(b, x1);
    putf(b, y0);
    putf(b, y1);
    putf(b, y0);
    putf(b, y1);
    u32 argb = (u32)clampi(l->argb[0], 0, 255) << 24 | (u32)clampi(l->argb[1], 0, 255) << 16 |
               (u32)clampi(l->argb[2], 0, 255) << 8 | (u32)clampi(l->argb[3], 0, 255);
    put32(b, argb);
    put32(b, (u32)l->img);
    put32(b, (u32)l->sx);
    put32(b, (u32)l->sy);
    put32(b, l->fillArgb);
}

static void get_layer(IK *t, Rd *r, Layer *l) {
    u32 fl = get32(r);
    int w = (int)get32(r), h = (int)get32(r);
    float xs[4], ys[4];
    for (int k = 0; k < 4; k++) xs[k] = getf(r);
    for (int k = 0; k < 4; k++) ys[k] = getf(r);
    u32 argb = get32(r);
    int img = (int)get32(r), sx = (int)get32(r), sy = (int)get32(r);
    u32 fillArgb = get32(r);
    image_free(l->surf);
    memset(l, 0, sizeof *l);
    if (!w && !h && img < 0 && !argb) return;
    l->made = true;
    l->ready = true;
    l->shown = fl & 1;
    l->textured = fl & 4;
    l->w = w;
    l->h = h;
    float minx = xs[0], miny = ys[0];
    for (int k = 1; k < 4; k++) {
        if (xs[k] < minx) minx = xs[k];
        if (ys[k] < miny) miny = ys[k];
    }
    l->x = (int)lroundf(minx);
    l->y = (int)lroundf(miny);
    float maxx = xs[0], maxy = ys[0];
    for (int k = 1; k < 4; k++) {
        if (xs[k] > maxx) maxx = xs[k];
        if (ys[k] > maxy) maxy = ys[k];
    }
    l->tw = w;
    l->th = h;
    int qw = (int)lroundf(maxx - minx), qh = (int)lroundf(maxy - miny);
    if (qw != w || qh != h) {
        l->quad = true;
        l->w = qw;
        l->h = qh;
    }
    l->argb[0] = (int)(argb >> 24);
    l->argb[1] = (int)(argb >> 16 & 0xff);
    l->argb[2] = (int)(argb >> 8 & 0xff);
    l->argb[3] = (int)(argb & 0xff);
    l->img = img;
    l->sx = sx;
    l->sy = sy;
    l->fillArgb = fillArgb;
    l->fill = img < 0;
    (void)t;
}

static void put_button(Buf *b, const Button *bt, bool wide) {
    put32(b, (u32)bt->type);
    put32(b, (u32)bt->layer);
    put32(b, (u32)bt->x);
    put32(b, (u32)bt->y);
    put32(b, (u32)(bt->x + bt->w));
    put32(b, (u32)(bt->y + bt->h));
    put32(b, bt->mode);
    putname(b, bt->bound && bt->sc ? bt->sc->name : NULL);
    put32(b, bt->bound ? (u32)bt->label : 0xffffffffu);
    put32(b, bt->call ? 1 : 0);
    if (wide) {
        put32(b, 0xffffu);
        put32(b, 0xffff0001u);
        put32(b, 0xffffffffu);
        for (int k = 0; k < 3; k++) put32(b, 0);
        for (int r = 0; r < WIDE_RECORDS; r++)
            for (int k = 0; k < 9; k++) put32(b, k == 5 ? 0xffffffffu : 0);
        for (int k = 0; k < 5; k++) put32(b, 0);
    } else {
        put32(b, 0xffffffffu);
        for (int k = 0; k < 7; k++) put32(b, bt->raw[k]);
    }
    for (int k = 0; k < 4; k++) put32(b, (u32)bt->state[k].img);
    for (int k = 0; k < 4; k++) {
        put32(b, (u32)bt->state[k].sx);
        put32(b, (u32)bt->state[k].sy);
    }
    put(b, bt->stateFlag, 4);
    if (bt->type == 3 || bt->type == 4) put32(b, (u32)bt->var);
}

static void get_button(IK *t, Rd *r, Button *bt, u32 type, bool wide) {
    memset(bt, 0, sizeof *bt);
    bt->made = true;
    bt->shown = true;
    bt->type = (int)type;
    bt->layer = (int)get32(r);
    bt->x = (int)get32(r);
    bt->y = (int)get32(r);
    bt->w = (int)get32(r) - bt->x;
    bt->h = (int)get32(r) - bt->y;
    bt->mode = (u8)get32(r);
    bt->enabled = bt->mode != 0;
    char name[16];
    getname(r, name);
    u32 label = get32(r);
    bt->call = get32(r) != 0;
    if (wide) {
        if (r->at + WIDE_MIDDLE > r->n) r->bad = true;
        else r->at += WIDE_MIDDLE;
    } else {
        get32(r);
        for (int k = 0; k < 7; k++) bt->raw[k] = get32(r);
    }
    for (int k = 0; k < 4; k++) bt->state[k].img = (int)get32(r);
    for (int k = 0; k < 4; k++) {
        bt->state[k].sx = (int)get32(r);
        bt->state[k].sy = (int)get32(r);
    }
    if (r->at + 4 <= r->n) {
        memcpy(bt->stateFlag, r->p + r->at, 4);
        r->at += 4;
    } else r->bad = true;
    bt->var = type == 3 || type == 4 ? (int)get32(r) : -1;
    if (name[0] && label != 0xffffffffu) {
        bt->sc = script_load(t, name);
        bt->label = (int)label;
        bt->bound = bt->sc != NULL;
    }
}

static void snapshot(IK *t) {
    Buf b = {0};
    u8 head[THUMB_AT] = {0};
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    head[4] = (u8)tm.tm_mday;
    head[5] = (u8)(tm.tm_mon + 1);
    wr16(head + 6, (u16)(tm.tm_year + 1900));
    head[8] = (u8)tm.tm_sec;
    head[9] = (u8)tm.tm_min;
    head[10] = (u8)tm.tm_hour;
    int len = t->msg.rawLen < TEXT_END - TEXT_AT - 1 ? t->msg.rawLen : TEXT_END - TEXT_AT - 1;
    if (t->msg.raw && len > 0) memcpy(head + TEXT_AT, t->msg.raw, (size_t)len);
    wr32(head + TEXT_END, THUMB_W);
    wr32(head + TEXT_END + 4, THUMB_H);
    put(&b, head, sizeof head);
    u8 *thumb = ik_alloc(THUMB_W * THUMB_H * 3);
    thumbnail(t, thumb);
    put(&b, thumb, THUMB_W * THUMB_H * 3);
    free(thumb);
    putname(&b, t->resumeSc ? t->resumeSc->name : NULL);
    put32(&b, (u32)t->resumeLabel);
    put32(&b, (u32)(t->msgMode & 0xffff) | (t->msg.count ? 0x10000u : 0));
    put32(&b, 0);
    put32(&b, 0);
    putname(&b, t->bgmName);
    put32(&b, t->bgmLoop ? 1 : 0);
    put32(&b, (u32)t->flagCount * 8);
    if (t->flagCount) put(&b, t->flags, (size_t)t->flagCount);
    put32(&b, (u32)t->varCount);
    for (int i = 0; i < t->varCount; i++) put32(&b, (u32)t->vars[i]);
    bool wide = t->automap.bits != NULL;
    if (wide) {
        put32(&b, (u32)t->automap.w);
        put32(&b, (u32)t->automap.h);
        put32(&b, (u32)t->automap.n);
        put32(&b, (u32)t->automap.size);
        put(&b, t->automap.bits, t->automap.size);
    }
    put32(&b, (u32)t->imageCount);
    for (int i = 0; i < t->imageCount; i++) putname(&b, t->images[i] ? t->imageNames[i] : NULL);
    put32(&b, (u32)t->layerCount);
    for (int i = 0; i < t->layerCount; i++) put32(&b, (u32)i);
    for (int i = 0; i < t->layerCount; i++) put_layer(&b, &t->layers[i]);
    put32(&b, (u32)t->buttonCount);
    putname(&b, t->cancelBound && t->cancelSc ? t->cancelSc->name : NULL);
    put32(&b, t->cancelBound ? (u32)t->cancelLabel : 0xffffffffu);
    put32(&b, t->cancelCall ? 1 : 0);
    putname(&b, t->wheelBound && t->wheelSc ? t->wheelSc->name : NULL);
    put32(&b, t->wheelBound ? (u32)t->wheelLabel : 0xffffffffu);
    put32(&b, (t->wheelCall ? 1u : 0u) | (t->wheelBound ? 0x10000u : 0));
    put32(&b, (u32)t->wheelVar);
    for (int i = 0; i < t->buttonCount; i++) {
        if (t->buttons[i].made) put_button(&b, &t->buttons[i], wide);
        else put32(&b, 0xffffffffu);
    }
    if (wide) {
        u8 g[GAN_STATE];
        gan_save(t, g);
        put(&b, g, GAN_STATE);
    }
    wr32(b.p, (u32)b.n);
    free(t->save.snap);
    t->save.snap = b.p;
    t->save.snapSize = b.n;
    t->save.haveSnap = true;
}

static void save_slot(IK *t, int slot) {
    if (slot < 0 || slot >= t->save.slots) return;
    if (!t->save.haveSnap) snapshot(t);
    size_t n;
    u8 *d = read_save(t, &n);
    store_block(&d, &n, TABLE + 4 * (u32)slot, t->save.snap, t->save.snapSize);
    wr32(d + 0x10, (u32)slot);
    write_save(t, d, n);
    free(d);
}

static bool load_slot(IK *t, int slot) {
    if (slot < 0 || slot >= t->save.slots) return false;
    size_t n;
    u8 *d = read_save(t, &n);
    u32 at = rd32(d + TABLE + 4 * (u32)slot);
    if (!at || at + STATE_AT > n) {
        free(d);
        return false;
    }
    u32 size = rd32(d + at);
    if (at + size > n || size < STATE_AT) {
        free(d);
        return false;
    }
    Rd r = {d + at + STATE_AT, size - STATE_AT, 0, false};
    char name[16], bgm[16];
    getname(&r, name);
    int label = (int)get32(&r);
    u32 mode = get32(&r);
    get32(&r);
    get32(&r);
    getname(&r, bgm);
    bool loop = get32(&r) != 0;
    u32 bits = get32(&r);
    if ((int)((bits + 7) / 8) != t->flagCount || r.at + t->flagCount > r.n) {
        ik_log(t, "save slot %d: flag count %u", slot, bits);
        free(d);
        return false;
    }
    memcpy(t->flags, r.p + r.at, (size_t)t->flagCount);
    r.at += (size_t)t->flagCount;
    u32 vc = get32(&r);
    for (u32 i = 0; i < vc; i++) {
        u32 v = get32(&r);
        if (i < (u32)t->varCount) t->vars[i] = (i32)v;
    }
    bool wide = t->automap.bits != NULL;
    if (wide) {
        get32(&r);
        get32(&r);
        get32(&r);
        u32 size = get32(&r);
        if (r.at + size > r.n) r.bad = true;
        else {
            memcpy(t->automap.bits, r.p + r.at, size < t->automap.size ? size : t->automap.size);
            r.at += size;
        }
    }
    u32 ic = get32(&r);
    for (u32 i = 0; i < ic; i++) {
        char img[16];
        getname(&r, img);
        if (i >= (u32)t->imageCount) continue;
        if (strcmp(img, t->imageNames[i]) || !t->images[i]) {
            image_free(t->images[i]);
            t->images[i] = NULL;
            t->imageNames[i][0] = 0;
            if (img[0]) image_load(t, (int)i, img);
        }
    }
    u32 lc = get32(&r);
    for (u32 i = 0; i < lc; i++) get32(&r);
    for (u32 i = 0; i < lc; i++) {
        if (i < (u32)t->layerCount) get_layer(t, &r, &t->layers[i]);
        else
            for (int k = 0; k < 16; k++) get32(&r);
    }
    u32 bc = get32(&r);
    char cname[16];
    getname(&r, cname);
    u32 clabel = get32(&r);
    t->cancelCall = get32(&r) != 0;
    t->cancelSc = cname[0] ? script_load(t, cname) : NULL;
    t->cancelBound = t->cancelSc && clabel != 0xffffffffu;
    t->cancelLabel = (int)clabel;
    char wname[16];
    getname(&r, wname);
    u32 wlabel = get32(&r);
    u32 wflags = get32(&r);
    t->wheelVar = (int)get32(&r);
    t->wheelSc = wname[0] ? script_load(t, wname) : NULL;
    t->wheelBound = t->wheelSc && wlabel != 0xffffffffu;
    t->wheelLabel = (int)wlabel;
    t->wheelCall = wflags & 1;
    for (u32 i = 0; i < bc && !r.bad; i++) {
        u32 type = get32(&r);
        if (type == 0xffffffffu) {
            if (i < IK_BUTTONS) memset(&t->buttons[i], 0, sizeof(Button));
            continue;
        }
        Button tmp;
        get_button(t, &r, &tmp, type, wide);
        if (i < IK_BUTTONS) t->buttons[i] = tmp;
    }
    u8 ganState[GAN_STATE] = {0};
    if (wide && r.at + GAN_STATE <= r.n) memcpy(ganState, r.p + r.at, GAN_STATE);
    free(d);
    if (r.bad) ik_log(t, "save slot %d is shorter than its state", slot);
    t->msgMode = (int)(mode & 0xffff);
    if (wide) gan_restore(t, ganState);
    if (bgm[0]) snd_bgm(t, bgm, loop);
    else snd_bgm_fade(t, 0);
    snd_voice_stop(t);
    Script *sc = script_load(t, name);
    if (!sc) {
        ik_log(t, "save slot %d: missing script %s", slot, name);
        return false;
    }
    t->msg.tags = NULL;
    t->msg.count = t->msg.revealed = 0;
    t->msg.cursor = false;
    t->msg.name = -1;
    t->dun.on = false;
    t->dun.animating = false;
    t->hover = t->pressed = -1;
    t->sp = 0;
    t->stack[t->sp++] = (Frame){sc, label >= 0 && label < sc->labelCount ? sc->labels[label] : 0, false};
    t->resumeSc = sc;
    t->resumeLabel = label;
    t->wait = W_NONE;
    vm_start(t, "LOAD.SNR");
    t->dirty = true;
    return true;
}

static void slot_info(IK *t, int first, int count, int img, int dateBase, int msgBase, int flagBase) {
    size_t n;
    u8 *d = read_save(t, &n);
    Image *thumbs = img >= 0 && img < t->imageCount ? t->images[img] : NULL;
    for (int i = 0; i < count; i++) {
        int slot = first + i;
        u32 at = slot >= 0 && slot < t->save.slots ? rd32(d + TABLE + 4 * (u32)slot) : 0;
        bool ok = at && at + STATE_AT <= n;
        flag_put(t, flagBase + i, ok);
        if (!ok) {
            vm_set_text(t, dateBase + i, "", 0);
            vm_set_text(t, msgBase + i, "", 0);
            continue;
        }
        const u8 *s = d + at;
        char date[32];
        int len = snprintf(date, sizeof date, "%02u/%02u/%04u", s[5], s[4], rd16(s + 6));
        vm_set_text(t, dateBase + i, date, len);
        vm_set_text(t, msgBase + i, (const char *)s + TEXT_AT, (int)strnlen((const char *)s + TEXT_AT, TEXT_END - TEXT_AT));
        if (thumbs)
            for (int y = 0; y < THUMB_H; y++)
                for (int x = 0; x < THUMB_W; x++) {
                    int ty = i * THUMB_H + y;
                    if (x >= thumbs->w || ty >= thumbs->h) continue;
                    const u8 *p = s + THUMB_AT + (y * THUMB_W + x) * 3;
                    thumbs->px[(size_t)ty * thumbs->w + x] = 0xff000000u | p[2] | (u32)p[1] << 8 | (u32)p[0] << 16;
                }
    }
    free(d);
}

static int val(IK *t, const u8 *p) { return (int)ik_value(t, rd32(p)); }

bool save_exec(IK *t, u16 op, const u8 *a, int n) {
    switch (op) {
    case 0x0a00: {
        char name[16];
        int k = 0;
        while (k < n && k < 15 && a[k]) {
            name[k] = (char)a[k];
            k++;
        }
        name[k] = 0;
        t->save.slots = 50;
        if (n >= NAME_LEN + 4) t->save.slots = (int)rd32(a + n - 4);
        free(t->save.path);
        t->save.path = ik_fmt("%s/%s", t->saveDir, name);
        if (!ik_file_exists(t->save.path)) {
            char *orig = ik_fmt("%s/%s", t->gameDir, name);
            size_t sz;
            u8 *d = ik_read_file(orig, &sz);
            if (d && sz >= HEAD_SIZE) write_save(t, d, sz);
            free(d);
            free(orig);
        }
        break;
    }
    case 0x0a01: save_settings(t); break;
    case 0x0a02: load_settings(t); break;
    case 0x0a03:
        if (n >= 16) save_global(t, val(t, a), val(t, a + 4), val(t, a + 8), val(t, a + 12));
        break;
    case 0x0a04: load_global(t); break;
    case 0x0a05: {
        snapshot(t);
        int slot = val(t, a), w = val(t, a + 4), h = val(t, a + 8);
        if (slot >= 0 && slot < t->imageCount && w > 0 && h > 0) {
            image_free(t->images[slot]);
            Image *im = image_new(w, h);
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    int sx = x * THUMB_W / w, sy = y * THUMB_H / h;
                    const u8 *p = t->save.snap + THUMB_AT + (sy * THUMB_W + sx) * 3;
                    im->px[(size_t)y * w + x] = 0xff000000u | p[2] | (u32)p[1] << 8 | (u32)p[0] << 16;
                }
            t->images[slot] = im;
            t->imageNames[slot][0] = 0;
        }
        break;
    }
    case 0x0a06: save_slot(t, val(t, a)); break;
    case 0x0a07:
        if (!load_slot(t, val(t, a))) ik_log(t, "can't load save slot %d", val(t, a));
        break;
    case 0x0a08:
        if (n >= 24) slot_info(t, val(t, a), val(t, a + 4), val(t, a + 8), val(t, a + 12), val(t, a + 16), val(t, a + 20));
        break;
    case 0x0a09: {
        size_t sz;
        u8 *d = read_save(t, &sz);
        u32 v = rd32(a);
        if (v < (u32)t->varCount) t->vars[v] = (i32)rd32(d + 0x10);
        free(d);
        break;
    }
    case 0x0a0c: {
        int base = val(t, a);
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        char date[32];
        int len = snprintf(date, sizeof date, "%02d/%02d/%04d", tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900);
        vm_set_text(t, base, date, len);
        vm_set_text(t, base + 1, t->msg.raw ? t->msg.raw : "", t->msg.raw ? t->msg.rawLen : 0);
        break;
    }
    case 0x0a0d: t->save.haveSnap = false; break;
    case 0x0a0a:
    case 0x0a0b: break;
    default: return false;
    }
    return true;
}

void save_close(IK *t) {
    free(t->save.path);
    free(t->save.snap);
}

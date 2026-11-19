#include "br.h"

static const i32 defaults[LP_COUNT] = {
    [LP_SWITCH] = 80, [LP_IW] = 1024, [LP_IH] = 1024, [LP_TX] = -1, [LP_MSYS] = 1, [LP_MSWITCH] = 1622,
    [LP_MIW] = 1024, [LP_MIH] = 1024, [LP_MTX] = -1, [LP_R] = 255, [LP_G] = 255, [LP_B] = 255, [LP_A] = 255,
};

Layer *layer_find(BR *e, int no) {
    for (int i = 0; i < e->layerCount; i++)
        if (e->layers[i]->no == no) return e->layers[i];
    return NULL;
}

Pic *pic_find(BR *e, int no) {
    for (int i = 0; i < e->picCount; i++)
        if (e->pics[i]->no == no) return e->pics[i];
    return NULL;
}

static int cmp_layer(const void *a, const void *b) {
    const Layer *x = *(Layer *const *)a, *y = *(Layer *const *)b;
    if (x->order != y->order) return (x->order > y->order) - (x->order < y->order);
    return (x->no > y->no) - (x->no < y->no);
}

void layers_sort(BR *e) { qsort(e->layers, (size_t)e->layerCount, sizeof *e->layers, cmp_layer); }

Layer *layer_new(BR *e, int no, int mode) {
    Layer *l = layer_find(e, no);
    if (l) {
        l->mode = mode;
        return l;
    }
    if (e->layerCount == e->layerCap) {
        e->layerCap = e->layerCap ? e->layerCap * 2 : 64;
        e->layers = br_realloc(e->layers, sizeof(Layer *) * (size_t)e->layerCap);
    }
    l = br_alloc(sizeof *l);
    l->no = no;
    l->order = no;
    l->mode = mode;
    l->group = -1;
    l->visible = true;
    l->pic = -1;
    l->tmParts = -1;
    l->tmRegion = -2;
    memcpy(l->v, defaults, sizeof l->v);
    memcpy(l->from, defaults, sizeof l->from);
    e->layers[e->layerCount++] = l;
    layers_sort(e);
    return l;
}

static void pic_free(Pic *p) {
    image_free(p->img);
    stream_free(p->stream);
    free(p->path);
    free(p);
}

static void pic_remove(BR *e, int i) {
    pic_free(e->pics[i]);
    memmove(&e->pics[i], &e->pics[i + 1], sizeof(Pic *) * (size_t)(e->picCount - i - 1));
    e->picCount--;
}

void layers_clear_pics(BR *e, int layer) {
    for (int i = e->picCount - 1; i >= 0; i--)
        if (e->pics[i]->layer == layer) pic_remove(e, i);
    Layer *l = layer_find(e, layer);
    if (l) l->pic = -1;
}

void layers_reset(BR *e) {
    for (int i = 0; i < e->layerCount; i++) {
        free(e->layers[i]->tmCursor);
        free(e->layers[i]);
    }
    for (int i = 0; i < e->picCount; i++) pic_free(e->pics[i]);
    e->layerCount = e->picCount = 0;
}

void layers_free(BR *e) {
    layers_reset(e);
    free(e->layers);
    free(e->pics);
    e->layers = NULL;
    e->pics = NULL;
    e->layerCap = e->picCap = 0;
    image_free(e->transFrom);
    image_free(e->frozenImg);
    e->transFrom = e->frozenImg = NULL;
    e->frozen = false;
}

void pic_reload(BR *e, Pic *p) {
    image_free(p->img);
    p->img = NULL;
    stream_free(p->stream);
    p->stream = NULL;
    if (p->type == 2) p->stream = stream_open(e, p->url, 1, e->now);
    else if (p->path) p->img = image_load(e, p->path);
    else p->img = image_storage(e, p->url);
}

Pic *pic_new(BR *e, int layer, int no, int url, const char *path, int type, int numsel) {
    Pic *p = pic_find(e, no);
    if (!p) {
        if (e->picCount == e->picCap) {
            e->picCap = e->picCap ? e->picCap * 2 : 64;
            e->pics = br_realloc(e->pics, sizeof(Pic *) * (size_t)e->picCap);
        }
        p = br_alloc(sizeof *p);
        p->no = no;
        e->pics[e->picCount++] = p;
    }
    p->layer = layer;
    p->url = url;
    char *old = p->path;
    p->path = path ? br_strdup(path) : NULL;
    free(old);
    p->type = type;
    p->numsel = numsel;
    pic_reload(e, p);
    return p;
}

int layers_load(BR *e, int layer, int no, Node *src, int type, int numsel) {
    Layer *l = layer_find(e, layer);
    if (!l) l = layer_new(e, layer, 1);
    const char *path = src && src->t == N_STR ? src->s : NULL;
    int url = src && src->t == N_INT ? src->i : -1;
    pic_new(e, layer, no, url, path, type, numsel);
    return 0;
}

void layers_visible(BR *e, int no, bool on) {
    if (no < 0) {
        for (int i = 0; i < e->layerCount; i++) e->layers[i]->visible = on;
        return;
    }
    Layer *l = layer_find(e, no);
    if (l) l->visible = on;
}

static bool tweened(int i) { return i != LP_SWITCH && i != LP_MSYS && i != LP_MSWITCH; }

static float cur(BR *e, Layer *l, int i) {
    if (!l->dur || e->now >= l->start + l->dur || !tweened(i)) return (float)l->v[i];
    float t = (float)(e->now - l->start) / (float)l->dur;
    if (l->ease == 2) t = l->easeDir == 1 ? t * t : l->easeDir == 2 ? 1 - (1 - t) * (1 - t) : t * t * (3 - 2 * t);
    return (float)l->from[i] + (float)(l->v[i] - l->from[i]) * t;
}

void layers_set(BR *e, int no, const i32 *a, int count) {
    Layer *l = layer_find(e, no);
    if (!l) l = layer_new(e, no, 1);
    i32 v[31] = {0};
    for (int i = 0; i < count && i < 31; i++) v[i] = a[i];
    int mask = v[7];
    for (int i = 0; i < LP_COUNT; i++) l->from[i] = (i32)cur(e, l, i);
    static const struct {
        int bit, lo, hi;
    } groups[] = {{1, LP_SWITCH, LP_SWITCH}, {2, LP_IX, LP_IY}, {4, LP_IW, LP_IH}, {8, LP_TX, LP_TH}, {16, LP_MSYS, LP_MTH}, {0x800, LP_R, LP_A}};
    for (size_t g = 0; g < sizeof groups / sizeof *groups; g++)
        if (mask & groups[g].bit)
            for (int i = groups[g].lo; i <= groups[g].hi; i++) l->v[i] = v[i];
    if (v[3] == 201 && v[4] > 0) {
        l->start = e->now;
        l->dur = (u32)v[4];
        l->ease = v[5];
        l->easeDir = v[6];
    } else {
        l->dur = 0;
        memcpy(l->from, l->v, sizeof l->v);
    }
}

bool layers_busy(BR *e) {
    for (int i = 0; i < e->layerCount; i++) {
        Layer *l = e->layers[i];
        if (l->dur && e->now < l->start + l->dur) return true;
    }
    return e->transFrom && e->now < e->transStart + e->transDur;
}

static Image *layer_image(BR *e, Layer *l) {
    Pic *p = l->pic >= 0 ? pic_find(e, l->pic) : NULL;
    if (!p) return NULL;
    return p->stream ? stream_frame(e, p->stream, e->now) : p->img;
}

static void draw_layer(BR *e, u32 *dst, Layer *l) {
    if (!l->visible) return;
    Image *im = layer_image(e, l);
    if (!im) return;
    int align = l->v[LP_SWITCH];
    float sx = cur(e, l, LP_IW), sy = cur(e, l, LP_IH);
    int cx = (int)cur(e, l, LP_TX), cy = (int)cur(e, l, LP_TY), cw = (int)cur(e, l, LP_TW), ch = (int)cur(e, l, LP_TH);
    if (l->v[LP_TX] < 0) {
        cx = cy = 0;
        cw = im->w;
        ch = im->h;
    }
    float dw, dh;
    if (align & 0x50) {
        dw = (float)cw * sx / 1024.0f;
        dh = (float)ch * sy / 1024.0f;
    } else {
        dw = sx;
        dh = sy;
    }
    float rx = align & 1 ? BR_W : align & 2 ? BR_W / 2.0f : 0;
    float ry = align & 4 ? BR_H : align & 8 ? BR_H / 2.0f : 0;
    float px = align & 0x100 ? dw : align & 0x200 ? dw / 2 : 0;
    float py = align & 0x400 ? dh : align & 0x800 ? dh / 2 : 0;
    float x = rx + cur(e, l, LP_IX) - px, y = ry + cur(e, l, LP_IY) - py;
    if (l->shakeDur && e->now < l->shakeStart + l->shakeDur) {
        u32 t = e->now - l->shakeStart;
        float k = 1.0f - (float)t / (float)l->shakeDur;
        int phase = (int)(t / 33);
        x += (float)((phase & 1) ? l->shakeX : -l->shakeX) * k;
        y += (float)((phase & 2) ? l->shakeY : -l->shakeY) * k;
    }
    int a = (int)cur(e, l, LP_A), r = (int)cur(e, l, LP_R), g = (int)cur(e, l, LP_G), b = (int)cur(e, l, LP_B);
    if (a <= 0) return;
    int ox = (int)(x + 0.5f), oy = (int)(y + 0.5f), ow = (int)(dw + 0.5f), oh = (int)(dh + 0.5f);
    if (r >= 255 && g >= 255 && b >= 255) blit_scaled(dst, BR_W, BR_H, im, cx, cy, cw, ch, ox, oy, ow, oh, a);
    else blit_tinted(dst, BR_W, BR_H, im, cx, cy, cw, ch, ox, oy, ow, oh, a, (u32)r | (u32)g << 8 | (u32)b << 16);
}

static void draw_all(BR *e, u32 *dst) {
    for (int i = e->layerCount - 1; i >= 0; i--) draw_layer(e, dst, e->layers[i]);
}

static Image *snapshot(BR *e) {
    Image *im = image_new(BR_W, BR_H);
    for (int i = 0; i < BR_W * BR_H; i++) im->px[i] = 0xff000000u;
    draw_all(e, im->px);
    return im;
}

void layers_draw(BR *e, u32 *dst) {
    if (e->frozen && e->frozenImg) {
        blit(dst, BR_W, BR_H, e->frozenImg, 0, 0, -1, -1, 0, 0, 255);
        return;
    }
    draw_all(e, dst);
    if (e->transFrom && e->now < e->transStart + e->transDur) {
        int a = 255 - (int)((e->now - e->transStart) * 255 / e->transDur);
        blit(dst, BR_W, BR_H, e->transFrom, 0, 0, -1, -1, 0, 0, a);
    } else if (e->transFrom) {
        image_free(e->transFrom);
        e->transFrom = NULL;
    }
}

void layers_freeze(BR *e) {
    if (!e->frozen) {
        image_free(e->frozenImg);
        e->frozenImg = snapshot(e);
        e->frozen = true;
    }
    layers_reset(e);
}

void layers_delete(BR *e, int a, int b) {
    if (b < a) b = a;
    for (int i = e->layerCount - 1; i >= 0; i--) {
        Layer *l = e->layers[i];
        if (l->no < a || l->no > b) continue;
        for (int k = e->picCount - 1; k >= 0; k--)
            if (e->pics[k]->layer == l->no) pic_remove(e, k);
        free(l->tmCursor);
        free(l);
        memmove(&e->layers[i], &e->layers[i + 1], sizeof(Layer *) * (size_t)(e->layerCount - i - 1));
        e->layerCount--;
    }
}

void layers_transition(BR *e, int type, int ms) {
    Image *from = e->frozen ? e->frozenImg : NULL;
    e->frozenImg = NULL;
    if (!from && ms > 0 && type) from = snapshot(e);
    e->frozen = false;
    image_free(e->transFrom);
    e->transFrom = NULL;
    if (ms <= 0 || !type || !from) {
        image_free(from);
        return;
    }
    e->transFrom = from;
    e->transStart = e->now;
    e->transDur = (u32)ms;
}

void pics_activate(BR *e, int a, int b) {
    if (b < a) b = a;
    for (int i = 0; i < e->picCount; i++) {
        Pic *p = e->pics[i];
        if (p->no < a || p->no > b) continue;
        Layer *l = layer_find(e, p->layer);
        if (l) l->pic = p->no;
        if (p->stream) stream_restart(p->stream, e->now);
    }
}

void pics_delete(BR *e, int a, int b) {
    if (b < a) b = a;
    for (int i = e->picCount - 1; i >= 0; i--) {
        Pic *p = e->pics[i];
        if (p->no < a || p->no > b) continue;
        Layer *l = layer_find(e, p->layer);
        if (l && l->pic == p->no) l->pic = -1;
        pic_remove(e, i);
    }
}

void pic_stream_restart(BR *e, int no) {
    Pic *p = pic_find(e, no);
    if (p && p->stream) stream_restart(p->stream, e->now);
}

void layer_order(BR *e, int no, int order) {
    Layer *l = layer_find(e, no);
    if (!l) return;
    l->order = order;
    layers_sort(e);
}

void layer_shake(BR *e, int no, int ms, int ax, int ay) {
    Layer *l = layer_find(e, no);
    if (!l) return;
    l->shakeStart = e->now;
    l->shakeDur = (u32)(ms > 0 ? ms : 0);
    l->shakeX = ax;
    l->shakeY = ay;
}

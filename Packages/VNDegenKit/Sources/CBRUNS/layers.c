#include "br.h"

enum { T_ALIGN, T_X, T_Y, T_SX, T_SY, T_CX, T_CY, T_CW, T_CH, T_R, T_G, T_B, T_A, T_COUNT };

static LNode *node_find(BR *e, int id) {
    for (int i = 0; i < e->nodeCount; i++)
        if (e->nodes[i]->id == id) return e->nodes[i];
    return NULL;
}

static void node_defaults(LNode *n) {
    n->v[T_ALIGN] = 80;
    n->v[T_SX] = n->v[T_SY] = 1024;
    n->v[T_CX] = -1;
    n->v[T_R] = n->v[T_G] = n->v[T_B] = n->v[T_A] = 255;
    for (int i = 0; i < T_COUNT; i++) n->from[i] = n->v[i];
    n->visible = true;
}

static void node_free(LNode *n) {
    image_free(n->img);
    stream_free(n->stream);
    free(n);
}

static int cmp_node(const void *a, const void *b) {
    const LNode *x = *(LNode *const *)a, *y = *(LNode *const *)b;
    return (x->id > y->id) - (x->id < y->id);
}

static LNode *node_new(BR *e, int id, int parent) {
    LNode *n = node_find(e, id);
    if (n) {
        n->parent = parent;
        return n;
    }
    if (e->nodeCount == e->nodeCap) {
        e->nodeCap = e->nodeCap ? e->nodeCap * 2 : 64;
        e->nodes = br_realloc(e->nodes, sizeof(LNode *) * (size_t)e->nodeCap);
    }
    n = br_alloc(sizeof *n);
    n->id = id;
    n->parent = parent;
    node_defaults(n);
    e->nodes[e->nodeCount++] = n;
    qsort(e->nodes, (size_t)e->nodeCount, sizeof *e->nodes, cmp_node);
    return n;
}

static void node_remove_at(BR *e, int i) {
    node_free(e->nodes[i]);
    memmove(&e->nodes[i], &e->nodes[i + 1], sizeof(LNode *) * (size_t)(e->nodeCount - i - 1));
    e->nodeCount--;
}

void layers_clear_children(BR *e, int parent) {
    for (int i = e->nodeCount - 1; i >= 0; i--) {
        LNode *n = e->nodes[i];
        if (n->parent == parent && n->id != parent) {
            layers_clear_children(e, n->id);
            for (int k = 0; k < e->nodeCount; k++)
                if (e->nodes[k] == n) {
                    node_remove_at(e, k);
                    break;
                }
            if (i > e->nodeCount) i = e->nodeCount;
        }
    }
}

void layers_reset(BR *e) {
    for (int i = 0; i < e->nodeCount; i++) node_free(e->nodes[i]);
    e->nodeCount = 0;
}

void layers_free(BR *e) {
    layers_reset(e);
    free(e->nodes);
    e->nodes = NULL;
    e->nodeCap = 0;
    image_free(e->transFrom);
    image_free(e->frozenImg);
    e->transFrom = e->frozenImg = NULL;
    e->frozen = false;
}

void layers_create(BR *e, int id, int parent) { node_new(e, id, parent); }

int layers_load(BR *e, int parent, int child, Node *src, int mode, int loop) {
    if (!node_find(e, parent)) node_new(e, parent, 1);
    LNode *n = node_new(e, child, parent);
    image_free(n->img);
    n->img = NULL;
    stream_free(n->stream);
    n->stream = NULL;
    if (mode == 2) {
        n->stream = stream_open(e, src && src->t == N_INT ? src->i : -1, loop, e->now);
        return 0;
    }
    if (src && src->t == N_STR) n->img = image_load(e, src->s);
    else if (src) n->img = image_storage(e, src->i);
    return 0;
}

void layers_visible(BR *e, int id, bool on) {
    LNode *n = node_find(e, id);
    if (n) n->visible = on;
}

static float cur(BR *e, LNode *n, int i) {
    if (!n->dur || e->now >= n->start + n->dur) return (float)n->v[i];
    float t = (float)(e->now - n->start) / (float)n->dur;
    if (n->ease == 2) t = n->easeDir == 1 ? t * t : n->easeDir == 2 ? 1 - (1 - t) * (1 - t) : t * t * (3 - 2 * t);
    return (float)n->from[i] + (float)(n->v[i] - n->from[i]) * t;
}

void layers_set(BR *e, int id, const i32 *a, int count) {
    LNode *n = node_find(e, id);
    if (!n) n = node_new(e, id, 1);
    i32 v[31] = {0};
    for (int i = 0; i < count && i < 31; i++) v[i] = a[i];
    int mask = v[7];
    float now[T_COUNT];
    for (int i = 0; i < T_COUNT; i++) now[i] = cur(e, n, i);
    for (int i = 0; i < T_COUNT; i++) n->from[i] = (i32)now[i];
    if (mask & 1) n->v[T_ALIGN] = v[8];
    if (mask & 2) {
        n->v[T_X] = v[9];
        n->v[T_Y] = v[10];
    }
    if (mask & 4) {
        n->v[T_SX] = v[11];
        n->v[T_SY] = v[12];
    }
    if (mask & 8) {
        n->v[T_CX] = v[13];
        n->v[T_CY] = v[14];
        n->v[T_CW] = v[15];
        n->v[T_CH] = v[16];
    }
    if (mask & 0x800) {
        n->v[T_R] = v[27];
        n->v[T_G] = v[28];
        n->v[T_B] = v[29];
        n->v[T_A] = v[30];
    }
    n->from[T_ALIGN] = n->v[T_ALIGN];
    if (v[3] == 201 && v[4] > 0) {
        n->start = e->now;
        n->dur = (u32)v[4];
        n->ease = v[5];
        n->easeDir = v[6];
    } else n->dur = 0;
}

bool layers_busy(BR *e) {
    for (int i = 0; i < e->nodeCount; i++) {
        LNode *n = e->nodes[i];
        if (n->dur && e->now < n->start + n->dur) return true;
    }
    return e->transFrom && e->now < e->transStart + e->transDur;
}

static Image *node_image(BR *e, LNode *n) { return n->stream ? stream_frame(e, n->stream, e->now) : n->img; }

static void node_size(BR *e, LNode *n, int *w, int *h) {
    Image *im = node_image(e, n);
    if (im) {
        *w = im->w;
        *h = im->h;
        return;
    }
    for (int i = 0; i < e->nodeCount; i++) {
        LNode *c = e->nodes[i];
        if (c->parent != n->id || c == n) continue;
        Image *ci = node_image(e, c);
        if (ci) {
            *w = ci->w;
            *h = ci->h;
            return;
        }
    }
    *w = BR_W;
    *h = BR_H;
}

typedef struct Xf {
    float x, y, w, h, sx, sy;
    int a, r, g, b;
} Xf;

static void draw_node(BR *e, u32 *dst, LNode *n, Xf p, int depth) {
    if (!n->visible || depth > 8) return;
    int align = n->v[T_ALIGN];
    float sx = cur(e, n, T_SX), sy = cur(e, n, T_SY);
    int w, h;
    node_size(e, n, &w, &h);
    int cx = n->v[T_CX], cy = n->v[T_CY], cw = n->v[T_CW], ch = n->v[T_CH];
    if (cx < 0) {
        cx = cy = 0;
        cw = w;
        ch = h;
    }
    float dw, dh;
    if (align & 0x50) {
        dw = (float)cw * sx / 1024.0f * p.sx;
        dh = (float)ch * sy / 1024.0f * p.sy;
    } else {
        dw = sx * p.sx;
        dh = sy * p.sy;
    }
    float rx = p.x + (align & 1 ? p.w : align & 2 ? p.w / 2 : 0);
    float ry = p.y + (align & 4 ? p.h : align & 8 ? p.h / 2 : 0);
    float px = align & 0x100 ? dw : align & 0x200 ? dw / 2 : 0;
    float py = align & 0x400 ? dh : align & 0x800 ? dh / 2 : 0;
    Xf me;
    me.x = rx + cur(e, n, T_X) * p.sx - px;
    me.y = ry + cur(e, n, T_Y) * p.sy - py;
    me.w = dw;
    me.h = dh;
    me.sx = align & 0x50 ? p.sx * sx / 1024.0f : p.sx;
    me.sy = align & 0x50 ? p.sy * sy / 1024.0f : p.sy;
    me.a = p.a * (int)cur(e, n, T_A) / 255;
    me.r = p.r * (int)cur(e, n, T_R) / 255;
    me.g = p.g * (int)cur(e, n, T_G) / 255;
    me.b = p.b * (int)cur(e, n, T_B) / 255;
    if (me.a <= 0) return;
    Image *im = node_image(e, n);
    if (im) {
        int ox = (int)(me.x + 0.5f), oy = (int)(me.y + 0.5f), ow = (int)(dw + 0.5f), oh = (int)(dh + 0.5f);
        if (me.r == 255 && me.g == 255 && me.b == 255) blit_scaled(dst, BR_W, BR_H, im, cx, cy, cw, ch, ox, oy, ow, oh, me.a);
        else blit_tinted(dst, BR_W, BR_H, im, cx, cy, cw, ch, ox, oy, ow, oh, me.a, (u32)me.r | (u32)me.g << 8 | (u32)me.b << 16);
    }
    for (int i = e->nodeCount - 1; i >= 0; i--) {
        LNode *c = e->nodes[i];
        if (c->parent == n->id && c != n) draw_node(e, dst, c, me, depth + 1);
    }
}

static void draw_tree(BR *e, u32 *dst) {
    Xf root = {0, 0, BR_W, BR_H, 1, 1, 255, 255, 255, 255};
    for (int i = e->nodeCount - 1; i >= 0; i--) {
        LNode *n = e->nodes[i];
        if (n->parent == n->id || !node_find(e, n->parent)) draw_node(e, dst, n, root, 0);
    }
}

static Image *snapshot(BR *e) {
    Image *im = image_new(BR_W, BR_H);
    for (int i = 0; i < BR_W * BR_H; i++) im->px[i] = 0xff000000u;
    draw_tree(e, im->px);
    return im;
}

void layers_draw(BR *e, u32 *dst) {
    if (e->frozen && e->frozenImg) {
        blit(dst, BR_W, BR_H, e->frozenImg, 0, 0, -1, -1, 0, 0, 255);
        return;
    }
    draw_tree(e, dst);
    if (e->transFrom && e->now < e->transStart + e->transDur) {
        int a = 255 - (int)((e->now - e->transStart) * 255 / e->transDur);
        blit(dst, BR_W, BR_H, e->transFrom, 0, 0, -1, -1, 0, 0, a);
    } else if (e->transFrom) {
        image_free(e->transFrom);
        e->transFrom = NULL;
    }
}

void layers_freeze(BR *e) {
    if (e->frozen) return;
    image_free(e->frozenImg);
    e->frozenImg = snapshot(e);
    e->frozen = true;
}

void layers_transition(BR *e, int type, int ms) {
    Image *from = e->frozen && e->frozenImg ? e->frozenImg : NULL;
    if (from) e->frozenImg = NULL;
    else if (ms > 0 && type) from = snapshot(e);
    e->frozen = false;
    image_free(e->frozenImg);
    e->frozenImg = NULL;
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

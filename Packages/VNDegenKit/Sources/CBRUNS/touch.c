#include "br.h"

static Node *map_of(BR *e, int parts) {
    Parts *p = parts_get(e, parts);
    return p && p->type == P_TOUCHMAP ? p->map : NULL;
}

static int region_at(BR *e, Layer *l, int mx, int my) {
    Node *map = map_of(e, l->tmParts);
    if (!map) return -1;
    int x = mx - l->v[LP_IX], y = my - l->v[LP_IY];
    for (int i = 0; i < map->n; i++) {
        Node *ent = map->k[i];
        if (ent->t != N_LIST) continue;
        Node *url = sx_cdr_value(sx_find(ent, "url"));
        Node *col = sx_cdr_value(sx_find(ent, "color"));
        Node *ev = sx_cdr_value(sx_find(ent, "event"));
        if (!url || !col || col->t != N_LIST || col->n < 3) continue;
        Image *im = image_url(e, url);
        if (!im) continue;
        bool hit = false;
        if (x >= 0 && y >= 0 && x < im->w && y < im->h) {
            u32 p = im->px[(size_t)y * im->w + x];
            hit = (int)(p & 0xff) == sx_int(col->k[0], -1) && (int)(p >> 8 & 0xff) == sx_int(col->k[1], -1) && (int)(p >> 16 & 0xff) == sx_int(col->k[2], -1);
        }
        image_free(im);
        if (hit) return sx_int(ev, -1);
    }
    return -1;
}

static Layer *active_layer(BR *e) {
    for (int i = 0; i < e->layerCount; i++) {
        Layer *l = e->layers[i];
        if (l->visible && l->tmParts > 0) return l;
    }
    return NULL;
}

void touch_attach(BR *e, int no, int parts) {
    Layer *l = layer_find(e, no);
    if (!l) l = layer_new(e, no, 3);
    l->tmParts = parts;
    l->tmRegion = -2;
}

void touch_base(BR *e, int no, int mode, int base) {
    Layer *l = layer_find(e, no);
    if (!l || mode < 0 || mode >= 8) return;
    l->tmBase[mode] = base;
    l->tmHas[mode] = true;
}

void touch_cursor(BR *e, int no, int mode, const char *path) {
    Layer *l = layer_find(e, no);
    if (!l) return;
    free(l->tmCursor);
    l->tmCursor = br_strdup(path ? path : "");
    l->tmCursorMode = mode;
}

int touch_poll(BR *e, int no) {
    Layer *l = layer_find(e, no);
    if (!l) return -1;
    int r = region_at(e, l, e->mouseX, e->mouseY);
    return r;
}

int touch_position(BR *e) { return (e->mouseX & 0xffff) | (e->mouseY & 0xffff) << 16; }

bool touch_event(BR *e, int *out) {
    Layer *l = active_layer(e);
    if (!l) return false;
    int r = region_at(e, l, e->mouseX, e->mouseY);
    if (r != l->tmRegion) {
        l->tmRegion = r;
        if (r >= 0 && l->tmHas[5]) {
            snd_se_hover(e);
            *out = l->tmBase[5] + r;
            return true;
        }
    }
    if (e->lPress && r >= 0 && l->tmHas[2]) {
        *out = l->tmBase[2] + r;
        return true;
    }
    if (e->rPress && r >= 0 && l->tmHas[3]) {
        *out = l->tmBase[3] + r;
        return true;
    }
    return false;
}

void touch_auto(BR *e, int no, int mode) {
    Layer *l = layer_find(e, no);
    if (!l) return;
    l->amMode = mode;
    l->amEvent = 0;
}

int touch_take(BR *e, int no) {
    Layer *l = layer_find(e, no);
    if (!l) return 0;
    int v = l->amEvent;
    l->amEvent = 0;
    return v;
}

int touch_last_pos(BR *e, int no) {
    Layer *l = layer_find(e, no);
    return l ? l->amPos : 0;
}

void touch_tick(BR *e) {
    if (!e->lPress) return;
    for (int i = 0; i < e->layerCount; i++) {
        Layer *l = e->layers[i];
        if (!l->visible || l->tmParts <= 0 || l->amMode <= 0 || l->amMode >= 8 || !l->tmHas[l->amMode]) continue;
        int r = region_at(e, l, e->mouseX, e->mouseY);
        if (r < 0) continue;
        l->amEvent = l->tmBase[l->amMode] + r;
        l->amPos = touch_position(e);
        break;
    }
}

#include "ikura.h"

#include <math.h>

#define MAP_W 81
#define FOCAL 400.0f
#define CX 400.0f
#define CY 300.0f
#define EYE 0.375f
#define WALL_H 0.75f
#define BACK 0.0f
#define AMBIENT 0.0519f
#define LIGHT_D0 0.6891f
#define LIGHT_POW 2.506f
#define STEP_MS 500
#define LUT_SCALE 64.0f
#define LUT_SIZE 4096

enum { T_FLOOR, T_CEIL, T_WALL, T_DOOR };

static const int dirX[4] = {0, 1, 0, -1}, dirZ[4] = {-1, 0, 1, 0};

static u8 grid(const Dungeon *d, int x, int y) {
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_W) return 1;
    return d->map[y * MAP_W + x];
}

static u8 edge(const Dungeon *d, int cx, int cz, int side) {
    switch (side) {
    case 0: return grid(d, 2 * cx + 1, 2 * cz);
    case 1: return grid(d, 2 * cx + 2, 2 * cz + 1);
    case 2: return grid(d, 2 * cx + 1, 2 * cz + 2);
    default: return grid(d, 2 * cx, 2 * cz + 1);
    }
}

static bool solid(u8 v) { return v == 1 || v == 2 || v == 4; }

static bool blocks(u8 v) { return v == 1 || v == 4; }

static void build_lut(Dungeon *d) {
    for (int i = 0; i < LUT_SIZE; i++) {
        float dist = sqrtf((float)i / LUT_SCALE);
        float a = dist > 0 ? powf(LIGHT_D0 / dist, LIGHT_POW) : 1;
        if (a > 1) a = 1;
        d->light[i] = (u16)((AMBIENT + (1 - AMBIENT) * a) * 256.0f + 0.5f);
    }
}

static inline u32 sample(const Image *im, float u, float v, u32 light) {
    if (!im) return 0;
    float fu = (u - floorf(u)) * (float)im->w - 0.5f, fv = (v - floorf(v)) * (float)im->h - 0.5f;
    int x0 = (int)floorf(fu), y0 = (int)floorf(fv);
    u32 wx = (u32)((fu - (float)x0) * 256.0f), wy = (u32)((fv - (float)y0) * 256.0f);
    int x1 = x0 + 1, y1 = y0 + 1;
    x0 = (x0 % im->w + im->w) % im->w;
    x1 = (x1 % im->w + im->w) % im->w;
    y0 = (y0 % im->h + im->h) % im->h;
    y1 = (y1 % im->h + im->h) % im->h;
    u32 p00 = im->px[y0 * im->w + x0], p10 = im->px[y0 * im->w + x1], p01 = im->px[y1 * im->w + x0],
        p11 = im->px[y1 * im->w + x1];
    u32 out = 0xff000000u;
    for (int s = 0; s < 24; s += 8) {
        u32 a = (p00 >> s & 0xff) * (256 - wx) + (p10 >> s & 0xff) * wx;
        u32 b = (p01 >> s & 0xff) * (256 - wx) + (p11 >> s & 0xff) * wx;
        u32 c = (a * (256 - wy) + b * wy) >> 16;
        c = c * light >> 8;
        out |= (c > 255 ? 255 : c) << s;
    }
    return out;
}

static u32 lit(const Dungeon *d, float dist2) {
    int i = (int)(dist2 * LUT_SCALE);
    return d->light[i < 0 ? 0 : i >= LUT_SIZE ? LUT_SIZE - 1 : i];
}

static void pose(IK *t, float *yaw, float *px, float *pz) {
    Dungeon *d = &t->dun;
    float y = (float)d->dir * (float)M_PI / 2, x = (float)d->x, z = (float)d->y;
    if (d->animating) {
        u32 el = t->now - d->animStart;
        float k = el >= STEP_MS ? 1.0f : (float)el / STEP_MS;
        y = d->fromYaw + (d->toYaw - d->fromYaw) * k;
        x = d->fromX + ((float)d->x - d->fromX) * k;
        z = d->fromZ + ((float)d->y - d->fromZ) * k;
    }
    *yaw = y;
    *px = x;
    *pz = z;
}

void dun_render(IK *t) {
    Dungeon *d = &t->dun;
    if (!d->on || !d->loaded) return;
    if (!d->lutReady) {
        build_lut(d);
        d->lutReady = true;
    }
    float yaw, px, pz;
    pose(t, &yaw, &px, &pz);
    size_t bytes = sizeof(u32) * (size_t)t->W * (size_t)t->H;
    if (d->view && !d->stale && d->viewYaw == yaw && d->viewX == px && d->viewZ == pz) {
        memcpy(t->screen, d->view, bytes);
        return;
    }
    float fx = sinf(yaw), fz = -cosf(yaw), rx = -fz, rz = fx;
    float camx = px + 0.5f - BACK * fx, camz = pz + 0.5f - BACK * fz;
    int W = t->W > IK_MAX_W ? IK_MAX_W : t->W, H = t->H;
    for (int i = 0; i < W; i++) {
        float a = ((float)i + 0.5f - CX) / FOCAL;
        float dx = fx + a * rx, dz = fz + a * rz;
        float x = camx, z = camz, tt = 0;
        int cx = (int)floorf(x), cz = (int)floorf(z), kind = 0;
        float u = 0;
        for (int step = 0; step < 64; step++) {
            float tx = dx > 0 ? ((float)cx + 1 - x) / dx : dx < 0 ? ((float)cx - x) / dx : 1e9f;
            float tz = dz > 0 ? ((float)cz + 1 - z) / dz : dz < 0 ? ((float)cz - z) / dz : 1e9f;
            if (tx < tz) {
                int side = dx > 0 ? 1 : 3;
                tt += tx;
                x += dx * tx;
                z += dz * tx;
                u8 v = edge(d, cx, cz, side);
                if (solid(v)) {
                    float f = z - floorf(z);
                    u = dx > 0 ? f : 1 - f;
                    kind = v == 1 ? T_WALL : T_DOOR;
                    break;
                }
                cx += dx > 0 ? 1 : -1;
            } else {
                int side = dz > 0 ? 2 : 0;
                tt += tz;
                x += dx * tz;
                z += dz * tz;
                u8 v = edge(d, cx, cz, side);
                if (solid(v)) {
                    float f = x - floorf(x);
                    u = dz > 0 ? 1 - f : f;
                    kind = v == 1 ? T_WALL : T_DOOR;
                    break;
                }
                cz += dz > 0 ? 1 : -1;
            }
        }
        Column *c = &d->cols[i];
        c->wz = kind ? tt : 1e9f;
        c->lat2 = 1 + a * a;
        c->u = t->automap.bits ? 1 - u : u;
        c->kind = kind;
        c->dx = dx;
        c->dz = dz;
        c->top = kind ? (int)ceilf(CY - (WALL_H - EYE) * FOCAL / c->wz - 0.5f) : H;
        c->bot = kind ? (int)floorf(CY + EYE * FOCAL / c->wz - 0.5f) : -1;
    }
    for (int py = 0; py < H; py++) {
        u32 *row = &t->screen[(size_t)py * W];
        float fy = (float)py + 0.5f;
        float zz = fy > CY ? EYE * FOCAL / (fy - CY) : (WALL_H - EYE) * FOCAL / (CY - fy);
        float hh = fy > CY ? EYE : WALL_H - EYE;
        const Image *flat = d->tex[fy > CY ? T_FLOOR : T_CEIL];
        for (int i = 0; i < W; i++) {
            const Column *c = &d->cols[i];
            if (py >= c->top && py <= c->bot) {
                float h = EYE + (CY - fy) * c->wz / FOCAL;
                row[i] = sample(d->tex[c->kind], c->u, (WALL_H - h) / WALL_H,
                                lit(d, c->wz * c->wz * c->lat2 + (h - EYE) * (h - EYE)));
            } else
                row[i] = sample(flat, camx + c->dx * zz, camz + c->dz * zz, lit(d, zz * zz * c->lat2 + hh * hh));
        }
    }
    if (!d->view) d->view = ik_alloc(bytes);
    memcpy(d->view, t->screen, bytes);
    d->viewYaw = yaw;
    d->viewX = px;
    d->viewZ = pz;
    d->stale = false;
}

static void place_markers(IK *t) {
    Dungeon *d = &t->dun;
    float yaw, px, pz;
    pose(t, &yaw, &px, &pz);
    if (d->compass >= 0 && d->compass < t->layerCount) t->layers[d->compass].rot = yaw;
    if (d->marker >= 0 && d->marker < t->layerCount) {
        Layer *l = &t->layers[d->marker];
        l->rot = yaw;
        l->x = d->mapX + (int)lroundf(px * (float)d->cellW);
        l->y = d->mapY + (int)lroundf(pz * (float)d->cellH);
    }
}

void dun_update(IK *t) {
    Dungeon *d = &t->dun;
    if (!d->animating) return;
    t->dirty = true;
    if (t->now - d->animStart >= STEP_MS) d->animating = false;
    place_markers(t);
}

static int val(IK *t, const u8 *p) { return (int)ik_value(t, rd32(p)); }

static Image *load_tex(IK *t, const char *name) {
    size_t n;
    u8 *data = store_load(&t->store, AR_GRP, name, &n);
    if (!data) {
        ik_log(t, "missing picture %s", name);
        return NULL;
    }
    Image *im = image_decode(data, n);
    free(data);
    return im;
}

static void start_anim(IK *t, float fromYaw, float fromX, float fromZ) {
    Dungeon *d = &t->dun;
    d->fromYaw = fromYaw;
    d->toYaw = (float)d->dir * (float)M_PI / 2;
    if (d->toYaw - fromYaw > (float)M_PI) d->fromYaw += 2 * (float)M_PI;
    if (fromYaw - d->toYaw > (float)M_PI) d->fromYaw -= 2 * (float)M_PI;
    d->fromX = fromX;
    d->fromZ = fromZ;
    d->animStart = t->now;
    d->animating = true;
    t->wait = W_ANIM;
}

static void act(IK *t, int actVar, int xVar, int yVar) {
    Dungeon *d = &t->dun;
    if (actVar < 0 || actVar >= t->varCount || xVar < 0 || xVar >= t->varCount || yVar < 0 || yVar >= t->varCount) return;
    int action = t->vars[actVar];
    float yaw = (float)d->dir * (float)M_PI / 2, fx = (float)d->x, fz = (float)d->y;
    if (action == 0 || action == 1) {
        int dir = action == 0 ? d->dir : (d->dir + 2) & 3;
        if (!blocks(edge(d, d->x, d->y, dir))) {
            d->x += dirX[dir];
            d->y += dirZ[dir];
            start_anim(t, yaw, fx, fz);
        }
    } else if (action == 2 || action == 3) {
        d->dir = (d->dir + (action == 2 ? 3 : 1)) & 3;
        start_anim(t, yaw, fx, fz);
    }
    t->vars[xVar] = d->x;
    t->vars[yVar] = d->y;
    t->vars[actVar] = d->dir;
}

static bool automap_bit(IK *t, int plane, int x, int y, bool *ok) {
    Automap *m = &t->automap;
    *ok = m->bits && plane >= 0 && plane < m->n && x >= 0 && y >= 0 && x < m->w && y < m->h;
    if (!*ok) return true;
    size_t i = ((size_t)plane * (size_t)m->h + (size_t)y) * (size_t)m->w + (size_t)x;
    return m->bits[i >> 3] >> (7 - (i & 7)) & 1;
}

static void automap_mark(IK *t, int plane, int x, int y, bool unexplored) {
    bool ok;
    automap_bit(t, plane, x, y, &ok);
    if (!ok) return;
    Automap *m = &t->automap;
    size_t i = ((size_t)plane * (size_t)m->h + (size_t)y) * (size_t)m->w + (size_t)x;
    if (unexplored) m->bits[i >> 3] |= (u8)(0x80 >> (i & 7));
    else m->bits[i >> 3] &= (u8)~(0x80 >> (i & 7));
}

typedef struct MapDraw {
    Layer *layer;
    Image *dst;
    Image *src;
    int x, y, cw, ch, plane, ox, oy, cols, rows;
    int sx[2], sy[2];
} MapDraw;

static bool map_draw_args(IK *t, u16 op, const u8 *a, int n, MapDraw *d) {
    int k = op == 0x0c05 ? 2 : 0;
    if (n < 52 + 8 * (k / 2)) return false;
    Layer *l = NULL;
    u32 li = ik_value(t, rd32(a));
    if (li < (u32)t->layerCount) l = &t->layers[li];
    int img = val(t, a + 12);
    if (!l || img < 0 || img >= t->imageCount || !t->images[img]) return false;
    d->layer = l;
    d->dst = layer_surface(t, l);
    d->src = t->images[img];
    d->x = val(t, a + 4);
    d->y = val(t, a + 8);
    if (op == 0x0c05) {
        for (int i = 0; i < 2; i++) {
            d->sx[i] = val(t, a + 16 + 8 * i);
            d->sy[i] = val(t, a + 20 + 8 * i);
        }
    } else {
        for (int i = 0; i < 2; i++) {
            int c = val(t, a + 16 + 4 * i);
            if (c < 0 || c >= d->src->cellCount) return false;
            d->sx[i] = d->src->cells[c].sx;
            d->sy[i] = d->src->cells[c].sy;
        }
    }
    const u8 *q = a + 24 + 4 * k;
    d->cw = val(t, q);
    d->ch = val(t, q + 4);
    d->plane = val(t, q + 8);
    d->ox = val(t, q + 12);
    d->oy = val(t, q + 16);
    d->cols = val(t, q + 20);
    d->rows = val(t, q + 24);
    return d->cw > 0 && d->ch > 0;
}

static void map_cell(IK *t, MapDraw *d, int cx, int cy, int which) {
    static const int white[4] = {255, 255, 255, 255};
    int x = d->x + cx * d->cw, y = d->y + cy * d->ch;
    layer_restore(t, d->layer, x, y, d->cw, d->ch);
    image_blit(d->dst, d->src, d->sx[which] + cx * d->cw, d->sy[which] + cy * d->ch, d->cw, d->ch, x, y, white, 0, false);
}

void automap_exec(IK *t, u16 op, const u8 *a, int n) {
    Automap *m = &t->automap;
    if (!m->bits) return;
    switch (op) {
    case 0x0c03: {
        int plane = n >= 20 ? val(t, a + 16) : -1;
        if (plane < 0) memset(m->bits, 0xff, m->size);
        else
            for (int y = 0; y < m->h; y++)
                for (int x = 0; x < m->w; x++) automap_mark(t, plane, x, y, true);
        break;
    }
    case 0x0c04:
        if (n >= 20)
            for (int y = 0, x0 = val(t, a), y0 = val(t, a + 4), w = val(t, a + 8), h = val(t, a + 12), p = val(t, a + 16); y < h; y++)
                for (int x = 0; x < w; x++) automap_mark(t, p, x0 + x, y0 + y, false);
        break;
    default: {
        MapDraw d;
        if (!map_draw_args(t, op, a, n, &d)) break;
        for (int cy = 0; cy < d.rows; cy++)
            for (int cx = 0; cx < d.cols; cx++) {
                bool ok;
                map_cell(t, &d, cx, cy, automap_bit(t, d.plane, d.ox + cx, d.oy + cy, &ok) ? 1 : 0);
            }
        break;
    }
    }
    t->dirty = true;
}

static void automap_here(IK *t, const u8 *a, int n) {
    MapDraw d;
    if (!map_draw_args(t, 0x0809, a, n, &d)) return;
    int cx = t->dun.x - d.ox, cy = t->dun.y - d.oy;
    automap_mark(t, d.plane, t->dun.x, t->dun.y, false);
    if (cx >= 0 && cy >= 0 && cx < d.cols && cy < d.rows) map_cell(t, &d, cx, cy, 0);
}

bool dun_exec(IK *t, u16 op, const u8 *a, int n) {
    Dungeon *d = &t->dun;
    char name[16];
    switch (op) {
    case 0x0800: {
        int k = 0;
        while (k < n && k < 15 && a[k]) {
            name[k] = (char)a[k];
            k++;
        }
        name[k] = 0;
        size_t sz;
        u8 *m = store_load(&t->store, AR_DATA, name, &sz);
        if (!m || sz < sizeof d->map) {
            ik_log(t, "missing map %s", name);
            free(m);
            break;
        }
        memcpy(d->map, m, sizeof d->map);
        free(m);
        d->loaded = true;
        break;
    }
    case 0x0801: {
        int slot = val(t, a);
        int k = 0;
        while (4 + k < n && k < 15 && a[4 + k]) {
            name[k] = (char)a[4 + k];
            k++;
        }
        name[k] = 0;
        if (slot < 0 || slot > 3) break;
        image_free(d->tex[slot]);
        d->tex[slot] = load_tex(t, name);
        break;
    }
    case 0x0802: d->on = val(t, a) != 0; break;
    case 0x0803:
        if (n < 12) break;
        d->x = val(t, a);
        d->y = val(t, a + 4);
        d->dir = val(t, a + 8) & 3;
        d->animating = false;
        place_markers(t);
        break;
    case 0x0804:
        if (n >= 12) act(t, (int)rd32(a), (int)rd32(a + 4), (int)rd32(a + 8));
        place_markers(t);
        break;
    case 0x0805: {
        if (n < 12) break;
        int x = val(t, a), y = val(t, a + 4), v = (int)rd32(a + 8);
        if (v >= 0 && v < t->varCount) t->vars[v] = grid(d, 2 * x + 1, 2 * y + 1);
        break;
    }
    case 0x0806:
        d->compass = val(t, a);
        place_markers(t);
        break;
    case 0x0809:
        automap_here(t, a, n);
        break;
    case 0x0807:
        if (n < 28) break;
        d->marker = val(t, a);
        d->mapX = val(t, a + 12);
        d->mapY = val(t, a + 16);
        d->cellW = val(t, a + 20);
        d->cellH = val(t, a + 24);
        place_markers(t);
        break;
    default: return false;
    }
    d->stale = true;
    t->dirty = true;
    return true;
}

void dun_close(IK *t) {
    for (int i = 0; i < 4; i++) image_free(t->dun.tex[i]);
    free(t->dun.view);
}

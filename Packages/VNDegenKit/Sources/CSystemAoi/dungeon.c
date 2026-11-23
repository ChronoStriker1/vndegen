#include "sa.h"
#include <math.h>

enum {
    MAP_W = 15,
    MAP_H = 15,
    MAP_HEADER = 48,
    CELL_BYTES = 104,
    F_FLOOR = 5,
    F_WALL_N = 18,
    F_WALL_E = 9,
    F_WALL_S = 12,
    F_WALL_W = 15,
    WALL_EXIT = 20,
    VIEW_CELLS = 6,
    MOVE_MS = 133,
    PANEL_X = 0,
    PANEL_Y = 0,
    REST_X = 1101,
    REST_Y = 674,
    REST_W = 160,
    REST_H = 40,
    COMPASS_X = 923,
    COMPASS_Y = 10,
    NEEDLE_CX = 962,
    NEEDLE_CY = 40,
    MINI_X = 1070,
    MINI_Y = 132,
    MINI_CELL = 32,
    MINI_CELLS = 5,
    MINI_WALL = 4,
    PAD_W = 64,
    HOVER_REST = 100,
};

enum { PAD_FORWARD, PAD_BACK, PAD_RIGHT, PAD_LEFT, PAD_CORNER_L, PAD_CORNER_R };

#define CELL 8.0f
#define CAM_Y 3.2f
#define CAM_BACK 4.6f
#define CAM_PITCH (-0.08f)
#define FOV 47.2f
#define LIGHT_GAIN 1.6f
#define QUARTER 1.5707963f

static const float lights[2][3] = {{10, 4.5f, 30}, {-10, 4.5f, 30}};
static const int wallField[4] = {F_WALL_N, F_WALL_E, F_WALL_S, F_WALL_W};
static const int stepX[4] = {0, 1, 0, -1}, stepY[4] = {-1, 0, 1, 0};
static const int sideQuarter[4] = {-1, 0, 1, 2};

static const struct {
    int x, y, h;
} padPos[6] = {{1150, 535, 63}, {1150, 613, 63}, {1186, 574, 63}, {1114, 574, 63}, {1111, 539, 67}, {1190, 539, 67}};

static int texture_index(Dungeon *d, Vfs *vfs, const char *name) {
    if (!*name) return -1;
    for (int i = 0; i < d->texCount; i++)
        if (!strcmp(d->texName[i], name)) return i;
    if (d->texCount == DUN_TEX) return -1;
    size_t n;
    u8 *data = sa_vfs_read(vfs, name, &n);
    Image *im = data ? sa_bmp_decode(data, n) : NULL;
    free(data);
    if (!im) return -1;
    d->tex[d->texCount] = im;
    snprintf(d->texName[d->texCount], sizeof d->texName[0], "%s", name);
    return d->texCount++;
}

static void free_level(Dungeon *d) {
    sa_pvm_free(&d->set);
    free(d->last);
    free(d->layerTex);
    d->last = d->layerTex = NULL;
    for (int i = 0; i < d->texCount; i++) sa_image_free(d->tex[i]);
    d->texCount = 0;
    d->loaded = false;
}

bool sa_dungeon_load(Dungeon *d, Vfs *vfs, const char *set, const char *map) {
    free_level(d);
    size_t n;
    u8 *data = sa_vfs_read(vfs, set, &n);
    bool ok = data && sa_pvm_parse(&d->set, data, n);
    free(data);
    if (!ok) return false;
    d->last = sa_alloc(sizeof(int) * (size_t)d->set.count);
    d->layerTex = sa_alloc(sizeof(int) * (size_t)d->set.count);
    for (int i = 0; i < d->set.count; i++) {
        int j = i;
        while (j + 1 < d->set.count) {
            int p = d->set.layer[j + 1].parent;
            while (p >= 0 && p != i) p = d->set.layer[p].parent;
            if (p != i) break;
            j++;
        }
        d->last[i] = j;
        d->layerTex[i] = texture_index(d, vfs, d->set.layer[i].texture);
    }
    data = sa_vfs_read(vfs, map, &n);
    if (!data || n < MAP_HEADER + (size_t)DUN_CELLS * CELL_BYTES) {
        free(data);
        return false;
    }
    for (int c = 0; c < DUN_CELLS; c++) {
        int x = MAP_W - 1 - c / MAP_H, y = c % MAP_H;
        for (int f = 0; f < DUN_FIELDS; f++) d->cell[y * MAP_W + x][f] = (i32)rd32(data + MAP_HEADER + (size_t)c * CELL_BYTES + (size_t)f * 4);
    }
    free(data);
    if (!d->r3.zbuf) r3_init(&d->r3, SA_W, SA_H);
    d->fov = FOV;
    d->r3.fogNear = -4;
    d->r3.fogFar = 28;
    d->r3.fogColor = 0x1e2d28;
    d->r3.cull = -1;
    memset(d->visited, 0, sizeof d->visited);
    d->hover = -1;
    d->steps = 0;
    d->loaded = true;
    return true;
}

static Image *rotate_quarter(const Image *im, int q) {
    if (!im) return NULL;
    q &= 3;
    int w = q & 1 ? im->h : im->w, h = q & 1 ? im->w : im->h;
    Image *out = sa_image_new(w, h);
    for (int y = 0; y < im->h; y++)
        for (int x = 0; x < im->w; x++) {
            int nx = x, ny = y;
            if (q == 1) nx = im->h - 1 - y, ny = x;
            if (q == 2) nx = im->w - 1 - x, ny = im->h - 1 - y;
            if (q == 3) nx = y, ny = im->w - 1 - x;
            out->px[(size_t)ny * w + nx] = im->px[(size_t)y * im->w + x];
        }
    return out;
}

void sa_dungeon_art(Dungeon *d, Vfs *vfs, const char *platform) {
    if (!d->art[DUN_ART_PANEL]) {
        static const char *const names[] = {"dgn_b00.agf", "dgn_c00.agf", "dgn_c01.agf", "dgn_c02.agf", "dgn_c03.agf",
                                            "dgn_c04.agf", "dgn_c05.agf", "dgn_c06.agf", "dgn_d00.agf", "dgn_d01.agf"};
        for (int i = 0; i < DUN_ART_PLATFORM; i++) d->art[i] = sa_load_image(vfs, names[i]);
        for (int q = 0; q < 4; q++) d->needle[q] = rotate_quarter(d->art[DUN_ART_NEEDLE], q);
    }
    sa_image_free(d->art[DUN_ART_PLATFORM]);
    d->art[DUN_ART_PLATFORM] = platform ? sa_load_image(vfs, platform) : NULL;
}

void sa_dungeon_free(Dungeon *d) {
    free_level(d);
    for (int i = 0; i < DUN_ART; i++) sa_image_free(d->art[i]);
    for (int q = 0; q < 4; q++) sa_image_free(d->needle[q]);
    R3 keep = d->r3;
    memset(d, 0, sizeof *d);
    d->r3 = keep;
}

static const i32 *cell_at(const Dungeon *d, int x, int y) {
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return NULL;
    return d->cell[y * MAP_W + x];
}

void sa_dungeon_place(Dungeon *d, int x, int y, int dir) {
    d->x = x;
    d->y = y;
    d->dir = dir & 3;
    d->camX = d->toX = (float)x * CELL;
    d->camZ = d->toZ = -(float)y * CELL;
    d->yaw = d->toYaw = (float)d->dir * QUARTER;
    d->animDur = 0;
    if (cell_at(d, x, y)) d->visited[y * MAP_W + x] = true;
}

bool sa_dungeon_find_entrance(const Dungeon *d, int *x, int *y, int *dir) {
    for (int side = 0; side < 4; side++)
        for (int c = 0; c < DUN_CELLS; c++)
            if (d->cell[c][F_FLOOR] >= 0 && d->cell[c][wallField[side]] == WALL_EXIT) {
                *x = c % MAP_W;
                *y = c / MAP_W;
                *dir = (side + 2) & 3;
                return true;
            }
    return false;
}

static void start_anim(Dungeon *d) {
    d->fromX = d->camX;
    d->fromZ = d->camZ;
    d->fromYaw = d->yaw;
    d->toX = (float)d->x * CELL;
    d->toZ = -(float)d->y * CELL;
    d->toYaw = d->yaw;
    d->animMs = 0;
    d->animDur = MOVE_MS;
}

static bool walk(Dungeon *d, int side) {
    const i32 *here = cell_at(d, d->x, d->y);
    if (!here || here[wallField[side]] >= 0) return false;
    const i32 *there = cell_at(d, d->x + stepX[side], d->y + stepY[side]);
    if (!there || there[F_FLOOR] < 0) return false;
    d->x += stepX[side];
    d->y += stepY[side];
    d->visited[d->y * MAP_W + d->x] = true;
    d->steps++;
    start_anim(d);
    return true;
}

static void turn(Dungeon *d, int by) {
    d->dir = (d->dir + by) & 3;
    start_anim(d);
    d->toYaw = d->yaw + (float)by * QUARTER;
}

static int act(Dungeon *d, int pad) {
    if (d->animDur) return DUN_STAY;
    switch (pad) {
    case PAD_FORWARD: return walk(d, d->dir) ? DUN_MOVED : DUN_STAY;
    case PAD_BACK: return walk(d, (d->dir + 2) & 3) ? DUN_MOVED : DUN_STAY;
    case PAD_RIGHT: turn(d, 1); return DUN_STAY;
    case PAD_LEFT: turn(d, -1); return DUN_STAY;
    }
    return DUN_STAY;
}

bool sa_dungeon_tick(Dungeon *d, u32 ms) {
    if (!d->animDur) return false;
    d->animMs += ms;
    float t = d->animMs >= d->animDur ? 1 : (float)d->animMs / (float)d->animDur;
    d->camX = d->fromX + (d->toX - d->fromX) * t;
    d->camZ = d->fromZ + (d->toZ - d->fromZ) * t;
    d->yaw = d->fromYaw + (d->toYaw - d->fromYaw) * t;
    if (t >= 1) {
        d->animDur = 0;
        d->yaw = d->toYaw = (float)d->dir * QUARTER;
    }
    return true;
}

static bool on_art(const Image *im, int fh, int ax, int ay, int x, int y) {
    if (!im || x < ax || y < ay || x >= ax + im->w || y >= ay + fh) return false;
    return im->px[(size_t)(y - ay) * im->w + (x - ax)] >> 24 > 128;
}

static int hit(const Dungeon *d, int x, int y) {
    if (x >= REST_X && x < REST_X + REST_W && y >= REST_Y && y < REST_Y + REST_H) return HOVER_REST;
    for (int i = 0; i < 4; i++)
        if (on_art(d->art[DUN_ART_PAD0 + i], padPos[i].h, padPos[i].x, padPos[i].y, x, y)) return i;
    return -1;
}

bool sa_dungeon_hover(Dungeon *d, int x, int y) {
    int h = hit(d, x, y);
    if (h == d->hover) return false;
    d->hover = h;
    return true;
}

int sa_dungeon_click(Dungeon *d, int x, int y) {
    int h = hit(d, x, y);
    if (h == HOVER_REST) return DUN_REST;
    return h >= 0 ? act(d, h) : DUN_STAY;
}

int sa_dungeon_key(Dungeon *d, int key) {
    switch (key) {
    case 0x26: return act(d, PAD_FORWARD);
    case 0x28: return act(d, PAD_BACK);
    case 0x25: return act(d, PAD_LEFT);
    case 0x27: return act(d, PAD_RIGHT);
    }
    return DUN_STAY;
}

typedef struct Cam {
    float pos[3], r[3], u[3], f[3];
} Cam;

static void make_cam(const Dungeon *d, Cam *c) {
    float s = sinf(d->yaw), co = cosf(d->yaw);
    float len = sqrtf(1 + CAM_PITCH * CAM_PITCH), cp = 1 / len, sp = CAM_PITCH / len;
    c->pos[0] = d->camX - s * CAM_BACK;
    c->pos[1] = CAM_Y;
    c->pos[2] = d->camZ - co * CAM_BACK;
    c->f[0] = s * cp;
    c->f[1] = sp;
    c->f[2] = co * cp;
    c->r[0] = co;
    c->r[1] = 0;
    c->r[2] = -s;
    c->u[0] = c->f[1] * c->r[2] - c->f[2] * c->r[1];
    c->u[1] = c->f[2] * c->r[0] - c->f[0] * c->r[2];
    c->u[2] = c->f[0] * c->r[1] - c->f[1] * c->r[0];
}

static float shade_of(const float *n) {
    float s = 192.0f / 255.0f;
    for (int i = 0; i < 2; i++) {
        float l = sqrtf(lights[i][0] * lights[i][0] + lights[i][1] * lights[i][1] + lights[i][2] * lights[i][2]);
        float dot = (n[0] * lights[i][0] + n[1] * lights[i][1] + n[2] * lights[i][2]) / l;
        if (dot > 0) s += 80.0f / 255.0f * dot;
    }
    return s * LIGHT_GAIN;
}

static void draw_part(Dungeon *d, const Cam *c, const char *name, float cx, float cz, int quarter) {
    int i = sa_pvm_find(&d->set, name);
    if (i < 0) return;
    float a = (float)quarter * QUARTER, ca = cosf(a), sa = sinf(a);
    for (int k = i; k <= d->last[i]; k++) {
        const PvmLayer *ly = &d->set.layer[k];
        if (ly->name[0] == '>' || !strncmp(ly->name, "next", 4)) continue;
        const Image *tex = ly->tcount && d->layerTex[k] >= 0 ? d->tex[d->layerTex[k]] : NULL;
        for (int t = 0; t < ly->tcount; t++) {
            R3Vert v[3];
            for (int j = 0; j < 3; j++) {
                const PvmVertex *pv = &ly->v[ly->tri[t * 3 + j]];
                float wx = pv->pos[0] * ca + pv->pos[2] * sa + cx, wy = pv->pos[1], wz = -pv->pos[0] * sa + pv->pos[2] * ca + cz;
                float n[3] = {pv->normal[0] * ca + pv->normal[2] * sa, pv->normal[1], -pv->normal[0] * sa + pv->normal[2] * ca};
                float dx = wx - c->pos[0], dy = wy - c->pos[1], dz = wz - c->pos[2];
                v[j].x = dx * c->r[0] + dy * c->r[1] + dz * c->r[2];
                v[j].y = dx * c->u[0] + dy * c->u[1] + dz * c->u[2];
                v[j].z = dx * c->f[0] + dy * c->f[1] + dz * c->f[2];
                float cn[3] = {n[0] * c->r[0] + n[1] * c->r[1] + n[2] * c->r[2], n[0] * c->u[0] + n[1] * c->u[1] + n[2] * c->u[2],
                               n[0] * c->f[0] + n[1] * c->f[1] + n[2] * c->f[2]};
                v[j].u = pv->uv[0];
                v[j].v = pv->uv[1];
                v[j].shade = shade_of(cn);
            }
            r3_tri(&d->r3, v, tex);
        }
    }
}

void sa_dungeon_draw(Dungeon *d, u32 *fb) {
    u32 fog = d->r3.fogColor;
    u32 bg = 0xff000000u | (fog & 0xff) << 16 | (fog & 0xff00) | (fog >> 16 & 0xff);
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = bg;
    if (!d->loaded) return;
    r3_begin(&d->r3, fb, d->fov);
    Cam c;
    make_cam(d, &c);
    for (int row = 0; row < MAP_H; row++)
        for (int col = 0; col < MAP_W; col++) {
            if (abs(col - d->x) > VIEW_CELLS || abs(row - d->y) > VIEW_CELLS) continue;
            const i32 *cell = d->cell[row * MAP_W + col];
            if (cell[F_FLOOR] < 0) continue;
            float cx = (float)col * CELL, cz = -(float)row * CELL;
            char name[16];
            snprintf(name, sizeof name, "f%03d", cell[F_FLOOR]);
            draw_part(d, &c, name, cx, cz, 0);
            draw_part(d, &c, "r000", cx, cz, 0);
            for (int s = 0; s < 4; s++) {
                int w = cell[wallField[s]];
                if (w < 0) continue;
                snprintf(name, sizeof name, "w%03d", w);
                draw_part(d, &c, name, cx, cz, sideQuarter[s]);
            }
        }
}

static void fill(u32 *fb, int x, int y, int w, int h, u32 c) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (i >= 0 && j >= 0 && i < SA_W && j < SA_H) fb[(size_t)j * SA_W + i] = c;
}

static u32 rgb(int r, int g, int b) { return 0xff000000u | (u32)b << 16 | (u32)g << 8 | (u32)r; }

static void draw_minimap(Dungeon *d, u32 *fb) {
    fill(fb, MINI_X, MINI_Y, MINI_CELL * MINI_CELLS, MINI_CELL * MINI_CELLS, rgb(0, 0, 0));
    u32 grid = rgb(56, 56, 56), wall = rgb(240, 240, 240);
    for (int k = 0; k <= MINI_CELLS; k++) {
        int p = k * MINI_CELL - (k == MINI_CELLS ? 2 : 0);
        for (int t = 0; t < MINI_CELL * MINI_CELLS; t += 6) {
            fill(fb, MINI_X + p, MINI_Y + t, 2, 4, grid);
            fill(fb, MINI_X + t, MINI_Y + p, 4, 2, grid);
        }
    }
    for (int j = 0; j < MINI_CELLS; j++)
        for (int i = 0; i < MINI_CELLS; i++) {
            int mx = d->x + i - MINI_CELLS / 2, my = d->y + j - MINI_CELLS / 2;
            const i32 *cell = cell_at(d, mx, my);
            if (!cell || !d->visited[my * MAP_W + mx]) continue;
            int x0 = MINI_X + i * MINI_CELL, y0 = MINI_Y + j * MINI_CELL;
            for (int y = 2; y < MINI_CELL; y++)
                for (int x = 2; x < MINI_CELL; x++) {
                    float dx = (float)(x - MINI_CELL / 2) / (MINI_CELL / 2), dy = (float)(y - MINI_CELL / 2) / (MINI_CELL / 2);
                    int v = 60 + (int)(10 * (dx * dx + dy * dy) / 2);
                    fb[(size_t)(y0 + y) * SA_W + x0 + x] = rgb(v, v - 11, v - 11);
                }
            if (cell[F_WALL_N] >= 0 && cell[F_WALL_N] != WALL_EXIT) fill(fb, x0 + 2, y0 + 2, MINI_CELL - 2, MINI_WALL, wall);
            if (cell[F_WALL_S] >= 0 && cell[F_WALL_S] != WALL_EXIT) fill(fb, x0 + 2, y0 + MINI_CELL - MINI_WALL, MINI_CELL - 2, MINI_WALL, wall);
            if (cell[F_WALL_W] >= 0 && cell[F_WALL_W] != WALL_EXIT) fill(fb, x0 + 2, y0 + 2, MINI_WALL, MINI_CELL - 2, wall);
            if (cell[F_WALL_E] >= 0 && cell[F_WALL_E] != WALL_EXIT) fill(fb, x0 + MINI_CELL - MINI_WALL, y0 + 2, MINI_WALL, MINI_CELL - 2, wall);
        }
    int cx = MINI_X + MINI_CELLS / 2 * MINI_CELL + MINI_CELL / 2, cy = MINI_Y + MINI_CELLS / 2 * MINI_CELL + MINI_CELL / 2;
    for (int t = 0; t < 10; t++)
        for (int w = -t / 2; w <= t / 2; w++) {
            int ax, ay;
            switch (d->dir) {
            case 0: ax = w, ay = t - 6; break;
            case 1: ax = 6 - t, ay = w; break;
            case 2: ax = w, ay = 6 - t; break;
            default: ax = t - 6, ay = w; break;
            }
            fb[(size_t)(cy + ay) * SA_W + cx + ax] = rgb(76, 152, 255);
        }
}

void sa_dungeon_draw_hud(Dungeon *d, u32 *fb, struct Party *party, int rankLimit) {
    sa_blit(fb, d->art[DUN_ART_PANEL], PANEL_X, PANEL_Y);
    sa_blit_rect(fb, d->art[DUN_ART_COMPASS], 0, 0, d->art[DUN_ART_COMPASS] ? d->art[DUN_ART_COMPASS]->w : 0, 76, COMPASS_X,
                 COMPASS_Y);
    static const int nudge[4][2] = {{2, 1}, {1, 0}, {0, 1}, {1, 2}};
    const Image *n = d->needle[(4 - d->dir) & 3];
    if (n) sa_blit(fb, n, NEEDLE_CX - n->w / 2 + nudge[d->dir][0], NEEDLE_CY - n->h / 2 + nudge[d->dir][1]);
    draw_minimap(d, fb);
    for (int i = 0; i < 6; i++) {
        const Image *im = d->art[DUN_ART_PAD0 + i];
        if (!im) continue;
        int h = padPos[i].h;
        sa_blit_rect(fb, im, 0, i < 4 && d->hover == i ? h : 0, PAD_W, h, padPos[i].x, padPos[i].y);
        if (i >= 4) sa_blit_rect(fb, im, 0, h, PAD_W, h, padPos[i].x, padPos[i].y + h);
    }
    sa_blit_rect(fb, d->art[DUN_ART_REST], 0, d->hover == HOVER_REST ? REST_H : 0, REST_W, REST_H, REST_X, REST_Y);
    if (party) sa_party_draw_diorama(party, fb, rankLimit, d->art[DUN_ART_PLATFORM]);
}

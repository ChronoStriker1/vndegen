#include "sa.h"
#include <math.h>

enum {
    MAP_HEADER = 48,
    CELL_BYTES = 104,
    F_FLOOR = 5,
    F_OBJECT = 6,
    F_MARK = 7,
    F_WALL_N = 9,
    F_WALL_E = 12,
    F_WALL_S = 15,
    F_WALL_W = 18,
    WALL_PASSAGE = 20,
    OBJECT_DOOR = 100,
    OBJECT_DOOR_SPOT = 99,
    WALL_DOORWAY = 10,
    WALL_HIDDEN = 100,
    VIEW_CELLS = 6,
    MOVE_MS = 133,
    DOOR_TURN_MS = 18000,
    DOOR_FRAME_MS = 66,
    ACTION_X = 1101,
    ACTION_Y = 498,
    ACTION_W = 160,
    ACTION_H = 40,
    HOVER_ACTION = 101,
    FLOOR_DOWN = 100,
    FLOOR_UP = 101,
    OBJECT_CRYSTAL = 60,
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
    MINI_MARK = 16,
    DUN_BIG_ICONS = 16,
    DIGIT_W = 10,
    STEPS_X = 994,
    STEPS_Y = 72,
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
#define VIEW_SLOPE 0.75f

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

static void free_set(PartSet *p) {
    sa_pvm_free(&p->pvm);
    free(p->last);
    free(p->layerTex);
    memset(p, 0, sizeof *p);
}

static void free_level(Dungeon *d) {
    free_set(&d->set);
    free_set(&d->common);
    for (int i = 0; i < d->texCount; i++) sa_image_free(d->tex[i]);
    d->texCount = 0;
    d->loaded = false;
}

static bool load_set(Dungeon *d, Vfs *vfs, PartSet *p, const char *name) {
    size_t n;
    u8 *data = sa_vfs_read(vfs, name, &n);
    bool ok = data && sa_pvm_parse(&p->pvm, data, n);
    free(data);
    if (!ok) return false;
    p->last = sa_alloc(sizeof(int) * (size_t)p->pvm.count);
    p->layerTex = sa_alloc(sizeof(int) * (size_t)p->pvm.count);
    for (int i = 0; i < p->pvm.count; i++) {
        int j = i;
        while (j + 1 < p->pvm.count) {
            int q = p->pvm.layer[j + 1].parent;
            while (q >= 0 && q != i) q = p->pvm.layer[q].parent;
            if (q != i) break;
            j++;
        }
        p->last[i] = j;
        p->layerTex[i] = texture_index(d, vfs, p->pvm.layer[i].texture);
    }
    return true;
}

bool sa_dungeon_read_map(Dungeon *d, Vfs *vfs, const char *map) {
    size_t n;
    u8 *data = sa_vfs_read(vfs, map, &n);
    int w = data && n >= MAP_HEADER ? (int)rd32(data + 4) : 0, h = data && n >= MAP_HEADER ? (int)rd32(data + 8) : 0;
    if (w <= 0 || h <= 0 || w > DUN_SIDE || h > DUN_SIDE || n < MAP_HEADER + (size_t)(w * h) * CELL_BYTES) {
        free(data);
        return false;
    }
    d->w = w;
    d->h = h;
    for (int c = 0; c < DUN_CELLS; c++)
        for (int f = 0; f < DUN_FIELDS; f++) d->cell[c][f] = f == F_FLOOR || f == F_OBJECT ? -1 : 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            for (int f = 0; f < DUN_FIELDS; f++)
                d->cell[y * DUN_SIDE + x][f] = (i32)rd32(data + MAP_HEADER + (size_t)(y * w + x) * CELL_BYTES + (size_t)f * 4);
    free(data);
    return true;
}

bool sa_dungeon_load(Dungeon *d, Vfs *vfs, const char *set, const char *map) {
    free_level(d);
    if (!load_set(d, vfs, &d->set, set)) return false;
    load_set(d, vfs, &d->common, "mallset.pvm");
    if (!sa_dungeon_read_map(d, vfs, map)) return false;
    if (!d->r3.zbuf) r3_init(&d->r3, SA_W, SA_H);
    d->fov = FOV;
    d->r3.fogNear = -4;
    d->r3.fogFar = 28;
    d->r3.fogColor = 0x1e2d28;
    d->r3.cull = -1;
    d->floorGain = 1;
    memset(d->own, 0, sizeof d->own);
    memset(d->ownPassage, 0, sizeof d->ownPassage);
    d->visited = d->own;
    d->passage = d->ownPassage;
    d->hover = -1;
    d->steps = 0;
    d->loaded = true;
    return true;
}

static Image *scale2(Image *im) {
    if (!im) return NULL;
    Image *out = sa_image_new(im->w * 2, im->h * 2);
    for (int y = 0; y < out->h; y++)
        for (int x = 0; x < out->w; x++) out->px[(size_t)y * out->w + x] = im->px[(size_t)(y / 2) * im->w + x / 2];
    sa_image_free(im);
    return out;
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
        static const char *const names[] = {"dgn_b00.agf", "dgn_c00.agf", "dgn_c01.agf", "dgn_c02.agf", "dgn_c03.agf", "dgn_c04.agf",
                                            "dgn_c05.agf", "dgn_c06.agf", "dgn_d00.agf", "dgn_d01.agf", "dgn_g02.agf", "dgn_g01.agf",
                                            "dgn_g00.agf", "dgn_g07.agf", "dgn_f00.agf"};
        for (int i = 0; i < DUN_ART_PLATFORM; i++) d->art[i] = sa_load_image(vfs, names[i]);
        for (int q = 0; q < 4; q++) d->needle[q] = rotate_quarter(d->art[DUN_ART_NEEDLE], q);
        static const char *const icons[] = {"mmf0003.agf", "mmf0004.agf", "mma0001.agf", "mma0000.agf"};
        for (int i = 0; i < DUN_ICONS; i++) d->icon[i] = scale2(sa_load_image(vfs, icons[i]));
        for (int q = 0; q < 4; q++) d->arrow[q] = rotate_quarter(d->icon[DUN_ICON_PARTY], q);
    }
    sa_image_free(d->art[DUN_ART_PLATFORM]);
    d->art[DUN_ART_PLATFORM] = platform ? sa_load_image(vfs, platform) : NULL;
}

void sa_dungeon_free(Dungeon *d) {
    free_level(d);
    for (int i = 0; i < DUN_ART; i++) sa_image_free(d->art[i]);
    for (int q = 0; q < 4; q++) sa_image_free(d->needle[q]);
    for (int i = 0; i < DUN_ICONS; i++) sa_image_free(d->icon[i]);
    for (int i = 0; i < d->bigCount; i++) sa_image_free(d->big[i]);
    d->bigCount = 0;
    for (int q = 0; q < 4; q++) sa_image_free(d->arrow[q]);
    R3 keep = d->r3;
    memset(d, 0, sizeof *d);
    d->r3 = keep;
}

static bool hiddenOpen;

static const i32 *cell_at(const Dungeon *d, int x, int y);

static bool hidden_between(const Dungeon *d, int x, int y, int side) {
    if (!hiddenOpen) return false;
    const i32 *n = cell_at(d, x + stepX[side & 3], y + stepY[side & 3]);
    return n && n[wallField[(side + 2) & 3]] == WALL_HIDDEN;
}

static const i32 *cell_at(const Dungeon *d, int x, int y) {
    if (x < 0 || y < 0 || x >= d->w || y >= d->h) return NULL;
    return d->cell[y * DUN_SIDE + x];
}

void sa_dungeon_place(Dungeon *d, int x, int y, int dir) {
    d->x = x;
    d->y = y;
    d->dir = dir & 3;
    d->camX = d->toX = (float)x * CELL;
    d->camZ = d->toZ = -(float)y * CELL;
    d->yaw = d->toYaw = (float)d->dir * QUARTER;
    d->animDur = 0;
    if (cell_at(d, x, y) && d->visited) d->visited[y * DUN_SIDE + x] = true;
}

void sa_dungeon_sense(bool on) { hiddenOpen = on; }

static bool passable(const i32 *cell, int side) {
    int w = cell[wallField[side]];
    return w < 0 || w == WALL_DOORWAY || (w == WALL_HIDDEN && hiddenOpen) || cell[wallField[side] + 1] == 1;
}

static void mark_passage(Dungeon *d, int from, int to, int side);

static bool step_in(const Dungeon *d, int c, int *x, int *y, int *dir) {
    for (int side = 0; side < 4; side++) {
        if (!passable(d->cell[c], side) || d->cell[c][wallField[side]] == WALL_PASSAGE) continue;
        const i32 *next = cell_at(d, c % DUN_SIDE + stepX[side], c / DUN_SIDE + stepY[side]);
        if (!next || next[F_FLOOR] < 0) continue;
        *x = c % DUN_SIDE + stepX[side];
        *y = c / DUN_SIDE + stepY[side];
        *dir = side;
        return true;
    }
    return false;
}

bool sa_dungeon_find_entrance(const Dungeon *d, int *x, int *y, int *dir) {
    for (int c = 0; c < DUN_CELLS; c++)
        if (d->cell[c][F_FLOOR] >= 0 && d->cell[c][F_OBJECT] == OBJECT_DOOR && step_in(d, c, x, y, dir)) return true;
    for (int c = 0; c < DUN_CELLS; c++)
        for (int side = 0; side < 4; side++)
            if (d->cell[c][F_FLOOR] >= 0 && d->cell[c][wallField[side]] == WALL_PASSAGE && step_in(d, c, x, y, dir)) return true;
    return false;
}

bool sa_dungeon_find_mark(const Dungeon *d, int mark, int *x, int *y) {
    for (int c = 0; c < DUN_CELLS; c++)
        if (d->cell[c][F_MARK] == mark && d->cell[c][F_FLOOR] >= 0) {
            *x = c % DUN_SIDE;
            *y = c / DUN_SIDE;
            return true;
        }
    return false;
}

void sa_dungeon_open_doors(Dungeon *d) {
    for (int c = 0; c < DUN_CELLS; c++)
        if (d->cell[c][F_OBJECT] == OBJECT_DOOR_SPOT) d->cell[c][F_OBJECT] = OBJECT_DOOR;
}

int sa_dungeon_neighbour(const Dungeon *d, int x, int y, int side) {
    const i32 *c = cell_at(d, x, y);
    if (!c || c[F_FLOOR] < 0 || (!passable(c, side & 3) && !hidden_between(d, x, y, side))) return -1;
    const i32 *n = cell_at(d, x + stepX[side & 3], y + stepY[side & 3]);
    return n && n[F_FLOOR] >= 0 ? (y + stepY[side & 3]) * DUN_SIDE + x + stepX[side & 3] : -1;
}

bool sa_dungeon_cell_is_passage(const Dungeon *d, int x, int y) {
    const i32 *c = cell_at(d, x, y);
    if (!c) return false;
    for (int side = 0; side < 4; side++)
        if (c[wallField[side]] == WALL_PASSAGE) return true;
    return false;
}

int sa_dungeon_passage_side(const Dungeon *d) {
    const i32 *c = cell_at(d, d->x, d->y);
    if (!c) return -1;
    for (int side = 0; side < 4; side++)
        if (c[wallField[side]] == WALL_PASSAGE) return side;
    return -1;
}

bool sa_dungeon_has_passage(const Dungeon *d, int side, int along) {
    int x, y;
    return sa_dungeon_passage_cell(d, side, along, &x, &y);
}

bool sa_dungeon_passage_cell(const Dungeon *d, int side, int along, int *x, int *y) {
    for (int c = 0; c < DUN_CELLS; c++) {
        if (d->cell[c][F_FLOOR] < 0 || d->cell[c][wallField[side & 3]] != WALL_PASSAGE) continue;
        int cx = c % DUN_SIDE, cy = c / DUN_SIDE;
        if ((side & 1 ? cy : cx) != along) continue;
        *x = cx;
        *y = cy;
        return true;
    }
    return false;
}

bool sa_dungeon_arrive(Dungeon *d, int side, int along) {
    int x, y;
    if (!sa_dungeon_passage_cell(d, side, along, &x, &y)) return false;
    int in = (side + 2) & 3;
    const i32 *next = cell_at(d, x + stepX[in], y + stepY[in]);
    if (d->visited) d->visited[y * DUN_SIDE + x] = true;
    if (next && next[F_FLOOR] >= 0 && passable(d->cell[y * DUN_SIDE + x], in)) {
        int from = y * DUN_SIDE + x;
        x += stepX[in];
        y += stepY[in];
        mark_passage(d, from, y * DUN_SIDE + x, in);
    }
    sa_dungeon_place(d, x, y, in);
    return true;
}

static void mark_passage(Dungeon *d, int from, int to, int side) {
    if (side == DIR_E || side == DIR_S) d->passage[from] |= side == DIR_E ? PASS_EAST : PASS_SOUTH;
    else d->passage[to] |= side == DIR_W ? PASS_EAST : PASS_SOUTH;
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
    if (!here || (!passable(here, side) && !hidden_between(d, d->x, d->y, side))) {
        if (here && side == d->dir) d->bumped = true;
        return false;
    }
    const i32 *there = cell_at(d, d->x + stepX[side], d->y + stepY[side]);
    if (!there || there[F_FLOOR] < 0) return false;
    int from = d->y * DUN_SIDE + d->x;
    d->x += stepX[side];
    d->y += stepY[side];
    int to = d->y * DUN_SIDE + d->x;
    d->visited[to] = true;
    mark_passage(d, from, to, side);
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
    case PAD_RIGHT: turn(d, 1); return DUN_TURNED;
    case PAD_LEFT: turn(d, -1); return DUN_TURNED;
    }
    return DUN_STAY;
}

static bool door_near(const Dungeon *d) {
    for (int y = d->y - VIEW_CELLS; y <= d->y + VIEW_CELLS; y++)
        for (int x = d->x - VIEW_CELLS; x <= d->x + VIEW_CELLS; x++) {
            const i32 *c = cell_at(d, x, y);
            if (c && c[F_FLOOR] >= 0 && c[F_OBJECT] == OBJECT_DOOR) return true;
        }
    return false;
}

int sa_dungeon_action(const Dungeon *d) {
    const i32 *c = cell_at(d, d->x, d->y);
    if (!c || d->animDur) return DUN_ACT_NONE;
    if (c[F_OBJECT] == OBJECT_DOOR) return DUN_ACT_HOME;
    if (c[F_FLOOR] == FLOOR_DOWN) return DUN_ACT_DOWN;
    if (c[F_FLOOR] == FLOOR_UP) return DUN_ACT_UP;
    if (c[F_OBJECT] == OBJECT_CRYSTAL) return DUN_ACT_OPEN;
    return DUN_ACT_NONE;
}

bool sa_dungeon_tick(Dungeon *d, u32 ms) {
    u32 before = d->clock / DOOR_FRAME_MS;
    d->clock += ms;
    bool turning = d->clock / DOOR_FRAME_MS != before && door_near(d);
    if (!d->animDur) return turning;
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
    if (sa_dungeon_action(d) && x >= ACTION_X && x < ACTION_X + ACTION_W && y >= ACTION_Y && y < ACTION_Y + ACTION_H) return HOVER_ACTION;
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
    if (h == HOVER_ACTION) return DUN_ACTION;
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

static void draw_model(Dungeon *d, const PartSet *ps, const Cam *c, const char *name, float cx, float cz, float a) {
    int i = sa_pvm_find(&ps->pvm, name);
    if (i < 0) return;
    float ca = cosf(a), sa = sinf(a);
    for (int k = i; k <= ps->last[i]; k++) {
        const PvmLayer *ly = &ps->pvm.layer[k];
        if (ly->name[0] == '>' || !strncmp(ly->name, "next", 4)) continue;
        const Image *tex = ly->tcount && ps->layerTex[k] >= 0 ? d->tex[ps->layerTex[k]] : NULL;
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
                v[j].shade = shade_of(cn) * (name[0] == 'f' ? d->floorGain : 1);
            }
            r3_tri(&d->r3, v, tex);
        }
    }
}

static float spin(const Dungeon *d) { return (float)(d->clock % DOOR_TURN_MS) * 6.2831853f / DOOR_TURN_MS; }

static void draw_part(Dungeon *d, const PartSet *ps, const Cam *c, const char *name, float cx, float cz, int quarter) {
    draw_model(d, ps, c, name, cx, cz, (float)quarter * QUARTER);
}

void sa_dungeon_draw(Dungeon *d, u32 *fb) {
    u32 fog = d->r3.fogColor;
    u32 bg = 0xff000000u | fog;
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = bg;
    if (!d->loaded) return;
    r3_begin(&d->r3, fb, d->fov);
    Cam c;
    make_cam(d, &c);
    int order[DUN_CELLS], n = 0;
    float dist[DUN_CELLS];
    for (int row = 0; row < d->h; row++)
        for (int col = 0; col < d->w; col++) {
            if (abs(col - d->x) > VIEW_CELLS || abs(row - d->y) > VIEW_CELLS) continue;
            float dx = (float)col * CELL - c.pos[0], dz = -(float)row * CELL - c.pos[2];
            float ahead = dx * c.f[0] + dz * c.f[2], side = fabsf(dx * c.r[0] + dz * c.r[2]);
            if (ahead < -CELL || side > ahead * VIEW_SLOPE + CELL * 1.5f) continue;
            int k = n++;
            dist[k] = dx * dx + dz * dz;
            order[k] = row * DUN_SIDE + col;
            while (k > 0 && dist[k - 1] > dist[k]) {
                float tf = dist[k];
                dist[k] = dist[k - 1];
                dist[k - 1] = tf;
                int ti = order[k];
                order[k] = order[k - 1];
                order[k - 1] = ti;
                k--;
            }
        }
    for (int i = 0; i < n; i++) {
        int row = order[i] / DUN_SIDE, col = order[i] % DUN_SIDE;
        const i32 *cell = d->cell[row * DUN_SIDE + col];
        if (cell[F_FLOOR] < 0) continue;
        float cx = (float)col * CELL, cz = -(float)row * CELL;
        char name[16];
        snprintf(name, sizeof name, "f%03d", cell[F_FLOOR]);
        draw_part(d, &d->set, &c, name, cx, cz, 0);
        draw_part(d, &d->set, &c, "r000", cx, cz, 0);
        for (int s = 0; s < 4; s++) {
            int w = cell[wallField[s]];
            if (w < 0 || w == WALL_PASSAGE) continue;
            snprintf(name, sizeof name, "w%03d", w);
            draw_part(d, &d->set, &c, name, cx, cz, sideQuarter[s]);
        }
        if (cell[F_OBJECT] >= 0) {
            snprintf(name, sizeof name, "a%03d", cell[F_OBJECT]);
            draw_model(d, &d->common, &c, name, cx, cz, cell[F_OBJECT] == OBJECT_DOOR ? spin(d) : 0);
        }
    }
}

static void fill(u32 *fb, int x, int y, int w, int h, u32 c) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (i >= 0 && j >= 0 && i < SA_W && j < SA_H) fb[(size_t)j * SA_W + i] = c;
}

static u32 rgb(int r, int g, int b) { return 0xff000000u | (u32)b << 16 | (u32)g << 8 | (u32)r; }

static const Image *icon_at(Dungeon *d, const Image *base, int scale) {
    if (scale == 1 || !base) return base;
    for (int i = 0; i < d->bigCount; i++)
        if (d->bigFrom[i] == base) return d->big[i];
    if (d->bigCount >= DUN_BIG_ICONS) return base;
    d->bigFrom[d->bigCount] = base;
    d->big[d->bigCount] = sa_image_scale(base, base->w * scale, base->h * scale);
    return d->big[d->bigCount++];
}

static void draw_map(Dungeon *d, u32 *fb, int ox, int oy, int scale) {
    const int cs = MINI_CELL * scale, ws = MINI_WALL * scale, ms = MINI_MARK * scale, inset = 2 * scale;
    fill(fb, ox, oy, cs * MINI_CELLS, cs * MINI_CELLS, rgb(0, 0, 0));
    u32 grid = rgb(56, 56, 56), wall = rgb(240, 240, 240);
    for (int k = 0; k <= MINI_CELLS; k++) {
        int p = k * cs - (k == MINI_CELLS ? inset : 0);
        for (int t = 0; t < cs * MINI_CELLS; t += 6 * scale) {
            fill(fb, ox + p, oy + t, inset, 4 * scale, grid);
            fill(fb, ox + t, oy + p, 4 * scale, inset, grid);
        }
    }
    for (int j = 0; j < MINI_CELLS; j++)
        for (int i = 0; i < MINI_CELLS; i++) {
            int mx = d->x + i - MINI_CELLS / 2, my = d->y + j - MINI_CELLS / 2;
            const i32 *cell = cell_at(d, mx, my);
            if (!cell || !d->visited[my * DUN_SIDE + mx]) continue;
            int x0 = ox + i * cs, y0 = oy + j * cs;
            for (int y = inset; y < cs; y++)
                for (int x = inset; x < cs; x++) {
                    float dx = (float)(x - cs / 2) / (cs / 2), dy = (float)(y - cs / 2) / (cs / 2);
                    int v = 60 + (int)(10 * (dx * dx + dy * dy) / 2);
                    fb[(size_t)(y0 + y) * SA_W + x0 + x] = rgb(v, v - 11, v - 11);
                }
            if (cell[F_WALL_N] >= 0 && cell[F_WALL_N] != WALL_PASSAGE && cell[F_WALL_N] != WALL_DOORWAY) fill(fb, x0 + inset, y0 + inset, cs - inset, ws, wall);
            if (cell[F_WALL_S] >= 0 && cell[F_WALL_S] != WALL_PASSAGE && cell[F_WALL_S] != WALL_DOORWAY) fill(fb, x0 + inset, y0 + cs - ws, cs - inset, ws, wall);
            if (cell[F_WALL_W] >= 0 && cell[F_WALL_W] != WALL_PASSAGE && cell[F_WALL_W] != WALL_DOORWAY) fill(fb, x0 + inset, y0 + inset, ws, cs - inset, wall);
            if (cell[F_WALL_E] >= 0 && cell[F_WALL_E] != WALL_PASSAGE && cell[F_WALL_E] != WALL_DOORWAY) fill(fb, x0 + cs - ws, y0 + inset, ws, cs - inset, wall);
        }
    for (int j = 0; j < MINI_CELLS; j++)
        for (int i = 0; i < MINI_CELLS; i++) {
            int mx = d->x + i - MINI_CELLS / 2, my = d->y + j - MINI_CELLS / 2;
            const i32 *cell = cell_at(d, mx, my);
            if (!cell || !d->visited[my * DUN_SIDE + mx]) continue;
            int x0 = ox + i * cs, y0 = oy + j * cs;
            for (int side = 0; side < 4; side++) {
                if (cell[wallField[side]] != WALL_PASSAGE) continue;
                int px = side == DIR_E ? x0 + cs - ws : side == DIR_W ? x0 + inset : x0 + cs / 2 - ms / 2;
                int py = side == DIR_S ? y0 + cs - ws : side == DIR_N ? y0 + inset : y0 + cs / 2 - ms / 2;
                bool across = side == DIR_N || side == DIR_S;
                fill(fb, px, py, across ? ms : ws, across ? ws : ms, rgb(230, 40, 210));
            }
            int icon = cell[F_OBJECT] == OBJECT_DOOR ? DUN_ICON_DOOR : cell[F_OBJECT] == OBJECT_CRYSTAL ? DUN_ICON_CHEST : -1;
            if (icon >= 0 && d->icon[icon]) sa_blit(fb, icon_at(d, d->icon[icon], scale), x0, y0);
        }
    int cx = ox + MINI_CELLS / 2 * cs, cy = oy + MINI_CELLS / 2 * cs;
    for (int g = 0; g < d->markCount; g++) {
        int dx = d->markX[g] - d->x, dy = d->markY[g] - d->y;
        if (abs(dx) > MINI_CELLS / 2 + 1 || abs(dy) > MINI_CELLS / 2 + 1 || !d->icon[DUN_ICON_GROUP]) continue;
        sa_blit(fb, icon_at(d, d->icon[DUN_ICON_GROUP], scale), cx + dx * cs, cy + dy * cs);
    }
    if (d->arrow[d->dir]) sa_blit(fb, icon_at(d, d->arrow[d->dir], scale), cx, cy);
}

void sa_dungeon_draw_map(Dungeon *d, u32 *fb, int x, int y, int scale) { draw_map(d, fb, x, y, scale); }

void sa_dungeon_draw_hud(Dungeon *d, u32 *fb, struct Party *party, int rankLimit) {
    sa_blit(fb, d->art[DUN_ART_PANEL], PANEL_X, PANEL_Y);
    sa_blit_rect(fb, d->art[DUN_ART_COMPASS], 0, 0, d->art[DUN_ART_COMPASS] ? d->art[DUN_ART_COMPASS]->w : 0, 76, COMPASS_X,
                 COMPASS_Y);
    static const int nudge[4][2] = {{2, 1}, {1, 0}, {0, 1}, {1, 2}};
    const Image *n = d->needle[(4 - d->dir) & 3];
    if (n) sa_blit(fb, n, NEEDLE_CX - n->w / 2 + nudge[d->dir][0], NEEDLE_CY - n->h / 2 + nudge[d->dir][1]);
    draw_map(d, fb, MINI_X, MINI_Y, 1);
    char digits[12];
    int len = snprintf(digits, sizeof digits, "%d", d->steps);
    for (int i = 0; i < len && d->art[DUN_ART_DIGITS]; i++)
        sa_blit_rect(fb, d->art[DUN_ART_DIGITS], (digits[i] - '0') * DIGIT_W, 0, DIGIT_W, d->art[DUN_ART_DIGITS]->h, STEPS_X - (len - 1 - i) * DIGIT_W, STEPS_Y);
    for (int i = 0; i < 6; i++) {
        const Image *im = d->art[DUN_ART_PAD0 + i];
        if (!im) continue;
        int h = padPos[i].h;
        sa_blit_rect(fb, im, 0, i < 4 && d->hover == i ? h : 0, PAD_W, h, padPos[i].x, padPos[i].y);
        if (i >= 4) sa_blit_rect(fb, im, 0, h, PAD_W, h, padPos[i].x, padPos[i].y + h);
    }
    sa_blit_rect(fb, d->art[DUN_ART_REST], 0, d->hover == HOVER_REST ? REST_H : 0, REST_W, REST_H, REST_X, REST_Y);
    static const int actionArt[] = {0, DUN_ART_HOME, DUN_ART_DOWN, DUN_ART_UP, DUN_ART_OPEN};
    int act = sa_dungeon_action(d);
    if (act) sa_blit_rect(fb, d->art[actionArt[act]], 0, d->hover == HOVER_ACTION ? ACTION_H : 0, ACTION_W, ACTION_H, ACTION_X, ACTION_Y);
    if (party) sa_party_draw_diorama(party, fb, rankLimit, d->art[DUN_ART_PLATFORM]);
}

const Image *sa_dungeon_platform(const Dungeon *d) { return d->art[DUN_ART_PLATFORM]; }

void sa_dungeon_draw_frame(Dungeon *d, u32 *fb, struct Party *party, int rankLimit) {
    sa_blit(fb, d->art[DUN_ART_PANEL], PANEL_X, PANEL_Y);
    if (party) sa_party_draw_diorama(party, fb, rankLimit, d->art[DUN_ART_PLATFORM]);
}

static int field_of(const char *name, int dir) {
    if (!strcmp(name, "床物")) return F_OBJECT;
    if (!strcmp(name, "壁")) return wallField[dir & 3];
    if (!strcmp(name, "移動")) return wallField[dir & 3] + 1;
    return -1;
}

int sa_dungeon_get(const Dungeon *d, const char *name, int x, int y, int dir) {
    const i32 *c = cell_at(d, x, y);
    int f = field_of(name, dir);
    return c && f >= 0 ? c[f] : -1;
}

void sa_dungeon_set(Dungeon *d, const char *name, int value, int x, int y, int mode, int dir) {
    int f = field_of(name, dir);
    if (f < 0 || !cell_at(d, x, y)) return;
    d->cell[y * DUN_SIDE + x][f] = value;
    if (mode == 2 && f != F_OBJECT) {
        int nx = x + stepX[dir & 3], ny = y + stepY[dir & 3];
        int g = field_of(name, dir + 2);
        if (cell_at(d, nx, ny)) d->cell[ny * DUN_SIDE + nx][g] = value;
    }
}

int sa_dungeon_object(const Dungeon *d) {
    const i32 *c = cell_at(d, d->x, d->y);
    return c ? c[F_OBJECT] : -1;
}

int sa_dungeon_crystal_index(const Dungeon *d) {
    int n = 0;
    for (int c = 0; c < DUN_CELLS; c++) {
        if (c == d->y * DUN_SIDE + d->x) return n;
        if (d->cell[c][F_OBJECT] == OBJECT_CRYSTAL) n++;
    }
    return n;
}

void sa_dungeon_clear_object(Dungeon *d) {
    if (cell_at(d, d->x, d->y)) d->cell[d->y * DUN_SIDE + d->x][F_OBJECT] = -1;
}

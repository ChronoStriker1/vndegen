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
    FOG_PULSE_MS = 4000,
    ACTION_X = 1101,
    ACTION_Y = 498,
    ACTION_W = 160,
    ACTION_H = 40,
    HOVER_ACTION = 101,
    FLOOR_DOWN = 100,
    FLOOR_UP = 101,
    OBJECT_CRYSTAL = 60,
    OBJECT_DARK = 41,
    LINK_INTS = 11,
    HEADER_ROUTES = 32,
    HEADER_LINKS = 36,
    HEADER_CRYSTALS = 44,
    TRAILER_START_BYTES = 12,
    PLATE_X = 0,
    PLATE_Y = 38,
    PLATE_SRC_Y = 20,
    PLATE_W = 80,
    PLATE_H = 20,
    PLATE_TEXT_X = 4,
    PLATE_TEXT_Y = 5,
    PLATE_NUMBER_SIZE = 12,
    PLATE_NUMBER_SPACING = 3,
    PLATE_NUMBER_Y = 5,
    PLATE_TEXT_SIZE = 10,
    PLATE_NUMBER_RIGHT = 74,
    PANEL_X = 0,
    PANEL_Y = 0,
    FOLD_X = 139,
    FOLD_Y = 0,
    FOLD_SIZE = 30,
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
    DUN_BIG_ICONS = 48,
    DIGIT_W = 10,
    STEPS_X = 994,
    STEPS_Y = 72,
    PAD_W = 64,
    HOVER_REST = 100,
    HOVER_WALK = 200,
    WALK_ALPHA = 128,
};

enum { PAD_FORWARD, PAD_BACK, PAD_RIGHT, PAD_LEFT, PAD_CORNER_L, PAD_CORNER_R };

static const struct {
    const char *art;
    int x, y, pad;
} walkArrows[DUN_WALKS] = {{"dgn_e00.agf", 469, 164, PAD_FORWARD}, {"dgn_e01.agf", 469, 360, PAD_BACK},
                           {"dgn_e03.agf", 350, 262, PAD_LEFT},    {"dgn_e02.agf", 640, 262, PAD_RIGHT},
                           {"dgn_e05.agf", 249, 275, PAD_CORNER_L}, {"dgn_e04.agf", 929, 275, PAD_CORNER_R}};

#define CELL 8.0f
#define CAM_Y 3.2f
#define CAM_BACK 4.6f
#define CAM_PITCH (-0.08f)
#define FOV 47.2f
#define AMBIENT (235.0f / 255.0f)
#define AMBIENT_DIM (225.0f / 255.0f)
#define DIFFUSE (35.0f / 255.0f)
#define FOG_DENSITY 0.0475f
#define AMBIENT_ALL (192.0f / 255.0f)
#define POLE_SIDE (DUN_SIDE + 1)
#define QUARTER 1.5707963f
#define VIEW_SLOPE 0.75f

static const float lights[2][3] = {{0.46f, 0.3f, -0.44f}, {-0.46f, 0.3f, -0.44f}};
static const int wallField[4] = {F_WALL_N, F_WALL_E, F_WALL_S, F_WALL_W};
static const int stepX[4] = {0, 1, 0, -1}, stepY[4] = {-1, 0, 1, 0};
static const int sideQuarter[4] = {-1, 0, 1, 2};

static const struct {
    int x, y, h;
} padPos[6] = {{1150, 535, 63}, {1150, 613, 63}, {1186, 574, 63}, {1114, 574, 63}, {1111, 539, 67}, {1190, 539, 67}};

static const struct {
    int object, icon;
} trapIcons[DUN_TRAP_ICONS] = {{1, 121},  {2, 122},  {3, 123},  {4, 124},  {5, 125},  {6, 126},  {10, 101}, {12, 127}, {13, 127}, {20, 102},
                               {21, 103}, {22, 105}, {23, 106}, {24, 108}, {25, 109}, {30, 104}, {32, 114}, {39, 107}, {41, 200}, {-1, 0}};

static int trap_icon(int object) {
    for (int i = 0; i < DUN_TRAP_ICONS; i++)
        if (trapIcons[i].object == object && object >= 0) return i;
    return -1;
}

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

static i32 trailer_int(const u8 *data, size_t n, size_t *at, bool *ok) {
    if (*at + 4 > n) {
        *ok = false;
        return 0;
    }
    i32 v = (i32)rd32(data + *at);
    *at += 4;
    return v;
}

static void read_trailer(Dungeon *d, const u8 *data, size_t n) {
    d->linkCount = d->routeCount = d->crystalCount = 0;
    size_t at = MAP_HEADER + (size_t)(d->w * d->h) * CELL_BYTES;
    bool ok = true;
    int routes = (int)rd32(data + HEADER_ROUTES), links = (int)rd32(data + HEADER_LINKS), crystals = (int)rd32(data + HEADER_CRYSTALS);
    trailer_int(data, n, &at, &ok);
    for (int r = 0; r < routes && ok; r++) {
        int count = trailer_int(data, n, &at, &ok);
        DunRoute *rt = d->routeCount < DUN_ROUTES ? &d->route[d->routeCount++] : NULL;
        if (rt) rt->count = 0;
        for (int k = 0; k < count && ok; k++) {
            int v[4];
            for (int j = 0; j < 4; j++) v[j] = trailer_int(data, n, &at, &ok);
            if (rt && rt->count < DUN_ROUTE_POINTS) memcpy(rt->point[rt->count++], v, sizeof v);
        }
    }
    at += TRAILER_START_BYTES;
    for (int i = 0; i < links && ok; i++) {
        i32 v[LINK_INTS];
        for (int k = 0; k < LINK_INTS; k++) v[k] = trailer_int(data, n, &at, &ok);
        bool same = v[3] == -1, other = (v[3] & 0xff) == 0 && v[3] >> 16 >= 1 && v[3] >> 16 <= 10;
        if (!ok || d->linkCount >= DUN_LINKS || (v[0] != DUN_LINK_STAIRS && v[0] != DUN_LINK_TELEPORT) || (!same && !other)) continue;
        if (v[1] < 0 || v[1] >= d->w || v[2] < 0 || v[2] >= d->h || v[4] < 0 || v[4] >= DUN_SIDE || v[5] < 0 || v[5] >= DUN_SIDE || v[6] < 0 || v[6] > 3) continue;
        d->link[d->linkCount++] = (DunLink){v[0], v[1], v[2], v[3], v[4], v[5], v[6]};
    }
    for (int i = 0; i < crystals && ok; i++) {
        int x = trailer_int(data, n, &at, &ok), y = trailer_int(data, n, &at, &ok);
        if (ok && d->crystalCount < DUN_CRYSTALS) {
            d->crystal[d->crystalCount][0] = x;
            d->crystal[d->crystalCount++][1] = y;
        }
    }
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
    read_trailer(d, data, n);
    free(data);
    return true;
}

static float def_value(const char *text, const char *key, float fallback) {
    size_t n = strlen(key);
    for (const char *p = text; *p; p++) {
        if ((p != text && p[-1] != '\n') || strncmp(p, key, n) || (p[n] != ' ' && p[n] != '\t' && p[n] != '=')) continue;
        const char *q = strchr(p, '=');
        return q ? strtof(q + 1, NULL) : fallback;
    }
    return fallback;
}

static bool def_string(const char *text, const char *key, char *out, size_t size) {
    size_t n = strlen(key);
    for (const char *p = text; *p; p++) {
        if ((p != text && p[-1] != '\n') || strncmp(p, key, n) || (p[n] != ' ' && p[n] != '\t' && p[n] != '=')) continue;
        const char *q = strchr(p, '=');
        if (!q) return false;
        q++;
        while (*q == ' ' || *q == '\t') q++;
        size_t k = 0;
        while (q[k] && q[k] != ';' && q[k] != ' ' && q[k] != '\t' && q[k] != '\r' && q[k] != '\n' && k + 1 < size) {
            out[k] = q[k];
            k++;
        }
        out[k] = 0;
        return k > 0;
    }
    return false;
}

static void load_effects(Dungeon *d, Vfs *vfs) {
    d->fogTex = -1;
    for (int i = 0; i < d->set.pvm.count; i++) {
        const char *name = d->set.pvm.layer[i].name;
        if (strncmp(name, ">e,", 3)) continue;
        char file[64];
        snprintf(file, sizeof file, "%s.def", name + 3);
        size_t n;
        u8 *data = sa_vfs_read(vfs, file, &n);
        if (!data) continue;
        char *text = sa_alloc(n + 1);
        memcpy(text, data, n);
        text[n] = 0;
        free(data);
        char type[32], tex[48];
        if (def_string(text, "type", type, sizeof type) && !strcmp(type, "FogFoot") && def_string(text, "TexFile", tex, sizeof tex)) {
            d->fogTex = texture_index(d, vfs, tex);
            d->fogSpeed[0] = def_value(text, "texSpdX", 0);
            d->fogSpeed[1] = def_value(text, "texSpdY", 0);
            d->fogAlpha[0] = def_value(text, "AlphaMin", 0);
            d->fogAlpha[1] = def_value(text, "AlphaMax", 0);
            d->fogHeight = def_value(text, "HighAdd", 0);
        }
        free(text);
    }
}

bool sa_dungeon_load(Dungeon *d, Vfs *vfs, const char *set, const char *map) {
    free_level(d);
    if (!load_set(d, vfs, &d->set, set)) return false;
    load_effects(d, vfs);
    load_set(d, vfs, &d->common, "mallset.pvm");
    if (!sa_dungeon_read_map(d, vfs, map)) return false;
    if (!d->r3.zbuf) r3_init(&d->r3, SA_W, SA_H);
    d->fov = FOV;
    d->r3.fogDensity = FOG_DENSITY;
    d->r3.fogColor = 0x1e2d28;
    d->r3.cull = -1;
    d->ambient = AMBIENT_ALL;
    memset(d->own, 0, sizeof d->own);
    memset(d->ownPassage, 0, sizeof d->ownPassage);
    memset(d->seen, 0, sizeof d->seen);
    d->blind = false;
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

static Image *lower_half(Image *im) {
    if (!im) return NULL;
    Image *out = sa_image_new(im->w, im->h / 2);
    memcpy(out->px, im->px + (size_t)(im->h - out->h) * im->w, sizeof(u32) * (size_t)out->w * out->h);
    sa_image_free(im);
    return out;
}

void sa_dungeon_art(Dungeon *d, Vfs *vfs, const char *platform) {
    if (!d->art[DUN_ART_PANEL]) {
        static const char *const names[] = {"dgn_b00.agf", "dgn_c00.agf", "dgn_c01.agf", "dgn_c02.agf", "dgn_c03.agf", "dgn_c04.agf",
                                            "dgn_c05.agf", "dgn_c06.agf", "dgn_d00.agf", "dgn_d01.agf", "dgn_g02.agf", "dgn_g01.agf",
                                            "dgn_g00.agf", "dgn_g07.agf", "dgn_f00.agf", "dgn_b01.agf"};
        for (int i = 0; i < DUN_ART_PLATFORM; i++) d->art[i] = sa_load_image(vfs, names[i]);
        for (int q = 0; q < 4; q++) d->needle[q] = rotate_quarter(d->art[DUN_ART_NEEDLE], q);
        static const char *const icons[] = {"mmf0003.agf", "mmf0004.agf", "mma0001.agf", "mma0000.agf"};
        for (int i = 0; i < DUN_ICONS; i++) d->icon[i] = scale2(sa_load_image(vfs, icons[i]));
        for (int q = 0; q < 4; q++) d->arrow[q] = rotate_quarter(d->icon[DUN_ICON_PARTY], q);
        for (int i = 0; i < DUN_TRAP_ICONS; i++) {
            if (trapIcons[i].object < 0) continue;
            char name[16];
            snprintf(name, sizeof name, "mmf%04d.agf", trapIcons[i].icon);
            d->trapIcon[i] = scale2(sa_load_image(vfs, name));
        }
        d->plates = sa_load_image(vfs, "dgn_b02.agf");
        for (int i = 0; i < DUN_WALKS; i++) d->walk[i] = lower_half(sa_load_image(vfs, walkArrows[i].art));
    }
    sa_image_free(d->art[DUN_ART_PLATFORM]);
    d->art[DUN_ART_PLATFORM] = platform ? sa_load_image(vfs, platform) : NULL;
}

void sa_dungeon_free(Dungeon *d) {
    free_level(d);
    for (int i = 0; i < DUN_ART; i++) sa_image_free(d->art[i]);
    for (int q = 0; q < 4; q++) sa_image_free(d->needle[q]);
    for (int i = 0; i < DUN_ICONS; i++) sa_image_free(d->icon[i]);
    for (int i = 0; i < DUN_TRAP_ICONS; i++) sa_image_free(d->trapIcon[i]);
    sa_image_free(d->plates);
    for (int i = 0; i < d->bigCount; i++) sa_image_free(d->big[i]);
    d->bigCount = 0;
    for (int q = 0; q < 4; q++) sa_image_free(d->arrow[q]);
    for (int i = 0; i < DUN_WALKS; i++) sa_image_free(d->walk[i]);
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

int sa_dungeon_group_step(const Dungeon *d, int x, int y, int side) {
    const i32 *c = cell_at(d, x, y), *n = cell_at(d, x + stepX[side & 3], y + stepY[side & 3]);
    if (!c || !n || c[F_FLOOR] < 0 || n[F_FLOOR] < 0 || c[wallField[side & 3]] >= 0) return -1;
    return (y + stepY[side & 3]) * DUN_SIDE + x + stepX[side & 3];
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
    d->passCell = from;
    d->passSide = side;
    return true;
}

static void turn(Dungeon *d, int by) {
    d->dir = (d->dir + by) & 3;
    start_anim(d);
    d->passCell = -1;
    d->toYaw = d->yaw + (float)by * QUARTER;
}

static int act(Dungeon *d, int pad) {
    if (d->animDur) return DUN_STAY;
    switch (pad) {
    case PAD_FORWARD: return walk(d, d->dir) ? DUN_MOVED : DUN_STAY;
    case PAD_BACK: return walk(d, (d->dir + 2) & 3) ? DUN_MOVED : DUN_STAY;
    case PAD_RIGHT: turn(d, 1); return DUN_TURNED;
    case PAD_LEFT: turn(d, -1); return DUN_TURNED;
    case PAD_CORNER_L: return walk(d, (d->dir + 3) & 3) ? DUN_MOVED : DUN_STAY;
    case PAD_CORNER_R: return walk(d, (d->dir + 1) & 3) ? DUN_MOVED : DUN_STAY;
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

static bool on_art(const Image *im, int sy, int fh, int ax, int ay, int x, int y, u32 minAlpha) {
    if (!im || x < ax || y < ay || x >= ax + im->w || y >= ay + fh) return false;
    return im->px[(size_t)(sy + y - ay) * im->w + (x - ax)] >> 24 > minAlpha;
}

static int hit(const Dungeon *d, int x, int y) {
    if (x >= REST_X && x < REST_X + REST_W && y >= REST_Y && y < REST_Y + REST_H) return HOVER_REST;
    if (sa_dungeon_action(d) && x >= ACTION_X && x < ACTION_X + ACTION_W && y >= ACTION_Y && y < ACTION_Y + ACTION_H) return HOVER_ACTION;
    for (int i = 0; i < 6; i++) {
        int h = padPos[i].h;
        if (on_art(d->art[DUN_ART_PAD0 + i], 0, h, padPos[i].x, padPos[i].y, x, y, 128)) return i;
        if (i >= PAD_CORNER_L && on_art(d->art[DUN_ART_PAD0 + i], h, h, padPos[i].x, padPos[i].y + h, x, y, 128)) return i;
    }
    for (int i = 0; i < DUN_WALKS; i++)
        if (d->walk[i] && on_art(d->walk[i], 0, d->walk[i]->h, walkArrows[i].x, walkArrows[i].y, x, y, 0)) return HOVER_WALK + i;
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
    if (h >= HOVER_WALK) return act(d, walkArrows[h - HOVER_WALK].pad);
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

static float shade_of(const Dungeon *d, const float *n) {
    float s = d->ambient < AMBIENT_ALL ? AMBIENT_DIM : AMBIENT;
    for (int i = 0; i < 2; i++) {
        float l = sqrtf(lights[i][0] * lights[i][0] + lights[i][1] * lights[i][1] + lights[i][2] * lights[i][2]);
        float dot = (n[0] * lights[i][0] + n[1] * lights[i][1] + n[2] * lights[i][2]) / l;
        if (dot > 0) s += DIFFUSE * dot;
    }
    return s;
}

static void draw_model(Dungeon *d, const PartSet *ps, const Cam *c, const char *name, float cx, float cz, float a, float swing) {
    int i = sa_pvm_find(&ps->pvm, name);
    if (i < 0) return;
    float ca = cosf(a), sa = sinf(a);
    for (int k = i; k <= ps->last[i]; k++) {
        const PvmLayer *ly = &ps->pvm.layer[k];
        if (ly->name[0] == '>' || (k != i && !strncmp(ly->name, "next", 4))) continue;
        if (ly->hasTranslate && swing < 0) continue;
        float hinge = ly->hasTranslate ? (ly->translate[2] > 0 ? -swing : swing) : 0, hc = cosf(hinge), hs = sinf(hinge);
        const Image *tex = ly->tcount && ps->layerTex[k] >= 0 ? d->tex[ps->layerTex[k]] : NULL;
        for (int t = 0; t < ly->tcount; t++) {
            R3Vert v[3];
            bool bad = false;
            for (int j = 0; j < 3; j++) bad = bad || ly->tri[t * 3 + j] < 0 || ly->tri[t * 3 + j] >= ly->vcount;
            if (bad) continue;
            for (int j = 0; j < 3; j++) {
                const PvmVertex *pv = &ly->v[ly->tri[t * 3 + j]];
                float px = pv->pos[0] * hc + pv->pos[2] * hs, pz = -pv->pos[0] * hs + pv->pos[2] * hc;
                float nx = pv->normal[0] * hc + pv->normal[2] * hs, nz = -pv->normal[0] * hs + pv->normal[2] * hc;
                if (ly->hasTranslate) {
                    px += ly->translate[0];
                    pz += ly->translate[2];
                }
                float wx = px * ca + pz * sa + cx, wy = pv->pos[1] + (ly->hasTranslate ? ly->translate[1] : 0), wz = -px * sa + pz * ca + cz;
                float n[3] = {nx * ca + nz * sa, pv->normal[1], -nx * sa + nz * ca};
                float dx = wx - c->pos[0], dy = wy - c->pos[1], dz = wz - c->pos[2];
                v[j].x = dx * c->r[0] + dy * c->r[1] + dz * c->r[2];
                v[j].y = dx * c->u[0] + dy * c->u[1] + dz * c->u[2];
                v[j].z = dx * c->f[0] + dy * c->f[1] + dz * c->f[2];
                float cn[3] = {n[0] * c->r[0] + n[1] * c->r[1] + n[2] * c->r[2], n[0] * c->u[0] + n[1] * c->u[1] + n[2] * c->u[2],
                               n[0] * c->f[0] + n[1] * c->f[1] + n[2] * c->f[2]};
                v[j].u = pv->uv[0];
                v[j].v = pv->uv[1];
                v[j].shade = shade_of(d, cn);
            }
            r3_tri(&d->r3, v, tex);
        }
    }
}

static float spin(const Dungeon *d) { return (float)(d->clock % DOOR_TURN_MS) * 6.2831853f / DOOR_TURN_MS; }

static void draw_part(Dungeon *d, const PartSet *ps, const Cam *c, const char *name, float cx, float cz, int quarter) {
    draw_model(d, ps, c, name, cx, cz, (float)quarter * QUARTER, 0);
}

static float leaf_swing(const Dungeon *d, int cellIndex, int side) {
    if (!d->animDur || d->passCell < 0) return 0;
    if (cellIndex == d->passCell && side == d->passSide) return QUARTER * fminf(1, (float)d->animMs / (float)d->animDur);
    int px = d->passCell % DUN_SIDE + stepX[d->passSide], py = d->passCell / DUN_SIDE + stepY[d->passSide];
    return cellIndex == py * DUN_SIDE + px && side == ((d->passSide + 2) & 3) ? -1 : 0;
}

static void draw_fog_foot(Dungeon *d, const Cam *c, const int *order, int n) {
    if (d->fogTex < 0) return;
    float frames = (float)d->clock * 60.0f / 1000.0f;
    float su = d->fogSpeed[0] * frames, sv = d->fogSpeed[1] * frames;
    float pulse = 0.5f + 0.5f * sinf((float)(d->clock % FOG_PULSE_MS) * 6.2831853f / FOG_PULSE_MS);
    d->r3.blend = (d->fogAlpha[0] + (d->fogAlpha[1] - d->fogAlpha[0]) * pulse) / 255.0f;
    static const float corner[6][2] = {{-1, 1}, {1, 1}, {1, -1}, {-1, 1}, {1, -1}, {-1, -1}};
    for (int i = 0; i < n; i++) {
        int row = order[i] / DUN_SIDE, col = order[i] % DUN_SIDE;
        if (d->cell[order[i]][F_FLOOR] < 0) continue;
        float cx = (float)col * CELL, cz = -(float)row * CELL;
        R3Vert v[6];
        for (int j = 0; j < 6; j++) {
            float wx = cx + corner[j][0] * CELL * 0.5f, wz = cz + corner[j][1] * CELL * 0.5f;
            float dx = wx - c->pos[0], dy = d->fogHeight - c->pos[1], dz = wz - c->pos[2];
            v[j].x = dx * c->r[0] + dy * c->r[1] + dz * c->r[2];
            v[j].y = dx * c->u[0] + dy * c->u[1] + dz * c->u[2];
            v[j].z = dx * c->f[0] + dy * c->f[1] + dz * c->f[2];
            v[j].u = wx / CELL + su;
            v[j].v = -wz / CELL + sv;
            v[j].shade = 1;
        }
        r3_tri(&d->r3, v, d->tex[d->fogTex]);
        r3_tri(&d->r3, v + 3, d->tex[d->fogTex]);
    }
    d->r3.blend = 0;
}

void sa_dungeon_draw(Dungeon *d, u32 *fb) {
    u32 fog = d->r3.fogColor;
    u32 bg = 0xff000000u | fog;
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = bg;
    if (!d->loaded || d->blind) return;
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
    bool pole[POLE_SIDE * POLE_SIDE];
    memset(pole, 0, sizeof pole);
    for (int i = 0; i < n; i++) {
        int row = order[i] / DUN_SIDE, col = order[i] % DUN_SIDE;
        const i32 *cell = d->cell[row * DUN_SIDE + col];
        if (cell[F_FLOOR] < 0) continue;
        for (int s = 0; s < 4; s++) {
            int w = cell[wallField[s]];
            if (w < 0 || w == WALL_PASSAGE) continue;
            int ax = col + (s == DIR_E), ay = row + (s == DIR_S), bx = col + (s != DIR_W), by = row + (s != DIR_N);
            pole[ay * POLE_SIDE + ax] = pole[by * POLE_SIDE + bx] = true;
        }
    }
    for (int i = 0; i < n; i++) {
        int row = order[i] / DUN_SIDE, col = order[i] % DUN_SIDE;
        const i32 *cell = d->cell[row * DUN_SIDE + col];
        if (cell[F_FLOOR] < 0) continue;
        for (int k = 0; k < 4; k++) {
            int px = col + (k & 1), py = row + (k >> 1);
            if (!pole[py * POLE_SIDE + px]) continue;
            pole[py * POLE_SIDE + px] = false;
            draw_part(d, &d->set, &c, "p000", ((float)px - 0.5f) * CELL, -((float)py - 0.5f) * CELL, 0);
        }
        float cx = (float)col * CELL, cz = -(float)row * CELL;
        char name[16];
        snprintf(name, sizeof name, "f%03d", cell[F_FLOOR]);
        draw_part(d, &d->set, &c, name, cx, cz, 0);
        draw_part(d, &d->set, &c, "r000", cx, cz, 0);
        for (int s = 0; s < 4; s++) {
            int w = cell[wallField[s]];
            if (w == WALL_PASSAGE) draw_part(d, &d->set, &c, "next000", cx, cz, sideQuarter[s]);
            if (w < 0 || w == WALL_PASSAGE) continue;
            snprintf(name, sizeof name, "w%03d", w);
            draw_model(d, &d->set, &c, name, cx, cz, (float)sideQuarter[s] * QUARTER, leaf_swing(d, order[i], s));
        }
        if (cell[F_OBJECT] >= 0) {
            snprintf(name, sizeof name, "a%03d", cell[F_OBJECT]);
            draw_model(d, &d->common, &c, name, cx, cz, cell[F_OBJECT] == OBJECT_DOOR ? spin(d) : 0, 0);
        }
    }
    draw_fog_foot(d, &c, order, n);
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
    if (sa_dungeon_in_dark(d)) return;
    if (d->blind) {
        if (d->arrow[d->dir]) sa_blit(fb, icon_at(d, d->arrow[d->dir], scale), ox + MINI_CELLS / 2 * cs, oy + MINI_CELLS / 2 * cs);
        return;
    }
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
            int trap = trap_icon(cell[F_OBJECT]);
            if (trap >= 0 && d->trapIcon[trap] && (cell[F_OBJECT] == OBJECT_DARK || d->seen[my * DUN_SIDE + mx]))
                sa_blit(fb, icon_at(d, d->trapIcon[trap], scale), x0, y0);
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
    if (d->hover >= HOVER_WALK) sa_blit_alpha(fb, d->walk[d->hover - HOVER_WALK], walkArrows[d->hover - HOVER_WALK].x, walkArrows[d->hover - HOVER_WALK].y, WALK_ALPHA);
    sa_blit(fb, d->art[DUN_ART_PANEL], PANEL_X, PANEL_Y);
    sa_blit_rect(fb, d->art[DUN_ART_FOLD], FOLD_SIZE, 0, FOLD_SIZE, FOLD_SIZE, FOLD_X, FOLD_Y);
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
        int lit = d->hover == i ? 1 : 0;
        if (i < PAD_CORNER_L) sa_blit_rect(fb, im, 0, lit * h, PAD_W, h, padPos[i].x, padPos[i].y);
        else
            for (int k = 0; k < 2; k++) sa_blit_rect(fb, im, 0, (lit * 2 + k) * h, PAD_W, h, padPos[i].x, padPos[i].y + k * h);
    }
    sa_blit_rect(fb, d->art[DUN_ART_REST], 0, d->hover == HOVER_REST ? REST_H : 0, REST_W, REST_H, REST_X, REST_Y);
    static const int actionArt[] = {0, DUN_ART_HOME, DUN_ART_DOWN, DUN_ART_UP, DUN_ART_OPEN};
    int act = sa_dungeon_action(d);
    if (act) sa_blit_rect(fb, d->art[actionArt[act]], 0, d->hover == HOVER_ACTION ? ACTION_H : 0, ACTION_W, ACTION_H, ACTION_X, ACTION_Y);
    for (int i = 0; i < d->statusCount && d->plates; i++) {
        int y = PLATE_Y + i * PLATE_H;
        sa_blit_rect(fb, d->plates, 0, PLATE_SRC_Y, PLATE_W, PLATE_H, PLATE_X, y);
        char n[12];
        snprintf(n, sizeof n, "%d", d->status[i].steps);
        sa_font_set_plain(true);
        sa_font_set_spacing(0);
        sa_text_draw(fb, SA_W, SA_H, d->status[i].label, -1, PLATE_X + PLATE_TEXT_X, y + PLATE_TEXT_Y, PLATE_TEXT_SIZE, 0xffffff, 0xffffffffu);
        sa_font_set_plain(false);
        sa_font_set_spacing(PLATE_NUMBER_SPACING);
        sa_text_draw(fb, SA_W, SA_H, n, -1, PLATE_NUMBER_RIGHT - sa_text_width(n, -1, PLATE_NUMBER_SIZE) + PLATE_NUMBER_SPACING, y + PLATE_NUMBER_Y, PLATE_NUMBER_SIZE, 0xffffff, 0xffffffffu);
        sa_font_set_spacing(1);
    }
    if (party) sa_party_draw_diorama(party, fb, rankLimit, d->art[DUN_ART_PLATFORM]);
}

const Image *sa_dungeon_platform(const Dungeon *d) { return d->art[DUN_ART_PLATFORM]; }

void sa_dungeon_draw_frame(Dungeon *d, u32 *fb, struct Party *party, int rankLimit) {
    sa_blit(fb, d->art[DUN_ART_PANEL], PANEL_X, PANEL_Y);
    sa_blit_rect(fb, d->art[DUN_ART_FOLD], FOLD_SIZE, 0, FOLD_SIZE, FOLD_SIZE, FOLD_X, FOLD_Y);
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
    for (int i = 0; i < d->crystalCount; i++)
        if (d->crystal[i][0] == d->x && d->crystal[i][1] == d->y) return i;
    int n = 0;
    for (int c = 0; c < DUN_CELLS; c++) {
        if (c == d->y * DUN_SIDE + d->x) return n;
        if (d->cell[c][F_OBJECT] == OBJECT_CRYSTAL) n++;
    }
    return n;
}

bool sa_dungeon_link(const Dungeon *d, int kind, DunLink *out) {
    for (int i = 0; i < d->linkCount; i++)
        if (d->link[i].kind == kind && d->link[i].x == d->x && d->link[i].y == d->y) {
            *out = d->link[i];
            return true;
        }
    return false;
}

void sa_dungeon_reveal(Dungeon *d) {
    if (cell_at(d, d->x, d->y)) d->seen[d->y * DUN_SIDE + d->x] = true;
}

bool sa_dungeon_in_dark(const Dungeon *d) { return sa_dungeon_object(d) == OBJECT_DARK; }

void sa_dungeon_clear_object(Dungeon *d) {
    if (cell_at(d, d->x, d->y)) d->cell[d->y * DUN_SIDE + d->x][F_OBJECT] = -1;
}

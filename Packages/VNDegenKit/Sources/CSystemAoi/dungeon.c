#include "sa.h"
#include <math.h>

enum {
    MAP_W = 15,
    MAP_H = 15,
    MAP_HEADER = 48,
    CELL_BYTES = 104,
    F_FLOOR = 5,
    F_WALL0 = 9,
    WALL_STEP = 3,
    VIEW_CELLS = 6,
};

#define CELL 8.0f
#define CAM_Y 3.2f
#define CAM_BACK 4.6f
#define CAM_PITCH (-0.08f)

static const float lights[2][3] = {{10, 4.5f, 30}, {-10, 4.5f, 30}};

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

bool sa_dungeon_load(Dungeon *d, Vfs *vfs, const char *set, const char *map) {
    sa_dungeon_free(d);
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
    for (int c = 0; c < DUN_CELLS; c++)
        for (int f = 0; f < DUN_FIELDS; f++) d->cell[c][f] = (i32)rd32(data + MAP_HEADER + (size_t)c * CELL_BYTES + (size_t)f * 4);
    free(data);
    if (!d->r3.zbuf) r3_init(&d->r3, SA_W, SA_H);
    d->fov = getenv("SA_FOV") ? (float)atof(getenv("SA_FOV")) : 40;
    d->r3.fogNear = getenv("SA_FOGN") ? (float)atof(getenv("SA_FOGN")) : 4;
    d->r3.fogFar = getenv("SA_FOGF") ? (float)atof(getenv("SA_FOGF")) : 24;
    d->r3.fogColor = getenv("SA_FOGC") ? (u32)strtoul(getenv("SA_FOGC"), NULL, 16) : 0x1e2a28;
    d->r3.cull = getenv("SA_CULL") ? atoi(getenv("SA_CULL")) : 1;
    d->loaded = true;
    return true;
}

void sa_dungeon_free(Dungeon *d) {
    R3 keep = d->r3;
    sa_pvm_free(&d->set);
    free(d->last);
    free(d->layerTex);
    for (int i = 0; i < d->texCount; i++) sa_image_free(d->tex[i]);
    memset(d, 0, sizeof *d);
    d->r3 = keep;
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
    return s;
}

static void draw_part(Dungeon *d, const Cam *c, const char *name, float cx, float cz, int quarter) {
    int i = sa_pvm_find(&d->set, name);
    if (i < 0) return;
    float a = (float)quarter * 1.5707963f, ca = cosf(a), sa = sinf(a);
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

static const int sideQuarter[4] = {-1, 0, 1, 2};

void sa_dungeon_draw(Dungeon *d, u32 *fb) {
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u | d->r3.fogColor;
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
                int w = cell[F_WALL0 + s * WALL_STEP];
                if (w < 0) continue;
                snprintf(name, sizeof name, "w%03d", w);
                draw_part(d, &c, name, cx, cz, sideQuarter[s]);
            }
        }
}

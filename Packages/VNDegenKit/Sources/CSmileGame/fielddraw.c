#include "sg.h"

#include <math.h>
#include <time.h>

#define CHUNK 8
#define BORDER 12
#define WATER_DROP 0.25f
#define TILE 48.0f

static const float kDefaultPitch = -48.0f, kDefaultDistance = 3.5f, kDefaultFov = 70.0f;
static const float kDistScale = 20.0f, kFovScale = 0.1f;
static const float kFillK = 0.77f, kKeyK = 0.74f;
static const int kShadowTexels = 12;
static const float kSpriteForward = 0.235f;

bool maplook_parse(MapLook *l, const SgrRec *rec) {
    memset(l, 0, sizeof *l);
    Rd r = rd_make(rec->body, rec->size);
    rd_guid(&r);
    free(rd_str(&r));
    rd_i32(&r);
    free(rd_str(&r));
    rd_i64(&r);
    i32 n = rd_i32(&r);
    if (n < 0 || n > 100000) return false;
    rd_skip(&r, 24 * (size_t)n);
    rd_i32(&r);
    n = rd_i32(&r);
    if (n < 0 || n > 10000) return false;
    rd_skip(&r, 24 * (size_t)n);
    l->sky = rd_i32(&r);
    rd_skip(&r, 32);
    i32 w = rd_i32(&r), h = rd_i32(&r);
    if (r.bad || w <= 0 || h <= 0 || w > 4096 || h > 4096) return false;
    rd_skip(&r, 4 * (size_t)w * (size_t)h);
    n = rd_i32(&r);
    if (n < 0 || n > 100000) return false;
    rd_skip(&r, 28 * (size_t)n);
    n = rd_i32(&r);
    if (n < 0 || n > 100000) return false;
    rd_skip(&r, 16 * (size_t)n);
    rd_skip(&r, 32 + 32 + 48 + 4 + 2);
    n = rd_i32(&r);
    if (n < 0 || n > 4096) return false;
    rd_skip(&r, 20 * (size_t)n);
    n = rd_i32(&r);
    if (n < 0 || n > 4096) return false;
    l->slopes = sg_alloc(sizeof(SlopeChip) * (size_t)(n ? n : 1));
    for (int i = 0; i < n && !r.bad; i++) {
        l->slopes[i].index = rd_i32(&r);
        l->slopes[i].id = rd_guid(&r);
    }
    l->slopeCount = n;
    rd_u8(&r);
    l->outsideChip = rd_guid(&r);
    l->outsideHeight = rd_i32(&r);
    l->sun = rd_i32(&r) != 0;
    l->sunPitch = rd_f32(&r);
    l->sunYaw = rd_f32(&r);
    for (int i = 0; i < 3; i++) l->sunColor[i] = rd_i32(&r);
    rd_skip(&r, 4);
    rd_u8(&r);
    l->cameraKind = rd_i32(&r);
    l->hasCamera = rd_u8(&r) != 0;
    if (l->hasCamera) {
        i32 size = rd_i32(&r);
        if (size < 0 || size > 1024) return false;
        for (int i = 0; i < size / 4; i++) {
            float v = rd_f32(&r);
            if (i < 8) l->camera[i] = v;
        }
    }
    return !r.bad;
}

void maplook_free(MapLook *l) {
    free(l->slopes);
    memset(l, 0, sizeof *l);
}

View *sg_view(SG *g) {
    if (!g->r3.view) g->r3.view = sg_alloc(sizeof(View));
    return g->r3.view;
}

static u8 chip_attr(SG *g, Guid id) {
    Game *d = &g->data;
    for (int p = -1; p < d->packCount; p++) {
        Sgr *s = p < 0 ? &d->resFile : &d->packRes[p];
        for (int i = 0; i < s->count; i++) {
            const SgrRec *rec = &s->recs[i];
            if (rec->type != SGR_RESOURCE || rec->sub != RES_CHIP || rec->size < 17 || memcmp(rec->body, id.b, 16) != 0) continue;
            Rd r = rd_make(rec->body, rec->size);
            rd_guid(&r);
            free(rd_str(&r));
            free(rd_str(&r));
            return rd_u8(&r);
        }
    }
    return 0;
}

static bool liquid(u8 attr) { return (attr & 0x0f) == 4 || (attr & 0x0f) == 7; }

static int map_height(const Map *m, const MapLook *l, int x, int z) {
    if (x < 0 || z < 0 || x >= m->w || z >= m->h) return l->outsideHeight;
    return m->height[(size_t)z * (size_t)m->w + (size_t)x];
}

typedef struct Build {
    SG *g;
    Field *f;
    View *v;
    Mesh *op, *al;
    float sun[3];
} Build;

static int chip_slot(const Field *f, int x, int z) {
    const Map *m = &f->map;
    if (x < 0 || z < 0 || x >= m->w || z >= m->h) return f->chipImageCount - 1;
    int c = m->chip[(size_t)z * (size_t)m->w + (size_t)x];
    return c >= 0 && c < f->chipImageCount - 1 ? c : -1;
}

static Image *chip_image(Build *b, int slot) {
    Field *f = b->f;
    if (slot < 0 || slot >= f->chipImageCount) return NULL;
    if (!f->chipImages[slot]) {
        Guid id = slot == f->chipImageCount - 1 ? b->v->look.outsideChip : map_chip_guid(&f->map, slot);
        f->chipImages[slot] = sg_image(b->g, id);
        b->v->chipAttr[slot] = chip_attr(b->g, id);
    }
    return f->chipImages[slot];
}

static float face_shade(const Build *b, float nx, float ny, float nz) {
    float d = -(nx * b->sun[0] + ny * b->sun[1] + nz * b->sun[2]);
    return d > 0 ? d : 0;
}

static void emit(Build *b, const Image *tex, const Vec3 p[], const float uv[][2], int n, float shade, u8 mode, u8 flags) {
    for (int k = 1; k + 1 < n; k++) {
        Tri t = {.tex = tex, .alphaTest = false, .mode = mode, .flags = flags};
        int idx[3] = {0, k, k + 1};
        for (int c = 0; c < 3; c++) t.v[c] = (Vert){p[idx[c]].x, p[idx[c]].y, p[idx[c]].z, uv[idx[c]][0], uv[idx[c]][1], shade};
        mesh_add(mode == TRI_OPAQUE ? b->op : b->al, &t);
    }
}

static const int kDx[4] = {0, 0, -1, 1}, kDz[4] = {1, -1, 0, 0};

static void side_frame(int s, int x, int z, float *ax, float *az, float *bx, float *bz) {
    float fx = (float)x, fz = (float)z;
    switch (s) {
    case 0: *ax = fx, *az = fz + 1, *bx = fx + 1, *bz = fz + 1; break;
    case 1: *ax = fx + 1, *az = fz, *bx = fx, *bz = fz; break;
    case 2: *ax = fx, *az = fz, *bx = fx, *bz = fz + 1; break;
    default: *ax = fx + 1, *az = fz + 1, *bx = fx + 1, *bz = fz; break;
    }
}

static void side_column(Build *b, const Image *im, int s, int x, int z, float top, float bottom, float shade, u8 flags) {
    if (!im || top <= bottom) return;
    float ax, az, bx, bz;
    side_frame(s, x, z, &ax, &az, &bx, &bz);
    int sections = im->h / (int)TILE;
    float th = (float)im->h;
    int unit = 0;
    for (float y = top; y > bottom + 1e-4f; y -= 1.0f, unit++) {
        float y1 = fmaxf(bottom, y - 1.0f);
        int sec = sections >= 3 && unit > 0 ? 2 : sections >= 2 ? 1 : 0;
        if (sections >= 4 && y1 <= bottom + 1e-4f && unit > 0) sec = 3;
        float v0 = (float)sec * TILE / th, v1 = v0 + (y - y1) * TILE / th;
        Vec3 p[4] = {{ax, y, az}, {ax, y1, az}, {bx, y1, bz}, {bx, y, bz}};
        float uv[4][2] = {{0, v0}, {0, v1}, {1, v1}, {1, v0}};
        emit(b, im, p, uv, 4, shade, TRI_OPAQUE, flags);
    }
}

static const float kNrm[4][3] = {{0, 0, 1}, {0, 0, -1}, {-1, 0, 0}, {1, 0, 0}};

static const u8 kCorners[12][4] = {
    {0, 0, 1, 1}, {1, 1, 0, 0}, {0, 1, 1, 0}, {1, 0, 0, 1}, {0, 0, 1, 0}, {1, 0, 0, 0},
    {0, 1, 0, 0}, {0, 0, 0, 1}, {0, 1, 1, 1}, {1, 1, 0, 1}, {1, 1, 1, 0}, {1, 0, 1, 1},
};

static void slope_top(Build *b, const Image *im, int x, int z, float h, int dir, u8 flags) {
    const u8 *c = kCorners[dir];
    float fx = (float)x, fz = (float)z;
    Vec3 P[4] = {{fx, h + c[0], fz}, {fx + 1, h + c[1], fz}, {fx + 1, h + c[2], fz + 1}, {fx, h + c[3], fz + 1}};
    float vt = im && im->h >= 2 * TILE ? TILE / (float)im->h : 1.0f;
    float UV[4][2] = {{0, 0}, {1, 0}, {1, vt}, {0, vt}};
    int rot = dir == 0 ? 2 : dir == 2 ? 1 : dir == 3 ? 3 : 0;
    float R[4][2];
    for (int i = 0; i < 4; i++) R[i][0] = UV[(i + rot) & 3][0], R[i][1] = UV[(i + rot) & 3][1];
    bool diagA = dir == 4 || dir == 5 || dir == 8 || dir == 9;
    int tris[2][3];
    if (diagA) {
        int t0[3] = {0, 3, 1}, t1[3] = {1, 3, 2};
        memcpy(tris[0], t0, sizeof t0);
        memcpy(tris[1], t1, sizeof t1);
    } else {
        int t0[3] = {0, 3, 2}, t1[3] = {0, 2, 1};
        memcpy(tris[0], t0, sizeof t0);
        memcpy(tris[1], t1, sizeof t1);
    }
    for (int t = 0; t < 2; t++) {
        Vec3 p[3];
        float uv[3][2];
        for (int k = 0; k < 3; k++) p[k] = P[tris[t][k]], uv[k][0] = R[tris[t][k]][0], uv[k][1] = R[tris[t][k]][1];
        float ux = p[1].x - p[0].x, uy = p[1].y - p[0].y, uz = p[1].z - p[0].z;
        float vx = p[2].x - p[0].x, vy = p[2].y - p[0].y, vz = p[2].z - p[0].z;
        float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        float l = sqrtf(nx * nx + ny * ny + nz * nz);
        emit(b, im, p, uv, 3, l > 0 ? face_shade(b, nx / l, ny / l, nz / l) : 1, TRI_OPAQUE, flags);
    }
}

static void slope_side(Build *b, const Image *im, int s, int x, int z, float h, float ca, float cb, float floor, u8 flags) {
    if (!im) return;
    float ax, az, bx, bz;
    side_frame(s, x, z, &ax, &az, &bx, &bz);
    float ya = h + ca, yb = h + cb;
    if (ya <= floor + 1e-4f && yb <= floor + 1e-4f) return;
    float base = fmaxf(floor, h);
    float th = (float)im->h, v0 = im->h >= 2 * TILE ? TILE / th : 0, v1 = im->h >= 2 * TILE ? 2 * TILE / th : 1;
    Vec3 p[4];
    float uv[4][2];
    int n = 0;
    p[n] = (Vec3){ax, ya, az}, uv[n][0] = 0, uv[n][1] = v0 + (1 - ca) * (v1 - v0), n++;
    p[n] = (Vec3){ax, base, az}, uv[n][0] = 0, uv[n][1] = v1 - (base - h) * (v1 - v0), n++;
    p[n] = (Vec3){bx, base, bz}, uv[n][0] = 1, uv[n][1] = v1 - (base - h) * (v1 - v0), n++;
    p[n] = (Vec3){bx, yb, bz}, uv[n][0] = 1, uv[n][1] = v0 + (1 - cb) * (v1 - v0), n++;
    if (ya <= base + 1e-4f) {
        p[0] = p[1];
        uv[0][0] = uv[1][0], uv[0][1] = uv[1][1];
        memmove(&p[1], &p[2], sizeof(Vec3) * 2);
        memmove(&uv[1], &uv[2], sizeof uv[0] * 2);
        n = 3;
    } else if (yb <= base + 1e-4f) {
        n = 3;
    }
    emit(b, im, p, uv, n, face_shade(b, kNrm[s][0], kNrm[s][1], kNrm[s][2]), TRI_OPAQUE, flags);
}

static void stairs(Build *b, const Image *im, int x, int z, float h, int dir, u8 flags) {
    float fx = (float)x, fz = (float)z;
    float th = im ? (float)im->h : TILE;
    float ux, uz, sx, sz, ox, oz;
    switch (dir) {
    case 0: ux = 0, uz = 1, sx = 1, sz = 0, ox = fx, oz = fz; break;
    case 1: ux = 0, uz = -1, sx = -1, sz = 0, ox = fx + 1, oz = fz + 1; break;
    case 2: ux = 1, uz = 0, sx = 0, sz = -1, ox = fx, oz = fz + 1; break;
    default: ux = -1, uz = 0, sx = 0, sz = 1, ox = fx + 1, oz = fz; break;
    }
    for (int i = 0; i < 3; i++) {
        float t0 = (float)i / 3, t1 = (float)(i + 1) / 3, y0 = h + t0, y1 = h + t1;
        float tv0 = (1 - t1) * TILE / th, tv1 = (1 - t0) * TILE / th;
        Vec3 riser[4] = {{ox + ux * t0, y1, oz + uz * t0}, {ox + ux * t0, y0, oz + uz * t0}, {ox + ux * t0 + sx, y0, oz + uz * t0 + sz},
                         {ox + ux * t0 + sx, y1, oz + uz * t0 + sz}};
        float ruv[4][2] = {{0, TILE / th + tv0}, {0, TILE / th + tv1}, {1, TILE / th + tv1}, {1, TILE / th + tv0}};
        emit(b, im, riser, ruv, 4, face_shade(b, -ux, 0, -uz), TRI_OPAQUE, flags);
        Vec3 tread[4] = {{ox + ux * t1, y1, oz + uz * t1}, {ox + ux * t0, y1, oz + uz * t0}, {ox + ux * t0 + sx, y1, oz + uz * t0 + sz},
                         {ox + ux * t1 + sx, y1, oz + uz * t1 + sz}};
        float tuv[4][2] = {{0, tv0}, {0, tv1}, {1, tv1}, {1, tv0}};
        emit(b, im, tread, tuv, 4, face_shade(b, 0, 1, 0), TRI_OPAQUE, flags);
    }
}

static void stairs_side(Build *b, const Image *im, int s, int x, int z, float h, int dir, float floor, u8 flags) {
    if (!im) return;
    float ax, az, bx, bz;
    side_frame(s, x, z, &ax, &az, &bx, &bz);
    float fx = (float)x, fz = (float)z;
    float th = (float)im->h, v0 = im->h >= 3 * TILE ? 2 * TILE / th : TILE / th;
    for (int i = 0; i < 3; i++) {
        float t0 = (float)i / 3, t1 = (float)(i + 1) / 3, top = h + t1, base = fmaxf(floor, h);
        if (top <= base + 1e-4f) continue;
        float pa[2], pb[2];
        float t[2] = {t0, t1};
        float q[2][2];
        for (int k = 0; k < 2; k++) {
            float tt = t[k];
            switch (dir) {
            case 0: q[k][0] = (s == 2 ? fx : fx + 1), q[k][1] = fz + tt; break;
            case 1: q[k][0] = (s == 2 ? fx : fx + 1), q[k][1] = fz + 1 - tt; break;
            case 2: q[k][0] = fx + tt, q[k][1] = (s == 1 ? fz : fz + 1); break;
            default: q[k][0] = fx + 1 - tt, q[k][1] = (s == 1 ? fz : fz + 1); break;
            }
        }
        float da = (q[0][0] - ax) * (bx - ax) + (q[0][1] - az) * (bz - az);
        float db = (q[1][0] - ax) * (bx - ax) + (q[1][1] - az) * (bz - az);
        if (da <= db) pa[0] = q[0][0], pa[1] = q[0][1], pb[0] = q[1][0], pb[1] = q[1][1];
        else pa[0] = q[1][0], pa[1] = q[1][1], pb[0] = q[0][0], pb[1] = q[0][1];
        float ua = 1 - (da <= db ? t0 : t1), ub = 1 - (da <= db ? t1 : t0);
        float vt = v0 + (1 - t1) * TILE / th, vb = v0 + (1 - (base - h)) * TILE / th;
        Vec3 p[4] = {{pa[0], top, pa[1]}, {pa[0], base, pa[1]}, {pb[0], base, pb[1]}, {pb[0], top, pb[1]}};
        float uv[4][2] = {{ua, vt}, {ua, vb}, {ub, vb}, {ub, vt}};
        emit(b, im, p, uv, 4, face_shade(b, kNrm[s][0], kNrm[s][1], kNrm[s][2]), TRI_OPAQUE, flags);
    }
}

static int cell_slope(const View *v, const Map *m, int x, int z) {
    if (x < 0 || z < 0 || x >= m->w || z >= m->h) return -1;
    return v->slopeDir[(size_t)z * (size_t)m->w + (size_t)x];
}

static float surf_height(Build *b, int x, int z) {
    int h = map_height(&b->f->map, &b->v->look, x, z);
    int slot = chip_slot(b->f, x, z);
    if (slot >= 0) chip_image(b, slot);
    if (slot >= 0 && liquid(b->v->chipAttr[slot])) return (float)h - WATER_DROP;
    return (float)h;
}

static void build_cell(Build *b, int x, int z) {
    Map *m = &b->f->map;
    View *v = b->v;
    int slot = chip_slot(b->f, x, z);
    Image *im = chip_image(b, slot);
    if (!im) return;
    int hi = map_height(m, &v->look, x, z);
    float h = (float)hi;
    u8 attr = slot >= 0 ? v->chipAttr[slot] : 0;
    bool water = liquid(attr);
    float top = water ? h - WATER_DROP : h;
    u8 flags = TF_LIT | TF_SHADOW | TF_CAST | TF_CULL;
    if (water) flags |= TF_ANIM;
    int sdir = cell_slope(v, m, x, z);
    Image *sim = NULL;
    bool isStairs = false;
    if (sdir >= 0) {
        int si = v->slopeIdx[(size_t)z * (size_t)m->w + (size_t)x];
        sim = v->slopeImages[si];
        isStairs = (v->chipAttr[b->f->chipImageCount + si] & 0x10) && sdir < 4;
    }
    float fx = (float)x, fz = (float)z;
    if (sdir >= 0 && sim) {
        if (isStairs) stairs(b, sim, x, z, h, sdir, flags & ~TF_CULL);
        else slope_top(b, sim, x, z, h, sdir, flags);
    } else if (hi > 0 || water || x < 0 || z < 0 || x >= m->w || z >= m->h) {
        float frames = water && im->w > (int)TILE ? (float)(im->w / (int)TILE) : 1.0f;
        float u1 = 1.0f / frames;
        float vt = im->h >= 2 * TILE ? TILE / (float)im->h : 1.0f;
        Vec3 p[4] = {{fx, top, fz}, {fx, top, fz + 1}, {fx + 1, top, fz + 1}, {fx + 1, top, fz}};
        float uv[4][2] = {{0, 0}, {0, vt}, {u1, vt}, {u1, 0}};
        emit(b, im, p, uv, 4, face_shade(b, 0, 1, 0), TRI_OPAQUE, flags);
    }
    for (int s = 0; s < 4; s++) {
        int nx = x + kDx[s], nz = z + kDz[s];
        if (nx < -v->border || nz < -v->border || nx >= m->w + v->border || nz >= m->h + v->border) continue;
        float ns = surf_height(b, nx, nz);
        int nsd = cell_slope(v, m, nx, nz);
        if (nsd >= 0) {
            const u8 *c = kCorners[nsd];
            int ea = s == 0 ? 0 : s == 1 ? 3 : s == 2 ? 1 : 0, eb = s == 0 ? 1 : s == 1 ? 2 : s == 2 ? 2 : 3;
            if (c[ea] && c[eb]) ns += 1.0f;
        }
        float shade = face_shade(b, kNrm[s][0], kNrm[s][1], kNrm[s][2]);
        if (ns < top) {
            if (water) {
                float frames = im->w > (int)TILE ? (float)(im->w / (int)TILE) : 1.0f;
                float ax, az, bx, bz;
                side_frame(s, x, z, &ax, &az, &bx, &bz);
                float th = (float)im->h, v0 = im->h >= 2 * TILE ? TILE / th : 0;
                for (float y = top; y > ns + 1e-4f; y -= 1.0f) {
                    float y1 = fmaxf(ns, y - 1.0f);
                    Vec3 p[4] = {{ax, y, az}, {ax, y1, az}, {bx, y1, bz}, {bx, y, bz}};
                    float uv[4][2] = {{0, v0}, {0, v0 + (y - y1) * TILE / th}, {1 / frames, v0 + (y - y1) * TILE / th}, {1 / frames, v0}};
                    emit(b, im, p, uv, 4, shade, TRI_OPAQUE, flags);
                }
            } else {
                side_column(b, im, s, x, z, h, ns, shade, flags);
            }
        }
        if (sdir >= 0 && sim) {
            const u8 *c = kCorners[sdir];
            static const int ca[4] = {3, 1, 0, 2}, cb[4] = {2, 0, 3, 1};
            if (isStairs) {
                bool perpendicular = ((sdir == 0 || sdir == 1) && s >= 2) || ((sdir == 2 || sdir == 3) && s < 2);
                if (perpendicular) stairs_side(b, sim, s, x, z, h, sdir, ns, flags & ~TF_CULL);
                else if (c[ca[s]] && c[cb[s]] && ns < h + 1) side_column(b, im, s, x, z, h + 1, fmaxf(ns, h), shade, flags);
            } else {
                slope_side(b, sim, s, x, z, h, c[ca[s]], c[cb[s]], ns, flags);
            }
        }
    }
}

static void chunk_add(View *v, int ci, Mesh *op, Mesh *al, int opStart, int alStart) {
    Chunk *c = &v->chunks[ci];
    for (int k = 0; k < 2; k++) {
        Mesh *m = k ? al : op;
        int start = k ? alStart : opStart;
        for (int i = start; i < m->count; i++)
            for (int j = 0; j < 3; j++) {
                const Vert *p = &m->tris[i].v[j];
                c->min[0] = fminf(c->min[0], p->x), c->max[0] = fmaxf(c->max[0], p->x);
                c->min[1] = fminf(c->min[1], p->y), c->max[1] = fmaxf(c->max[1], p->y);
                c->min[2] = fminf(c->min[2], p->z), c->max[2] = fmaxf(c->max[2], p->z);
            }
    }
}

static void add_model(Build *b, Model *mdl, float bx, float by, float bz, int dir) {
    if (!mdl) return;
    const Mesh *mesh = model_mesh(mdl);
    const float *nrm = model_normals(mdl);
    float ang = (float)dir * 1.5707963f;
    float cs = cosf(ang), sn = sinf(ang);
    for (int k = 0; k < mesh->count; k++) {
        Tri t = mesh->tris[k];
        float sh[3] = {1, 1, 1};
        for (int c = 0; c < 3; c++) {
            float x = t.v[c].x, z = t.v[c].z;
            t.v[c].x = bx + x * cs + z * sn;
            t.v[c].z = bz - x * sn + z * cs;
            t.v[c].y += by;
            if (nrm) {
                const float *n = nrm + k * 9 + c * 3;
                float nx = n[0] * cs + n[2] * sn, nz = -n[0] * sn + n[2] * cs;
                sh[c] = face_shade(b, nx, n[1], nz);
            }
            t.v[c].shade = sh[c];
        }
        u8 mode = t.mode ? t.mode : t.modulate ? TRI_MODULATE : TRI_OPAQUE;
        t.mode = mode;
        if (mode == TRI_OPAQUE) t.flags |= TF_LIT | TF_SHADOW | TF_CAST | TF_CULL;
        mesh_add(mode == TRI_OPAQUE && !(t.flags & TF_NOZWRITE) ? b->op : b->al, &t);
    }
}

static void light_setup(SG *g, View *v, float sun[3]) {
    const Map *m = &g->field.map;
    MapLook *l = &v->look;
    float p = l->sunPitch, y = l->sunYaw;
    sun[0] = cosf(p) * cosf(y);
    sun[1] = sinf(p);
    sun[2] = cosf(p) * sinf(y);
    float a[3], s[3];
    for (int i = 0; i < 3; i++) {
        float key = (float)m->colors[1 + i] / 255.0f, fill = (float)m->colors[5 + i] / 255.0f;
        float col = l->sun ? (float)l->sunColor[i] / 255.0f : 0.0f;
        a[i] = fill * kFillK;
        s[i] = key * col * kKeyK;
        if (!l->sun) a[i] = fill * kFillK + key * kKeyK;
    }
    r3_light(&g->r3, a, s, sun);
}

static bool game_camera(SG *g, float out[8]) {
    const SgrRec *rec = g->data.settingsFile.count ? &g->data.settingsFile.recs[0] : NULL;
    if (!rec) return false;
    for (u32 i = 0; i + 36 <= rec->size; i++) {
        i32 n;
        memcpy(&n, rec->body + i, 4);
        if (n != 32) continue;
        float f[8];
        memcpy(f, rec->body + i + 4, sizeof f);
        if (f[5] == -1.0f && f[0] < 0 && f[0] > -90 && f[3] > 0 && f[3] < 180 && f[4] > 0 && f[4] < 100) {
            memcpy(out, f, sizeof f);
            return true;
        }
    }
    return false;
}

static void pose_from(ViewPose *p, const float f[8]) {
    p->pitch = f[0];
    p->yaw = f[1];
    p->fov = f[3];
    p->distance = f[4];
    p->lift = -f[5];
}

static void pose_default(SG *g, View *v) {
    ViewPose *p = &v->base;
    memset(p, 0, sizeof *p);
    float f[8] = {kDefaultPitch, 0, 0, kDefaultFov, kDefaultDistance, -1, 0, 0};
    game_camera(g, f);
    pose_from(p, f);
    p->follow = true;
    if (v->look.hasCamera && v->look.cameraKind == 0) pose_from(p, v->look.camera);
    else if (v->look.hasCamera && v->look.cameraKind == 1) p->fov = v->look.camera[0], p->distance = v->look.camera[1];
}

void view_free(View *v) {
    if (!v) return;
    maplook_free(&v->look);
    free(v->chunks);
    mesh_free(&v->alpha);
    free(v->slopeImages);
    free(v->chipAttr);
    free(v->slopeDir);
    free(v->slopeIdx);
    img_free(v->blob);
    for (int i = 0; i < 3; i++) model_free(v->sky[i]);
    free(v);
}

void view_unload(SG *g) {
    View *v = g->r3.view;
    if (!v) return;
    maplook_free(&v->look);
    free(v->chunks);
    mesh_free(&v->alpha);
    free(v->slopeImages);
    free(v->chipAttr);
    free(v->slopeDir);
    free(v->slopeIdx);
    r3_shadow_free(&g->r3);
    Image *blob = v->blob;
    Model *sky[3] = {v->sky[0], v->sky[1], v->sky[2]};
    memset(v, 0, sizeof *v);
    v->blob = blob;
    memcpy(v->sky, sky, sizeof sky);
}

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

void view_load(SG *g) {
    double t0 = now_ms();
    Field *f = &g->field;
    view_unload(g);
    View *v = sg_view(g);
    Sgr *file = NULL;
    const SgrRec *rec = game_record(&g->data, SGR_MAP, 0, f->map.id, &file);
    if (!rec || !maplook_parse(&v->look, rec)) sg_log(g, "map look: unread fields");
    Map *m = &f->map;
    int maxIndex = 0;
    for (int i = 0; i < m->chipCount; i++)
        if (m->chips[i].index > maxIndex) maxIndex = m->chips[i].index;
    free(f->chipImages);
    f->chipImageCount = maxIndex + 2;
    f->chipImages = sg_alloc(sizeof(Image *) * (size_t)f->chipImageCount);
    v->chipAttr = sg_alloc((size_t)f->chipImageCount + (size_t)v->look.slopeCount + 1);
    v->slopeImages = sg_alloc(sizeof(Image *) * (size_t)(v->look.slopeCount + 1));
    for (int i = 0; i < v->look.slopeCount; i++) {
        v->slopeImages[i] = sg_image(g, v->look.slopes[i].id);
        v->chipAttr[f->chipImageCount + i] = chip_attr(g, v->look.slopes[i].id);
    }
    size_t cells = (size_t)m->w * (size_t)m->h;
    v->slopeDir = sg_alloc(sizeof(i16) * cells);
    v->slopeIdx = sg_alloc(cells);
    for (size_t i = 0; i < cells; i++) v->slopeDir[i] = -1;
    for (int i = 0; i < m->stairCount; i++) {
        const MapStair *s = &m->stairs[i];
        if (s->x < 0 || s->z < 0 || s->x >= m->w || s->z >= m->h || s->kind < 0 || s->kind > 11) continue;
        int si = -1;
        for (int k = 0; k < v->look.slopeCount; k++)
            if (v->look.slopes[k].index == s->dir) si = k;
        if (si < 0) continue;
        v->slopeDir[(size_t)s->z * (size_t)m->w + (size_t)s->x] = (i16)s->kind;
        v->slopeIdx[(size_t)s->z * (size_t)m->w + (size_t)s->x] = (u8)si;
    }
    v->border = BORDER;
    v->chunkW = (m->w + 2 * BORDER + CHUNK - 1) / CHUNK;
    v->chunkH = (m->h + 2 * BORDER + CHUNK - 1) / CHUNK;
    v->chunkCount = v->chunkW * v->chunkH;
    v->chunks = sg_alloc(sizeof(Chunk) * (size_t)v->chunkCount);
    Build b = {g, f, v, &f->terrain, &v->alpha, {0}};
    light_setup(g, v, b.sun);
    mesh_free(&f->terrain);
    if (!v->blob) {
        size_t size = 0;
        u8 *data = pack_read(&g->data.pack, "samples/common/res/system/circle_shadow.png", &size);
        if (data) v->blob = img_decode(data, size);
        free(data);
    }
    f->objectModels = sg_realloc(f->objectModels, sizeof(Model *) * (size_t)(m->objectCount ? m->objectCount : 1));
    for (int i = 0; i < m->objectCount; i++) {
        const Res *r = res_find(&g->res, m->objects[i].id);
        f->objectModels[i] = r ? model_load(g, r->path) : NULL;
    }
    int *objChunk = sg_alloc(sizeof(int) * (size_t)(m->objectCount + 1));
    for (int i = 0; i < m->objectCount; i++) {
        int cx = (m->objects[i].x + BORDER) / CHUNK, cz = (m->objects[i].z + BORDER) / CHUNK;
        if (cx < 0) cx = 0;
        if (cz < 0) cz = 0;
        if (cx >= v->chunkW) cx = v->chunkW - 1;
        if (cz >= v->chunkH) cz = v->chunkH - 1;
        objChunk[i] = cz * v->chunkW + cx;
    }
    for (int cz = 0; cz < v->chunkH; cz++)
        for (int cx = 0; cx < v->chunkW; cx++) {
            int ci = cz * v->chunkW + cx;
            Chunk *c = &v->chunks[ci];
            c->min[0] = c->min[1] = c->min[2] = 1e30f;
            c->max[0] = c->max[1] = c->max[2] = -1e30f;
            int opStart = f->terrain.count, alStart = v->alpha.count;
            for (int z = cz * CHUNK - BORDER; z < (cz + 1) * CHUNK - BORDER && z < m->h + BORDER; z++)
                for (int x = cx * CHUNK - BORDER; x < (cx + 1) * CHUNK - BORDER && x < m->w + BORDER; x++) build_cell(&b, x, z);
            for (int i = 0; i < m->objectCount; i++) {
                if (objChunk[i] != ci) continue;
                Model *mdl = f->objectModels[i];
                if (mdl && mdl->rig && mdl->rig->clipCount) model_pose(mdl, model_rest_clip(mdl), 0);
                const MapObject *o = &m->objects[i];
                add_model(&b, mdl, (float)o->x + 0.5f, (float)map_height(m, &v->look, o->x, o->z), (float)o->z + 0.5f, o->dir);
            }
            c->first = opStart;
            c->count = f->terrain.count - opStart;
            c->afirst = alStart;
            c->acount = v->alpha.count - alStart;
            chunk_add(v, ci, &f->terrain, &v->alpha, opStart, alStart);
        }
    free(objChunk);
    float mn[3] = {(float)-BORDER, -2.0f, (float)-BORDER}, mx[3] = {(float)(m->w + BORDER), 1.0f, (float)(m->h + BORDER)};
    for (int i = 0; i < v->chunkCount; i++)
        if (v->chunks[i].count) mx[1] = fmaxf(mx[1], v->chunks[i].max[1]), mn[1] = fminf(mn[1], v->chunks[i].min[1]);
    if (v->look.sun) {
        r3_shadow_begin(&g->r3, b.sun, mn, mx, kShadowTexels);
        for (int i = 0; i < f->terrain.count; i++)
            if (f->terrain.tris[i].flags & TF_CAST) r3_shadow_tri(&g->r3, &f->terrain.tris[i]);
        r3_shadow_seal(&g->r3);
    }
    if (v->look.sky == 0 && !v->sky[0]) {
        static const char *parts[3] = {"Sky noon - 01", "Cloud noon - 01", "Sun noon - 01"};
        for (int i = 0; i < 3; i++) {
            char *path = sg_fmt("samples/common/res/system/%s.fbx", parts[i]);
            v->sky[i] = model_load(g, path);
            free(path);
        }
    }
    pose_default(g, v);
    v->pose = v->base;
    v->ready = true;
    sg_log(g, "view: %d opaque, %d alpha triangles, %d chunks, sky %d, sun %d (%.2f %.2f), camera %d kind %d, %.1f ms", f->terrain.count, v->alpha.count,
           v->chunkCount, v->look.sky, v->look.sun, v->look.sunPitch, v->look.sunYaw, v->look.hasCamera, v->look.cameraKind, now_ms() - t0);
}

void view_follow(SG *g, float x, float y, float z, u32 ms) {
    View *v = sg_view(g);
    v->waterMs += ms;
    for (int i = 0; i < g->field.npcCount; i++) g->field.npcs[i].animMs += ms;
    if (v->moveTotal) {
        v->moveMs += ms;
        float t = v->moveMs >= v->moveTotal ? 1.0f : (float)v->moveMs / (float)v->moveTotal;
        float s = t * t * (3 - 2 * t);
        ViewPose *a = &v->from, *b = &v->to;
        v->pose.pitch = a->pitch + (b->pitch - a->pitch) * s;
        v->pose.yaw = a->yaw + (b->yaw - a->yaw) * s;
        v->pose.distance = a->distance + (b->distance - a->distance) * s;
        v->pose.fov = a->fov + (b->fov - a->fov) * s;
        v->pose.lift = a->lift + (b->lift - a->lift) * s;
        Vec3 at = a->follow ? (Vec3){x, y, z} : a->target, to = b->follow ? (Vec3){x, y, z} : b->target;
        v->pose.target.x = at.x + (to.x - at.x) * s;
        v->pose.target.y = at.y + (to.y - at.y) * s;
        v->pose.target.z = at.z + (to.z - at.z) * s;
        v->pose.follow = t >= 1.0f && b->follow;
        if (t >= 1.0f) v->moveTotal = 0;
    }
    if (v->pose.follow) v->pose.target = (Vec3){x, y, z};
}

void sg_camera_get(SG *g, ViewPose *out) { *out = sg_view(g)->pose; }

void sg_camera_to(SG *g, const ViewPose *pose, u32 ms) {
    View *v = sg_view(g);
    v->from = v->pose;
    v->to = *pose;
    if (ms == 0) {
        v->pose = *pose;
        v->moveTotal = 0;
        return;
    }
    v->moveMs = 0;
    v->moveTotal = ms;
}

void sg_camera_reset(SG *g, u32 ms) {
    View *v = sg_view(g);
    v->eye = false;
    sg_camera_to(g, &v->base, ms);
}

bool sg_camera_moving(SG *g) { return sg_view(g)->moveTotal != 0; }

void sg_camera_command(SG *g, const Cmd *c) {
    View *v = sg_view(g);
    ViewPose p = v->pose;
    i32 mode = cmd_int(c, 0);
    v->eye = mode == 2;
    u32 ms = 0;
    if (mode == 0 && c->argCount >= 6) {
        p.pitch = (float)cmd_int(c, 1) / 1000.0f;
        p.yaw = (float)cmd_int(c, 2) / 1000.0f;
        p.distance = (float)cmd_int(c, 3) / 1000.0f;
        p.fov = (float)cmd_int(c, 4) / 1000.0f;
        p.follow = true;
        ms = (u32)cmd_int(c, 5);
    } else if (mode == 3 && c->argCount >= 9) {
        p.target = (Vec3){(float)cmd_int(c, 1) / 1000.0f, (float)cmd_int(c, 2) / 1000.0f, (float)cmd_int(c, 3) / 1000.0f};
        p.pitch = (float)cmd_int(c, 4) / 1000.0f;
        p.yaw = (float)cmd_int(c, 5) / 1000.0f;
        p.distance = (float)cmd_int(c, 6) / 1000.0f;
        p.fov = (float)cmd_int(c, 7) / 1000.0f;
        p.follow = false;
        ms = (u32)cmd_int(c, 8);
    } else if (mode == 2 && c->argCount >= 4) {
        static const float yaws[4] = {0.0f, 90.0f, 180.0f, 270.0f};
        static const int dx[4] = {0, 1, 0, -1}, dz[4] = {-1, 0, 1, 0};
        const Actor *pl = &g->field.player;
        int d = pl->dir & 3;
        p.target = (Vec3){pl->x + (float)dx[d], pl->y + (float)cmd_int(c, 2) / 1000.0f, pl->z + (float)dz[d]};
        p.pitch = 0.0f;
        p.yaw = yaws[d];
        p.distance = 1.0f / kDistScale;
        p.fov = (float)cmd_int(c, 1) / 500.0f / kFovScale;
        p.lift = 0.0f;
        p.follow = false;
        ms = (u32)cmd_int(c, 3);
    } else {
        sg_log(g, "camera mode %d not handled", mode);
        return;
    }
    sg_camera_to(g, &p, ms);
}

static void pose_to_cam(const ViewPose *p, Cam *c, int w, int h) {
    c->target = p->target;
    c->target.y += p->lift;
    c->pitch = -p->pitch;
    c->yaw = -p->yaw;
    c->distance = p->distance * kDistScale;
    c->fov = p->fov * kFovScale;
    cam_update(c, w, h);
}

static int sprite_row(int dir) {
    switch (dir) {
    case 0: return 0;
    case 1: return 3;
    case 3: return 2;
    default: return 1;
    }
}

static float ground_at(SG *g, float x, float z) {
    const Map *m = &g->field.map;
    return (float)map_height(m, &g->r3.view->look, (int)floorf(x), (int)floorf(z));
}

static float actor_lift(SG *g, const Actor *a) {
    const Map *m = &g->field.map;
    View *v = g->r3.view;
    int x = (int)floorf(a->x), z = (int)floorf(a->z);
    if (!v || !v->slopeDir || x < 0 || z < 0 || x >= m->w || z >= m->h) return 0;
    int sdir = v->slopeDir[(size_t)z * (size_t)m->w + (size_t)x];
    if (sdir < 0 || (int)lroundf(a->y) != m->height[(size_t)z * (size_t)m->w + (size_t)x]) return 0;
    const u8 *c = kCorners[sdir];
    float fx = a->x - (float)x, fz = a->z - (float)z;
    return c[0] * (1 - fx) * (1 - fz) + c[1] * fx * (1 - fz) + c[2] * fx * fz + c[3] * (1 - fx) * fz;
}

static void draw_blob(SG *g, const Actor *a, float size) {
    View *v = g->r3.view;
    if (!v->blob) return;
    float y = a->y + actor_lift(g, a) + 0.02f, r = size * 0.5f;
    Tri t1 = {.tex = v->blob, .mode = TRI_MODULATE, .flags = TF_NOZWRITE};
    Vert p[4] = {{a->x - r, y, a->z - r, 0, 0, 1}, {a->x - r, y, a->z + r, 0, 1, 1}, {a->x + r, y, a->z + r, 1, 1, 1}, {a->x + r, y, a->z - r, 1, 0, 1}};
    t1.v[0] = p[0], t1.v[1] = p[1], t1.v[2] = p[2];
    Tri t2 = t1;
    t2.v[0] = p[0], t2.v[1] = p[2], t2.v[2] = p[3];
    r3_tri(&g->r3, &g->field.cam, &t1);
    r3_tri(&g->r3, &g->field.cam, &t2);
}

static const float kFacing[4] = {3.14159265f, 1.5707963f, 0.0f, -1.5707963f};

static Tri *posed_actor(SG *g, Actor *a, float ang, int *count) {
    Model *mdl = a->model;
    Rig *rig = mdl->rig;
    if (rig && rig->animated) {
        const char *motion = rig->motion[0] ? rig->motion : a->moving ? "walk" : "wait";
        if (!model_has_motion(mdl, motion)) motion = rig->motion[0] ? rig->motion : model_rest_clip(mdl);
        model_pose(mdl, motion, rig->motion[0] || a->moving || !strcmp(motion, "wait") ? (float)a->animMs / 1000.0f : 0);
    }
    const Mesh *m = model_mesh(mdl);
    const float *nrm = model_normals(mdl);
    float cs = cosf(ang), sn = sinf(ang);
    const float *sun = g->r3.x->sunDir;
    Tri *out = sg_alloc(sizeof(Tri) * (size_t)(m->count + 1));
    for (int i = 0; i < m->count; i++) {
        Tri t = m->tris[i];
        for (int k = 0; k < 3; k++) {
            float x = t.v[k].x, z = t.v[k].z;
            t.v[k].x = a->x + x * cs + z * sn;
            t.v[k].z = a->z - x * sn + z * cs;
            t.v[k].y += a->y;
            if (nrm) {
                const float *n = nrm + i * 9 + k * 3;
                float nx = n[0] * cs + n[2] * sn, nz = -n[0] * sn + n[2] * cs;
                float d = -(nx * sun[0] + n[1] * sun[1] + nz * sun[2]);
                t.v[k].shade = d > 0 ? d : 0;
            }
        }
        if (!t.mode && t.modulate) t.mode = TRI_MODULATE;
        out[i] = t;
    }
    *count = m->count;
    return out;
}

static void draw_model(SG *g, Actor *a, float ang, bool xray) {
    int n = 0;
    Tri *tris = posed_actor(g, a, ang, &n);
    if (xray)
        for (int i = 0; i < n; i++)
            if (tris[i].mode == TRI_OPAQUE) r3_tri_ex(&g->r3, &g->field.cam, &tris[i], TRI_OPAQUE, TF_XRAY | TF_CULL);
    for (int i = 0; i < n; i++) {
        Tri *t = &tris[i];
        r3_tri_ex(&g->r3, &g->field.cam, t, t->mode, t->mode == TRI_OPAQUE ? (u8)(t->flags | TF_LIT | TF_SHADOW) : t->flags);
    }
    free(tris);
}

static void cast_model(SG *g, Actor *a, float ang) {
    int n = 0;
    float y = a->y;
    a->y += actor_lift(g, a);
    Tri *tris = posed_actor(g, a, ang, &n);
    a->y = y;
    for (int i = 0; i < n; i++)
        if (tris[i].mode == TRI_OPAQUE && !(tris[i].flags & TF_NOZWRITE)) r3_shadow_tri(&g->r3, &tris[i]);
    free(tris);
}

int sg_sprite_rows(const Res *res, const Image *im) {
    int frames = res->frames > 0 ? res->frames : 1;
    return res->divisions == 1 && im->h / 4 >= im->w / frames ? 4 : 1;
}

static void draw_sprite(SG *g, const Actor *a, bool xray) {
    Field *f = &g->field;
    const Image *im = a->sheet;
    if (!im || !a->graphic) return;
    int frames = a->graphic->frames > 0 ? a->graphic->frames : 1;
    int rows = sg_sprite_rows(a->graphic, im);
    int cw = im->w / frames, ch = im->h / rows;
    int frame = 0;
    if (a->graphic->frameMs > 0) frame = (int)(a->animMs / (u32)a->graphic->frameMs) % frames;
    if (frames == 3 && rows == 4) {
        static const int cycle[4] = {1, 0, 1, 2};
        u32 step = (u32)(a->graphic->frameMs > 0 ? a->graphic->frameMs : 250);
        frame = a->moving || a->graphic->flagA ? cycle[(a->animMs / step) % 4] : 1;
    }
    int row = rows == 4 ? sprite_row(a->dir) : 0;
    float w = (float)cw / TILE, h = (float)ch / TILE;
    float p = f->cam.pitch * 3.14159265f / 180.0f;
    float tall = h / cosf(p);
    float yaw = f->cam.yaw * 3.14159265f / 180.0f;
    float rx = cosf(yaw) * w / 2, rz = sinf(yaw) * w / 2;
    float ax = a->x - sinf(yaw) * kSpriteForward, az = a->z + cosf(yaw) * kSpriteForward;
    float u0 = (float)(frame * cw) / (float)im->w, u1 = (float)((frame + 1) * cw) / (float)im->w;
    float v0 = (float)(row * ch) / (float)im->h, v1 = (float)((row + 1) * ch) / (float)im->h;
    float inset = 0.5f / (float)im->w;
    Vert va = {ax - rx, a->y + tall, az - rz, u0 + inset, v0, 1}, vb = {ax + rx, a->y + tall, az + rz, u1 - inset, v0, 1};
    Vert vc = {ax + rx, a->y, az + rz, u1 - inset, v1, 1}, vd = {ax - rx, a->y, az - rz, u0 + inset, v1, 1};
    Tri t1 = {.v = {va, vb, vc}, .tex = im, .alphaTest = true}, t2 = {.v = {va, vc, vd}, .tex = im, .alphaTest = true};
    if (xray) {
        r3_tri_ex(&g->r3, &f->cam, &t1, TRI_OPAQUE, TF_XRAY);
        r3_tri_ex(&g->r3, &f->cam, &t2, TRI_OPAQUE, TF_XRAY);
        r3_flush(&g->r3);
    }
    r3_tri(&g->r3, &f->cam, &t1);
    r3_tri(&g->r3, &f->cam, &t2);
}

static void draw_actor(SG *g, Actor *a) {
    if (!a->visible) return;
    bool xray = a == &g->field.player;
    float y = a->y;
    a->y += actor_lift(g, a);
    if (a->model) draw_model(g, a, kFacing[a->dir & 3], xray);
    else draw_sprite(g, a, xray);
    a->y = y;
}

static void draw_sky(SG *g, View *v) {
    Cam *c = &g->field.cam;
    for (int k = 0; k < 3; k++) {
        Model *mdl = v->sky[k];
        if (!mdl) continue;
        for (int i = 0; i < mdl->mesh.count; i++) {
            Tri t = mdl->mesh.tris[i];
            for (int j = 0; j < 3; j++) {
                t.v[j].x += c->m[12];
                t.v[j].y += c->m[13] - 4.0f;
                t.v[j].z += c->m[14];
            }
            u8 mode = t.mode == TRI_ADD ? TRI_ADD : k == 1 ? TRI_BLEND : TRI_OPAQUE;
            t.alphaTest = false;
            r3_tri_ex(&g->r3, c, &t, mode, TF_NOZTEST | TF_NOZWRITE);
        }
    }
}

typedef struct ChunkOrder {
    int index;
    float d;
} ChunkOrder;

static int order_cmp(const void *a, const void *b) {
    float da = ((const ChunkOrder *)a)->d, db = ((const ChunkOrder *)b)->d;
    return da < db ? -1 : da > db;
}

void field_draw(SG *g) {
    double t0 = now_ms();
    Field *f = &g->field;
    r3_begin(&g->r3, g->fb, 0xff000000u);
    View *v = g->r3.view;
    if (!f->loaded || !v || !v->ready) return;
    pose_to_cam(&v->pose, &f->cam, g->w, g->h);
    r3_set_anim(&g->r3, (float)((v->waterMs / 125) % 8) / 8.0f);
    for (int k = 0; k < 3; k++) g->r3.tint[k] = 256;
    if (v->look.sky == 0) draw_sky(g, v);
    if (g->r3.x->shadow.base) {
        r3_shadow_restore(&g->r3);
        for (int i = 0; i < f->npcCount; i++)
            if (f->npcs[i].visible && f->npcs[i].model) cast_model(g, &f->npcs[i], kFacing[f->npcs[i].dir & 3]);
        if (f->player.visible && f->player.model && !v->eye) cast_model(g, &f->player, kFacing[f->player.dir & 3]);
    }
    ChunkOrder *order = sg_alloc(sizeof(ChunkOrder) * (size_t)(v->chunkCount + 1));
    int n = 0;
    for (int i = 0; i < v->chunkCount; i++) {
        Chunk *c = &v->chunks[i];
        if (!c->count && !c->acount) continue;
        if (!r3_box_visible(&f->cam, g->w, g->h, c->min, c->max)) continue;
        float cx = (c->min[0] + c->max[0]) * 0.5f - f->cam.m[12], cz = (c->min[2] + c->max[2]) * 0.5f - f->cam.m[14];
        order[n++] = (ChunkOrder){i, cx * cx + cz * cz};
    }
    qsort(order, (size_t)n, sizeof *order, order_cmp);
    int tris = 0;
    for (int k = 0; k < n; k++) {
        Chunk *c = &v->chunks[order[k].index];
        for (int i = c->first; i < c->first + c->count; i++) r3_tri(&g->r3, &f->cam, &f->terrain.tris[i]);
        tris += c->count;
    }
    for (int i = 0; i < f->npcCount; i++)
        if (f->npcs[i].visible && f->npcs[i].model) draw_actor(g, &f->npcs[i]);
    if (f->player.visible && f->player.model && !v->eye) draw_actor(g, &f->player);
    for (int k = n - 1; k >= 0; k--) {
        Chunk *c = &v->chunks[order[k].index];
        for (int i = c->afirst; i < c->afirst + c->acount; i++) r3_tri(&g->r3, &f->cam, &v->alpha.tris[i]);
        tris += c->acount;
    }
    if (!g->r3.x->shadow.base) {
        for (int i = 0; i < f->npcCount; i++)
            if (f->npcs[i].visible && f->npcs[i].model) draw_blob(g, &f->npcs[i], 0.9f);
        if (f->player.visible && f->player.model && !v->eye) draw_blob(g, &f->player, 0.9f);
    }
    for (int i = 0; i < f->npcCount; i++)
        if (f->npcs[i].visible && !f->npcs[i].model) draw_actor(g, &f->npcs[i]);
    if (f->player.visible && !f->player.model && !v->eye) draw_actor(g, &f->player);
    free(order);
    v->stats.submitMs = now_ms() - t0;
    r3_flush(&g->r3);
    v->stats.drawMs = now_ms() - t0;
    v->stats.tris = tris;
    v->stats.chunksDrawn = n;
    v->stats.chunksTotal = v->chunkCount;
}

void sg_actor_motion(SG *g, Actor *a, const char *motion) {
    (void)g;
    if (!a->model || !a->model->rig) return;
    snprintf(a->model->rig->motion, sizeof a->model->rig->motion, "%s", motion ? motion : "");
    a->animMs = 0;
}

bool sg_debug_warp(VNSmileGame *vg, const char *mapName, int x, int z, int dir) {
    SG *g = (SG *)vg;
    Game *d = &g->data;
    int mi = 0;
    for (int i = 0; i < d->pack.count; i++) {
        const char *key = d->pack.entries[i].key;
        size_t n = strlen(key);
        if (strncmp(key, "map/", 4) != 0 || n <= 4 || strcmp(key + n - 4, ".sgr") != 0) continue;
        if (mi >= d->mapCount) break;
        Sgr *s = &d->maps[mi++];
        for (int k = 0; k < s->count; k++) {
            if (s->recs[k].type != SGR_MAP) continue;
            Rd r = rd_make(s->recs[k].body, s->recs[k].size);
            Guid id = rd_guid(&r);
            char *name = rd_str(&r);
            bool hit = (name && strcmp(name, mapName) == 0) || strncasecmp(key + 4, mapName, strlen(mapName)) == 0;
            free(name);
            if (!hit) continue;
            g->scene = SCENE_FIELD;
            bool ok = field_load(g, id, x, z, dir);
            interp_free(&g->interp);
            memset(&g->interp, 0, sizeof g->interp);
            return ok;
        }
    }
    return false;
}

void sg_debug_pose(VNSmileGame *vg, float pitch, float yaw, float distance, float fov) {
    SG *g = (SG *)vg;
    View *v = sg_view(g);
    v->pose.pitch = pitch;
    v->pose.yaw = yaw;
    v->pose.distance = distance;
    v->pose.fov = fov;
}

bool sg_debug_player(VNSmileGame *vg, const char *resName) {
    SG *g = (SG *)vg;
    Actor *a = &g->field.player;
    for (int i = 0; i < g->res.count; i++) {
        const Res *r = &g->res.items[i];
        if (r->kind != RES_CHARACTER || strcmp(r->name, resName) != 0) continue;
        model_free(a->model);
        a->model = NULL;
        a->sheet = NULL;
        a->graphic = r;
        size_t n = strlen(r->path);
        if (n > 4 && strcasecmp(r->path + n - 4, ".fbx") == 0) a->model = model_load(g, r->path);
        else a->sheet = sg_image(g, r->id);
        a->visible = true;
        return true;
    }
    return false;
}

const FrameStats *sg_debug_stats(VNSmileGame *vg) { return &sg_view((SG *)vg)->stats; }

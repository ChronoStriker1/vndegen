#include "sg.h"

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

#define NEAR_Z 0.05f
#define SHADOW_BIAS 0.08f

static const u32 kXrayAlpha = 102;
static const int kBandsPerThread = 4;

static void pool_free(R3 *r);

void r3_init(R3 *r, int w, int h) {
    memset(r, 0, sizeof *r);
    r->w = w;
    r->h = h;
    r->zbuf = sg_alloc(sizeof(float) * (size_t)w * (size_t)h);
    r->x = sg_alloc(sizeof *r->x);
    r->x->mark = sg_alloc((size_t)w * (size_t)h);
    for (int i = 0; i < 3; i++) r->x->amb[i] = 1.0f;
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    r3_threads(r, cores >= 6 ? 6 : (int)cores);
}

void r3_free(R3 *r) {
    view_free(r->view);
    if (r->x) {
        pool_free(r);
        r3_shadow_free(r);
        free(r->x->mark);
        free(r->x);
    }
    free(r->zbuf);
    memset(r, 0, sizeof *r);
}

void r3_begin(R3 *r, u32 *fb, u32 clear) {
    r3_flush(r);
    if (++r->x->markSerial == 0) {
        memset(r->x->mark, 0, (size_t)r->w * (size_t)r->h);
        r->x->markSerial = 1;
    }
    r->fb = fb;
    r->tint[0] = r->tint[1] = r->tint[2] = 256;
    size_t n = (size_t)r->w * (size_t)r->h;
    for (size_t i = 0; i < n; i++) fb[i] = clear;
    memset(r->zbuf, 0, sizeof(float) * n);
}

void r3_light(R3 *r, const float amb[3], const float sun[3], const float dir[3]) {
    for (int i = 0; i < 3; i++) {
        r->x->amb[i] = amb[i];
        r->x->sun[i] = sun[i];
        r->x->sunDir[i] = dir[i];
    }
}

void r3_set_anim(R3 *r, float u) { r->x->animU = u; }

void cam_update(Cam *c, int w, int h) {
    float p = c->pitch * 3.14159265f / 180.0f, y = c->yaw * 3.14159265f / 180.0f;
    float fx = sinf(y) * cosf(p), fy = -sinf(p), fz = -cosf(y) * cosf(p);
    float ex = c->target.x - fx * c->distance, ey = c->target.y - fy * c->distance, ez = c->target.z - fz * c->distance;
    float rx = cosf(y), ry = 0, rz = sinf(y);
    float ux = ry * fz - rz * fy, uy = rz * fx - rx * fz, uz = rx * fy - ry * fx;
    float *m = c->m;
    m[0] = rx, m[1] = ry, m[2] = rz, m[3] = -(rx * ex + ry * ey + rz * ez);
    m[4] = -ux, m[5] = -uy, m[6] = -uz, m[7] = (ux * ex + uy * ey + uz * ez);
    m[8] = fx, m[9] = fy, m[10] = fz, m[11] = -(fx * ex + fy * ey + fz * ez);
    m[12] = ex, m[13] = ey, m[14] = ez;
    c->focal = (float)h * 0.5f / tanf(c->fov * 0.5f * 3.14159265f / 180.0f);
    c->cx = (float)w * 0.5f;
    c->cy = (float)h * 0.5f;
}

typedef struct PV {
    float x, y, z, u, v, s, lx, ly, lz;
} PV;

typedef struct Scr {
    float x, y, a[8];
} Scr;

enum { A_IZ, A_UZ, A_VZ, A_S, A_LX, A_LY, A_LZ };

static inline u32 tex_fetch(const Image *tex, float u, float v) {
    float fu = u * (float)tex->w, fv = v * (float)tex->h;
    int tx = (int)fu, ty = (int)fv;
    if (fu < 0) tx--;
    if (fv < 0) ty--;
    if ((unsigned)tx >= (unsigned)tex->w) {
        tx %= tex->w;
        if (tx < 0) tx += tex->w;
    }
    if ((unsigned)ty >= (unsigned)tex->h) {
        ty %= tex->h;
        if (ty < 0) ty += tex->h;
    }
    return tex->px[(size_t)ty * (size_t)tex->w + (size_t)tx];
}

static inline float shadow_sample(const ShadowMap *sm, float lx, float ly, float lz, float bias) {
    float fx = lx - 0.5f, fy = ly - 0.5f;
    int ix = (int)fx, iy = (int)fy;
    if (fx < (float)ix) ix--;
    if (fy < (float)iy) iy--;
    if (ix < 0 || iy < 0 || ix + 1 >= sm->w || iy + 1 >= sm->h) return 1.0f;
    float wx = fx - (float)ix, wy = fy - (float)iy;
    const float *d = sm->depth + (size_t)iy * (size_t)sm->w + (size_t)ix;
    float z = lz - bias;
    float a = z <= d[0] ? 1.0f : 0.0f, b = z <= d[1] ? 1.0f : 0.0f;
    float c = z <= d[sm->w] ? 1.0f : 0.0f, e = z <= d[sm->w + 1] ? 1.0f : 0.0f;
    float top = a + (b - a) * wx, bot = c + (e - c) * wx;
    return top + (bot - top) * wy;
}

static inline float soft_shadow(const ShadowMap *sm, float lx, float ly, float lz, float bias) {
    return 0.5f * (shadow_sample(sm, lx - 0.8f, ly - 0.8f, lz, bias) + shadow_sample(sm, lx + 0.8f, ly + 0.8f, lz, bias));
}

static inline u32 clamp255(float v) { return v >= 255.0f ? 255u : (u32)v; }

typedef struct RCmd {
    Scr s[3];
    const Image *tex;
    float bias;
    int y0, y1;
    u8 alphaTest, mode, flags, na;
} RCmd;

struct Pool {
    pthread_t thr[8];
    int count;
    pthread_mutex_t mu;
    pthread_cond_t go, done;
    unsigned gen;
    int pending;
    bool quit;
    R3 *r;
    RCmd *q;
    int qn, qcap;
    atomic_int next;
    int bands;
};

static void raster(R3 *r, const RCmd *cmd, int band0, int band1);

static void run_band(R3 *r, int band, int bands) {
    Pool *pl = r->x->pool;
    int y0 = r->h * band / bands, y1 = r->h * (band + 1) / bands;
    for (int i = 0; i < pl->qn; i++) {
        const RCmd *c = &pl->q[i];
        if (c->y1 < y0 || c->y0 >= y1) continue;
        raster(r, c, y0, y1);
    }
}

static inline float mn3(float a, float b, float c) {
    float m = a < b ? a : b;
    return m < c ? m : c;
}

static inline float mx3(float a, float b, float c) {
    float m = a > b ? a : b;
    return m > c ? m : c;
}

typedef struct WorkerArg {
    Pool *pl;
    int index;
} WorkerArg;

static void run_bands(Pool *pl) {
    for (int b; (b = atomic_fetch_add(&pl->next, 1)) < pl->bands;) run_band(pl->r, b, pl->bands);
}

static void *worker(void *arg) {
    WorkerArg wa = *(WorkerArg *)arg;
    free(arg);
    Pool *pl = wa.pl;
    unsigned seen = 0;
    pthread_mutex_lock(&pl->mu);
    for (;;) {
        while (pl->gen == seen && !pl->quit) pthread_cond_wait(&pl->go, &pl->mu);
        if (pl->quit) break;
        seen = pl->gen;
        pthread_mutex_unlock(&pl->mu);
        run_bands(pl);
        pthread_mutex_lock(&pl->mu);
        if (--pl->pending == 0) pthread_cond_signal(&pl->done);
    }
    pthread_mutex_unlock(&pl->mu);
    return NULL;
}

void r3_threads(R3 *r, int n) {
    pool_free(r);
    if (n <= 1) return;
    if (n > 9) n = 9;
    Pool *pl = sg_alloc(sizeof *pl);
    pl->r = r;
    pthread_mutex_init(&pl->mu, NULL);
    pthread_cond_init(&pl->go, NULL);
    pthread_cond_init(&pl->done, NULL);
    r->x->pool = pl;
    pthread_mutex_lock(&pl->mu);
    for (int i = 0; i < n - 1; i++) {
        WorkerArg *wa = sg_alloc(sizeof *wa);
        wa->pl = pl;
        wa->index = pl->count + 1;
        if (pthread_create(&pl->thr[pl->count], NULL, worker, wa) == 0) pl->count++;
        else free(wa);
    }
    pthread_mutex_unlock(&pl->mu);
}

static void pool_free(R3 *r) {
    Pool *pl = r->x->pool;
    if (!pl) return;
    pthread_mutex_lock(&pl->mu);
    pl->quit = true;
    pthread_cond_broadcast(&pl->go);
    pthread_mutex_unlock(&pl->mu);
    for (int i = 0; i < pl->count; i++) pthread_join(pl->thr[i], NULL);
    pthread_mutex_destroy(&pl->mu);
    pthread_cond_destroy(&pl->go);
    pthread_cond_destroy(&pl->done);
    free(pl->q);
    free(pl);
    r->x->pool = NULL;
}

void r3_flush(R3 *r) {
    Pool *pl = r->x ? r->x->pool : NULL;
    if (!pl || !pl->qn) return;
    pthread_mutex_lock(&pl->mu);
    pl->pending = pl->count;
    pl->bands = (pl->count + 1) * kBandsPerThread;
    atomic_store(&pl->next, 0);
    pl->gen++;
    pthread_cond_broadcast(&pl->go);
    pthread_mutex_unlock(&pl->mu);
    run_bands(pl);
    pthread_mutex_lock(&pl->mu);
    while (pl->pending > 0) pthread_cond_wait(&pl->done, &pl->mu);
    pthread_mutex_unlock(&pl->mu);
    pl->qn = 0;
}

static void submit(R3 *r, const Scr *a, const Scr *b, const Scr *c, const Tri *t, u8 mode, u8 flags, int na) {
    float sh = (t->v[0].shade + t->v[1].shade + t->v[2].shade) / 3.0f;
    float bias = SHADOW_BIAS;
    if (na > 4 && sh > 0.01f) {
        float tanA = sqrtf(fmaxf(0.0f, 1.0f - sh * sh)) / sh;
        bias += fminf(tanA, 8.0f) * 1.5f / r->x->shadow.scale;
    }
    float miny = mn3(a->y, b->y, c->y), maxy = mx3(a->y, b->y, c->y);
    float minx = mn3(a->x, b->x, c->x), maxx = mx3(a->x, b->x, c->x);
    if (maxy < 0 || miny > (float)r->h || maxx < 0 || minx > (float)r->w) return;
    RCmd cmd = {{*a, *b, *c}, t->tex, bias, (int)miny - 1, (int)maxy + 1, t->alphaTest, mode, flags, (u8)na};
    Pool *pl = r->x->pool;
    if (!pl) {
        raster(r, &cmd, 0, r->h);
        return;
    }
    if (pl->qn == pl->qcap) {
        pl->qcap = pl->qcap ? pl->qcap * 2 : 4096;
        pl->q = sg_realloc(pl->q, sizeof(RCmd) * (size_t)pl->qcap);
    }
    pl->q[pl->qn++] = cmd;
}

static void raster(R3 *r, const RCmd *cmd, int band0, int band1) {
    const Scr *p0 = &cmd->s[0], *p1 = &cmd->s[1], *p2 = &cmd->s[2];
    u8 mode = cmd->mode, flags = cmd->flags;
    int na = cmd->na;
    float x10 = p1->x - p0->x, y10 = p1->y - p0->y, x20 = p2->x - p0->x, y20 = p2->y - p0->y;
    float det = x10 * y20 - x20 * y10;
    if (fabsf(det) < 1e-6f) return;
    float miny = mn3(p0->y, p1->y, p2->y), maxy = mx3(p0->y, p1->y, p2->y);
    int y0 = (int)ceilf(miny - 0.5f), y1 = (int)floorf(maxy - 0.5f);
    if (y0 < band0) y0 = band0;
    if (y1 > band1 - 1) y1 = band1 - 1;
    if (y0 > y1) return;
    float inv = 1.0f / det;
    float dx[8] = {0}, dy[8] = {0}, base[8] = {0};
    for (int i = 0; i < na; i++) {
        float a10 = p1->a[i] - p0->a[i], a20 = p2->a[i] - p0->a[i];
        dx[i] = (a10 * y20 - a20 * y10) * inv;
        dy[i] = (a20 * x10 - a10 * x20) * inv;
        base[i] = p0->a[i] - dx[i] * p0->x - dy[i] * p0->y;
    }
    const Scr *v[3] = {p0, p1, p2};
    float sgn = det > 0 ? 1.0f : -1.0f;
    const Image *tex = cmd->tex;
    bool alphaTest = cmd->alphaTest;
    bool zwrite = !(flags & TF_NOZWRITE) && mode == TRI_OPAQUE;
    bool ztest = !(flags & TF_NOZTEST);
    bool xray = flags & TF_XRAY;
    u8 markSerial = r->x->markSerial;
    bool lit = flags & TF_LIT;
    const R3x *x3 = r->x;
    bool shadow = (flags & TF_SHADOW) && x3->shadowOn;
    float animU = (flags & TF_ANIM) ? x3->animU : 0;
    float ambR = x3->amb[0], ambG = x3->amb[1], ambB = x3->amb[2];
    float sunR = x3->sun[0], sunG = x3->sun[1], sunB = x3->sun[2];
    int tr = r->tint[0], tg = r->tint[1], tb = r->tint[2];
    for (int y = y0; y <= y1; y++) {
        float py = (float)y + 0.5f;
        float xl = -1e30f, xr = 1e30f;
        for (int e = 0; e < 3; e++) {
            const Scr *a = v[e], *b = v[(e + 1) % 3];
            float ex = b->x - a->x, ey = b->y - a->y;
            float c0 = (ex * (py - a->y) + ey * a->x) * sgn, cx = -ey * sgn;
            if (cx > 0) {
                float q = -c0 / cx;
                if (q > xl) xl = q;
            } else if (cx < 0) {
                float q = -c0 / cx;
                if (q < xr) xr = q;
            } else if (c0 < 0) {
                xr = -1e30f;
            }
        }
        if (xl < 0) xl = 0;
        if (xr > (float)r->w) xr = (float)r->w;
        int xa = (int)ceilf(xl - 0.5f), xb = (int)ceilf(xr - 0.5f) - 1;
        if (xa > xb) continue;
        float pxa = (float)xa + 0.5f;
        float at[8] = {0};
        for (int i = 0; i < na; i++) at[i] = base[i] + dx[i] * pxa + dy[i] * py;
        u32 *row = r->fb + (size_t)y * (size_t)r->w;
        float *zrow = r->zbuf + (size_t)y * (size_t)r->w;
        u8 *markRow = x3->mark + (size_t)y * (size_t)r->w;
        for (int x = xa; x <= xb; x++, at[0] += dx[0], at[1] += dx[1], at[2] += dx[2], at[3] += dx[3], at[4] += dx[4], at[5] += dx[5],
                 at[6] += dx[6]) {
            float iz = at[A_IZ];
            if (xray) {
                if (iz >= zrow[x] * 0.999f || markRow[x] == markSerial) continue;
            } else if (ztest && iz <= zrow[x]) {
                continue;
            }
            float z = 1.0f / iz;
            u32 c = 0xffffffffu;
            if (tex) c = tex_fetch(tex, at[A_UZ] * z + animU, at[A_VZ] * z);
            u32 a = c >> 24;
            u32 cr = c & 255, cg = c >> 8 & 255, cb = c >> 16 & 255;
            if (mode == TRI_OPAQUE && alphaTest && a < 128) continue;
            if (a == 0 && mode != TRI_MODULATE) continue;
            if (lit) {
                float s = at[A_S];
                if (s < 0) s = 0;
                if (shadow && s > 0) s *= soft_shadow(&x3->shadow, at[A_LX] * z, at[A_LY] * z, at[A_LZ] * z, cmd->bias);
                float lr = ambR + sunR * s, lg = ambG + sunG * s, lb = ambB + sunB * s;
                cr = clamp255((float)cr * lr);
                cg = clamp255((float)cg * lg);
                cb = clamp255((float)cb * lb);
            } else {
                cr = cr * (u32)tr >> 8;
                cg = cg * (u32)tg >> 8;
                cb = cb * (u32)tb >> 8;
                if (cr > 255) cr = 255;
                if (cg > 255) cg = 255;
                if (cb > 255) cb = 255;
            }
            u32 d = row[x];
            u32 dr = d & 255, dg = d >> 8 & 255, db = d >> 16 & 255;
            if (mode == TRI_MODULATE) {
                u32 ia = 255 - a;
                u32 mr = (cr * a + 255 * ia) / 255, mg = (cg * a + 255 * ia) / 255, mb = (cb * a + 255 * ia) / 255;
                row[x] = 0xff000000u | (db * mb / 255) << 16 | (dg * mg / 255) << 8 | (dr * mr / 255);
                continue;
            }
            if (mode == TRI_ADD) {
                dr += cr * a / 255;
                dg += cg * a / 255;
                db += cb * a / 255;
                row[x] = 0xff000000u | (db > 255 ? 255 : db) << 16 | (dg > 255 ? 255 : dg) << 8 | (dr > 255 ? 255 : dr);
                continue;
            }
            if (xray) {
                if (alphaTest && a < 128) continue;
                markRow[x] = markSerial;
                a = kXrayAlpha;
                cr = cg = cb = 255;
            }
            if ((mode == TRI_BLEND || !alphaTest || xray) && a < 255) {
                u32 ia = 255 - a;
                row[x] = 0xff000000u | ((cb * a + db * ia) / 255) << 16 | ((cg * a + dg * ia) / 255) << 8 | ((cr * a + dr * ia) / 255);
                continue;
            }
            row[x] = 0xff000000u | cb << 16 | cg << 8 | cr;
            if (zwrite) zrow[x] = iz;
        }
    }
}

static inline PV xform(const Cam *c, const Vert *v) {
    const float *m = c->m;
    PV o;
    o.x = m[0] * v->x + m[1] * v->y + m[2] * v->z + m[3];
    o.y = m[4] * v->x + m[5] * v->y + m[6] * v->z + m[7];
    o.z = m[8] * v->x + m[9] * v->y + m[10] * v->z + m[11];
    o.u = v->u;
    o.v = v->v;
    o.s = v->shade;
    o.lx = o.ly = o.lz = 0;
    return o;
}

static inline PV lerp(const PV *a, const PV *b, float t) {
    PV o;
    const float *pa = &a->x, *pb = &b->x;
    float *po = &o.x;
    for (int i = 0; i < 9; i++) po[i] = pa[i] + (pb[i] - pa[i]) * t;
    return o;
}

static inline Scr project(const Cam *c, const PV *v) {
    float iz = 1.0f / v->z;
    Scr s;
    s.x = c->cx + v->x * c->focal * iz;
    s.y = c->cy + v->y * c->focal * iz;
    s.a[A_IZ] = iz;
    s.a[A_UZ] = v->u * iz;
    s.a[A_VZ] = v->v * iz;
    s.a[A_S] = v->s;
    s.a[A_LX] = v->lx * iz;
    s.a[A_LY] = v->ly * iz;
    s.a[A_LZ] = v->lz * iz;
    s.a[7] = 0;
    return s;
}

static inline void light_coords(const ShadowMap *sm, const Vert *v, PV *o) {
    float p[3] = {v->x - sm->origin[0], v->y - sm->origin[1], v->z - sm->origin[2]};
    o->lx = (p[0] * sm->axis[0][0] + p[1] * sm->axis[0][1] + p[2] * sm->axis[0][2]) * sm->scale;
    o->ly = (p[0] * sm->axis[1][0] + p[1] * sm->axis[1][1] + p[2] * sm->axis[1][2]) * sm->scale;
    o->lz = p[0] * sm->axis[2][0] + p[1] * sm->axis[2][1] + p[2] * sm->axis[2][2];
}

void r3_tri_ex(R3 *r, const Cam *c, const Tri *t, u8 mode, u8 flags) {
    if (flags & TF_CULL) {
        float ax = t->v[1].x - t->v[0].x, ay = t->v[1].y - t->v[0].y, az = t->v[1].z - t->v[0].z;
        float bx = t->v[2].x - t->v[0].x, by = t->v[2].y - t->v[0].y, bz = t->v[2].z - t->v[0].z;
        float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
        float ex = c->m[12] - t->v[0].x, ey = c->m[13] - t->v[0].y, ez = c->m[14] - t->v[0].z;
        if (nx * ex + ny * ey + nz * ez <= 0) return;
    }
    PV in[3];
    bool shadow = (flags & TF_SHADOW) && r->x->shadowOn;
    for (int i = 0; i < 3; i++) {
        in[i] = xform(c, &t->v[i]);
        if (shadow) light_coords(&r->x->shadow, &t->v[i], &in[i]);
    }
    if (in[0].z < NEAR_Z && in[1].z < NEAR_Z && in[2].z < NEAR_Z) return;
    PV out[4];
    int n = 0;
    for (int i = 0; i < 3; i++) {
        const PV *a = &in[i], *b = &in[(i + 1) % 3];
        bool ai = a->z >= NEAR_Z, bi = b->z >= NEAR_Z;
        if (ai) out[n++] = *a;
        if (ai != bi) out[n++] = lerp(a, b, (NEAR_Z - a->z) / (b->z - a->z));
    }
    if (n < 3) return;
    Scr s[4];
    for (int i = 0; i < n; i++) s[i] = project(c, &out[i]);
    int na = shadow ? 7 : 4;
    submit(r, &s[0], &s[1], &s[2], t, mode, flags, na);
    if (n == 4) submit(r, &s[0], &s[2], &s[3], t, mode, flags, na);
}

void r3_tri(R3 *r, const Cam *c, const Tri *t) {
    if (t->mode || t->flags) r3_tri_ex(r, c, t, t->mode, t->flags);
    else r3_tri_ex(r, c, t, t->modulate ? TRI_MODULATE : TRI_OPAQUE, t->modulate ? TF_NOZWRITE : 0);
}

bool r3_box_visible(const Cam *c, int w, int h, const float mn[3], const float mx[3]) {
    int out[5] = {0};
    const float *m = c->m;
    for (int k = 0; k < 8; k++) {
        float px = (k & 1) ? mx[0] : mn[0], py = (k & 2) ? mx[1] : mn[1], pz = (k & 4) ? mx[2] : mn[2];
        float x = m[0] * px + m[1] * py + m[2] * pz + m[3];
        float y = m[4] * px + m[5] * py + m[6] * pz + m[7];
        float z = m[8] * px + m[9] * py + m[10] * pz + m[11];
        if (z < NEAR_Z) out[0]++;
        if (x * c->focal + c->cx * z < 0) out[1]++;
        if (c->cx * z - x * c->focal < 0) out[2]++;
        if (y * c->focal + c->cy * z < 0) out[3]++;
        if (c->cy * z - y * c->focal < 0) out[4]++;
    }
    (void)w;
    (void)h;
    for (int i = 0; i < 5; i++)
        if (out[i] == 8) return false;
    return true;
}

static void norm3(float v[3]) {
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 0) v[0] /= l, v[1] /= l, v[2] /= l;
}

void r3_shadow_free(R3 *r) {
    free(r->x->shadow.depth);
    free(r->x->shadow.base);
    memset(&r->x->shadow, 0, sizeof r->x->shadow);
    r->x->shadowOn = false;
}

void r3_shadow_begin(R3 *r, const float dir[3], const float mn[3], const float mx[3], int tpu) {
    r3_shadow_free(r);
    ShadowMap *sm = &r->x->shadow;
    float L[3] = {dir[0], dir[1], dir[2]};
    norm3(L);
    float up[3] = {0, 0, 1};
    if (fabsf(L[2]) > 0.9f) up[0] = 1, up[2] = 0;
    float U[3] = {L[1] * up[2] - L[2] * up[1], L[2] * up[0] - L[0] * up[2], L[0] * up[1] - L[1] * up[0]};
    norm3(U);
    float V[3] = {L[1] * U[2] - L[2] * U[1], L[2] * U[0] - L[0] * U[2], L[0] * U[1] - L[1] * U[0]};
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (int k = 0; k < 8; k++) {
        float p[3] = {(k & 1) ? mx[0] : mn[0], (k & 2) ? mx[1] : mn[1], (k & 4) ? mx[2] : mn[2]};
        float q[3] = {p[0] * U[0] + p[1] * U[1] + p[2] * U[2], p[0] * V[0] + p[1] * V[1] + p[2] * V[2], p[0] * L[0] + p[1] * L[1] + p[2] * L[2]};
        for (int i = 0; i < 3; i++) lo[i] = fminf(lo[i], q[i]), hi[i] = fmaxf(hi[i], q[i]);
    }
    float scale = (float)tpu;
    float span = fmaxf(hi[0] - lo[0], hi[1] - lo[1]);
    if (span * scale > 2048) scale = 2048 / span;
    sm->scale = scale;
    sm->w = (int)ceilf((hi[0] - lo[0]) * scale) + 2;
    sm->h = (int)ceilf((hi[1] - lo[1]) * scale) + 2;
    for (int i = 0; i < 3; i++) {
        sm->axis[0][i] = U[i];
        sm->axis[1][i] = V[i];
        sm->axis[2][i] = L[i];
        sm->origin[i] = U[i] * lo[0] + V[i] * lo[1] + L[i] * lo[2];
    }
    size_t n = (size_t)sm->w * (size_t)sm->h;
    sm->depth = sg_alloc(sizeof(float) * n);
    for (size_t i = 0; i < n; i++) sm->depth[i] = 1e30f;
    r->x->shadowOn = true;
}

void r3_shadow_seal(R3 *r) {
    ShadowMap *sm = &r->x->shadow;
    if (!sm->depth) return;
    size_t n = (size_t)sm->w * (size_t)sm->h;
    free(sm->base);
    sm->base = sg_alloc(sizeof(float) * n);
    memcpy(sm->base, sm->depth, sizeof(float) * n);
    sm->dirty[0] = sm->dirty[1] = 1 << 30;
    sm->dirty[2] = sm->dirty[3] = -1;
}

void r3_shadow_restore(R3 *r) {
    ShadowMap *sm = &r->x->shadow;
    if (!sm->base || sm->dirty[2] < sm->dirty[0]) return;
    for (int y = sm->dirty[1]; y <= sm->dirty[3]; y++) {
        size_t o = (size_t)y * (size_t)sm->w + (size_t)sm->dirty[0];
        memcpy(sm->depth + o, sm->base + o, sizeof(float) * (size_t)(sm->dirty[2] - sm->dirty[0] + 1));
    }
    sm->dirty[0] = sm->dirty[1] = 1 << 30;
    sm->dirty[2] = sm->dirty[3] = -1;
}

void r3_shadow_tri(R3 *r, const Tri *t) {
    ShadowMap *sm = &r->x->shadow;
    if (!sm->depth) return;
    PV p[3];
    for (int i = 0; i < 3; i++) light_coords(sm, &t->v[i], &p[i]);
    float x10 = p[1].lx - p[0].lx, y10 = p[1].ly - p[0].ly, x20 = p[2].lx - p[0].lx, y20 = p[2].ly - p[0].ly;
    float det = x10 * y20 - x20 * y10;
    if (fabsf(det) < 1e-9f) return;
    float inv = 1.0f / det;
    float a[3][3] = {{p[0].lz, p[1].lz, p[2].lz}, {t->v[0].u, t->v[1].u, t->v[2].u}, {t->v[0].v, t->v[1].v, t->v[2].v}};
    float gx[3], gy[3], g0[3];
    for (int i = 0; i < 3; i++) {
        float a10 = a[i][1] - a[i][0], a20 = a[i][2] - a[i][0];
        gx[i] = (a10 * y20 - a20 * y10) * inv;
        gy[i] = (a20 * x10 - a10 * x20) * inv;
        g0[i] = a[i][0] - gx[i] * p[0].lx - gy[i] * p[0].ly;
    }
    int x0 = (int)fmaxf(0, floorf(fminf(p[0].lx, fminf(p[1].lx, p[2].lx))));
    int x1 = (int)fminf((float)sm->w - 1, ceilf(fmaxf(p[0].lx, fmaxf(p[1].lx, p[2].lx))));
    int y0 = (int)fmaxf(0, floorf(fminf(p[0].ly, fminf(p[1].ly, p[2].ly))));
    int y1 = (int)fminf((float)sm->h - 1, ceilf(fmaxf(p[0].ly, fmaxf(p[1].ly, p[2].ly))));
    float sgn = det > 0 ? 1.0f : -1.0f;
    const Image *tex = t->alphaTest ? t->tex : NULL;
    if (sm->base && x0 <= x1 && y0 <= y1) {
        if (x0 < sm->dirty[0]) sm->dirty[0] = x0;
        if (y0 < sm->dirty[1]) sm->dirty[1] = y0;
        if (x1 > sm->dirty[2]) sm->dirty[2] = x1;
        if (y1 > sm->dirty[3]) sm->dirty[3] = y1;
    }
    for (int y = y0; y <= y1; y++) {
        float py = (float)y + 0.5f;
        for (int x = x0; x <= x1; x++) {
            float px = (float)x + 0.5f;
            bool in = true;
            for (int e = 0; e < 3 && in; e++) {
                const PV *A = &p[e], *B = &p[(e + 1) % 3];
                in = ((B->lx - A->lx) * (py - A->ly) - (B->ly - A->ly) * (px - A->lx)) * sgn >= 0;
            }
            if (!in) continue;
            if (tex) {
                u32 c = tex_fetch(tex, g0[1] + gx[1] * px + gy[1] * py, g0[2] + gx[2] * px + gy[2] * py);
                if ((c >> 24) < 128) continue;
            }
            float d = g0[0] + gx[0] * px + gy[0] * py;
            float *dst = &sm->depth[(size_t)y * (size_t)sm->w + (size_t)x];
            if (d < *dst) *dst = d;
        }
    }
}

void mesh_add(Mesh *m, const Tri *t) {
    if (m->count == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 256;
        m->tris = sg_realloc(m->tris, sizeof(Tri) * (size_t)m->cap);
    }
    m->tris[m->count++] = *t;
}

void mesh_quad(Mesh *m, const Image *tex, Vec3 a, Vec3 b, Vec3 c, Vec3 d, float u0, float v0, float u1, float v1, float shade,
               bool alphaTest) {
    Vert va = {a.x, a.y, a.z, u0, v0, shade}, vb = {b.x, b.y, b.z, u1, v0, shade};
    Vert vc = {c.x, c.y, c.z, u1, v1, shade}, vd = {d.x, d.y, d.z, u0, v1, shade};
    mesh_add(m, &(Tri){.v = {va, vb, vc}, .tex = tex, .alphaTest = alphaTest});
    mesh_add(m, &(Tri){.v = {va, vc, vd}, .tex = tex, .alphaTest = alphaTest});
}

void mesh_free(Mesh *m) {
    free(m->tris);
    memset(m, 0, sizeof *m);
}

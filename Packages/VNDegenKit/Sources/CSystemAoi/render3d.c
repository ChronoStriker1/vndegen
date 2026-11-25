#include "sa.h"
#include <math.h>

#define NEAR_Z 0.05f

void r3_init(R3 *r, int w, int h) {
    memset(r, 0, sizeof *r);
    r->w = w;
    r->h = h;
    r->zbuf = sa_alloc(sizeof(float) * (size_t)w * (size_t)h);
}

void r3_free(R3 *r) {
    free(r->zbuf);
    memset(r, 0, sizeof *r);
}

void r3_begin(R3 *r, u32 *fb, float fovDeg) {
    r->fb = fb;
    r->focal = (float)r->h * 0.5f / tanf(fovDeg * 0.5f * 3.14159265f / 180.0f);
    memset(r->zbuf, 0, sizeof(float) * (size_t)r->w * (size_t)r->h);
}

static R3Vert lerp(const R3Vert *a, const R3Vert *b, float t) {
    R3Vert o;
    o.x = a->x + (b->x - a->x) * t;
    o.y = a->y + (b->y - a->y) * t;
    o.z = a->z + (b->z - a->z) * t;
    o.u = a->u + (b->u - a->u) * t;
    o.v = a->v + (b->v - a->v) * t;
    o.shade = a->shade + (b->shade - a->shade) * t;
    return o;
}

typedef struct Scr {
    float x, y, iz, uz, vz, s;
} Scr;

static float edge(const Scr *a, const Scr *b, float x, float y) { return (b->x - a->x) * (y - a->y) - (b->y - a->y) * (x - a->x); }

static void raster(R3 *r, const Scr *p0, const Scr *p1, const Scr *p2, const Image *tex) {
    float area = edge(p0, p1, p2->x, p2->y);
    if (fabsf(area) < 1e-6f) return;
    if (r->cull && (r->cull > 0 ? area > 0 : area < 0)) return;
    float minx = fminf(p0->x, fminf(p1->x, p2->x)), maxx = fmaxf(p0->x, fmaxf(p1->x, p2->x));
    float miny = fminf(p0->y, fminf(p1->y, p2->y)), maxy = fmaxf(p0->y, fmaxf(p1->y, p2->y));
    int x0 = (int)fmaxf(0, floorf(minx)), x1 = (int)fminf((float)r->w - 1, ceilf(maxx));
    int y0 = (int)fmaxf(0, floorf(miny)), y1 = (int)fminf((float)r->h - 1, ceilf(maxy));
    if (x0 > x1 || y0 > y1) return;
    float inv = 1.0f / area;
    float a0 = -(p2->y - p1->y) * inv, b0 = (p2->x - p1->x) * inv;
    float a1 = -(p0->y - p2->y) * inv, b1 = (p0->x - p2->x) * inv;
    float a2 = -(p1->y - p0->y) * inv, b2 = (p1->x - p0->x) * inv;
    float sx = (float)x0 + 0.5f, sy = (float)y0 + 0.5f;
    float r0 = edge(p1, p2, sx, sy) * inv, r1 = edge(p2, p0, sx, sy) * inv, r2 = edge(p0, p1, sx, sy) * inv;
    float diz_dx = a0 * p0->iz + a1 * p1->iz + a2 * p2->iz, diz_dy = b0 * p0->iz + b1 * p1->iz + b2 * p2->iz;
    float duz_dx = a0 * p0->uz + a1 * p1->uz + a2 * p2->uz, duz_dy = b0 * p0->uz + b1 * p1->uz + b2 * p2->uz;
    float dvz_dx = a0 * p0->vz + a1 * p1->vz + a2 * p2->vz, dvz_dy = b0 * p0->vz + b1 * p1->vz + b2 * p2->vz;
    float ds_dx = a0 * p0->s + a1 * p1->s + a2 * p2->s, ds_dy = b0 * p0->s + b1 * p1->s + b2 * p2->s;
    float riz = r0 * p0->iz + r1 * p1->iz + r2 * p2->iz, ruz = r0 * p0->uz + r1 * p1->uz + r2 * p2->uz;
    float rvz = r0 * p0->vz + r1 * p1->vz + r2 * p2->vz, rs = r0 * p0->s + r1 * p1->s + r2 * p2->s;
    int tw = tex ? tex->w : 1, th = tex ? tex->h : 1;
    bool pow2 = tex && !(tw & (tw - 1)) && !(th & (th - 1));
    float fogSpan = r->fogFar - r->fogNear, fogInv = fogSpan > 0 ? 1.0f / fogSpan : 0;
    int fr = (int)(r->fogColor & 0xff), fg = (int)(r->fogColor >> 8 & 0xff), fbl = (int)(r->fogColor >> 16 & 0xff);
    const float eps = -1e-5f;
    for (int y = y0; y <= y1; y++) {
        float lo = 0, hi = (float)(x1 - x0);
        const float wv[3] = {r0, r1, r2}, av[3] = {a0, a1, a2};
        bool empty = false;
        for (int e = 0; e < 3 && !empty; e++) {
            if (av[e] > 1e-12f) lo = fmaxf(lo, (eps - wv[e]) / av[e]);
            else if (av[e] < -1e-12f) hi = fminf(hi, (eps - wv[e]) / av[e]);
            else if (wv[e] < eps) empty = true;
        }
        int xa = x0 + (int)ceilf(lo), xb = x0 + (int)floorf(hi);
        if (!empty && xa <= xb) {
            float t = (float)(xa - x0);
            float iz = riz + diz_dx * t, uz = ruz + duz_dx * t, vz = rvz + dvz_dx * t, sh = rs + ds_dx * t;
            u32 *row = r->fb + (size_t)y * r->w;
            float *zrow = r->zbuf + (size_t)y * r->w;
            for (int x = xa; x <= xb; x++, iz += diz_dx, uz += duz_dx, vz += dvz_dx, sh += ds_dx) {
                if (iz <= zrow[x]) continue;
                float z = 1.0f / iz;
            u32 c = 0xffffffffu;
            if (tex) {
                int tx = (int)(uz * z * (float)tw), ty = (int)(vz * z * (float)th);
                if (pow2) {
                    tx &= tw - 1;
                    ty &= th - 1;
                } else {
                    tx %= tw;
                    ty %= th;
                    if (tx < 0) tx += tw;
                    if (ty < 0) ty += th;
                }
                c = tex->px[(size_t)ty * tw + tx];
                if ((c >> 24) < 128) continue;
            }
            float f = fogSpan > 0 ? (r->fogFar - z) * fogInv : 1.0f;
            f = f < 0 ? 0 : f > 1 ? 1 : f;
            float cr = fminf((float)(c & 0xff) * sh, 255), cg = fminf((float)(c >> 8 & 0xff) * sh, 255), cb = fminf((float)(c >> 16 & 0xff) * sh, 255);
            float g1 = 1 - f;
            float or_ = cr * f + (float)fr * g1, og = cg * f + (float)fg * g1, ob = cb * f + (float)fbl * g1;
            if (r->blend > 0) {
                u32 dst = row[x];
                float a = r->blend, k = 1 - a;
                or_ = or_ * a + (float)(dst & 0xff) * k;
                og = og * a + (float)(dst >> 8 & 0xff) * k;
                ob = ob * a + (float)(dst >> 16 & 0xff) * k;
            } else {
                zrow[x] = iz;
            }
            row[x] = 0xff000000u | (u32)ob << 16 | (u32)og << 8 | (u32)or_;
            }
        }
        r0 += b0;
        r1 += b1;
        r2 += b2;
        riz += diz_dy;
        ruz += duz_dy;
        rvz += dvz_dy;
        rs += ds_dy;
    }
}

static Scr project(const R3 *r, const R3Vert *v) {
    Scr s;
    float iz = 1.0f / v->z;
    s.x = (float)r->w * 0.5f + v->x * iz * r->focal;
    s.y = (float)r->h * 0.5f - v->y * iz * r->focal;
    s.iz = iz;
    s.uz = v->u * iz;
    s.vz = v->v * iz;
    s.s = v->shade;
    return s;
}

void r3_tri(R3 *r, const R3Vert *v, const Image *tex) {
    R3Vert poly[4];
    int n = 0;
    for (int i = 0; i < 3; i++) {
        const R3Vert *a = &v[i], *b = &v[(i + 1) % 3];
        bool ain = a->z >= NEAR_Z, bin = b->z >= NEAR_Z;
        if (ain) poly[n++] = *a;
        if (ain != bin) poly[n++] = lerp(a, b, (NEAR_Z - a->z) / (b->z - a->z));
    }
    if (n < 3) return;
    Scr s[4];
    for (int i = 0; i < n; i++) s[i] = project(r, &poly[i]);
    for (int i = 1; i + 1 < n; i++) raster(r, &s[0], &s[i], &s[i + 1], tex);
}

Image *sa_bmp_decode(const u8 *d, size_t n) {
    if (n < 54 || d[0] != 'B' || d[1] != 'M') return NULL;
    u32 off = rd32(d + 10);
    int w = (int)rd32(d + 18), h = (int)rd32(d + 22), bpp = d[28] | d[29] << 8;
    bool flip = h > 0;
    if (h < 0) h = -h;
    if (w <= 0 || h <= 0 || (bpp != 24 && bpp != 32)) return NULL;
    size_t stride = ((size_t)w * (size_t)(bpp / 8) + 3) & ~(size_t)3;
    if (off + stride * (size_t)h > n) return NULL;
    Image *im = sa_image_new(w, h);
    for (int y = 0; y < h; y++) {
        const u8 *row = d + off + stride * (size_t)(flip ? h - 1 - y : y);
        for (int x = 0; x < w; x++) {
            const u8 *p = row + (size_t)x * (size_t)(bpp / 8);
            im->px[(size_t)y * w + x] = 0xff000000u | (u32)p[0] << 16 | (u32)p[1] << 8 | p[2];
        }
    }
    u32 key = im->px[0];
    size_t same = 0, total = (size_t)w * (size_t)h;
    for (size_t i = 0; i < total; i++) same += im->px[i] == key;
    if (same * 10 >= total)
        for (size_t i = 0; i < total; i++)
            if (im->px[i] == key) im->px[i] = 0;
    return im;
}

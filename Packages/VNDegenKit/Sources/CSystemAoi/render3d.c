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
    for (int i = 0; i < r->w * r->h; i++) r->zbuf[i] = 1e30f;
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

static u32 texel(const Image *t, float u, float v) {
    int x = (int)floorf(u * (float)t->w), y = (int)floorf(v * (float)t->h);
    x %= t->w;
    y %= t->h;
    if (x < 0) x += t->w;
    if (y < 0) y += t->h;
    return t->px[(size_t)y * t->w + x];
}

static float edge(const Scr *a, const Scr *b, float x, float y) { return (b->x - a->x) * (y - a->y) - (b->y - a->y) * (x - a->x); }

static void raster(R3 *r, const Scr *p0, const Scr *p1, const Scr *p2, const Image *tex) {
    float area = edge(p0, p1, p2->x, p2->y);
    if (fabsf(area) < 1e-6f) return;
    if (r->cull && (r->cull > 0 ? area > 0 : area < 0)) return;
    float minx = fminf(p0->x, fminf(p1->x, p2->x)), maxx = fmaxf(p0->x, fmaxf(p1->x, p2->x));
    float miny = fminf(p0->y, fminf(p1->y, p2->y)), maxy = fmaxf(p0->y, fmaxf(p1->y, p2->y));
    int x0 = (int)fmaxf(0, floorf(minx)), x1 = (int)fminf((float)r->w - 1, ceilf(maxx));
    int y0 = (int)fmaxf(0, floorf(miny)), y1 = (int)fminf((float)r->h - 1, ceilf(maxy));
    float inv = 1.0f / area;
    u32 fr = r->fogColor & 0xff, fg = r->fogColor >> 8 & 0xff, fbl = r->fogColor >> 16 & 0xff;
    for (int y = y0; y <= y1; y++) {
        float py = (float)y + 0.5f;
        for (int x = x0; x <= x1; x++) {
            float px = (float)x + 0.5f;
            float w0 = edge(p1, p2, px, py) * inv, w1 = edge(p2, p0, px, py) * inv, w2 = edge(p0, p1, px, py) * inv;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            float iz = w0 * p0->iz + w1 * p1->iz + w2 * p2->iz;
            if (iz <= 0) continue;
            float z = 1.0f / iz;
            size_t i = (size_t)y * r->w + x;
            if (z >= r->zbuf[i]) continue;
            float u = (w0 * p0->uz + w1 * p1->uz + w2 * p2->uz) * z, v = (w0 * p0->vz + w1 * p1->vz + w2 * p2->vz) * z;
            u32 c = tex ? texel(tex, u, v) : 0xffffffffu;
            if (tex && (c >> 24) < 128) continue;
            r->zbuf[i] = z;
            float s = w0 * p0->s + w1 * p1->s + w2 * p2->s;
            float f = r->fogFar > r->fogNear ? (r->fogFar - z) / (r->fogFar - r->fogNear) : 1.0f;
            f = f < 0 ? 0 : f > 1 ? 1 : f;
            float rr = (float)(c & 0xff) * s, gg = (float)(c >> 8 & 0xff) * s, bb = (float)(c >> 16 & 0xff) * s;
            rr = rr > 255 ? 255 : rr;
            gg = gg > 255 ? 255 : gg;
            bb = bb > 255 ? 255 : bb;
            u32 R = (u32)(rr * f + (float)fr * (1 - f)), G = (u32)(gg * f + (float)fg * (1 - f)), B = (u32)(bb * f + (float)fbl * (1 - f));
            r->fb[i] = 0xff000000u | B << 16 | G << 8 | R;
        }
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
    return im;
}

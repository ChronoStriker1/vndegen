#include "sg.h"

#include <math.h>

#define NEAR_Z 0.05f

void r3_init(R3 *r, int w, int h) {
    memset(r, 0, sizeof *r);
    r->w = w;
    r->h = h;
    r->zbuf = sg_alloc(sizeof(float) * (size_t)w * (size_t)h);
}

void r3_free(R3 *r) {
    free(r->zbuf);
    memset(r, 0, sizeof *r);
}

void r3_begin(R3 *r, u32 *fb, u32 clear) {
    r->fb = fb;
    r->tint[0] = r->tint[1] = r->tint[2] = 256;
    size_t n = (size_t)r->w * (size_t)r->h;
    for (size_t i = 0; i < n; i++) fb[i] = clear;
    memset(r->zbuf, 0, sizeof(float) * n);
}

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
    c->focal = (float)h * 0.5f / tanf(c->fov * 0.5f * 3.14159265f / 180.0f);
    c->cx = (float)w * 0.5f;
    c->cy = (float)h * 0.5f;
}

typedef struct Scr {
    float x, y, iz, uz, vz, s;
} Scr;

static float edge(const Scr *a, const Scr *b, float x, float y) { return (b->x - a->x) * (y - a->y) - (b->y - a->y) * (x - a->x); }

static void raster(R3 *r, const Scr *p0, const Scr *p1, const Scr *p2, const Image *tex, bool alphaTest, bool modulate) {
    float area = edge(p0, p1, p2->x, p2->y);
    if (fabsf(area) < 1e-6f) return;
    float minx = fminf(p0->x, fminf(p1->x, p2->x)), maxx = fmaxf(p0->x, fmaxf(p1->x, p2->x));
    float miny = fminf(p0->y, fminf(p1->y, p2->y)), maxy = fmaxf(p0->y, fmaxf(p1->y, p2->y));
    int x0 = (int)fmaxf(0, floorf(minx)), x1 = (int)fminf((float)r->w - 1, ceilf(maxx));
    int y0 = (int)fmaxf(0, floorf(miny)), y1 = (int)fminf((float)r->h - 1, ceilf(maxy));
    if (x0 > x1 || y0 > y1) return;
    float inv = 1.0f / area;
    for (int y = y0; y <= y1; y++) {
        float py = (float)y + 0.5f;
        u32 *row = r->fb + (size_t)y * (size_t)r->w;
        float *zrow = r->zbuf + (size_t)y * (size_t)r->w;
        for (int x = x0; x <= x1; x++) {
            float px = (float)x + 0.5f;
            float w0 = edge(p1, p2, px, py) * inv, w1 = edge(p2, p0, px, py) * inv, w2 = edge(p0, p1, px, py) * inv;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            float iz = w0 * p0->iz + w1 * p1->iz + w2 * p2->iz;
            if (iz <= zrow[x]) continue;
            float z = 1.0f / iz;
            float u = (w0 * p0->uz + w1 * p1->uz + w2 * p2->uz) * z;
            float v = (w0 * p0->vz + w1 * p1->vz + w2 * p2->vz) * z;
            float s = w0 * p0->s + w1 * p1->s + w2 * p2->s;
            u32 c = 0xffffffffu;
            if (tex) {
                int tx = (int)floorf(u * (float)tex->w), ty = (int)floorf(v * (float)tex->h);
                tx %= tex->w;
                ty %= tex->h;
                if (tx < 0) tx += tex->w;
                if (ty < 0) ty += tex->h;
                c = tex->px[(size_t)ty * (size_t)tex->w + (size_t)tx];
            }
            u32 a = c >> 24;
            if (modulate) {
                u32 d = row[x];
                u32 ia = 255 - a;
                u32 mr = ((c & 255) * a + 255 * ia) / 255, mg = ((c >> 8 & 255) * a + 255 * ia) / 255, mb = ((c >> 16 & 255) * a + 255 * ia) / 255;
                row[x] = 0xff000000u | ((d >> 16 & 255) * mb / 255) << 16 | ((d >> 8 & 255) * mg / 255) << 8 | ((d & 255) * mr / 255);
                continue;
            }
            if (alphaTest ? a < 128 : a == 0) continue;
            if (s < 0) s = 0;
            int si = (int)(s * 256.0f);
            u32 rr = ((c & 255) * (u32)si >> 8) * (u32)r->tint[0] >> 8, gg = ((c >> 8 & 255) * (u32)si >> 8) * (u32)r->tint[1] >> 8,
                bb = ((c >> 16 & 255) * (u32)si >> 8) * (u32)r->tint[2] >> 8;
            if (rr > 255) rr = 255;
            if (gg > 255) gg = 255;
            if (bb > 255) bb = 255;
            u32 out = 0xff000000u | bb << 16 | gg << 8 | rr;
            if (!alphaTest && a < 255) {
                u32 d = row[x];
                u32 ia = 255 - a;
                out = 0xff000000u | ((bb * a + (d >> 16 & 255) * ia) / 255) << 16 | ((gg * a + (d >> 8 & 255) * ia) / 255) << 8 |
                      ((rr * a + (d & 255) * ia) / 255);
            } else {
                zrow[x] = iz;
            }
            row[x] = out;
        }
    }
}

static Vert xform(const Cam *c, const Vert *v) {
    const float *m = c->m;
    Vert o = *v;
    o.x = m[0] * v->x + m[1] * v->y + m[2] * v->z + m[3];
    o.y = m[4] * v->x + m[5] * v->y + m[6] * v->z + m[7];
    o.z = m[8] * v->x + m[9] * v->y + m[10] * v->z + m[11];
    return o;
}

static Vert lerp(const Vert *a, const Vert *b, float t) {
    return (Vert){a->x + (b->x - a->x) * t, a->y + (b->y - a->y) * t, a->z + (b->z - a->z) * t,
                  a->u + (b->u - a->u) * t, a->v + (b->v - a->v) * t, a->shade + (b->shade - a->shade) * t};
}

static Scr project(const Cam *c, const Vert *v) {
    float iz = 1.0f / v->z;
    return (Scr){c->cx + v->x * c->focal * iz, c->cy + v->y * c->focal * iz, iz, v->u * iz, v->v * iz, v->shade};
}

void r3_tri(R3 *r, const Cam *c, const Tri *t) {
    Vert in[3] = {xform(c, &t->v[0]), xform(c, &t->v[1]), xform(c, &t->v[2])};
    Vert out[4];
    int n = 0;
    for (int i = 0; i < 3; i++) {
        const Vert *a = &in[i], *b = &in[(i + 1) % 3];
        bool ai = a->z >= NEAR_Z, bi = b->z >= NEAR_Z;
        if (ai) out[n++] = *a;
        if (ai != bi) out[n++] = lerp(a, b, (NEAR_Z - a->z) / (b->z - a->z));
    }
    if (n < 3) return;
    Scr s[4];
    for (int i = 0; i < n; i++) s[i] = project(c, &out[i]);
    raster(r, &s[0], &s[1], &s[2], t->tex, t->alphaTest, t->modulate);
    if (n == 4) raster(r, &s[0], &s[2], &s[3], t->tex, t->alphaTest, t->modulate);
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
    mesh_add(m, &(Tri){{va, vb, vc}, tex, alphaTest, false});
    mesh_add(m, &(Tri){{va, vc, vd}, tex, alphaTest, false});
}

void mesh_free(Mesh *m) {
    free(m->tris);
    memset(m, 0, sizeof *m);
}

#include "sg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct PGroup {
    int emitter;
    float emitterLife, amount, interval;
    float emitterSize[3], gravity[3], drag;
    bool billboard;
    float life, lifeRandom, speed;
    float velocity[3], velocityRandom;
    int colorCount;
    float colors[4][3];
    float minSize, maxSize;
    float alpha[3], alphaTime[2];
    float bright[3], brightTime[2];
    float scale[3], scaleTime[2];
    float angle[3], angleRandom[3], spin[3];
    int patH, patV, patStart, patEnd, patCycle;
    float offset[3];
    Image *tex;
    float carry, clock;
} PGroup;

typedef struct Particle {
    float p[3], v[3];
    float age, life, size, angle, spin;
    float color[3];
    int group;
} Particle;

struct Ptcl {
    PGroup *groups;
    int groupCount;
    Particle *parts;
    int count, cap;
    u32 seed;
};

enum { EMIT_POINT, EMIT_SPHERE, EMIT_BOX };

static float frand(Ptcl *p) {
    p->seed = p->seed * 1664525u + 1013904223u;
    return (float)(p->seed >> 8) / 16777216.0f;
}

static float srand1(Ptcl *p) { return frand(p) * 2.0f - 1.0f; }

static void read_floats(const char *s, float *out, int n) {
    char *end;
    for (int i = 0; i < n; i++) {
        out[i] = strtof(s, &end);
        if (end == s) return;
        s = end;
    }
}

static Image *load_texture(SG *g, const char *ptclPath, const char *name) {
    char dir[1024];
    pack_key(ptclPath, dir, sizeof dir);
    char *slash = strrchr(dir, '/');
    if (slash) slash[1] = 0;
    else dir[0] = 0;
    char *path = sg_fmt("%stexture/%s.png", dir, name);
    size_t size = 0;
    u8 *data = pack_read(&g->data.pack, path, &size);
    free(path);
    if (!data) return NULL;
    Image *im = img_decode(data, size);
    free(data);
    return im;
}

static void group_key(SG *g, const char *ptclPath, PGroup *gr, const char *key, const char *val) {
    if (!strcmp(key, "emitterType")) gr->emitter = !strcmp(val, "sphere") ? EMIT_SPHERE : !strcmp(val, "box") ? EMIT_BOX : EMIT_POINT;
    else if (!strcmp(key, "emitterLifeTime")) read_floats(val, &gr->emitterLife, 1);
    else if (!strcmp(key, "emitterAmount")) read_floats(val, &gr->amount, 1);
    else if (!strcmp(key, "emitInterval")) read_floats(val, &gr->interval, 1);
    else if (!strcmp(key, "emitterSize")) read_floats(val, gr->emitterSize, 3);
    else if (!strcmp(key, "gravity")) read_floats(val, gr->gravity, 3);
    else if (!strcmp(key, "airDrag")) read_floats(val, &gr->drag, 1);
    else if (!strcmp(key, "billboard")) gr->billboard = !strcmp(val, "true");
    else if (!strcmp(key, "particleLifeTime")) read_floats(val, &gr->life, 1);
    else if (!strcmp(key, "particleLifeRandom")) read_floats(val, &gr->lifeRandom, 1);
    else if (!strcmp(key, "baseSpeed")) read_floats(val, &gr->speed, 1);
    else if (!strcmp(key, "linearVelocity")) read_floats(val, gr->velocity, 3);
    else if (!strcmp(key, "linearVelocityRandom")) read_floats(val, &gr->velocityRandom, 1);
    else if (!strcmp(key, "randomColorCount")) gr->colorCount = atoi(val);
    else if (!strncmp(key, "randomColor", 11) && key[11] >= '0' && key[11] <= '3' && !key[12]) read_floats(val, gr->colors[key[11] - '0'], 3);
    else if (!strcmp(key, "minSize")) read_floats(val, &gr->minSize, 1);
    else if (!strcmp(key, "maxSize")) read_floats(val, &gr->maxSize, 1);
    else if (!strcmp(key, "startAlpha")) read_floats(val, &gr->alpha[0], 1);
    else if (!strcmp(key, "middleAlpha")) read_floats(val, &gr->alpha[1], 1);
    else if (!strcmp(key, "endAlpha")) read_floats(val, &gr->alpha[2], 1);
    else if (!strcmp(key, "startAlphaTime")) read_floats(val, &gr->alphaTime[0], 1);
    else if (!strcmp(key, "endAlphaTime")) read_floats(val, &gr->alphaTime[1], 1);
    else if (!strcmp(key, "startBrightness")) read_floats(val, &gr->bright[0], 1);
    else if (!strcmp(key, "middleBrightness")) read_floats(val, &gr->bright[1], 1);
    else if (!strcmp(key, "endBrightness")) read_floats(val, &gr->bright[2], 1);
    else if (!strcmp(key, "startBrightnessTime")) read_floats(val, &gr->brightTime[0], 1);
    else if (!strcmp(key, "endBrightnessTime")) read_floats(val, &gr->brightTime[1], 1);
    else if (!strcmp(key, "startScale")) read_floats(val, &gr->scale[0], 1);
    else if (!strcmp(key, "middleScale")) read_floats(val, &gr->scale[1], 1);
    else if (!strcmp(key, "endScale")) read_floats(val, &gr->scale[2], 1);
    else if (!strcmp(key, "startScaleTime")) read_floats(val, &gr->scaleTime[0], 1);
    else if (!strcmp(key, "endScaleTime")) read_floats(val, &gr->scaleTime[1], 1);
    else if (!strcmp(key, "angle")) read_floats(val, gr->angle, 3);
    else if (!strcmp(key, "angleRandRange")) read_floats(val, gr->angleRandom, 3);
    else if (!strcmp(key, "angluarVelocity")) read_floats(val, gr->spin, 3);
    else if (!strcmp(key, "texPatternH")) gr->patH = atoi(val);
    else if (!strcmp(key, "texPatternV")) gr->patV = atoi(val);
    else if (!strcmp(key, "texPatternStart")) gr->patStart = atoi(val);
    else if (!strcmp(key, "texPatternEnd")) gr->patEnd = atoi(val);
    else if (!strcmp(key, "texPatternCycle")) gr->patCycle = atoi(val);
    else if (!strcmp(key, "positionOffset")) read_floats(val, gr->offset, 3);
    else if (!strcmp(key, "texture")) gr->tex = load_texture(g, ptclPath, val);
}

static void spawn(Ptcl *p, int gi) {
    PGroup *gr = &p->groups[gi];
    if (p->count == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 64;
        p->parts = sg_realloc(p->parts, sizeof(Particle) * (size_t)p->cap);
    }
    Particle *q = &p->parts[p->count++];
    memset(q, 0, sizeof *q);
    q->group = gi;
    float d[3] = {srand1(p), srand1(p), srand1(p)};
    if (gr->emitter == EMIT_SPHERE) {
        float r = cbrtf(frand(p)), n = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (n < 1e-4f) n = 1;
        for (int i = 0; i < 3; i++) q->p[i] = gr->offset[i] + d[i] / n * r * gr->emitterSize[i];
    } else if (gr->emitter == EMIT_BOX) {
        for (int i = 0; i < 3; i++) q->p[i] = gr->offset[i] + d[i] * gr->emitterSize[i];
    } else {
        for (int i = 0; i < 3; i++) q->p[i] = gr->offset[i];
    }
    float u[3] = {srand1(p), srand1(p), srand1(p)};
    float un = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    if (un < 1e-4f) un = 1;
    for (int i = 0; i < 3; i++) q->v[i] = gr->velocity[i] * (1.0f - frand(p) * gr->velocityRandom) + u[i] / un * gr->speed;
    q->life = gr->life * (1.0f + srand1(p) * gr->lifeRandom * 0.5f);
    if (q->life < 0.05f) q->life = 0.05f;
    q->size = gr->minSize + (gr->maxSize - gr->minSize) * frand(p);
    q->angle = gr->angle[2] + srand1(p) * gr->angleRandom[2] * 0.5f;
    q->spin = gr->spin[2];
    int nc = gr->colorCount < 1 ? 1 : gr->colorCount > 4 ? 4 : gr->colorCount;
    memcpy(q->color, gr->colors[(int)(frand(p) * (float)nc) % nc], sizeof q->color);
}

void sg_ptcl_update(Ptcl *p, u32 ms) {
    if (!p) return;
    float dt = (float)ms / 1000.0f;
    while (dt > 0) {
        float step = dt > 0.05f ? 0.05f : dt;
        dt -= step;
        for (int i = 0; i < p->count; i++) {
            Particle *q = &p->parts[i];
            const PGroup *gr = &p->groups[q->group];
            for (int k = 0; k < 3; k++) {
                q->v[k] += gr->gravity[k] * step;
                q->v[k] *= fmaxf(0.0f, 1.0f - gr->drag * step);
                q->p[k] += q->v[k] * step;
            }
            q->angle += q->spin * step;
            q->age += step;
        }
        int n = 0;
        for (int i = 0; i < p->count; i++)
            if (p->parts[i].age < p->parts[i].life) p->parts[n++] = p->parts[i];
        p->count = n;
        for (int gi = 0; gi < p->groupCount; gi++) {
            PGroup *gr = &p->groups[gi];
            gr->clock += step;
            if (gr->emitterLife > 0 && gr->clock > gr->emitterLife) continue;
            if (gr->interval > 0) {
                gr->carry += step;
                while (gr->carry >= gr->interval) {
                    gr->carry -= gr->interval;
                    for (int k = 0; k < (int)gr->amount && p->count < 4096; k++) spawn(p, gi);
                }
            } else {
                gr->carry += gr->amount * step;
                while (gr->carry >= 1.0f && p->count < 4096) {
                    gr->carry -= 1.0f;
                    spawn(p, gi);
                }
            }
        }
    }
}

Ptcl *sg_ptcl_load(SG *g, const char *path) {
    size_t size = 0;
    u8 *data = pack_read(&g->data.pack, path, &size);
    if (!data) {
        sg_log(g, "particle missing: %s", path);
        return NULL;
    }
    data[size] = 0;
    Ptcl *p = sg_alloc(sizeof *p);
    p->seed = 0x2545f491u;
    char *save = NULL;
    PGroup *cur = NULL;
    for (char *line = strtok_r((char *)data, "\r\n", &save); line; line = strtok_r(NULL, "\r\n", &save)) {
        while (*line == ' ' || *line == '\t') line++;
        char *sp = line;
        while (*sp && *sp != ' ' && *sp != '\t') sp++;
        char *val = sp;
        if (*val) *val++ = 0;
        while (*val == ' ' || *val == '\t') val++;
        if (!strcmp(line, "name")) {
            p->groups = sg_realloc(p->groups, sizeof(PGroup) * (size_t)(p->groupCount + 1));
            cur = &p->groups[p->groupCount++];
            memset(cur, 0, sizeof *cur);
            cur->patH = cur->patV = 1;
            cur->colorCount = 1;
            cur->colors[0][0] = cur->colors[0][1] = cur->colors[0][2] = 1;
            cur->alpha[1] = cur->scale[0] = cur->scale[1] = cur->scale[2] = 1;
        } else if (!strcmp(line, "end")) {
            cur = NULL;
        } else if (cur) {
            group_key(g, path, cur, line, val);
        }
    }
    free(data);
    sg_ptcl_update(p, 2000);
    sg_log(g, "particle %s: %d groups", path, p->groupCount);
    return p;
}

void sg_ptcl_free(Ptcl *p) {
    if (!p) return;
    for (int i = 0; i < p->groupCount; i++) img_free(p->groups[i].tex);
    free(p->groups);
    free(p->parts);
    free(p);
}

static float curve3(const float *v, const float *t, float x) {
    if (x < t[0]) return t[0] > 0 ? v[0] + (v[1] - v[0]) * x / t[0] : v[1];
    if (x > 1.0f - t[1]) return t[1] > 0 ? v[1] + (v[2] - v[1]) * (x - (1.0f - t[1])) / t[1] : v[2];
    return v[1];
}

typedef struct PDraw {
    int index;
    float depth;
} PDraw;

static int pdraw_cmp(const void *a, const void *b) {
    float da = ((const PDraw *)a)->depth, db = ((const PDraw *)b)->depth;
    return da < db ? 1 : da > db ? -1 : 0;
}

static u16 col16(float v) {
    v *= 256.0f;
    return (u16)(v < 0 ? 0 : v > 4096 ? 4096 : v);
}

void sg_ptcl_draw(SG *g, Ptcl *p, const Cam *cam, float x, float y, float z) {
    if (!p || !p->count) return;
    const float *m = cam->m;
    float right[3] = {m[0], m[1], m[2]}, up[3] = {-m[4], -m[5], -m[6]};
    PDraw *order = sg_alloc(sizeof(PDraw) * (size_t)p->count);
    for (int i = 0; i < p->count; i++) {
        const Particle *q = &p->parts[i];
        float wx = x + q->p[0], wy = y + q->p[1], wz = z + q->p[2];
        order[i] = (PDraw){i, m[8] * wx + m[9] * wy + m[10] * wz + m[11]};
    }
    qsort(order, (size_t)p->count, sizeof *order, pdraw_cmp);
    for (int k = 0; k < p->count; k++) {
        const Particle *q = &p->parts[order[k].index];
        const PGroup *gr = &p->groups[q->group];
        if (!gr->tex) continue;
        float t = q->age / q->life;
        float alpha = curve3(gr->alpha, gr->alphaTime, t), bright = curve3(gr->bright, gr->brightTime, t);
        float r = q->size * curve3(gr->scale, gr->scaleTime, t);
        if (r <= 0.001f || (alpha <= 0.002f && bright <= 0.002f)) continue;
        int frames = gr->patH * gr->patV;
        int span = gr->patEnd - gr->patStart + 1;
        int frame = gr->patStart;
        if (span > 1) frame += (int)(t * (float)span * (float)(gr->patCycle > 0 ? gr->patCycle : 1)) % span;
        if (frames < 1 || frame < 0 || frame >= frames) frame = 0;
        float cu = 1.0f / (float)gr->patH, cv = 1.0f / (float)gr->patV;
        float u0 = (float)(frame % gr->patH) * cu, v0 = (float)(frame / gr->patH) * cv;
        float ax[3], ay[3];
        if (gr->billboard || fabsf(fabsf(gr->angle[0]) - 90.0f) > 1.0f) {
            float a = q->angle * 3.14159265f / 180.0f, c = cosf(a), s = sinf(a);
            for (int i = 0; i < 3; i++) {
                ax[i] = (right[i] * c + up[i] * s) * r;
                ay[i] = (up[i] * c - right[i] * s) * r;
            }
        } else {
            float a = q->angle * 3.14159265f / 180.0f, c = cosf(a), s = sinf(a);
            ax[0] = c * r, ax[1] = 0, ax[2] = s * r;
            ay[0] = s * r, ay[1] = 0, ay[2] = -c * r;
        }
        float cx = x + q->p[0], cy = y + q->p[1], cz = z + q->p[2];
        Vert va = {cx - ax[0] + ay[0], cy - ax[1] + ay[1], cz - ax[2] + ay[2], u0, v0, 1};
        Vert vb = {cx + ax[0] + ay[0], cy + ax[1] + ay[1], cz + ax[2] + ay[2], u0 + cu, v0, 1};
        Vert vc = {cx + ax[0] - ay[0], cy + ax[1] - ay[1], cz + ax[2] - ay[2], u0 + cu, v0 + cv, 1};
        Vert vd = {cx - ax[0] - ay[0], cy - ax[1] - ay[1], cz - ax[2] - ay[2], u0, v0 + cv, 1};
        Tri t1 = {.v = {va, vb, vc}, .tex = gr->tex}, t2 = {.v = {va, vc, vd}, .tex = gr->tex};
        if (alpha > 0.002f) {
            u16 c[4] = {col16(q->color[0]), col16(q->color[1]), col16(q->color[2]), col16(alpha)};
            memcpy(t1.color, c, sizeof c);
            memcpy(t2.color, c, sizeof c);
            r3_tri_ex(&g->r3, cam, &t1, TRI_BLEND, TF_NOZWRITE);
            r3_tri_ex(&g->r3, cam, &t2, TRI_BLEND, TF_NOZWRITE);
        }
        if (bright > 0.002f) {
            u16 c[4] = {col16(q->color[0] * bright * 2.0f), col16(q->color[1] * bright * 2.0f), col16(q->color[2] * bright * 2.0f), 256};
            memcpy(t1.color, c, sizeof c);
            memcpy(t2.color, c, sizeof c);
            r3_tri_ex(&g->r3, cam, &t1, TRI_ADD, TF_NOZWRITE);
            r3_tri_ex(&g->r3, cam, &t2, TRI_ADD, TF_NOZWRITE);
        }
    }
    free(order);
}

#include "sa.h"
#include <math.h>

#define FX_STEP_MS 50
#define FX_CEIL 5.5f
#define FX_NEAR 1.0f
#define FX_SWAY 0.08f
#define DEG 0.017453293f

static float frand(DunFx *fx) {
    fx->rng = fx->rng * 1664525u + 1013904223u;
    return (float)(fx->rng >> 8) / 16777216.0f;
}

void sa_fx_free(DunFx *fx) {
    sa_image_free(fx->tex);
    memset(fx, 0, sizeof *fx);
}

void sa_fx_load(DunFx *fx, Vfs *vfs, const char *def) {
    sa_fx_free(fx);
    char type[32], file[48];
    if (!sa_def_string(def, "type", type, sizeof type) || !sa_def_string(def, "File", file, sizeof file)) return;
    size_t n;
    u8 *data = sa_vfs_read(vfs, file, &n);
    fx->tex = data ? sa_bmp_decode_alpha(data, n) : NULL;
    free(data);
    if (!fx->tex) return;
    fx->rng = 0x2545f491u;
    if (!strcmp(type, "Leaf")) {
        fx->type = FX_LEAF;
        fx->size = sa_def_value(def, "Rate", 0.5f);
        fx->num = sa_def_value(def, "Num", 10);
        fx->fall = sa_def_value(def, "Fall", 0.05f);
        static const char *const flutter[3] = {"FlutterX", "FlutterY", "FlutterZ"}, *const rot[3] = {"RotX", "RotY", "RotZ"};
        static const float flutterBase[3] = {0.04f, 0.04f, 0.04f}, rotBase[3] = {0, 5, 2.5f};
        for (int i = 0; i < 3; i++) {
            fx->flutter[i] = sa_def_value(def, flutter[i], flutterBase[i]);
            fx->rot[i] = sa_def_value(def, rot[i], rotBase[i]);
        }
        fx->count = (int)fx->num < FX_PARTICLES ? (int)fx->num : FX_PARTICLES;
    } else if (!strcmp(type, "snow2")) {
        fx->type = FX_SNOW;
        fx->num = sa_def_value(def, "Num", 0.1f);
        fx->size = sa_def_value(def, "Size", 0.1f);
        fx->fall = sa_def_value(def, "Speed", -0.005f);
        const char *clip = strstr(def, "\nClip");
        fx->clip[0] = 0;
        fx->clip[1] = 30;
        if (clip && (clip = strchr(clip, '='))) sscanf(clip + 1, "%f,%f", &fx->clip[0], &fx->clip[1]);
        fx->fadeIn = sa_def_value(def, "AlphaSt", 40);
        fx->fadeOut = sa_def_value(def, "AlphaEd", 40);
        float volume = 3.14159265f * (fx->clip[1] * fx->clip[1] - fx->clip[0] * fx->clip[0]) * FX_CEIL;
        int want = (int)(fx->num * volume + 0.5f);
        fx->count = want < FX_PARTICLES ? want : FX_PARTICLES;
    } else {
        sa_fx_free(fx);
    }
}

static void spawn(DunFx *fx, FxParticle *p, float camX, float camZ, bool anyHeight) {
    if (fx->type == FX_LEAF) {
        p->x = fx->area[0] + frand(fx) * (fx->area[2] - fx->area[0]);
        p->z = fx->area[1] + frand(fx) * (fx->area[3] - fx->area[1]);
        p->y = anyHeight ? frand(fx) * FX_CEIL : FX_CEIL;
    } else {
        float a = frand(fx) * 6.2831853f, r = fx->clip[0] + (fx->clip[1] - fx->clip[0]) * sqrtf(frand(fx));
        p->x = camX + cosf(a) * r;
        p->z = camZ + sinf(a) * r;
        p->y = fx->fall > 0 ? 0 : FX_CEIL;
        if (anyHeight) p->y = frand(fx) * FX_CEIL;
        p->life = FX_CEIL / fmaxf(fabsf(fx->fall), 1e-4f);
        p->age = anyHeight ? (fx->fall > 0 ? p->y : FX_CEIL - p->y) / fmaxf(fabsf(fx->fall), 1e-4f) : 0;
    }
    p->phase = frand(fx) * 6.2831853f;
    p->angle = frand(fx) * 360;
    p->flip = frand(fx) * 360;
}

static void step(DunFx *fx, float camX, float camZ) {
    for (int i = 0; i < fx->count; i++) {
        FxParticle *p = &fx->p[i];
        p->phase += FX_SWAY;
        if (fx->type == FX_LEAF) {
            p->y -= fx->fall;
            p->x += fx->flutter[0] * sinf(p->phase);
            p->y += fx->flutter[1] * cosf(p->phase);
            p->z += fx->flutter[2] * cosf(p->phase);
            p->angle += fx->rot[2];
            p->flip += fx->rot[1];
            if (p->y < 0) spawn(fx, p, camX, camZ, false);
        } else {
            p->y += fx->fall;
            p->age += 1;
            float dx = p->x - camX, dz = p->z - camZ;
            if (p->age >= p->life || dx * dx + dz * dz > fx->clip[1] * fx->clip[1]) spawn(fx, p, camX, camZ, false);
        }
    }
}

void sa_fx_area(DunFx *fx, float x0, float z0, float x1, float z1) {
    fx->area[0] = x0;
    fx->area[1] = z0;
    fx->area[2] = x1;
    fx->area[3] = z1;
}

bool sa_fx_tick(DunFx *fx, u32 ms, float camX, float camZ) {
    if (fx->type == FX_NONE || !fx->count) return false;
    if (!fx->placed) {
        for (int i = 0; i < fx->count; i++) spawn(fx, &fx->p[i], camX, camZ, true);
        fx->placed = true;
    }
    fx->ms += ms;
    int steps = 0;
    while (fx->ms >= FX_STEP_MS && steps < 120) {
        fx->ms -= FX_STEP_MS;
        step(fx, camX, camZ);
        steps++;
    }
    return steps > 0;
}

void sa_fx_draw(DunFx *fx, R3 *r, const float pos[3], const float right[3], const float up[3], const float fwd[3]) {
    if (fx->type == FX_NONE || !fx->tex || !fx->placed) return;
    int cull = r->cull;
    r->cull = 0;
    r->texAlpha = true;
    for (int i = 0; i < fx->count; i++) {
        const FxParticle *p = &fx->p[i];
        float dx = p->x - pos[0], dy = p->y - pos[1], dz = p->z - pos[2];
        float vx = dx * right[0] + dy * right[1] + dz * right[2];
        float vy = dx * up[0] + dy * up[1] + dz * up[2];
        float vz = dx * fwd[0] + dy * fwd[1] + dz * fwd[2];
        if (vz < FX_NEAR) continue;
        float alpha = 1, half;
        float sx = 1, ca = 1, sa = 0;
        if (fx->type == FX_LEAF) {
            half = fx->size * 0.5f;
            ca = cosf(p->angle * DEG);
            sa = sinf(p->angle * DEG);
            sx = cosf(p->flip * DEG);
        } else {
            half = fx->size * 0.5f;
            if (fx->fadeIn > 0 && p->age < fx->fadeIn) alpha = p->age / fx->fadeIn;
            if (fx->fadeOut > 0 && p->life - p->age < fx->fadeOut) alpha = fminf(alpha, (p->life - p->age) / fx->fadeOut);
            if (vz < fx->clip[0] || vz > fx->clip[1]) continue;
        }
        if (alpha <= 0) continue;
        r->blend = alpha;
        static const float corner[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        R3Vert v[4];
        for (int k = 0; k < 4; k++) {
            float cx = corner[k][0] * half * sx, cy = corner[k][1] * half;
            v[k].x = vx + cx * ca - cy * sa;
            v[k].y = vy + cx * sa + cy * ca;
            v[k].z = vz;
            v[k].u = corner[k][0] > 0 ? 1 : 0;
            v[k].v = corner[k][1] > 0 ? 0 : 1;
            v[k].shade = 1;
        }
        R3Vert a[3] = {v[0], v[1], v[2]}, b[3] = {v[0], v[2], v[3]};
        r3_tri(r, a, fx->tex);
        r3_tri(r, b, fx->tex);
    }
    r->blend = 0;
    r->texAlpha = false;
    r->cull = cull;
}

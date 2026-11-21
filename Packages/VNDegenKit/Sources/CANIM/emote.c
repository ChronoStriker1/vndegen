#include "anim.h"
#include "vnemote.h"

#include <dlfcn.h>
#include <math.h>

enum { EMOTE_KEY = 0x473ab96d, MAX_MODELS = 8, IDLE_VARS = 8 };
enum { IDLE_STEP_MIN = 800, IDLE_STEP_MAX = 1900, IDLE_LONG_AFTER = 40000 };
static const float CHAR_DROP = 100.0f;
static const char IDLE_DEFAULT[] = "sPauseWait01", IDLE_LONG[] = "sPoseWaitPause", IDLE_LONG_FIXED[] = "body_slant";

typedef struct IdleTrack {
    const IdleMotion *motion;
    float from[IDLE_VARS], to[IDLE_VARS], now[IDLE_VARS];
    u32 start, length;
    bool running, held, high;
} IdleTrack;

typedef struct Tex {
    const u8 *px;
    int w, h;
} Tex;

typedef struct EmoteSlot {
    int layer;
    char name[64];
    vnemote *model;
    u32 varSerial;
    u32 lastNow;
    u32 used;
    bool failed;
    bool built, plain;
    float ticks, scale;
    u64 hash;
    int sameBuilds;
    u32 buildSerial;
    u32 *cache;
    size_t cacheCap;
    int cacheX, cacheY, cacheW, cacheH;
    u32 cacheSerial;
    float cacheOx, cacheOy, cacheScale;
    float viewOx, viewOy, viewScale;
    bool cacheValid;
    const vnemote_draw *draws;
    const vnemote_vertex *verts;
    const vnemote_stencil *stencils;
    int drawCount, stencilCount;
    Tex *tex, *stencilTex;
    int texCap, stencilTexCap;
    IdleTrack idle[2];
    u32 seed;
} EmoteSlot;

struct EmoteCache {
    EmoteSlot slot[MAX_MODELS];
    u32 tick;
};

static const vnemote_api_v1 *api(AN *a) {
    static const vnemote_api_v1 *table;
    static bool tried;
    if (!tried) {
        tried = true;
        void *sym = dlsym(RTLD_DEFAULT, "vnemote_get_api_v1");
        if (sym) table = ((const vnemote_api_v1 *(*)(void))sym)();
        if (!table || table->version < 1) table = NULL;
        if (!table) an_log(a, "the E-mote library isn't present; characters aren't drawn");
    }
    return table;
}

static void slot_free(const vnemote_api_v1 *e, EmoteSlot *s) {
    if (s->model && e) e->close(s->model);
    free(s->tex);
    free(s->stencilTex);
    free(s->cache);
    memset(s, 0, sizeof *s);
}

void an_emote_free(AN *a) {
    struct EmoteCache *c = a->emotes;
    if (!c) return;
    const vnemote_api_v1 *e = api(a);
    an_pool_free(a);
    for (int i = 0; i < MAX_MODELS; i++) slot_free(e, &c->slot[i]);
    free(c);
    a->emotes = NULL;
}

void an_layer_set_var(Layer *l, const char *name, float value) {
    for (int i = 0; i < l->varCount; i++)
        if (!strcmp(l->vars[i].name, name)) {
            l->vars[i].value = value;
            return;
        }
    if (l->varCount >= (int)(sizeof l->vars / sizeof l->vars[0])) return;
    snprintf(l->vars[l->varCount].name, sizeof l->vars[0].name, "%s", name);
    l->vars[l->varCount++].value = value;
}

void an_layer_apply_preset(AN *a, Layer *l, const char *preset, u32 ms) {
    const EmotePreset *p = an_define_preset(&a->def, preset);
    if (!p) {
        an_log(a, "no E-mote preset %s", preset);
        return;
    }
    for (int i = 0; i < p->count; i++) an_layer_set_var(l, p->vars[i].name, p->vars[i].value);
    snprintf(l->motion, sizeof l->motion, "%s", preset);
    l->motionTime = ms;
    l->varSerial++;
}

static void push_vars(const vnemote_api_v1 *e, vnemote *m, const Layer *l, u32 ms) {
    float ticks = (float)ms * 60.0f / 1000.0f;
    for (int i = 0; i < l->varCount; i++) e->set_variable(m, l->vars[i].name, l->vars[i].value, ticks, 0);
}

static EmoteSlot *slot_for(AN *a, int layer, Layer *l) {
    const vnemote_api_v1 *e = api(a);
    if (!e) return NULL;
    if (!a->emotes) a->emotes = an_alloc(sizeof(struct EmoteCache));
    struct EmoteCache *c = a->emotes;
    c->tick++;
    EmoteSlot *free_slot = NULL, *oldest = NULL;
    for (int i = 0; i < MAX_MODELS; i++) {
        EmoteSlot *s = &c->slot[i];
        if (s->name[0] && s->layer == layer) {
            if (!strcmp(s->name, l->name)) {
                s->used = c->tick;
                return s->failed ? NULL : s;
            }
            slot_free(e, s);
        }
        if (!s->name[0] && !free_slot) free_slot = s;
        if (!oldest || s->used < oldest->used) oldest = s;
    }
    EmoteSlot *s = free_slot ? free_slot : oldest;
    slot_free(e, s);
    s->layer = layer;
    snprintf(s->name, sizeof s->name, "%s", l->name);
    s->used = c->tick;
    s->lastNow = a->now;
    const PackEntry *pe = an_pack_find(&a->psb, l->name);
    u8 *data = pe ? an_pack_read(&a->psb, pe) : NULL;
    char err[256] = "";
    s->model = data ? e->open(data, pe->size, EMOTE_KEY, err, sizeof err) : NULL;
    free(data);
    if (!s->model) {
        an_log(a, "can't open E-mote %s: %s", l->name, data ? err : "not in psbpack.dat");
        s->failed = true;
        return NULL;
    }
    s->varSerial = l->varSerial;
    push_vars(e, s->model, l, 0);
    e->reset(s->model);
    l->fresh = false;
    return s;
}

static inline u32 blend_px(u32 d, float r, float g, float b, float al, u32 mode) {
    float dr = (float)(d & 0xff) / 255.0f, dg = (float)(d >> 8 & 0xff) / 255.0f, db = (float)(d >> 16 & 0xff) / 255.0f;
    switch (mode) {
    case 1:
        dr += r * al;
        dg += g * al;
        db += b * al;
        break;
    case 2:
    case 5:
        dr -= r * al;
        dg -= g * al;
        db -= b * al;
        break;
    case 3:
        dr = r * dr + dr * (1 - al);
        dg = g * dg + dg * (1 - al);
        db = b * db + db * (1 - al);
        break;
    case 4:
        dr = r * (1 - dr) + dr;
        dg = g * (1 - dg) + dg;
        db = b * (1 - db) + db;
        break;
    default:
        dr = r * al + dr * (1 - al);
        dg = g * al + dg * (1 - al);
        db = b * al + db * (1 - al);
        break;
    }
    u32 R = (u32)(fminf(fmaxf(dr, 0), 1) * 255.0f + 0.5f), G = (u32)(fminf(fmaxf(dg, 0), 1) * 255.0f + 0.5f),
        B = (u32)(fminf(fmaxf(db, 0), 1) * 255.0f + 0.5f);
    return R | G << 8 | B << 16 | 0xff000000u;
}

static inline bool sample(const Tex *t, float u, float v, float out[4]) {
    float fx = u * (float)t->w - 0.5f, fy = v * (float)t->h - 0.5f;
    int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
    float ax = fx - (float)x0, ay = fy - (float)y0;
    int x1 = x0 + 1, y1 = y0 + 1;
    x0 = clampi(x0, 0, t->w - 1);
    x1 = clampi(x1, 0, t->w - 1);
    y0 = clampi(y0, 0, t->h - 1);
    y1 = clampi(y1, 0, t->h - 1);
    const u8 *p00 = t->px + ((size_t)y0 * (size_t)t->w + (size_t)x0) * 4, *p10 = t->px + ((size_t)y0 * (size_t)t->w + (size_t)x1) * 4;
    const u8 *p01 = t->px + ((size_t)y1 * (size_t)t->w + (size_t)x0) * 4, *p11 = t->px + ((size_t)y1 * (size_t)t->w + (size_t)x1) * 4;
    if (!(p00[3] | p10[3] | p01[3] | p11[3])) return false;
    float w00 = (1 - ax) * (1 - ay) * (1.0f / 255.0f), w10 = ax * (1 - ay) * (1.0f / 255.0f);
    float w01 = (1 - ax) * ay * (1.0f / 255.0f), w11 = ax * ay * (1.0f / 255.0f);
    for (int c = 0; c < 4; c++) out[c] = p00[c] * w00 + p10[c] * w10 + p01[c] * w01 + p11[c] * w11;
    return true;
}

typedef struct View {
    float ox, oy, scale, alpha;
    int x0, y0, x1, y1;
    u32 *px;
    int stride, px0, py0;
    bool premul;
} View;

static inline u32 premul_px(u32 d, float r, float g, float b, float al, u32 mode) {
    u32 dr = d & 0xff, dg = d >> 8 & 0xff, db = d >> 16 & 0xff, da = d >> 24;
    if (mode == 1) {
        dr += (u32)(r * al * 255.0f + 0.5f);
        dg += (u32)(g * al * 255.0f + 0.5f);
        db += (u32)(b * al * 255.0f + 0.5f);
        return (dr > 255 ? 255 : dr) | (dg > 255 ? 255 : dg) << 8 | (db > 255 ? 255 : db) << 16 | da << 24;
    }
    u32 A = (u32)(al * 256.0f + 0.5f), IA = 256 - A;
    u32 R = ((u32)(r * 255.0f + 0.5f) * A + dr * IA) >> 8;
    u32 G = ((u32)(g * 255.0f + 0.5f) * A + dg * IA) >> 8;
    u32 B = ((u32)(b * 255.0f + 0.5f) * A + db * IA) >> 8;
    u32 OA = (255 * A + da * IA) >> 8;
    return R | G << 8 | B << 16 | OA << 24;
}

enum { ST_NONE, ST_TEST, ST_INC, ST_DEC };
enum { AT_U, AT_V, AT_R, AT_G, AT_B, AT_A, AT_MX, AT_MY, AT_COUNT };

static void raster(AN *a, const View *vw, const Tex *t, const vnemote_vertex *v, const vnemote_draw *d, int st, u8 ref) {
    float sx[3], sy[3];
    for (int i = 0; i < 3; i++) {
        sx[i] = vw->ox + v[i].x * vw->scale;
        sy[i] = vw->oy + v[i].y * vw->scale;
    }
    float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
    if (fabsf(area) < 1e-6f) return;
    int minx = (int)floorf(fminf(sx[0], fminf(sx[1], sx[2]))), maxx = (int)ceilf(fmaxf(sx[0], fmaxf(sx[1], sx[2])));
    int miny = (int)floorf(fminf(sy[0], fminf(sy[1], sy[2]))), maxy = (int)ceilf(fmaxf(sy[0], fmaxf(sy[1], sy[2])));
    if (maxx < vw->x0 || minx > vw->x1 || maxy < vw->y0 || miny > vw->y1) return;
    minx = clampi(minx, vw->x0, vw->x1);
    maxx = clampi(maxx, vw->x0, vw->x1);
    miny = clampi(miny, vw->y0, vw->y1);
    maxy = clampi(maxy, vw->y0, vw->y1);
    if (minx > maxx || miny > maxy) return;
    float inv = 1.0f / area;
    float e0x = -(sy[2] - sy[1]) * inv, e0y = (sx[2] - sx[1]) * inv;
    float e1x = -(sy[0] - sy[2]) * inv, e1y = (sx[0] - sx[2]) * inv;
    float px0 = (float)minx + 0.5f, py0 = (float)miny + 0.5f;
    float w0r = ((sx[2] - sx[1]) * (py0 - sy[1]) - (sy[2] - sy[1]) * (px0 - sx[1])) * inv;
    float w1r = ((sx[0] - sx[2]) * (py0 - sy[2]) - (sy[0] - sy[2]) * (px0 - sx[2])) * inv;
    float at[3][AT_COUNT];
    for (int i = 0; i < 3; i++) {
        at[i][AT_U] = v[i].u;
        at[i][AT_V] = v[i].v;
        at[i][AT_R] = v[i].r;
        at[i][AT_G] = v[i].g;
        at[i][AT_B] = v[i].b;
        at[i][AT_A] = v[i].a * vw->alpha;
        at[i][AT_MX] = v[i].x;
        at[i][AT_MY] = v[i].y;
    }
    float base[AT_COUNT], dx[AT_COUNT], dy[AT_COUNT];
    for (int k = 0; k < AT_COUNT; k++) {
        float d0 = at[0][k] - at[2][k], d1 = at[1][k] - at[2][k];
        base[k] = at[2][k] + d0 * w0r + d1 * w1r;
        dx[k] = d0 * e0x + d1 * e1x;
        dy[k] = d0 * e0y + d1 * e1y;
    }
    bool clip = d && (d->clip[0] > -1e29f || d->clip[1] > -1e29f || d->clip[2] < 1e29f || d->clip[3] < 1e29f);
    bool mask = st == ST_INC || st == ST_DEC;
    u32 blend = d ? d->blend : 0;
    bool mod2x = (blend & 0xf0) == 0x10;
    u32 mode = blend & 0x0f;
    if (mode > 5) mode = 0;
    const float eps = -1e-4f;
    for (int y = miny; y <= maxy; y++) {
        u32 *row = vw->px + (size_t)(y - vw->py0) * (size_t)vw->stride;
        u8 *srow = a->stencil + (size_t)y * (size_t)a->W;
        float w0 = w0r, w1 = w1r, p[AT_COUNT];
        for (int k = 0; k < AT_COUNT; k++) p[k] = base[k];
        bool inside = false;
        for (int x = minx; x <= maxx; x++, w0 += e0x, w1 += e1x) {
            if (x > minx)
                for (int k = 0; k < AT_COUNT; k++) p[k] += dx[k];
            if (w0 < eps || w1 < eps || 1.0f - w0 - w1 < eps) {
                if (inside) break;
                continue;
            }
            inside = true;
            if (st != ST_NONE && srow[x] != ref) continue;
            if (clip && (p[AT_MX] < d->clip[0] || p[AT_MY] < d->clip[1] || p[AT_MX] > d->clip[2] || p[AT_MY] > d->clip[3])) continue;
            float c[4];
            if (!sample(t, p[AT_U], p[AT_V], c)) continue;
            float al = c[3] * p[AT_A];
            if (al <= 0.003f) continue;
            if (mask) {
                srow[x] = (u8)(st == ST_INC ? ref + 1 : ref - 1);
                continue;
            }
            float r = c[0] * p[AT_R], g = c[1] * p[AT_G], b = c[2] * p[AT_B];
            if (mod2x) {
                r = fminf(r * 2, 1);
                g = fminf(g * 2, 1);
                b = fminf(b * 2, 1);
            }
            u32 *out = &row[x - vw->px0];
            if (vw->premul) {
                *out = premul_px(*out, r, g, b, al, mode);
                continue;
            }
            if (mode == 0) {
                u32 dd = *out;
                u32 A = (u32)(al * 256.0f + 0.5f), IA = 256 - A;
                u32 R = ((u32)(r * 255.0f + 0.5f) * A + (dd & 0xff) * IA) >> 8;
                u32 G = ((u32)(g * 255.0f + 0.5f) * A + (dd >> 8 & 0xff) * IA) >> 8;
                u32 B = ((u32)(b * 255.0f + 0.5f) * A + (dd >> 16 & 0xff) * IA) >> 8;
                *out = R | G << 8 | B << 16 | 0xff000000u;
                continue;
            }
            if (mode == 3 || mode == 4) {
                r *= al;
                g *= al;
                b *= al;
            } else if (mode == 5) {
                r = 1 - r;
                g = 1 - g;
                b = 1 - b;
            }
            *out = blend_px(*out, r, g, b, al, mode);
        }
        w0r += e0y;
        w1r += e1y;
        for (int k = 0; k < AT_COUNT; k++) base[k] += dy[k];
    }
}

static void build_job(void *ctx, int i) {
    EmoteSlot *s = ((EmoteSlot **)ctx)[i];
    const vnemote_api_v1 *e = api(NULL);
    e->set_detail(s->model, s->scale);
    e->progress(s->model, s->ticks);
    e->build(s->model);
    int nv = 0;
    s->draws = e->draws(s->model, &s->drawCount);
    s->verts = e->vertices(s->model, &nv);
    s->stencils = e->stencils(s->model, &s->stencilCount);
    if (s->drawCount > s->texCap) {
        s->texCap = s->drawCount;
        s->tex = realloc(s->tex, sizeof(Tex) * (size_t)s->texCap);
    }
    if (s->stencilCount > s->stencilTexCap) {
        s->stencilTexCap = s->stencilCount;
        s->stencilTex = realloc(s->stencilTex, sizeof(Tex) * (size_t)s->stencilTexCap);
    }
    for (int k = 0; k < s->drawCount; k++) {
        Tex *t = &s->tex[k];
        t->px = e->texture(s->model, s->draws[k].texture, &t->w, &t->h);
        if (t->w <= 0 || t->h <= 0) t->px = NULL;
    }
    for (int k = 0; k < s->stencilCount; k++) {
        Tex *t = &s->stencilTex[k];
        t->px = s->stencils[k].phase == VNEMOTE_STENCIL_NEXT ? NULL : e->texture(s->model, s->stencils[k].texture, &t->w, &t->h);
        if (t->w <= 0 || t->h <= 0) t->px = NULL;
    }
    s->plain = true;
    for (int k = 0; k < s->drawCount; k++)
        if ((s->draws[k].blend & 0x0f) > 1) s->plain = false;
    u64 h = 1469598103934665603ull;
    const u32 *w = (const u32 *)s->verts;
    for (size_t q = 0; q < (size_t)nv * sizeof(vnemote_vertex) / 4; q++) h = (h ^ w[q]) * 1099511628211ull;
    for (int k = 0; k < s->drawCount; k++) h = (h ^ s->draws[k].texture ^ (u64)s->draws[k].first << 20 ^ (u64)s->draws[k].count << 40) * 1099511628211ull;
    s->sameBuilds = h == s->hash ? s->sameBuilds + 1 : 0;
    if (h != s->hash) s->buildSerial++;
    s->hash = h;
    s->built = true;
}

static float layer_var(const Layer *l, const char *name) {
    for (int i = 0; i < l->varCount; i++)
        if (!strcmp(l->vars[i].name, name)) return l->vars[i].value;
    return 0;
}

static float idle_random(EmoteSlot *s) {
    s->seed = s->seed * 1664525u + 1013904223u;
    return (float)(s->seed >> 8) / 16777216.0f;
}

static bool idle_owned(const IdleTrack *t, const char *name) {
    if (!t || !t->motion) return false;
    for (int i = 0; i < t->motion->count && i < IDLE_VARS; i++)
        if (!strcmp(t->motion->vars[i].name, name)) return true;
    return false;
}

static void idle_track(AN *a, const vnemote_api_v1 *e, EmoteSlot *s, Layer *l, IdleTrack *t, const IdleMotion *m,
                       const IdleTrack *over, bool run) {
    if (t->motion != m) {
        memset(t, 0, sizeof *t);
        t->motion = m;
    }
    if (!m) return;
    if (!run) {
        if (t->running) {
            t->running = false;
            t->held = true;
        }
    } else if (!t->running || a->now - t->start >= t->length) {
        t->high = !t->high;
        for (int i = 0; i < m->count && i < IDLE_VARS; i++) {
            float lo = m->vars[i].lo, hi = m->vars[i].hi, mid = (lo + hi) * 0.5f;
            t->from[i] = t->running || t->held ? t->now[i] : layer_var(l, m->vars[i].name);
            t->to[i] = t->high ? mid + (hi - mid) * idle_random(s) : lo + (mid - lo) * idle_random(s);
        }
        t->start = a->now;
        t->length = IDLE_STEP_MIN + (u32)((float)(IDLE_STEP_MAX - IDLE_STEP_MIN) * idle_random(s));
        t->running = true;
        t->held = false;
    }
    if (!t->running && !t->held) return;
    float k = t->running ? fminf((float)(a->now - t->start) / (float)t->length, 1.0f) : 1.0f;
    for (int i = 0; i < m->count && i < IDLE_VARS; i++) {
        if (t->running) t->now[i] = t->from[i] + (t->to[i] - t->from[i]) * k;
        if (over && over->running && idle_owned(over, m->vars[i].name)) continue;
        if (!strcmp(m->name, IDLE_LONG) && !strcmp(m->vars[i].name, IDLE_LONG_FIXED)) continue;
        e->set_variable(s->model, m->vars[i].name, t->now[i], 0, 0);
    }
}

static void idle_step(AN *a, const vnemote_api_v1 *e, EmoteSlot *s, Layer *l, int layer) {
    if (!s->seed) s->seed = 0x9e3779b9u ^ (u32)layer * 2654435761u;
    const IdleMotion *base = an_define_idle(&a->def, l->idle[0] ? l->idle : IDLE_DEFAULT);
    const IdleMotion *wide = an_define_idle(&a->def, IDLE_LONG);
    idle_track(a, e, s, l, &s->idle[1], wide == base ? NULL : wide, NULL, a->now - a->lastInput >= IDLE_LONG_AFTER);
    idle_track(a, e, s, l, &s->idle[0], base, &s->idle[1], true);
}

void an_emote_prepare(AN *a) {
    const vnemote_api_v1 *e = api(a);
    if (!e) return;
    EmoteSlot *jobs[MAX_MODELS];
    int n = 0;
    for (int i = 0; i < AN_LAYERS && n < MAX_MODELS; i++) {
        Layer *l = &a->scene.layer[i];
        if (l->kind != LK_EMOTE) continue;
        EmoteSlot *s = slot_for(a, i, l);
        if (!s) continue;
        if (s->varSerial != l->varSerial) {
            s->varSerial = l->varSerial;
            push_vars(e, s->model, l, l->fresh ? 0 : l->motionTime);
            if (l->fresh) {
                e->reset(s->model);
                memset(s->idle, 0, sizeof s->idle);
            }
            l->fresh = false;
        }
        idle_step(a, e, s, l, i);
        float ticks = (float)(a->now - s->lastNow) * 60.0f / 1000.0f;
        s->lastNow = a->now;
        s->ticks = ticks > 6 ? 6 : ticks;
        s->scale = (l->scale > 0 ? (float)l->scale / 1000.0f : 1.0f) * a->emoteScale;
        jobs[n++] = s;
    }
    an_parallel(a, n, build_job, jobs);
}

typedef struct RasterJob {
    AN *a;
    EmoteSlot *s;
    View view;
    int left, right, top, bottom;
    int bands;
} RasterJob;

static void raster_band(void *ctx, int band) {
    const RasterJob *j = ctx;
    AN *a = j->a;
    const EmoteSlot *s = j->s;
    int rows = j->bottom - j->top + 1;
    int top = j->top + rows * band / j->bands, bottom = j->top + rows * (band + 1) / j->bands - 1;
    if (bottom < top) return;
    View vw = j->view;
    for (int i = 0; i < s->drawCount; i++) {
        const vnemote_draw *d = &s->draws[i];
        if (d->pass != VNEMOTE_PASS_NORMAL && d->pass != VNEMOTE_PASS_FILTERED) continue;
        const Tex *t = &s->tex[i];
        if (!t->px) continue;
        vw.x0 = j->left;
        vw.y0 = top;
        vw.x1 = j->right;
        vw.y1 = bottom;
        if (!d->stencil_count) {
            for (u32 k = 0; k + 2 < d->count; k += 3) raster(a, &vw, t, s->verts + d->first + k, d, ST_NONE, 0);
            continue;
        }
        float bx0 = 1e30f, by0 = 1e30f, bx1 = -1e30f, by1 = -1e30f;
        for (u32 k = 0; k < d->count; k++) {
            const vnemote_vertex *p = s->verts + d->first + k;
            float px = vw.ox + p->x * vw.scale, py = vw.oy + p->y * vw.scale;
            bx0 = fminf(bx0, px);
            by0 = fminf(by0, py);
            bx1 = fmaxf(bx1, px);
            by1 = fmaxf(by1, py);
        }
        vw.x0 = clampi((int)floorf(bx0), j->left, j->right);
        vw.y0 = clampi((int)floorf(by0), top, bottom);
        vw.x1 = clampi((int)ceilf(bx1), j->left, j->right);
        vw.y1 = clampi((int)ceilf(by1), top, bottom);
        if (ceilf(by1) < (float)top || floorf(by0) > (float)bottom || ceilf(bx1) < (float)j->left || floorf(bx0) > (float)j->right) continue;
        for (int y = vw.y0; y <= vw.y1; y++) memset(a->stencil + (size_t)y * (size_t)a->W + (size_t)vw.x0, (int)d->stencil_initial, (size_t)(vw.x1 - vw.x0 + 1));
        u8 ref = (u8)d->stencil_initial;
        float keepAlpha = vw.alpha;
        vw.alpha = 1.0f;
        for (u32 k = d->stencil_first; k < d->stencil_first + d->stencil_count && (int)k < s->stencilCount; k++) {
            const vnemote_stencil *m = &s->stencils[k];
            if (m->phase == VNEMOTE_STENCIL_NEXT) {
                ref++;
                continue;
            }
            const Tex *mt = &s->stencilTex[k];
            if (!mt->px) continue;
            int op = m->phase == VNEMOTE_STENCIL_INNER ? ST_INC : ST_DEC;
            u8 r = m->phase == VNEMOTE_STENCIL_INNER ? ref : (u8)d->stencil_final;
            for (u32 q = 0; q + 2 < m->count; q += 3) raster(a, &vw, mt, s->verts + m->first + q, NULL, op, r);
        }
        vw.alpha = keepAlpha;
        for (u32 k = 0; k + 2 < d->count; k += 3) raster(a, &vw, t, s->verts + d->first + k, d, ST_TEST, (u8)d->stencil_final);
    }
}

static void composite_band(void *ctx, int band) {
    const RasterJob *j = ctx;
    AN *a = j->a;
    const EmoteSlot *s = j->s;
    int top = s->cacheH * band / j->bands, bottom = s->cacheH * (band + 1) / j->bands;
    for (int y = top; y < bottom; y++) {
        const u32 *src = s->cache + (size_t)y * (size_t)s->cacheW;
        u32 *dst = a->fb + (size_t)(s->cacheY + y) * (size_t)a->W + (size_t)s->cacheX;
        for (int x = 0; x < s->cacheW; x++) {
            u32 c = src[x], sa = c >> 24;
            if (!sa && !(c & 0xffffffu)) continue;
            if (sa == 255) {
                dst[x] = c;
                continue;
            }
            u32 d = dst[x], ia = 255 - sa;
            u32 r = (c & 0xff) + ((d & 0xff) * ia + 127) / 255, g = (c >> 8 & 0xff) + ((d >> 8 & 0xff) * ia + 127) / 255;
            u32 b = (c >> 16 & 0xff) + ((d >> 16 & 0xff) * ia + 127) / 255;
            dst[x] = (r > 255 ? 255 : r) | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b) << 16 | 0xff000000u;
        }
    }
}

static bool cache_bounds(AN *a, const EmoteSlot *s, const View *vw, int *x0, int *y0, int *x1, int *y1) {
    float bx0 = 1e30f, by0 = 1e30f, bx1 = -1e30f, by1 = -1e30f;
    for (int i = 0; i < s->drawCount; i++) {
        const vnemote_draw *d = &s->draws[i];
        if (d->pass != VNEMOTE_PASS_NORMAL && d->pass != VNEMOTE_PASS_FILTERED) continue;
        for (u32 k = 0; k < d->count; k++) {
            const vnemote_vertex *p = s->verts + d->first + k;
            bx0 = fminf(bx0, p->x);
            by0 = fminf(by0, p->y);
            bx1 = fmaxf(bx1, p->x);
            by1 = fmaxf(by1, p->y);
        }
    }
    *x0 = clampi((int)floorf(vw->ox + bx0 * vw->scale) - 1, 0, a->W - 1);
    *y0 = clampi((int)floorf(vw->oy + by0 * vw->scale) - 1, 0, a->H - 1);
    *x1 = clampi((int)ceilf(vw->ox + bx1 * vw->scale) + 1, 0, a->W - 1);
    *y1 = clampi((int)ceilf(vw->oy + by1 * vw->scale) + 1, 0, a->H - 1);
    return bx0 <= bx1 && *x0 <= *x1 && *y0 <= *y1;
}

void an_emote_draw(AN *a, int layer, Layer *l, int x, int y, int alpha) {
    struct EmoteCache *c = a->emotes;
    if (!c) return;
    EmoteSlot *s = NULL;
    for (int i = 0; i < MAX_MODELS && !s; i++)
        if (c->slot[i].built && c->slot[i].layer == layer && !strcmp(c->slot[i].name, l->name)) s = &c->slot[i];
    if (!s) return;
    float charScale = an_define_char_scale(&a->def, l->name);
    int baseX = a->slotX[layer] ? a->slotX[layer] : a->W / 2;
    RasterJob j = {a, s, {(float)(baseX + x), (float)(a->H / 2 + a->slotY[layer] + y) + CHAR_DROP * charScale, s->scale,
                          (float)alpha / 255.0f, 0, 0, 0, 0, a->fb, a->W, 0, 0, false}, 0, a->W - 1, 0, a->H - 1, 0};
    if (!a->stencil) a->stencil = an_alloc((size_t)a->W * (size_t)a->H);
    j.bands = an_pool_size(a) * 2;
    View *v = &j.view;
    bool moving = s->viewOx != v->ox || s->viewOy != v->oy || s->viewScale != v->scale;
    s->viewOx = v->ox;
    s->viewOy = v->oy;
    s->viewScale = v->scale;
    if (!s->sameBuilds || !s->plain || alpha < 255 || moving) {
        s->cacheValid = false;
        an_parallel(a, j.bands, raster_band, &j);
        return;
    }
    if (!s->cacheValid || s->cacheSerial != s->buildSerial || s->cacheOx != v->ox || s->cacheOy != v->oy || s->cacheScale != v->scale) {
        int x0, y0, x1, y1;
        if (!cache_bounds(a, s, v, &x0, &y0, &x1, &y1)) return;
        s->cacheX = x0;
        s->cacheY = y0;
        s->cacheW = x1 - x0 + 1;
        s->cacheH = y1 - y0 + 1;
        size_t need = (size_t)s->cacheW * (size_t)s->cacheH;
        if (need > s->cacheCap) {
            free(s->cache);
            s->cache = an_alloc(need * sizeof(u32));
            s->cacheCap = need;
        }
        memset(s->cache, 0, need * sizeof(u32));
        v->px = s->cache;
        v->stride = s->cacheW;
        v->px0 = x0;
        v->py0 = y0;
        v->premul = true;
        j.left = x0;
        j.right = x1;
        j.top = y0;
        j.bottom = y1;
        an_parallel(a, j.bands, raster_band, &j);
        s->cacheValid = true;
        s->cacheSerial = s->buildSerial;
        s->cacheOx = v->ox;
        s->cacheOy = v->oy;
        s->cacheScale = v->scale;
    }
    an_parallel(a, j.bands, composite_band, &j);
}

#include "anim.h"
#include "vnemote.h"

#include <dlfcn.h>
#include <math.h>

enum { EMOTE_KEY = 0x473ab96d, MAX_MODELS = 8 };

typedef struct EmoteSlot {
    int layer;
    char name[64];
    vnemote *model;
    char motion[64];
    u32 lastNow;
    u32 used;
    bool failed;
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
    memset(s, 0, sizeof *s);
}

void an_emote_free(AN *a) {
    struct EmoteCache *c = a->emotes;
    if (!c) return;
    const vnemote_api_v1 *e = api(a);
    for (int i = 0; i < MAX_MODELS; i++) slot_free(e, &c->slot[i]);
    free(c);
    a->emotes = NULL;
}

static void apply_preset(AN *a, const vnemote_api_v1 *e, vnemote *m, const char *name, u32 ms) {
    const EmotePreset *p = an_define_preset(&a->def, name);
    if (!p) {
        if (!e->play_timeline(m, name, 0, 0)) an_log(a, "no E-mote preset or timeline %s", name);
        return;
    }
    float ticks = (float)ms * 60.0f / 1000.0f;
    for (int i = 0; i < p->count; i++) e->set_variable(m, p->vars[i].name, p->vars[i].value, ticks, 0);
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
    apply_preset(a, e, s->model, "AllReset", 0);
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

typedef struct Tex {
    const u8 *px;
    int w, h;
} Tex;

static inline void sample(const Tex *t, float u, float v, float out[4]) {
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
    for (int c = 0; c < 4; c++) {
        float top = p00[c] + (p10[c] - p00[c]) * ax, bottom = p01[c] + (p11[c] - p01[c]) * ax;
        out[c] = (top + (bottom - top) * ay) / 255.0f;
    }
}

typedef struct View {
    float ox, oy, scale, alpha;
    int x0, y0, x1, y1;
} View;

enum { ST_NONE, ST_TEST, ST_INC, ST_DEC };

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
    minx = clampi(minx, vw->x0, vw->x1);
    maxx = clampi(maxx, vw->x0, vw->x1);
    miny = clampi(miny, vw->y0, vw->y1);
    maxy = clampi(maxy, vw->y0, vw->y1);
    float inv = 1.0f / area;
    u32 blend = d ? d->blend : 0;
    bool mod2x = (blend & 0xf0) == 0x10;
    u32 mode = blend & 0x0f;
    if (mode > 5) mode = 0;
    for (int y = miny; y <= maxy; y++) {
        float py = (float)y + 0.5f;
        u32 *row = a->fb + (size_t)y * (size_t)a->W;
        u8 *srow = a->stencil + (size_t)y * (size_t)a->W;
        for (int x = minx; x <= maxx; x++) {
            if (st != ST_NONE && srow[x] != ref) continue;
            float px = (float)x + 0.5f;
            float w0 = ((sx[2] - sx[1]) * (py - sy[1]) - (sy[2] - sy[1]) * (px - sx[1])) * inv;
            float w1 = ((sx[0] - sx[2]) * (py - sy[2]) - (sy[0] - sy[2]) * (px - sx[2])) * inv;
            float w2 = 1.0f - w0 - w1;
            if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) continue;
            float mx = v[0].x * w0 + v[1].x * w1 + v[2].x * w2, my = v[0].y * w0 + v[1].y * w1 + v[2].y * w2;
            if (d && (mx < d->clip[0] || my < d->clip[1] || mx > d->clip[2] || my > d->clip[3])) continue;
            float u = v[0].u * w0 + v[1].u * w1 + v[2].u * w2, vv = v[0].v * w0 + v[1].v * w1 + v[2].v * w2;
            float c[4];
            sample(t, u, vv, c);
            float r = c[0] * (v[0].r * w0 + v[1].r * w1 + v[2].r * w2);
            float g = c[1] * (v[0].g * w0 + v[1].g * w1 + v[2].g * w2);
            float b = c[2] * (v[0].b * w0 + v[1].b * w1 + v[2].b * w2);
            float al = c[3] * (v[0].a * w0 + v[1].a * w1 + v[2].a * w2) * vw->alpha;
            if (al <= 0.003f) continue;
            if (st == ST_INC || st == ST_DEC) {
                srow[x] = (u8)(st == ST_INC ? ref + 1 : ref - 1);
                continue;
            }
            if (mod2x) {
                r = fminf(r * 2, 1);
                g = fminf(g * 2, 1);
                b = fminf(b * 2, 1);
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
            row[x] = blend_px(row[x], r, g, b, al, mode);
        }
    }
}

void an_emote_draw(AN *a, int layer, Layer *l, int x, int y, int alpha) {
    const vnemote_api_v1 *e = api(a);
    EmoteSlot *s = e ? slot_for(a, layer, l) : NULL;
    if (!s) return;
    if (strcmp(s->motion, l->motion)) {
        snprintf(s->motion, sizeof s->motion, "%s", l->motion);
        if (s->motion[0]) apply_preset(a, e, s->model, s->motion, l->motionTime);
    }
    float ticks = (float)(a->now - s->lastNow) * 60.0f / 1000.0f;
    s->lastNow = a->now;
    if (ticks > 6) ticks = 6;
    e->progress(s->model, ticks);
    e->build(s->model);
    int nd = 0, nv = 0;
    const vnemote_draw *draws = e->draws(s->model, &nd);
    const vnemote_vertex *verts = e->vertices(s->model, &nv);
    float charScale = an_define_char_scale(&a->def, l->name);
    float scale = (l->scale > 0 ? (float)l->scale / 1000.0f : 1.0f) * a->emoteScale * charScale;
    int baseX = a->slotX[layer] ? a->slotX[layer] : a->W / 2;
    View vw = {(float)(baseX + x), (float)(a->H / 2 + a->slotY[layer] + y) + 256.0f * a->emoteScale * charScale, scale,
               (float)alpha / 255.0f, 0, 0, 0, 0};
    int ns = 0;
    const vnemote_stencil *stencils = e->stencils(s->model, &ns);
    if (!a->stencil) a->stencil = an_alloc((size_t)a->W * (size_t)a->H);
    for (int i = 0; i < nd; i++) {
        const vnemote_draw *d = &draws[i];
        if (d->pass != VNEMOTE_PASS_NORMAL && d->pass != VNEMOTE_PASS_FILTERED) continue;
        Tex t;
        t.px = e->texture(s->model, d->texture, &t.w, &t.h);
        if (!t.px || t.w <= 0 || t.h <= 0) continue;
        vw.x0 = 0;
        vw.y0 = 0;
        vw.x1 = a->W - 1;
        vw.y1 = a->H - 1;
        if (!d->stencil_count) {
            for (u32 k = 0; k + 2 < d->count; k += 3) raster(a, &vw, &t, verts + d->first + k, d, ST_NONE, 0);
            continue;
        }
        float bx0 = 1e30f, by0 = 1e30f, bx1 = -1e30f, by1 = -1e30f;
        for (u32 k = 0; k < d->count; k++) {
            const vnemote_vertex *p = verts + d->first + k;
            float px = vw.ox + p->x * vw.scale, py = vw.oy + p->y * vw.scale;
            bx0 = fminf(bx0, px);
            by0 = fminf(by0, py);
            bx1 = fmaxf(bx1, px);
            by1 = fmaxf(by1, py);
        }
        vw.x0 = clampi((int)floorf(bx0), 0, a->W - 1);
        vw.y0 = clampi((int)floorf(by0), 0, a->H - 1);
        vw.x1 = clampi((int)ceilf(bx1), 0, a->W - 1);
        vw.y1 = clampi((int)ceilf(by1), 0, a->H - 1);
        if (vw.x1 < vw.x0 || vw.y1 < vw.y0) continue;
        for (int y = vw.y0; y <= vw.y1; y++) memset(a->stencil + (size_t)y * (size_t)a->W + (size_t)vw.x0, (int)d->stencil_initial, (size_t)(vw.x1 - vw.x0 + 1));
        u8 ref = (u8)d->stencil_initial;
        float keepAlpha = vw.alpha;
        vw.alpha = 1.0f;
        for (u32 k = d->stencil_first; k < d->stencil_first + d->stencil_count && (int)k < ns; k++) {
            const vnemote_stencil *m = &stencils[k];
            if (m->phase == VNEMOTE_STENCIL_NEXT) {
                ref++;
                continue;
            }
            Tex mt;
            mt.px = e->texture(s->model, m->texture, &mt.w, &mt.h);
            if (!mt.px || mt.w <= 0 || mt.h <= 0) continue;
            int op = m->phase == VNEMOTE_STENCIL_INNER ? ST_INC : ST_DEC;
            u8 r = m->phase == VNEMOTE_STENCIL_INNER ? ref : (u8)d->stencil_final;
            for (u32 q = 0; q + 2 < m->count; q += 3) raster(a, &vw, &mt, verts + m->first + q, NULL, op, r);
        }
        vw.alpha = keepAlpha;
        for (u32 k = 0; k + 2 < d->count; k += 3) raster(a, &vw, &t, verts + d->first + k, d, ST_TEST, (u8)d->stencil_final);
    }
}

#include "age.h"

#include <math.h>

static M4 m_ident(void) {
    M4 m = { { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } } };
    return m;
}

static M4 m_mul(M4 a, M4 b) {
    M4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[i][k] * b.m[k][j];
            r.m[i][j] = s;
        }
    return r;
}

static M4 m_trans(float x, float y, float z) {
    M4 m = m_ident();
    m.m[3][0] = x;
    m.m[3][1] = y;
    m.m[3][2] = z;
    return m;
}

static M4 m_scale(Vec3 s) {
    M4 m = m_ident();
    m.m[0][0] = s.x;
    m.m[1][1] = s.y;
    m.m[2][2] = s.z;
    return m;
}

static M4 m_rot(Vec3 a, float deg) {
    float len = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
    if (len < 1e-6f || deg == 0) return m_ident();
    float x = a.x / len, y = a.y / len, z = a.z / len;
    float r = deg * 3.14159265f / 180.0f, c = cosf(r), s = sinf(r), t = 1 - c;
    M4 m = m_ident();
    m.m[0][0] = t * x * x + c;
    m.m[0][1] = t * x * y + s * z;
    m.m[0][2] = t * x * z - s * y;
    m.m[1][0] = t * x * y - s * z;
    m.m[1][1] = t * y * y + c;
    m.m[1][2] = t * y * z + s * x;
    m.m[2][0] = t * x * z + s * y;
    m.m[2][1] = t * y * z - s * x;
    m.m[2][2] = t * z * z + c;
    return m;
}

static M4 m_lerp(M4 a, M4 b, float t) {
    M4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) r.m[i][j] = a.m[i][j] + (b.m[i][j] - a.m[i][j]) * t;
    return r;
}

static Vec3 v3(float x, float y, float z) { Vec3 v = { x, y, z }; return v; }
static Vec3 v_lerp(Vec3 a, Vec3 b, float t) { return v3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }

static void obj_init(Obj *o, i32 handle) {
    memset(o, 0, sizeof *o);
    o->handle = handle;
    o->color = 0xffffffffu;
    o->scale = v3(1, 1, 1);
    o->scaleTo = o->scale;
    o->rotAxis = v3(0, 0, 1);
    o->rotAxisTo = o->rotAxis;
}

void gfx_init(AG *g) {
    Gfx *x = &g->gfx;
    for (int i = 0; i < AG_SURFACES; i++) { x->surf[i].rec[0] = x->surf[i].snap[0] = -1; x->surf[i].res = -1; }
    x->target = 1000;
    x->background = 0xff000000u;
    x->tileEdge = 128;
    obj_init(&x->range, 0);
}

static void objs_clear(Gfx *x) {
    for (int i = 0; i < x->nobj; i++) free(x->obj[i]);
    x->nobj = 0;
}

void gfx_close(AG *g) {
    Gfx *x = &g->gfx;
    objs_clear(x);
    free(x->obj);
    for (int i = 0; i < AG_SURFACES; i++) image_free(x->surf[i].im);
}

void gfx_reset(AG *g) {
    Gfx *x = &g->gfx;
    objs_clear(x);
    for (int i = 0; i < AG_SURFACES; i++) x->surf[i].res = -1;
    x->target = 1000;
    x->rangeFirst = x->rangeCount = 0;
    obj_init(&x->range, 0);
}

static int find(Gfx *x, i32 h, bool *found) {
    int lo = 0, hi = x->nobj;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (x->obj[mid]->handle < h) lo = mid + 1;
        else hi = mid;
    }
    *found = lo < x->nobj && x->obj[lo]->handle == h;
    return lo;
}

Obj *gfx_obj(AG *g, i32 h, bool create) {
    Gfx *x = &g->gfx;
    bool found;
    int i = find(x, h, &found);
    if (found) return x->obj[i];
    if (!create) return NULL;
    if (x->nobj >= x->cap) {
        x->cap = x->cap ? x->cap * 2 : 256;
        x->obj = ag_realloc(x->obj, sizeof(Obj *) * (size_t)x->cap);
    }
    memmove(x->obj + i + 1, x->obj + i, sizeof(Obj *) * (size_t)(x->nobj - i));
    Obj *o = ag_alloc(sizeof *o);
    obj_init(o, h);
    x->obj[i] = o;
    x->nobj++;
    return o;
}

void gfx_erase(AG *g, i32 h);
static void obj_erase(Gfx *x, i32 h) {
    bool found;
    int i = find(x, h, &found);
    if (!found) return;
    free(x->obj[i]);
    memmove(x->obj + i, x->obj + i + 1, sizeof(Obj *) * (size_t)(x->nobj - i - 1));
    x->nobj--;
}

void gfx_erase(AG *g, i32 h) { obj_erase(&g->gfx, h); }

Image *gfx_surface(AG *g, i32 slot) {
    if (slot < 0 || slot >= AG_SURFACES) return NULL;
    return g->gfx.surf[slot].im;
}

void gfx_set_surface(AG *g, i32 slot, Image *im, i32 res, bool created) {
    if (slot < 0 || slot >= AG_SURFACES) { image_free(im); return; }
    Surface *s = &g->gfx.surf[slot];
    if (s->im != im) image_free(s->im);
    s->im = im;
    s->res = res;
    s->created = created;
    if (created) { s->rec[0] = -1; s->rec[4] = 1; }
}

static void surf_drawn(AG *g, i32 slot) {
    if (slot >= 0 && slot < AG_SURFACES) g->gfx.surf[slot].rec[4] = 1;
}

static float chan_t(Gfx *x, Obj *o, Chan *c, bool *done) {
    *done = false;
    if (!c->on) return 1;
    if (x->forceComplete && !(o->detached & 1)) { *done = true; return 1; }
    i32 el = (i32)(x->frameNow - o->start) - c->delay;
    if (el < 0) return 0;
    if (c->dur <= 0 || el >= c->dur) { *done = true; return 1; }
    return (float)el / (float)c->dur;
}

static u32 lerp_color(u32 a, u32 b, float t) {
    u32 r = 0;
    for (int k = 0; k < 32; k += 8) {
        int ca = (int)(a >> k & 255), cb = (int)(b >> k & 255);
        r |= (u32)(ca + (int)((cb - ca) * t)) << k;
    }
    return r;
}

static void obj_sample(Gfx *x, Obj *o) {
    if (!(o->cColor.on || o->cScale.on || o->cRot.on || o->cTrans.on || o->cCell.on)) return;
    if (!o->started) { o->start = x->frameNow; o->started = true; }
    bool done;
    float t = chan_t(x, o, &o->cColor, &done);
    if (o->cColor.on) {
        o->color = lerp_color(o->colorFrom, o->colorTo, t);
        if (done) { o->color = o->colorTo; o->cColor.on = false; }
    }
    t = chan_t(x, o, &o->cScale, &done);
    if (o->cScale.on) {
        o->scale = v_lerp(o->scaleFrom, o->scaleTo, t);
        if (done) { o->scale = o->scaleTo; o->cScale.on = false; }
    }
    t = chan_t(x, o, &o->cRot, &done);
    if (o->cRot.on) {
        o->rotAxis = o->rotAxisTo;
        o->rotAngle = o->rotFrom + (o->rotTo - o->rotFrom) * t;
        if (done) { o->rotAngle = o->rotTo; o->cRot.on = false; }
    }
    t = chan_t(x, o, &o->cTrans, &done);
    if (o->cTrans.on) {
        o->trans = v_lerp(o->transFrom, o->transTo, t);
        if (done) { o->trans = o->transTo; o->cTrans.on = false; }
    }
    if (o->cCell.on) {
        i32 el = (i32)(x->frameNow - o->start) - o->cCell.delay;
        bool forced = x->forceComplete && !(o->detached & 1);
        if (!forced && el < 0) {
            o->cellX = o->cellY = 0;
            o->cellHidden = !(o->cellFlags & 2);
        } else if (forced || el >= o->cCell.dur) {
            o->cCell.on = false;
            if (o->cellFlags & 1) {
                int f = o->cellFrames - 1;
                o->cellX = o->cellCols ? f % o->cellCols : 0;
                o->cellY = o->cellCols ? f / o->cellCols : 0;
                o->cellHidden = false;
            } else o->cellHidden = true;
        } else {
            int f = o->cCell.dur ? (int)((i64)el * o->cellFrames / o->cCell.dur) : 0;
            o->cellX = o->cellCols ? f % o->cellCols : 0;
            o->cellY = o->cellCols ? f / o->cellCols : 0;
            o->cellHidden = false;
        }
    }
    if (!(o->cColor.on || o->cScale.on || o->cRot.on || o->cTrans.on || o->cCell.on)) o->started = false;
}

static float cyc_phase(Gfx *x, Obj *o, int k) {
    i32 p = o->cycPeriod[k];
    if (p <= 0) return 0;
    if (!o->cycStarted[k]) { o->cycStart[k] = x->frameNow; o->cycStarted[k] = true; }
    u32 e = (x->frameNow - o->cycStart[k]) % (u32)p;
    u32 m = e < (u32)p - e ? e : (u32)p - e;
    return 2.0f * (float)m / (float)p;
}

static M4 obj_matrix(Gfx *x, Obj *o, u32 *color) {
    M4 m = m_trans(-o->anchor.x, -o->anchor.y, -o->anchor.z);
    m = m_mul(m, m_scale(o->scale));
    m = m_mul(m, m_rot(o->rotAxis, o->rotAngle));
    m = m_mul(m, m_trans(o->trans.x, o->trans.y, o->trans.z));
    *color = o->color;
    if (o->cycPeriod[0] > 0) *color = lerp_color(o->color, o->cycColor, cyc_phase(x, o, 0));
    if (o->cycPeriod[1] > 0) m = m_mul(m, m_lerp(m_ident(), m_scale(o->cycScale), cyc_phase(x, o, 1)));
    if (o->cycPeriod[2] > 0) {
        if (!o->cycStarted[2]) { o->cycStart[2] = x->frameNow; o->cycStarted[2] = true; }
        u32 e = (x->frameNow - o->cycStart[2]) % (u32)o->cycPeriod[2];
        int deg = (int)((u64)e * 360 / (u32)o->cycPeriod[2]);
        m = m_mul(m, m_rot(o->cycAxis, (float)deg));
    }
    if (o->cycPeriod[3] > 0) m = m_mul(m, m_lerp(m_ident(), m_trans(o->cycTrans.x, o->cycTrans.y, o->cycTrans.z), cyc_phase(x, o, 3)));
    m = m_mul(m, m_trans(o->anchor.x, o->anchor.y, o->anchor.z));
    return m;
}

static M4 range_matrix(Gfx *x) {
    Obj *o = &x->range;
    obj_sample(x, o);
    u32 c;
    return obj_matrix(x, o, &c);
}

static inline u32 blend_px(u32 d, u32 s, int mode, u32 mod, bool live, bool offscreen) {
    int sa = (int)(s >> 24);
    int mr = (int)(mod & 255), mg = (int)(mod >> 8 & 255), mb = (int)(mod >> 16 & 255), ma = (int)(mod >> 24);
    int r = (int)(s & 255) * mr / 255, gg = (int)(s >> 8 & 255) * mg / 255, b = (int)(s >> 16 & 255) * mb / 255;
    if (mode != 0 || live) sa = sa * ma / 255;
    int dr = (int)(d & 255), dg = (int)(d >> 8 & 255), db = (int)(d >> 16 & 255), da = (int)(d >> 24);
    if (mode == 1 || mode == 3) {
        if (mode == 1) {
            dr += r * sa / 255; dg += gg * sa / 255; db += b * sa / 255;
            if (dr > 255) dr = 255;
            if (dg > 255) dg = 255;
            if (db > 255) db = 255;
        } else {
            dr -= r * sa / 255; dg -= gg * sa / 255; db -= b * sa / 255;
            if (dr < 0) dr = 0;
            if (dg < 0) dg = 0;
            if (db < 0) db = 0;
        }
        return (u32)dr | (u32)dg << 8 | (u32)db << 16 | (u32)da << 24;
    }
    if (mode == 2 && offscreen) return (u32)r | (u32)gg << 8 | (u32)b << 16 | (u32)sa << 24;
    if (sa == 255) return (u32)r | (u32)gg << 8 | (u32)b << 16 | (offscreen ? 0xff000000u : (u32)da << 24 | 0xff000000u);
    if (sa == 0) return d;
    int ia = 255 - sa;
    if (offscreen) {
        int oa = sa + da * ia / 255;
        if (oa == 0) return 0;
        int orr = (r * sa + dr * da * ia / 255) / oa, og = (gg * sa + dg * da * ia / 255) / oa, ob = (b * sa + db * da * ia / 255) / oa;
        return (u32)orr | (u32)og << 8 | (u32)ob << 16 | (u32)oa << 24;
    }
    dr = (r * sa + dr * ia) / 255;
    dg = (gg * sa + dg * ia) / 255;
    db = (b * sa + db * ia) / 255;
    return (u32)dr | (u32)dg << 8 | (u32)db << 16 | 0xff000000u;
}

static u32 sample_bilinear(const Image *im, float u, float v, int l, int t, int r, int b) {
    u -= 0.5f;
    v -= 0.5f;
    int x0 = (int)floorf(u), y0 = (int)floorf(v);
    float fx = u - (float)x0, fy = v - (float)y0;
    u32 c[4];
    for (int k = 0; k < 4; k++) {
        int xx = x0 + (k & 1), yy = y0 + (k >> 1);
        if (xx < l) xx = l;
        if (xx >= r) xx = r - 1;
        if (yy < t) yy = t;
        if (yy >= b) yy = b - 1;
        c[k] = im->px[(size_t)yy * (size_t)im->w + (size_t)xx];
    }
    u32 out = 0;
    for (int s = 0; s < 32; s += 8) {
        float a = (float)(c[0] >> s & 255) * (1 - fx) + (float)(c[1] >> s & 255) * fx;
        float bb = (float)(c[2] >> s & 255) * (1 - fx) + (float)(c[3] >> s & 255) * fx;
        int v2 = (int)(a * (1 - fy) + bb * fy + 0.5f);
        out |= (u32)clampi(v2, 0, 255) << s;
    }
    return out;
}

static void draw_obj(AG *g, Image *dst, Obj *o, bool offscreen, bool useRange, M4 rm) {
    Gfx *x = &g->gfx;
    obj_sample(x, o);
    if (!o->visible || o->cellHidden) return;
    Image *src = gfx_surface(g, o->slot);
    if (!src) return;
    int w = o->sr - o->sl, h = o->sb - o->st;
    if (w <= 0 || h <= 0) return;
    int sl = o->sl, st = o->st;
    if (o->cCell.on || o->cellX || o->cellY) { sl += o->cellX * w; st += o->cellY * h; }
    if (o->cycPeriod[4] > 0 && o->loopFrames > 0) {
        if (!o->cycStarted[4]) { o->cycStart[4] = x->frameNow; o->cycStarted[4] = true; }
        int f = (int)((x->frameNow - o->cycStart[4]) / (u32)o->cycPeriod[4]) % o->loopFrames;
        if (o->loopCols > 0) { sl += (f % o->loopCols) * w; st += (f / o->loopCols) * h; }
    }
    int bx = sl, by = st;
    int sr = sl + w, sb = st + h;
    if (sl < 0) sl = 0;
    if (st < 0) st = 0;
    if (sr > src->w) sr = src->w;
    if (sb > src->h) sb = src->h;
    if (sr <= sl || sb <= st) return;
    u32 mod;
    M4 m = obj_matrix(x, o, &mod);
    if (useRange) m = m_mul(m, rm);
    float a = m.m[0][0], b = m.m[0][1], c = m.m[1][0], d = m.m[1][1];
    float tx = m.m[3][0] + o->pos.x * a + o->pos.y * c, ty = m.m[3][1] + o->pos.x * b + o->pos.y * d;
    int mode = o->mode;
    bool live = o->live || g->gfx.surf[o->slot].created;
    if (mode == 0 && !live && (mod & 0xffffff) == 0xffffff) mod = 0xffffffffu;
    if (fabsf(a - 1) < 1e-4f && fabsf(d - 1) < 1e-4f && fabsf(b) < 1e-4f && fabsf(c) < 1e-4f && fabsf(tx - roundf(tx)) < 1e-3f && fabsf(ty - roundf(ty)) < 1e-3f) {
        int ox = (int)roundf(tx) + (sl - bx), oy = (int)roundf(ty) + (st - by);
        int x0 = ox, y0 = oy, x1 = ox + (sr - sl), y1 = oy + (sb - st);
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > dst->w) x1 = dst->w;
        if (y1 > dst->h) y1 = dst->h;
        for (int yy = y0; yy < y1; yy++) {
            const u32 *sp = src->px + (size_t)(st + yy - oy) * (size_t)src->w + (size_t)sl;
            u32 *dp = dst->px + (size_t)yy * (size_t)dst->w;
            for (int xx = x0; xx < x1; xx++) dp[xx] = blend_px(dp[xx], sp[xx - ox], mode, mod, live, offscreen);
        }
        return;
    }
    float det = a * d - b * c;
    if (fabsf(det) < 1e-8f) return;
    float ia = d / det, ib = -b / det, ic = -c / det, id = a / det;
    float qx[4] = { 0, (float)(sr - sl), 0, (float)(sr - sl) }, qy[4] = { 0, 0, (float)(sb - st), (float)(sb - st) };
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    float offx = (float)(sl - bx), offy = (float)(st - by);
    for (int k = 0; k < 4; k++) {
        float px = qx[k] + offx, py = qy[k] + offy;
        float X = px * a + py * c + tx, Y = px * b + py * d + ty;
        if (X < minx) minx = X;
        if (X > maxx) maxx = X;
        if (Y < miny) miny = Y;
        if (Y > maxy) maxy = Y;
    }
    int x0 = (int)floorf(minx), y0 = (int)floorf(miny), x1 = (int)ceilf(maxx), y1 = (int)ceilf(maxy);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > dst->w) x1 = dst->w;
    if (y1 > dst->h) y1 = dst->h;
    bool scaled = fabsf(fabsf(det) - 1) > 1e-3f || fabsf(b) > 1e-4f || fabsf(c) > 1e-4f;
    for (int yy = y0; yy < y1; yy++) {
        u32 *dp = dst->px + (size_t)yy * (size_t)dst->w;
        for (int xx = x0; xx < x1; xx++) {
            float X = (float)xx + 0.5f - tx, Y = (float)yy + 0.5f - ty;
            float u = X * ia + Y * ic - offx, v = X * ib + Y * id - offy;
            if (u < 0 || v < 0 || u >= (float)(sr - sl) || v >= (float)(sb - st)) continue;
            u32 s;
            if (scaled) s = sample_bilinear(src, u + (float)sl, v + (float)st, sl, st, sr, sb);
            else s = src->px[(size_t)(st + (int)v) * (size_t)src->w + (size_t)(sl + (int)u)];
            dp[xx] = blend_px(dp[xx], s, mode, mod, live, offscreen);
        }
    }
}

static float cmd_t(Gfx *x, Cmd *c, bool *done) {
    if (!c->started) { c->start = x->frameNow; c->started = true; }
    i32 el = (i32)(x->frameNow - c->start) - c->delay;
    *done = x->forceComplete || el >= c->dur;
    if (*done) return 1;
    if (el <= 0) return 0;
    return (float)el / (float)c->dur;
}

static bool in_cmd_range(Gfx *x, i32 h) {
    for (int i = 0; i < AG_CMDS; i++) {
        Cmd *c = &x->cmd[i];
        if (!c->on) continue;
        if (h >= c->a0 && h < c->a0 + c->an) return true;
        if (c->type != 3 && h >= c->b0 && h < c->b0 + c->bn) return true;
    }
    return false;
}

static void draw_range(AG *g, Image *dst, i32 first, i32 count, bool offscreen, M4 rm) {
    Gfx *x = &g->gfx;
    for (int i = 0; i < x->nobj; i++) {
        Obj *o = x->obj[i];
        if (o->handle < first || o->handle >= first + count) continue;
        bool inRange = x->rangeCount > 0 && o->handle >= x->rangeFirst && o->handle < x->rangeFirst + x->rangeCount;
        draw_obj(g, dst, o, offscreen, inRange, rm);
    }
}

static void run_cmds(AG *g, M4 rm) {
    Gfx *x = &g->gfx;
    for (int i = 0; i < AG_CMDS; i++) {
        Cmd *c = &x->cmd[i];
        if (!c->on || c->type == 3) continue;
        Image *t = gfx_surface(g, c->target);
        bool done;
        float f = cmd_t(x, c, &done);
        if (t) {
            memset(t->px, 0, sizeof(u32) * (size_t)t->w * (size_t)t->h);
            draw_range(g, t, c->a0, c->an, true, rm);
            Image *b = image_new(t->w, t->h);
            draw_range(g, b, c->b0, c->bn, true, rm);
            int k = (int)(f * 256);
            for (int p = 0; p < t->w * t->h; p++) {
                u32 sb = b->px[p], d = t->px[p];
                int ba = (int)(sb >> 24) * k / 256;
                if (!ba) continue;
                int da = (int)(d >> 24), ia = 255 - ba;
                int oa = ba + da * ia / 255;
                u32 o = 0;
                for (int s2 = 0; s2 < 24; s2 += 8) {
                    int cs = (int)(sb >> s2 & 255), cd = (int)(d >> s2 & 255);
                    o |= (u32)(oa ? (cs * ba + cd * da * ia / 255) / oa : 0) << s2;
                }
                t->px[p] = o | (u32)oa << 24;
            }
            image_free(b);
        }
        if (done) c->on = false;
    }
}

static void flush_blur(AG *g, Image *dst, bool offscreen, M4 rm, i32 upto) {
    Gfx *x = &g->gfx;
    for (int i = 0; i < AG_CMDS; i++) {
        Cmd *c = &x->cmd[i];
        if (!c->on || c->type != 3 || c->drawn || c->key > upto) continue;
        c->drawn = true;
        bool done;
        cmd_t(x, c, &done);
        c->on = false;
        draw_range(g, dst, c->a0, c->an, offscreen, rm);
        c->on = !done;
    }
}

void gfx_render(AG *g, i32 target, i32 first, i32 count) {
    Gfx *x = &g->gfx;
    Image screen = { g->width, g->height, g->screen };
    Image *dst = target >= 1000 || target < 0 ? &screen : gfx_surface(g, target);
    if (!dst) return;
    bool offscreen = dst != &screen;
    if (!offscreen && count < 0) {
        u32 bg = x->background | 0xff000000u;
        for (int i = 0; i < g->width * g->height; i++) g->screen[i] = bg;
    }
    M4 rm = m_ident();
    if (x->rangeCount > 0) rm = range_matrix(x);
    if (count < 0) {
        run_cmds(g, rm);
        for (int i = 0; i < AG_CMDS; i++) x->cmd[i].drawn = false;
    }
    for (int i = 0; i < x->nobj; i++) {
        Obj *o = x->obj[i];
        if (count >= 0 && (o->handle < first || o->handle >= first + count)) continue;
        if (count < 0) {
            flush_blur(g, dst, offscreen, rm, o->handle);
            if (in_cmd_range(x, o->handle)) continue;
        }
        bool inRange = x->rangeCount > 0 && o->handle >= x->rangeFirst && o->handle < x->rangeFirst + x->rangeCount;
        draw_obj(g, dst, o, offscreen, inRange, rm);
    }
    if (count < 0) flush_blur(g, dst, offscreen, rm, 0x7fffffff);
    if (!offscreen) g->frameSerial++;
}

bool gfx_busy(AG *g) {
    Gfx *x = &g->gfx;
    if (x->animDur > 0) {
        if (!x->animStarted) { x->animStart = x->frameNow; x->animStarted = true; }
        if (x->frameNow - x->animStart < x->animDur) return true;
        x->animDur = 0;
        x->animStarted = false;
    }
    for (int i = 0; i < x->nobj; i++) {
        Obj *o = x->obj[i];
        if (o->detached & 1) continue;
        if (o->cColor.on || o->cScale.on || o->cRot.on || o->cTrans.on || o->cCell.on) {
            obj_sample(x, o);
            if (o->cColor.on || o->cScale.on || o->cRot.on || o->cTrans.on || o->cCell.on) return true;
        }
    }
    for (int i = 0; i < AG_CMDS; i++) {
        Cmd *c = &x->cmd[i];
        if (!c->on) continue;
        bool done;
        cmd_t(x, c, &done);
        if (!done) return true;
        if (c->type == 3) c->on = false;
        else { int w, h; g->needRender = true; vnage_pixels(g, &w, &h); }
    }
    Obj *r = &x->range;
    obj_sample(x, r);
    return r->cColor.on || r->cScale.on || r->cRot.on || r->cTrans.on;
}

void gfx_complete(AG *g) {
    Gfx *x = &g->gfx;
    if (x->serviceFlags & 2) return;
    x->forceComplete = true;
    for (int i = 0; i < x->nobj; i++) obj_sample(x, x->obj[i]);
    obj_sample(x, &x->range);
    for (int i = 0; i < AG_CMDS; i++)
        if (x->cmd[i].on) {
            if (x->cmd[i].type == 3) x->cmd[i].on = false;
            else { M4 rm = m_ident(); run_cmds(g, rm); }
        }
    x->forceComplete = false;
    x->animDur = 0;
    x->animStarted = false;
}

void gfx_tick(AG *g) {
    Gfx *x = &g->gfx;
    x->framePrev = x->frameNow;
    x->frameNow = g->now;
}

static float arg_f(AG *g, int n) {
    if (arg_type(g, n) == 1) {
        u32 v = arg_raw(g, n);
        float f;
        memcpy(&f, &v, 4);
        return f;
    }
    return (float)arg_i(g, n);
}

static int op_create_surface(AG *g) {
    i32 slot = arg_i(g, 1), w = arg_i(g, 2), h = arg_i(g, 3);
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return OP_NEXT;
    Image *im = image_new(w, h);
    gfx_set_surface(g, slot, im, -1, true);
    return OP_NEXT;
}

static Image *load_agf(AG *g, i32 res, i32 key) {
    size_t n = 0;
    u8 *d = store_read(&g->st, (u32)res, &n);
    if (!d) { vm_problem(g, "picture %#x can't be read", res); return NULL; }
    Image *im = agf_decode(d, n);
    free(d);
    if (!im) { vm_problem(g, "picture %#x can't be decoded", res); return NULL; }
    if (key >= 0) {
        u32 k = (u32)(key >> 16 & 255) | (u32)(key & 0xff00) | (u32)(key & 255) << 16;
        for (int i = 0; i < im->w * im->h; i++)
            if ((im->px[i] & 0xffffff) == k) im->px[i] = 0;
    }
    return im;
}

static int op_load_surface(AG *g) {
    i32 res = arg_i(g, 1), slot = arg_i(g, 2), key = arg_i(g, 3);
    Image *im = load_agf(g, res, key);
    if (im) {
        gfx_set_surface(g, slot, im, res, false);
        Surface *s = &g->gfx.surf[slot];
        s->rec[0] = res;
        s->rec[1] = key < 0 ? 0 : (i32)(0xff000000u | (u32)key);
        s->rec[4] = 0;
    }
    return OP_NEXT;
}

static int op_load_raw_surface(AG *g) {
    i32 res = arg_i(g, 1), slot = arg_i(g, 2), key = arg_i(g, 3);
    Image *im = load_agf(g, res, key);
    if (im) {
        gfx_set_surface(g, slot, im, -1, false);
        Surface *s = &g->gfx.surf[slot];
        s->rec[0] = -1;
        s->rec[1] = key < 0 ? 0 : (i32)(0xff000000u | (u32)key);
        s->rec[4] = 0;
    }
    return OP_NEXT;
}

static int op_release_surface(AG *g) {
    i32 slot = arg_i(g, 1);
    movie_stop(g, slot);
    if (slot >= 0 && slot < AG_SURFACES) {
        image_free(g->gfx.surf[slot].im);
        g->gfx.surf[slot].im = NULL;
        g->gfx.surf[slot].res = -1;
        g->gfx.surf[slot].created = false;
        g->gfx.surf[slot].rec[0] = -1;
    }
    return OP_NEXT;
}

static int op_release_transient(AG *g) {
    for (int s = 42; s < 1000; s++) {
        movie_stop(g, s);
        image_free(g->gfx.surf[s].im);
        g->gfx.surf[s].im = NULL;
        g->gfx.surf[s].res = -1;
        g->gfx.surf[s].created = false;
        g->gfx.surf[s].rec[0] = -1;
    }
    return OP_NEXT;
}

static int op_bind(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->slot = arg_i(g, 2);
    o->sl = arg_i(g, 3);
    o->st = arg_i(g, 4);
    o->sr = o->sl + arg_i(g, 5);
    o->sb = o->st + arg_i(g, 6);
    o->pos = v3((float)arg_i(g, 7), (float)arg_i(g, 8), 0);
    o->visible = true;
    o->live = false;
    return OP_NEXT;
}

static u32 resolve_color(u32 cur, i32 alpha, i32 rgb) {
    u32 a = alpha < 0 ? cur >> 24 : (u32)(alpha > 255 ? 255 : alpha);
    u32 c = rgb < 0 ? cur & 0xffffff : (u32)rgb & 0xffffff;
    u32 abgr = (c >> 16 & 255) | (c & 0xff00) | (c & 255) << 16;
    return a << 24 | abgr;
}

static int op_color(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->mode = arg_i(g, 2);
    o->color = resolve_color(o->color, arg_i(g, 3), arg_i(g, 4));
    if (o->cColor.on) o->colorFrom = o->color;
    if (o->visible) o->live = true;
    return OP_NEXT;
}

static void arm(Obj *o, Chan *c, i32 delay, i32 dur) {
    o->started = false;
    c->delay = delay;
    c->dur = dur;
    c->on = true;
}

static int op_color_anim(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    i32 delay = arg_i(g, 2), dur = arg_i(g, 3);
    o->colorFrom = o->color;
    o->colorTo = resolve_color(o->color, arg_i(g, 4), arg_i(g, 5));
    arm(o, &o->cColor, delay, dur);
    o->live = true;
    return OP_NEXT;
}

static int op_erase(AG *g) {
    i32 h = arg_i(g, 1), n = arg_i(g, 2);
    if (n <= 1) obj_erase(&g->gfx, h);
    else for (i32 i = 0; i < n; i++) obj_erase(&g->gfx, h + i);
    return OP_NEXT;
}

static int op_clear_objects(AG *g) { objs_clear(&g->gfx); return OP_NEXT; }

static int op_set_anchor(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->anchor = v3(arg_f(g, 2), arg_f(g, 3), arg_f(g, 4));
    return OP_NEXT;
}

static int op_get_anchor(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), false);
    if (!o) return OP_NEXT;
    arg_set_i(g, 2, (i32)o->anchor.x);
    arg_set_i(g, 3, (i32)o->anchor.y);
    arg_set_i(g, 4, (i32)o->anchor.z);
    return OP_NEXT;
}

static int op_set_pos(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->pos = v3(arg_f(g, 2), arg_f(g, 3), arg_f(g, 4));
    return OP_NEXT;
}

static int op_get_pos(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), false);
    if (!o) return OP_NEXT;
    arg_set_i(g, 2, (i32)o->pos.x);
    arg_set_i(g, 3, (i32)o->pos.y);
    arg_set_i(g, 4, (i32)o->pos.z);
    return OP_NEXT;
}

static int op_scale_now(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->scale = v3(arg_f(g, 2) / 100, arg_f(g, 3) / 100, arg_f(g, 4) / 100);
    o->scaleTo = o->scale;
    o->cScale.on = false;
    return OP_NEXT;
}

static int op_rot_now(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->rotAxis = v3(arg_f(g, 2), arg_f(g, 3), arg_f(g, 4));
    o->rotAngle = arg_f(g, 5);
    o->rotAxisTo = o->rotAxis;
    o->rotTo = o->rotAngle;
    o->cRot.on = false;
    return OP_NEXT;
}

static int op_trans_now(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->trans = v3(arg_f(g, 2), arg_f(g, 3), arg_f(g, 4));
    o->transTo = o->trans;
    o->cTrans.on = false;
    return OP_NEXT;
}

static int op_scale_to(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->scaleFrom = o->scale;
    o->scaleTo = v3(arg_f(g, 4) / 100, arg_f(g, 5) / 100, arg_f(g, 6) / 100);
    arm(o, &o->cScale, arg_i(g, 2), arg_i(g, 3));
    return OP_NEXT;
}

static int op_rot_to(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->rotFrom = o->rotAngle;
    o->rotAxisTo = v3(arg_f(g, 4), arg_f(g, 5), arg_f(g, 6));
    o->rotTo = arg_f(g, 7);
    arm(o, &o->cRot, arg_i(g, 2), arg_i(g, 3));
    return OP_NEXT;
}

static int op_trans_to(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->transFrom = o->trans;
    o->transTo = v3(arg_f(g, 4), arg_f(g, 5), arg_f(g, 6));
    arm(o, &o->cTrans, arg_i(g, 2), arg_i(g, 3));
    return OP_NEXT;
}

static int op_query_trans(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 2), false);
    if (!o) { arg_set_i(g, 1, 1); return OP_NEXT; }
    arg_set_i(g, 1, 0);
    arg_set_i(g, 3, (i32)o->transTo.x);
    arg_set_i(g, 4, (i32)o->transTo.y);
    arg_set_i(g, 5, (i32)o->transTo.z);
    return OP_NEXT;
}

static int op_query_rot(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 2), false);
    if (!o) { arg_set_i(g, 1, 1); return OP_NEXT; }
    arg_set_i(g, 1, 0);
    arg_set_i(g, 3, (i32)o->rotAxis.x);
    arg_set_i(g, 4, (i32)o->rotAxis.y);
    arg_set_i(g, 5, (i32)o->rotAxis.z);
    arg_set_i(g, 6, (i32)o->rotAngle);
    return OP_NEXT;
}

static int op_query_slot(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 2), false);
    arg_set_i(g, 1, o ? o->slot : -1);
    return OP_NEXT;
}

static int op_query_res(AG *g) {
    i32 s = arg_i(g, 2);
    arg_set_i(g, 1, s >= 0 && s < AG_SURFACES ? g->gfx.surf[s].res : 0);
    return OP_NEXT;
}

static int op_texture_size(AG *g) {
    Image *im = gfx_surface(g, arg_i(g, 1));
    arg_set_i(g, 2, im ? im->w : 0);
    arg_set_i(g, 3, im ? im->h : 0);
    return OP_NEXT;
}

static int op_clone(AG *g) {
    Obj *s = gfx_obj(g, arg_i(g, 1), false);
    i32 dh = arg_i(g, 2);
    if (!s) { obj_erase(&g->gfx, dh); return OP_NEXT; }
    Obj copy = *s;
    Obj *d = gfx_obj(g, dh, true);
    *d = copy;
    d->handle = dh;
    return OP_NEXT;
}

static int op_swap(AG *g) {
    i32 a = arg_i(g, 1), b = arg_i(g, 2);
    Obj *oa = gfx_obj(g, a, false), *ob = gfx_obj(g, b, false);
    if (oa && ob) {
        Obj t = *oa;
        *oa = *ob;
        *ob = t;
        oa->handle = a;
        ob->handle = b;
    } else if (oa || ob) {
        Obj t = oa ? *oa : *ob;
        obj_erase(&g->gfx, oa ? a : b);
        Obj *n = gfx_obj(g, oa ? b : a, true);
        *n = t;
        n->handle = oa ? b : a;
    }
    return OP_NEXT;
}

static int op_detached(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->detached = (u32)arg_i(g, 2);
    return OP_NEXT;
}

static int op_cyc_color(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->cycPeriod[0] = arg_i(g, 2);
    o->cycStarted[0] = false;
    i32 a = arg_i(g, 3);
    o->cycColor = resolve_color(o->color, a > 255 ? 255 : a, arg_i(g, 4));
    return OP_NEXT;
}

static int op_cyc_scale(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->cycPeriod[1] = arg_i(g, 2);
    o->cycStarted[1] = false;
    o->cycScale = v3(arg_f(g, 3) / 100, arg_f(g, 4) / 100, arg_f(g, 5) / 100);
    return OP_NEXT;
}

static int op_cyc_rot(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->cycPeriod[2] = arg_i(g, 2);
    o->cycStarted[2] = false;
    o->cycAxis = v3(arg_f(g, 3), arg_f(g, 4), arg_f(g, 5));
    return OP_NEXT;
}

static int op_cyc_trans(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->cycPeriod[3] = arg_i(g, 2);
    o->cycStarted[3] = false;
    o->cycTrans = v3(arg_f(g, 3), arg_f(g, 4), arg_f(g, 5));
    return OP_NEXT;
}

static int op_cyc_cells(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    o->cycPeriod[4] = arg_i(g, 2);
    o->cycStarted[4] = false;
    o->loopFrames = arg_i(g, 3);
    o->loopCols = arg_i(g, 4);
    return OP_NEXT;
}

static int op_cyc_reset(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    for (int k = 0; k < 5; k++) { o->cycPeriod[k] = 0; o->cycStarted[k] = false; }
    return OP_NEXT;
}

static int op_cells(AG *g) {
    Obj *o = gfx_obj(g, arg_i(g, 1), true);
    if (!o->visible) return OP_NEXT;
    o->cellFrames = arg_i(g, 4);
    o->cellCols = arg_i(g, 5);
    o->cellFlags = arg_i(g, 6);
    o->cellX = o->cellY = 0;
    arm(o, &o->cCell, arg_i(g, 2), arg_i(g, 3));
    return OP_NEXT;
}

static int op_range(AG *g) {
    Gfx *x = &g->gfx;
    obj_init(&x->range, 0);
    x->rangeFirst = arg_i(g, 1);
    x->rangeCount = arg_i(g, 2);
    x->range.anchor = v3(arg_f(g, 3), arg_f(g, 4), arg_f(g, 5));
    return OP_NEXT;
}

static int op_range_scale_now(AG *g) {
    Obj *o = &g->gfx.range;
    o->scale = o->scaleTo = v3(arg_f(g, 1) / 100, arg_f(g, 2) / 100, arg_f(g, 3) / 100);
    o->cScale.on = false;
    return OP_NEXT;
}

static int op_range_trans_now(AG *g) {
    Obj *o = &g->gfx.range;
    o->trans = o->transTo = v3(arg_f(g, 1), arg_f(g, 2), arg_f(g, 3));
    o->cTrans.on = false;
    return OP_NEXT;
}

static int op_range_scale_to(AG *g) {
    Obj *o = &g->gfx.range;
    o->scaleFrom = o->scale;
    o->scaleTo = v3(arg_f(g, 3) / 100, arg_f(g, 4) / 100, arg_f(g, 5) / 100);
    arm(o, &o->cScale, arg_i(g, 1), arg_i(g, 2));
    return OP_NEXT;
}

static int op_range_rot_to(AG *g) {
    Obj *o = &g->gfx.range;
    o->rotFrom = o->rotAngle;
    o->rotAxisTo = v3(arg_f(g, 3), arg_f(g, 4), arg_f(g, 5));
    o->rotTo = arg_f(g, 6);
    arm(o, &o->cRot, arg_i(g, 1), arg_i(g, 2));
    return OP_NEXT;
}

static int op_range_trans_to(AG *g) {
    Obj *o = &g->gfx.range;
    o->transFrom = o->trans;
    o->transTo = v3(arg_f(g, 3), arg_f(g, 4), arg_f(g, 5));
    arm(o, &o->cTrans, arg_i(g, 1), arg_i(g, 2));
    return OP_NEXT;
}

static int op_target(AG *g) { g->gfx.target = arg_i(g, 1); return OP_NEXT; }

static int op_clear_target(AG *g) {
    i32 t = g->gfx.target;
    if (t >= 1000 || t < 0) {
        for (int i = 0; i < g->width * g->height; i++) g->screen[i] = 0xff000000u;
    } else {
        Image *im = gfx_surface(g, t);
        if (im) for (int i = 0; i < im->w * im->h; i++) im->px[i] = 0xff000000u;
    }
    return OP_NEXT;
}

static int op_present(AG *g) {
    if (g->gfx.target >= 1000 || g->gfx.target < 0) { g->needRender = true; return OP_NEXT; }
    gfx_render(g, g->gfx.target, 0, -1);
    return OP_NEXT;
}

static int op_present_range(AG *g) {
    gfx_render(g, g->gfx.target, arg_i(g, 1), arg_i(g, 2));
    return OP_NEXT;
}

static int op_fill_rect(AG *g) {
    Image *im = gfx_surface(g, arg_i(g, 1));
    if (!im) return OP_NEXT;
    i32 x0 = arg_i(g, 2), y0 = arg_i(g, 3), w = arg_i(g, 4), h = arg_i(g, 5), a = arg_i(g, 6), rgb = arg_i(g, 7);
    u32 c = resolve_color(0, a, rgb);
    surf_drawn(g, arg_i(g, 1));
    for (i32 y = y0 < 0 ? 0 : y0; y < y0 + h && y < im->h; y++)
        for (i32 x = x0 < 0 ? 0 : x0; x < x0 + w && x < im->w; x++) im->px[(size_t)y * (size_t)im->w + (size_t)x] = c;
    return OP_NEXT;
}

static void copy_rect(Image *s, Image *d, int sx, int sy, int w, int h, int dx, int dy) {
    for (int y = 0; y < h; y++) {
        int yy = sy + y, ty = dy + y;
        if (yy < 0 || yy >= s->h || ty < 0 || ty >= d->h) continue;
        for (int x = 0; x < w; x++) {
            int xx = sx + x, tx = dx + x;
            if (xx < 0 || xx >= s->w || tx < 0 || tx >= d->w) continue;
            d->px[(size_t)ty * (size_t)d->w + (size_t)tx] = s->px[(size_t)yy * (size_t)s->w + (size_t)xx];
        }
    }
}

static int op_copy_rect(AG *g) {
    Image *s = gfx_surface(g, arg_i(g, 1)), *d = gfx_surface(g, arg_i(g, 2));
    if (s && d) copy_rect(s, d, arg_i(g, 3), arg_i(g, 4), arg_i(g, 5), arg_i(g, 6), arg_i(g, 7), arg_i(g, 8));
    surf_drawn(g, arg_i(g, 2));
    return OP_NEXT;
}

static int op_scale_copy(AG *g) {
    Image *s = gfx_surface(g, arg_i(g, 1)), *d = gfx_surface(g, arg_i(g, 2));
    if (!s || !d) return OP_NEXT;
    i32 sx = arg_i(g, 3), sy = arg_i(g, 4), sw = arg_i(g, 5), sh = arg_i(g, 6);
    i32 dx = arg_i(g, 7), dy = arg_i(g, 8), dw = arg_i(g, 9), dh = arg_i(g, 10);
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return OP_NEXT;
    surf_drawn(g, arg_i(g, 2));
    for (i32 y = 0; y < dh; y++) {
        i32 ty = dy + y;
        if (ty < 0 || ty >= d->h) continue;
        for (i32 x = 0; x < dw; x++) {
            i32 tx = dx + x;
            if (tx < 0 || tx >= d->w) continue;
            float u = (float)sx + ((float)x + 0.5f) * (float)sw / (float)dw, v = (float)sy + ((float)y + 0.5f) * (float)sh / (float)dh;
            d->px[(size_t)ty * (size_t)d->w + (size_t)tx] = sample_bilinear(s, u, v, sx < 0 ? 0 : sx, sy < 0 ? 0 : sy, sx + sw > s->w ? s->w : sx + sw, sy + sh > s->h ? s->h : sy + sh);
        }
    }
    return OP_NEXT;
}

static int op_background(AG *g) {
    u32 c = (u32)arg_i(g, 1);
    g->gfx.background = 0xff000000u | (c >> 16 & 255) | (c & 0xff00) | (c & 255) << 16;
    return OP_NEXT;
}

static int op_anim_clock(AG *g) {
    g->gfx.animDur = (u32)arg_i(g, 1);
    g->gfx.animStarted = false;
    return OP_NEXT;
}

static int op_anim_reset(AG *g) { gfx_complete(g); return OP_NEXT; }
static int op_service_flags(AG *g) { g->gfx.serviceFlags = (u32)arg_i(g, 1); return OP_NEXT; }
static int op_sample_time(AG *g) { gfx_tick(g); return OP_NEXT; }
static int op_tile_edge(AG *g) { g->gfx.tileEdge = arg_i(g, 1); return OP_NEXT; }

static int op_surface_flags(AG *g) {
    i32 s = arg_i(g, 1), f = arg_i(g, 2);
    if (s < 0 || s >= AG_SURFACES) return OP_NEXT;
    Surface *x = &g->gfx.surf[s];
    x->flags = (u32)f;
    x->rec[2] = x->snap[2] = f & 1;
    x->rec[3] = x->snap[3] = (f >> 1) & 1;
    return OP_NEXT;
}

static int op_script_entry(AG *g) {
    for (int s = 0; s < AG_SURFACES; s++) {
        Surface *x = &g->gfx.surf[s];
        x->flags = 0;
        x->rec[2] = x->rec[3] = x->snap[2] = x->snap[3] = 0;
    }
    return OP_NEXT;
}

static int op_yield(AG *g) {
    g->wait = W_YIELD;
    return OP_WAIT;
}

static int op_tick_presentation(AG *g) { (void)g; return OP_NEXT; }

static Cmd *cmd_slot(AG *g, i32 key) {
    Gfx *x = &g->gfx;
    Cmd *free_ = NULL;
    for (int i = 0; i < AG_CMDS; i++) {
        if (x->cmd[i].on && x->cmd[i].key == key) return &x->cmd[i];
        if (!x->cmd[i].on && !free_) free_ = &x->cmd[i];
    }
    return free_ ? free_ : &x->cmd[0];
}

static int queue_cmd(AG *g, int type, i32 key, i32 target, i32 a0, i32 an, i32 b0, i32 bn, i32 delay, i32 dur) {
    Cmd *c = cmd_slot(g, key);
    memset(c, 0, sizeof *c);
    c->on = true;
    c->type = type;
    c->key = key;
    c->target = target;
    c->a0 = a0;
    c->an = an;
    c->b0 = b0;
    c->bn = bn;
    c->delay = delay;
    c->dur = dur;
    return OP_NEXT;
}

static int op_cmd_alpha(AG *g) {
    return queue_cmd(g, 0, arg_i(g, 1), arg_i(g, 2), arg_i(g, 3), arg_i(g, 4), arg_i(g, 5), arg_i(g, 6), arg_i(g, 7), arg_i(g, 8));
}

static int op_cmd_pattern(AG *g) {
    return queue_cmd(g, 2, arg_i(g, 1), arg_i(g, 2), arg_i(g, 3), arg_i(g, 4), arg_i(g, 5), arg_i(g, 6), arg_i(g, 9), arg_i(g, 10));
}

static int op_cmd_dirblur(AG *g) {
    return queue_cmd(g, 3, arg_i(g, 1), arg_i(g, 2), arg_i(g, 3), arg_i(g, 4), 0, 0, arg_i(g, 9), arg_i(g, 10));
}

static int op_cmd_radblur(AG *g) {
    return queue_cmd(g, 3, arg_i(g, 1), arg_i(g, 2), arg_i(g, 3), arg_i(g, 4), 0, 0, arg_i(g, 11), arg_i(g, 12));
}

static int op_cmd_clear(AG *g) {
    for (int i = 0; i < AG_CMDS; i++) g->gfx.cmd[i].on = false;
    return OP_NEXT;
}
static int op_nop(AG *g) { (void)g; return OP_NEXT; }

enum { TR_NONE, TR_CROSS, TR_TO_BLACK, TR_TO_WHITE, TR_FROM_BLACK, TR_STRIPS, TR_STAGGER };

static Image *snapshot(AG *g, i32 slot) {
    Image *s = slot >= 0 ? gfx_surface(g, slot) : NULL;
    Image *im = image_new(g->width, g->height);
    if (s) {
        for (int y = 0; y < g->height && y < s->h; y++)
            for (int x = 0; x < g->width && x < s->w; x++) im->px[(size_t)y * (size_t)g->width + (size_t)x] = s->px[(size_t)y * (size_t)s->w + (size_t)x] | 0xff000000u;
    } else {
        int w, h;
        vnage_pixels(g, &w, &h);
        memcpy(im->px, g->screen, sizeof(u32) * (size_t)g->width * (size_t)g->height);
    }
    return im;
}

static int begin(AG *g, int kind, Image *a, Image *b, i32 arg) {
    Gfx *x = &g->gfx;
    image_free(x->trA);
    image_free(x->trB);
    x->trKind = kind;
    x->trA = a;
    x->trB = b;
    x->trStart = g->now;
    if (arg <= 64) { x->trStep = 16; x->trInterval = arg; }
    else { x->trStep = 1; x->trInterval = arg / 16; }
    if (x->trInterval < 1) x->trInterval = 1;
    if (g->in.skip) { gfx_transition_finish(g); return OP_NEXT; }
    g->wait = W_TRANS;
    return OP_WAIT;
}

static i32 tr_total(Gfx *x) {
    if (x->trKind == TR_STRIPS || x->trKind == TR_STAGGER) {
        bool vert = x->trMode == 4 || x->trMode == 5 || x->trMode == 8 || x->trMode == 9;
        i32 thick = x->trDiv > 0 ? ((vert ? 1024 : 576) / x->trDiv) : 1;
        i32 steps = x->trKind == TR_STAGGER ? x->trDiv - 1 + thick : thick;
        return steps * x->trInterval;
    }
    return (256 / x->trStep) * x->trInterval;
}

bool gfx_transition_done(AG *g) {
    Gfx *x = &g->gfx;
    if (x->trKind == TR_NONE) return true;
    return (i32)(g->now - x->trStart) >= tr_total(x);
}

static void tr_draw(AG *g, i32 el) {
    Gfx *x = &g->gfx;
    int W = g->width, H = g->height;
    u32 *o = g->screen;
    if (x->trKind == TR_STRIPS || x->trKind == TR_STAGGER) {
        i32 step = x->trInterval > 0 ? el / x->trInterval : 0;
        int m = x->trMode;
        bool vert = m == 4 || m == 5 || m == 8 || m == 9;
        bool rev = m == 5 || m == 7 || m == 9 || m == 11;
        int len = vert ? W : H;
        int thick = x->trDiv > 0 ? len / x->trDiv : len;
        if (thick < 1) thick = 1;
        for (int y = 0; y < H; y++)
            for (int xx = 0; xx < W; xx++) {
                int p = vert ? xx : y;
                if (rev) p = len - 1 - p;
                int strip = p / thick, within = p % thick;
                i32 shown = x->trKind == TR_STAGGER ? step - strip : step;
                size_t i = (size_t)y * (size_t)W + (size_t)xx;
                o[i] = within < shown ? x->trB->px[i] : x->trA->px[i];
            }
        return;
    }
    i32 acc = x->trInterval > 0 ? (el / x->trInterval) * x->trStep : 256;
    if (acc > 256) acc = 256;
    for (size_t i = 0; i < (size_t)W * (size_t)H; i++) {
        u32 a = x->trA->px[i], b = x->trB ? x->trB->px[i] : (x->trKind == TR_TO_WHITE ? 0xffffffffu : 0xff000000u);
        u32 r = 0;
        for (int k = 0; k < 24; k += 8) {
            int ca = (int)(a >> k & 255), cb = (int)(b >> k & 255);
            r |= (u32)(ca + (cb - ca) * acc / 256) << k;
        }
        o[i] = r | 0xff000000u;
    }
}

void gfx_transition_finish(AG *g) {
    Gfx *x = &g->gfx;
    if (x->trKind == TR_NONE) return;
    tr_draw(g, 0x3fffffff);
    image_free(x->trA);
    image_free(x->trB);
    x->trA = x->trB = NULL;
    x->trKind = TR_NONE;
    x->hold = true;
    g->frameSerial++;
}

bool gfx_transition_frame(AG *g) {
    Gfx *x = &g->gfx;
    if (x->trKind == TR_NONE) return false;
    tr_draw(g, (i32)(g->now - x->trStart));
    g->frameSerial++;
    return true;
}

static int op_crossfade(AG *g) { return begin(g, TR_CROSS, snapshot(g, arg_i(g, 1)), snapshot(g, arg_i(g, 2)), arg_i(g, 3)); }
static int op_from_black(AG *g) {
    Image *b = snapshot(g, arg_i(g, 1)), *a = image_new(g->width, g->height);
    for (int i = 0; i < g->width * g->height; i++) a->px[i] = 0xff000000u;
    return begin(g, TR_FROM_BLACK, a, b, arg_i(g, 2));
}
static int op_to_black(AG *g) { return begin(g, TR_TO_BLACK, snapshot(g, arg_i(g, 1)), NULL, arg_i(g, 2)); }
static int op_to_white(AG *g) { return begin(g, TR_TO_WHITE, snapshot(g, arg_i(g, 1)), NULL, arg_i(g, 2)); }

static int strips(AG *g, int kind, int base) {
    i32 sel = arg_i(g, 4), div = arg_i(g, 3);
    if (sel < 0 || sel > 3 || div <= 0) {
        if (div <= 0) { Image *b = snapshot(g, arg_i(g, 1)); memcpy(g->screen, b->px, sizeof(u32) * (size_t)g->width * (size_t)g->height); image_free(b); g->gfx.hold = true; }
        return OP_NEXT;
    }
    Gfx *x = &g->gfx;
    x->trMode = base + sel;
    x->trDiv = div;
    int r = begin(g, kind, snapshot(g, -1), snapshot(g, arg_i(g, 1)), 0);
    x->trStep = 1;
    x->trInterval = arg_i(g, 2) > 0 ? arg_i(g, 2) : 1;
    return r;
}

static int op_strips(AG *g) { return strips(g, TR_STRIPS, 4); }
static int op_stagger(AG *g) { return strips(g, TR_STAGGER, 8); }

void gfx_register(void) {
    vm_register_draw(0x1f6, op_clear_objects);
    vm_register_draw(0x1f7, op_erase);
    vm_register_draw(0x1f8, op_create_surface);
    vm_register_draw(0x1f9, op_load_surface);
    vm_register_draw(0x1fa, op_release_surface);
    vm_register_draw(0x1fb, op_bind);
    vm_register_draw(0x1fd, op_scale_now);
    vm_register_draw(0x1fe, op_rot_now);
    vm_register_draw(0x1ff, op_trans_now);
    vm_register_draw(0x202, op_color_anim);
    vm_register_draw(0x203, op_color);
    vm_register_draw(0x207, op_copy_rect);
    vm_register(0x208, op_texture_size);
    vm_register_draw(0x20b, op_fill_rect);
    vm_register_draw(0x20c, op_present);
    vm_register(0x20d, op_target);
    vm_register_draw(0x20e, op_clear_target);
    vm_register_draw(0x214, op_swap);
    vm_register(0x215, op_query_slot);
    vm_register(0x216, op_query_res);
    vm_register_draw(0x217, op_set_anchor);
    vm_register(0x218, op_get_anchor);
    vm_register_draw(0x219, op_set_pos);
    vm_register(0x21a, op_get_pos);
    vm_register(0x21b, op_tick_presentation);
    vm_register_draw(0x21c, op_yield);
    vm_register_draw(0x21d, op_clone);
    vm_register_draw(0x21e, op_scale_to);
    vm_register_draw(0x21f, op_rot_to);
    vm_register_draw(0x220, op_trans_to);
    vm_register_draw(0x222, op_present_range);
    vm_register_draw(0x224, op_cmd_clear);
    vm_register(0x227, op_query_rot);
    vm_register(0x228, op_query_trans);
    vm_register_draw(0x229, op_range);
    vm_register_draw(0x22a, op_range_scale_now);
    vm_register_draw(0x22c, op_range_trans_now);
    vm_register_draw(0x22d, op_range_scale_to);
    vm_register_draw(0x22e, op_range_rot_to);
    vm_register_draw(0x22f, op_range_trans_to);
    vm_register_draw(0x230, op_cyc_reset);
    vm_register_draw(0x231, op_cyc_cells);
    vm_register_draw(0x232, op_cyc_color);
    vm_register_draw(0x233, op_cyc_scale);
    vm_register_draw(0x234, op_cyc_rot);
    vm_register_draw(0x235, op_cyc_trans);
    vm_register_draw(0x238, op_anim_clock);
    vm_register_draw(0x239, op_cells);
    vm_register(0x23c, op_sample_time);
    vm_register_draw(0x23d, op_release_transient);
    vm_register_draw(0x242, op_detached);
    vm_register_draw(0x243, op_anim_reset);
    vm_register_draw(0x248, op_tile_edge);
    vm_register_draw(0x249, op_load_raw_surface);
    vm_register_draw(0x24e, op_service_flags);
    vm_register_draw(0x258, op_surface_flags);
    vm_register_draw(0x259, op_script_entry);
    vm_register_draw(0x25a, op_background);
    vm_register_draw(0x32, op_scale_copy);
    vm_register_draw(0x223, op_cmd_alpha);
    vm_register_draw(0x24f, op_cmd_pattern);
    vm_register_draw(0x250, op_cmd_dirblur);
    vm_register_draw(0x251, op_cmd_radblur);
    vm_register(0xd9, op_nop);
    vm_register(0x21, op_from_black);
    vm_register(0x22, op_to_black);
    vm_register(0x24, op_to_white);
    vm_register(0x25, op_crossfade);
    vm_register(0x27, op_strips);
    vm_register(0x28, op_stagger);
}

static u32 swap_rb(u32 c) { return (c & 0xff00ff00u) | (c >> 16 & 255) | (c & 255) << 16; }

static void put_f(u8 *r, int o, float v) { memcpy(r + o, &v, 4); }
static float get_f(const u8 *r, int o) { float v; memcpy(&v, r + o, 4); return v; }
static void put_m(u8 *r, int o, M4 m) { memcpy(r + o, m.m, 64); }
static M4 get_m(const u8 *r, int o) { M4 m; memcpy(m.m, r + o, 64); return m; }
static void put_v(u8 *r, int o, Vec3 v) { put_f(r, o, v.x); put_f(r, o + 4, v.y); put_f(r, o + 8, v.z); }
static Vec3 get_v(const u8 *r, int o) { return v3(get_f(r, o), get_f(r, o + 4), get_f(r, o + 8)); }

void gfx_obj_record(const Obj *o, u8 *r) {
    memset(r, 0, 0x2e4);
    u32 flags = o->visible ? 1 : 0;
    if (o->cColor.on || o->cScale.on || o->cRot.on || o->cTrans.on || o->cCell.on) flags |= 2;
    bool cyc = false;
    for (int k = 0; k < 5; k++) if (o->cycPeriod[k] > 0) cyc = true;
    if (cyc) flags |= 4;
    wr32(r, flags);
    wr32(r + 4, (u32)o->slot);
    wr32(r + 8, (u32)o->sl);
    wr32(r + 0xc, (u32)o->st);
    wr32(r + 0x10, (u32)o->sr);
    wr32(r + 0x14, (u32)o->sb);
    put_v(r, 0x18, o->anchor);
    put_v(r, 0x24, o->pos);
    wr32(r + 0x30, (u32)o->mode);
    wr32(r + 0x34, o->started ? o->start : 0);
    const Chan *ch[5] = { &o->cColor, &o->cScale, &o->cRot, &o->cTrans, &o->cCell };
    for (int k = 0; k < 5; k++) {
        wr32(r + 0x38 + k * 4, ch[k]->on ? (u32)ch[k]->delay : 0);
        wr32(r + 0x4c + k * 4, ch[k]->on ? (u32)ch[k]->dur : 0);
    }
    wr32(r + 0x60, swap_rb(o->color));
    wr32(r + 0x64, o->cColor.on ? swap_rb(o->colorTo) : 0xffffffffu);
    put_m(r, 0x6c, m_scale(o->scale));
    put_m(r, 0xac, m_scale(o->cScale.on ? o->scaleTo : o->scale));
    put_m(r, 0xec, m_rot(o->rotAxis, o->rotAngle));
    put_m(r, 0x12c, m_rot(o->cRot.on ? o->rotAxisTo : o->rotAxis, o->cRot.on ? o->rotTo : o->rotAngle));
    put_m(r, 0x16c, m_trans(o->trans.x, o->trans.y, o->trans.z));
    put_m(r, 0x1ac, m_trans(o->transTo.x, o->transTo.y, o->transTo.z));
    put_v(r, 0x1ec, o->rotAxis);
    put_v(r, 0x1f8, o->rotAxisTo);
    put_f(r, 0x204, o->rotAngle);
    put_f(r, 0x208, o->cRot.on ? o->rotTo : o->rotAngle);
    for (int k = 0; k < 5; k++) {
        wr32(r + 0x20c + k * 4, o->cycStarted[k] ? o->cycStart[k] : 0);
        wr32(r + 0x220 + k * 4, (u32)o->cycPeriod[k]);
    }
    wr32(r + 0x234, (u32)o->cellFlags);
    wr32(r + 0x238, (u32)(o->cCell.on || o->cellFrames ? o->cellFrames : o->loopFrames));
    wr32(r + 0x23c, (u32)(o->cCell.on || o->cellFrames ? o->cellCols : o->loopCols));
    wr32(r + 0x240, o->cycPeriod[0] > 0 ? swap_rb(o->cycColor) : 0xffffffffu);
    put_v(r, 0x244, o->cycAxis);
    put_m(r, 0x250, m_scale(o->cycPeriod[1] > 0 ? o->cycScale : v3(1, 1, 1)));
    put_m(r, 0x290, o->cycPeriod[3] > 0 ? m_trans(o->cycTrans.x, o->cycTrans.y, o->cycTrans.z) : m_ident());
    wr32(r + 0x2d0, o->detached);
    put_v(r, 0x2d4, o->rotAxis);
    put_f(r, 0x2e0, o->rotAngle * 3.14159265f / 180.0f);
}

void gfx_record_obj(const u8 *r, Obj *o, i32 handle) {
    obj_init(o, handle);
    u32 flags = rd32(r);
    o->visible = flags & 1;
    o->slot = (i32)rd32(r + 4);
    o->sl = (i32)rd32(r + 8);
    o->st = (i32)rd32(r + 0xc);
    o->sr = (i32)rd32(r + 0x10);
    o->sb = (i32)rd32(r + 0x14);
    o->anchor = get_v(r, 0x18);
    o->pos = get_v(r, 0x24);
    o->mode = (int)rd32(r + 0x30);
    o->color = swap_rb(rd32(r + 0x60));
    o->live = true;
    M4 sc = get_m(r, 0x6c), st = get_m(r, 0xac), tc = get_m(r, 0x16c), tt = get_m(r, 0x1ac);
    o->scale = v3(sc.m[0][0], sc.m[1][1], sc.m[2][2]);
    o->scaleTo = v3(st.m[0][0], st.m[1][1], st.m[2][2]);
    o->trans = v3(tc.m[3][0], tc.m[3][1], tc.m[3][2]);
    o->transTo = v3(tt.m[3][0], tt.m[3][1], tt.m[3][2]);
    o->rotAxis = get_v(r, 0x1ec);
    o->rotAxisTo = get_v(r, 0x1f8);
    o->rotAngle = get_f(r, 0x204);
    o->rotTo = get_f(r, 0x208);
    if (o->rotAxis.x == 0 && o->rotAxis.y == 0 && o->rotAxis.z == 0) o->rotAxis = v3(0, 0, 1);
    if (o->rotAxisTo.x == 0 && o->rotAxisTo.y == 0 && o->rotAxisTo.z == 0) o->rotAxisTo = o->rotAxis;
    Chan *ch[5] = { &o->cColor, &o->cScale, &o->cRot, &o->cTrans, &o->cCell };
    u32 target = rd32(r + 0x64);
    for (int k = 0; k < 5; k++) {
        ch[k]->delay = (i32)rd32(r + 0x38 + k * 4);
        ch[k]->dur = (i32)rd32(r + 0x4c + k * 4);
    }
    if (flags & 2) {
        if (target != 0xffffffffu) { o->cColor.on = true; o->colorFrom = o->color; o->colorTo = swap_rb(target); }
        if (o->scaleTo.x != o->scale.x || o->scaleTo.y != o->scale.y) { o->cScale.on = true; o->scaleFrom = o->scale; }
        if (o->rotTo != o->rotAngle) { o->cRot.on = true; o->rotFrom = o->rotAngle; }
        if (o->transTo.x != o->trans.x || o->transTo.y != o->trans.y || o->transTo.z != o->trans.z) { o->cTrans.on = true; o->transFrom = o->trans; }
        if (o->cCell.dur > 0) o->cCell.on = true;
    } else {
        o->scaleTo = o->scale;
        o->transTo = o->trans;
        o->rotTo = o->rotAngle;
    }
    for (int k = 0; k < 5; k++) o->cycPeriod[k] = (flags & 4) ? (i32)rd32(r + 0x220 + k * 4) : 0;
    o->cellFlags = (i32)rd32(r + 0x234);
    if (o->cCell.on) { o->cellFrames = (i32)rd32(r + 0x238); o->cellCols = (i32)rd32(r + 0x23c); }
    o->loopFrames = (i32)rd32(r + 0x238);
    o->loopCols = (i32)rd32(r + 0x23c);
    o->cycColor = swap_rb(rd32(r + 0x240));
    o->cycAxis = get_v(r, 0x244);
    M4 cs = get_m(r, 0x250), ct = get_m(r, 0x290);
    o->cycScale = v3(cs.m[0][0], cs.m[1][1], cs.m[2][2]);
    o->cycTrans = v3(ct.m[3][0], ct.m[3][1], ct.m[3][2]);
    o->detached = rd32(r + 0x2d0);
}

void gfx_restore_objects(AG *g, const u8 *recs, const i32 *handles, int n, i32 rfirst, i32 rcount, const u8 *range) {
    Gfx *x = &g->gfx;
    objs_clear(x);
    for (int i = 0; i < n; i++) {
        Obj *o = gfx_obj(g, handles[i], true);
        gfx_record_obj(recs + (size_t)i * 0x2e4, o, handles[i]);
    }
    x->rangeFirst = rfirst;
    x->rangeCount = rcount;
    gfx_record_obj(range, &x->range, 0);
}

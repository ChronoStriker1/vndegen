#include "th.h"

#include <math.h>

#define CACHE_MAX 40

static inline u32 over(u32 d, u32 s, int alpha) {
    u32 a = (s >> 24) * (u32)alpha / 255;
    if (a == 0) return d;
    if (a == 255) return s | 0xff000000u;
    u32 ia = 255 - a;
    u32 r = ((s & 0xff) * a + (d & 0xff) * ia) / 255;
    u32 g = (((s >> 8) & 0xff) * a + ((d >> 8) & 0xff) * ia) / 255;
    u32 b = (((s >> 16) & 0xff) * a + ((d >> 16) & 0xff) * ia) / 255;
    u32 da = d >> 24;
    u32 oa = a + da * ia / 255;
    return r | g << 8 | b << 16 | oa << 24;
}

void fill_rect(u32 *dst, int dw, int dh, int x, int y, int w, int h, u32 rgba) {
    int x0 = clampi(x, 0, dw), y0 = clampi(y, 0, dh), x1 = clampi(x + w, 0, dw), y1 = clampi(y + h, 0, dh);
    int alpha = (int)(rgba >> 24);
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) dst[(size_t)j * dw + i] = alpha == 255 ? rgba : over(dst[(size_t)j * dw + i], rgba | 0xff000000u, alpha);
}

void blit_part(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int w, int h, int dx, int dy, int alpha) {
    if (!im || alpha <= 0) return;
    int x0 = sx > im->x ? sx : im->x, y0 = sy > im->y ? sy : im->y;
    int x1 = sx + w < im->x + im->w ? sx + w : im->x + im->w, y1 = sy + h < im->y + im->h ? sy + h : im->y + im->h;
    for (int y = y0; y < y1; y++) {
        int ty = dy + (y - sy);
        if (ty < 0 || ty >= dh) continue;
        const u32 *srow = im->px + (size_t)(y - im->y) * im->w;
        u32 *drow = dst + (size_t)ty * dw;
        for (int x = x0; x < x1; x++) {
            int tx = dx + (x - sx);
            if (tx < 0 || tx >= dw) continue;
            u32 s = srow[x - im->x];
            if (im->opaque && alpha >= 255) drow[tx] = s;
            else drow[tx] = over(drow[tx], s, alpha);
        }
    }
}

void blit(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, int alpha) {
    if (!im) return;
    blit_part(dst, dw, dh, im, 0, 0, im->cw, im->ch, dx, dy, alpha);
}

void blit_scaled(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, float sx, float sy, int alpha) {
    if (!im || !im->w || !im->h || sx <= 0 || sy <= 0) return;
    if (fabsf(sx - 1) < 1e-4f && fabsf(sy - 1) < 1e-4f) {
        blit(dst, dw, dh, im, dx, dy, alpha);
        return;
    }
    float fx0 = dx + im->x * sx, fy0 = dy + im->y * sy;
    int x0 = (int)floorf(fx0), y0 = (int)floorf(fy0);
    int x1 = (int)ceilf(fx0 + im->w * sx), y1 = (int)ceilf(fy0 + im->h * sy);
    x0 = clampi(x0, 0, dw);
    y0 = clampi(y0, 0, dh);
    x1 = clampi(x1, 0, dw);
    y1 = clampi(y1, 0, dh);
    for (int y = y0; y < y1; y++) {
        float v = (y + 0.5f - dy) / sy - im->y - 0.5f;
        int vy = (int)floorf(v);
        float fy = v - vy;
        u32 *drow = dst + (size_t)y * dw;
        for (int x = x0; x < x1; x++) {
            float u = (x + 0.5f - dx) / sx - im->x - 0.5f;
            int ux = (int)floorf(u);
            float fx = u - ux;
            float acc[4] = {0, 0, 0, 0}, wsum = 0;
            for (int k = 0; k < 4; k++) {
                int px = ux + (k & 1), py = vy + (k >> 1);
                float wgt = ((k & 1) ? fx : 1 - fx) * ((k >> 1) ? fy : 1 - fy);
                if (px < 0 || py < 0 || px >= im->w || py >= im->h || wgt <= 0) continue;
                u32 s = im->px[(size_t)py * im->w + px];
                float a = (float)(s >> 24) * wgt;
                acc[0] += (s & 0xff) * a;
                acc[1] += ((s >> 8) & 0xff) * a;
                acc[2] += ((s >> 16) & 0xff) * a;
                acc[3] += a;
                wsum += wgt;
            }
            if (acc[3] <= 0) continue;
            u32 a = (u32)(acc[3] + 0.5f);
            u32 s = (u32)(acc[0] / acc[3] + 0.5f) | (u32)(acc[1] / acc[3] + 0.5f) << 8 | (u32)(acc[2] / acc[3] + 0.5f) << 16 |
                    (a > 255 ? 255u : a) << 24;
            drow[x] = over(drow[x], s, alpha);
        }
    }
}

static Image *composite(Image *base, const Image *top) {
    if (!base) return image_copy(top);
    int x0 = base->x < top->x ? base->x : top->x, y0 = base->y < top->y ? base->y : top->y;
    int x1 = base->x + base->w > top->x + top->w ? base->x + base->w : top->x + top->w;
    int y1 = base->y + base->h > top->y + top->h ? base->y + base->h : top->y + top->h;
    if (x0 != base->x || y0 != base->y || x1 != base->x + base->w || y1 != base->y + base->h) {
        Image *grown = image_new(base->cw, base->ch, x0, y0, x1 - x0, y1 - y0);
        for (int y = 0; y < base->h; y++)
            memcpy(grown->px + (size_t)(y + base->y - y0) * grown->w + (base->x - x0), base->px + (size_t)y * base->w,
                   sizeof(u32) * (size_t)base->w);
        image_free(base);
        base = grown;
    }
    for (int y = 0; y < top->h; y++) {
        u32 *d = base->px + (size_t)(y + top->y - base->y) * base->w + (top->x - base->x);
        const u32 *s = top->px + (size_t)y * top->w;
        for (int x = 0; x < top->w; x++) d[x] = over(d[x], s[x], 255);
    }
    base->opaque = base->opaque && top->x >= base->x;
    return base;
}

Image *image_load(TH *t, const char *name) {
    size_t size;
    u8 *d = store_load(&t->store, PAC_IMAGE, name, &size);
    if (!d) return NULL;
    Image *im = grd_decode(d, size);
    free(d);
    if (!im) th_log(t, "bad image %s", name);
    return im;
}

Image *compose_named(TH *t, const char *spec, bool chara) {
    const char *def = ini_get(chara ? &t->charaMacro : &t->fileMacro, chara ? "CHARAFILEMACRO" : "FILEMACRO", spec);
    if (!def && !chara) def = ini_get(&t->charaMacro, "CHARAFILEMACRO", spec);
    if (!def) def = ini_get(&t->fileMacro, "FILEMACRO", spec);
    if (!def) return image_load(t, spec);
    char *list = th_strdup(def);
    Image *out = NULL;
    for (char *p = strtok(list, "/"); p; p = strtok(NULL, "/")) {
        Image *part = image_load(t, p);
        if (!part) {
            th_log(t, "missing image %s in %s", p, spec);
            continue;
        }
        if (!out) out = part;
        else {
            out = composite(out, part);
            image_free(part);
        }
    }
    free(list);
    return out;
}

Image *image_get(TH *t, const char *name) {
    for (int i = 0; i < t->cacheCount; i++)
        if (!strcmp(t->cache[i].name, name)) {
            t->cache[i].used = ++t->cacheTick;
            return t->cache[i].img;
        }
    Image *im = compose_named(t, name, strncmp(name, "fm_C", 4) == 0);
    if (!im) {
        th_log(t, "missing image %s", name);
        return NULL;
    }
    if (t->cacheCount >= CACHE_MAX) image_cache_trim(t);
    t->cache = th_realloc(t->cache, sizeof *t->cache * (size_t)(t->cacheCount + 1));
    t->cache[t->cacheCount++] = (struct CacheEntry){th_strdup(name), im, ++t->cacheTick};
    return im;
}

static bool in_use(TH *t, Image *im) {
    if (im == t->bg || im == t->navi || im == t->pan.img) return true;
    for (int i = 0; i < t->sprCount; i++)
        if (t->spr[i].img == im) return true;
    return false;
}

void image_cache_trim(TH *t) {
    while (t->cacheCount >= CACHE_MAX / 2) {
        int oldest = -1;
        for (int i = 0; i < t->cacheCount; i++)
            if (!in_use(t, t->cache[i].img) && (oldest < 0 || t->cache[i].used < t->cache[oldest].used)) oldest = i;
        if (oldest < 0) return;
        image_free(t->cache[oldest].img);
        free(t->cache[oldest].name);
        t->cache[oldest] = t->cache[--t->cacheCount];
    }
}

void gfx_init(TH *t) {
    size_t n = (size_t)t->W * (size_t)t->H;
    t->screen = th_alloc(n * 4);
    t->sceneBuf = th_alloc(n * 4);
    t->prevBuf = th_alloc(n * 4);
    for (size_t i = 0; i < n; i++) t->screen[i] = t->sceneBuf[i] = t->prevBuf[i] = 0xff000000u;
}

void gfx_close(TH *t) {
    for (int i = 0; i < t->cacheCount; i++) {
        image_free(t->cache[i].img);
        free(t->cache[i].name);
    }
    free(t->cache);
    free(t->screen);
    free(t->sceneBuf);
    free(t->prevBuf);
}

static float sprite_offset(TH *t, int i, int n) {
    float left = (float)ini_int(&t->sys, "GAMESYSTEM", "CHARA_SPR_LEFT", -300);
    float right = (float)ini_int(&t->sys, "GAMESYSTEM", "CHARA_SPR_RIGHT", 300);
    if (n <= 1) return 0;
    if (n == 2) return i == 0 ? left : right;
    if (n == 3) return i == 0 ? 0 : i == 1 ? left : right;
    return left + (right - left) * (float)i / (float)(n - 1);
}

static void render_scene(TH *t) {
    size_t n = (size_t)t->W * (size_t)t->H;
    u32 *d = t->sceneBuf;
    for (size_t i = 0; i < n; i++) d[i] = 0xff000000u;
    if (!strcmp(t->bgName, "wh")) {
        for (size_t i = 0; i < n; i++) d[i] = 0xffffffffu;
    } else if (t->bg) {
        blit(d, t->W, t->H, t->bg, (t->W - t->bg->cw) / 2, (t->H - t->bg->ch) / 2, 255);
    }
    if (t->pan.active && t->pan.img) {
        u32 el = t->now - t->pan.start;
        float k = t->pan.dur ? (el >= t->pan.dur ? 1.0f : (float)el / (float)t->pan.dur) : 1.0f;
        int y = t->pan.fromY + (int)lroundf((float)(t->pan.toY - t->pan.fromY) * k);
        blit(d, t->W, t->H, t->pan.img, 0, y, 255);
    }
    int plus = ini_int(&t->sys, "GAMESYSTEM", "CHARA_SPR_PLUS_Y", 0);
    for (int i = 0; i < t->sprCount; i++) {
        Sprite *s = &t->spr[i];
        if (!s->img) continue;
        float cx = t->W / 2.0f + s->x;
        float cy = s->y + (float)plus + s->img->ch / 2.0f;
        float x0 = cx - s->img->cw * s->sx / 2.0f, y0 = cy - s->img->ch * s->sy / 2.0f;
        blit_scaled(d, t->W, t->H, s->img, (int)lroundf(x0), (int)lroundf(y0), s->sx, s->sy, 255);
    }
    if (t->navi) {
        int st[4] = {0};
        ini_ints(&t->sys, "MESSAGEWINDOW", "ECSTACY_NAVI_STAT", st, 4);
        blit(d, t->W, t->H, t->navi, st[0], st[1], 255);
    }
}

void gfx_begin_transition(TH *t, int kind) {
    memcpy(t->prevBuf, t->screen, sizeof(u32) * (size_t)t->W * (size_t)t->H);
    t->transActive = kind != 0;
    t->transKind = kind;
    t->transStart = t->now;
    t->transDur = kind == 0 ? 0 : (u32)(kind * 250 > 3000 ? 3000 : kind * 250);
    if (kind == 1) t->transDur = 500;
    t->sceneDirty = true;
}

bool gfx_transition_done(TH *t) { return !t->transActive || t->now - t->transStart >= t->transDur; }

void gfx_finish_transition(TH *t) {
    t->transActive = false;
    t->dirty = true;
}

void gfx_set_bg(TH *t, const char *name, int trans) {
    gfx_begin_transition(t, trans);
    snprintf(t->bgName, sizeof t->bgName, "%s", name);
    t->bg = NULL;
    t->sprCount = 0;
    if (strcmp(name, "bl") && strcmp(name, "wh")) t->bg = image_get(t, name);
    t->sceneDirty = true;
}

void gfx_clear_sprites(TH *t, int trans) {
    gfx_begin_transition(t, trans);
    t->sprCount = 0;
    t->sceneDirty = true;
}

void gfx_set_sprites(TH *t, const char *list, int trans) {
    gfx_begin_transition(t, trans);
    t->sprCount = 0;
    char *copy = th_strdup(list);
    char *save = NULL;
    for (char *item = strtok_r(copy, ",", &save); item && t->sprCount < TH_MAX_SPRITES; item = strtok_r(NULL, ",", &save)) {
        while (*item == ' ') item++;
        if (!*item) continue;
        char *f[4] = {item, NULL, NULL, NULL};
        int k = 1;
        for (char *p = item; *p && k < 4; p++)
            if (*p == '/') {
                *p = 0;
                f[k++] = p + 1;
            }
        Sprite *s = &t->spr[t->sprCount++];
        snprintf(s->name, sizeof s->name, "%s", f[0]);
        s->y = f[1] ? (float)atof(f[1]) : 0;
        s->sx = f[2] ? (float)atof(f[2]) : 1.0f;
        s->sy = f[3] ? (float)atof(f[3]) : s->sx;
        s->img = image_get(t, f[0]);
    }
    free(copy);
    for (int i = 0; i < t->sprCount; i++) {
        t->spr[i].x = sprite_offset(t, i, t->sprCount);
        t->spr[i].layer = TH_MAX_SPRITES - t->sprCount + i;
    }
    t->sceneDirty = true;
}

void gfx_compose(TH *t) {
    if (t->sceneDirty || t->pan.active) {
        render_scene(t);
        t->sceneDirty = false;
    }
    size_t n = (size_t)t->W * (size_t)t->H;
    if (t->transActive && t->transDur) {
        u32 el = t->now - t->transStart;
        int k = el >= t->transDur ? 256 : (int)(el * 256 / t->transDur);
        for (size_t i = 0; i < n; i++) {
            u32 a = t->prevBuf[i], b = t->sceneBuf[i];
            u32 r = ((a & 0xff) * (256 - k) + (b & 0xff) * k) >> 8;
            u32 g = (((a >> 8) & 0xff) * (256 - k) + ((b >> 8) & 0xff) * k) >> 8;
            u32 bl = (((a >> 16) & 0xff) * (256 - k) + ((b >> 16) & 0xff) * k) >> 8;
            t->screen[i] = r | g << 8 | bl << 16 | 0xff000000u;
        }
        if (el >= t->transDur) t->transActive = false;
    } else {
        memcpy(t->screen, t->sceneBuf, n * 4);
    }
    if (t->flashDur && t->now - t->flashStart < t->flashDur) {
        u32 el = t->now - t->flashStart;
        int a = (int)(255 - el * 255 / t->flashDur);
        fill_rect(t->screen, t->W, t->H, 0, 0, t->W, t->H, (t->flashColor & 0xffffff) | (u32)a << 24);
    }
    if (!t->transActive) {
        msg_draw(t);
        buttons_draw(t);
        ui_bar_draw(t);
        ui_draw(t);
    }
    t->serial++;
}

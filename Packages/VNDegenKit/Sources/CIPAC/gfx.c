#include "ipac.h"

static inline u32 over(u32 d, u32 s, int alpha) {
    u32 a = (s >> 24) * (u32)alpha / 255;
    if (a == 0) return d;
    if (a == 255) return s | 0xff000000u;
    u32 ia = 255 - a;
    u32 r = ((s & 0xff) * a + (d & 0xff) * ia) / 255;
    u32 g = (((s >> 8) & 0xff) * a + ((d >> 8) & 0xff) * ia) / 255;
    u32 b = (((s >> 16) & 0xff) * a + ((d >> 16) & 0xff) * ia) / 255;
    return r | g << 8 | b << 16 | 0xff000000u;
}

static inline u32 mix(u32 a, u32 b, u32 k) {
    u32 ik = 256 - k;
    u32 r = ((a & 0xff) * ik + (b & 0xff) * k) >> 8;
    u32 g = (((a >> 8) & 0xff) * ik + ((b >> 8) & 0xff) * k) >> 8;
    u32 bl = (((a >> 16) & 0xff) * ik + ((b >> 16) & 0xff) * k) >> 8;
    return r | g << 8 | bl << 16 | 0xff000000u;
}

void fill_rect(u32 *dst, int dw, int dh, int x, int y, int w, int h, u32 rgba) {
    int x0 = clampi(x, 0, dw), y0 = clampi(y, 0, dh), x1 = clampi(x + w, 0, dw), y1 = clampi(y + h, 0, dh);
    int alpha = (int)(rgba >> 24);
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) dst[(size_t)j * dw + i] = alpha == 255 ? rgba : over(dst[(size_t)j * dw + i], rgba | 0xff000000u, alpha);
}

void blit_part(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int w, int h, int dx, int dy, int alpha) {
    if (!im || alpha <= 0) return;
    int x0 = sx < 0 ? 0 : sx, y0 = sy < 0 ? 0 : sy;
    int x1 = sx + w < im->w ? sx + w : im->w, y1 = sy + h < im->h ? sy + h : im->h;
    if (dx + (x0 - sx) < 0) x0 = sx - dx;
    if (dy + (y0 - sy) < 0) y0 = sy - dy;
    if (dx + (x1 - sx) > dw) x1 = sx - dx + dw;
    if (dy + (y1 - sy) > dh) y1 = sy - dy + dh;
    for (int y = y0; y < y1; y++) {
        const u32 *srow = im->px + (size_t)y * im->w;
        u32 *drow = dst + (size_t)(dy + y - sy) * dw + (dx - sx);
        if (im->opaque && alpha >= 255) memcpy(drow + x0, srow + x0, sizeof(u32) * (size_t)(x1 - x0));
        else
            for (int x = x0; x < x1; x++) drow[x] = over(drow[x], srow[x], alpha);
    }
}

void blit(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, int alpha) {
    if (!im) return;
    blit_part(dst, dw, dh, im, 0, 0, im->w, im->h, dx, dy, alpha);
}

static int sprite_x(int pos) { return pos == SP_L ? -200 : pos == SP_R ? 200 : 0; }

static void render_scene(IP *t) {
    size_t n = (size_t)t->W * (size_t)t->H;
    for (size_t i = 0; i < n; i++) t->scene[i] = 0xff000000u;
    if (t->bg) blit(t->scene, t->W, t->H, t->bg, -t->bgX, -t->bgY, 255);
    for (int i = 0; i < SP_COUNT; i++)
        if (t->spr[i].img) blit(t->scene, t->W, t->H, t->spr[i].img, sprite_x(i), 0, 255);
}

void gfx_init(IP *t) {
    size_t n = (size_t)t->W * (size_t)t->H;
    t->screen = ip_alloc(sizeof(u32) * n);
    t->scene = ip_alloc(sizeof(u32) * n);
    t->prev = ip_alloc(sizeof(u32) * n);
    for (size_t i = 0; i < n; i++) t->scene[i] = t->prev[i] = t->screen[i] = 0xff000000u;
    t->speed = 4;
}

void gfx_close(IP *t) {
    free(t->screen);
    free(t->scene);
    free(t->prev);
    free(t->shakeBuf);
    for (int i = 0; i < t->cacheCount; i++) {
        image_free(t->cache[i].img);
        free(t->cache[i].name);
    }
    free(t->cache);
}

static void snapshot(IP *t) {
    if (t->snapped) return;
    if (t->transActive) gfx_finish_transition(t);
    memcpy(t->prev, t->scene, sizeof(u32) * (size_t)t->W * (size_t)t->H);
    t->snapped = true;
}

void gfx_set_bg(IP *t, const char *name) {
    snapshot(t);
    snprintf(t->bgName, sizeof t->bgName, "%s", name);
    t->bg = image_get(t, PK_BG, name);
    for (int i = 0; i < SP_COUNT; i++) {
        t->spr[i].img = NULL;
        t->spr[i].name[0] = 0;
    }
}

void gfx_set_sprite(IP *t, int pos, const char *name) {
    snapshot(t);
    Sprite *s = &t->spr[pos];
    if (!name) {
        s->img = NULL;
        s->name[0] = 0;
        return;
    }
    snprintf(s->name, sizeof s->name, "%s", name);
    s->img = image_get(t, PK_CHR, name);
}

void gfx_begin_transition(IP *t) {
    t->snapped = false;
    render_scene(t);
    t->rule = t->ruleName[0] ? image_get(t, PK_PARTS, t->ruleName) : NULL;
    t->transStart = t->now;
    t->transDur = (u32)(256 * 1000 / (t->speed * 60));
    t->transActive = t->transDur > 0;
    t->dirty = true;
}

bool gfx_transition_done(IP *t) { return !t->transActive || t->now - t->transStart >= t->transDur; }

void gfx_finish_transition(IP *t) {
    t->transActive = false;
    t->dirty = true;
}

u32 gfx_click_at(IP *t, int x, int y) {
    Image *m = t->clickMap;
    if (!m || x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    u32 p = m->px[(size_t)y * m->w + x];
    return (p & 0xff) << 16 | (p & 0xff00) | (p >> 16 & 0xff);
}

void gfx_update(IP *t) {
    for (int i = 0; i < IP_OBJS; i++) {
        Obj *o = &t->obj[i];
        if (!o->img) continue;
        if (o->mx || o->my || o->malpha) {
            o->x += o->mx;
            o->y += o->my;
            o->alpha = clampi(o->alpha + o->malpha, 0, 255);
            t->dirty = true;
        }
    }
    if (t->transActive) t->dirty = true;
    if (t->shakeDur && t->now - t->shakeStart < t->shakeDur) t->dirty = true;
}

static void draw_transition(IP *t) {
    u32 el = t->now - t->transStart;
    int n = t->W * t->H;
    if (el >= t->transDur) {
        t->transActive = false;
        memcpy(t->screen, t->scene, sizeof(u32) * (size_t)n);
        return;
    }
    int level = (int)(el * 256 / t->transDur);
    Image *r = t->rule;
    if (r && r->w == t->W && r->h == t->H) {
        for (int i = 0; i < n; i++) {
            int v = (int)(r->px[i] & 0xff);
            int k = clampi((level * 2 - v) * 8, 0, 256);
            t->screen[i] = mix(t->prev[i], t->scene[i], (u32)k);
        }
    } else {
        for (int i = 0; i < n; i++) t->screen[i] = mix(t->prev[i], t->scene[i], (u32)level);
    }
}

void gfx_compose(IP *t) {
    if (t->transActive) draw_transition(t);
    else memcpy(t->screen, t->scene, sizeof(u32) * (size_t)t->W * (size_t)t->H);
    for (int i = 0; i < IP_OBJS; i++) {
        Obj *o = &t->obj[i];
        if (o->img && o->vis) blit(t->screen, t->W, t->H, o->img, o->x, o->y, o->alpha);
    }
    msg_draw(t);
    ui_draw(t);
    if (t->shakeDur && t->now - t->shakeStart < t->shakeDur) {
        u32 el = t->now - t->shakeStart;
        int dx = (el / 33) % 2 ? t->shakeX : -t->shakeX, dy = (el / 33) % 2 ? -t->shakeY : t->shakeY;
        if (!t->shakeBuf) t->shakeBuf = ip_alloc(sizeof(u32) * (size_t)t->W * (size_t)t->H);
        u32 *tmp = t->shakeBuf;
        memcpy(tmp, t->screen, sizeof(u32) * (size_t)t->W * (size_t)t->H);
        for (int y = 0; y < t->H; y++)
            for (int x = 0; x < t->W; x++) {
                int sx = clampi(x - dx, 0, t->W - 1), sy = clampi(y - dy, 0, t->H - 1);
                t->screen[(size_t)y * t->W + x] = tmp[(size_t)sy * t->W + sx];
            }
    }
    t->serial++;
}

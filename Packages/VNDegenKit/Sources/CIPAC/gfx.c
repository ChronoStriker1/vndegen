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

#define MOVE_MS 533

int gfx_slot_x(int slot) { return slot == SP_L ? -200 : slot == SP_R ? 200 : 0; }

static void render_scene(IP *t) {
    size_t n = (size_t)t->W * (size_t)t->H;
    for (size_t i = 0; i < n; i++) t->scene[i] = 0xff000000u;
    if (t->bg) blit(t->scene, t->W, t->H, t->bg, -t->bgX, -t->bgY, 255);
    for (int i = 0; i < SP_COUNT; i++)
        if (t->spr[i].img) blit(t->scene, t->W, t->H, t->spr[i].img, t->spr[i].x, 0, 255);
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
    image_free(t->work);
    image_free(t->workBg);
    image_free(t->viewer);
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
}

void gfx_apply_now(IP *t) {
    t->snapped = false;
    render_scene(t);
    t->dirty = true;
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
    s->x = gfx_slot_x(pos);
    s->moving = false;
}

bool gfx_move_sprite(IP *t, int from, int toward) {
    if (from == toward || !t->spr[from].img) return false;
    int to = toward > from ? from + 1 : from - 1;
    if (t->transActive) gfx_finish_transition(t);
    Sprite moved = t->spr[from];
    memset(&t->spr[from], 0, sizeof t->spr[from]);
    if (t->spr[to].img) ip_log(t, "sprite move into an occupied slot %d", to);
    moved.fromX = moved.x;
    moved.toX = gfx_slot_x(to);
    moved.moveStart = t->now;
    moved.moving = true;
    t->spr[to] = moved;
    return true;
}

bool gfx_moving(IP *t) {
    for (int i = 0; i < SP_COUNT; i++)
        if (t->spr[i].moving) return true;
    return false;
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
    bool moved = false;
    for (int i = 0; i < SP_COUNT; i++) {
        Sprite *sp = &t->spr[i];
        if (!sp->moving) continue;
        u32 el = t->now - sp->moveStart;
        if (el >= MOVE_MS) {
            sp->x = sp->toX;
            sp->moving = false;
        } else sp->x = sp->fromX + (sp->toX - sp->fromX) * (int)el / MOVE_MS;
        moved = true;
    }
    if (moved && !t->transActive) {
        render_scene(t);
        t->dirty = true;
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
    choice_draw(t);
    if (t->viewer && t->layerMode) blit(t->screen, t->W, t->H, t->viewer, 0, 0, 255);
    ui_draw(t);
    if (t->shakeDur && t->now - t->shakeStart < t->shakeDur) {
        u32 el = t->now - t->shakeStart;
        int ph = (int)(el / 33) % 2, dx = ph ? t->shakeX : -t->shakeX, dy = ph ? -t->shakeY : t->shakeY;
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

bool anim_pos(IP *t, const Obj *o, int *x, int *y) {
    (void)t;
    int k = o->frame;
    if (k < 0 || k >= o->animFrames) return false;
    *x = o->x + o->animX[k];
    *y = o->y + o->animY[k];
    return true;
}

bool frame_rect(Image *im, int frames, int k, int *x, int *y, int *w, int *h) {
    if (!im || k < 0) return false;
    if (frames > 0 && im->h >= im->w * 2) {
        int fh = im->h / frames;
        if (k >= frames || fh <= 0) return false;
        *x = 0;
        *y = k * fh;
        *w = im->w;
        *h = fh;
        return true;
    }
    int found = 0;
    for (int y0 = 0; y0 < im->h;) {
        bool any = false;
        for (int i = 0; i < im->w && !any; i++) any = im->px[(size_t)y0 * im->w + i] >> 24;
        if (!any) {
            y0++;
            continue;
        }
        int y1 = y0;
        for (;;) {
            bool row = false;
            if (y1 < im->h)
                for (int i = 0; i < im->w && !row; i++) row = im->px[(size_t)y1 * im->w + i] >> 24;
            if (!row) break;
            y1++;
        }
        for (int x0 = 0; x0 < im->w;) {
            bool col = false;
            for (int j = y0; j < y1 && !col; j++) col = im->px[(size_t)j * im->w + x0] >> 24;
            if (!col) {
                x0++;
                continue;
            }
            int x1 = x0;
            for (;;) {
                bool c = false;
                if (x1 < im->w)
                    for (int j = y0; j < y1 && !c; j++) c = im->px[(size_t)j * im->w + x1] >> 24;
                if (!c) break;
                x1++;
            }
            if (found++ == k) {
                *x = x0;
                *y = y0;
                *w = x1 - x0;
                *h = y1 - y0;
                return true;
            }
            x0 = x1;
        }
        y0 = y1;
    }
    return false;
}

static bool viewer_load(IP *t) {
    char name[128];
    int i = t->viewerIndex;
    if (i == 0) snprintf(name, sizeof name, "%s", t->viewerBase);
    else if (i <= 20) snprintf(name, sizeof name, "%s_%d", t->viewerBase, i);
    else snprintf(name, sizeof name, "%s%c", t->viewerBase, 'a' + (i - 21));
    if (!store_has(&t->store, PK_BG, name, ".IES")) return false;
    image_free(t->viewer);
    t->viewer = image_load(t, PK_BG, name);
    t->dirty = true;
    return t->viewer != NULL;
}

void viewer_open(IP *t, int page, int slot) {
    for (int i = 0; i < t->vlistCount; i++)
        if (t->vlist[i].page == page && t->vlist[i].slot == slot) {
            snprintf(t->viewerBase, sizeof t->viewerBase, "%s", t->vlist[i].img);
            t->viewerIndex = 0;
            viewer_load(t);
            return;
        }
    ip_log(t, "BG_VISUAL %d %d is not in the CG list", page, slot);
}

void viewer_next(IP *t) {
    if (!t->viewer) return;
    for (int tries = 0; tries < 26; tries++) {
        t->viewerIndex = (t->viewerIndex + 1) % 26;
        if (viewer_load(t)) return;
    }
}

#include "sg.h"

#include <math.h>

static const u32 kPictureFadeMs = 500;
static const u32 kPlain = 0xffffffffu;

static SgaPicture *picture(SG *g, int n) { return n >= 0 && n < SGA_PICTURES ? &g->interp.a.pics[n] : NULL; }

static u32 lerp_rgba(u32 a, u32 b, float t) {
    if (a == b) return a;
    if (a == kPlain) a = 0x7f7f7f7fu;
    if (b == kPlain) b = 0x7f7f7f7fu;
    u32 out = 0;
    for (int s = 0; s < 32; s += 8) {
        float x = (float)(a >> s & 255), y = (float)(b >> s & 255);
        out |= (u32)(x + (y - x) * t + 0.5f) << s;
    }
    return out;
}

static void show(SG *g, SgaPicture *p, Guid id, float zoom, u32 color, float x, float y) {
    *p = (SgaPicture){0};
    p->im = sg_image(g, id);
    p->used = true;
    p->kept = true;
    p->id = id;
    p->zoom = p->zoomFrom = p->zoomTo = zoom;
    p->color = p->colorFrom = p->colorTo = color;
    p->x = p->xFrom = p->xTo = x;
    p->y = p->yFrom = p->yTo = y;
    p->alpha = p->alphaFrom = p->alphaTo = 1.0f;
}

void sgi_picture_cmd(SG *g, const Cmd *c) {
    int n = cmd_int(c, 0);
    SgaPicture *p = picture(g, n);
    if (!p) return;
    if (c->type == CMD_SHOW_PICTURE) {
        show(g, p, cmd_guid(c, 1), (float)cmd_int(c, 2) / 100.0f, (u32)cmd_int(c, 3), (float)cmd_int(c, 5), (float)cmd_int(c, 6));
        if (cmd_int(c, 8)) {
            p->alpha = p->alphaFrom = 0.0f;
            p->total = kPictureFadeMs;
        }
        sg_log(g, "picture %d shown, zoom %d colour %08x%s", n, cmd_int(c, 2), (u32)cmd_int(c, 3), p->total ? ", fading in" : "");
    } else if (c->type == CMD_ERASE_PICTURE) {
        if (!p->kept) return;
        if (cmd_int(c, 1) && p->used) {
            p->alphaFrom = p->alpha;
            p->alphaTo = 0;
            p->zoomFrom = p->zoomTo = p->zoom;
            p->xFrom = p->xTo = p->x;
            p->yFrom = p->yTo = p->y;
            p->colorFrom = p->colorTo = p->color;
            p->ms = 0;
            p->total = kPictureFadeMs;
            p->eraseAfter = true;
        } else {
            p->used = false;
            p->total = 0;
        }
        sg_log(g, "picture %d erased", n);
    } else if (c->type == CMD_MOVE_PICTURE) {
        if (!p->used) return;
        p->zoomFrom = p->zoom;
        p->zoomTo = (float)cmd_int(c, 1) / 100.0f;
        p->colorFrom = p->color;
        p->colorTo = c->argCount > 4 ? (u32)cmd_int(c, 4) : p->color;
        p->xFrom = p->x;
        p->yFrom = p->y;
        p->xTo = c->argCount > 6 ? (float)cmd_int(c, 6) : p->x;
        p->yTo = c->argCount > 7 ? (float)cmd_int(c, 7) : p->y;
        p->alphaFrom = p->alphaTo = p->alpha;
        p->ms = 0;
        p->total = (u32)cmd_int(c, 2);
        if (!p->total) {
            p->zoom = p->zoomTo;
            p->color = p->colorTo;
            p->x = p->xTo;
            p->y = p->yTo;
        }
        sg_log(g, "picture %d to zoom %d at %d,%d over %d ms", n, cmd_int(c, 1), (int)p->xTo, (int)p->yTo, cmd_int(c, 2));
    }
}

void sgi_pictures_frame(SG *g, u32 ms) {
    for (int i = 0; i < SGA_PICTURES; i++) {
        SgaPicture *p = &g->interp.a.pics[i];
        if (!p->used || !p->total) continue;
        p->ms += ms;
        float t = p->ms >= p->total ? 1.0f : (float)p->ms / (float)p->total;
        p->alpha = p->alphaFrom + (p->alphaTo - p->alphaFrom) * t;
        p->zoom = p->zoomFrom + (p->zoomTo - p->zoomFrom) * t;
        p->x = p->xFrom + (p->xTo - p->xFrom) * t;
        p->y = p->yFrom + (p->yTo - p->yFrom) * t;
        p->color = t >= 1.0f ? p->colorTo : lerp_rgba(p->colorFrom, p->colorTo, t);
        if (t >= 1.0f) {
            p->total = 0;
            if (p->eraseAfter) {
                p->used = false;
                p->eraseAfter = false;
                p->color = 0;
            }
        }
    }
}

static void draw_one(SG *g, const SgaPicture *p) {
    const Image *im = p->im;
    float scale[4] = {1, 1, 1, 1};
    if (p->color != kPlain)
        for (int k = 0; k < 4; k++) scale[k] = (float)(p->color >> (8 * k) & 255) / 127.5f;
    float alpha = p->alpha * scale[3];
    if (alpha > 1.0f) alpha = 1.0f;
    if (alpha <= 0.0f || p->zoom <= 0.0f) return;
    int dw = (int)lroundf((float)im->w * p->zoom), dh = (int)lroundf((float)im->h * p->zoom);
    if (dw <= 0 || dh <= 0) return;
    int dx = (int)lroundf((float)g->w * 0.5f + p->x - (float)dw * 0.5f), dy = (int)lroundf((float)g->h * 0.5f + p->y - (float)dh * 0.5f);
    bool tinted = p->color != kPlain;
    u32 a255 = (u32)(alpha * 255.0f + 0.5f);
    for (int y = 0; y < dh; y++) {
        int ty = dy + y;
        if (ty < 0 || ty >= g->h) continue;
        const u32 *row = im->px + (size_t)(y * im->h / dh) * (size_t)im->w;
        u32 *out = g->fb + (size_t)ty * (size_t)g->w;
        for (int x = 0; x < dw; x++) {
            int tx = dx + x;
            if (tx < 0 || tx >= g->w) continue;
            u32 s = row[x * im->w / dw];
            u32 sa = (s >> 24) * a255 / 255;
            if (!sa) continue;
            u32 c[3];
            for (int k = 0; k < 3; k++) {
                float v = (float)(s >> (8 * k) & 255);
                if (tinted) v *= scale[k];
                c[k] = v >= 255.0f ? 255u : (u32)v;
            }
            u32 d = out[tx], o = 0xff000000u;
            for (int k = 0; k < 3; k++) {
                u32 dv = d >> (8 * k) & 255;
                o |= ((c[k] * sa + dv * (255 - sa)) / 255) << (8 * k);
            }
            out[tx] = o;
        }
    }
}

void sgi_pictures_draw(SG *g) {
    for (int i = 0; i < SGA_PICTURES; i++) {
        const SgaPicture *p = &g->interp.a.pics[i];
        if (p->used && p->im) draw_one(g, p);
    }
}

static void put32(u8 *b, i32 v) {
    b[0] = (u8)v;
    b[1] = (u8)(v >> 8);
    b[2] = (u8)(v >> 16);
    b[3] = (u8)(v >> 24);
}

static u32 bswap(u32 v) { return v >> 24 | (v >> 8 & 0xff00u) | (v << 8 & 0xff0000u) | v << 24; }

static i32 get32(const u8 *b) { return (i32)((u32)b[0] | (u32)b[1] << 8 | (u32)b[2] << 16 | (u32)b[3] << 24); }

enum { PIC_BODY = 46 };

void sgi_pictures_save(SG *g, SaveData *s) {
    for (int i = 0; i < s->pictureCount; i++) free(s->pictures[i].body);
    free(s->pictures);
    s->pictures = sg_alloc(sizeof(SavePicture) * SGA_PICTURES);
    s->pictureCount = 0;
    for (int i = 0; i < SGA_PICTURES; i++) {
        const SgaPicture *p = &g->interp.a.pics[i];
        if (!p->kept) continue;
        u8 *b = sg_alloc(PIC_BODY);
        put32(b, i);
        memcpy(b + 8, p->id.b, 16);
        bool fading = p->total && p->eraseAfter;
        put32(b + 24, (i32)lroundf((p->total ? p->zoomTo : p->zoom) * 100.0f));
        put32(b + 28, (i32)bswap(fading ? 0 : p->total ? p->colorTo : p->color));
        put32(b + 33, (i32)(p->total ? p->xTo : p->x));
        put32(b + 37, (i32)(p->total ? p->yTo : p->y));
        b[41] = p->used && !fading;
        put32(b + 42, -1);
        s->pictures[s->pictureCount++] = (SavePicture){b, PIC_BODY};
    }
}

void sgi_pictures_load(SG *g, const SaveData *s) {
    SgaState *a = &g->interp.a;
    memset(a->pics, 0, sizeof a->pics);
    for (int i = 0; i < s->pictureCount; i++) {
        const SavePicture *sp = &s->pictures[i];
        if (sp->size < PIC_BODY) continue;
        const u8 *b = sp->body;
        SgaPicture *p = picture(g, get32(b));
        if (!p) continue;
        Guid id;
        memcpy(id.b, b + 8, 16);
        show(g, p, id, (float)get32(b + 24) / 100.0f, bswap((u32)get32(b + 28)), (float)get32(b + 33), (float)get32(b + 37));
        if (!b[41]) p->used = false;
        sg_log(g, "picture %d restored%s", get32(b), p->used ? "" : " hidden");
    }
}

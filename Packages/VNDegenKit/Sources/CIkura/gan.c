#include "ikura.h"

#define GAN_W 800
#define GAN_H 600
#define GAN_UNIT_MS 40
#define GAN_LAYER 5
#define GAN_PROGRAM 0x10
#define GAN_INDEX 0x2010

typedef struct GanFrame {
    int id, ref;
    u32 off, size;
    u8 *px;
} GanFrame;

struct Gan {
    char name[16];
    u8 *data;
    size_t size;
    GanFrame *frames;
    int frameCount;
    const u8 *prog;
    int progLen, pc, start;
    int cur, shown;
    bool playing, stopAtEnd, paused;
    u32 nextAt, pausedLeft, startedAt;
};

typedef struct Src {
    const u8 *p, *end;
} Src;

static int src_byte(Src *s) { return s->p < s->end ? *s->p++ : -1; }

static int src_int(Src *s) {
    int v = src_byte(s);
    if (v < 0) return -1;
    if (v > 0x7f) {
        int lo = src_byte(s);
        if (lo < 0) return -1;
        v = (v & 0x7f) << 8 | lo;
        if (v == 0x7fff) {
            if (s->end - s->p < 4) return -1;
            v = (int)rd32(s->p);
            s->p += 4;
        }
    }
    return v;
}

static bool src_colour(Src *s, u8 *out) {
    if (s->end - s->p < 3) return false;
    memcpy(out, s->p, 3);
    s->p += 3;
    return true;
}

static void unpack_key(Src *in, u8 *out, size_t len) {
    int last = -1;
    size_t dst = 0;
    while (dst + 3 <= len) {
        if (!src_colour(in, out + dst)) break;
        int colour = out[dst] | out[dst + 1] << 8 | out[dst + 2] << 16;
        dst += 3;
        if (colour == last) {
            int count = src_int(in);
            if (count < 0) break;
            if (count > 2) {
                size_t n = (size_t)(count - 2) * 3;
                if (n > len - dst) n = len - dst;
                for (size_t i = 0; i < n; i++) out[dst + i] = out[dst + i - 3];
                dst += n;
            }
        }
        last = colour;
    }
}

static void unpack_ref(Src *in, u8 *out, size_t len) {
    if (in->end - in->p < 4) return;
    u32 head = rd32(in->p);
    if (head < 4 || head > (u32)(in->end - in->p)) return;
    Src chunks = {in->p + 4, in->p + head};
    in->p += head;
    int count = 0, last = -1;
    size_t dst = 0;
    while (dst < len) {
        int run = src_int(&chunks);
        if (run < 0) break;
        while (run-- > 0 && dst + 3 <= len) {
            if (!count) {
                if (!src_colour(in, out + dst)) return;
                int colour = out[dst] | out[dst + 1] << 8 | out[dst + 2] << 16;
                dst += 3;
                if (colour == last) {
                    count = src_int(in);
                    if (count < 0) return;
                    count = count > 2 ? count - 2 : 0;
                }
                last = colour;
            } else {
                count--;
                out[dst++] = (u8)last;
                out[dst++] = (u8)(last >> 8);
                out[dst++] = (u8)(last >> 16);
            }
        }
        int skip = src_int(&chunks);
        if (skip < 0) break;
        size_t n = (size_t)skip * 3;
        dst += n < len - dst ? n : len - dst;
    }
}

static GanFrame *frame_by_id(Gan *g, int id) {
    for (int i = 0; i < g->frameCount; i++)
        if (g->frames[i].id == id) return &g->frames[i];
    return NULL;
}

static const u8 *frame_pixels(Gan *g, GanFrame *f, int depth) {
    if (f->px) return f->px;
    if (depth > 64 || (size_t)f->off + f->size > g->size) return NULL;
    size_t len = (size_t)GAN_W * GAN_H * 3;
    u8 *px = ik_alloc(len);
    Src in = {g->data + f->off, g->data + f->off + f->size};
    GanFrame *ref = f->ref ? frame_by_id(g, f->ref) : NULL;
    const u8 *base = ref && ref != f ? frame_pixels(g, ref, depth + 1) : NULL;
    if (base) {
        memcpy(px, base, len);
        unpack_ref(&in, px, len);
    } else unpack_key(&in, px, len);
    f->px = px;
    return px;
}

void gan_free(IK *t) {
    Gan *g = t->gan;
    if (!g) return;
    if (g->shown > 1 && t->now > g->startedAt)
        ik_log(t, "animation %s: %d frames in %u ms, %.2f a second", g->name, g->shown, t->now - g->startedAt,
               (double)g->shown * 1000.0 / (double)(t->now - g->startedAt));
    for (int i = 0; i < g->frameCount; i++) free(g->frames[i].px);
    free(g->frames);
    free(g->data);
    free(g);
    t->gan = NULL;
    t->dirty = true;
}

static void gan_load(IK *t, const char *name) {
    gan_free(t);
    char file[32];
    snprintf(file, sizeof file, "%s%s", name, strchr(name, '.') ? "" : ".GAN");
    size_t n;
    u8 *d = store_load(&t->store, AR_GRP, file, &n);
    if (!d || n < GAN_INDEX || memcmp(d, "GANM0100", 8)) {
        ik_log(t, "can't load animation %s", file);
        free(d);
        return;
    }
    Gan *g = ik_alloc(sizeof *g);
    snprintf(g->name, sizeof g->name, "%s", store_name(&t->store, AR_GRP, file));
    g->data = d;
    g->size = n;
    g->progLen = (int)rd32(d + 8);
    if (g->progLen < 0 || g->progLen > (GAN_INDEX - GAN_PROGRAM) / 8) g->progLen = 0;
    g->prog = d + GAN_PROGRAM;
    int count = (int)rd32(d + 12);
    if (count < 0 || (size_t)GAN_INDEX + (size_t)count * 16 > n) count = 0;
    g->frameCount = count;
    g->frames = ik_alloc(sizeof(GanFrame) * (size_t)(count ? count : 1));
    for (int i = 0; i < count; i++) {
        const u8 *e = d + GAN_INDEX + 16 * i;
        g->frames[i] = (GanFrame){(int)rd32(e), (int)rd32(e + 4), rd32(e + 8), rd32(e + 12), NULL};
    }
    g->cur = count ? g->frames[0].id : 0;
    t->gan = g;
    t->dirty = true;
}

static void run_program(IK *t, Gan *g) {
    for (int guard = 0; guard < g->progLen + 1 && g->playing; guard++) {
        if (g->pc < 0 || g->pc >= g->progLen) {
            g->playing = false;
            return;
        }
        int op = (int)rd32(g->prog + 8 * g->pc), arg = (int)rd32(g->prog + 8 * g->pc + 4);
        g->pc++;
        if (op == 1) {
            if (arg != g->cur) g->shown++;
            g->cur = arg;
            t->dirty = true;
        } else if (op == 2) {
            u32 due = g->nextAt + (u32)(arg > 0 ? arg : 1) * GAN_UNIT_MS;
            g->nextAt = (i32)(t->now - due) > 250 ? t->now : due;
            return;
        } else if (op == 5) {
            if (g->stopAtEnd) {
                g->playing = false;
                return;
            }
            g->pc = arg;
        }
    }
}

void gan_update(IK *t) {
    Gan *g = t->gan;
    if (!g || !g->playing || g->paused) return;
    if ((i32)(t->now - g->nextAt) >= 0) run_program(t, g);
}

void gan_draw(IK *t, int layer) {
    Gan *g = t->gan;
    if (!g || layer != GAN_LAYER || layer >= t->layerCount) return;
    Layer *l = &t->layers[layer];
    GanFrame *f = frame_by_id(g, g->cur);
    const u8 *px = f ? frame_pixels(g, f, 0) : NULL;
    if (!px) return;
    u32 a = (u32)clampi(l->argb[0], 0, 255);
    if (!a) return;
    int w = l->w < GAN_W ? l->w : GAN_W, h = l->h < GAN_H ? l->h : GAN_H;
    for (int y = 0; y < h; y++) {
        int dy = l->y + y;
        if (dy < 0 || dy >= t->H) continue;
        const u8 *row = px + (size_t)y * GAN_W * 3;
        u32 *out = t->screen + (size_t)dy * t->W;
        for (int x = 0; x < w; x++) {
            int dx = l->x + x;
            if (dx < 0 || dx >= t->W) continue;
            const u8 *p = row + x * 3;
            u32 r = p[2], gg = p[1], b = p[0];
            if (a < 255) {
                u32 d = out[dx], ia = 255 - a;
                r = (r * a + (d & 0xff) * ia) / 255;
                gg = (gg * a + (d >> 8 & 0xff) * ia) / 255;
                b = (b * a + (d >> 16 & 0xff) * ia) / 255;
            }
            out[dx] = r | gg << 8 | b << 16 | 0xff000000u;
        }
    }
}

bool gan_drawn(const IK *t, int layer) { return t->gan && layer == GAN_LAYER; }

bool gan_exec(IK *t, u16 op, const u8 *a, int n) {
    Gan *g = t->gan;
    switch (op) {
    case 0x0d00: {
        char name[16] = {0};
        for (int k = 0; k < 12 && k < n && a[k]; k++) name[k] = (char)a[k];
        gan_load(t, name);
        break;
    }
    case 0x0d01:
        if (!g) break;
        g->start = (int)ik_value(t, rd32(a));
        g->pc = 0;
        g->playing = true;
        g->stopAtEnd = false;
        g->paused = false;
        g->nextAt = t->now;
        g->startedAt = t->now;
        g->shown = 0;
        run_program(t, g);
        break;
    case 0x0d02:
        if (!g) break;
        if (ik_value(t, rd32(a)) == 2) g->stopAtEnd = true;
        else gan_free(t);
        break;
    case 0x0d03: {
        u32 v = rd32(a);
        if (v < (u32)t->varCount) t->vars[v] = g && g->playing ? -1 : 0;
        break;
    }
    case 0x0d04:
        if (!g || n < 4) break;
        if (ik_value(t, rd32(a))) {
            if (!g->paused) g->pausedLeft = g->nextAt - t->now;
            g->paused = true;
        } else if (g->paused) {
            g->paused = false;
            g->nextAt = t->now + ((i32)g->pausedLeft > 0 ? g->pausedLeft : 0);
        }
        break;
    case 0x0d05:
        t->wait = W_TIME;
        t->waitUntil = t->now + 1;
        break;
    default: return false;
    }
    return true;
}

void gan_save(IK *t, u8 out[32]) {
    memset(out, 0, 32);
    Gan *g = t->gan;
    if (!g) return;
    memcpy(out, g->name, strnlen(g->name, 12));
    out[14] = g->playing ? 1 : 0;
    out[15] = 1;
    wr32(out + 16, (u32)g->start);
    wr32(out + 20, (u32)g->pc);
    wr32(out + 24, g->stopAtEnd ? 2u : 0u);
    wr32(out + 28, 1);
}

void gan_restore(IK *t, const u8 in[32]) {
    gan_free(t);
    char name[16] = {0};
    memcpy(name, in, 12);
    if (!name[0]) return;
    gan_load(t, name);
    Gan *g = t->gan;
    if (!g) return;
    g->start = (int)rd32(in + 16);
    g->stopAtEnd = rd32(in + 24) == 2;
    if (in[14]) {
        g->playing = true;
        g->pc = 0;
        g->nextAt = t->now;
        run_program(t, g);
    }
}

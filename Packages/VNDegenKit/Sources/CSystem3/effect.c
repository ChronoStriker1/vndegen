// CE and CD: copies that appear step by step. Each effect's pattern and step count were read from System39.exe under
// Wine (docs/system3-cleanroom.md). A step only decides which pixels of the rectangle have arrived; the picture at the
// end is the plain copy, so the VM's state doesn't depend on the effect.
#include "s3.h"

typedef struct S3Effect {
    int dx, dy, w, h;
    int number, steps, perTime, timeMs, shown;
    int key;           // -1: every pixel is copied
    u64 start;
    u8 *src8;          // the source as it was when the effect started
    u32 *src32;
} S3Effect;

static int ceil_div(int a, int b) { return (a + b - 1) / b; }

// How many steps an effect has, how many of them fit in the option's time, and its default time in milliseconds.
// False for the effects no library game uses. The line and zoom effects move a pixel at a time, so their steps are
// short: CE's 8 pixels for each option time are 8 steps here.
static bool effect_params(int number, int w, int h, int *steps, int *perTime, int *ms) {
    int side = (w < h ? w : h) / 2;
    *perTime = 1;
    switch (number) {
    case 3: *steps = (h + 1) / 2; *ms = 3; return true;
    case 4: *steps = (w + 1) / 2; *ms = 3; return true;
    case 6: *steps = 8; *ms = 30; return true;
    case 7: case 8: *steps = ceil_div(side, 16); *ms = 40; return true;
    case 9: *steps = 4; *ms = 80; return true;
    case 10: *steps = w; *perTime = ceil_div(w, 64); *ms = 5; return true;
    case 11: *steps = ceil_div(h, 16) + 15; *ms = 40; return true;
    case 12: case 13: *steps = w; *perTime = 8; *ms = 20; return true;
    case 14: case 15: *steps = h; *perTime = 8; *ms = 20; return true;
    case 16: *steps = w; *perTime = ceil_div(w, 8); *ms = 30; return true;
    case 17: *steps = h; *perTime = ceil_div(h, 16); *ms = 30; return true;
    case 18: case 19: *steps = (w + 1) / 2; *perTime = 8; *ms = 30; return true;
    case 20: case 21: *steps = (h + 1) / 2; *perTime = 8; *ms = 30; return true;
    case 22: case 23: *steps = 4; *ms = 80; return true;
    default: return false;
    }
}

// The order the cells of a 2 by 2 tile arrive in effects 9 and 23.
static int tile_step(int x, int y) { return x ? (y ? 2 : 4) : (y ? 3 : 1); }

// Whether pixel x, y of the rectangle has arrived by the given step, counted from 1.
static bool arrived(const S3Effect *e, int x, int y, int step) {
    if (step >= e->steps) return true;
    int w = e->w, h = e->h;
    int edge = x < w - 1 - x ? x : w - 1 - x;
    int edgeY = y < h - 1 - y ? y : h - 1 - y;
    int ring = edge < edgeY ? edge : edgeY;
    switch (e->number) {
    case 3: return (y % 2 ? (h - 1 - y) / 2 : y / 2) < step;
    case 4: return (x % 2 ? (w - 1 - x) / 2 : x / 2) < step;
    case 6: {
        int bx = x % 64, by = y % 64;
        bx = bx < 63 - bx ? bx : 63 - bx;
        by = by < 63 - by ? by : 63 - by;
        return (bx < by ? bx : by) / 4 < step;
    }
    case 7: return ring / 16 < step;
    case 8: return ring >= (e->steps - step) * 16;
    case 9: return tile_step((x >> 2) & 1, (y >> 2) & 1) <= step;
    case 11: return y % 16 < step - y / 16;
    case 12: return x < step;
    case 13: return w - 1 - x < step;
    case 14: return y < step;
    case 15: return h - 1 - y < step;
    case 16: {
        // Column 8 of each 16 first, then the columns on either side of it, each round left to right.
        int d = x % 16 - 8, tile = x / 16, tiles = ceil_div(w, 16);
        if (d == 0) return tile < step;
        int round = d < 0 ? -d : d;
        return tiles + (round - 1) * 2 * tiles + tile * 2 + (d > 0) < step;
    }
    case 17: return y % 16 * ceil_div(h, 16) + y / 16 < step;
    case 18: return edge >= e->steps - step;
    case 19: return edge < step;
    case 20: return edgeY >= e->steps - step;
    case 21: return edgeY < step;
    case 22: return step >= 2 && ((x >> 1) & 1) == ((y >> 1) & 1);
    case 23: return tile_step((x >> 1) & 1, (y >> 1) & 1) <= step;
    default: return true;
    }
}

static void put_pixel(S3 *s, const S3Effect *e, int x, int y, size_t from, u32 keyColor) {
    size_t at = (size_t)(e->dy + y) * s->dib.w + (size_t)(e->dx + x);
    if (e->src8) { if ((int)e->src8[from] != e->key) s->dib.p8[at] = e->src8[from]; }
    else if (e->key < 0 || e->src32[from] != keyColor) s->dib.p32[at] = e->src32[from];
}

// Effect 10 grows the whole source from the centre of the rectangle.
static void draw_zoom(S3 *s, const S3Effect *e, int step) {
    int rw = e->w * step / e->steps, rh = e->h * step / e->steps;
    if (rw < 1 || rh < 1) return;
    int ox = (e->w - rw) / 2, oy = (e->h - rh) / 2;
    for (int y = 0; y < rh; y++)
        for (int x = 0; x < rw; x++)
            put_pixel(s, e, ox + x, oy + y, (size_t)(y * e->h / rh) * e->w + (size_t)(x * e->w / rw), 0);
}

static void draw_step(S3 *s, const S3Effect *e, int step) {
    if (e->number == 10) { draw_zoom(s, e, step); return; }
    u32 keyColor = e->src32 && e->key >= 0 ? s3_rgb(s, e->key) : 0;
    for (int y = 0; y < e->h; y++)
        for (int x = 0; x < e->w; x++)
            if (arrived(e, x, y, step)) put_pixel(s, e, x, y, (size_t)y * e->w + x, keyColor);
}

// Starts a CE or CD: ms is the time a step takes, 0 for the effect's default, and key the transparent colour or -1.
// False if the rectangle is empty or no library game uses the effect, so the caller shows the result at once.
bool s3_effect_start(S3 *s, int sx, int sy, int w, int h, int dx, int dy, int number, int ms, int key) {
    s3_effect_free(s);
    int steps = 0, perTime = 1, defaultMs = 0;
    if (!s3_clip_copy(s, &sx, &sy, &w, &h, &dx, &dy) || !effect_params(number, w, h, &steps, &perTime, &defaultMs)) return false;
    if (steps < 1) steps = 1; // a rectangle one pixel wide or high still has to arrive
    S3Effect *e = s3_alloc(sizeof *e);
    *e = (S3Effect){ dx, dy, w, h, number, steps, perTime, ms > 0 ? ms : defaultMs, 0, key, s->clockMs, NULL, NULL };
    for (int j = 0; j < h; j++) {
        if (s->dib.depth == 8) {
            if (!e->src8) e->src8 = s3_alloc((size_t)w * h);
            memcpy(e->src8 + (size_t)j * w, s->dib.p8 + (size_t)(sy + j) * s->dib.w + sx, (size_t)w);
        } else {
            if (!e->src32) e->src32 = s3_alloc((size_t)w * h * 4);
            memcpy(e->src32 + (size_t)j * w, s->dib.p32 + (size_t)(sy + j) * s->dib.w + sx, (size_t)w * 4);
        }
    }
    s->effect = e;
    return true;
}

// Draws what has arrived by now, or everything if finish. Returns true when the effect has ended.
bool s3_effect_update(S3 *s, bool finish) {
    S3Effect *e = s->effect;
    if (!e) return true;
    int step = (int)((s->clockMs - e->start) * (u64)e->perTime / (u64)e->timeMs);
    if (finish || step >= e->steps) step = e->steps;
    if (step > e->shown) {
        draw_step(s, e, step);
        e->shown = step;
        s3_mark_dirty(s);
    }
    if (step < e->steps) return false;
    s3_effect_free(s);
    return true;
}

void s3_effect_free(S3 *s) {
    S3Effect *e = s->effect;
    if (!e) return;
    free(e->src8);
    free(e->src32);
    free(e);
    s->effect = NULL;
}

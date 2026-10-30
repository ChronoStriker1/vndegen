// VA: moving, animated sprites drawn into the DIB. VA is in neither SDK manual. Its argument counts come from the
// SDK compiler; what each subcommand does comes from how the games call it and from System39.exe under Wine,
// recording a moving sprite's state over time (docs/system3-cleanroom.md).
//   VA0 n,0,0   hide          VA0 n,0,1   show at the current position     VA0 n,1,steps   move to the target
//   VA1 n,x,y   position      VA2 n,x,y   target                            VA3 n,w,h       size
//   VA4 n,frames,delay        frames in the strip, and the time per step in hundredths of a second
//   VA5 n,x,y   the strip's top left in the DIB, frames side by side
//   VA6 n,x,y   where the DIB keeps what the sprite covers
//   VA7 n,flag,colour         the transparent palette index
//   VA10 n,v,w  v = 1 while moving, w = steps taken      VA11 n,x,y   the current position
// Step k of a move puts the sprite at start + (target - start) * k / steps, truncated toward zero; the move ends after
// `steps` steps, leaving the sprite where step steps - 1 put it.
#include "vm.h"

#define SPRITES 32

typedef struct Sprite {
    int x, y, tx, ty, sx, sy;   // position, target, start of the current move
    int w, h, frames, delay;
    int stripX, stripY, saveX, saveY, key;
    int steps, step;
    bool shown, moving, saved;
    int drawnX, drawnY;
    u64 nextStep;
} Sprite;

typedef struct S3Sprites { Sprite sp[SPRITES]; } S3Sprites;

static Sprite *get(S3 *s, int n) {
    if (n < 0 || n >= SPRITES) return NULL;
    if (!s->sprites) s->sprites = s3_alloc(sizeof(S3Sprites));
    return &s->sprites->sp[n];
}

static void restore(S3 *s, Sprite *p) {
    if (!p->saved) return;
    s3_copy(s, p->saveX, p->saveY, p->w, p->h, p->drawnX, p->drawnY);
    p->saved = false;
}

static void draw(S3 *s, Sprite *p) {
    restore(s, p);
    if (p->w <= 0 || p->h <= 0) return;
    s3_copy(s, p->x, p->y, p->w, p->h, p->saveX, p->saveY);
    p->drawnX = p->x;
    p->drawnY = p->y;
    p->saved = true;
    int frame = p->frames > 0 ? p->step % p->frames : 0;
    s3_copy_key(s, p->stripX + frame * p->w, p->stripY, p->w, p->h, p->x, p->y, p->key);
}

void s3_sprite_command(S3 *s, int sub, int n, u16 a, u16 b, const S3LV *va, const S3LV *vb) {
    Sprite *p = get(s, n);
    if (!p) return;
    switch (sub) {
    case 0:
        if (a == 1) {
            p->sx = p->x; p->sy = p->y;
            p->steps = b; p->step = 0;
            p->moving = b > 0;
            p->shown = true;
            p->nextStep = s->clockMs + (u64)(p->delay > 0 ? p->delay : 1) * 10;
            draw(s, p);
        } else if (b == 1) {
            p->moving = false;
            p->shown = true;
            draw(s, p);
        } else {
            p->moving = false;
            p->shown = false;
            restore(s, p);
        }
        return;
    case 1: p->x = (int16_t)a; p->y = (int16_t)b; return;
    case 2: p->tx = (int16_t)a; p->ty = (int16_t)b; return;
    case 3: p->w = a; p->h = b; return;
    case 4: p->frames = a; p->delay = b; return;
    case 5: p->stripX = a; p->stripY = b; return;
    case 6: p->saveX = a; p->saveY = b; return;
    case 7: p->key = b; return;
    case 10:
        if (va) s3_lv_set(s, va, 0, p->moving);
        if (vb) s3_lv_set(s, vb, 0, (u16)p->step);
        s->polls++;
        return;
    case 11:
        if (va) s3_lv_set(s, va, 0, (u16)p->x);
        if (vb) s3_lv_set(s, vb, 0, (u16)p->y);
        s->polls++;
        return;
    }
}

// Moves the sprites whose step time has come.
void s3_sprite_update(S3 *s) {
    if (!s->sprites) return;
    for (int n = 0; n < SPRITES; n++) {
        Sprite *p = &s->sprites->sp[n];
        if (!p->moving) continue;
        u64 period = (u64)(p->delay > 0 ? p->delay : 1) * 10;
        bool moved = false;
        while (p->moving && s->clockMs >= p->nextStep) {
            p->nextStep += period;
            p->step++;
            if (p->step >= p->steps) { p->moving = false; break; }
            p->x = p->sx + (p->tx - p->sx) * p->step / p->steps;
            p->y = p->sy + (p->ty - p->sy) * p->step / p->steps;
            moved = true;
        }
        if (moved && p->shown) draw(s, p);
    }
}

void s3_sprite_free(S3 *s) {
    free(s->sprites);
    s->sprites = NULL;
}

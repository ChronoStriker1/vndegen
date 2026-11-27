#include "sg.h"

#include <math.h>

static const float kStepCells[7] = {1.33f, 2.0f, 2.8f, 4.0f, 5.5f, 7.5f, 10.0f};
static const u32 kPauseMs[7] = {1850, 1580, 1320, 1050, 700, 350, 0};

u32 sgi_rand(SG *g, u32 n) {
    u32 x = g->fi.rng ? g->fi.rng : 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->fi.rng = x;
    return n ? x % n : x;
}

static int clamp_index(i32 v) { return v < -3 ? 0 : v > 3 ? 6 : v + 3; }

float sgi_step_speed(i32 speed) { return kStepCells[clamp_index(speed)]; }

u32 sgi_step_pause(i32 freq) { return kPauseMs[clamp_index(freq)]; }

static bool on_stairs(const Map *m, int x, int z) {
    for (int i = 0; i < m->stairCount; i++)
        if (m->stairs[i].x == x && m->stairs[i].z == z) return true;
    return false;
}

static bool side_of_stair(const Map *m, int x, int z, int dx) {
    for (int i = 0; i < m->stairCount; i++)
        if (m->stairs[i].x == x && m->stairs[i].z == z && m->stairs[i].kind >= 0 && m->stairs[i].kind <= 3)
            return (m->stairs[i].kind >= 2) != (dx != 0);
    return false;
}

static bool occupies(const Actor *a, int x, int z) {
    if ((int)floorf(a->x) == x && (int)floorf(a->z) == z) return true;
    return a->moving && (int)floorf(a->tx) == x && (int)floorf(a->tz) == z;
}

bool sgi_cell_free(SG *g, int fromX, int fromZ, int x, int z, int self) {
    Field *f = &g->field;
    const Map *m = &f->map;
    if (x < 0 || z < 0 || x >= m->w || z >= m->h) return false;
    const Actor *me = self >= 0 && self < f->npcCount ? &f->npcs[self] : &f->player;
    int h0 = sg_floor_level(g, fromX, fromZ, (int)lroundf(me->y)), h1 = sg_floor_level(g, x, z, h0);
    if (h0 != h1 && (abs(h1 - h0) > 1 || !(on_stairs(m, x, z) || on_stairs(m, fromX, fromZ)))) return false;
    if (side_of_stair(m, x, z, x - fromX) || side_of_stair(m, fromX, fromZ, x - fromX)) return false;
    if (self >= 0 && occupies(&f->player, x, z)) return false;
    for (int i = 0; i < f->npcCount; i++)
        if (i != self && f->npcs[i].visible && occupies(&f->npcs[i], x, z)) return false;
    return true;
}

static const int kDx[4] = {0, 1, 0, -1}, kDz[4] = {-1, 0, 1, 0};

static int chase_dir(SG *g, const Actor *a, int *alt) {
    const Actor *p = &g->field.player;
    int dx = (int)floorf(p->x) - (int)floorf(a->x), dz = (int)floorf(p->z) - (int)floorf(a->z);
    int hx = dx > 0 ? 1 : dx < 0 ? 3 : -1, hz = dz > 0 ? 2 : dz < 0 ? 0 : -1;
    if (abs(dx) >= abs(dz)) {
        *alt = hz;
        return hx >= 0 ? hx : hz;
    }
    *alt = hx;
    return hz;
}

static void set_motion(SG *g, SgaEvent *ev, Actor *a, const char *motion) {
    if (!a->model || ev->graphicOverride) return;
    if (ev->motion && strcmp(ev->motion, motion) == 0) return;
    free(ev->motion);
    ev->motion = sg_strdup(motion);
    sg_actor_motion(g, a, motion);
}

static bool touches_player(SG *g, int npc, int x, int z) {
    const Actor *p = &g->field.player;
    return (int)floorf(p->x) == x && (int)floorf(p->z) == z && g->interp.a.ev[npc].sheets[g->interp.a.ev[npc].sheet].trigger == TRIG_EVENT_TOUCH;
}

void sgi_wander(SG *g, u32 ms) {
    SgaState *st = &g->interp.a;
    Field *f = &g->field;
    if (!f->loaded || st->mapPending || g->pendingTransfer) return;
    for (int i = 0; i < st->evCount && i < f->npcCount; i++) {
        SgaEvent *ev = &st->ev[i];
        Actor *a = &f->npcs[i];
        if (ev->sheet < 0 || ev->erased || ev->moveLeft > 0) continue;
        const SgaSheet *sh = &ev->sheets[ev->sheet];
        if (sh->moveType != MOVE_RANDOM && sh->moveType != MOVE_CHASE) continue;
        if (g->interp.active && st->eventIndex == i) continue;
        if (!ev->wanderStarted) {
            ev->wanderStarted = true;
            ev->wanderWait = sgi_rand(g, sgi_step_pause(sh->moveFreq) + 1);
            set_motion(g, ev, a, "wait");
        }
        if (ev->wanderLeft > 0) {
            sga_move_actor(g, a, &ev->wanderDir, &ev->wanderLeft, sgi_step_speed(sh->moveSpeed), false, ms);
            if (!ev->wanderLeft) {
                ev->wanderWait = sgi_step_pause(sh->moveFreq);
                set_motion(g, ev, a, "wait");
            }
            continue;
        }
        if (ev->wanderWait > ms) {
            ev->wanderWait -= ms;
            continue;
        }
        ev->wanderWait = 0;
        int cx = (int)floorf(a->x), cz = (int)floorf(a->z);
        int alt = -1, dir = sh->moveType == MOVE_CHASE ? chase_dir(g, a, &alt) : (int)sgi_rand(g, 4);
        int tries[2] = {dir, alt}, chosen = -1;
        for (int k = 0; k < 2 && chosen < 0; k++) {
            int d = tries[k];
            if (d < 0) continue;
            int nx = cx + kDx[d], nz = cz + kDz[d];
            if (!g->interp.active && touches_player(g, i, nx, nz)) {
                a->dir = d;
                sga_start_event(g, i, ev->sheet);
                chosen = -2;
                break;
            }
            if (sgi_cell_free(g, cx, cz, nx, nz, i)) chosen = d;
        }
        if (chosen == -2) return;
        if (chosen < 0) {
            if (dir >= 0 && !sh->fixDir) a->dir = dir;
            ev->wanderWait = sgi_step_pause(sh->moveFreq);
            continue;
        }
        ev->wanderDir = chosen;
        ev->wanderLeft = 1;
        a->moving = false;
        set_motion(g, ev, a, "walk");
        sga_move_actor(g, a, &ev->wanderDir, &ev->wanderLeft, sgi_step_speed(sh->moveSpeed), false, ms);
    }
}

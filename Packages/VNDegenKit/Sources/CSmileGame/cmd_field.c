#include "sg.h"

#include <math.h>

static const u32 kSpinMs = 240;

static void spin_step(Actor *a, u32 *left, int base, u32 ms) {
    if (!*left) return;
    *left = *left > ms ? *left - ms : 0;
    a->dir = *left ? (base + 1 + (int)((kSpinMs - *left) * 4 / kSpinMs)) & 3 : base;
}

void sgi_spins(SG *g, u32 ms) {
    spin_step(&g->field.player, &g->fi.playerSpinMs, g->fi.playerSpinBase, ms);
    SgaState *st = &g->interp.a;
    for (int i = 0; i < st->evCount && i < g->field.npcCount; i++) spin_step(&g->field.npcs[i], &st->ev[i].spinMs, st->ev[i].spinBase, ms);
}

static const int kDx[4] = {0, 1, 0, -1}, kDz[4] = {-1, 0, 1, 0};

static int toward(const Actor *from, const Actor *to) {
    float dx = floorf(to->x) - floorf(from->x), dz = floorf(to->z) - floorf(from->z);
    if (fabsf(dx) >= fabsf(dz) && dx != 0) return dx > 0 ? 1 : 3;
    if (dz != 0) return dz > 0 ? 2 : 0;
    return from->dir;
}

int sgi_turn(SG *g, int code, const Actor *self, const Actor *other) {
    switch (code) {
    case 0:
    case 1:
    case 2:
    case 3: return sga_dir_from_code(code);
    case 4: return (int)sgi_rand(g, 4);
    case 5: return other ? toward(self, other) : self->dir;
    case 6: return other ? (toward(self, other) + 2) & 3 : self->dir;
    case 7: return self->dir;
    default: return self->dir;
    }
}

static Actor *running_actor(SG *g) {
    int i = g->interp.a.eventIndex;
    return i >= 0 && i < g->field.npcCount ? &g->field.npcs[i] : NULL;
}

bool sgi_move_one(SG *g, const Cmd *c) {
    SgaState *a = &g->interp.a;
    int i = a->eventIndex;
    if (i < 0 || i >= a->evCount) return false;
    Actor *ac = &g->field.npcs[i];
    if (cmd_int(c, 0) == 7) return false;
    int dir = sgi_turn(g, cmd_int(c, 0), ac, &g->field.player);
    ac->dir = dir;
    int cx = (int)floorf(ac->x), cz = (int)floorf(ac->z);
    if (!sgi_cell_free(g, cx, cz, cx + kDx[dir], cz + kDz[dir], i)) {
        sg_log(g, "move event %d code %d blocked", i, cmd_int(c, 0));
        return false;
    }
    a->ev[i].moveDir = dir;
    a->ev[i].moveLeft = 1;
    a->ev[i].moveSpeed = 4.0f;
    ac->moving = false;
    a->waitMoves = true;
    sg_log(g, "move event %d code %d dir %d", i, cmd_int(c, 0), dir);
    return true;
}

static int find_event(SG *g, Guid id) {
    if (guid_zero(id)) return g->interp.a.eventIndex;
    for (int i = 0; i < g->field.npcCount; i++)
        if (guid_eq(g->field.npcs[i].eventId, id)) return i;
    return -1;
}

static void place_event(SG *g, const Cmd *c) {
    int i = find_event(g, cmd_guid(c, 2));
    if (i < 0 || i >= g->field.npcCount) return;
    Actor *ac = &g->field.npcs[i];
    int x = cmd_int(c, 0), z = cmd_int(c, 1), code = cmd_int(c, 3);
    ac->x = ac->tx = (float)x + 0.5f;
    ac->z = ac->tz = (float)z + 0.5f;
    ac->y = (float)sga_floor(g, x, z);
    ac->moving = false;
    if (i < g->interp.a.evCount) {
        g->interp.a.ev[i].moveLeft = 0;
        g->interp.a.ev[i].wanderLeft = 0;
    }
    if (code >= 1 && code <= 4) {
        static const int dirs[5] = {0, 0, 2, 3, 1};
        ac->dir = dirs[code];
    }
    sg_log(g, "event %d placed at %d,%d code %d", i, x, z, code);
}

static void random_variable(SG *g, const Cmd *c) {
    i32 index = cmd_int(c, 1), lo = cmd_int(c, 3), hi = cmd_int(c, 4);
    if (index < 0 || index >= 1024) return;
    if (hi < lo) {
        i32 t = hi;
        hi = lo;
        lo = t;
    }
    g->vars[index] = lo + (i32)sgi_rand(g, (u32)(hi - lo + 1));
    g->interp.a.dirty = true;
    sg_log(g, "variable %d = %d (random %d..%d)", index, g->vars[index], lo, hi);
}

bool sgi_encounters_on(SG *g) { return g->e.save.config[1] != 0; }

bool sgi_exec(SG *g, const Cmd *c, bool *busy) {
    *busy = false;
    switch (c->type) {
    case 15: place_event(g, c); return true;
    case 66:
        g->e.save.config[1] = cmd_int(c, 0) == 0;
        sg_log(g, "encounters %s", cmd_int(c, 0) == 0 ? "on" : "off");
        return true;
    case 70: random_variable(g, c); return true;
    default: return false;
    }
}

void sgi_turn_command(SG *g, const Cmd *c, bool player) {
    int code = cmd_int(c, 0);
    Actor *ev = running_actor(g);
    if (player) {
        Actor *p = &g->field.player;
        if (g->fi.playerSpinMs) p->dir = g->fi.playerSpinBase;
        p->dir = sgi_turn(g, code, p, ev);
        if (code == 7) {
            g->fi.playerSpinBase = p->dir;
            g->fi.playerSpinMs = kSpinMs;
        }
    } else if (ev && g->interp.a.eventIndex < g->interp.a.evCount) {
        SgaEvent *se = &g->interp.a.ev[g->interp.a.eventIndex];
        if (se->spinMs) ev->dir = se->spinBase;
        ev->dir = sgi_turn(g, code, ev, &g->field.player);
        if (code == 7) {
            se->spinBase = ev->dir;
            se->spinMs = kSpinMs;
        }
    }
    sg_log(g, "turn %s code %d", player ? "player" : "event", code);
}

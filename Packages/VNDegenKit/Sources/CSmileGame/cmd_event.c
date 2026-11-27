#include "sg.h"

#include <math.h>

static const float kWalkCellsPerSec = 4.0f;

static void parse_sheets(SgaEvent *ev, const SgrRec *rec, Game *d) {
    Rd r = rd_make(rec->body, rec->size);
    rd_guid(&r);
    free(rd_str(&r));
    i32 n = rd_i32(&r);
    if (r.bad || n < 0 || n > 1000) return;
    ev->sheets = sg_alloc(sizeof(SgaSheet) * (size_t)(n ? n : 1));
    for (int i = 0; i < n && !r.bad; i++) {
        SgaSheet *s = &ev->sheets[i];
        i32 conds = rd_i32(&r);
        if (conds < 0 || conds > 64) {
            r.bad = true;
            break;
        }
        for (int k = 0; k < conds; k++) {
            SgaCond c = {rd_i32(&r), rd_i32(&r), rd_i32(&r), rd_i32(&r), rd_guid(&r)};
            if (s->condCount < SGA_CONDS) s->conds[s->condCount++] = c;
        }
        free(rd_str(&r));
        s->graphic = rd_guid(&r);
        s->dir = rd_i32(&r);
        s->fixDir = rd_i32(&r);
        s->solid = rd_u8(&r);
        rd_i32(&r);
        s->still = rd_u8(&r);
        s->moveType = rd_i32(&r);
        s->moveSpeed = rd_i32(&r);
        s->script = rd_guid(&r);
        s->motion = rd_str(&r);
        const SgrRec *sr = game_record(d, SGR_EVENT, 16, s->script, NULL);
        if (sr && sr->size >= 21) {
            Rd t = rd_make(sr->body + 17, 4);
            s->trigger = rd_i32(&t);
        }
        ev->sheetCount = i + 1;
    }
}

static bool compare_op(i32 a, i32 b, int op) {
    switch (op) {
    case 0: return a == b;
    case 1: return a != b;
    case 2: return a >= b;
    case 3: return a <= b;
    case 4: return a > b;
    case 5: return a < b;
    default: return false;
    }
}

bool sga_cond_holds(SG *g, Guid event, const SgaCond *c) {
    switch (c->kind) {
    case COND_SWITCH: {
        bool on = c->index >= 65536 ? sg_local_switch(g, event, c->index - 65536) : c->index >= 0 && c->index < 1024 && g->switches[c->index];
        return on == (c->value == 0);
    }
    case COND_VARIABLE: return compare_op(c->index >= 0 && c->index < 1024 ? g->vars[c->index] : 0, c->value, c->op);
    case COND_MONEY: return compare_op(g->interp.a.money, c->value, c->op);
    case COND_ITEM:
    case COND_ITEM_ALL: return compare_op(sga_item_count(g, c->id), c->value, c->op);
    case COND_HERO: return sga_in_party(g, c->id) == (c->value == 0 ? true : c->value == 1);
    default: return true;
    }
}

int sga_event_sheet(SG *g, int npc) {
    SgaState *a = &g->interp.a;
    if (npc < 0 || npc >= a->evCount) return -1;
    SgaEvent *ev = &a->ev[npc];
    if (ev->erased) return -1;
    Guid id = g->field.npcs[npc].eventId;
    for (int i = ev->sheetCount - 1; i >= 0; i--) {
        bool ok = true;
        for (int k = 0; k < ev->sheets[i].condCount && ok; k++) ok = sga_cond_holds(g, id, &ev->sheets[i].conds[k]);
        if (ok) return i;
    }
    return -1;
}

void sga_actor_graphic(SG *g, Actor *a, Guid graphic) {
    const Res *res = res_find(&g->res, graphic);
    if (res == a->graphic && (a->sheet || a->model)) return;
    if (a->model) model_free(a->model);
    a->model = NULL;
    a->sheet = NULL;
    a->graphic = res;
    if (!res) return;
    size_t n = strlen(res->path);
    if (n > 4 && strcasecmp(res->path + n - 4, ".fbx") == 0) a->model = model_load(g, res->path);
    else a->sheet = sg_image(g, res->id);
}

static void apply_sheet(SG *g, int i) {
    SgaEvent *ev = &g->interp.a.ev[i];
    Actor *ac = &g->field.npcs[i];
    int s = sga_event_sheet(g, i);
    if (s == ev->sheet && !ev->erased) return;
    ev->sheet = s;
    ev->graphicOverride = false;
    free(ev->motion);
    ev->motion = NULL;
    if (s < 0) {
        ac->visible = false;
        return;
    }
    const SgaSheet *sh = &ev->sheets[s];
    sga_actor_graphic(g, ac, sh->graphic);
    ac->visible = ac->graphic != NULL;
    ev->motion = sg_strdup(sh->motion ? sh->motion : "");
}

void sga_refresh_events(SG *g) {
    SgaState *a = &g->interp.a;
    a->dirty = false;
    for (int i = 0; i < a->evCount && i < g->field.npcCount; i++) apply_sheet(g, i);
}

static void free_events(SgaState *a) {
    for (int i = 0; i < a->evCount; i++) {
        for (int k = 0; k < a->ev[i].sheetCount; k++) free(a->ev[i].sheets[k].motion);
        free(a->ev[i].sheets);
        free(a->ev[i].motion);
    }
    free(a->ev);
    a->ev = NULL;
    a->evCount = 0;
}

static const SgrRec *resource_record(SG *g, Guid id) {
    Game *d = &g->data;
    for (int p = -1; p < d->packCount; p++) {
        Sgr *s = p < 0 ? &d->resFile : &d->packRes[p];
        for (int i = 0; i < s->count; i++)
            if (s->recs[i].type == SGR_RESOURCE && s->recs[i].size >= 16 && memcmp(s->recs[i].body, id.b, 16) == 0) return &s->recs[i];
    }
    return NULL;
}

static void build_floor(SG *g) {
    SgaState *a = &g->interp.a;
    const Map *m = &g->field.map;
    free(a->floor);
    a->floorW = m->w;
    a->floorH = m->h;
    a->floor = sg_alloc(sizeof(i16) * (size_t)m->w * (size_t)m->h);
    memcpy(a->floor, m->height, sizeof(i16) * (size_t)m->w * (size_t)m->h);
    for (int i = 0; i < m->objectCount; i++) {
        const MapObject *o = &m->objects[i];
        const SgrRec *rec = resource_record(g, o->id);
        if (!rec || rec->sub != RES_MAPOBJECT) continue;
        Rd r = rd_make(rec->body, rec->size);
        rd_guid(&r);
        free(rd_str(&r));
        free(rd_str(&r));
        rd_i32(&r);
        int n = rd_i16(&r);
        for (int k = 0; k < n && !r.bad; k++) {
            int ex = rd_i16(&r) - 32, ez = rd_i16(&r) - 32, h = rd_i16(&r);
            rd_i16(&r);
            int dx = ex, dz = ez;
            switch (o->dir & 3) {
            case 1: dx = -ez, dz = ex; break;
            case 2: dx = -ex, dz = -ez; break;
            case 3: dx = ez, dz = -ex; break;
            default: break;
            }
            int x = o->x + dx, z = o->z + dz;
            if (x < 0 || z < 0 || x >= m->w || z >= m->h || h <= 0) continue;
            i16 *f = &a->floor[(size_t)z * (size_t)m->w + (size_t)x];
            i16 top = (i16)(m->height[(size_t)z * (size_t)m->w + (size_t)x] + h);
            if (top > *f) *f = top;
        }
    }
}

int sga_floor(SG *g, int x, int z) {
    SgaState *a = &g->interp.a;
    const Map *m = &g->field.map;
    if (x < 0 || z < 0 || x >= m->w || z >= m->h) return 0;
    if (a->floor && a->floorW == m->w && a->floorH == m->h && guid_eq(a->mapId, m->id)) return a->floor[(size_t)z * (size_t)m->w + (size_t)x];
    return m->height[(size_t)z * (size_t)m->w + (size_t)x];
}

void sga_map_loaded(SG *g) {
    SgaState *a = &g->interp.a;
    Field *f = &g->field;
    free_events(a);
    a->mapId = f->map.id;
    a->mapPending = false;
    build_floor(g);
    a->evCount = f->npcCount;
    a->ev = sg_alloc(sizeof(SgaEvent) * (size_t)(a->evCount ? a->evCount : 1));
    for (int i = 0; i < a->evCount; i++) {
        a->ev[i].sheet = -2;
        a->ev[i].autoSheet = -1;
        const SgrRec *rec = game_record(&g->data, SGR_EVENT, 0, f->npcs[i].eventId, NULL);
        if (rec) parse_sheets(&a->ev[i], rec, &g->data);
    }
    sga_refresh_events(g);
    if (!guid_zero(a->heroGraphic)) sga_actor_graphic(g, &f->player, a->heroGraphic);
    a->lastX = (int)f->player.x;
    a->lastZ = (int)f->player.z;
    a->playerMoveLeft = 0;
    Interp *ip = &g->interp;
    if (ip->active && ip->a.eventIndex == -2) {
        bool match = false;
        for (int i = 0; i < a->evCount && !match; i++) {
            SgaEvent *ev = &a->ev[i];
            if (ev->sheet >= 0 && ev->sheets[ev->sheet].trigger == TRIG_AUTO && guid_eq(ev->sheets[ev->sheet].script, ip->script.id)) {
                ev->autoSheet = ev->sheet;
                ip->a.eventIndex = i;
                match = true;
            }
        }
        if (!match) {
            sg_log(g, "autorun from load does not hold, stopped");
            ip->active = false;
        }
    }
    sg_log(g, "events ready: %d", a->evCount);
}

int sga_dir_from_code(int code) {
    switch (code) {
    case 0: return 0;
    case 1: return 2;
    case 2: return 3;
    case 3: return 1;
    default: return 2;
    }
}

void sga_start_event(SG *g, int npc, int sheet) {
    const SgaSheet *sh = &g->interp.a.ev[npc].sheets[sheet];
    const SgrRec *sr = game_record(&g->data, SGR_EVENT, 16, sh->script, NULL);
    Script s;
    if (!sr || !script_parse(&s, sr)) {
        script_free(&s);
        return;
    }
    if (s.count > 0) {
        interp_start(g, &s);
        g->interp.a.eventIndex = npc;
        sg_log(g, "start %s sheet %d trigger %d", g->field.events[npc].name ? g->field.events[npc].name : "?", sheet, sh->trigger);
    }
    script_free(&s);
}

static int npc_at(SG *g, int x, int z, bool needVisible) {
    Field *f = &g->field;
    for (int i = 0; i < f->npcCount; i++) {
        if ((int)floorf(f->npcs[i].x) != x || (int)floorf(f->npcs[i].z) != z) continue;
        if (i >= g->interp.a.evCount || g->interp.a.ev[i].sheet < 0) continue;
        if (needVisible && !f->npcs[i].visible) continue;
        return i;
    }
    return -1;
}

static int trigger_of(SG *g, int npc) {
    SgaEvent *ev = &g->interp.a.ev[npc];
    return ev->sheet >= 0 ? ev->sheets[ev->sheet].trigger : 0;
}

static void face_player(SG *g, int npc) {
    SgaEvent *ev = &g->interp.a.ev[npc];
    const SgaSheet *sh = &ev->sheets[ev->sheet];
    if (sh->fixDir || sh->dir != 1 || sh->still) return;
    static const int back[4] = {2, 3, 0, 1};
    g->field.npcs[npc].dir = back[g->field.player.dir & 3];
}

static bool try_autoruns(SG *g) {
    SgaState *a = &g->interp.a;
    for (int i = 0; i < a->evCount; i++) {
        SgaEvent *ev = &a->ev[i];
        if (ev->sheet < 0) continue;
        int t = ev->sheets[ev->sheet].trigger;
        if (t == TRIG_AUTO && ev->autoSheet != ev->sheet) {
            ev->autoSheet = ev->sheet;
            sga_start_event(g, i, ev->sheet);
            if (g->interp.active) return true;
        } else if (t == TRIG_REPEAT) {
            sga_start_event(g, i, ev->sheet);
            if (g->interp.active) return true;
        }
    }
    return false;
}

bool sga_field_input(SG *g) {
    SgaState *a = &g->interp.a;
    Field *f = &g->field;
    if (!f->loaded || g->pendingTransfer || a->mapPending) return false;
    if (try_autoruns(g)) return true;
    Actor *p = &f->player;
    if (p->moving || a->playerMoveLeft) return false;
    int px = (int)floorf(p->x), pz = (int)floorf(p->z);
    if (px != a->lastX || pz != a->lastZ) {
        a->lastX = px;
        a->lastZ = pz;
        int i = npc_at(g, px, pz, false);
        if (i >= 0 && (trigger_of(g, i) == TRIG_TOUCH || trigger_of(g, i) == TRIG_EVENT_TOUCH)) {
            sga_start_event(g, i, a->ev[i].sheet);
            if (g->interp.active) return true;
        }
    }
    static const int dx[4] = {0, 1, 0, -1}, dz[4] = {-1, 0, 1, 0};
    int dir = p->dir & 3;
    int tx = px + dx[dir], tz = pz + dz[dir];
    if (sg_pressed(g, KEY_OK)) {
        int i = npc_at(g, tx, tz, false);
        if (i < 0) i = npc_at(g, px, pz, false);
        if (i >= 0 && trigger_of(g, i) == TRIG_TALK) {
            face_player(g, i);
            sga_start_event(g, i, a->ev[i].sheet);
            if (g->interp.active) return true;
        }
    }
    u32 want = dir == 0 ? KEY_UP : dir == 1 ? KEY_RIGHT : dir == 2 ? KEY_DOWN : KEY_LEFT;
    if (g->keys & want) {
        int i = npc_at(g, tx, tz, true);
        if (i >= 0 && (trigger_of(g, i) == TRIG_TOUCH || trigger_of(g, i) == TRIG_EVENT_TOUCH)) {
            sga_start_event(g, i, a->ev[i].sheet);
            if (g->interp.active) return true;
        }
    }
    return false;
}

void sga_move_actor(SG *g, Actor *a, int *dir, int *left, float speed, bool through, u32 ms) {
    static const int dx[4] = {0, 1, 0, -1}, dz[4] = {-1, 0, 1, 0};
    float budget = speed * (float)ms / 1000.0f;
    while (*left > 0 && budget > 0) {
        if (!a->moving) {
            a->dir = *dir;
            a->tx = floorf(a->x) + 0.5f + (float)dx[*dir];
            a->tz = floorf(a->z) + 0.5f + (float)dz[*dir];
            a->moving = true;
        }
        float ddx = a->tx - a->x, ddz = a->tz - a->z;
        float d = sqrtf(ddx * ddx + ddz * ddz);
        if (d <= budget) {
            a->x = a->tx;
            a->z = a->tz;
            a->moving = false;
            budget -= d;
            (*left)--;
        } else {
            a->x += ddx / d * budget;
            a->z += ddz / d * budget;
            budget = 0;
        }
        a->y = (float)sga_floor(g, (int)floorf(a->x), (int)floorf(a->z));
    }
    (void)through;
    if (*left <= 0) a->moving = false;
}

void sga_step_moves(SG *g, u32 ms) {
    SgaState *a = &g->interp.a;
    Field *f = &g->field;
    if (a->playerMoveLeft > 0) {
        sga_move_actor(g, &f->player, &a->playerMoveDir, &a->playerMoveLeft, a->playerMoveSpeed, true, ms);
        if (!a->playerMoveLeft) {
            a->lastX = (int)floorf(f->player.x);
            a->lastZ = (int)floorf(f->player.z);
        }
    }
    for (int i = 0; i < a->evCount && i < f->npcCount; i++)
        if (a->ev[i].moveLeft > 0) sga_move_actor(g, &f->npcs[i], &a->ev[i].moveDir, &a->ev[i].moveLeft, a->ev[i].moveSpeed, true, ms);
}

bool sga_moves_pending(SG *g) {
    SgaState *a = &g->interp.a;
    if (a->playerMoveLeft > 0) return true;
    for (int i = 0; i < a->evCount; i++)
        if (a->ev[i].moveLeft > 0) return true;
    return false;
}

void sga_move_command(SG *g, const Cmd *c, bool player) {
    SgaState *a = &g->interp.a;
    int code = cmd_int(c, 0), count = c->argCount >= 2 ? cmd_int(c, 1) : 1;
    if (count <= 0) count = 1;
    int dir = sga_dir_from_code(code);
    if (code > 3) {
        sg_log(g, "move code %d not known", code);
        return;
    }
    if (player) {
        a->playerMoveDir = dir;
        a->playerMoveLeft = count;
        a->playerMoveSpeed = kWalkCellsPerSec;
        g->field.player.moving = false;
        sg_log(g, "move player dir %d x%d", dir, count);
    } else {
        int i = a->eventIndex;
        if (i < 0 || i >= a->evCount) return;
        a->ev[i].moveDir = dir;
        a->ev[i].moveLeft = count;
        a->ev[i].moveSpeed = kWalkCellsPerSec;
        g->field.npcs[i].moving = false;
        sg_log(g, "move event %d dir %d x%d", i, dir, count);
    }
}

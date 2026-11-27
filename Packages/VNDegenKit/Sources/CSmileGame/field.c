#include "sg.h"

#include <math.h>

static int cell_height(const Map *m, int x, int z) {
    if (x < 0 || z < 0 || x >= m->w || z >= m->h) return 0;
    return m->height[(size_t)z * (size_t)m->w + (size_t)x];
}

static const Res *hero_graphic(SG *g) { return res_find(&g->res, party_leader_graphic(g)); }

static void actor_set_graphic(SG *g, Actor *a, const Res *res) {
    a->graphic = res;
    a->sheet = NULL;
    a->model = NULL;
    if (!res) return;
    size_t n = strlen(res->path);
    if (n > 4 && strcasecmp(res->path + n - 4, ".fbx") == 0) {
        a->model = model_load(g, res->path);
    } else {
        a->sheet = sg_image(g, res->id);
    }
}

static bool cond_holds(SG *g, const EventDef *e, const EventCond *c) {
    if (c->kind == 0) {
        bool on;
        if (c->index >= 65536) on = sg_local_switch(g, e->id, c->index - 65536);
        else on = c->index >= 0 && c->index < 1024 && g->switches[c->index];
        return on == (c->value == 0);
    }
    if (c->kind == 1) {
        i32 v = c->index >= 0 && c->index < 1024 ? g->vars[c->index] : 0;
        return v == c->value;
    }
    return true;
}

int event_sheet(SG *g, const EventDef *e) {
    for (int i = e->sheetCount - 1; i >= 0; i--) {
        bool ok = true;
        for (int k = 0; k < e->sheets[i].condCount && ok; k++) ok = cond_holds(g, e, &e->sheets[i].conds[k]);
        if (ok) return i;
    }
    return -1;
}

bool field_load(SG *g, Guid mapId, int x, int z, int dir) {
    Field *f = &g->field;
    Sgr *file = NULL;
    const SgrRec *rec = game_record(&g->data, SGR_MAP, 0, mapId, &file);
    if (!rec) {
        sg_log(g, "map not found");
        return false;
    }
    Actor player = f->player;
    field_free(f);
    if (!map_parse(&f->map, rec)) {
        sg_log(g, "map parse failed");
        return false;
    }
    sg_log(g, "map %s %dx%d events %d objects %d chips %d", f->map.name, f->map.w, f->map.h, f->map.eventCount, f->map.objectCount,
           f->map.chipCount);
    f->events = sg_alloc(sizeof(EventDef) * (size_t)(f->map.eventCount ? f->map.eventCount : 1));
    f->npcs = sg_alloc(sizeof(Actor) * (size_t)(f->map.eventCount ? f->map.eventCount : 1));
    for (int i = 0; i < f->map.eventCount; i++) {
        Actor *a = &f->npcs[i];
        a->x = (float)f->map.events[i].x + 0.5f;
        a->z = (float)f->map.events[i].z + 0.5f;
        a->y = (float)cell_height(&f->map, f->map.events[i].x, f->map.events[i].z);
        a->dir = 2;
        a->eventId = f->map.events[i].id;
        const SgrRec *er = NULL;
        for (int k = 0; k < file->count && !er; k++)
            if (file->recs[k].type == SGR_EVENT && file->recs[k].sub == 0 && memcmp(file->recs[k].body, a->eventId.b, 16) == 0)
                er = &file->recs[k];
        if (!er || !event_parse(&f->events[i], er)) continue;
        int s = event_sheet(g, &f->events[i]);
        if (s >= 0) {
            actor_set_graphic(g, a, res_find(&g->res, f->events[i].sheets[s].graphic));
            a->visible = a->graphic != NULL;
        }
    }
    f->npcCount = f->map.eventCount;
    f->player = player;
    f->player.x = (float)x + 0.5f;
    f->player.z = (float)z + 0.5f;
    f->player.tx = f->player.x;
    f->player.tz = f->player.z;
    f->player.y = (float)cell_height(&f->map, x, z);
    f->player.dir = dir;
    f->player.moving = false;
    if (!f->player.graphic) actor_set_graphic(g, &f->player, hero_graphic(g));
    f->player.visible = true;
    f->loaded = true;
    view_load(g);
    for (int i = 0; i < f->npcCount; i++) {
        const EventDef *e = &f->events[i];
        int sheet = event_sheet(g, e);
        if (sheet < 0) continue;
        const SgrRec *sr = NULL;
        for (int k = 0; k < file->count && !sr; k++)
            if (file->recs[k].type == SGR_EVENT && file->recs[k].sub == 16 && memcmp(file->recs[k].body, e->sheets[sheet].script.b, 16) == 0)
                sr = &file->recs[k];
        if (!sr) continue;
        Script s;
        if (script_parse(&s, sr) && s.trigger == 1 && s.count > 0 && !g->interp.active) {
            sg_log(g, "autorun event %s", e->name);
            interp_start(g, &s);
        }
        script_free(&s);
    }
    return true;
}

void field_free(Field *f) {
    if (f->objectModels)
        for (int i = 0; i < f->map.objectCount; i++) model_free(f->objectModels[i]);
    free(f->objectModels);
    map_free(&f->map);
    mesh_free(&f->terrain);
    free(f->chipImages);
    for (int i = 0; i < f->npcCount; i++) {
        event_free(&f->events[i]);
        model_free(f->npcs[i].model);
    }
    free(f->events);
    free(f->npcs);
    Actor player = f->player;
    memset(f, 0, sizeof *f);
    f->player = player;
}

static bool walkable(SG *g, Field *f, int fromX, int fromZ, int x, int z) {
    (void)f;
    return sgi_cell_free(g, fromX, fromZ, x, z, -1);
}

void field_update(SG *g, u32 ms) {
    Field *f = &g->field;
    if (!f->loaded) return;
    Actor *p = &f->player;
    p->animMs += ms;
    if (!g->interp.active) {
        if (!p->moving) {
            int dx = 0, dz = 0, dir = p->dir;
            if (g->keys & KEY_UP) dz = -1, dir = 0;
            else if (g->keys & KEY_DOWN) dz = 1, dir = 2;
            else if (g->keys & KEY_LEFT) dx = -1, dir = 3;
            else if (g->keys & KEY_RIGHT) dx = 1, dir = 1;
            if (dx || dz) {
                p->dir = dir;
                int cx = (int)p->x, cz = (int)p->z;
                if (walkable(g, f, cx, cz, cx + dx, cz + dz)) {
                    p->tx = (float)(cx + dx) + 0.5f;
                    p->tz = (float)(cz + dz) + 0.5f;
                    p->moving = true;
                }
            }
        }
        if (p->moving) {
            float speed = (g->keys & KEY_DASH ? 8.0f : 4.0f) * (float)ms / 1000.0f;
            float ddx = p->tx - p->x, ddz = p->tz - p->z;
            float d = sqrtf(ddx * ddx + ddz * ddz);
            if (d <= speed) {
                p->x = p->tx;
                p->z = p->tz;
                p->moving = false;
            } else {
                p->x += ddx / d * speed;
                p->z += ddz / d * speed;
            }
            p->y = sg_ground_at(g, (int)p->x, (int)p->z, p->y);
        }
    }
    view_follow(g, p->x, p->y, p->z, ms);
}

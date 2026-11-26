#include "sg.h"

#include <math.h>

static const float kTileTop = 48.0f;

static int cell_height(const Map *m, int x, int z) {
    if (x < 0 || z < 0 || x >= m->w || z >= m->h) return 0;
    return m->height[(size_t)z * (size_t)m->w + (size_t)x];
}

static Image *chip_image(SG *g, Field *f, int index) {
    if (index < 0 || index >= f->chipImageCount) return NULL;
    if (!f->chipImages[index]) f->chipImages[index] = sg_image(g, map_chip_guid(&f->map, index));
    return f->chipImages[index];
}

static void side_quad(Mesh *mesh, const Image *im, Vec3 a, Vec3 b, int unitFromTop, float shade) {
    float th = (float)im->h;
    float v0, v1;
    if (th >= kTileTop * 3 && unitFromTop > 0) {
        v0 = kTileTop * 2 / th;
        v1 = kTileTop * 3 / th;
    } else if (th >= kTileTop * 2) {
        v0 = kTileTop / th;
        v1 = kTileTop * 2 / th;
    } else {
        v0 = 0;
        v1 = 1;
    }
    Vec3 c = {b.x, b.y - 1, b.z}, d = {a.x, a.y - 1, a.z};
    mesh_quad(mesh, im, a, b, c, d, 0, v0, 1, v1, shade, false);
}

static void build_terrain(SG *g, Field *f) {
    Map *m = &f->map;
    int maxIndex = 0;
    for (int i = 0; i < m->chipCount; i++)
        if (m->chips[i].index > maxIndex) maxIndex = m->chips[i].index;
    f->chipImageCount = maxIndex + 1;
    f->chipImages = sg_alloc(sizeof(Image *) * (size_t)f->chipImageCount);
    for (int z = 0; z < m->h; z++) {
        for (int x = 0; x < m->w; x++) {
            int h = cell_height(m, x, z);
            Image *im = chip_image(g, f, m->chip[(size_t)z * (size_t)m->w + (size_t)x]);
            if (!im || h <= 0) continue;
            float fx = (float)x, fz = (float)z, fh = (float)h;
            float topV = fminf(1.0f, kTileTop / (float)im->h);
            mesh_quad(&f->terrain, im, (Vec3){fx, fh, fz}, (Vec3){fx + 1, fh, fz}, (Vec3){fx + 1, fh, fz + 1}, (Vec3){fx, fh, fz + 1}, 0,
                      0, 1, topV, 1.0f, false);
            static const int dx[4] = {0, 0, -1, 1}, dz[4] = {1, -1, 0, 0};
            static const float shade[4] = {0.78f, 0.6f, 0.68f, 0.68f};
            for (int s = 0; s < 4; s++) {
                int nh = cell_height(m, x + dx[s], z + dz[s]);
                if (nh >= h) continue;
                for (int k = h; k > nh; k--) {
                    float y = (float)k;
                    Vec3 a, b;
                    if (s == 0) a = (Vec3){fx, y, fz + 1}, b = (Vec3){fx + 1, y, fz + 1};
                    else if (s == 1) a = (Vec3){fx + 1, y, fz}, b = (Vec3){fx, y, fz};
                    else if (s == 2) a = (Vec3){fx, y, fz}, b = (Vec3){fx, y, fz + 1};
                    else a = (Vec3){fx + 1, y, fz + 1}, b = (Vec3){fx + 1, y, fz};
                    side_quad(&f->terrain, im, a, b, h - k, shade[s]);
                }
            }
        }
    }
}

static const Res *hero_graphic(SG *g) {
    const SgrRec *hero = game_hero(&g->data, g->data.settings.party[0]);
    if (!hero) return NULL;
    Rd r = rd_make(hero->body, hero->size);
    rd_guid(&r);
    free(rd_str(&r));
    return res_find(&g->res, rd_guid(&r));
}

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
    build_terrain(g, f);
    f->objectModels = sg_alloc(sizeof(Model *) * (size_t)(f->map.objectCount ? f->map.objectCount : 1));
    for (int i = 0; i < f->map.objectCount; i++) {
        const Res *r = res_find(&g->res, f->map.objects[i].id);
        if (r) f->objectModels[i] = model_load(g, r->path);
    }
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
    f->cam.pitch = 49.0f;
    f->cam.yaw = 0.0f;
    f->cam.fov = 30.0f;
    f->cam.distance = 15.5f;
    f->loaded = true;
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

static bool walkable(Field *f, int fromX, int fromZ, int x, int z) {
    Map *m = &f->map;
    if (x < 0 || z < 0 || x >= m->w || z >= m->h) return false;
    int h0 = cell_height(m, fromX, fromZ), h1 = cell_height(m, x, z);
    if (h1 != h0) {
        bool stair = false;
        for (int i = 0; i < m->stairCount; i++)
            if ((m->stairs[i].x == x && m->stairs[i].z == z) || (m->stairs[i].x == fromX && m->stairs[i].z == fromZ)) stair = true;
        if (!stair || abs(h1 - h0) > 1) return false;
    }
    for (int i = 0; i < f->npcCount; i++)
        if (f->npcs[i].visible && (int)f->npcs[i].x == x && (int)f->npcs[i].z == z) return false;
    return true;
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
                if (walkable(f, cx, cz, cx + dx, cz + dz)) {
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
            p->y = (float)cell_height(&f->map, (int)p->x, (int)p->z);
        }
    }
    f->cam.target = (Vec3){p->x, p->y, p->z - 1.0f};
}

static int sprite_row(int dir) {
    switch (dir) {
    case 0: return 0;
    case 1: return 3;
    case 3: return 2;
    default: return 1;
    }
}

static void draw_actor(SG *g, Field *f, const Actor *a) {
    if (!a->visible) return;
    if (a->model) {
        Mesh *m = &a->model->mesh;
        float ang = (float)a->dir * 1.5707963f;
        float cs = cosf(ang), sn = sinf(ang);
        for (int i = 0; i < m->count; i++) {
            Tri t = m->tris[i];
            for (int k = 0; k < 3; k++) {
                float x = t.v[k].x, z = t.v[k].z;
                t.v[k].x = a->x + x * cs - z * sn;
                t.v[k].z = a->z + x * sn + z * cs;
                t.v[k].y += a->y;
            }
            r3_tri(&g->r3, &f->cam, &t);
        }
        return;
    }
    const Image *im = a->sheet;
    if (!im || !a->graphic) return;
    int frames = a->graphic->frames > 0 ? a->graphic->frames : 1;
    int rows = a->graphic->divisions == 1 ? 4 : 1;
    int cw = im->w / frames, ch = im->h / rows;
    int frame = 0;
    if (a->graphic->frameMs > 0) frame = (int)(a->animMs / (u32)a->graphic->frameMs) % frames;
    if (frames == 3 && rows == 4) {
        static const int cycle[4] = {1, 0, 1, 2};
        frame = a->moving ? cycle[(a->animMs / (u32)(a->graphic->frameMs > 0 ? a->graphic->frameMs / 2 : 125)) % 4] : 1;
    }
    int row = rows == 4 ? sprite_row(a->dir) : 0;
    float w = (float)cw / kTileTop, h = (float)ch / kTileTop;
    float p = f->cam.pitch * 3.14159265f / 180.0f;
    float tall = h / cosf(p);
    Vec3 base = {a->x, a->y, a->z};
    Vec3 tl = {base.x - w / 2, base.y + tall, base.z};
    Vec3 tr = {base.x + w / 2, base.y + tall, base.z};
    Vec3 br = {base.x + w / 2, base.y, base.z};
    Vec3 bl = {base.x - w / 2, base.y, base.z};
    float u0 = (float)(frame * cw) / (float)im->w, u1 = (float)((frame + 1) * cw) / (float)im->w;
    float v0 = (float)(row * ch) / (float)im->h, v1 = (float)((row + 1) * ch) / (float)im->h;
    float inset = 0.5f / (float)im->w;
    Vert va = {tl.x, tl.y, tl.z, u0 + inset, v0, 1}, vb = {tr.x, tr.y, tr.z, u1 - inset, v0, 1};
    Vert vc = {br.x, br.y, br.z, u1 - inset, v1, 1}, vd = {bl.x, bl.y, bl.z, u0 + inset, v1, 1};
    r3_tri(&g->r3, &f->cam, &(Tri){{va, vb, vc}, im, true, false});
    r3_tri(&g->r3, &f->cam, &(Tri){{va, vc, vd}, im, true, false});
}

void field_draw(SG *g) {
    Field *f = &g->field;
    r3_begin(&g->r3, g->fb, 0xff000000u);
    if (!f->loaded) return;
    cam_update(&f->cam, g->w, g->h);
    for (int k = 0; k < 3; k++) g->r3.tint[k] = f->map.colors[1 + k] * 384 / 255;
    for (int i = 0; i < f->terrain.count; i++) r3_tri(&g->r3, &f->cam, &f->terrain.tris[i]);
    for (int i = 0; i < f->map.objectCount; i++) {
        Model *m = f->objectModels[i];
        if (!m) continue;
        const MapObject *o = &f->map.objects[i];
        float ang = (float)o->dir * 1.5707963f;
        float cs = cosf(ang), sn = sinf(ang);
        float bx = (float)o->x + 0.5f, bz = (float)o->z + 0.5f, by = (float)cell_height(&f->map, o->x, o->z);
        for (int k = 0; k < m->mesh.count; k++) {
            Tri t = m->mesh.tris[k];
            for (int c = 0; c < 3; c++) {
                float x = t.v[c].x, z = t.v[c].z;
                t.v[c].x = bx + x * cs - z * sn;
                t.v[c].z = bz + x * sn + z * cs;
                t.v[c].y += by;
            }
            r3_tri(&g->r3, &f->cam, &t);
        }
    }
    for (int i = 0; i < f->npcCount; i++)
        if (f->npcs[i].model) draw_actor(g, f, &f->npcs[i]);
    g->r3.tint[0] = g->r3.tint[1] = g->r3.tint[2] = 256;
    for (int i = 0; i < f->npcCount; i++)
        if (!f->npcs[i].model) draw_actor(g, f, &f->npcs[i]);
    draw_actor(g, f, &f->player);
}

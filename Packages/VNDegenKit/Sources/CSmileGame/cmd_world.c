#include "sg.h"

#include <math.h>

static const u32 kBalloonMs = 1000;

static SgaState *st(SG *g) { return &g->interp.a; }

i32 sga_item_count(SG *g, Guid id) {
    SgaState *a = st(g);
    for (int i = 0; i < a->itemCount; i++)
        if (guid_eq(a->items[i].id, id)) return a->items[i].count;
    return 0;
}

void sga_item_add(SG *g, Guid id, i32 n) {
    SgaState *a = st(g);
    for (int i = 0; i < a->itemCount; i++)
        if (guid_eq(a->items[i].id, id)) {
            a->items[i].count += n;
            if (a->items[i].count < 0) a->items[i].count = 0;
            return;
        }
    if (n <= 0) return;
    a->items = sg_realloc(a->items, sizeof(SgaItem) * (size_t)(a->itemCount + 1));
    a->items[a->itemCount++] = (SgaItem){id, n};
}

bool sga_in_party(SG *g, Guid hero) {
    SgaState *a = st(g);
    for (int i = 0; i < a->partyCount; i++)
        if (guid_eq(a->party[i], hero)) return true;
    return false;
}

static Guid hero_default_graphic(SG *g, Guid hero) {
    const SgrRec *rec = game_hero(&g->data, hero);
    if (!rec) return (Guid){{0}};
    Rd r = rd_make(rec->body, rec->size);
    rd_guid(&r);
    free(rd_str(&r));
    return rd_guid(&r);
}

static void update_leader_graphic(SG *g) {
    SgaState *a = st(g);
    if (!a->partyCount) return;
    Guid gr = guid_zero(a->heroGraphic) ? hero_default_graphic(g, a->party[0]) : a->heroGraphic;
    sga_actor_graphic(g, &g->field.player, gr);
}

void sga_new_game(SG *g) {
    SgaState *a = st(g);
    free(a->items);
    a->items = NULL;
    a->itemCount = 0;
    a->money = 0;
    a->partyCount = 0;
    for (int i = 0; i < g->data.settings.partyCount && i < SGA_PARTY; i++)
        if (!guid_zero(g->data.settings.party[i])) a->party[a->partyCount++] = g->data.settings.party[i];
    a->heroGraphic = (Guid){{0}};
    memset(a->pics, 0, sizeof a->pics);
    a->tint = 0;
    a->tintTotal = 0;
    a->started = true;
}

void sga_free(SG *g) { interp_free(&g->interp); }

Image *sga_path_image(SG *g, const char *path) {
    SgaState *a = st(g);
    char key[512];
    pack_key(path, key, sizeof key);
    for (int i = 0; i < a->faceCount; i++)
        if (strcmp(a->faces[i].key, key) == 0) return a->faces[i].im;
    size_t size = 0;
    u8 *data = pack_read(&g->data.pack, key, &size);
    Image *im = data ? img_decode(data, size) : NULL;
    free(data);
    a->faces = sg_realloc(a->faces, sizeof(SgaFaceImage) * (size_t)(a->faceCount + 1));
    a->faces[a->faceCount++] = (SgaFaceImage){sg_strdup(key), im};
    return im;
}

Image *sga_face(SG *g, Guid folder, int expr) {
    if (guid_zero(folder)) return NULL;
    const Res *r = res_find(&g->res, folder);
    if (!r) return NULL;
    static const char *names[4] = {"normal", "smile", "anger", "sorrow"};
    char dir[512];
    pack_key(r->path, dir, sizeof dir);
    size_t dn = strlen(dir);
    const Pack *p = &g->data.pack;
    const char *first = NULL;
    for (int i = 0; i < p->count; i++) {
        const char *k = p->entries[i].key;
        if (strncmp(k, dir, dn) != 0 || k[dn] != '/' || strchr(k + dn + 1, '/')) continue;
        size_t kn = strlen(k);
        if (kn < 4 || strcmp(k + kn - 4, ".png") != 0) continue;
        if (!first) first = k;
        if (expr >= 0 && expr < 4) {
            const char *us = strrchr(k + dn + 1, '_');
            if (us && strncmp(us + 1, names[expr], strlen(names[expr])) == 0 && us[1 + strlen(names[expr])] == '.') return sga_path_image(g, k);
        }
    }
    return first ? sga_path_image(g, first) : NULL;
}

static const Res *named_window(SG *g, const char *name) {
    for (int i = 0; i < g->res.count; i++)
        if (g->res.items[i].kind == RES_WINDOW && strcmp(g->res.items[i].name, name) == 0) return &g->res.items[i];
    return NULL;
}

Image *sga_default_window(SG *g, const Res **res) {
    *res = named_window(g, "default");
    return *res ? sg_image(g, (*res)->id) : NULL;
}

Image *sga_selected_window(SG *g, const Res **res) {
    *res = named_window(g, "default_selected");
    return *res ? sg_image(g, (*res)->id) : NULL;
}

static void set_fade(SG *g, float to, u32 ms) {
    SgaState *a = st(g);
    a->fadeFrom = a->fade;
    a->fadeTo = to;
    a->fadeMs = 0;
    a->fadeTotal = ms;
    if (!ms) a->fade = to;
}

static Guid run_event_id(SG *g) {
    int i = st(g)->eventIndex;
    return i >= 0 && i < g->field.npcCount ? g->field.npcs[i].eventId : (Guid){{0}};
}

static void set_local_switch(SG *g, Guid event, int index, bool on) {
    if (index < 0 || index >= 4 || guid_zero(event)) return;
    for (int i = 0; i < g->localCount; i++)
        if (guid_eq(g->locals[i].event, event)) {
            g->locals[i].on[index] = on;
            return;
        }
    g->locals = sg_realloc(g->locals, sizeof *g->locals * (size_t)(g->localCount + 1));
    memset(&g->locals[g->localCount], 0, sizeof *g->locals);
    g->locals[g->localCount].event = event;
    g->locals[g->localCount].on[index] = on;
    g->localCount++;
}

static void cmd_variable(SG *g, const Cmd *c) {
    i32 index = cmd_int(c, 1), kind = cmd_int(c, 2), value = cmd_int(c, 3), op = cmd_int(c, 4);
    if (index < 0 || index >= 1024) return;
    if (kind == 1 && value >= 0 && value < 1024) value = g->vars[value];
    i32 *v = &g->vars[index];
    switch (op) {
    case 0: *v = value; break;
    case 1: *v += value; break;
    case 2: *v -= value; break;
    case 3: *v *= value; break;
    case 4: *v = value ? *v / value : 0; break;
    case 5: *v = value ? *v % value : 0; break;
    default: sg_log(g, "variable op %d not known", op); return;
    }
    sg_log(g, "variable %d = %d", index, *v);
}

static void cmd_picture(SG *g, const Cmd *c) {
    SgaState *a = st(g);
    int n = cmd_int(c, 0);
    if (n < 0 || n >= SGA_PICTURES) return;
    SgaPicture *p = &a->pics[n];
    if (c->type == CMD_SHOW_PICTURE) {
        Image *im = sg_image(g, cmd_guid(c, 1));
        *p = (SgaPicture){0};
        p->used = im != NULL;
        p->id = cmd_guid(c, 1);
        p->im = im;
        p->alphaTo = (float)cmd_int(c, 2) / 100.0f;
        p->x = p->xTo = (float)cmd_int(c, 5);
        p->y = p->yTo = (float)cmd_int(c, 6);
        p->zoom = p->zoomTo = 1.0f;
        bool fade = cmd_int(c, 8) != 0;
        p->alphaFrom = fade ? 0.0f : p->alphaTo;
        p->alpha = p->alphaFrom;
        p->total = fade ? 500 : 0;
        p->ms = 0;
        sg_log(g, "picture %d shown%s", n, fade ? ", fading in" : "");
    } else if (c->type == CMD_ERASE_PICTURE) {
        if (!p->used) return;
        if (cmd_int(c, 1)) {
            p->alphaFrom = p->alpha;
            p->alphaTo = 0;
            p->ms = 0;
            p->total = 500;
            p->eraseAfter = true;
        } else {
            *p = (SgaPicture){0};
        }
        sg_log(g, "picture %d erased", n);
    } else if (c->type == CMD_MOVE_PICTURE) {
        if (!p->used) return;
        p->alphaFrom = p->alpha;
        p->alphaTo = (float)cmd_int(c, 1) / 100.0f;
        p->ms = 0;
        p->total = (u32)cmd_int(c, 2);
        sg_log(g, "picture %d to alpha %d over %d ms", n, cmd_int(c, 1), cmd_int(c, 2));
    }
}

static void cmd_party(SG *g, const Cmd *c) {
    SgaState *a = st(g);
    Guid hero = cmd_guid(c, 0);
    if (cmd_int(c, 1) == 0) {
        if (!sga_in_party(g, hero) && a->partyCount < SGA_PARTY) a->party[a->partyCount++] = hero;
    } else {
        for (int i = 0; i < a->partyCount; i++)
            if (guid_eq(a->party[i], hero)) {
                memmove(&a->party[i], &a->party[i + 1], sizeof(Guid) * (size_t)(a->partyCount - i - 1));
                a->partyCount--;
                break;
            }
    }
    a->heroGraphic = (Guid){{0}};
    update_leader_graphic(g);
    sg_log(g, "party now %d", a->partyCount);
}

static void cmd_hero_graphic(SG *g, const Cmd *c) {
    SgaState *a = st(g);
    Guid hero = cmd_guid(c, 0);
    if (a->partyCount && !guid_eq(a->party[0], hero)) return;
    a->heroGraphic = cmd_guid(c, 2);
    update_leader_graphic(g);
    sg_log(g, "hero graphic %s motion %s", g->field.player.graphic ? g->field.player.graphic->name : "default", cmd_str(c, 3));
}

static void cmd_event_graphic(SG *g, const Cmd *c) {
    SgaState *a = st(g);
    int i = a->eventIndex;
    if (i < 0 || i >= a->evCount) return;
    SgaEvent *ev = &a->ev[i];
    ev->graphicOverride = true;
    ev->graphic = cmd_guid(c, 0);
    free(ev->motion);
    ev->motion = sg_strdup(cmd_str(c, 1));
    Actor *ac = &g->field.npcs[i];
    sga_actor_graphic(g, ac, ev->graphic);
    ac->visible = ac->graphic != NULL;
    sg_log(g, "event graphic %s motion %s", ac->graphic ? ac->graphic->name : "none", ev->motion);
}

static void cmd_transfer(SG *g, const Cmd *c) {
    SgaState *a = st(g);
    Guid map = cmd_guid(c, 0);
    g->pendingMap = guid_zero(map) ? g->field.map.id : map;
    g->pendingX = cmd_int(c, 1);
    g->pendingZ = cmd_int(c, 2);
    int code = cmd_int(c, 3);
    g->pendingDir = code == 0 ? g->field.player.dir : code == 1 ? 0 : code == 2 ? 2 : code == 3 ? 3 : 1;
    g->pendingTransfer = true;
    a->mapPending = true;
    a->playerMoveLeft = 0;
    sg_log(g, "transfer %d,%d dir code %d", g->pendingX, g->pendingZ, code);
}

bool sga_busy(SG *g) {
    SgaState *a = st(g);
    if (a->mapPending || g->pendingTransfer) return true;
    if (a->waitMoves && sga_moves_pending(g)) return true;
    if (a->balloonTotal && a->balloonMs < a->balloonTotal) return true;
    if (a->fadeWait && a->fadeTotal) return true;
    if (a->tintWait && a->tintTotal) return true;
    a->waitMoves = a->fadeWait = a->tintWait = false;
    return false;
}

bool sga_exec(SG *g, const Cmd *c) {
    SgaState *a = st(g);
    switch (c->type) {
    case CMD_TRANSFER: cmd_transfer(g, c); return true;
    case CMD_VARIABLE: cmd_variable(g, c); a->dirty = true; return false;
    case CMD_SWITCH: {
        i32 index = cmd_int(c, 0);
        bool on = cmd_int(c, 1) == 0;
        if (index >= 65536) set_local_switch(g, run_event_id(g), index - 65536, on);
        else if (index >= 0 && index < 1024) g->switches[index] = on;
        a->dirty = true;
        sg_log(g, "switch %d %s", index, on ? "on" : "off");
        return false;
    }
    case CMD_ITEM: {
        i32 n = cmd_int(c, 2) * (cmd_int(c, 3) == 2 ? -1 : 1);
        sga_item_add(g, cmd_guid(c, 0), n);
        a->dirty = true;
        sg_log(g, "item %+d, now %d", n, sga_item_count(g, cmd_guid(c, 0)));
        return false;
    }
    case CMD_MONEY: {
        i32 n = cmd_int(c, 1) * (cmd_int(c, 2) == 2 ? -1 : 1);
        a->money += n;
        if (a->money < 0) a->money = 0;
        a->dirty = true;
        sg_log(g, "money %+d, now %d", n, a->money);
        return false;
    }
    case CMD_PARTY: cmd_party(g, c); a->dirty = true; return false;
    case CMD_HERO_GRAPHIC: cmd_hero_graphic(g, c); return false;
    case CMD_EVENT_GRAPHIC: cmd_event_graphic(g, c); return false;
    case CMD_SCREEN:
        set_fade(g, cmd_int(c, 0) ? 1.0f : 0.0f, 0);
        sg_log(g, "screen %s", cmd_int(c, 0) ? "off" : "on");
        return false;
    case CMD_FADE:
        set_fade(g, cmd_int(c, 1) ? 1.0f : 0.0f, (u32)cmd_int(c, 0));
        a->fadeWait = cmd_int(c, 2) != 0;
        return a->fadeWait;
    case CMD_TINT: {
        a->tintFrom = a->tint;
        a->tintTo = (u32)cmd_int(c, 1);
        a->tintMs = 0;
        a->tintTotal = (u32)cmd_int(c, 0);
        if (!a->tintTotal) a->tint = a->tintTo;
        a->tintWait = cmd_int(c, 2) != 0;
        sg_log(g, "tint %08x over %d ms", a->tintTo, cmd_int(c, 0));
        return a->tintWait;
    }
    case CMD_FLASH:
        a->flashColor = 0xffffffffu;
        a->flashMs = 0;
        a->flashTotal = (u32)cmd_int(c, 0);
        return false;
    case CMD_SHAKE:
        a->shakeMs = 0;
        a->shakeTotal = (u32)cmd_int(c, 0);
        a->shakePower = cmd_int(c, 1);
        return false;
    case CMD_SHOW_PICTURE:
    case CMD_ERASE_PICTURE:
    case CMD_MOVE_PICTURE: cmd_picture(g, c); return false;
    case CMD_MOVE_EVENT:
    case CMD_MOVE_PLAYER:
        if (c->argCount < 6) {
            sg_log(g, "command %d with %d arguments not known", c->type, c->argCount);
            return false;
        }
        sga_move_command(g, c, c->type == CMD_MOVE_PLAYER);
        a->waitMoves = cmd_int(c, 5) == 0;
        return a->waitMoves;
    case CMD_TURN_EVENT: {
        int code = cmd_int(c, 0), i = a->eventIndex;
        if (code <= 3 && i >= 0 && i < a->evCount) g->field.npcs[i].dir = sga_dir_from_code(code);
        sg_log(g, "turn event %d", code);
        return false;
    }
    case CMD_TURN_PLAYER: {
        int code = cmd_int(c, 0);
        if (code <= 3) g->field.player.dir = sga_dir_from_code(code);
        sg_log(g, "turn player %d", code);
        return false;
    }
    case CMD_ERASE_EVENT: {
        int i = a->eventIndex;
        if (i >= 0 && i < a->evCount) {
            a->ev[i].erased = cmd_int(c, 0) != 0;
            sga_refresh_events(g);
            sg_log(g, "event %d %s", i, cmd_int(c, 0) ? "hidden" : "shown");
        }
        return false;
    }
    case CMD_BALLOON:
        a->balloonTarget = cmd_int(c, 0);
        a->balloonMs = 0;
        a->balloonTotal = kBalloonMs;
        sg_log(g, "balloon on %s", cmd_int(c, 0) ? "player" : "event");
        return true;
    case CMD_CAMERA:
        sg_log(g, "camera mode %d (%d args) not applied", cmd_int(c, 0), c->argCount);
        return false;
    case CMD_BGM:
    case CMD_SE:
    case CMD_BGS: {
        const Res *r = res_find(&g->res, cmd_guid(c, 0));
        sg_log(g, "sound %d %s vol %d (not played)", c->type, r ? r->name : "stop", cmd_int(c, 1));
        return false;
    }
    case CMD_EFFECT: sg_log(g, "effect on %d (not drawn)", cmd_int(c, 0)); return false;
    case CMD_TITLE: sg_log(g, "party recovered (not run)"); return false;
    default: sg_log(g, "command %d not handled (%d args)", c->type, c->argCount); return false;
    }
}

static void tick(u32 *ms, u32 total, u32 dt) {
    if (*ms < total) *ms = *ms + dt > total ? total : *ms + dt;
}

static u32 lerp_color(u32 a, u32 b, float t) {
    u32 out = 0;
    for (int s = 0; s < 32; s += 8) {
        float x = (float)(a >> s & 255), y = (float)(b >> s & 255);
        out |= (u32)(x + (y - x) * t + 0.5f) << s;
    }
    return out;
}

void sga_frame(SG *g, u32 ms) {
    SgaState *a = st(g);
    Field *f = &g->field;
    if (!a->started) sga_new_game(g);
    g->fadeLevel = 0;
    g->fadeTotal = 0;
    if (f->loaded && (a->mapPending || !guid_eq(a->mapId, f->map.id)) && !g->pendingTransfer) {
        bool fromScript = a->mapPending, sameMap = guid_eq(a->mapId, f->map.id);
        sga_map_loaded(g);
        if (fromScript && !sameMap) a->eventIndex = -1;
    }
    if (a->dirty) sga_refresh_events(g);
    if (a->fadeTotal) {
        a->fadeMs += ms;
        float t = a->fadeMs >= a->fadeTotal ? 1.0f : (float)a->fadeMs / (float)a->fadeTotal;
        a->fade = a->fadeFrom + (a->fadeTo - a->fadeFrom) * t;
        if (t >= 1.0f) a->fadeTotal = 0;
    }
    if (a->tintTotal) {
        a->tintMs += ms;
        float t = a->tintMs >= a->tintTotal ? 1.0f : (float)a->tintMs / (float)a->tintTotal;
        a->tint = lerp_color(a->tintFrom, a->tintTo, t);
        if (t >= 1.0f) a->tintTotal = 0;
    }
    tick(&a->flashMs, a->flashTotal, ms);
    tick(&a->shakeMs, a->shakeTotal, ms);
    tick(&a->balloonMs, a->balloonTotal, ms);
    if (a->balloonTotal && a->balloonMs >= a->balloonTotal) a->balloonTotal = 0;
    for (int i = 0; i < SGA_PICTURES; i++) {
        SgaPicture *p = &a->pics[i];
        if (!p->used || !p->total) continue;
        p->ms += ms;
        float t = p->ms >= p->total ? 1.0f : (float)p->ms / (float)p->total;
        p->alpha = p->alphaFrom + (p->alphaTo - p->alphaFrom) * t;
        if (t >= 1.0f) {
            p->total = 0;
            if (p->eraseAfter) *p = (SgaPicture){0};
        }
    }
    if (f->loaded) sga_step_moves(g, ms);
}

void sga_draw_under(SG *g) { (void)g; }

void sga_draw_over(SG *g) {
    SgaState *a = st(g);
    u32 *fb = g->fb;
    if (a->tint >> 24) fill_rect(fb, g->w, g->h, 0, 0, g->w, g->h, (a->tint & 0xff00ff00u) | (a->tint >> 16 & 255) | (a->tint & 255) << 16);
    for (int i = 0; i < SGA_PICTURES; i++) {
        SgaPicture *p = &a->pics[i];
        if (!p->used || !p->im || p->alpha <= 0) continue;
        int al = (int)(p->alpha * 255.0f + 0.5f);
        if (al > 255) al = 255;
        int x = (g->w - p->im->w) / 2 + (int)p->x, y = (g->h - p->im->h) / 2 + (int)p->y;
        blit(fb, g->w, g->h, p->im, 0, 0, p->im->w, p->im->h, x, y, p->im->w, p->im->h, al);
    }
    if (a->flashTotal && a->flashMs < a->flashTotal) {
        u32 al = (u32)(160.0f * (1.0f - (float)a->flashMs / (float)a->flashTotal));
        fill_rect(fb, g->w, g->h, 0, 0, g->w, g->h, al << 24 | 0xffffffu);
    }
    if (a->fade > 0) {
        u32 al = (u32)(a->fade * 255.0f + 0.5f);
        fill_rect(fb, g->w, g->h, 0, 0, g->w, g->h, al << 24);
    }
}

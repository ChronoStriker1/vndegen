#include "sg.h"

bool battle_map_info(SG *g, Guid mapId, BattleMapInfo *out) {
    memset(out, 0, sizeof *out);
    const SgrRec *rec = game_record(&g->data, SGR_MAP, 0, mapId, NULL);
    if (!rec) return false;
    Rd r = rd_make(rec->body, rec->size);
    rd_guid(&r);
    free(rd_str(&r));
    rd_i32(&r);
    free(rd_str(&r));
    rd_i64(&r);
    i32 n = rd_i32(&r);
    if (n < 0 || n > 100000) return false;
    rd_skip(&r, 24 * (size_t)n);
    out->rate = rd_i32(&r);
    n = rd_i32(&r);
    if (n < 0 || n > 1000) return false;
    for (int i = 0; i < n && !r.bad; i++) {
        i32 lo = rd_i32(&r), hi = rd_i32(&r);
        Guid m = rd_guid(&r);
        if (out->count < BATTLE_MAX_ENCOUNTERS) {
            out->entries[out->count].monster = m;
            out->entries[out->count].a = lo;
            out->entries[out->count].b = hi;
            out->count++;
        }
    }
    rd_skip(&r, 36);
    i32 w = rd_i32(&r), h = rd_i32(&r);
    if (r.bad || w <= 0 || h <= 0 || w > 4096 || h > 4096) return false;
    rd_skip(&r, 4 * (size_t)w * (size_t)h);
    n = rd_i32(&r);
    if (n < 0 || n > 100000) return false;
    rd_skip(&r, 28 * (size_t)n);
    n = rd_i32(&r);
    if (n < 0 || n > 100000) return false;
    rd_skip(&r, 16 * (size_t)n + 32);
    out->background = rd_guid(&r);
    rd_guid(&r);
    rd_guid(&r);
    out->bgm = rd_guid(&r);
    return !r.bad;
}

Guid battle_bgm_for(SG *g, const BattleSetup *s, Guid mapId) {
    if (!guid_zero(s->bgm) && res_find(&g->res, s->bgm)) return s->bgm;
    BattleMapInfo info;
    if (battle_map_info(g, mapId, &info) && !guid_zero(info.bgm) && res_find(&g->res, info.bgm)) return info.bgm;
    return sgb(g)->tables.text.battleBgm;
}

Guid battle_background_for(SG *g, const BattleSetup *s, Guid mapId) {
    if (!guid_zero(s->background)) return s->background;
    BattleMapInfo info;
    if (battle_map_info(g, mapId, &info)) return info.background;
    return (Guid){{0}};
}

static u32 enc_rand(BattleEnc *e) {
    u32 x = e->rng ? e->rng : 0x2545f491u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    e->rng = x;
    return x;
}

static void enc_next(BattleEnc *e) {
    int r = e->info.rate > 0 ? e->info.rate : 0;
    e->steps = 0;
    e->next = r * 3 / 4 + (int)(enc_rand(e) % (u32)(r / 2 + 1));
}

static void enc_load(SG *g, BattleEnc *e) {
    e->map = g->field.map.id;
    e->valid = battle_map_info(g, e->map, &e->info);
    e->cellX = (int)g->field.player.x;
    e->cellZ = (int)g->field.player.z;
    enc_next(e);
}

bool battle_encounter_roll(BattleEnc *e) {
    if (e->info.rate <= 0 || e->info.count <= 0) return false;
    if (e->next <= 0) enc_next(e);
    return ++e->steps >= e->next;
}

void battle_encounter_setup(BattleEnc *e, BattleSetup *s) {
    memset(s, 0, sizeof *s);
    const BattleEncounterEntry *en = &e->info.entries[enc_rand(e) % (u32)e->info.count];
    int lo = en->a > 0 ? en->a : 1, hi = en->b >= lo ? en->b : lo;
    int n = lo + (int)(enc_rand(e) % (u32)(hi - lo + 1));
    for (int i = 0; i < n && i < BATTLE_MAX_MONSTERS; i++) s->monsters[s->count++] = en->monster;
}

void battle_encounter_tick(SG *g) {
    BattleEnc *e = battle_enc(g);
    if (!e) return;
    if (g->scene != SCENE_FIELD || !g->field.loaded) {
        e->map = (Guid){{0}};
        return;
    }
    if (!guid_eq(e->map, g->field.map.id)) enc_load(g, e);
    Actor *p = &g->field.player;
    if (p->moving) return;
    int cx = (int)p->x, cz = (int)p->z;
    if (cx == e->cellX && cz == e->cellZ) return;
    e->cellX = cx;
    e->cellZ = cz;
    if (!e->valid || g->interp.active || menu_active(g)) return;
    if (!battle_encounter_roll(e)) return;
    BattleSetup s;
    battle_encounter_setup(e, &s);
    if (!s.count) return;
    sg_log(g, "encounter after %d steps", e->steps);
    enc_next(e);
    sg_battle_start(g, &s);
}

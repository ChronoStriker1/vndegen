#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sg.h"

static int failures, checks;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        checks++;                                                                                                      \
        if (!(cond)) {                                                                                                 \
            failures++;                                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                 \
        }                                                                                                              \
    } while (0)

enum { VK_UP = 0x26, VK_OK = 0x0d };

static void run(SG *g, int ms) {
    for (int t = 0; t < ms; t += 16) vnsmilegame_frame((VNSmileGame *)g, 16);
}

static void press(SG *g, int vk) {
    vnsmilegame_key((VNSmileGame *)g, vk, true);
    run(g, 48);
    vnsmilegame_key((VNSmileGame *)g, vk, false);
    run(g, 32);
}

static SG *new_game(const char *dir) {
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = "/nonexistent-sgtest-v", .audioRate = 44100};
    char err[512];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) {
        fprintf(stderr, "open %s: %s\n", dir, err);
        return NULL;
    }
    run(g, 16);
    if (g->titleCursor == 1) press(g, VK_UP);
    press(g, VK_OK);
    run(g, 200);
    interp_free(&g->interp);
    return g;
}

static const HeroDef *hero_named(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->heroCount; i++)
        if (!strcmp(t->heroes[i].name, name)) return &t->heroes[i];
    return NULL;
}

static const MonsterDef *monster_named(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->monsterCount; i++)
        if (!strcmp(t->monsters[i].name, name)) return &t->monsters[i];
    return NULL;
}

static Member *join(SG *g, const char *name, int level) {
    Party *p = &sgb(g)->party;
    const HeroDef *h = hero_named(g, name);
    CHECK(h != NULL);
    if (!h) return NULL;
    if (!party_has_hero(p, h->id)) party_add_hero(g, h->id);
    for (int i = 0; i < p->count; i++)
        if (guid_eq(p->members[i].hero, h->id)) {
            member_set_level(&sgb(g)->tables, &p->members[i], level);
            p->members[i].hp = member_max_hp(&sgb(g)->tables, &p->members[i]);
            return &p->members[i];
        }
    return NULL;
}

static Guid map_named(SG *g, const char *name) {
    for (int f = 0; f < g->data.mapCount; f++)
        for (int i = 0; i < g->data.maps[f].count; i++) {
            const SgrRec *r = &g->data.maps[f].recs[i];
            if (r->type != SGR_MAP) continue;
            Map m;
            memset(&m, 0, sizeof m);
            bool ok = map_parse(&m, r) && !strcmp(m.name, name);
            Guid id = m.id;
            map_free(&m);
            if (ok) return id;
        }
    return (Guid){{0}};
}

static bool battle_with(SG *g, Guid monster, BattleSetup *out) {
    for (int f = 0; f < g->data.mapCount; f++)
        for (int i = 0; i < g->data.maps[f].count; i++) {
            const SgrRec *r = &g->data.maps[f].recs[i];
            if (r->type != SGR_EVENT || r->sub != 16) continue;
            Script s;
            if (!script_parse(&s, r)) continue;
            bool found = false;
            for (int k = 0; k < s.count && !found; k++) {
                if (s.cmds[k].type != 65) continue;
                BattleSetup bs;
                memset(&bs, 0, sizeof bs);
                if (battle_setup_cmd(&bs, &s.cmds[k]) && bs.count > 0 && guid_eq(bs.monsters[0], monster)) {
                    *out = bs;
                    found = true;
                }
            }
            script_free(&s);
            if (found) return true;
        }
    return false;
}

static void wait_command(SG *g) {
    for (int t = 0; t < 20000 && strcmp(battle_phase(g), "command") != 0; t += 16) run(g, 16);
}

static void box_mean(SG *g, int x0, int y0, int w, int h, float out[3]) {
    double s[3] = {0};
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            u32 p = g->fb[(size_t)y * (size_t)g->w + (size_t)x];
            s[0] += p & 255;
            s[1] += (p >> 8) & 255;
            s[2] += (p >> 16) & 255;
        }
    for (int i = 0; i < 3; i++) out[i] = (float)(s[i] / (w * h));
}

static int dark_pixels(SG *g, int x0, int y0, int w, int h) {
    int n = 0;
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            u32 p = g->fb[(size_t)y * (size_t)g->w + (size_t)x];
            int l = (int)(((p >> 16) & 255) + ((p >> 8) & 255) + (p & 255));
            if (l < 3 * 140) n++;
        }
    return n;
}

static void test_skill_formula(void) {
    CHECK(battle_skill_damage(120, 1, 472) == 124);
    CHECK(battle_skill_damage(120, 1, 443) == 124);
    CHECK(battle_skill_damage(120, 1, 272) == 122);
    CHECK(battle_skill_damage(74, 4, 1100) * 2 == 236);
    CHECK(battle_skill_damage(80, 3, 1100) * 150 / 100 == 169);
    CHECK(battle_skill_damage(84, 6, 1100) * 150 / 100 == 225);
    CHECK(battle_skill_damage(39, 6, 1100) * 150 / 100 == 157);
    CHECK(battle_skill_damage(90, 6, 1100) == 156);
    CHECK(battle_skill_damage(34, 4, 279) == 45);
    CHECK(battle_skill_damage(34, 4, 283) == 45);
    CHECK(battle_skill_damage(27, 15, 279) == 68);
    CHECK(battle_skill_damage(70, 15, 283) == 112);
}

static void test_dragon_thunder(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    Party *p = &sgb(g)->party;
    p->count = 0;
    Member *aria = join(g, "アリア", 25), *roi = join(g, "ロイ", 26);
    const MonsterDef *dragon = monster_named(g, "竜王（予定）");
    CHECK(dragon && aria && roi);
    int casts = 0, exact = 0;
    for (int seed = 1; seed <= 8 && dragon && aria && roi; seed++) {
        for (int i = 0; i < p->count; i++) p->members[i].hp = member_max_hp(&sgb(g)->tables, &p->members[i]);
        BattleSetup bs = {0};
        bs.monsters[0] = dragon->id;
        bs.count = 1;
        bs.continueOnLose = true;
        battle_seed(g, (u32)seed * 104729u);
        sg_battle_start(g, &bs);
        bool thunder = false;
        int before[2] = {0};
        for (int k = 0; k < 1500 && strcmp(vnsmilegame_scene((VNSmileGame *)g), "battle") == 0; k++) {
            const char *bar = battle_bar(g);
            bool now = bar && !strcmp(bar, "ドラゴンサンダー");
            if (now && !thunder) {
                before[0] = p->members[0].hp;
                before[1] = p->members[1].hp;
            } else if (!now && thunder) {
                for (int i = 0; i < 2; i++)
                    if (before[i] > 124 && p->members[i].hp > 0 && p->members[i].hp < before[i]) {
                        casts++;
                        exact += before[i] - p->members[i].hp == 124;
                    }
            }
            thunder = now;
            if (!now) press(g, VK_OK);
            else run(g, 80);
        }
        run(g, 3000);
    }
    CHECK(casts > 0);
    CHECK(exact == casts);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_campfire(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    Party *p = &sgb(g)->party;
    p->count = 0;
    join(g, "アリア", 25);
    Member *roi = join(g, "ロイ", 26);
    const HeroDef *rh = hero_named(g, "ロイ");
    CHECK(rh && !res_find(&g->res, rh->graphic));
    const Res *knight = NULL;
    for (int i = 0; i < g->res.count && !knight; i++)
        if (strstr(g->res.items[i].path, "c_042a.fbx")) knight = &g->res.items[i];
    CHECK(knight != NULL);
    if (roi && rh && knight) party_set_look(p, rh->id, (Guid){{0}}, knight->id);
    CHECK(sg_debug_warp((VNSmileGame *)g, "フィールドマップ", 72, 25, 1));
    run(g, 300);
    const MonsterDef *dragon = monster_named(g, "竜王（予定）");
    BattleSetup bs;
    CHECK(dragon && battle_with(g, dragon->id, &bs));
    CHECK(guid_eq(bs.background, map_named(g, "Snowfield")));
    if (dragon) {
        battle_seed(g, 5);
        sg_battle_start(g, &bs);
        wait_command(g);
        run(g, 500);
        float c[3];
        box_mean(g, 240, 330, 20, 20, c);
        CHECK(c[0] >= 230 && c[1] >= 220 && c[2] >= 200 && c[0] > c[2] + 15);
        CHECK(dark_pixels(g, 590, 300, 70, 110) > 1500);
        battle_abort(g);
    }
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_cave(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    Party *p = &sgb(g)->party;
    p->count = 0;
    join(g, "アリア", 25);
    CHECK(sg_debug_warp((VNSmileGame *)g, "王家の洞窟", 26, 5, 3));
    run(g, 300);
    interp_free(&g->interp);
    BattleSetup bs = {0};
    bs.monsters[0] = monster_named(g, "ミニゴーレム")->id;
    bs.count = 1;
    CHECK(guid_eq(battle_background_for(g, &bs, g->field.map.id), map_named(g, "Cave")));
    battle_seed(g, 5);
    sg_battle_start(g, &bs);
    wait_command(g);
    run(g, 500);
    float c[3];
    box_mean(g, 440, 240, 20, 20, c);
    CHECK(fabsf(c[0] - 31) <= 6 && fabsf(c[1] - 26) <= 6 && fabsf(c[2] - 15) <= 6);
    battle_abort(g);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    const char *sacred = getenv("SG_SACRED");
    test_skill_formula();
    if (sacred) {
        test_dragon_thunder(sacred);
        test_campfire(sacred);
        test_cave(sacred);
    }
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

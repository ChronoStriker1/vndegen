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

static unsigned damage_set(int attack, int defense) {
    unsigned set = 0;
    u32 rng = 7;
    for (int i = 0; i < 5000; i++) {
        int d = battle_attack_damage(attack, defense, battle_spread(&rng));
        if (d >= 0 && d < 32) set |= 1u << d;
    }
    return set;
}

static unsigned bits(const int *v, int n) {
    unsigned s = 0;
    for (int i = 0; i < n; i++) s |= 1u << v[i];
    return s;
}

static void test_formulas(void) {
    CHECK(damage_set(42, 5) == bits((int[]){14, 15}, 2));
    CHECK(damage_set(54, 5) == bits((int[]){18, 19, 20}, 3));
    CHECK(damage_set(66, 5) == bits((int[]){22, 23, 24}, 3));
    CHECK(damage_set(25, 25) == bits((int[]){3}, 1));
    CHECK(damage_set(30, 35) == bits((int[]){2, 3}, 2));
    CHECK(damage_set(30, 45) == bits((int[]){0}, 1));
    CHECK(damage_set(30, 55) == bits((int[]){0}, 1));
    CHECK(battle_exp_for_level(1) == 0);
    CHECK(battle_exp_for_level(2) == 26);
    CHECK(battle_exp_for_level(3) == 84);
    CHECK(battle_exp_for_level(4) >= 182 && battle_exp_for_level(4) <= 186);
    CHECK(battle_exp_for_level(5) == 350);
    CHECK(battle_exp_for_level(6) == 600);
    CHECK(battle_exp_for_level(7) > 600);
    CHECK(exp_for_level(5) == 350);
    u32 rng = 1;
    int fails = 0;
    for (int i = 0; i < 300; i++) fails += !battle_escape_roll(&rng, 15, 40);
    CHECK(fails > 150);
}

static SG *open_game(const char *dir) {
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = "/nonexistent-sgtest-c", .audioRate = 44100};
    char err[256];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) fprintf(stderr, "open %s: %s\n", dir, err);
    return g;
}

static void step(SG *g, int ms) {
    for (int t = 0; t < ms; t += 16) vnsmilegame_frame((VNSmileGame *)g, 16);
}

static void press(SG *g, int vk) {
    vnsmilegame_key((VNSmileGame *)g, vk, true);
    step(g, 48);
    vnsmilegame_key((VNSmileGame *)g, vk, false);
    step(g, 32);
}

static bool find_battle(SG *g, const char *hex, BattleSetup *out) {
    for (int f = 0; f < g->data.mapCount; f++)
        for (int i = 0; i < g->data.maps[f].count; i++) {
            const SgrRec *r = &g->data.maps[f].recs[i];
            if (r->type != SGR_EVENT || r->sub != 16 || r->size < 16) continue;
            char str[33];
            for (int k = 0; k < 16; k++) snprintf(str + k * 2, 3, "%02x", r->body[k]);
            if (strncmp(str, hex, strlen(hex)) != 0) continue;
            Script sc;
            if (!script_parse(&sc, r)) return false;
            bool ok = false;
            for (int k = 0; k < sc.count && !ok; k++)
                if (sc.cmds[k].type == 65) ok = battle_setup_cmd(out, &sc.cmds[k]);
            script_free(&sc);
            return ok;
        }
    return false;
}

static const HeroDef *hero_named(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->heroCount; i++)
        if (strcmp(t->heroes[i].name, name) == 0) return &t->heroes[i];
    return NULL;
}

static const MonsterDef *monster_named(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->monsterCount; i++)
        if (strcmp(t->monsters[i].name, name) == 0) return &t->monsters[i];
    return NULL;
}

static void start_party(SG *g, const char *const *names, int n, int level) {
    press(g, 0x0d);
    interp_free(&g->interp);
    Party *p = &sgb(g)->party;
    p->count = 0;
    for (int i = 0; i < n; i++) {
        const HeroDef *h = hero_named(g, names[i]);
        CHECK(h != NULL);
        if (!h) continue;
        party_add_hero(g, h->id);
        Member *m = &p->members[p->count - 1];
        if (level) {
            m->level = level;
            m->exp = exp_for_level(level);
        }
        m->hp = member_max_hp(&sgb(g)->tables, m);
        m->mp = member_max_mp(&sgb(g)->tables, m);
    }
}

static void wait_phase(SG *g, const char *phase) {
    for (int t = 0; t < 2000 && sg_battle_active(g) && strcmp(battle_phase(g), phase) != 0; t++) step(g, 16);
}

static void test_sacred(const char *dir) {
    SG *g = open_game(dir);
    CHECK(g != NULL);
    if (!g) return;
    BattleSetup s;
    CHECK(find_battle(g, "c426b710", &s));
    CHECK(s.count == 1);
    CHECK(guid_eq(s.monsters[0], monster_named(g, "スライミー")->id));
    CHECK(s.continueOnLose);
    CHECK(s.noEscape);
    CHECK(guid_zero(s.background));
    CHECK(s.hasLayout && s.layout[0][0] == 0 && s.layout[0][1] == -3);

    start_party(g, (const char *const[]){"アリア"}, 1, 2);
    Member *aria = &sgb(g)->party.members[0];
    CHECK(aria->exp == 26);
    CHECK(member_attack(&sgb(g)->tables, aria) == 42);
    i32 money = sgb(g)->party.money;
    battle_seed(g, 12345);
    sg_battle_start(g, &s);
    CHECK(sg_battle_active(g));
    CHECK(strcmp(battle_bar(g), "モンスターが現れた！") == 0);
    wait_phase(g, "command");
    CHECK(strcmp(battle_phase(g), "command") == 0);
    press(g, 0x28);
    press(g, 0x28);
    press(g, 0x28);
    press(g, 0x0d);
    CHECK(strcmp(battle_phase(g), "command") == 0);
    press(g, 0x28);
    int hp = battle_monster_hp(g, 0);
    CHECK(hp == 30);
    int hits = 0;
    while (sg_battle_active(g) && hits < 6) {
        wait_phase(g, "command");
        if (strcmp(battle_phase(g), "command") != 0) break;
        press(g, 0x0d);
        CHECK(strcmp(battle_phase(g), "target") == 0);
        CHECK(strcmp(battle_bar(g), "スライミー") == 0);
        press(g, 0x0d);
        CHECK(strcmp(battle_bar(g), "アリアの攻撃！") == 0);
        step(g, 700);
        int now = battle_monster_hp(g, 0);
        int dealt = hp - now;
        CHECK(now == 0 || dealt == 14 || dealt == 15 || dealt == 0);
        hp = now;
        hits++;
    }
    wait_phase(g, "result");
    CHECK(strcmp(battle_phase(g), "result") == 0);
    press(g, 0x0d);
    CHECK(!sg_battle_active(g));
    CHECK(sg_battle_result(g) == BATTLE_WON);
    CHECK(aria->exp == 31);
    CHECK(aria->level == 2);
    CHECK(sgb(g)->party.money == money + 5);

    member_set_level(&sgb(g)->tables, aria, 4);
    aria->exp = 346;
    aria->hp = member_max_hp(&sgb(g)->tables, aria);
    CHECK(aria->hp == 85);
    sg_battle_start(g, &s);
    for (int n = 0; n < 6 && sg_battle_active(g); n++) {
        wait_phase(g, "command");
        if (strcmp(battle_phase(g), "command") != 0) break;
        press(g, 0x0d);
        press(g, 0x0d);
    }
    wait_phase(g, "result");
    press(g, 0x0d);
    CHECK(sg_battle_result(g) == BATTLE_WON);
    CHECK(aria->exp == 351);
    CHECK(aria->level == 5);
    CHECK(member_max_hp(&sgb(g)->tables, aria) == 100);
    CHECK(aria->hp == 100);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_colette(const char *dir) {
    SG *g = open_game(dir);
    CHECK(g != NULL);
    if (!g) return;
    BattleSetup s;
    CHECK(find_battle(g, "4ff8bc66", &s));
    CHECK(s.count == 1);
    CHECK(guid_eq(s.monsters[0], monster_named(g, "スライムＬＶ01")->id));
    CHECK(!s.continueOnLose && !s.noEscape && !s.hasLayout && !s.hasCenter);
    CHECK(!guid_zero(s.background));
    CHECK(find_battle(g, "5afb7767", &s));
    CHECK(s.count == 2 && s.noEscape && !s.continueOnLose);
    CHECK(s.hasLayout && s.layout[0][0] == -2 && s.layout[0][1] == -3 && s.layout[1][0] == 2 && s.layout[1][1] == -3);

    start_party(g, (const char *const[]){"ダーク", "コレット"}, 2, 0);
    Tables *t = &sgb(g)->tables;
    Member *dark = &sgb(g)->party.members[0], *colette = &sgb(g)->party.members[1];
    CHECK(member_max_hp(t, dark) == 32 && member_max_mp(t, dark) == 10);
    CHECK(member_max_hp(t, colette) == 29 && member_max_mp(t, colette) == 8);
    const MonsterDef *golem = monster_named(g, "ゴーレムＬＶ01");
    CHECK(damage_set(member_attack(t, dark), golem->stats[3]) == bits((int[]){3}, 1));
    CHECK(damage_set(member_attack(t, colette), golem->stats[3]) == bits((int[]){3}, 1));
    CHECK(damage_set(golem->stats[2], member_defense(t, dark)) & bits((int[]){26}, 1));
    CHECK(damage_set(golem->stats[2], member_defense(t, colette)) & bits((int[]){27}, 1));
    bool rock = false;
    u32 rng = 3;
    for (int i = 0; i < 5000; i++) rock |= battle_skill_damage(20, golem->stats[4], member_defense(t, colette), battle_spread(&rng)) == 21;
    CHECK(rock);

    BattleSetup gs = {0};
    gs.monsters[0] = golem->id;
    gs.count = 1;
    battle_seed(g, 99);
    sg_battle_start(g, &gs);
    wait_phase(g, "command");
    press(g, 0x0d);
    press(g, 0x0d);
    CHECK(strcmp(battle_phase(g), "command") == 0);
    press(g, 0x0d);
    press(g, 0x0d);
    CHECK(strcmp(battle_phase(g), "act") == 0);
    step(g, 32);
    const char *bar = battle_bar(g);
    CHECK(strstr(bar, "ゴーレム") != NULL || strcmp(bar, "落石ＬＶ01") == 0);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    test_formulas();
    const char *sacred = getenv("SG_SACRED"), *colette = getenv("SG_COLETTE");
    if (sacred) test_sacred(sacred);
    if (colette) test_colette(colette);
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

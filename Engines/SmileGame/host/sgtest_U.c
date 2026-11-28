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
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = "/nonexistent-sgtest-u", .audioRate = 44100};
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

static unsigned long long damage_set(int attack, int defense, bool crit) {
    unsigned long long set = 0;
    u32 rng = 11;
    for (int i = 0; i < 5000; i++) {
        float s = battle_spread(&rng);
        int d = crit ? battle_critical_damage(attack, defense, s) : battle_attack_damage(attack, defense, s);
        set |= 1ull << (d & 63);
    }
    return set;
}

static bool has_all(unsigned long long set, const int *v, int n) {
    for (int i = 0; i < n; i++)
        if (!(set >> (v[i] & 63) & 1)) return false;
    return true;
}

static void test_formulas(void) {
    CHECK(has_all(damage_set(350, battle_hero_defense(245, 26), false), (int[]){71, 72}, 2));
    CHECK(has_all(damage_set(350, battle_hero_defense(255, 25), false), (int[]){69, 70, 71, 72, 73, 74}, 6));
    CHECK(has_all(damage_set(350, battle_hero_defense(305, 26), false), (int[]){57, 58, 60, 61, 62}, 5));
    CHECK(has_all(damage_set(350, battle_hero_defense(360, 25), false), (int[]){45, 46, 47}, 3));
    CHECK(has_all(damage_set(260, battle_hero_defense(236, 25), false), (int[]){41, 42, 43, 44}, 4));
    CHECK(has_all(damage_set(260, battle_hero_defense(245, 24), false), (int[]){38, 39, 40, 41, 42}, 5));
    CHECK(has_all(damage_set(80, battle_hero_defense(15, 1), false), (int[]){26, 27}, 2));
    unsigned long long c245 = 0, c360 = 0, c280 = 0;
    u32 rng = 5;
    for (int i = 0; i < 5000; i++) {
        float s = battle_spread(&rng);
        int a = battle_critical_damage(350, 245, s), b = battle_critical_damage(350, 360, s), c = battle_critical_damage(350, 280, s);
        if (a >= 128) c245 |= 1ull << (a - 128);
        if (b >= 100 && b < 164) c360 |= 1ull << (b - 100);
        if (c >= 128) c280 |= 1ull << (c - 128);
    }
    CHECK((c245 >> (151 - 128) & 1) && (c245 >> (155 - 128) & 1) && (c245 >> (163 - 128) & 1));
    CHECK((c280 >> (145 - 128) & 1) && (c280 >> (154 - 128) & 1) && (c280 >> (155 - 128) & 1));
    CHECK(c360 >> (130 - 100) & 1);
}

static const HeroDef *hero_named(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->heroCount; i++)
        if (!strcmp(t->heroes[i].name, name)) return &t->heroes[i];
    return NULL;
}

static const ItemDef *item_named(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->itemCount; i++)
        if (!strcmp(t->items[i].name, name)) return &t->items[i];
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

static void test_status(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    Tables *t = &sgb(g)->tables;
    Member *roi = join(g, "ロイ", 26), *aria = join(g, "アリア", 25);
    if (roi && aria) {
        roi->stat[STAT_SPEED] = 144;
        roi->equip[SLOT_ACC2] = item_named(g, "風のブーツ")->id;
        CHECK(member_speed(t, roi) == 144);
        roi->equip[SLOT_ACC1] = item_named(g, "風のブーツ")->id;
        roi->equip[SLOT_ACC2] = item_named(g, "クリティカルネックレス")->id;
        CHECK(member_speed(t, roi) == 144);
        aria->equip[SLOT_WEAPON] = item_named(g, "バトルアックス")->id;
        aria->equip[SLOT_ACC2] = item_named(g, "命中の指輪")->id;
        CHECK(member_hit(t, aria) == 95);
        aria->equip[SLOT_ACC2] = (Guid){{0}};
        CHECK(member_hit(t, aria) == 75);
    }
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_dragon_critical(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    Party *p = &sgb(g)->party;
    p->count = 0;
    join(g, "アリア", 25);
    join(g, "ロイ", 26);
    const MonsterDef *dragon = monster_named(g, "竜王（予定）");
    CHECK(dragon != NULL);
    int crits = 0, attacks = 0;
    for (int seed = 1; seed <= 6 && dragon; seed++) {
        for (int i = 0; i < p->count; i++) p->members[i].hp = member_max_hp(&sgb(g)->tables, &p->members[i]);
        BattleSetup bs = {0};
        bs.monsters[0] = dragon->id;
        bs.count = 1;
        bs.continueOnLose = true;
        battle_seed(g, (u32)seed * 7919u);
        sg_battle_start(g, &bs);
        char last[256] = "";
        for (int k = 0; k < 1500 && strcmp(vnsmilegame_scene((VNSmileGame *)g), "battle") == 0; k++) {
            press(g, VK_OK);
            const char *bar = battle_bar(g);
            if (bar && strcmp(bar, last)) {
                snprintf(last, sizeof last, "%s", bar);
                if (!strcmp(bar, "竜王（予定）の会心の一撃！")) crits++;
                if (!strcmp(bar, "竜王（予定）の攻撃！")) attacks++;
            }
        }
        run(g, 3000);
    }
    CHECK(crits > 0);
    CHECK(attacks > crits);
    vnsmilegame_close((VNSmileGame *)g);
}

static void bottom_mean(SG *g, int y0, float out[3]) {
    double s[3] = {0};
    int n = 0;
    for (int y = y0; y < g->h; y++)
        for (int x = 0; x < g->w; x++) {
            u32 c = g->fb[(size_t)y * (size_t)g->w + (size_t)x];
            s[0] += c & 255;
            s[1] += c >> 8 & 255;
            s[2] += c >> 16 & 255;
            n++;
        }
    for (int i = 0; i < 3; i++) out[i] = n ? (float)(s[i] / n) : 0;
}

static void test_dark_background(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    float c[3];
    sg_debug_warp((VNSmileGame *)g, "ラストダンジョン", 24, 49, 0);
    run(g, 1500);
    interp_free(&g->interp);
    run(g, 100);
    bottom_mean(g, g->h - 20, c);
    CHECK(fabsf(c[0] - 20) <= 3 && fabsf(c[1] - 19) <= 3 && fabsf(c[2] - 20) <= 3);
    sg_debug_warp((VNSmileGame *)g, "城下町ダンジョン", 19, 38, 0);
    run(g, 48);
    bottom_mean(g, g->h - 30, c);
    CHECK(fabsf(c[0] - 140) <= 12 && fabsf(c[1] - 101) <= 12 && fabsf(c[2] - 183) <= 12);
    sg_debug_warp((VNSmileGame *)g, "パワースポット", 12, 22, 0);
    run(g, 1500);
    interp_free(&g->interp);
    run(g, 100);
    bottom_mean(g, g->h - 4, c);
    CHECK(c[0] >= 230 && c[1] >= 230 && c[2] >= 230);
    vnsmilegame_close((VNSmileGame *)g);
}

static bool choice_up(SG *g) { return g->interp.active && g->interp.state == 4; }

static void window_box(SG *g, int box[4]) {
    box[0] = box[1] = 1 << 20;
    box[2] = box[3] = -1;
    for (int y = 354; y < g->h; y++)
        for (int x = 0; x < g->w; x++) {
            u32 c = g->fb[(size_t)y * (size_t)g->w + (size_t)x];
            if ((c & 255) > 178 && (c >> 8 & 255) > 178 && (c >> 16 & 255) > 178) {
                if (x < box[0]) box[0] = x;
                if (y < box[1]) box[1] = y;
                if (x > box[2]) box[2] = x;
                if (y > box[3]) box[3] = y;
            }
        }
}

static void test_choice_window(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    g->vars[1] = 17;
    sg_debug_warp((VNSmileGame *)g, "地下牢", 7, 9, 0);
    run(g, 1500);
    for (int i = 0; i < 6 && !choice_up(g); i++) {
        press(g, VK_UP);
        press(g, VK_OK);
        run(g, 500);
    }
    CHECK(choice_up(g));
    CHECK(g->interp.a.closeWindow == 0);
    int box[4];
    window_box(g, box);
    CHECK(box[2] < 0);
    press(g, VK_OK);
    run(g, 1500);
    for (int i = 0; i < 6 && !choice_up(g); i++) {
        press(g, VK_OK);
        run(g, 300);
    }
    CHECK(choice_up(g));
    CHECK(g->interp.a.closeWindow != 0);
    window_box(g, box);
    int w = box[2] - box[0] + 1, h = box[3] - box[1] + 1;
    CHECK(w >= 640 && w <= 700 && h >= 140 && h <= 156);
    CHECK(abs((box[0] + box[2]) / 2 - 480) <= 3);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    const char *colette = getenv("SG_COLETTE"), *sacred = getenv("SG_SACRED");
    test_formulas();
    if (sacred) {
        test_status(sacred);
        test_dragon_critical(sacred);
        test_dark_background(sacred);
    }
    if (colette) test_choice_window(colette);
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

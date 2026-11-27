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

static SG *open_game(const char *dir) {
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = "/nonexistent-sgtest-n", .audioRate = 44100};
    char err[512];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) fprintf(stderr, "open %s: %s\n", dir, err);
    return g;
}

static void run(SG *g, int ms) {
    for (int t = 0; t < ms; t += 16) vnsmilegame_frame((VNSmileGame *)g, 16);
}

static void press(SG *g, int vk) {
    vnsmilegame_key((VNSmileGame *)g, vk, true);
    run(g, 48);
    vnsmilegame_key((VNSmileGame *)g, vk, false);
    run(g, 48);
}

static SG *new_game(const char *dir) {
    SG *g = open_game(dir);
    if (!g) return NULL;
    run(g, 16);
    if (g->titleCursor == 1) press(g, VK_UP);
    press(g, VK_OK);
    run(g, 200);
    interp_free(&g->interp);
    return g;
}

static bool showing(SG *g, const char *text) {
    Interp *ip = &g->interp;
    return ip->active && ip->state == 1 && ip->page < ip->pageCount && strstr(ip->pages[ip->page], text);
}

static void test_exp_curve(void) {
    static const struct { int level, exp; } pc[] = {
        {2, 26}, {3, 84}, {4, 186}, {5, 350}, {6, 600}, {7, 966}, {8, 1484}, {9, 2196}, {10, 3150}, {11, 4400},
        {12, 6006}, {30, 200100}, {40, 627900}, {50, 1531250}, {51, 1651250}, {55, 2131250}, {60, 2731250},
        {70, 3931250}, {80, 5131250}, {90, 6331250}, {99, 7411250},
    };
    CHECK(battle_exp_for_level(1) == 0);
    for (size_t i = 0; i < sizeof pc / sizeof pc[0]; i++) {
        if (battle_exp_for_level(pc[i].level) != pc[i].exp)
            fprintf(stderr, "level %d: %d, PC %d\n", pc[i].level, battle_exp_for_level(pc[i].level), pc[i].exp);
        CHECK(battle_exp_for_level(pc[i].level) == pc[i].exp);
    }
}

static void test_pier_inn_keeps_facing(const char *dir) {
    SG *g = new_game(dir);
    if (!g) {
        CHECK(false);
        return;
    }
    Party *p = &g->b.party;
    p->money = 80;
    p->members[0].hp = 1;
    sg_debug_warp((VNSmileGame *)g, "桟橋", 6, 16, 0);
    run(g, 1500);
    for (int i = 0; i < 10 && g->interp.active; i++) press(g, VK_OK), run(g, 300);
    CHECK(g->field.player.dir == 0);
    press(g, VK_OK);
    for (int i = 0; i < 20 && !menu_active(g); i++) press(g, VK_OK), run(g, 200);
    run(g, 300);
    CHECK(menu_page(g) && strcmp(menu_page(g), "inn") == 0);
    press(g, VK_OK);
    CHECK(p->money == 30);
    run(g, 12000);
    CHECK(!menu_active(g));
    run(g, 300);
    CHECK(showing(g, "おはようございます"));
    CHECK(p->members[0].hp == member_max_hp(&g->b.tables, &p->members[0]));
    CHECK(g->field.player.dir == 0);
    vnsmilegame_close((VNSmileGame *)g);
}

static int group_sizes(SG *g, const char *name, int *seen) {
    const Tables *t = &g->b.tables;
    BattleEnc e;
    memset(&e, 0, sizeof e);
    for (int i = 0; i < t->monsterCount; i++)
        if (strcmp(t->monsters[i].name, name) == 0) e.info.entries[0].monster = t->monsters[i].id;
    e.info.entries[0].a = e.info.entries[0].b = 1;
    e.info.count = 1;
    e.info.rate = 40;
    int most = 0;
    for (int k = 0; k < 400; k++) {
        BattleSetup s;
        battle_encounter_setup(g, &e, &s);
        if (s.count > most) most = s.count;
        if (s.count >= 1 && s.count <= 8) seen[s.count]++;
    }
    return most;
}

static void test_encounter_groups(const char *dir) {
    SG *g = open_game(dir);
    if (!g) {
        CHECK(false);
        return;
    }
    int seen[9] = {0};
    CHECK(group_sizes(g, "ベスライミー", seen) == 5);
    for (int n = 1; n <= 5; n++) CHECK(seen[n] > 0);
    memset(seen, 0, sizeof seen);
    CHECK(group_sizes(g, "スライミー", seen) == 2);
    CHECK(seen[1] > 0 && seen[2] > 0);
    memset(seen, 0, sizeof seen);
    CHECK(group_sizes(g, "ミニゴーレム", seen) == 1);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    test_exp_curve();
    const char *sacred = getenv("SG_SACRED");
    if (sacred) {
        test_pier_inn_keeps_facing(sacred);
        test_encounter_groups(sacred);
    } else
        printf("SG_SACRED not set: Sacred checks skipped\n");
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

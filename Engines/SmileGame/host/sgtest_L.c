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

enum { VK_UP = 0x26, VK_DOWN = 0x28, VK_RIGHT = 0x27, VK_OK = 0x0d, VK_CANCEL = 0x1b };

static SG *open_game(const char *dir) {
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = "/nonexistent-sgtest-l", .audioRate = 44100};
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

static void region(SG *g, int x0, int y0, int w, int h, double *r, double *gr, double *b) {
    double sr = 0, sg = 0, sb = 0;
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            u32 c = g->fb[y * g->w + x];
            sr += c & 255, sg += c >> 8 & 255, sb += c >> 16 & 255;
        }
    double n = (double)w * h;
    *r = sr / n, *gr = sg / n, *b = sb / n;
}

static double luma(SG *g) {
    double r, gg, b;
    region(g, 0, 0, g->w, g->h, &r, &gg, &b);
    return (r + gg + b) / 3;
}

static void to_innkeeper(SG *g) {
    sg_debug_warp((VNSmileGame *)g, "宿屋", 15, 15, 0);
    run(g, 1500);
    press(g, VK_OK);
    for (int i = 0; i < 20 && !menu_active(g); i++) press(g, VK_OK), run(g, 200);
    run(g, 300);
}

static void test_inn_stay(const char *dir) {
    SG *g = new_game(dir);
    if (!g) {
        CHECK(false);
        return;
    }
    Party *p = &g->b.party;
    p->money = 77;
    g->vars[8] = 1;
    p->members[0].hp = 1;
    to_innkeeper(g);
    CHECK(menu_page(g) && strcmp(menu_page(g), "inn") == 0);
    double before = luma(g);
    press(g, VK_OK);
    CHECK(p->money == 27);
    run(g, 3000);
    CHECK(luma(g) < 2.0 && before > 60.0);
    CHECK(menu_active(g));
    run(g, 4500);
    CHECK(luma(g) < 2.0);
    run(g, 3500);
    CHECK(!menu_active(g));
    run(g, 300);
    CHECK(showing(g, "おはようございます"));
    CHECK(p->members[0].hp == member_max_hp(&g->b.tables, &p->members[0]));
    CHECK(g->field.player.dir == 2);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_inn_no_money(const char *dir) {
    SG *g = new_game(dir);
    if (!g) {
        CHECK(false);
        return;
    }
    Party *p = &g->b.party;
    p->money = 27;
    g->vars[8] = 1;
    to_innkeeper(g);
    press(g, VK_OK);
    run(g, 500);
    CHECK(p->money == 27 && menu_active(g));
    press(g, VK_OK);
    run(g, 600);
    CHECK(showing(g, "そうですか"));
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_inn_bed(const char *dir) {
    SG *g = new_game(dir);
    if (!g) {
        CHECK(false);
        return;
    }
    sg_debug_warp((VNSmileGame *)g, "宿屋", 15, 15, 2);
    run(g, 1500);
    double r, gg, b;
    region(g, 240, 120, 40, 30, &r, &gg, &b);
    CHECK(r > 0 && gg / r > 0.76);
    region(g, 110, 120, 40, 30, &r, &gg, &b);
    CHECK(r > 0 && gg / r < 0.76);
    vnsmilegame_close((VNSmileGame *)g);
}

static void step(SG *g, int vk) {
    vnsmilegame_key((VNSmileGame *)g, vk, true);
    for (int t = 0; t < 1000 && !g->field.player.moving; t += 16) run(g, 16);
    vnsmilegame_key((VNSmileGame *)g, vk, false);
    for (int t = 0; t < 1000 && g->field.player.moving; t += 16) run(g, 16);
    run(g, 100);
}

static void warp(SG *g, const char *map, int x, int z, int dir) {
    sg_debug_warp((VNSmileGame *)g, map, x, z, dir);
    run(g, 1500);
    for (int i = 0; i < 20 && g->interp.active; i++) press(g, VK_OK), run(g, 300);
}

static bool at(SG *g, int x, int z) { return (int)g->field.player.x == x && (int)g->field.player.z == z; }

static void test_bridges(const char *dir) {
    SG *g = new_game(dir);
    if (!g) {
        CHECK(false);
        return;
    }
    g->switches[69] = 1;
    for (int i = 30; i <= 36; i++) g->switches[i] = 1;
    warp(g, "コムコムの穴Ｂ02Ｆ", 11, 15, 1);
    for (int i = 0; i < 4; i++) step(g, VK_RIGHT);
    CHECK(at(g, 15, 15));
    warp(g, "コムコムの穴Ｂ03Ｆ", 9, 16, 0);
    for (int i = 0; i < 4; i++) step(g, VK_UP);
    CHECK(at(g, 9, 12));
    warp(g, "コムコムの穴Ｂ03Ｆ", 6, 15, 0);
    step(g, VK_UP);
    CHECK(at(g, 6, 15));
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    const char *colette = getenv("SG_COLETTE");
    if (colette) {
        test_inn_stay(colette);
        test_inn_no_money(colette);
        test_inn_bed(colette);
        test_bridges(colette);
    }
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

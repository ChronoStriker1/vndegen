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

enum { VK_UP = 0x26, VK_DOWN = 0x28, VK_LEFT = 0x25, VK_RIGHT = 0x27, VK_OK = 0x0d };

static void run(SG *g, int ms) {
    for (int t = 0; t < ms; t += 16) vnsmilegame_frame((VNSmileGame *)g, 16);
}

static void press(SG *g, int vk, int ms) {
    vnsmilegame_key((VNSmileGame *)g, vk, true);
    run(g, ms);
    vnsmilegame_key((VNSmileGame *)g, vk, false);
    run(g, 400);
}

static SG *new_game(const char *dir) {
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = "/nonexistent-sgtest-t", .audioRate = 44100};
    char err[512];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) {
        fprintf(stderr, "open %s: %s\n", dir, err);
        return NULL;
    }
    run(g, 16);
    if (g->titleCursor == 1) press(g, VK_UP, 48);
    press(g, VK_OK, 48);
    run(g, 200);
    interp_free(&g->interp);
    return g;
}

static void warp(SG *g, const char *map, int x, int z, int dir) {
    sg_debug_warp((VNSmileGame *)g, map, x, z, dir);
    run(g, 1500);
}

static int cell_x(SG *g) { return (int)floorf(g->field.player.x); }
static int cell_z(SG *g) { return (int)floorf(g->field.player.z); }

static bool on_map(SG *g, const char *name) { return g->field.loaded && g->field.map.name && !strcmp(g->field.map.name, name); }

static void test_escape_gate_keeps_lap(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    g->vars[10] = 2;
    warp(g, "幽玄の塔01Ｆ", 26, 2, 0);
    for (int i = 0; i < 10 && strcmp(vnsmilegame_scene((VNSmileGame *)g), "field"); i++) press(g, VK_OK, 48);
    CHECK(on_map(g, "幽玄の塔01Ｆ") && cell_x(g) == 26 && cell_z(g) == 2);
    press(g, VK_UP, 300);
    for (int i = 0; i < 20 && !(g->interp.active && g->interp.state == 4); i++) run(g, 100);
    CHECK(g->interp.active && g->interp.state == 4 && g->interp.a.choiceCount == 2 && !strcmp(g->interp.a.choices[0], "脱出する"));
    press(g, VK_OK, 48);
    run(g, 3000);
    CHECK(on_map(g, "ワールド"));
    CHECK(cell_x(g) == 40 && cell_z(g) == 11);
    CHECK(g->vars[10] == 2);
    vnsmilegame_close((VNSmileGame *)g);
}

static const char *page_text(SG *g) {
    Interp *ip = &g->interp;
    return ip->active && ip->state == 1 && ip->page < ip->pageCount ? ip->pages[ip->page] : NULL;
}

static void test_prison_blank_last_line(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    g->vars[1] = 17;
    warp(g, "地下牢", 7, 9, 0);
    for (int i = 0; i < 20 && !(g->interp.active && g->interp.state == 4); i++) run(g, 100);
    CHECK(g->interp.active && g->interp.state == 4 && g->interp.a.choiceCount == 3 && !strcmp(g->interp.a.choices[1], "陵辱する（騎乗位）"));
    press(g, VK_DOWN, 48);
    press(g, VK_OK, 48);
    const char *t = NULL;
    for (int i = 0; i < 10; i++) {
        t = page_text(g);
        if (t && !strncmp(t, "さっさと脱いで", strlen("さっさと脱いで"))) break;
        run(g, 3000);
        press(g, VK_OK, 48);
    }
    CHECK(t && !strncmp(t, "さっさと脱いで", strlen("さっさと脱いで")));
    run(g, 3000);
    press(g, VK_OK, 48);
    t = page_text(g);
    CHECK(t && !t[0]);
    press(g, VK_OK, 48);
    run(g, 1500);
    t = page_text(g);
    CHECK(t && !strncmp(t, "コレットはまたがったまま", strlen("コレットはまたがったまま")));
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    const char *colette = getenv("SG_COLETTE");
    if (colette) {
        test_escape_gate_keeps_lap(colette);
        test_prison_blank_last_line(colette);
    }
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

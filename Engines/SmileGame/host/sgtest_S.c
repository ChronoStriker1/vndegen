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

enum { VK_OK = 0x0d, VK_ESC = 0x1b, VK_LEFT = 0x25, VK_UP = 0x26, VK_RIGHT = 0x27, VK_DOWN = 0x28 };

static SG *open_game(const char *dir) {
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = "/nonexistent-sgtest-s", .audioRate = 44100, .logPath = getenv("SGLOG")};
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

static void step(SG *g, int vk) {
    vnsmilegame_key((VNSmileGame *)g, vk, true);
    run(g, 160);
    vnsmilegame_key((VNSmileGame *)g, vk, false);
    run(g, 400);
}

static int px(SG *g) { return (int)floorf(g->field.player.x); }
static int pz(SG *g) { return (int)floorf(g->field.player.z); }

static void finish_events(SG *g) {
    for (int i = 0; i < 600 && g->interp.active; i++) {
        press(g, VK_OK);
        run(g, 100);
    }
}

static SG *started(const char *dir) {
    SG *g = open_game(dir);
    if (!g) return NULL;
    run(g, 32);
    press(g, VK_OK);
    run(g, 200);
    interp_free(&g->interp);
    return g;
}

static void test_keys_follow_camera(const char *dir) {
    SG *g = started(dir);
    CHECK(g != NULL);
    if (!g) return;
    CHECK(sg_debug_warp((VNSmileGame *)g, "CG・回想ルーム", 11, 14, 0));
    run(g, 1500);
    step(g, VK_UP);
    CHECK(px(g) == 11 && pz(g) == 14);
    step(g, VK_DOWN);
    CHECK(px(g) == 12 && pz(g) == 14);
    step(g, VK_LEFT);
    CHECK(px(g) == 12 && pz(g) == 15);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_cg_viewer(const char *dir) {
    SG *g = started(dir);
    CHECK(g != NULL);
    if (!g) return;
    CHECK(sg_debug_warp((VNSmileGame *)g, "CG・回想ルーム", 18, 9, 0));
    run(g, 1500);
    step(g, VK_RIGHT);
    CHECK(g->interp.active);
    finish_events(g);
    CHECK(px(g) == 18 && pz(g) == 7);
    CHECK(g->vars[10] == 1);
    step(g, VK_LEFT);
    finish_events(g);
    CHECK(g->vars[10] == 30);
    CHECK(px(g) == 18 && pz(g) == 7);
    press(g, VK_ESC);
    run(g, 400);
    CHECK(menu_page(g) == NULL);
    step(g, VK_RIGHT);
    finish_events(g);
    CHECK(g->vars[10] == 1);
    CHECK(px(g) == 18 && pz(g) == 7);
    step(g, VK_DOWN);
    finish_events(g);
    run(g, 600);
    CHECK(px(g) == 18 && pz(g) == 9);
    press(g, VK_ESC);
    run(g, 400);
    CHECK(menu_page(g) != NULL);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_walk_stops_at_dragon(const char *dir) {
    SG *g = started(dir);
    CHECK(g != NULL);
    if (!g) return;
    static const int after[] = {10, 20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 51, 52, 62};
    for (size_t i = 0; i < sizeof after / sizeof after[0]; i++) g->switches[after[i]] = true;
    CHECK(sg_debug_warp((VNSmileGame *)g, "城下町ルディアノ", 19, 11, 2));
    for (int i = 0; i < 100 && !g->interp.active; i++) run(g, 50);
    CHECK(g->interp.active);
    for (int i = 0; i < 100 && g->interp.state != 1; i++) run(g, 50);
    CHECK(px(g) == 19 && pz(g) == 15);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_no_border_under_noon_sky(const char *dir) {
    SG *g = started(dir);
    CHECK(g != NULL);
    if (!g) return;
    CHECK(sg_debug_warp((VNSmileGame *)g, "パワースポット", 11, 21, 0));
    run(g, 600);
    CHECK(sg_view(g)->look.sky == 0 && sg_view(g)->look.sun && sg_view(g)->border == 0);
    CHECK(sg_debug_warp((VNSmileGame *)g, "王家の洞窟", 15, 21, 0));
    run(g, 600);
    CHECK(!sg_view(g)->look.sun && sg_view(g)->border > 0);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    const char *sacred = getenv("SG_SACRED");
    if (sacred) {
        test_keys_follow_camera(sacred);
        test_cg_viewer(sacred);
        test_walk_stops_at_dragon(sacred);
        test_no_border_under_noon_sky(sacred);
    } else
        printf("SG_SACRED not set: Sacred checks skipped\n");
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

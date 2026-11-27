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

enum { VK_UP = 0x26, VK_LEFT = 0x25, VK_RIGHT = 0x27, VK_OK = 0x0d };

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
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = "/nonexistent-sgtest-o", .audioRate = 44100};
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

static void rgb(SG *g, int x, int y, int *r, int *gr, int *b) {
    u32 c = g->fb[y * g->w + x];
    *r = (int)(c & 0xff), *gr = (int)(c >> 8 & 0xff), *b = (int)(c >> 16 & 0xff);
}

static void test_edge_faces(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    warp(g, "ヤーマ岬01", 15, 27, 0);
    run(g, 300);
    int r, gr, b;
    rgb(g, 405, 500, &r, &gr, &b);
    CHECK(r > gr && gr > b && r > 60 && r < 160);
    rgb(g, 200, 530, &r, &gr, &b);
    CHECK(b > r + 40 && b < 170);
    rgb(g, 200, 480, &r, &gr, &b);
    CHECK(b > 190 && r < 80);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_water_blocks_player(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    for (int i = 34; i <= 36; i++) g->switches[i] = 1;
    warp(g, "ヤーマ岬03", 6, 21, 1);
    CHECK(strcmp(vnsmilegame_scene((VNSmileGame *)g), "field") == 0);
    press(g, VK_RIGHT, 600);
    CHECK(cell_x(g) == 6 && cell_z(g) == 21);
    press(g, VK_LEFT, 200);
    CHECK(cell_x(g) == 5 && cell_z(g) == 21);
    CHECK(strcmp(vnsmilegame_scene((VNSmileGame *)g), "field") == 0);
    warp(g, "コムコムの穴Ｂ04Ｆ", 13, 6, 1);
    for (int i = 0; i < 10 && strcmp(vnsmilegame_scene((VNSmileGame *)g), "field"); i++) press(g, VK_OK, 48);
    CHECK(strcmp(vnsmilegame_scene((VNSmileGame *)g), "field") == 0);
    press(g, VK_RIGHT, 600);
    CHECK(cell_x(g) == 13 && cell_z(g) == 6);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    const char *colette = getenv("SG_COLETTE");
    if (colette) {
        test_edge_faces(colette);
        test_water_blocks_player(colette);
    }
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

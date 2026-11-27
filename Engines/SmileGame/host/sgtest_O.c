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

static void test_fire_particles(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    warp(g, "ゴーレム洞窟Ｂ01Ｆ", 23, 5, 3);
    for (int i = 0; i < 10 && strcmp(vnsmilegame_scene((VNSmileGame *)g), "field"); i++) press(g, VK_OK, 48);
    run(g, 1000);
    int fire = 0;
    for (int y = 330; y < 470; y++)
        for (int x = 410; x < 550; x++) {
            int r, gr, b;
            rgb(g, x, y, &r, &gr, &b);
            if (r > 200 && b < 110 && r > gr + 40) fire++;
        }
    CHECK(fire > 800);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_switch_after_transfer(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    Tables *t = &sgb(g)->tables;
    Party *p = &sgb(g)->party;
    for (int i = 0; i < p->count; i++) {
        member_set_level(t, &p->members[i], 40);
        p->members[i].hp = member_max_hp(t, &p->members[i]);
    }
    warp(g, "ゴーレム洞窟Ｂ02Ｆ", 4, 11, 0);
    for (int i = 0; i < 10 && strcmp(vnsmilegame_scene((VNSmileGame *)g), "field"); i++) press(g, VK_OK, 48);
    Guid golem = g->field.npcs[1].eventId;
    CHECK(g->field.npcs[1].visible && !sg_local_switch(g, golem, 0));
    press(g, VK_UP, 200);
    for (int i = 0; i < 400 && !(g->field.loaded && strcmp(g->field.map.name, "ゴーレム洞窟Ｂ02Ｆ") == 0 && !g->interp.active && i > 20); i++) {
        if (sg_battle_active(g) && strcmp(battle_phase(g), "command") == 0) press(g, VK_OK, 48);
        else if (strcmp(vnsmilegame_scene((VNSmileGame *)g), "message") == 0 || strcmp(vnsmilegame_scene((VNSmileGame *)g), "scroll") == 0 || strcmp(vnsmilegame_scene((VNSmileGame *)g), "battle") == 0) press(g, VK_OK, 48);
        else run(g, 200);
    }
    CHECK(strcmp(g->field.map.name, "ゴーレム洞窟Ｂ02Ｆ") == 0);
    CHECK(sg_local_switch(g, golem, 0));
    CHECK(!g->field.npcs[1].visible);
    vnsmilegame_close((VNSmileGame *)g);
}

static int dark_bottom(SG *g) {
    int bottom = -1;
    for (int y = 280; y < 440; y++)
        for (int x = 250; x < 400; x++) {
            int r, gr, b;
            rgb(g, x, y, &r, &gr, &b);
            if (r < 40 && gr < 40 && b < 45) bottom = y;
        }
    return bottom;
}

static void test_front_row(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    Tables *t = &sgb(g)->tables;
    const char *names[3] = {"コレット", "メイリ", "サキュバス"};
    for (int k = 0; k < 3; k++)
        for (int i = 0; i < t->heroCount; i++)
            if (!strcmp(t->heroes[i].name, names[k])) party_add_hero(g, t->heroes[i].id);
    warp(g, "ワールド", 6, 14, 0);
    int slime = -1;
    for (int i = 0; i < g->field.npcCount; i++)
        if (!strcmp(g->field.events[i].name, "歩行-話すとバトル（逃げられる）_2_2_3_4_2_2")) slime = i;
    CHECK(slime >= 0);
    if (slime >= 0) {
        const SgrRec *sr = game_record(&g->data, SGR_EVENT, 16, g->field.events[slime].sheets[0].script, NULL);
        Script sc;
        BattleSetup setup = {0};
        bool ok = false;
        if (sr && script_parse(&sc, sr)) {
            for (int k = 0; k < sc.count && !ok; k++)
                if (sc.cmds[k].type == 65) ok = battle_setup_cmd(&setup, &sc.cmds[k]);
            script_free(&sc);
        }
        CHECK(ok);
        if (ok) {
            sg_battle_start(g, &setup);
            for (int ms = 0; ms < 15000 && strcmp(battle_phase(g), "command") != 0; ms += 16) vnsmilegame_frame((VNSmileGame *)g, 16);
            int bottom = dark_bottom(g);
            CHECK(bottom >= 416 && bottom <= 432);
        }
    }
    vnsmilegame_close((VNSmileGame *)g);
}

static int bubble_pixels(SG *g) {
    int n = 0;
    for (int y = 200; y < 240; y++)
        for (int x = 455; x < 505; x++) {
            int r, gr, b;
            rgb(g, x, y, &r, &gr, &b);
            if (b > 150 && r > 100 && gr < 140 && b - gr > 60) n++;
        }
    return n;
}

static void test_poison_bubbles(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    Party *p = &sgb(g)->party;
    for (int i = 0; i < p->count; i++) p->members[i].state = 1;
    warp(g, "NO NAME", 14, 7, 0);
    int shown = 0, hidden = 0;
    for (int i = 0; i < 60; i++) {
        run(g, 64);
        if (bubble_pixels(g) > 40) shown++;
        else hidden++;
    }
    CHECK(shown > 25 && hidden > 10);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_vertex_colors(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    Model *m = NULL;
    for (int i = 0; i < g->res.count && !m; i++)
        if (!strcmp(g->res.items[i].name, "Bookshelf")) m = model_load(g, g->res.items[i].path);
    CHECK(m != NULL);
    if (m) {
        int colored = 0, chroma = 0;
        for (int i = 0; i < m->mesh.count; i++) {
            const u16 *c = m->mesh.tris[i].color;
            if (!c[3]) continue;
            colored++;
            int mx = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]);
            int mn = c[0] < c[1] ? (c[0] < c[2] ? c[0] : c[2]) : (c[1] < c[2] ? c[1] : c[2]);
            if (mx - mn > 30) chroma++;
        }
        CHECK(colored > 50);
        CHECK(chroma > 5);
        model_free(m);
    }
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    const char *colette = getenv("SG_COLETTE");
    if (colette) {
        test_edge_faces(colette);
        test_water_blocks_player(colette);
        test_fire_particles(colette);
        test_switch_after_transfer(colette);
        test_front_row(colette);
        test_poison_bubbles(colette);
        test_vertex_colors(colette);
    }
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

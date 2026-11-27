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

enum { VK_UP = 0x26, VK_DOWN = 0x28, VK_LEFT = 0x25, VK_OK = 0x0d };

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
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = "/nonexistent-sgtest-m", .audioRate = 44100};
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

static void warp(SG *g, const char *map, int x, int z, int dir) {
    sg_debug_warp((VNSmileGame *)g, map, x, z, dir);
    run(g, 1500);
}

static void add_hero(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->heroCount; i++)
        if (strcmp(t->heroes[i].name, name) == 0) {
            party_add_hero(g, t->heroes[i].id);
            Member *m = &sgb(g)->party.members[sgb(g)->party.count - 1];
            m->hp = member_max_hp(t, m);
            m->mp = member_max_mp(t, m);
        }
}

static Guid item_named(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->itemCount; i++)
        if (strcmp(t->items[i].name, name) == 0) return t->items[i].id;
    return (Guid){{0}};
}

static int item_count(SG *g, const char *name) {
    Party *p = &sgb(g)->party;
    Guid id = item_named(g, name);
    for (int i = 0; i < p->itemCount; i++)
        if (guid_eq(p->items[i].item, id)) return p->items[i].count;
    return 0;
}

static int event_named(SG *g, const char *name) {
    for (int i = 0; i < g->field.npcCount; i++)
        if (g->field.events[i].name && strcmp(g->field.events[i].name, name) == 0) return i;
    return -1;
}

static void finish_event(SG *g) {
    for (int i = 0; i < 60 && g->interp.active; i++) {
        if (g->interp.state == 1) press(g, VK_OK);
        run(g, 150);
    }
}

static void test_talk_while_stepping(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    add_hero(g, "メイリ");
    warp(g, "ワールド", 6, 16, 0);
    int wall = event_named(g, "イベント4_2");
    CHECK(wall >= 0 && g->field.npcs[wall].visible);
    vnsmilegame_key((VNSmileGame *)g, VK_UP, true);
    bool started = false;
    for (int t = 0; t < 1500 && !started; t += 16) {
        bool moving = g->field.player.moving;
        if (moving) vnsmilegame_key((VNSmileGame *)g, VK_OK, true);
        vnsmilegame_frame((VNSmileGame *)g, 16);
        vnsmilegame_key((VNSmileGame *)g, VK_OK, false);
        started = g->interp.active;
        if (started) CHECK(moving);
    }
    vnsmilegame_key((VNSmileGame *)g, VK_UP, false);
    CHECK(started && g->interp.a.eventIndex == wall);
    finish_event(g);
    run(g, 1000);
    CHECK(wall >= 0 && !g->field.npcs[wall].visible);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_fishing(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    warp(g, "ワールド", 1, 10, 3);
    party_give_item(&sgb(g)->party, item_named(g, "針と糸"), 1);
    party_give_item(&sgb(g)->party, item_named(g, "エサ"), 1);
    vnsmilegame_key((VNSmileGame *)g, VK_LEFT, true);
    run(g, 200);
    vnsmilegame_key((VNSmileGame *)g, VK_LEFT, false);
    for (int i = 0; i < 20 && g->interp.active && g->interp.state != 4; i++) press(g, VK_OK), run(g, 100);
    CHECK(g->interp.active && g->interp.state == 4 && g->interp.a.choiceCount == 2);
    CHECK(g->interp.a.choiceCount > 0 && strstr(g->interp.a.choices[0], "釣りをする"));
    press(g, VK_OK);
    bool vibrator = false;
    for (int i = 0; i < 60 && g->interp.active; i++) {
        Interp *ip = &g->interp;
        if (ip->state == 1 && ip->page < ip->pageCount && strstr(ip->pages[ip->page], "調教バイブを手に入れた")) vibrator = true;
        if (ip->state == 1) press(g, VK_OK);
        run(g, 150);
    }
    CHECK(vibrator);
    CHECK(item_count(g, "調教バイブ") == 1);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_chest_stays_open(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    warp(g, "ネコ屋敷", 4, 2, 0);
    press(g, VK_OK);
    finish_event(g);
    CHECK(item_count(g, "針と糸") == 1);
    run(g, 2000);
    double r = 0, gr = 0;
    for (int y = 240; y < 260; y++)
        for (int x = 462; x < 500; x++) {
            u32 c = g->fb[y * g->w + x];
            r += c & 255, gr += c >> 8 & 255;
        }
    r /= 20.0 * 38.0, gr /= 20.0 * 38.0;
    CHECK(gr < 90 && r > gr * 1.6);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_four_heroes(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    add_hero(g, "コレット");
    add_hero(g, "メイリ");
    add_hero(g, "サキュバス");
    CHECK(sgb(g)->party.count == 4);
    warp(g, "ワールド", 6, 14, 0);
    int slime = event_named(g, "歩行-話すとバトル（逃げられる）_2_2_3_4_2_2");
    CHECK(slime >= 0);
    if (slime < 0) {
        vnsmilegame_close((VNSmileGame *)g);
        return;
    }
    const EventDef *ev = &g->field.events[slime];
    const SgrRec *sr = ev->sheetCount ? game_record(&g->data, SGR_EVENT, 16, ev->sheets[0].script, NULL) : NULL;
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
        for (int t = 0; t < 15000 && strcmp(battle_phase(g), "command") != 0; t += 16) vnsmilegame_frame((VNSmileGame *)g, 16);
        CHECK(strcmp(battle_phase(g), "command") == 0);
        double sum = 0;
        int n = 0;
        for (int y = 60; y < 420; y++)
            for (int x = 640; x < g->w; x++) {
                u32 c = g->fb[y * g->w + x];
                int r = c & 255, gg = c >> 8 & 255, b = c >> 16 & 255;
                if (b > r + 25 && r > gg + 10 && b > 120 && gg < 140) sum += x, n++;
            }
        CHECK(n > 500);
        CHECK(n > 0 && fabs(sum / n - 773.0) < 20.0);
    }
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_noon_sky_edge(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    warp(g, "ネコ屋敷", 4, 2, 0);
    run(g, 500);
    u32 c = g->fb[20 * g->w + 20], d = g->fb[300 * g->w + 60];
    CHECK((c & 0xffffff) == 0xffffff && (d & 0xffffff) == 0xffffff);
    vnsmilegame_close((VNSmileGame *)g);
}

static const Res *res_named(SG *g, const char *name) {
    for (int i = 0; i < g->res.count; i++)
        if (strcmp(g->res.items[i].name, name) == 0) return &g->res.items[i];
    return NULL;
}

static void test_ending_sprites(const char *dir) {
    SG *g = new_game(dir);
    if (!g) return;
    warp(g, "エンドロール", 9, 17, 0);
    const Res *hime = res_named(g, "w_hime001");
    CHECK(hime != NULL);
    if (hime) {
        Image *im = sg_image(g, hime->id);
        CHECK(im && sg_sprite_rows(hime, im) == 4);
        Actor a = {0};
        a.graphic = hime;
        a.sheet = im;
        sg_actor_wait_sheet(g, &a);
        CHECK(a.waitSheet && a.waitFrames == 8 && a.waitSheet->w == 1088 && a.waitSheet->h == 136);
    }
    Tables *t = &sgb(g)->tables;
    Guid dark = sgb(g)->party.members[0].hero;
    CHECK(t->heroCount > 0 && sgb(g)->party.count > 0);
    g->interp.active = true;
    Cmd c = {69, 0, NULL, 0};
    CmdArg args[4] = {{.kind = ARG_GUID, .g = dark}, {.kind = ARG_GUID}, {.kind = ARG_GUID}, {.kind = ARG_STR, .s = ""}};
    c.args = args;
    c.argCount = 4;
    sga_exec(g, &c);
    g->interp.active = false;
    CHECK(g->field.player.graphic == NULL);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_experience(void) {
    CHECK(battle_exp_for_level(11) == 4400);
    CHECK(battle_exp_for_level(12) == 6006);
    CHECK(battle_exp_for_level(13) == 8034);
    CHECK(battle_exp_for_level(14) == 10556);
    CHECK(battle_exp_for_level(15) == 13650);
    CHECK(battle_exp_for_level(16) == 17400);
}

int main(void) {
    const char *colette = getenv("SG_COLETTE");
    test_experience();
    if (colette) {
        test_talk_while_stepping(colette);
        test_fishing(colette);
        test_chest_stays_open(colette);
        test_four_heroes(colette);
        test_noon_sky_edge(colette);
        test_ending_sprites(colette);
    }
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

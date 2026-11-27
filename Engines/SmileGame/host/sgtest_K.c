#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

enum { VK_LEFT = 0x25, VK_UP = 0x26, VK_RIGHT = 0x27, VK_DOWN = 0x28, VK_OK = 0x0d };

static char tmp[64];

static SG *open_game(const char *dir, const char *saveDir) {
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = saveDir ? saveDir : "/nonexistent-sgtest-k", .audioRate = 44100};
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

static SG *new_game(const char *dir, const char *saveDir) {
    SG *g = open_game(dir, saveDir);
    if (!g) return NULL;
    run(g, 16);
    if (g->titleCursor == 1) press(g, VK_UP);
    press(g, VK_OK);
    run(g, 200);
    interp_free(&g->interp);
    return g;
}

static void step(SG *g, int vk) {
    vnsmilegame_key((VNSmileGame *)g, vk, true);
    for (int t = 0; t < 1000 && !g->field.player.moving; t += 16) run(g, 16);
    vnsmilegame_key((VNSmileGame *)g, vk, false);
    for (int t = 0; t < 1000 && g->field.player.moving; t += 16) run(g, 16);
    run(g, 100);
}

static void walk(SG *g, const char *path) {
    for (const char *c = path; *c; c++) step(g, *c == 'U' ? VK_UP : *c == 'D' ? VK_DOWN : *c == 'L' ? VK_LEFT : VK_RIGHT);
}

static void play_through(SG *g, int limit) {
    for (int i = 0; i < limit && (g->interp.active || g->pendingTransfer); i++) {
        press(g, VK_OK);
        run(g, 250);
    }
    run(g, 500);
}

static void warp(SG *g, const char *map, int x, int z, int dir) {
    sg_debug_warp((VNSmileGame *)g, map, x, z, dir);
    run(g, 1500);
}

static bool at(SG *g, int x, int z) { return (int)g->field.player.x == x && (int)g->field.player.z == z; }

static bool on_map(SG *g, const char *name) { return g->field.map.name && strcmp(g->field.map.name, name) == 0; }

static void region(SG *g, int x0, int y0, int w, int h, double out[3]) {
    double s[3] = {0};
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            u32 c = g->fb[y * g->w + x];
            for (int k = 0; k < 3; k++) s[k] += c >> (8 * k) & 255;
        }
    for (int k = 0; k < 3; k++) out[k] = s[k] / ((double)w * h);
}

static bool copy_file(const char *from, const char *to) {
    size_t n;
    u8 *d = sg_read_file(from, &n);
    if (!d) return false;
    FILE *f = fopen(to, "wb");
    bool ok = f && fwrite(d, 1, n, f) == n;
    if (f) fclose(f);
    free(d);
    return ok;
}

static bool read_save(int slot, SaveData *s) {
    char path[256];
    snprintf(path, sizeof path, "%s/Save_%02d.sgs", tmp, slot);
    size_t n;
    u8 *d = sg_read_file(path, &n);
    memset(s, 0, sizeof *s);
    bool ok = d && save_parse(s, d, n);
    free(d);
    return ok;
}

static const SaveEvent *save_event(const SaveData *s, const char *prefix) {
    for (int i = 0; i < s->eventCount; i++) {
        char id[37];
        guid_str(s->events[i].id, id);
        if (strncmp(id, prefix, strlen(prefix)) == 0) return &s->events[i];
    }
    return NULL;
}

static SG *castle_from_pc_save(const char *dir) {
    char from[1024], to[256];
    snprintf(from, sizeof from, "%s/savedata/Save_00.sgs", dir);
    snprintf(to, sizeof to, "%s/Save_00.sgs", tmp);
    if (!copy_file(from, to)) return NULL;
    SG *g = open_game(dir, tmp);
    if (!g) return NULL;
    run(g, 16);
    if (!vnsmilegame_load((VNSmileGame *)g, 0)) {
        vnsmilegame_close((VNSmileGame *)g);
        return NULL;
    }
    run(g, 1500);
    return g;
}

static void test_stair_heights_in_saves(const char *dir) {
    SG *g = castle_from_pc_save(dir);
    CHECK(g != NULL);
    if (!g) return;
    CHECK(vnsmilegame_save((VNSmileGame *)g, 1));
    SaveData s;
    CHECK(read_save(1, &s));
    const SaveEvent *soldier = save_event(&s, "bd33d68c"), *exit = save_event(&s, "a4a61211");
    CHECK(soldier && fabsf(soldier->y - 7.6f) < 0.01f);
    CHECK(exit && fabsf(exit->y - 3.6f) < 0.01f);
    save_free(&s);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_queen_keeps_events(const char *dir) {
    SG *g = castle_from_pc_save(dir);
    CHECK(g != NULL);
    if (!g) return;
    walk(g, "DDLLLLLLDDDLLLLL");
    CHECK(at(g, 24, 15));
    press(g, VK_UP);
    press(g, VK_OK);
    play_through(g, 120);
    CHECK(!g->interp.active && on_map(g, "ルディアノ城") && at(g, 24, 15));
    CHECK(g->b.party.money == 100);
    CHECK(vnsmilegame_save((VNSmileGame *)g, 2));
    SaveData s;
    CHECK(read_save(2, &s));
    const SaveEvent *runner = save_event(&s, "fc647fc1");
    CHECK(runner && runner->x == 35.5f && runner->z == 8.5f);
    const SaveEvent *a = save_event(&s, "8af59aef"), *b = save_event(&s, "3d239643");
    CHECK(a && guid_zero(a->graphic) && b && guid_zero(b->graphic));
    save_free(&s);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_castle_light(const char *dir) {
    SG *g = castle_from_pc_save(dir);
    CHECK(g != NULL);
    if (!g) return;
    double c[3];
    region(g, 770, 200, 60, 40, c);
    CHECK(fabs(c[0] - 215) < 15 && fabs(c[1] - 161) < 15 && fabs(c[2] - 101) < 15);
    vnsmilegame_close((VNSmileGame *)g);
}

static const ItemDef *item_named(SG *g, const char *name) {
    for (int i = 0; i < g->b.tables.itemCount; i++)
        if (g->b.tables.items[i].name && strcmp(g->b.tables.items[i].name, name) == 0) return &g->b.tables.items[i];
    return NULL;
}

static void escort(const char *dir, bool sword) {
    SG *g = new_game(dir, NULL);
    CHECK(g != NULL);
    if (!g) return;
    warp(g, "城下町ルディアノ", 19, 35, 2);
    play_through(g, 60);
    sg_debug_warp((VNSmileGame *)g, "城下町ルディアノ", 19, 36, 2);
    run(g, 1500);
    const ItemDef *it = item_named(g, "ロングソード");
    CHECK(it != NULL && g->b.party.count == 1);
    if (it) g->b.party.members[0].equip[SLOT_WEAPON] = sword ? it->id : (Guid){{0}};
    CHECK(!it || sga_item_count(g, it->id) == 0);
    press(g, VK_OK);
    for (int i = 0; i < 120 && g->interp.active && on_map(g, "城下町ルディアノ"); i++) {
        press(g, VK_OK);
        run(g, 250);
    }
    run(g, 500);
    CHECK(on_map(g, sword ? "道中の森" : "城下町ルディアノ"));
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_cave_bridge(const char *dir) {
    SG *g = new_game(dir, NULL);
    CHECK(g != NULL);
    if (!g) return;
    warp(g, "王家の洞窟", 24, 8, 0);
    CHECK(g->field.player.y == 8.0f);
    step(g, VK_UP);
    CHECK(at(g, 24, 7) && g->field.player.y == 8.0f);
    step(g, VK_UP);
    step(g, VK_UP);
    CHECK(at(g, 24, 5) && g->field.player.y == 8.0f);
    step(g, VK_UP);
    step(g, VK_UP);
    CHECK(at(g, 24, 3) && g->field.player.y == 8.0f);
    warp(g, "王家の洞窟", 24, 6, 0);
    CHECK(g->field.player.y == 4.0f);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_open_door(const char *dir) {
    SG *g = new_game(dir, NULL);
    CHECK(g != NULL);
    if (!g) return;
    warp(g, "map_015", 30, 2, 3);
    step(g, VK_RIGHT);
    play_through(g, 20);
    CHECK(at(g, 30, 2));
    g->switches[10] = 1;
    g->interp.a.dirty = true;
    run(g, 100);
    step(g, VK_RIGHT);
    play_through(g, 20);
    step(g, VK_RIGHT);
    step(g, VK_RIGHT);
    CHECK((int)g->field.player.x >= 32 && (int)g->field.player.z == 2);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    const char *sacred = getenv("SG_SACRED");
    if (!sacred) {
        printf("SG_SACRED not set: Sacred checks skipped\n");
        return 0;
    }
    snprintf(tmp, sizeof tmp, "/tmp/sgtest-k-%d", (int)getpid());
    if (mkdir(tmp, 0700) != 0) return 1;
    test_stair_heights_in_saves(sacred);
    test_queen_keeps_events(sacred);
    test_castle_light(sacred);
    escort(sacred, true);
    escort(sacred, false);
    test_cave_bridge(sacred);
    test_open_door(sacred);
    for (int i = 0; i < 4; i++) {
        char path[256];
        snprintf(path, sizeof path, "%s/Save_%02d.sgs", tmp, i);
        remove(path);
    }
    rmdir(tmp);
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

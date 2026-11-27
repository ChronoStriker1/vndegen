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

enum { VK_OK = 0x0d, VK_ESC = 0x1b, VK_UP = 0x26, VK_DOWN = 0x28 };

static SG *open_game(const char *dir, const char *saves) {
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = saves, .audioRate = 44100, .logPath = getenv("SGLOG")};
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

static void presses(SG *g, int vk, int n) {
    for (int i = 0; i < n; i++) press(g, vk);
}

static bool slot_file(const char *saves, int slot) {
    char path[600];
    snprintf(path, sizeof path, "%s/Save_%02d.sgs", saves, slot);
    struct stat st;
    return stat(path, &st) == 0;
}

static void remove_slot(const char *saves, int slot) {
    char path[600];
    snprintf(path, sizeof path, "%s/Save_%02d.sgs", saves, slot);
    remove(path);
}

static bool on_page(SG *g, const char *page) {
    const char *p = menu_page(g);
    return p && strcmp(p, page) == 0;
}

static void open_save_page(SG *g) {
    press(g, VK_ESC);
    run(g, 400);
    presses(g, VK_UP, 8);
    presses(g, VK_DOWN, 4);
    press(g, VK_OK);
    run(g, 200);
}

static void test_save_cursor_after_load(const char *dir, const char *tmp) {
    char saves[512];
    snprintf(saves, sizeof saves, "%s/cursor", tmp);
    CHECK(mkdir(saves, 0700) == 0);
    SG *g = open_game(dir, saves);
    CHECK(g != NULL);
    if (!g) return;
    run(g, 32);
    press(g, VK_OK);
    run(g, 200);
    interp_free(&g->interp);
    CHECK(sg_debug_warp((VNSmileGame *)g, "魔王城B3F", 11, 7, 0));
    run(g, 500);
    CHECK(vnsmilegame_save((VNSmileGame *)g, 3));
    vnsmilegame_close((VNSmileGame *)g);

    g = open_game(dir, saves);
    CHECK(g != NULL);
    if (!g) return;
    run(g, 32);
    CHECK(g->titleCursor == 1);
    press(g, VK_OK);
    CHECK(on_page(g, "save"));
    presses(g, VK_DOWN, 3);
    press(g, VK_OK);
    run(g, 1500);
    CHECK(g->scene == SCENE_FIELD && !menu_active(g));

    open_save_page(g);
    CHECK(on_page(g, "save"));
    press(g, VK_OK);
    run(g, 300);
    CHECK(slot_file(saves, 0));
    CHECK(on_page(g, "save"));

    presses(g, VK_DOWN, 2);
    press(g, VK_OK);
    run(g, 300);
    CHECK(slot_file(saves, 2));
    press(g, VK_ESC);
    press(g, VK_ESC);
    run(g, 600);
    CHECK(!menu_active(g));
    remove_slot(saves, 2);

    press(g, VK_ESC);
    run(g, 400);
    CHECK(on_page(g, "menu"));
    press(g, VK_OK);
    run(g, 200);
    CHECK(on_page(g, "save"));
    press(g, VK_OK);
    run(g, 300);
    CHECK(slot_file(saves, 2));
    CHECK(on_page(g, "save"));
    vnsmilegame_close((VNSmileGame *)g);
    for (int i = 0; i < 10; i++) remove_slot(saves, i);
    rmdir(saves);
}

int main(void) {
    const char *sacred = getenv("SG_SACRED");
    if (sacred) {
        char tmp[] = "/tmp/sgtest_R_XXXXXX";
        if (!mkdtemp(tmp)) return 1;
        test_save_cursor_after_load(sacred, tmp);
        rmdir(tmp);
    } else
        printf("SG_SACRED not set: Sacred checks skipped\n");
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

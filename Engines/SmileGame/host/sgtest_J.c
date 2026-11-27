#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
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

static SG *open_game(const char *dir, const char *saves, const char *font) {
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = saves, .audioRate = 44100, .fallbackFont = font};
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

static int drawn(Font *f, const char *text, u32 *fb) {
    memset(fb, 0, sizeof(u32) * 64 * 64);
    font_draw(f, fb, 64, 64, 4, 4, 32, text, 0xffffffffu);
    int n = 0;
    for (int i = 0; i < 64 * 64; i++) n += fb[i] != 0;
    return n;
}

static void test_fallback(const char *colette, const char *fontPath) {
    static u32 with[64 * 64], alone[64 * 64], without[64 * 64];
    const char *glyph = "\xe5\x9a\x99";
    SG *g = open_game(colette, "/nonexistent-sgtest-j", fontPath);
    CHECK(g != NULL);
    if (!g) return;
    size_t n = 0;
    u8 *data = sg_read_file(fontPath, &n);
    Font *ume = data ? font_load(data, n) : NULL;
    free(data);
    CHECK(ume != NULL);
    int a = drawn(g->font, glyph, with), b = ume ? drawn(ume, glyph, alone) : 0;
    CHECK(a > 40 && a == b);
    CHECK(font_width(g->font, 32, glyph) == font_width(ume, 32, glyph));
    font_free(ume);
    vnsmilegame_close((VNSmileGame *)g);
    g = open_game(colette, "/nonexistent-sgtest-j", NULL);
    CHECK(g != NULL);
    if (!g) return;
    drawn(g->font, glyph, without);
    CHECK(memcmp(with, without, sizeof with) != 0);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_exe_case(const char *colette, const char *tmp) {
    char dir[512];
    snprintf(dir, sizeof dir, "%s/case", tmp);
    CHECK(mkdir(dir, 0700) == 0);
    DIR *d = opendir(colette);
    CHECK(d != NULL);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char from[1024], to[1024];
        snprintf(from, sizeof from, "%s/%s", colette, de->d_name);
        bool exe = strcasecmp(de->d_name, "sgb_rpgplayer.exe") == 0;
        snprintf(to, sizeof to, "%s/%s", dir, exe ? "SGB_RPGPlayer.EXE" : de->d_name);
        CHECK(symlink(from, to) == 0);
    }
    closedir(d);
    SG *g = open_game(dir, tmp, NULL);
    CHECK(g != NULL);
    if (!g) return;
    CHECK(sg_player_at_least(g, 1, 8));
    CHECK(g->fi.captionFadeMs == 0);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_load_closes_menu(const char *colette, const char *tmp) {
    SG *g = open_game(colette, tmp, NULL);
    CHECK(g != NULL);
    if (!g) return;
    press(g, 0x0d);
    for (int i = 0; i < 400 && g->interp.active; i++) press(g, 0x0d);
    CHECK(g->scene == SCENE_FIELD && !g->interp.active);
    CHECK(vnsmilegame_save((VNSmileGame *)g, 3));
    press(g, 0x1b);
    run(g, 500);
    CHECK(menu_page(g) && strcmp(menu_page(g), "menu") == 0);
    CHECK(vnsmilegame_load((VNSmileGame *)g, 3));
    CHECK(menu_page(g) == NULL);
    run(g, 200);
    CHECK(menu_page(g) == NULL && g->scene == SCENE_FIELD);
    char *p = sg_save_path(g, 3);
    remove(p);
    free(p);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_title_continue(const char *colette, const char *tmp) {
    char dir[512];
    snprintf(dir, sizeof dir, "%s/title", tmp);
    CHECK(mkdir(dir, 0700) == 0);
    SG *g = open_game(colette, dir, NULL);
    CHECK(g != NULL);
    if (!g) return;
    run(g, 32);
    CHECK(g->titleCursor == 0 && g->titleSaves == 0);
    CHECK(!vnsmilegame_save((VNSmileGame *)g, 0));
    press(g, 0x28);
    press(g, 0x0d);
    CHECK(g->scene == SCENE_TITLE && !menu_active(g));
    press(g, 0x26);
    press(g, 0x0d);
    for (int i = 0; i < 400 && g->interp.active; i++) press(g, 0x0d);
    CHECK(g->scene == SCENE_FIELD);
    int x = (int)g->field.player.x, z = (int)g->field.player.z;
    CHECK(vnsmilegame_save((VNSmileGame *)g, 9));
    vnsmilegame_close((VNSmileGame *)g);

    g = open_game(colette, dir, NULL);
    CHECK(g != NULL);
    if (!g) return;
    run(g, 32);
    CHECK(g->titleCursor == 1 && g->titleSaves == 1);
    press(g, 0x28);
    press(g, 0x0d);
    CHECK(menu_page(g) && strcmp(menu_page(g), "config") == 0);
    press(g, 0x1b);
    run(g, 32);
    CHECK(g->scene == SCENE_TITLE && !menu_active(g));
    press(g, 0x26);
    press(g, 0x0d);
    CHECK(menu_page(g) && strcmp(menu_page(g), "save") == 0);
    press(g, 0x0d);
    CHECK(g->scene == SCENE_TITLE && menu_active(g));
    press(g, 0x27);
    press(g, 0x28);
    press(g, 0x0d);
    CHECK(g->scene == SCENE_FIELD && !menu_active(g));
    CHECK((int)g->field.player.x == x && (int)g->field.player.z == z);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_pointer(const char *colette, const char *tmp) {
    SG *g = open_game(colette, tmp, NULL);
    CHECK(g != NULL);
    if (!g) return;
    run(g, 32);
    if (g->titleCursor == 1) press(g, 0x26);
    vnsmilegame_pointer((VNSmileGame *)g, VNSMILEGAME_POINTER_DOWN);
    run(g, 100);
    vnsmilegame_pointer((VNSmileGame *)g, VNSMILEGAME_POINTER_UP);
    run(g, 48);
    CHECK(g->scene == SCENE_FIELD);
    for (int i = 0; i < 400 && g->interp.active; i++) {
        vnsmilegame_pointer((VNSmileGame *)g, VNSMILEGAME_POINTER_DOWN);
        run(g, 48);
        vnsmilegame_pointer((VNSmileGame *)g, VNSMILEGAME_POINTER_UP);
        run(g, 48);
    }
    CHECK(!g->interp.active && !menu_active(g));
    vnsmilegame_pointer((VNSmileGame *)g, VNSMILEGAME_POINTER_DOWN);
    run(g, 400);
    CHECK(!menu_active(g));
    run(g, 300);
    CHECK(menu_page(g) && strcmp(menu_page(g), "menu") == 0);
    vnsmilegame_pointer((VNSmileGame *)g, VNSMILEGAME_POINTER_UP);
    run(g, 400);
    CHECK(menu_page(g) && strcmp(menu_page(g), "menu") == 0);
    vnsmilegame_pointer((VNSmileGame *)g, VNSMILEGAME_POINTER_DOWN);
    run(g, 48);
    vnsmilegame_pointer((VNSmileGame *)g, VNSMILEGAME_POINTER_UP);
    run(g, 48);
    CHECK(menu_page(g) && strcmp(menu_page(g), "items") == 0);
    vnsmilegame_pointer((VNSmileGame *)g, VNSMILEGAME_POINTER_DOWN);
    run(g, 48);
    vnsmilegame_pointer((VNSmileGame *)g, VNSMILEGAME_POINTER_CANCEL);
    run(g, 48);
    CHECK(menu_page(g) && strcmp(menu_page(g), "items") == 0);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    const char *colette = getenv("SG_COLETTE");
    char tmp[64];
    snprintf(tmp, sizeof tmp, "/tmp/sgtest-j-%d", (int)getpid());
    if (mkdir(tmp, 0700) != 0) return 1;
    if (colette) {
        test_fallback(colette, SG_FONTS "/ume-pgo4.ttf");
        test_exe_case(colette, tmp);
        test_load_closes_menu(colette, tmp);
        test_title_continue(colette, tmp);
        test_pointer(colette, tmp);
    }
    char cmd[128];
    snprintf(cmd, sizeof cmd, "rm -rf %s", tmp);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", tmp);
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

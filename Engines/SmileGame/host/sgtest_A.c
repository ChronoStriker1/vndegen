#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "png.h"
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

enum { VK_UP = 0x26, VK_DOWN = 0x28, VK_LEFT = 0x25, VK_RIGHT = 0x27, VK_OK = 0x0d, VK_CANCEL = 0x1b };

static void run(SG *g, int ms) {
    for (int t = 0; t < ms; t += 16) vnsmilegame_frame((VNSmileGame *)g, 16);
}

static void press(SG *g, int vk, int holdMs) {
    vnsmilegame_key((VNSmileGame *)g, vk, true);
    run(g, holdMs);
    vnsmilegame_key((VNSmileGame *)g, vk, false);
    run(g, 32);
}

static const char *state_name(SG *g) {
    Interp *ip = &g->interp;
    if (g->scene == SCENE_TITLE) return "title";
    if (!ip->active) return "field";
    switch (ip->state) {
    case 1: return "message";
    case 2: return "scroll";
    case 4: return "choice";
    default: return "event";
    }
}

static bool until_state(SG *g, const char *want, int maxPresses) {
    for (int i = 0; i < maxPresses; i++) {
        for (int t = 0; t < 1500 && strcmp(state_name(g), "event") == 0; t += 16) run(g, 16);
        if (strcmp(state_name(g), want) == 0) {
            run(g, 250);
            if (strcmp(state_name(g), want) == 0) return true;
            continue;
        }
        if (strcmp(state_name(g), "message") == 0 || strcmp(state_name(g), "scroll") == 0 || strcmp(state_name(g), "choice") == 0) press(g, VK_OK, 48);
        run(g, 300);
    }
    return strcmp(state_name(g), want) == 0;
}

static bool showing(SG *g, const char *text) {
    Interp *ip = &g->interp;
    if (!ip->active) return false;
    if (ip->state == 1 && ip->page < ip->pageCount && strstr(ip->pages[ip->page], text)) return true;
    if (ip->state == 2 && ip->scrollText && strstr(ip->scrollText, text)) return true;
    return false;
}

static bool upto(SG *g, const char *text, int maxPresses) {
    for (int i = 0; i < maxPresses; i++) {
        for (int t = 0; t < 3000 && !showing(g, text) && (!g->interp.active || g->interp.state == 0 || g->interp.state == 3 || g->interp.state == 5); t += 16) run(g, 16);
        if (showing(g, text)) {
            run(g, 1200);
            return true;
        }
        if (g->interp.active) press(g, VK_OK, 48);
        run(g, 250);
    }
    return false;
}

static int vk_of(char d) {
    switch (d) {
    case 'U': return VK_UP;
    case 'D': return VK_DOWN;
    case 'L': return VK_LEFT;
    default: return VK_RIGHT;
    }
}

static bool blocked_by_npc(SG *g, int x, int z) {
    for (int i = 0; i < g->field.npcCount; i++)
        if (g->field.npcs[i].visible && (int)floorf(g->field.npcs[i].x) == x && (int)floorf(g->field.npcs[i].z) == z) return true;
    return false;
}

static bool step_ok(SG *g, int x0, int z0, int x1, int z1) {
    const Map *m = &g->field.map;
    if (x1 < 0 || z1 < 0 || x1 >= m->w || z1 >= m->h) return false;
    int h0 = sga_floor(g, x0, z0), h1 = sga_floor(g, x1, z1);
    if (h0 != h1) {
        bool stair = false;
        for (int i = 0; i < m->stairCount; i++)
            if ((m->stairs[i].x == x1 && m->stairs[i].z == z1) || (m->stairs[i].x == x0 && m->stairs[i].z == z0)) stair = true;
        if (!stair || abs(h1 - h0) > 1) return false;
    }
    return !blocked_by_npc(g, x1, z1);
}

static bool walk_to(SG *g, int tx, int tz) {
    const Map *m = &g->field.map;
    int n = m->w * m->h;
    int *prev = malloc(sizeof(int) * (size_t)n);
    for (int i = 0; i < n; i++) prev[i] = -2;
    int sx = (int)floorf(g->field.player.x), sz = (int)floorf(g->field.player.z);
    int *q = malloc(sizeof(int) * (size_t)n), qh = 0, qt = 0;
    prev[sz * m->w + sx] = -1;
    q[qt++] = sz * m->w + sx;
    static const int dx[4] = {0, 1, 0, -1}, dz[4] = {-1, 0, 1, 0};
    while (qh < qt) {
        int c = q[qh++], cx = c % m->w, cz = c / m->w;
        if (cx == tx && cz == tz) break;
        for (int d = 0; d < 4; d++) {
            int nx = cx + dx[d], nz = cz + dz[d];
            if (nx < 0 || nz < 0 || nx >= m->w || nz >= m->h || prev[nz * m->w + nx] != -2) continue;
            if (!step_ok(g, cx, cz, nx, nz)) continue;
            prev[nz * m->w + nx] = c;
            q[qt++] = nz * m->w + nx;
        }
    }
    bool ok = prev[tz * m->w + tx] != -2;
    if (!ok) printf("no path from %d,%d to %d,%d\n", sx, sz, tx, tz);
    if (ok) {
        int path[4096], len = 0;
        for (int c = tz * m->w + tx; c != sz * m->w + sx && len < 4096; c = prev[c]) path[len++] = c;
        for (int i = len - 1; i >= 0 && ok; i--) {
            int nx = path[i] % m->w, nz = path[i] / m->w;
            int px = (int)floorf(g->field.player.x), pz = (int)floorf(g->field.player.z);
            int vk = nx > px ? VK_RIGHT : nx < px ? VK_LEFT : nz > pz ? VK_DOWN : VK_UP;
            vnsmilegame_key((VNSmileGame *)g, vk, true);
            int t = 0;
            for (; t < 2000; t += 16) {
                run(g, 16);
                if (g->interp.active) break;
                if ((int)floorf(g->field.player.x) == nx && (int)floorf(g->field.player.z) == nz && !g->field.player.moving) break;
            }
            vnsmilegame_key((VNSmileGame *)g, vk, false);
            run(g, 16);
            if (g->interp.active) break;
            if (t >= 2000) {
                printf("stuck at %d,%d heading %d,%d moving %d active %d\n", (int)floorf(g->field.player.x), (int)floorf(g->field.player.z), nx, nz,
                       g->field.player.moving, g->interp.active);
                ok = false;
            }
        }
    }
    free(prev);
    free(q);
    return ok;
}

static void shot(SG *g, const char *dir, const char *name) {
    char path[1400];
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    png_write(path, g->fb, g->w, g->h);
}

static void where(SG *g, const char *tag) {
    printf("%s: %s map %s at %d,%d dir %d vars %d %d %d %d sw4 %d sw5 %d money %d\n", tag, state_name(g), g->field.map.name ? g->field.map.name : "-",
           (int)floorf(g->field.player.x), (int)floorf(g->field.player.z), g->field.player.dir, g->vars[0], g->vars[1], g->vars[2], g->vars[3],
           g->switches[4], g->switches[5], g->b.party.money);
}

static int drive(int argc, char **argv) {
    if (argc < 4) return 2;
    char log[1024];
    snprintf(log, sizeof log, "%s/drive.log", argv[3]);
    VNSmileGameConfig cfg = {.gameDir = argv[2], .saveDir = argv[3], .logPath = log, .audioRate = 44100};
    char err[512];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) {
        fprintf(stderr, "open failed: %s\n", err);
        return 1;
    }
    run(g, 16);
    int rc = 0;
    for (int i = 4; i < argc && !rc; i++) {
        const char *c = argv[i];
        int a, b;
        char s[256];
        sg_log(g, "== %s", c);
        if (sscanf(c, "wait:%d", &a) == 1) run(g, a);
        else if (strcmp(c, "ok") == 0) {
            press(g, VK_OK, 48);
            run(g, 200);
        } else if (strcmp(c, "cancel") == 0) {
            press(g, VK_CANCEL, 48);
            run(g, 200);
        } else if (sscanf(c, "hold:%c,%d", s, &a) == 2) press(g, vk_of(s[0]), a);
        else if (sscanf(c, "key:%c", s) == 1) press(g, vk_of(s[0]), 48), run(g, 300);
        else if (sscanf(c, "until:%255s", s) == 1) {
            if (!until_state(g, s, 400)) {
                where(g, "until failed");
                rc = 1;
            }
        } else if (strncmp(c, "upto:", 5) == 0) {
            if (!upto(g, c + 5, 300)) {
                where(g, "upto failed");
                rc = 1;
            }
        } else if (sscanf(c, "pick:%d", &a) == 1) {
            if (!until_state(g, "choice", 100)) {
                where(g, "no choice");
                rc = 1;
                break;
            }
            for (int k = 0; k < a; k++) press(g, VK_DOWN, 48), run(g, 60);
            press(g, VK_OK, 48);
            run(g, 200);
        } else if (sscanf(c, "to:%d,%d", &a, &b) == 2) {
            Guid map = g->field.map.id;
            bool ok = false;
            for (int tries = 0; tries < 5 && !ok; tries++) {
                run(g, 100);
                until_state(g, "field", 50);
                if (!guid_eq(map, g->field.map.id)) break;
                ok = walk_to(g, a, b) && (int)floorf(g->field.player.x) == a && (int)floorf(g->field.player.z) == b;
                if (g->interp.active || !guid_eq(map, g->field.map.id)) ok = true;
            }
            if (!ok) {
                where(g, "walk failed");
                rc = 1;
            }
            run(g, 100);
        } else if (sscanf(c, "talk:%c", s) == 1) {
            press(g, vk_of(s[0]), 48);
            run(g, 100);
            press(g, VK_OK, 48);
            run(g, 300);
        } else if (sscanf(c, "save:%d", &a) == 1) {
            if (!sg_save_write(g, a)) rc = 1;
        } else if (sscanf(c, "load:%d", &a) == 1) {
            if (!sg_save_read(g, a)) rc = 1;
            run(g, 500);
        } else if (sscanf(c, "shot:%255s", s) == 1) shot(g, argv[3], s);
        else if (sscanf(c, "where:%255s", s) == 1) where(g, s);
        else {
            fprintf(stderr, "unknown %s\n", c);
            rc = 2;
        }
    }
    where(g, "end");
    vnsmilegame_close((VNSmileGame *)g);
    return rc;
}

static Guid guid(const char *s) {
    Guid g = {{0}};
    guid_parse(s, &g);
    return g;
}

static Cmd mk(int type, int nest, int n, const i32 *ints) {
    Cmd c = {type, nest, sg_alloc(sizeof(CmdArg) * (size_t)(n ? n : 1)), n};
    for (int i = 0; i < n; i++) c.args[i] = (CmdArg){ARG_INT, ints[i], {{0}}, NULL};
    return c;
}

static void run_script(SG *g, Cmd *cmds, int n) {
    Script s = {.cmds = cmds, .count = n, .trigger = 2};
    interp_start(g, &s);
    for (int i = 0; i < n; i++) free(cmds[i].args);
    run(g, 32);
}

static SG *bare_game(void) {
    SG *g = sg_alloc(sizeof *g);
    g->w = 960;
    g->h = 544;
    g->fb = sg_alloc(sizeof(u32) * 960 * 544);
    r3_init(&g->r3, 960, 544);
    g->scene = SCENE_FIELD;
    return g;
}

static void free_bare(SG *g) {
    interp_free(&g->interp);
    r3_free(&g->r3);
    free(g->locals);
    free(g->fb);
    free(g);
}

static void test_blocks(void) {
    SG *g = bare_game();
    g->vars[0] = 1;
    Cmd a[] = {
        mk(CMD_IF_VARIABLE, 0, 3, (i32[]){0, 1, 0}),
        mk(CMD_VARIABLE, 1, 5, (i32[]){0, 1, 0, 5, 0}),
        mk(CMD_ELSE, 0, 0, NULL),
        mk(CMD_VARIABLE, 1, 5, (i32[]){0, 1, 0, 7, 0}),
        mk(CMD_END, 0, 0, NULL),
        mk(CMD_VARIABLE, 0, 5, (i32[]){0, 2, 0, 3, 1}),
    };
    run_script(g, a, 6);
    CHECK(g->vars[1] == 5);
    CHECK(g->vars[2] == 3);
    CHECK(!g->interp.active);
    g->vars[0] = 2;
    Cmd b[] = {
        mk(CMD_IF_VARIABLE, 0, 3, (i32[]){0, 1, 0}),
        mk(CMD_VARIABLE, 1, 5, (i32[]){0, 1, 0, 5, 0}),
        mk(CMD_ELSE, 0, 0, NULL),
        mk(CMD_VARIABLE, 1, 5, (i32[]){0, 1, 0, 7, 0}),
        mk(CMD_END, 0, 0, NULL),
        mk(CMD_VARIABLE, 0, 5, (i32[]){0, 2, 0, 4, 2}),
    };
    run_script(g, b, 6);
    CHECK(g->vars[1] == 7);
    CHECK(g->vars[2] == -1);
    Cmd c[] = {
        mk(CMD_SWITCH, 0, 2, (i32[]){30, 0}),
        mk(CMD_IF_SWITCH, 0, 2, (i32[]){30, 1}),
        mk(CMD_VARIABLE, 1, 5, (i32[]){0, 3, 0, 9, 0}),
        mk(CMD_END, 0, 0, NULL),
        mk(CMD_IF_SWITCH, 0, 2, (i32[]){30, 0}),
        mk(CMD_VARIABLE, 1, 5, (i32[]){0, 4, 0, 8, 0}),
        mk(CMD_END, 0, 0, NULL),
    };
    run_script(g, c, 7);
    CHECK(g->switches[30] == 1);
    CHECK(g->vars[3] == 0);
    CHECK(g->vars[4] == 8);
    free_bare(g);
}

static void test_choice_cases(void) {
    SG *g = bare_game();
    for (int pick = 0; pick < 3; pick++) {
        g->vars[5] = 0;
        Cmd ch = {CMD_CHOICES, 0, sg_alloc(sizeof(CmdArg) * 5), 5};
        ch.args[0] = (CmdArg){ARG_INT, 3, {{0}}, NULL};
        for (int k = 1; k <= 3; k++) ch.args[k] = (CmdArg){ARG_STR, 0, {{0}}, sg_strdup("x")};
        ch.args[4] = (CmdArg){ARG_INT, 4, {{0}}, NULL};
        Cmd cmds[] = {
            ch,
            mk(CMD_VARIABLE, 1, 5, (i32[]){0, 5, 0, 10, 0}),
            mk(CMD_CASE, 0, 1, (i32[]){1}),
            mk(CMD_VARIABLE, 1, 5, (i32[]){0, 5, 0, 11, 0}),
            mk(CMD_CASE, 0, 1, (i32[]){2}),
            mk(CMD_END, 0, 0, NULL),
            mk(CMD_VARIABLE, 0, 5, (i32[]){0, 5, 0, 100, 1}),
        };
        Script s = {.cmds = cmds, .count = 7, .trigger = 2};
        interp_start(g, &s);
        for (int k = 1; k <= 3; k++) free(ch.args[k].s);
        for (int i = 0; i < 7; i++) free(cmds[i].args);
        run(g, 32);
        CHECK(g->interp.state == 4);
        for (int k = 0; k < pick; k++) press(g, VK_DOWN, 32);
        press(g, VK_OK, 32);
        run(g, 32);
        static const i32 want[3] = {110, 111, 100};
        CHECK(g->vars[5] == want[pick]);
        CHECK(!g->interp.active);
    }
    free_bare(g);
}

static SG *open_game(const char *dir, const char *log) {
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = NULL, .logPath = log, .audioRate = 44100};
    char err[512];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) fprintf(stderr, "open %s: %s\n", dir, err);
    return g;
}

static bool on_map(SG *g, const char *name, int x, int z) {
    return g->field.map.name && strcmp(g->field.map.name, name) == 0 && (int)floorf(g->field.player.x) == x && (int)floorf(g->field.player.z) == z;
}

static void test_colette(const char *dir) {
    SG *g = open_game(dir, NULL);
    if (!g) {
        CHECK(false);
        return;
    }
    run(g, 16);
    press(g, VK_OK, 48);
    run(g, 2000);
    CHECK(until_state(g, "field", 400));
    CHECK(on_map(g, "NO NAME", 14, 6));
    CHECK(g->field.player.dir == 2);
    CHECK(sga_floor(g, 14, 28) == 2);
    CHECK(sga_floor(g, 13, 28) == 3);
    CHECK(sga_floor(g, 14, 6) == 3);
    press(g, VK_RIGHT, 48);
    press(g, VK_OK, 48);
    CHECK(upto(g, "城に眠る", 20));
    int pg = g->interp.page;
    CHECK(pg == 1 && g->interp.pageCount == 3);
    if (pg == 1 && g->interp.pageCount == 3) {
        CHECK(strstr(g->interp.pages[1], "知っているはずなのですが") != NULL);
        CHECK(strncmp(g->interp.pages[2], "新王！", strlen("新王！")) == 0);
        CHECK(strstr(g->interp.pages[2], "ください\nっ。\n") != NULL);
    }
    CHECK(until_state(g, "field", 50));
    CHECK(sga_item_count(g, guid("e33cea40-0c59-4f76-8d89-a4ccccc40c75")) == 1);
    CHECK(g->vars[0] == 2);
    CHECK(walk_to(g, 10, 5));
    press(g, VK_UP, 400);
    run(g, 2500);
    CHECK(on_map(g, "ログ", 26, 17));
    CHECK(walk_to(g, 0, 17));
    run(g, 2500);
    CHECK(until_state(g, "choice", 20));
    CHECK(g->field.map.name && strcmp(g->field.map.name, "地下牢") == 0);
    press(g, VK_OK, 48);
    CHECK(until_state(g, "choice", 20));
    CHECK(g->vars[2] == 1);
    press(g, VK_DOWN, 48);
    press(g, VK_DOWN, 48);
    press(g, VK_OK, 48);
    run(g, 2500);
    CHECK(until_state(g, "field", 20));
    CHECK(on_map(g, "NO NAME", 10, 6));
    CHECK(g->field.player.dir == 2);
    CHECK(walk_to(g, 14, 6));
    press(g, VK_RIGHT, 48);
    press(g, VK_OK, 48);
    CHECK(upto(g, "むむ、あの女", 10));
    CHECK(until_state(g, "field", 50));
    CHECK(g->vars[0] == 3 && g->vars[2] == 2);
    CHECK(walk_to(g, 14, 29));
    run(g, 2500);
    CHECK(until_state(g, "field", 20));
    CHECK(on_map(g, "城下町", 3, 9));
    CHECK(walk_to(g, 6, 22));
    press(g, VK_RIGHT, 48);
    press(g, VK_OK, 48);
    CHECK(upto(g, "外にはモンスターがウヨウヨ", 10));
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_sacred(const char *dir) {
    SG *g = open_game(dir, NULL);
    if (!g) {
        CHECK(false);
        return;
    }
    run(g, 16);
    press(g, VK_OK, 48);
    run(g, 1500);
    CHECK(upto(g, "本作品は18歳以上", 10));
    CHECK(upto(g, "すごく鈍い音", 200));
    CHECK(g->interp.a.msgCmd == CMD_FACE_MESSAGE && g->interp.a.speaker == 1);
    CHECK(guid_eq(g->interp.a.faceL, guid("d9d5c728-2b11-4126-acac-3b1fcbf01c89")));
    CHECK(guid_eq(g->interp.a.faceR, guid("c13ac811-d0d0-40ec-bf94-f16a81c43df3")));
    CHECK(sga_face(g, g->interp.a.faceL, g->interp.a.exprL) == sga_path_image(g, "res/face/mana/mana_normal.png"));
    CHECK(sga_face(g, g->interp.a.faceL, 1) == sga_path_image(g, "res/face/mana/mana_smile.png"));
    CHECK(sga_face(g, g->interp.a.faceR, 2) == sga_path_image(g, "res/face/mugi/mugi.png"));
    CHECK(upto(g, "ルディアノ王が3年前から", 200));
    CHECK(g->field.map.name && strcmp(g->field.map.name, "ルディアノ城") == 0);
    CHECK(g->field.player.dir == 0);
    CHECK(g->b.party.count == 1 && guid_eq(g->b.party.members[0].hero, guid("1d9dc79e-ec79-452b-bf79-f5a7f5a51ac6")));
    CHECK(upto(g, "女王様がお呼び", 100));
    CHECK(g->field.player.graphic && strcmp(g->field.player.graphic->name, "c_023a") == 0);
    CHECK(until_state(g, "field", 100));
    CHECK(on_map(g, "ルディアノ城", 35, 10));
    CHECK(g->switches[20] == 1);
    CHECK(walk_to(g, 24, 15));
    press(g, VK_UP, 48);
    press(g, VK_OK, 48);
    CHECK(until_state(g, "field", 100));
    CHECK(g->b.party.money == 100);
    CHECK(g->switches[21] == 1);
    CHECK(walk_to(g, 35, 29));
    press(g, VK_DOWN, 48);
    press(g, VK_OK, 48);
    CHECK(upto(g, "これからダンジョンへ", 10));
    CHECK(until_state(g, "choice", 10));
    CHECK(g->interp.a.choiceCount == 3 && strcmp(g->interp.a.choices[1], "模擬戦闘をしたい") == 0);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "drive") == 0) return drive(argc, argv);
    test_blocks();
    test_choice_cases();
    const char *colette = getenv("SG_COLETTE"), *sacred = getenv("SG_SACRED");
    if (colette) test_colette(colette);
    else printf("SG_COLETTE not set: Colette play-through skipped\n");
    if (sacred) test_sacred(sacred);
    else printf("SG_SACRED not set: Sacred play-through skipped\n");
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

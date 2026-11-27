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
        if (strcmp(state_name(g), want) == 0) return true;
        if (strcmp(state_name(g), "message") == 0 || strcmp(state_name(g), "scroll") == 0) press(g, VK_OK, 48);
        run(g, 300);
    }
    return strcmp(state_name(g), want) == 0;
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
           g->switches[4], g->switches[5], g->interp.a.money);
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

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "drive") == 0) return drive(argc, argv);
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

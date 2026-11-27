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

static int vk_of(char d) {
    switch (d) {
    case 'U': return VK_UP;
    case 'D': return VK_DOWN;
    case 'L': return VK_LEFT;
    default: return VK_RIGHT;
    }
}

static const char *scene(SG *g) { return vnsmilegame_scene((VNSmileGame *)g); }

static bool in_scene(SG *g, const char *s) { return strcmp(scene(g), s) == 0; }

static bool until_scene(SG *g, const char *want, int maxPresses) {
    for (int i = 0; i < maxPresses; i++) {
        for (int t = 0; t < 1500 && in_scene(g, "event"); t += 16) run(g, 16);
        if (in_scene(g, want)) return true;
        if (in_scene(g, "message") || in_scene(g, "scroll") || (g->interp.active && g->interp.state == 4)) press(g, VK_OK, 48);
        run(g, 300);
    }
    return in_scene(g, want);
}

static bool showing(SG *g, const char *text) {
    Interp *ip = &g->interp;
    return ip->active && ip->state == 1 && ip->page < ip->pageCount && strstr(ip->pages[ip->page], text);
}

static bool upto(SG *g, const char *text, int maxPresses) {
    for (int i = 0; i < maxPresses; i++) {
        for (int t = 0; t < 3000 && !showing(g, text) && !in_scene(g, "message"); t += 16) run(g, 16);
        if (showing(g, text)) return true;
        if (g->interp.active) press(g, VK_OK, 48);
        run(g, 250);
    }
    return false;
}

static bool blocked(SG *g, int x, int z) {
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
    return !blocked(g, x1, z1);
}

static bool walk_to(SG *g, int tx, int tz) {
    const Map *m = &g->field.map;
    int n = m->w * m->h;
    int *prev = malloc(sizeof(int) * (size_t)n), *q = malloc(sizeof(int) * (size_t)n), qh = 0, qt = 0;
    for (int i = 0; i < n; i++) prev[i] = -2;
    int sx = (int)floorf(g->field.player.x), sz = (int)floorf(g->field.player.z);
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
                if (g->interp.active || sg_battle_active(g)) break;
                if ((int)floorf(g->field.player.x) == nx && (int)floorf(g->field.player.z) == nz && !g->field.player.moving) break;
            }
            vnsmilegame_key((VNSmileGame *)g, vk, false);
            run(g, 16);
            if (g->interp.active || sg_battle_active(g)) break;
            if (t >= 2000) ok = false;
        }
    }
    free(prev);
    free(q);
    return ok;
}

static const HeroDef *hero_named(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->heroCount; i++)
        if (strcmp(t->heroes[i].name, name) == 0) return &t->heroes[i];
    return NULL;
}

static const MonsterDef *monster_named(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->monsterCount; i++)
        if (strcmp(t->monsters[i].name, name) == 0) return &t->monsters[i];
    return NULL;
}

static void where(SG *g, const char *tag) {
    Party *p = &sgb(g)->party;
    printf("%s: %s map %s at %d,%d dir %d", tag, scene(g), g->field.map.name ? g->field.map.name : "-", (int)floorf(g->field.player.x),
           (int)floorf(g->field.player.z), g->field.player.dir);
    if (sg_battle_active(g)) printf(" battle %s [%s]", battle_phase(g), battle_bar(g));
    printf(" result %d money %d", sg_battle_result(g), p->money);
    for (int i = 0; i < p->count; i++) {
        const HeroDef *h = hero_def(&sgb(g)->tables, p->members[i].hero);
        printf(" | %s L%d HP%d MP%d EXP%d", h ? h->name : "?", p->members[i].level, p->members[i].hp, p->members[i].mp, p->members[i].exp);
    }
    Guid bgm;
    if (snd_bgm_playing(g, &bgm)) {
        const Res *r = res_find(&g->res, bgm);
        printf(" bgm %s", r ? r->name : "?");
    }
    printf("\n");
}

static bool fight(SG *g, const char *how, int maxTurns) {
    for (int turn = 0; turn < maxTurns && sg_battle_active(g); turn++) {
        for (int t = 0; t < 30000 && sg_battle_active(g); t += 16) {
            const char *ph = battle_phase(g);
            if (!strcmp(ph, "command") || !strcmp(ph, "result") || !strcmp(ph, "gameover")) break;
            run(g, 16);
        }
        if (!sg_battle_active(g)) break;
        const char *ph = battle_phase(g);
        if (!strcmp(ph, "result") || !strcmp(ph, "gameover")) {
            run(g, 1600);
            press(g, VK_OK, 48);
            run(g, 200);
            continue;
        }
        if (!strcmp(how, "escape")) {
            for (int k = 0; k < 8; k++) press(g, VK_UP, 48);
            press(g, VK_OK, 48);
        } else if (!strcmp(how, "defend")) {
            press(g, VK_DOWN, 48);
            press(g, VK_OK, 48);
        } else {
            press(g, VK_OK, 48);
            press(g, VK_OK, 48);
        }
        run(g, 100);
    }
    return !sg_battle_active(g);
}

static int drive(int argc, char **argv) {
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
        int a, b, d;
        char s[256];
        sg_log(g, "== %s", c);
        if (sscanf(c, "wait:%d", &a) == 1) run(g, a);
        else if (!strcmp(c, "ok")) press(g, VK_OK, 48), run(g, 200);
        else if (!strcmp(c, "cancel")) press(g, VK_CANCEL, 48), run(g, 200);
        else if (!strcmp(c, "new")) press(g, VK_OK, 48), run(g, 500);
        else if (sscanf(c, "key:%c", s) == 1) press(g, vk_of(s[0]), 48), run(g, 300);
        else if (sscanf(c, "hold:%c,%d", s, &a) == 2) press(g, vk_of(s[0]), a);
        else if (sscanf(c, "until:%255s", s) == 1) {
            if (!until_scene(g, s, 400)) where(g, "until failed"), rc = 1;
        } else if (!strncmp(c, "upto:", 5)) {
            if (!upto(g, c + 5, 300)) where(g, "upto failed"), rc = 1;
        } else if (sscanf(c, "pick:%d", &a) == 1) {
            if (!until_scene(g, "event", 1) && !(g->interp.active && g->interp.state == 4)) {
                for (int k = 0; k < 100 && !(g->interp.active && g->interp.state == 4); k++) press(g, VK_OK, 48), run(g, 250);
            }
            for (int k = 0; k < a; k++) press(g, VK_DOWN, 48), run(g, 60);
            press(g, VK_OK, 48);
            run(g, 200);
        } else if (sscanf(c, "to:%d,%d", &a, &b) == 2) {
            if (!walk_to(g, a, b)) where(g, "walk failed"), rc = 1;
            run(g, 100);
        } else if (sscanf(c, "talk:%c", s) == 1) {
            press(g, vk_of(s[0]), 48);
            run(g, 100);
            press(g, VK_OK, 48);
            run(g, 300);
        } else if (sscanf(c, "fight:%255s", s) == 1) {
            if (!fight(g, s, 60)) where(g, "fight unfinished"), rc = 1;
        } else if (sscanf(c, "var:%d=%d", &a, &b) == 2) g->vars[a] = b;
        else if (sscanf(c, "sw:%d=%d", &a, &b) == 2) g->switches[a] = (u8)b;
        else if (sscanf(c, "seed:%d", &a) == 1) battle_seed(g, (u32)a);
        else if (sscanf(c, "hp:%d=%d", &a, &b) == 2) sgb(g)->party.members[a].hp = b;
        else if (sscanf(c, "join:%255s", s) == 1) {
            const HeroDef *h = hero_named(g, s);
            if (h) party_add_hero(g, h->id);
            else rc = 1;
        } else if (sscanf(c, "warp:%255[^,],%d,%d,%d", s, &a, &b, &d) == 4) {
            if (!sg_debug_warp((VNSmileGame *)g, s, a, b, d)) rc = 1;
            run(g, 300);
        } else if (sscanf(c, "save:%d", &a) == 1) {
            if (!sg_save_write(g, a)) rc = 1;
        } else if (sscanf(c, "load:%d", &a) == 1) {
            if (!sg_save_read(g, a)) rc = 1;
            run(g, 500);
        } else if (sscanf(c, "shot:%255s", s) == 1) {
            char path[1400];
            snprintf(path, sizeof path, "%s/%s.png", argv[3], s);
            png_write(path, g->fb, g->w, g->h);
        } else if (sscanf(c, "where:%255s", s) == 1) where(g, s);
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
    if (argc > 3 && strcmp(argv[1], "drive") == 0) return drive(argc, argv);
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

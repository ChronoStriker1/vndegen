#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

enum { VK_OK = 0x0d };

static const char *kAide = "b92ec52f-2036-4dcb-933e-1c5e78a31889";

static Guid guid(const char *s) {
    Guid g = {{0}};
    guid_parse(s, &g);
    return g;
}

static void run(SG *g, int ms) {
    for (int t = 0; t < ms; t += 16) vnsmilegame_frame((VNSmileGame *)g, 16);
}

static SG *open_game(const char *dir, char *tmp) {
    strcpy(tmp, "/tmp/sgtestI.XXXXXX");
    if (!mkdtemp(tmp)) return NULL;
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = tmp, .audioRate = 44100};
    char err[512];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) fprintf(stderr, "open %s: %s\n", dir, err);
    return g;
}

static void close_game(SG *g, const char *tmp) {
    for (int i = 0; i < 4; i++) {
        char *p = sg_save_path(g, i);
        remove(p);
        free(p);
    }
    vnsmilegame_close((VNSmileGame *)g);
    rmdir(tmp);
}

static Cmd make_cmd(int type, int n, const i32 *v) {
    Cmd c = {.type = type, .argCount = n};
    c.args = sg_alloc(sizeof(CmdArg) * (size_t)(n ? n : 1));
    for (int i = 0; i < n; i++) c.args[i] = (CmdArg){.kind = ARG_INT, .i = v[i]};
    return c;
}

static int find_npc(SG *g, const char *id) {
    Guid want = guid(id);
    for (int i = 0; i < g->field.npcCount; i++)
        if (guid_eq(g->field.npcs[i].eventId, want)) return i;
    return -1;
}

static int throne(SG *g) {
    run(g, 16);
    vnsmilegame_key((VNSmileGame *)g, VK_OK, true);
    run(g, 48);
    vnsmilegame_key((VNSmileGame *)g, VK_OK, false);
    g->vars[0] = 1;
    sg_debug_warp((VNSmileGame *)g, "Map_069", 14, 6, 2);
    run(g, 48);
    int aide = find_npc(g, kAide);
    g->interp.a.eventIndex = aide;
    return aide;
}

static void exec(SG *g, int type, int n, const i32 *v) {
    Cmd c = make_cmd(type, n, v);
    sga_exec(g, &c);
    free(c.args);
    run(g, 600);
}

typedef struct Seen {
    i32 aideDir, playerDir;
    float aideX, aideZ;
} Seen;

static Seen saved(SG *g) {
    Seen s = {-1, -1, 0, 0};
    CHECK(sg_save_write(g, 1));
    char *path = sg_save_path(g, 1);
    size_t n = 0;
    u8 *data = sg_read_file(path, &n);
    free(path);
    SaveData d;
    if (data && save_parse(&d, data, n)) {
        s.playerDir = d.dir;
        for (int i = 0; i < d.eventCount; i++)
            if (guid_eq(d.events[i].id, guid(kAide))) {
                s.aideDir = d.events[i].dir;
                s.aideX = d.events[i].x;
                s.aideZ = d.events[i].z;
            }
        save_free(&d);
    } else {
        CHECK(false);
    }
    free(data);
    return s;
}

static void test_turn_codes(const char *dir) {
    static const struct {
        int code;
        i32 start;
        i32 aide;
    } turns[] = {{5, 0, 2}, {5, 3, 2}, {6, 0, 3}, {6, 2, 3}, {7, 0, 0}, {7, 1, 1}, {7, 3, 3}};
    for (size_t k = 0; k < sizeof turns / sizeof turns[0]; k++) {
        char tmp[32];
        SG *g = open_game(dir, tmp);
        if (!g) {
            CHECK(false);
            return;
        }
        CHECK(throne(g) >= 0);
        exec(g, CMD_TURN_EVENT, 1, &turns[k].start);
        exec(g, CMD_TURN_EVENT, 1, &(i32){turns[k].code});
        Seen s = saved(g);
        CHECK(s.aideDir == turns[k].aide);
        if (s.aideDir != turns[k].aide) fprintf(stderr, "  10 %d from %d: %d\n", turns[k].code, turns[k].start, s.aideDir);
        close_game(g, tmp);
    }
    static const struct {
        int code;
        i32 start;
        i32 player;
    } pturns[] = {{5, 1, 3}, {5, 0, 3}, {6, 1, 2}, {6, 3, 2}, {7, 0, 0}, {7, 2, 2}};
    for (size_t k = 0; k < sizeof pturns / sizeof pturns[0]; k++) {
        char tmp[32];
        SG *g = open_game(dir, tmp);
        if (!g) return;
        throne(g);
        exec(g, CMD_TURN_PLAYER, 1, &pturns[k].start);
        exec(g, CMD_TURN_PLAYER, 1, &(i32){pturns[k].code});
        Seen s = saved(g);
        CHECK(s.playerDir == pturns[k].player);
        close_game(g, tmp);
    }
}

static void test_move_one(const char *dir) {
    static const struct {
        int code;
        i32 dir;
        float x;
    } moves[] = {{5, 2, 15.5f}, {6, 3, 16.5f}, {7, 0, 15.5f}};
    for (size_t k = 0; k < sizeof moves / sizeof moves[0]; k++) {
        char tmp[32];
        SG *g = open_game(dir, tmp);
        if (!g) return;
        throne(g);
        exec(g, CMD_TURN_EVENT, 1, (i32[]){0});
        exec(g, CMD_MOVE_EVENT, 1, &(i32){moves[k].code});
        Seen s = saved(g);
        CHECK(s.aideDir == moves[k].dir);
        CHECK(fabsf(s.aideX - moves[k].x) < 0.01f);
        CHECK(fabsf(s.aideZ - 6.5f) < 0.01f);
        close_game(g, tmp);
    }
}

static void test_place_event(const char *dir) {
    static const i32 expect[5] = {0, 0, 1, 2, 3};
    for (int code = 0; code < 5; code++) {
        char tmp[32];
        SG *g = open_game(dir, tmp);
        if (!g) return;
        throne(g);
        exec(g, CMD_TURN_EVENT, 1, (i32[]){0});
        Cmd c = make_cmd(15, 4, (i32[]){15, 7, 0, code});
        c.args[2] = (CmdArg){.kind = ARG_GUID};
        sga_exec(g, &c);
        free(c.args);
        run(g, 100);
        Seen s = saved(g);
        CHECK(s.aideDir == expect[code]);
        CHECK(fabsf(s.aideX - 15.5f) < 0.01f && fabsf(s.aideZ - 7.5f) < 0.01f);
        close_game(g, tmp);
    }
}

static void test_random_variable(const char *dir) {
    char tmp[32];
    SG *g = open_game(dir, tmp);
    if (!g) return;
    throne(g);
    int seen[8] = {0};
    bool inRange = true;
    for (int i = 0; i < 200; i++) {
        g->vars[100] = 7;
        Cmd c = make_cmd(70, 6, (i32[]){0, 100, 1, 0, 4, 0});
        sga_exec(g, &c);
        free(c.args);
        if (g->vars[100] < 0 || g->vars[100] > 4) inRange = false;
        else seen[g->vars[100]]++;
    }
    CHECK(inRange);
    for (int v = 0; v <= 4; v++) CHECK(seen[v] > 0);
    close_game(g, tmp);
}

static void test_encounter_switch(const char *dir) {
    char tmp[32];
    SG *g = open_game(dir, tmp);
    if (!g) return;
    throne(g);
    CHECK(sgi_encounters_on(g));
    exec(g, 66, 1, (i32[]){1});
    CHECK(!sgi_encounters_on(g));
    CHECK(sg_save_write(g, 1));
    char *path = sg_save_path(g, 1);
    size_t n = 0;
    u8 *data = sg_read_file(path, &n);
    free(path);
    SaveData d;
    if (data && save_parse(&d, data, n)) {
        CHECK(d.config[0] == 1 && d.config[1] == 0);
        save_free(&d);
    } else {
        CHECK(false);
    }
    free(data);
    exec(g, 66, 1, (i32[]){0});
    CHECK(sgi_encounters_on(g));
    close_game(g, tmp);
}

static void test_sheet_facing(const char *dir) {
    char tmp[32];
    SG *g = open_game(dir, tmp);
    if (!g) return;
    run(g, 16);
    vnsmilegame_key((VNSmileGame *)g, VK_OK, true);
    run(g, 48);
    vnsmilegame_key((VNSmileGame *)g, VK_OK, false);
    sg_debug_warp((VNSmileGame *)g, "Map_007", 35, 29, 2);
    run(g, 48);
    static const struct {
        const char *id;
        i32 dir;
    } soldiers[] = {{"f2a0547a", 3}, {"00262240", 3}, {"2b628b3b", 3}};
    CHECK(sg_save_write(g, 1));
    char *path = sg_save_path(g, 1);
    size_t n = 0;
    u8 *data = sg_read_file(path, &n);
    free(path);
    SaveData d;
    if (data && save_parse(&d, data, n)) {
        for (size_t k = 0; k < sizeof soldiers / sizeof soldiers[0]; k++) {
            int found = 0;
            for (int i = 0; i < d.eventCount; i++) {
                char head[9];
                snprintf(head, sizeof head, "%02x%02x%02x%02x", d.events[i].id.b[0], d.events[i].id.b[1], d.events[i].id.b[2], d.events[i].id.b[3]);
                if (strcmp(head, soldiers[k].id) != 0) continue;
                found = 1;
                CHECK(d.events[i].dir == soldiers[k].dir);
            }
            CHECK(found);
        }
        save_free(&d);
    }
    free(data);
    close_game(g, tmp);
}

int main(void) {
    const char *colette = getenv("SG_COLETTE"), *sacred = getenv("SG_SACRED");
    if (colette) {
        test_turn_codes(colette);
        test_move_one(colette);
        test_place_event(colette);
        test_random_variable(colette);
        test_encounter_switch(colette);
    } else {
        printf("SG_COLETTE not set: Colette checks skipped\n");
    }
    if (sacred) {
        test_sheet_facing(sacred);
    } else {
        printf("SG_SACRED not set: Sacred checks skipped\n");
    }
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

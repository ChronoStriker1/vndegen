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


static int unhex(const char *h, u8 *out) {
    int n = 0;
    for (; h[0] && h[1]; h += 2) {
        unsigned v;
        sscanf(h, "%2x", &v);
        out[n++] = (u8)v;
    }
    return n;
}

static Cmd picture_cmd(int type, int n, const i32 *v, int guidAt, const char *id) {
    Cmd c = make_cmd(type, n, v);
    if (guidAt >= 0) c.args[guidAt] = (CmdArg){.kind = ARG_GUID, .g = guid(id)};
    return c;
}

static bool read_saved(SG *g, int slot, SaveData *d) {
    char *path = sg_save_path(g, slot);
    size_t n = 0;
    u8 *data = sg_read_file(path, &n);
    free(path);
    bool ok = data && save_parse(d, data, n);
    free(data);
    return ok;
}

static void test_pictures_saved(const char *dir) {
    static const char *pc[7] = {
        "010000000000000005640aa4c7effd41bd5419eb9907e6a964000000ffffffff00000000000000000001ffffffff",
        "02000000000000006f0c92242684fa4cbc8bf79518e982d6640000007f7f7f7f00640000003200000001ffffffff",
        "0300000000000000ff82637d48bb5740b6f82c21457dd19d32000000ff0000ff0039ffffff0000000001ffffffff",
        "0400000000000000a8faf8b396ec9741b91cf2ceb3c0ca90640000000000000000c80000009dffffff00ffffffff",
        "0500000000000000c681447ae5e9234a8fe457dce18be3c164000000ffffffff0038ffffff9cffffff00ffffffff",
        "06000000000000004d4731a20bb75340a46c2cd071c6b7fa28000000ffffffff00320000007800000001ffffffff",
        "070000000000000005f48c6674b24f4ba108a87b0107da236400000040ff000000fa0000009600000001ffffffff"};
    char tmp[32];
    SG *g = open_game(dir, tmp);
    if (!g) return;
    throne(g);
    struct {
        int type, n;
        i32 v[9];
        int guidAt;
        const char *id;
    } steps[] = {
        {CMD_SHOW_PICTURE, 9, {1, 0, 100, -1, 1, 0, 0, -1, 0}, 1, "a40a6405-efc7-41fd-bd54-19eb9907e6a9"},
        {CMD_SHOW_PICTURE, 9, {2, 0, 100, 0x7f7f7f7f, 1, 100, 50, -1, 0}, 1, "24920c6f-8426-4cfa-bc8b-f79518e982d6"},
        {CMD_SHOW_PICTURE, 9, {3, 0, 50, -16776961, 1, -200, 0, -1, 1}, 1, "7d6382ff-bb48-4057-b6f8-2c21457dd19d"},
        {CMD_SHOW_PICTURE, 9, {4, 0, 100, -1, 1, 200, -100, -1, 0}, 1, "b3f8faa8-ec96-4197-b91c-f2ceb3c0ca90"},
        {CMD_ERASE_PICTURE, 2, {4, 1}, -1, NULL},
        {CMD_SHOW_PICTURE, 9, {5, 0, 100, -1, 1, -200, -100, -1, 0}, 1, "7a4481c6-e9e5-4a23-8fe4-57dce18be3c1"},
        {CMD_ERASE_PICTURE, 2, {5, 0}, -1, NULL},
        {CMD_SHOW_PICTURE, 9, {6, 0, 100, -1, 1, 0, 150, -1, 0}, 1, "a231474d-b70b-4053-a46c-2cd071c6b7fa"},
        {CMD_MOVE_PICTURE, 8, {6, 40, 500, 1, -1, 1, 50, 120}, -1, NULL},
        {CMD_SHOW_PICTURE, 9, {7, 0, 100, 0x40ff0000, 1, 250, 150, -1, 0}, 1, "668cf405-b274-4b4f-a108-a87b0107da23"},
    };
    for (size_t k = 0; k < sizeof steps / sizeof steps[0]; k++) {
        Cmd c = picture_cmd(steps[k].type, steps[k].n, steps[k].v, steps[k].guidAt, steps[k].id);
        sga_exec(g, &c);
        free(c.args);
    }
    run(g, 2000);
    CHECK(sg_save_write(g, 1));
    SaveData d;
    if (read_saved(g, 1, &d)) {
        CHECK(d.pictureCount == 7);
        for (int i = 0; i < d.pictureCount && i < 7; i++) {
            u8 want[64];
            int n = unhex(pc[i], want);
            CHECK(d.pictures[i].size == n);
            if (d.pictures[i].size != n) continue;
            bool rounded = i == 2 || i == 3;
            for (int b = 0; b < n; b++) {
                if (rounded && b >= 33 && b < 41) continue;
                if (d.pictures[i].body[b] != want[b]) {
                    fprintf(stderr, "  picture %d byte %d: %02x, PC %02x\n", i + 1, b, d.pictures[i].body[b], want[b]);
                    CHECK(false);
                    break;
                }
            }
        }
        save_free(&d);
    } else {
        CHECK(false);
    }
    close_game(g, tmp);
}

static void test_pictures_restored(const char *dir) {
    static const char *pc[2] = {"0500000000000000ffcf5eec0b338947ae00f069cf0ccabb640000007f7f7f7f00000000000000000001ffffffff",
                                "0a00000000000000d2a50a5b6df1264e9ac3fc54a1798188640000000000000000000000000000000000ffffffff"};
    char tmp[32];
    SG *g = open_game(dir, tmp);
    if (!g) return;
    run(g, 16);
    vnsmilegame_key((VNSmileGame *)g, VK_OK, true);
    run(g, 48);
    vnsmilegame_key((VNSmileGame *)g, VK_OK, false);
    sg_debug_warp((VNSmileGame *)g, "Map_007", 35, 10, 0);
    run(g, 48);
    CHECK(sg_save_write(g, 0));
    SaveData d;
    if (!read_saved(g, 0, &d)) {
        CHECK(false);
        close_game(g, tmp);
        return;
    }
    for (int i = 0; i < d.pictureCount; i++) free(d.pictures[i].body);
    d.pictureCount = 2;
    d.pictures = sg_realloc(d.pictures, sizeof(SavePicture) * 2);
    for (int i = 0; i < 2; i++) {
        d.pictures[i].body = sg_alloc(64);
        d.pictures[i].size = unhex(pc[i], d.pictures[i].body);
    }
    size_t n = 0;
    u8 *bytes = save_serialize(&d, &n);
    save_free(&d);
    char *path = sg_save_path(g, 0);
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(bytes, 1, n, f);
        fclose(f);
    }
    free(path);
    free(bytes);
    CHECK(sg_save_read(g, 0));
    run(g, 100);
    SgaPicture *help = &g->interp.a.pics[5], *tutorial = &g->interp.a.pics[10];
    CHECK(help->used && guid_eq(help->id, guid("ec5ecfff-330b-4789-ae00-f069cf0ccabb")));
    CHECK(tutorial->kept && !tutorial->used);
    if (help->im) {
        int matched = 0, opaque = 0;
        for (int y = 0; y < help->im->h; y += 3)
            for (int x = 0; x < help->im->w; x += 7) {
                u32 s = help->im->px[(size_t)y * (size_t)help->im->w + (size_t)x];
                if ((s >> 24) != 255) continue;
                int fx = (g->w - help->im->w) / 2 + x, fy = (g->h - help->im->h) / 2 + y;
                u32 d2 = g->fb[(size_t)fy * (size_t)g->w + (size_t)fx];
                opaque++;
                bool same = true;
                for (int k = 0; k < 24; k += 8) same = same && abs((int)(s >> k & 255) - (int)(d2 >> k & 255)) <= 2;
                matched += same;
            }
        CHECK(opaque > 100 && matched == opaque);
    } else {
        CHECK(false);
    }
    CHECK(sg_save_write(g, 1));
    if (read_saved(g, 1, &d)) {
        CHECK(d.pictureCount == 2);
        for (int i = 0; i < d.pictureCount && i < 2; i++) {
            u8 want[64];
            int len = unhex(pc[i], want);
            CHECK(d.pictures[i].size == len && memcmp(d.pictures[i].body, want, (size_t)len) == 0);
        }
        save_free(&d);
    }
    close_game(g, tmp);
}

static void test_learn_skill(const char *dir) {
    char tmp[32];
    SG *g = open_game(dir, tmp);
    if (!g) return;
    throne(g);
    const char *dark = "1d9dc79e-ec79-452b-bf79-f5a7f5a51ac6", *fire = "588a77c7-8eb6-43bb-ab7c-75c61c154385",
               *ice = "48514fc9-f573-4a83-88cd-b6389b85b749";
    const char *skills[2] = {fire, ice};
    CHECK(skill_def(&g->b.tables, guid(fire)) && skill_def(&g->b.tables, guid(ice)));
    for (int k = 0; k < 2; k++) {
        Cmd c = make_cmd(56, 3, (i32[]){0, 0, 0});
        c.args[0] = (CmdArg){.kind = ARG_GUID, .g = guid(dark)};
        c.args[1] = (CmdArg){.kind = ARG_GUID, .g = guid(skills[k])};
        sga_exec(g, &c);
        free(c.args);
    }
    CHECK(sg_save_write(g, 1));
    SaveData d;
    if (read_saved(g, 1, &d)) {
        CHECK(d.partyCount == 1);
        if (d.partyCount == 1) {
            const SaveHero *h = &d.party[0];
            CHECK(h->size == 188 + 32);
            CHECK(h->learnedCount == 2);
            CHECK(guid_eq(h->learned[0], guid(fire)) && guid_eq(h->learned[1], guid(ice)));
            bool zeros = true;
            for (int i = 192; i < h->size; i++) zeros = zeros && h->body[i] == 0;
            CHECK(zeros);
        }
        save_free(&d);
    } else {
        CHECK(false);
    }
    CHECK(sg_save_read(g, 1));
    CHECK(g->b.party.count == 1 && g->b.party.members[0].learnedCount == 2);
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
        test_pictures_saved(colette);
        test_learn_skill(colette);
    } else {
        printf("SG_COLETTE not set: Colette checks skipped\n");
    }
    if (sacred) {
        test_sheet_facing(sacred);
        test_pictures_restored(sacred);
    } else {
        printf("SG_SACRED not set: Sacred checks skipped\n");
    }
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

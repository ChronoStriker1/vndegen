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

static Guid guid(const char *s) {
    Guid g = {{0}};
    guid_parse(s, &g);
    return g;
}

static const char *save_of(const char *env, const char *game, char *buf, size_t size) {
    const char *p = getenv(env);
    if (p) return p;
    snprintf(buf, size, "%s/savedata/Save_00.sgs", game);
    return buf;
}

static bool load(const char *path, u8 **data, size_t *size, SaveData *s) {
    *data = sg_read_file(path, size);
    if (!*data) {
        fprintf(stderr, "skip: no save at %s\n", path);
        return false;
    }
    bool ok = save_parse(s, *data, *size);
    CHECK(ok);
    return ok;
}

static void check_roundtrip(const SaveData *s, const u8 *data, size_t size) {
    size_t n = 0;
    u8 *out = save_serialize(s, &n);
    CHECK(n == size);
    CHECK(n == size && memcmp(out, data, n) == 0);
    free(out);
}

static void check_runtime_roundtrip(const char *game, const u8 *data, size_t size) {
    char dir[] = "/tmp/sgtestE.XXXXXX";
    if (!mkdtemp(dir)) return;
    char *path = sg_fmt("%s/Save_00.sgs", dir);
    FILE *f = fopen(path, "wb");
    fwrite(data, 1, size, f);
    fclose(f);
    VNSmileGameConfig cfg = {.gameDir = game, .saveDir = dir, .audioRate = 44100};
    char err[256];
    VNSmileGame *vg = vnsmilegame_open(&cfg, err, sizeof err);
    CHECK(vg != NULL);
    if (vg) {
        SG *g = (SG *)vg;
        CHECK(sg_save_read(g, 0));
        CHECK(sg_save_write(g, 3));
        SaveSlot slots[SAVE_SLOTS];
        CHECK(sg_save_slots(g, slots, SAVE_SLOTS) == SAVE_SLOTS);
        CHECK(slots[0].used && slots[3].used && !slots[1].used);
        vnsmilegame_close(vg);
    }
    char *again = sg_fmt("%s/Save_03.sgs", dir);
    size_t n = 0;
    u8 *out = sg_read_file(again, &n);
    SaveData a, b;
    CHECK(out && save_parse(&a, data, size) && save_parse(&b, out, n));
    if (out) {
        CHECK(n == size);
        b.playSeconds = a.playSeconds;
        size_t m = 0;
        u8 *norm = save_serialize(&b, &m);
        CHECK(m == size && memcmp(norm, data, m) == 0);
        free(norm);
        save_free(&a);
        save_free(&b);
    }
    free(out);
    remove(again);
    remove(path);
    rmdir(dir);
    free(again);
    free(path);
}

static void test_colette(const char *game) {
    char buf[1024];
    u8 *data;
    size_t size;
    SaveData s;
    if (!load(save_of("SG_COLETTE_SAVE", game, buf, sizeof buf), &data, &size, &s)) return;
    CHECK(guid_eq(s.map, guid("94ae69b6-668a-437c-9b14-67c3cb1a6de4")));
    CHECK(s.x == 14 && s.z == 6);
    CHECK(s.dir == 3);
    CHECK(s.partyCount == 1 && s.reserveCount == 0);
    CHECK(s.party[0].size == 188);
    CHECK(s.party[0].level == 1 && s.party[0].hp == 32 && s.party[0].maxHp == 32 && s.party[0].mp == 10 && s.party[0].maxMp == 10);
    CHECK(s.money == 0);
    CHECK(s.itemCount == 1 && s.items[0].count == 1);
    CHECK(s.seenCount == 1 && guid_eq(s.seen[0].id, s.items[0].id));
    CHECK(s.vars[0] == 2);
    CHECK(s.eventCount == 15);
    CHECK(s.eventCount > 0 && s.events[0].x == 15.5f && s.events[0].y == 3.0f && s.events[0].z == 6.5f);
    CHECK(s.playerY == 3.0f);
    CHECK(s.cameraPitch == -49.0f && s.cameraDistance == 3.0f && s.cameraFov == 70.0f);
    CHECK(s.pictureCount == 0);
    CHECK(s.config[0] == 1 && s.config[2] == 100 && s.config[6] == 100);
    CHECK(s.flagsTailSize == 6 && s.mapTailSize == 4);
    check_roundtrip(&s, data, size);
    save_free(&s);
    check_runtime_roundtrip(game, data, size);
    free(data);
}

static void test_sacred(const char *game) {
    char buf[1024];
    u8 *data;
    size_t size;
    SaveData s;
    if (!load(save_of("SG_SACRED_SAVE", game, buf, sizeof buf), &data, &size, &s)) return;
    CHECK(s.x == 35 && s.z == 10);
    CHECK(s.eventCount == 28);
    CHECK(s.partyCount == 1 && s.reserveCount == 1);
    CHECK(s.party[0].size == 160);
    CHECK(s.party[0].level == 2 && s.party[0].hp == 55 && s.party[0].maxHp == 55 && s.party[0].mp == 0);
    CHECK(s.lookCount == 2);
    CHECK(s.switches[20] == 1);
    CHECK(s.localCount == 1 && s.locals[0].index == 65536 && s.locals[0].on == 1);
    CHECK(s.pictureCount == 2);
    CHECK(s.cameraPitch == -48.0f && s.cameraDistance == 3.5f);
    CHECK(s.flagsTailSize == 1 && s.mapTailSize == 4);
    check_roundtrip(&s, data, size);
    save_free(&s);
    check_runtime_roundtrip(game, data, size);
    free(data);
}

static void test_title_music(const char *game) {
    VNSmileGameConfig cfg = {.gameDir = game, .audioRate = 44100};
    char err[256];
    VNSmileGame *vg = vnsmilegame_open(&cfg, err, sizeof err);
    CHECK(vg != NULL);
    if (!vg) return;
    SG *g = (SG *)vg;
    vnsmilegame_frame(vg, 16);
    Guid id;
    CHECK(snd_bgm_playing(g, &id) && guid_eq(id, g->data.settings.titleBgm));
    float *out = sg_alloc(sizeof(float) * 2 * 44100);
    for (int k = 0; k < 4; k++) vnsmilegame_audio(vg, out, 44100);
    float peak = 0;
    for (int i = 0; i < 2 * 44100; i++) peak = out[i] > peak ? out[i] : -out[i] > peak ? -out[i] : peak;
    CHECK(peak > 0.05f);
    free(out);
    vnsmilegame_close(vg);
}

static void test_new_game(const char *game, int heroSize, int flagsTail) {
    char dir[] = "/tmp/sgtestE.XXXXXX";
    if (!mkdtemp(dir)) return;
    VNSmileGameConfig cfg = {.gameDir = game, .saveDir = dir, .audioRate = 44100};
    char err[256];
    VNSmileGame *vg = vnsmilegame_open(&cfg, err, sizeof err);
    CHECK(vg != NULL);
    if (!vg) return;
    SG *g = (SG *)vg;
    vnsmilegame_frame(vg, 16);
    vnsmilegame_key(vg, 0x0d, true);
    vnsmilegame_frame(vg, 16);
    vnsmilegame_key(vg, 0x0d, false);
    vnsmilegame_frame(vg, 16);
    CHECK(g->scene == SCENE_FIELD);
    CHECK(sg_save_write(g, 5));
    char *path = sg_save_path(g, 5);
    size_t n = 0;
    u8 *data = sg_read_file(path, &n);
    SaveData s;
    CHECK(data && save_parse(&s, data, n));
    if (data) {
        CHECK(s.partyCount == g->data.settings.partyCount && s.partyCount > 0 && s.party[0].size == heroSize);
        CHECK(guid_eq(s.map, g->data.settings.startMap));
        CHECK(s.x == g->data.settings.startX && s.z == g->data.settings.startZ);
        CHECK(s.eventCount == g->field.npcCount);
        CHECK(s.flagsTailSize == flagsTail && s.mapTailSize == 4);
        CHECK(s.cameraFov == 70.0f && s.cameraPitch < -40.0f);
        save_free(&s);
    }
    free(data);
    remove(path);
    free(path);
    rmdir(dir);
    vnsmilegame_close(vg);
}

int main(void) {
    const char *colette = getenv("SG_COLETTE"), *sacred = getenv("SG_SACRED");
    if (colette) {
        test_colette(colette);
        test_title_music(colette);
        test_new_game(colette, 188, 6);
    }
    if (sacred) {
        test_sacred(sacred);
        test_title_music(sacred);
        test_new_game(sacred, 160, 1);
    }
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

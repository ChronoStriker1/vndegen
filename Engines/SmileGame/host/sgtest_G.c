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

enum { VK_UP = 0x26, VK_DOWN = 0x28, VK_LEFT = 0x25, VK_RIGHT = 0x27, VK_OK = 0x0d, VK_CANCEL = 0x1b };

static Guid guid(const char *s) {
    Guid g = {{0}};
    guid_parse(s, &g);
    return g;
}

static void run(SG *g, int ms) {
    for (int t = 0; t < ms; t += 16) vnsmilegame_frame((VNSmileGame *)g, 16);
}

static void press(SG *g, int vk) {
    vnsmilegame_key((VNSmileGame *)g, vk, true);
    run(g, 48);
    vnsmilegame_key((VNSmileGame *)g, vk, false);
    run(g, 32);
}

static SG *open_game(const char *dir, const char *saveDir) {
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = saveDir, .audioRate = 44100};
    char err[512];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) fprintf(stderr, "open %s: %s\n", dir, err);
    return g;
}

static bool read_slot(SG *g, int slot, SaveData *s) {
    char *path = sg_save_path(g, slot);
    size_t n = 0;
    u8 *data = sg_read_file(path, &n);
    free(path);
    bool ok = data && save_parse(s, data, n);
    free(data);
    return ok;
}

static void remove_slot(SG *g, int slot) {
    char *path = sg_save_path(g, slot);
    remove(path);
    free(path);
}

static void test_new_game_party(const char *dir, const char *hero, int level, int hp, int mp, int power, int vitality, int speed, int exp,
                                const char *weapon, const char *body) {
    char tmp[] = "/tmp/sgtestG.XXXXXX";
    if (!mkdtemp(tmp)) return;
    SG *g = open_game(dir, tmp);
    if (!g) {
        CHECK(false);
        return;
    }
    run(g, 16);
    press(g, VK_OK);
    CHECK(g->scene == SCENE_FIELD);
    CHECK(sg_save_write(g, 3));
    SaveData s;
    CHECK(read_slot(g, 3, &s));
    CHECK(s.partyCount == 1);
    if (s.partyCount == 1) {
        const SaveHero *h = &s.party[0];
        CHECK(guid_eq(h->id, guid(hero)));
        CHECK(h->level == level && h->maxHp == hp && h->hp == hp && h->maxMp == mp && h->mp == mp);
        CHECK(h->power == power && h->vitality == vitality && h->speed == speed && h->exp == exp);
        CHECK(guid_eq(h->equip[0], guid(weapon)) && guid_eq(h->equip[3], guid(body)));
    }
    CHECK(s.itemCount == 0 && s.money == 0 && s.reserveCount == 0);
    save_free(&s);
    remove_slot(g, 3);
    rmdir(tmp);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_load_keeps_party(const char *dir) {
    SG *g = open_game(dir, NULL);
    if (!g) {
        CHECK(false);
        return;
    }
    run(g, 16);
    Guid key = guid("e33cea40-0c59-4f76-8d89-a4ccccc40c75");
    CHECK(vnsmilegame_load((VNSmileGame *)g, 0));
    run(g, 500);
    CHECK(party_item_count(&g->b.party, key) == 1);
    CHECK(g->b.party.count == 1 && g->b.party.members[0].maxHp == 32 && g->b.party.members[0].stat[STAT_POWER] == 20);
    CHECK(member_attack(&g->b.tables, &g->b.party.members[0]) == 25);
    CHECK(member_defense(&g->b.tables, &g->b.party.members[0]) == 15);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    const char *colette = getenv("SG_COLETTE"), *sacred = getenv("SG_SACRED");
    if (colette) {
        test_new_game_party(colette, "1d9dc79e-ec79-452b-bf79-f5a7f5a51ac6", 1, 32, 10, 20, 10, 15, 0, "2b528eae-f72e-4380-bd7c-937fc9548cf7",
                            "42d3e093-74df-49f1-a6ff-aaa112554f3a");
        test_load_keeps_party(colette);
    } else {
        printf("SG_COLETTE not set: Colette checks skipped\n");
    }
    if (sacred) {
        test_new_game_party(sacred, "6e648100-18ef-4a00-a0a0-351654881c51", 1, 50, 20, 25, 20, 10, 0, "0d0ce646-5b74-448b-9ecf-6ee5cfba8183",
                            "42d3e093-74df-49f1-a6ff-aaa112554f3a");
    } else {
        printf("SG_SACRED not set: Sacred checks skipped\n");
    }
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

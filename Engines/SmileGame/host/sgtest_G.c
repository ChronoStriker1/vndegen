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

static Cmd mk(int type, int nest, int n, const i32 *ints) {
    Cmd c = {type, nest, sg_alloc(sizeof(CmdArg) * (size_t)(n ? n : 1)), n};
    for (int i = 0; i < n; i++) c.args[i] = (CmdArg){ARG_INT, ints[i], {{0}}, NULL};
    return c;
}

static Cmd mk_text(int nest, const char *text) {
    Cmd c = {CMD_MESSAGE, nest, sg_alloc(sizeof(CmdArg) * 3), 3};
    c.args[0] = (CmdArg){ARG_STR, 0, {{0}}, (char *)text};
    c.args[1] = (CmdArg){ARG_INT, 2, {{0}}, NULL};
    c.args[2] = (CmdArg){ARG_INT, 0, {{0}}, NULL};
    return c;
}

static void run_script(SG *g, Cmd *cmds, int n) {
    Script s = {.cmds = cmds, .count = n, .trigger = 2};
    interp_start(g, &s);
    for (int i = 0; i < n; i++) free(cmds[i].args);
    run(g, 300);
}

static bool message_is(SG *g, const char *text) {
    Interp *ip = &g->interp;
    return ip->active && ip->state == 1 && ip->page < ip->pageCount && strcmp(ip->pages[ip->page], text) == 0;
}

static SG *new_game(const char *dir) {
    SG *g = open_game(dir, NULL);
    if (!g) return NULL;
    run(g, 16);
    press(g, VK_OK);
    for (int i = 0; i < 400 && g->interp.active; i++) {
        press(g, VK_OK);
        run(g, 200);
    }
    return g;
}

static void inn_script(SG *g, int price) {
    i32 args[3] = {price, 1, 1};
    Cmd cmds[5] = {mk(CMD_INN, 0, 3, args), mk_text(1, "stayed"), mk(CMD_ELSE, 0, 0, NULL), mk_text(1, "left"), mk(CMD_END, 0, 0, NULL)};
    run_script(g, cmds, 5);
}

static void test_inn(const char *dir) {
    SG *g = new_game(dir);
    if (!g) {
        CHECK(false);
        return;
    }
    Party *p = &g->b.party;
    p->money = 100;
    p->members[0].hp = 1;
    inn_script(g, 30);
    CHECK(menu_page(g) && strcmp(menu_page(g), "inn") == 0);
    press(g, VK_OK);
    run(g, 400);
    CHECK(message_is(g, "stayed"));
    CHECK(p->money == 70 && p->members[0].hp == member_max_hp(&g->b.tables, &p->members[0]));
    press(g, VK_OK);
    run(g, 200);
    inn_script(g, 30);
    press(g, VK_CANCEL);
    run(g, 400);
    CHECK(message_is(g, "left"));
    CHECK(p->money == 70);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_shop(const char *dir) {
    SG *g = new_game(dir);
    if (!g) {
        CHECK(false);
        return;
    }
    Party *p = &g->b.party;
    p->money = 100;
    Guid herb = guid("48950440-0000-0000-0000-000000000000");
    for (int i = 0; i < g->b.tables.itemCount; i++)
        if (strcmp(g->b.tables.items[i].name, "薬草") == 0) herb = g->b.tables.items[i].id;
    Cmd cmds[5];
    cmds[0] = (Cmd){CMD_SHOP, 0, sg_alloc(sizeof(CmdArg) * 4), 4};
    cmds[0].args[0] = (CmdArg){ARG_INT, 1, {{0}}, NULL};
    cmds[0].args[1] = (CmdArg){ARG_GUID, 0, herb, NULL};
    cmds[0].args[2] = (CmdArg){ARG_INT, 15, {{0}}, NULL};
    cmds[0].args[3] = (CmdArg){ARG_INT, 2130706436, {{0}}, NULL};
    cmds[1] = mk_text(1, "bought");
    cmds[2] = mk(CMD_ELSE, 0, 0, NULL);
    cmds[3] = mk_text(1, "nothing");
    cmds[4] = mk(CMD_END, 0, 0, NULL);
    run_script(g, cmds, 5);
    CHECK(menu_page(g) && strcmp(menu_page(g), "shop") == 0);
    press(g, VK_OK);
    run(g, 100);
    CHECK(menu_page(g) && strcmp(menu_page(g), "shop-buy") == 0);
    press(g, VK_OK);
    run(g, 100);
    CHECK(p->money == 85 && party_item_count(p, herb) == 1);
    press(g, VK_CANCEL);
    run(g, 100);
    press(g, VK_CANCEL);
    run(g, 400);
    CHECK(message_is(g, "bought"));
    vnsmilegame_close((VNSmileGame *)g);
}

static const Res *named_res(SG *g, int kind, const char *name) {
    for (int i = 0; i < g->res.count; i++)
        if (g->res.items[i].kind == kind && strcmp(g->res.items[i].name, name) == 0) return &g->res.items[i];
    return NULL;
}

static void bgm_script(SG *g, Guid id) {
    Cmd c = {CMD_BGM, 0, sg_alloc(sizeof(CmdArg) * 4), 4};
    c.args[0] = (CmdArg){ARG_GUID, 0, id, NULL};
    c.args[1] = (CmdArg){ARG_INT, 32, {{0}}, NULL};
    c.args[2] = (CmdArg){ARG_INT, 100, {{0}}, NULL};
    c.args[3] = (CmdArg){ARG_INT, 0, {{0}}, NULL};
    run_script(g, &c, 1);
}

static void test_bgm_command(const char *dir) {
    SG *g = new_game(dir);
    if (!g) {
        CHECK(false);
        return;
    }
    const Res *drop = named_res(g, RES_BGM, "BGM06_雫");
    CHECK(drop != NULL);
    if (drop) {
        bgm_script(g, drop->id);
        Guid now = {{0}};
        CHECK(snd_bgm_playing(g, &now) && guid_eq(now, drop->id));
        bgm_script(g, (Guid){{0}});
        CHECK(!snd_bgm_playing(g, NULL));
    }
    vnsmilegame_close((VNSmileGame *)g);
}

static int npc_named(SG *g, const char *eventPrefix) {
    for (int i = 0; i < g->field.npcCount; i++) {
        char id[37];
        guid_str(g->field.npcs[i].eventId, id);
        if (strncmp(id, eventPrefix, strlen(eventPrefix)) == 0) return i;
    }
    return -1;
}

static void test_sacred_castle_load(const char *dir) {
    SG *g = open_game(dir, NULL);
    if (!g) {
        CHECK(false);
        return;
    }
    run(g, 16);
    CHECK(vnsmilegame_load((VNSmileGame *)g, 0));
    run(g, 500);
    int queen = npc_named(g, "2d01b68d"), king = npc_named(g, "e6e6e827");
    CHECK(queen >= 0 && king >= 0);
    if (queen >= 0) {
        Actor *a = &g->field.npcs[queen];
        CHECK(a->visible && a->model && a->model->rig && strcmp(a->model->rig->motion, "chair") == 0);
        CHECK(a->y == 10.0f);
    }
    if (king >= 0) {
        Actor *a = &g->field.npcs[king];
        CHECK(a->model && a->model->rig && strcmp(a->model->rig->motion, "bed") == 0);
    }
    CHECK(!g->interp.active);
    ViewPose pose;
    sg_camera_get(g, &pose);
    CHECK(pose.pitch == -48.0f && pose.yaw == 0.0f && pose.distance == 3.5f && pose.fov == 70.0f);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    const char *colette = getenv("SG_COLETTE"), *sacred = getenv("SG_SACRED");
    if (colette) {
        test_new_game_party(colette, "1d9dc79e-ec79-452b-bf79-f5a7f5a51ac6", 1, 32, 10, 20, 10, 15, 0, "2b528eae-f72e-4380-bd7c-937fc9548cf7",
                            "42d3e093-74df-49f1-a6ff-aaa112554f3a");
        test_load_keeps_party(colette);
        test_shop(colette);
        test_bgm_command(colette);
    } else {
        printf("SG_COLETTE not set: Colette checks skipped\n");
    }
    if (sacred) {
        test_new_game_party(sacred, "6e648100-18ef-4a00-a0a0-351654881c51", 1, 50, 20, 25, 20, 10, 0, "0d0ce646-5b74-448b-9ecf-6ee5cfba8183",
                            "42d3e093-74df-49f1-a6ff-aaa112554f3a");
        test_inn(sacred);
        test_sacred_castle_load(sacred);
    } else {
        printf("SG_SACRED not set: Sacred checks skipped\n");
    }
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

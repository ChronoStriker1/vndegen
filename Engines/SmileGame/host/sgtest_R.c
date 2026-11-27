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

static const HeroDef *hero_named(Tables *t, const char *name) {
    for (int i = 0; i < t->heroCount; i++)
        if (strcmp(t->heroes[i].name, name) == 0) return &t->heroes[i];
    return NULL;
}

static void wait_command(SG *g) {
    for (int k = 0; k < 2000 && sg_battle_active(g) && strcmp(battle_phase(g), "command") != 0; k++) run(g, 16);
}

static void test_back_command(const char *dir) {
    SG *g = open_game(dir, "/nonexistent-sgtest-r");
    CHECK(g != NULL);
    if (!g) return;
    run(g, 32);
    press(g, VK_OK);
    run(g, 200);
    interp_free(&g->interp);
    Tables *t = &g->b.tables;
    const HeroDef *aria = hero_named(t, "アリア"), *fafnir = hero_named(t, "ファフニール");
    CHECK(aria && fafnir);
    if (!aria || !fafnir) {
        vnsmilegame_close((VNSmileGame *)g);
        return;
    }
    g->b.party.count = 0;
    party_add_hero(g, aria->id);
    party_add_hero(g, fafnir->id);
    BattleSetup s;
    memset(&s, 0, sizeof s);
    for (int i = 0; i < t->monsterCount && !s.count; i++)
        if (strcmp(t->monsters[i].name, "スライミー") == 0) s.monsters[s.count++] = t->monsters[i].id;
    battle_seed(g, 3);
    sg_battle_start(g, &s);
    wait_command(g);
    CHECK(sg_battle_member(g) == 0);
    press(g, VK_OK);
    CHECK(strcmp(battle_phase(g), "target") == 0);
    press(g, VK_OK);
    CHECK(strcmp(battle_phase(g), "command") == 0 && sg_battle_member(g) == 1);
    press(g, VK_UP);
    CHECK(sg_battle_cursor(g) == 3);
    press(g, VK_OK);
    CHECK(strcmp(battle_phase(g), "command") == 0 && sg_battle_member(g) == 0 && sg_battle_cursor(g) == 0);
    vnsmilegame_close((VNSmileGame *)g);
}

static Member *member_named(SG *g, const char *name) {
    Party *p = &g->b.party;
    for (int i = 0; i < p->count + p->reserveCount; i++) {
        Member *m = i < p->count ? &p->members[i] : &p->reserve[i - p->count];
        const HeroDef *h = hero_def(&g->b.tables, m->hero);
        if (h && strcmp(h->name, name) == 0) return m;
    }
    return NULL;
}

static bool page_has(SG *g, const char *text) {
    Interp *ip = &g->interp;
    return ip->active && ip->page < ip->pageCount && strstr(ip->pages[ip->page], text);
}

static bool pages_have(SG *g, const char *text) {
    Interp *ip = &g->interp;
    for (int i = 0; ip->active && i < ip->pageCount; i++)
        if (strstr(ip->pages[i], text)) return true;
    return false;
}

static void test_ending_commands(const char *dir) {
    SG *g = open_game(dir, "/nonexistent-sgtest-r");
    CHECK(g != NULL);
    if (!g) return;
    run(g, 32);
    press(g, VK_OK);
    run(g, 200);
    interp_free(&g->interp);
    Tables *t = &g->b.tables;
    const char *names[] = {"アリア", "マオー", "ファフニール"};
    for (int i = 0; i < 3; i++) {
        const HeroDef *h = hero_named(t, names[i]);
        CHECK(h != NULL);
        if (h) party_add_hero(g, h->id);
    }
    Member *mao = member_named(g, "マオー"), *fafnir = member_named(g, "ファフニール");
    CHECK(mao && fafnir);
    if (!mao || !fafnir) {
        vnsmilegame_close((VNSmileGame *)g);
        return;
    }
    member_set_level(t, mao, 20);
    mao->exp = battle_exp_for_level(20);
    int fafLevel = fafnir->level, fafExp = fafnir->exp;
    g->b.party.playMs = (u64)(3 * 3600 + 25 * 60) * 1000;
    CHECK(sg_debug_warp((VNSmileGame *)g, "エンディング", 2, 3, 0));
    for (int i = 0; i < 400 && !page_has(g, "THANK YOU"); i++) {
        if (g->interp.active && g->interp.state == 4) press(g, VK_OK);
        else press(g, VK_OK);
        run(g, 100);
    }
    CHECK(page_has(g, "THANK YOU"));
    CHECK(g->vars[15] == 3 && g->vars[16] >= 25 && g->vars[16] <= 27 && g->vars[17] >= 0 && g->vars[17] < 60);
    char shown[64];
    snprintf(shown, sizeof shown, "3時間%d分%d秒", g->vars[16], g->vars[17]);
    CHECK(pages_have(g, shown) && !pages_have(g, "Variable"));
    mao = member_named(g, "マオー");
    fafnir = member_named(g, "ファフニール");
    CHECK(mao && mao->exp == 40850 + 20000 && mao->level == 22);
    CHECK(fafnir && fafnir->exp == fafExp + 20000 && fafnir->level > fafLevel);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_revive_fallen_ally(const char *dir) {
    SG *g = open_game(dir, "/nonexistent-sgtest-r");
    CHECK(g != NULL);
    if (!g) return;
    run(g, 32);
    press(g, VK_OK);
    run(g, 200);
    interp_free(&g->interp);
    Tables *t = &g->b.tables;
    const HeroDef *aria = hero_named(t, "アリア"), *mao = hero_named(t, "マオー");
    CHECK(aria && mao);
    if (!aria || !mao) {
        vnsmilegame_close((VNSmileGame *)g);
        return;
    }
    g->b.party.count = 0;
    party_add_hero(g, aria->id);
    party_add_hero(g, mao->id);
    Member *healer = &g->b.party.members[1];
    member_set_level(t, healer, 12);
    healer->mp = member_max_mp(t, healer);
    g->b.party.members[0].hp = 0;
    BattleSetup s;
    memset(&s, 0, sizeof s);
    for (int i = 0; i < t->monsterCount && !s.count; i++)
        if (strcmp(t->monsters[i].name, "スライミー") == 0) s.monsters[s.count++] = t->monsters[i].id;
    battle_seed(g, 5);
    sg_battle_start(g, &s);
    wait_command(g);
    CHECK(sg_battle_member(g) == 1);
    presses(g, VK_DOWN, 2);
    press(g, VK_OK);
    CHECK(strcmp(battle_phase(g), "list") == 0);
    press(g, 0x27);
    press(g, VK_OK);
    CHECK(strcmp(battle_phase(g), "ally") == 0);
    CHECK(strcmp(battle_bar(g), "アリア") == 0);
    press(g, VK_OK);
    wait_command(g);
    CHECK(g->b.party.members[0].hp > 0);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    const char *sacred = getenv("SG_SACRED");
    if (sacred) {
        char tmp[] = "/tmp/sgtest_R_XXXXXX";
        if (!mkdtemp(tmp)) return 1;
        test_save_cursor_after_load(sacred, tmp);
        test_back_command(sacred);
        test_ending_commands(sacred);
        test_revive_fallen_ally(sacred);
        rmdir(tmp);
    } else
        printf("SG_SACRED not set: Sacred checks skipped\n");
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

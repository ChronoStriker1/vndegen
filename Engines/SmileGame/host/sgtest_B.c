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

static Guid guid(const char *s) {
    Guid g = {{0}};
    guid_parse(s, &g);
    return g;
}

static SG *open_game(const char *dir) {
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = "/nonexistent-sgtest-b", .audioRate = 44100};
    char err[256];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) fprintf(stderr, "open %s: %s\n", dir, err);
    return g;
}

static const HeroDef *hero_named(const Tables *t, const char *name) {
    for (int i = 0; i < t->heroCount; i++)
        if (strcmp(t->heroes[i].name, name) == 0) return &t->heroes[i];
    return NULL;
}

static const ItemDef *item_named(const Tables *t, const char *name) {
    for (int i = 0; i < t->itemCount; i++)
        if (strcmp(t->items[i].name, name) == 0) return &t->items[i];
    return NULL;
}

static const SkillDef *skill_named(const Tables *t, const char *name) {
    for (int i = 0; i < t->skillCount; i++)
        if (strcmp(t->skills[i].name, name) == 0) return &t->skills[i];
    return NULL;
}

static const MonsterDef *monster_named(const Tables *t, const char *name) {
    for (int i = 0; i < t->monsterCount; i++)
        if (strcmp(t->monsters[i].name, name) == 0) return &t->monsters[i];
    return NULL;
}

static void test_colette(const char *dir) {
    SG *g = open_game(dir);
    CHECK(g != NULL);
    if (!g) return;
    Tables *t = &sgb(g)->tables;
    CHECK(t->heroCount == 6 && t->commandCount == 24 && t->itemCount == 107 && t->skillCount == 85 && t->monsterCount == 38 &&
          t->effectCount == 88);
    const GameText *x = &t->text;
    CHECK(strcmp(x->menu[MT_ITEMS], "アイテム") == 0 && strcmp(x->menu[MT_QUIT_GAME], "ゲーム終了") == 0 && strcmp(x->menu[MT_CLOSE], "閉じる") == 0);
    CHECK(strcmp(x->menu[MT_QUIT], "本当にゲームを終了しますか？") == 0 && strcmp(x->menu[MT_NEXT], "つぎのレベルまで") == 0);
    CHECK(strcmp(x->menu[MT_NONE], "なし") == 0 && strcmp(x->menu[MT_REMOVE], "はずす") == 0 && strcmp(x->menu[MT_NO_SKILLS], "スキル無し") == 0);
    CHECK(strcmp(x->config[CFG_TITLE], "コンフィグ") == 0 && strcmp(x->config[CFG_ELEMENT_ATTACK], "属性攻撃力") == 0);
    CHECK(strcmp(x->config[CFG_FAST], "速い") == 0 && strcmp(x->config[CFG_REMEMBER], "記憶する") == 0);
    CHECK(strcmp(x->money, "所持金") == 0 && strcmp(x->playTime, "プレイ時間") == 0 && strcmp(x->saveName, "セーブデータ") == 0);
    CHECK(strcmp(x->yesNo[0], "はい") == 0 && strcmp(x->yesNo[1], "いいえ") == 0 && strcmp(x->shop[SHOP_QUIT], "やめる") == 0);
    CHECK(x->elementCount == 13 && strcmp(x->elements[1], "火") == 0 && strcmp(x->elements[12], "魅了") == 0);
    CHECK(strcmp(x->battleStatus[13], "Exp") == 0);
    const Res *cursor = res_find(&g->res, x->sounds[SND_CURSOR]);
    CHECK(cursor && strcmp(cursor->name, "Cursor") == 0);

    const HeroDef *dark = hero_named(t, "ダーク");
    CHECK(dark && dark->level == 1 && dark->hp.base == 32 && dark->mp.base == 10);
    CHECK(dark && dark->skillCount == 8 && dark->commandCount == 4 && dark->equipRuleCount == 1);
    const SkillDef *cure = dark ? skill_def(t, dark->skills[0].skill) : NULL;
    CHECK(cure && strcmp(cure->name, "キュアー") == 0 && dark->skills[0].level == 2);
    const CommandDef *attack = dark ? command_def(t, dark->commands[0]) : NULL;
    CHECK(attack && strcmp(attack->name, "通常攻撃") == 0);

    const ItemDef *knife = item_named(t, "ナイフ");
    CHECK(knife && knife->kind == ITEM_WEAPON && knife->attack == 5 && knife->eq[EQ_HIT] == 95 && knife->price == 30);
    CHECK(knife && strcmp(knife->desc, "日常生活でも使える大きなナイフ") == 0);
    const ItemDef *tunic = item_named(t, "レザーチュニック");
    CHECK(tunic && tunic->kind == ITEM_BODY && tunic->eq[EQ_DEFENSE] == 5);
    const ItemDef *key = item_named(t, "地下牢のカギ");
    CHECK(key && strcmp(key->desc, "ぼろぼろに朽ちたカギ") == 0 && !key->usable[1]);
    const ItemDef *herb = item_named(t, "薬草");
    CHECK(herb && herb->kind == ITEM_EXPENDABLE && herb->price == 20 && herb->hp == 30 && herb->usable[0] && herb->usable[1]);
    const ItemDef *potion = item_named(t, "ポーション");
    CHECK(potion && potion->hpPercent == 60);
    const ItemDef *feather = item_named(t, "復活の羽");
    CHECK(feather && feather->revive && feather->price == 150);
    const ItemDef *book = item_named(t, "雷の呪文書");
    const SkillDef *storm = book ? skill_def(t, book->skill) : NULL;
    CHECK(book && book->kind == ITEM_SKILLBOOK && storm && strcmp(storm->name, "ハイサンダーストーム") == 0);
    const ItemDef *fireRing = item_named(t, "炎のガードリング");
    CHECK(fireRing && fireRing->kind == ITEM_ACCESSORY && fireRing->eq[EQ_RESIST] == 50);

    const SkillDef *heal = skill_named(t, "ヒーリング");
    CHECK(heal && heal->fieldUse && heal->battleUse && heal->mpCost == 3 && heal->target == TARGET_ALLY && heal->power == 27);
    const SkillDef *fire = skill_named(t, "ファイア");
    CHECK(fire && !fire->fieldUse && fire->target == TARGET_ENEMY);
    const SkillDef *area = skill_named(t, "エリアヒール");
    CHECK(area && area->target == TARGET_ALLIES);

    const MonsterDef *slime = monster_named(t, "スライムＬＶ01");
    CHECK(slime && slime->stats[0] == 25 && slime->money == 5 && slime->exp == 12 && slime->actionCount == 3);
    const ItemDef *drop = slime ? item_def(t, slime->drops[0].item) : NULL;
    CHECK(drop && strcmp(drop->name, "緑石") == 0 && slime->drops[0].rate == 9);

    Party *p = &sgb(g)->party;
    CHECK(p->count == 1);
    Member *m = &p->members[0];
    CHECK(guid_eq(m->hero, dark ? dark->id : (Guid){{0}}));
    CHECK(m->level == 1 && m->hp == 32 && m->mp == 10);
    CHECK(member_max_hp(t, m) == 32 && member_max_mp(t, m) == 10);
    CHECK(member_attack(t, m) == 25 && member_element_attack(t, m) == 0 && member_defense(t, m) == 15);
    CHECK(member_speed(t, m) == 15 && member_hit(t, m) == 95 && member_evasion(t, m) == 0);
    CHECK(exp_for_level(m->level + 1) - m->exp == 26);
    Member bare = *m;
    bare.equip[SLOT_WEAPON] = (Guid){{0}};
    CHECK(member_attack(t, &bare) == 20);
    CHECK(knife && member_can_equip(t, m, knife));
    const ItemDef *iceRod = item_named(t, "アイスロッド");
    CHECK(iceRod && !member_can_equip(t, m, iceRod));

    party_give_item(p, herb->id, 3);
    party_give_item(p, herb->id, -1);
    CHECK(party_item_count(p, herb->id) == 2);
    party_give_item(p, herb->id, -2);
    CHECK(party_item_count(p, herb->id) == 0 && p->itemCount == 0);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_sacred(const char *dir) {
    SG *g = open_game(dir);
    CHECK(g != NULL);
    if (!g) return;
    Tables *t = &sgb(g)->tables;
    CHECK(t->heroCount == 5 && t->commandCount == 16 && t->itemCount == 76 && t->skillCount == 27 && t->monsterCount == 53 &&
          t->effectCount == 88);
    CHECK(strcmp(t->text.menu[MT_ITEMS], "アイテム") == 0 && t->text.elementCount == 13);
    const Res *battle = res_find(&g->res, t->text.battleBgm);
    CHECK(battle && strcmp(battle->name, "BGM_battle1") == 0);
    const Res *fanfare = res_find(&g->res, t->text.sounds[SND_VICTORY]);
    CHECK(fanfare && strcmp(fanfare->name, "ME_fanfare1") == 0);

    const HeroDef *aria = hero_named(t, "アリア");
    CHECK(aria && aria->level == 2 && strcmp(aria->desc, "ルディアノ国の王女。剣の腕は父に習った") == 0);
    const Res *face = aria ? res_find(&g->res, aria->face) : NULL;
    CHECK(face && face->kind == RES_FACE && strcmp(face->name, "atena") == 0);
    const ItemDef *sword = item_named(t, "古びた剣");
    CHECK(sword && sword->attack == 10 && strcmp(sword->desc, "丁寧に使われていたがだいぶガタがきている") == 0);

    Party *p = &sgb(g)->party;
    p->count = 0;
    party_add_hero(g, aria->id);
    Member *m = &p->members[0];
    CHECK(m->level == 2 && m->hp == 55 && m->mp == 0 && member_max_hp(t, m) == 55);
    CHECK(member_attack(t, m) == 42 && member_defense(t, m) == 35 && member_speed(t, m) == 24);
    CHECK(member_hit(t, m) == 95 && member_evasion(t, m) == 0 && member_element_attack(t, m) == 0);
    CHECK(m->exp == 26 && exp_for_level(3) - m->exp == 58);
    party_add_hero(g, aria->id);
    CHECK(p->count == 1);
    party_remove_hero(g, aria->id);
    CHECK(p->count == 0);

    const MonsterDef *golem = monster_named(t, "ゴーレム");
    CHECK(golem && golem->money == 300 && golem->exp == 250 && golem->drops[0].rate == 100);
    vnsmilegame_close((VNSmileGame *)g);
}

static void frames(SG *g, int ms) {
    for (int t = 0; t < ms; t += 16) vnsmilegame_frame((VNSmileGame *)g, 16);
}

static void press(SG *g, int vk) {
    vnsmilegame_key((VNSmileGame *)g, vk, true);
    frames(g, 48);
    vnsmilegame_key((VNSmileGame *)g, vk, false);
    frames(g, 300);
}

static void shot(SG *g, const char *out, const char *name) {
    int w, h;
    const uint32_t *px = vnsmilegame_pixels((VNSmileGame *)g, &w, &h);
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.png", out, name);
    png_write(path, px, w, h);
    printf("%s %s\n", name, vnsmilegame_scene((VNSmileGame *)g));
}

static void keys(SG *g, const char *seq) {
    for (const char *c = seq; *c; c++) {
        int vk = *c == 'U' ? 0x26 : *c == 'D' ? 0x28 : *c == 'L' ? 0x25 : *c == 'R' ? 0x27 : *c == 'O' ? 0x0d : *c == 'X' ? 0x1b : 0;
        if (vk) press(g, vk);
    }
}

static int shots(const char *which, const char *dir, const char *out) {
    SG *g = open_game(dir);
    if (!g) return 1;
    bool colette = strcmp(which, "colette") == 0;
    g->scene = SCENE_FIELD;
    Tables *t = &sgb(g)->tables;
    Party *p = &sgb(g)->party;
    if (colette) {
        field_load(g, guid("94ae69b6-668a-437c-9b14-67c3cb1a6de4"), 14, 15, 2);
        party_give_item(p, item_named(t, "地下牢のカギ")->id, 1);
        p->playMs = 118000;
    } else {
        field_load(g, guid("6abf5210-1eb7-41b3-ab67-9e1fc54d1969"), 35, 28, 2);
        p->count = 0;
        party_add_hero(g, hero_named(t, "アリア")->id);
        p->playMs = 359000;
    }
    frames(g, 100);
    g->interp.active = false;
    g->fadeLevel = 0;
    keys(g, "X");
    shot(g, out, "menu");
    keys(g, "O");
    shot(g, out, "items");
    keys(g, "XDO");
    shot(g, out, "skill-who");
    keys(g, "O");
    shot(g, out, "skills");
    keys(g, "XXDO");
    shot(g, out, "equip-who");
    keys(g, "O");
    shot(g, out, "equip");
    keys(g, "O");
    shot(g, out, "equip-pick");
    keys(g, "XXXDO");
    shot(g, out, "status-who");
    keys(g, "O");
    shot(g, out, "status");
    keys(g, "XXDO");
    shot(g, out, "save");
    keys(g, "XDO");
    shot(g, out, "quit");
    keys(g, "XDO");
    shot(g, out, "config");
    keys(g, "X");
    if (colette) {
        Guid items[3] = {item_named(t, "薬草")->id, item_named(t, "毒消しハーブ")->id, item_named(t, "ロングソード")->id};
        i32 prices[3] = {20, 10, 150};
        p->money = 500;
        keys(g, "X");
        shop_open(g, items, prices, 3);
        frames(g, 300);
        shot(g, out, "shop");
        keys(g, "O");
        shot(g, out, "shop-buy");
        keys(g, "O");
        shot(g, out, "shop-bought");
        keys(g, "XX");
        inn_open(g, 20);
        frames(g, 300);
        shot(g, out, "inn");
        keys(g, "X");
    }
    vnsmilegame_close((VNSmileGame *)g);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 5 && strcmp(argv[1], "shots") == 0) return shots(argv[2], argv[3], argv[4]);
    const char *colette = getenv("SG_COLETTE"), *sacred = getenv("SG_SACRED");
    if (colette) test_colette(colette);
    else printf("SG_COLETTE not set: Colette table checks skipped\n");
    if (sacred) test_sacred(sacred);
    else printf("SG_SACRED not set: Sacred table checks skipped\n");
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

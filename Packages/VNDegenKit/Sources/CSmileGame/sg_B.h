#ifndef SMILEGAME_B_H
#define SMILEGAME_B_H

typedef struct GrowStat {
    i32 base;
    float growth, curve;
} GrowStat;

enum { STAT_POWER, STAT_VITALITY, STAT_MAGIC, STAT_SPEED, STAT_COUNT };

enum { SLOT_WEAPON, SLOT_SHIELD, SLOT_HEAD, SLOT_BODY, SLOT_ACC1, SLOT_ACC2, SLOT_COUNT };

typedef struct HeroSkill {
    Guid skill;
    i32 level;
} HeroSkill;

typedef struct HeroEquipRule {
    Guid item;
    u8 flag;
} HeroEquipRule;

typedef struct HeroDef {
    Guid id;
    char *name;
    Guid graphic;
    i32 level;
    GrowStat stats[STAT_COUNT];
    GrowStat hp, mp;
    HeroSkill *skills;
    int skillCount;
    HeroEquipRule *equipRules;
    int equipRuleCount;
    Guid equip[5];
    Guid *commands;
    int commandCount;
    char *desc;
    Guid face;
} HeroDef;

typedef struct CommandDef {
    Guid id;
    char *name;
    char *desc;
    Guid icon;
    u8 iconX, iconY;
    i32 kind, rate, flag, priority;
} CommandDef;

enum {
    ITEM_EXPENDABLE = 1,
    ITEM_SKILLBOOK = 2,
    ITEM_WEAPON = 3,
    ITEM_SHIELD = 4,
    ITEM_HEAD = 5,
    ITEM_BODY = 6,
    ITEM_ACCESSORY = 7,
};

enum { EQ_DEFENSE = 0, EQ_HP = 2, EQ_MP = 3, EQ_POWER = 4, EQ_SPEED = 7, EQ_EVASION = 8, EQ_HIT = 9, EQ_RESIST = 11, EQ_STATE_RESIST = 17, EQ_COUNT = 24 };

typedef struct ItemDef {
    Guid id;
    char *name;
    char *desc;
    i32 price;
    Guid icon;
    u8 iconX, iconY;
    i32 kind;
    Guid effect;
    i32 hp, mp, hpPercent, mpPercent;
    u8 cure[5];
    u8 usable[2];
    u8 revive;
    Guid skill;
    i32 eq[EQ_COUNT];
    i32 eqExtra;
    i32 attack;
    i32 element, elementAttack;
    Guid model;
    const u8 *block;
    i32 blockSize;
} ItemDef;

enum { TARGET_ALLY = 0, TARGET_ALLIES = 1, TARGET_ENEMY = 2, TARGET_ENEMIES = 3, TARGET_SELF = 4 };

typedef struct SkillDef {
    Guid id;
    char *name;
    char *desc;
    Guid icon;
    u8 iconX, iconY;
    u8 fieldUse, battleUse;
    i32 mpCost;
    i32 target;
    Guid effect;
    i32 power;
    const u8 *use;
    i32 useSize;
    const u8 *act;
    i32 actSize;
} SkillDef;

typedef struct MonsterAction {
    i32 cond, a, b, rate, kind;
    Guid skill;
    i32 priority;
} MonsterAction;

typedef struct MonsterDrop {
    Guid item;
    i32 rate;
} MonsterDrop;

typedef struct MonsterDef {
    Guid id;
    char *name;
    Guid graphic;
    Guid attackEffect;
    char *desc;
    i32 stats[10];
    i32 resist[13];
    MonsterAction *actions;
    int actionCount;
    i32 exp, money;
    MonsterDrop drops[2];
    i32 tail;
} MonsterDef;

typedef struct EffectDef {
    Guid id;
    char *name;
    const u8 *body;
    i32 size;
} EffectDef;

enum {
    SND_VICTORY,
    SND_GAMEOVER,
    SND_START,
    SND_CANCEL,
    SND_DECIDE,
    SND_CURSOR,
    SND_ITEM,
    SND_STATUS_UP,
    SND_COUNT = 10,
};

enum {
    MT_RESULT,
    MT_SAVED,
    MT_OVERWRITE,
    MT_QUIT,
    MT_DECIDE,
    MT_CURSOR,
    MT_CHOOSE_CHAR,
    MT_CHANGE,
    MT_CLOSE,
    MT_HIT,
    MT_COST,
    MT_NO_ITEMS,
    MT_EXPENDABLE,
    MT_WEAPON,
    MT_ATTACK,
    MT_BODY,
    MT_DEFENSE,
    MT_SHIELD,
    MT_EVASION,
    MT_HEAD,
    MT_ACCESSORY,
    MT_OWNED,
    MT_ITEMS,
    MT_SKILLS,
    MT_EQUIP,
    MT_STATUS,
    MT_SAVE,
    MT_QUIT_GAME,
    MT_USE,
    MT_DISCARD,
    MT_LV,
    MT_HP,
    MT_MP,
    MT_STATE,
    MT_NORMAL,
    MT_POISON,
    MT_DOWN,
    MT_WHO_USES,
    MT_NO_SKILLS,
    MT_WHO_EQUIPS,
    MT_REMOVE,
    MT_NONE,
    MT_WHO_STATUS,
    MT_POWER,
    MT_VITALITY,
    MT_MAGIC,
    MT_SPEED,
    MT_EXP,
    MT_NEXT,
    MT_GAMEOVER,
    MT_COUNT,
};

enum { SHOP_STAY, SHOP_LEAVE, SHOP_NO_MONEY, SHOP_BUY, SHOP_SELL, SHOP_QUIT, SHOP_BOUGHT, SHOP_SOLD, SHOP_COUNT };

enum {
    CFG_INSTANT,
    CFG_FAST,
    CFG_NORMAL,
    CFG_SLOW,
    CFG_REMEMBER,
    CFG_FORGET,
    CFG_BGM,
    CFG_SE,
    CFG_MSG_SPEED,
    CFG_CURSOR,
    CFG_RESET,
    CFG_TITLE,
    CFG_ELEMENT_ATTACK,
    CFG_COUNT,
};

typedef struct GameText {
    Guid windows[6];
    char *shop[SHOP_COUNT];
    char **elements;
    int elementCount;
    char *menu[MT_COUNT];
    Guid battleBgm;
    char *battle[23];
    Guid sounds[SND_COUNT];
    char *textColor;
    char *yesNo[2];
    char *config[CFG_COUNT];
    char *money, *playTime;
    char *battleStatus[14];
    char *saveName;
} GameText;

typedef struct Tables {
    Sgr itemFile, skillFile, monsterFile, effectFile;
    HeroDef *heroes;
    int heroCount;
    CommandDef *commands;
    int commandCount;
    ItemDef *items;
    int itemCount;
    SkillDef *skills;
    int skillCount;
    MonsterDef *monsters;
    int monsterCount;
    EffectDef *effects;
    int effectCount;
    GameText text;
} Tables;

bool tables_load(SG *g, Tables *t);
void tables_free(Tables *t);
bool gametext_parse(GameText *t, const SgrRec *rec);
bool hero_parse(HeroDef *h, const SgrRec *rec);
bool item_parse(ItemDef *it, const SgrRec *rec);
bool skill_parse(SkillDef *s, const SgrRec *rec);
bool monster_parse(MonsterDef *m, const SgrRec *rec);
const HeroDef *hero_def(const Tables *t, Guid id);
const ItemDef *item_def(const Tables *t, Guid id);
const SkillDef *skill_def(const Tables *t, Guid id);
const MonsterDef *monster_def(const Tables *t, Guid id);
const CommandDef *command_def(const Tables *t, Guid id);
int grow_stat(const GrowStat *s, int level);
int exp_for_level(int level);

typedef struct Member {
    Guid hero;
    i32 level, exp, hp, mp;
    i32 maxHp, maxMp, stat[STAT_COUNT];
    Guid equip[SLOT_COUNT];
    u8 state;
} Member;

typedef struct Look {
    Guid hero, face, graphic;
} Look;

typedef struct Stack {
    Guid item;
    i32 count;
} Stack;

typedef struct Party {
    Member members[8];
    int count;
    Member reserve[16];
    int reserveCount;
    Stack *items;
    int itemCount, itemCap;
    Guid *seen;
    int seenCount;
    Look *looks;
    int lookCount;
    i32 money;
    u64 playMs;
} Party;

void party_new(SG *g);
void party_clear(Party *p);
void party_add_hero(SG *g, Guid hero);
void party_remove_hero(SG *g, Guid hero);
bool party_has_hero(const Party *p, Guid hero);
int party_item_count(const Party *p, Guid item);
void party_give_item(Party *p, Guid item, int delta);
void party_set_look(Party *p, Guid hero, Guid face, Guid graphic);
Guid party_leader_graphic(SG *g);
void party_recover(SG *g);
void member_init(const Tables *t, Member *m, const HeroDef *h);
int member_max_hp(const Tables *t, const Member *m);
int member_max_mp(const Tables *t, const Member *m);
int member_stat(const Tables *t, const Member *m, int stat);
int member_equip_sum(const Tables *t, const Member *m, int eq);
int member_attack(const Tables *t, const Member *m);
int member_defense(const Tables *t, const Member *m);
int member_speed(const Tables *t, const Member *m);
int member_element_attack(const Tables *t, const Member *m);
int member_hit(const Tables *t, const Member *m);
int member_evasion(const Tables *t, const Member *m);
bool member_can_equip(const Tables *t, const Member *m, const ItemDef *it);
int slot_kind(int slot);

enum { MENU_CLOSED, MENU_MAIN, MENU_SHOP, MENU_INN };

typedef struct Menu Menu;

typedef struct SgB {
    Tables tables;
    Party party;
    Menu *menu;
    bool loaded;
} SgB;

SgB *sgb(SG *g);
void sgb_open(SG *g);
void sgb_close(SG *g);
bool menu_frame(SG *g, u32 ms);
bool menu_active(SG *g);
void menu_open(SG *g);
void shop_open(SG *g, const Guid *items, const i32 *prices, int count);
bool sgb_quit(SG *g);
bool menu_result(SG *g);
void inn_open(SG *g, int price);
const char *menu_page(SG *g);
void sgb_sound(SG *g, int which);

#endif

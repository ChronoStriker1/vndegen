#ifndef SMILEGAME_C_H
#define SMILEGAME_C_H

enum { BATTLE_MAX_MONSTERS = 8 };

enum { BATTLE_NONE, BATTLE_WON, BATTLE_LOST, BATTLE_ESCAPED };

typedef struct BattleSetup {
    Guid monsters[BATTLE_MAX_MONSTERS];
    int count;
    bool continueOnLose;
    bool noEscape;
    i32 flag;
    Guid bgm;
    Guid background;
    bool hasCenter;
    int centerX, centerZ;
    bool hasLayout;
    int layout[BATTLE_MAX_MONSTERS][2];
} BattleSetup;

enum { BATTLE_MAX_ENCOUNTERS = 16 };

typedef struct BattleEncounterEntry {
    Guid monster;
    i32 a, b;
} BattleEncounterEntry;

typedef struct BattleMapInfo {
    i32 rate;
    BattleEncounterEntry entries[BATTLE_MAX_ENCOUNTERS];
    int count;
    Guid background;
    Guid bgm;
} BattleMapInfo;

typedef struct BattleEnc {
    Guid map;
    bool valid;
    BattleMapInfo info;
    int cellX, cellZ;
    int steps, next;
    u32 rng;
} BattleEnc;

bool battle_map_info(SG *g, Guid mapId, BattleMapInfo *out);
Guid battle_bgm_for(SG *g, const BattleSetup *s, Guid mapId);
Guid battle_background_for(SG *g, const BattleSetup *s, Guid mapId);
bool battle_encounter_roll(BattleEnc *e);
void battle_encounter_setup(SG *g, BattleEnc *e, BattleSetup *s);
void battle_encounter_tick(SG *g);
BattleEnc *battle_enc(SG *g);

typedef struct BattleSkillInfo {
    u8 states;
    bool drain;
    i32 element, hit;
} BattleSkillInfo;

BattleSkillInfo battle_skill_info(const SkillDef *s);

typedef struct BattleFxDef {
    Image *image;
    int cols, rows, cells, frames;
    Guid sound;
} BattleFxDef;

bool battle_fx_def(SG *g, Guid effect, BattleFxDef *out);
u32 battle_fx_ms(const BattleFxDef *d);
void battle_fx_draw(SG *g, const BattleFxDef *d, u32 ms, int cx, int cy);

bool battle_setup_cmd(BattleSetup *s, const Cmd *c);
void sg_battle_start(SG *g, const BattleSetup *s);
bool sg_battle_active(SG *g);
int sg_battle_result(SG *g);
bool battle_frame(SG *g, u32 ms);
void battle_close(SG *g);
void battle_abort(SG *g);
void battle_seed(SG *g, u32 seed);
const char *battle_phase(SG *g);
int battle_monster_hp(SG *g, int index);
int sg_battle_member(SG *g);
int sg_battle_cursor(SG *g);
const char *battle_bar(SG *g);

int battle_attack_damage(int attack, int defense, float spread);
int battle_skill_damage(int power, int magic, int defense, float spread);
float battle_spread(u32 *rng);
int battle_exp_for_level(int level);
bool battle_escape_roll(u32 *rng, int partySpeed, int monsterSpeed);

#endif

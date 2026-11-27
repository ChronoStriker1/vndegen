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

bool battle_setup_cmd(BattleSetup *s, const Cmd *c);
void sg_battle_start(SG *g, const BattleSetup *s);
bool sg_battle_active(SG *g);
int sg_battle_result(SG *g);
bool battle_frame(SG *g, u32 ms);
void battle_close(SG *g);
void battle_seed(SG *g, u32 seed);
const char *battle_phase(SG *g);
int battle_monster_hp(SG *g, int index);
const char *battle_bar(SG *g);

int battle_attack_damage(int attack, int defense, float spread);
int battle_skill_damage(int power, int magic, int defense, float spread);
float battle_spread(u32 *rng);
int battle_exp_for_level(int level);
bool battle_escape_roll(u32 *rng, int partySpeed, int monsterSpeed);

#endif

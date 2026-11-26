#include "sa.h"

#include <string.h>

enum {
    GROUP_GAIN = 55,
    GROUP_EXP = 63,
    UNIT_GAIN = 68,
    UNIT_EXP = 72,
    UNIT_NAME = 2,
    UNIT_FIELD_ABILITY = 34,
    UNIT_RESERVE_ABILITY = 35,
    ABILITY_NAME = 1,
    ABILITY_AMOUNT = 4,
};

static u32 next(u32 *rng) {
    *rng ^= *rng << 13;
    *rng ^= *rng >> 17;
    *rng ^= *rng << 5;
    return *rng;
}

static int roll(u32 *rng, int lo, int hi) { return hi <= lo ? lo : lo + (int)(next(rng) % (u32)(hi - lo + 1)); }

static int scale(int v, int percent) { return v > 0 ? (v * percent + 99) / 100 : 0; }

void sa_reward_roll(BattleReward *r, const Table *groups, int group, const Table *units, const int *enemyRows, int enemies, u32 *rng) {
    memset(r->gain, 0, sizeof r->gain);
    memset(r->leveled, 0, sizeof r->leveled);
    r->exp = 0;
    for (int i = 0; i < enemies; i++) {
        int u = enemyRows[i];
        for (int k = 0; k < REWARD_KINDS; k++) {
            int lo = sa_table_int(groups, group, GROUP_GAIN + 2 * k), hi = sa_table_int(groups, group, GROUP_GAIN + 2 * k + 1);
            int own = sa_table_int(units, u, UNIT_GAIN + k);
            r->gain[k] += k == REWARD_RESIDENTS ? roll(rng, lo, hi + own) : roll(rng, lo, hi) + own;
        }
        r->exp += sa_table_int(units, u, UNIT_EXP) + roll(rng, sa_table_int(groups, group, GROUP_EXP), sa_table_int(groups, group, GROUP_EXP + 1));
    }
    int percent = 100 + r->strength * 10;
    for (int k = 0; k < REWARD_KINDS; k++) r->gain[k] = scale(r->gain[k], percent + (k == REWARD_RESIDENTS ? r->recruit : r->search));
    if (percent + r->recruit > 100 && r->gain[REWARD_RESIDENTS] < 1) r->gain[REWARD_RESIDENTS] = 1;
    if (r->noResidents) r->gain[REWARD_RESIDENTS] = 0;
    r->exp = scale(r->exp, percent);
}

static int ability_amount(const Party *p, const char *name) {
    for (int i = 0; i < p->abilities.rows; i++)
        if (!strcmp(sa_table_text(&p->abilities, i, ABILITY_NAME), name)) return sa_table_int(&p->abilities, i, ABILITY_AMOUNT);
    return 0;
}

static bool has(const Party *p, int unit, int col, const char *name) {
    return unit && !strcmp(sa_table_text(&p->units, sa_party_unit_row(p, unit), col), name);
}

int sa_reward_member_exp(const BattleReward *r, const Party *p, int slot) {
    int percent = 100;
    for (int i = 0; i < PARTY_SLOTS; i++)
        if (has(p, p->unit[i], UNIT_FIELD_ABILITY, "学")) percent += ability_amount(p, "学");
    for (int i = 0; i < p->benchCount; i++)
        if (p->bench[i].tile >= 0 && has(p, p->bench[i].unit, UNIT_RESERVE_ABILITY, "学")) percent += ability_amount(p, "学");
    if (has(p, p->unit[slot], UNIT_FIELD_ABILITY, "勤勉")) percent += ability_amount(p, "勤勉");
    return r->exp * percent / 100;
}

int sa_reward_lines(const BattleReward *r, const Party *p, char **lines, int max) {
    static const char *const fmt[REWARD_KINDS] = {
        POP_PINK "住民" POP_WHITE "が" POP_PINK "%d人" POP_WHITE "増えた",
        POP_RED "食料" POP_WHITE "を" POP_RED "%d食" POP_WHITE "手に入れた",
        POP_YELLOW "資金" POP_WHITE "を" POP_YELLOW "%dB" POP_WHITE "手に入れた",
        POP_GREEN "資源" POP_WHITE "を" POP_GREEN "%d材" POP_WHITE "手に入れた",
    };
    int n = 0;
    for (int k = 0; k < REWARD_KINDS && n < max; k++)
        if (r->gain[k] > 0) lines[n++] = sa_fmt(fmt[k], r->gain[k]);
    if (n < max) lines[n++] = sa_fmt(POP_YELLOW "%dP" POP_WHITE "の経験値を獲得した", r->exp);
    for (int s = 0; s < PARTY_SLOTS && n < max; s++)
        if (r->leveled[s] && p->unit[s])
            lines[n++] = sa_fmt(POP_YELLOW "%s" POP_WHITE "はレベルが上がった！", sa_table_text(&p->units, sa_party_unit_row(p, p->unit[s]), UNIT_NAME));
    return n;
}

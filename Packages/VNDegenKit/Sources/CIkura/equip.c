#include "ikura.h"

enum {
    V_SPEED = 153,
    V_HAND_R = 162,
    V_HAND_L = 163,
    V_ARMOR = 164,
    V_ACCESSORY = 165,
    V_HIT_R = 166,
    V_ATTACK_R = 167,
    V_HIT_L = 168,
    V_ATTACK_L = 169,
    V_DEFENSE = 170,
    V_R_WEAPON = 171,
    V_R_SHIELD = 172,
    V_R_WEAPON_MOD = 173,
    V_R_SHIELD_MOD = 174,
    V_L_WEAPON = 175,
    V_L_SHIELD = 176,
    V_L_WEAPON_MOD = 177,
    V_L_SHIELD_MOD = 178,
    V_ARMOR_DEF = 179,
    V_ARMOR_MOD = 180,
    V_ACC_SPEED = 232,
    V_ACC_DEF = 233,
    V_BASE_HIT = 150,
    V_BASE_SPEED = 151,
    V_LEVEL = 154,
};

#define HAND_ITEMS 20
#define FIRST_SHIELD 13
#define ARMORS 10
#define ACCESSORIES 6

static const signed char hand[HAND_ITEMS + 1][2] = {{0, 0},  {2, 0},  {4, 0}, {6, -3}, {8, -4}, {5, 1},  {7, -5},
                                                     {6, 5},  {7, -3}, {8, 3}, {11, -2}, {10, 1}, {14, -7}, {2, 0},
                                                     {4, -1}, {5, 0},  {3, 0}, {7, -2},  {6, -3}, {8, 0},  {10, -4}};
static const signed char armor[ARMORS + 1][2] = {{0, 0},  {2, 0}, {4, -3}, {5, -4}, {6, -7}, {7, -6},
                                                 {5, 3},  {6, -3}, {8, -5}, {8, -1}, {10, -7}};
static const signed char accessory[ACCESSORIES + 1][2] = {{0, 0}, {0, 0}, {2, 0}, {0, 5}, {0, 0}, {0, 0}, {-4, 8}};

static const int saved[22] = {V_HAND_R,       V_HAND_L,       V_ARMOR,        V_ACCESSORY, V_HIT_R,     V_ATTACK_R,
                              V_HIT_L,        V_ATTACK_L,     V_SPEED,        V_DEFENSE,   V_R_WEAPON,  V_R_SHIELD,
                              V_R_WEAPON_MOD, V_R_SHIELD_MOD, V_L_WEAPON,     V_L_SHIELD,  V_L_WEAPON_MOD,
                              V_L_SHIELD_MOD, V_ARMOR_DEF,    V_ARMOR_MOD,    V_ACC_SPEED, V_ACC_DEF};

static i32 get(IK *t, int v) { return v < t->varCount ? t->vars[v] : 0; }

static void put(IK *t, int v, i32 x) {
    if (v < t->varCount) t->vars[v] = x;
}

static bool two_handed(int item) { return item == 4 || item == 10 || item == 12; }

static void hand_stats(IK *t, int item, int weapon, int shield, int weaponMod, int shieldMod) {
    bool ok = item > 0 && item <= HAND_ITEMS;
    bool isShield = ok && item >= FIRST_SHIELD;
    put(t, weapon, ok && !isShield ? hand[item][0] : 0);
    put(t, weaponMod, ok && !isShield ? hand[item][1] : 0);
    put(t, shield, isShield ? hand[item][0] : 0);
    put(t, shieldMod, isShield ? hand[item][1] : 0);
}

static void recompute(IK *t) {
    if (t->varCount <= V_ACC_DEF) return;
    hand_stats(t, get(t, V_HAND_R), V_R_WEAPON, V_R_SHIELD, V_R_WEAPON_MOD, V_R_SHIELD_MOD);
    hand_stats(t, get(t, V_HAND_L), V_L_WEAPON, V_L_SHIELD, V_L_WEAPON_MOD, V_L_SHIELD_MOD);
    int a = get(t, V_ARMOR), c = get(t, V_ACCESSORY);
    bool aOk = a > 0 && a <= ARMORS, cOk = c > 0 && c <= ACCESSORIES;
    put(t, V_ARMOR_DEF, aOk ? armor[a][0] : 0);
    put(t, V_ARMOR_MOD, aOk ? armor[a][1] : 0);
    put(t, V_ACC_SPEED, cOk ? accessory[c][0] : 0);
    put(t, V_ACC_DEF, cOk ? accessory[c][1] : 0);
    i32 base = get(t, V_BASE_HIT), level = get(t, V_LEVEL);
    put(t, V_HIT_R, base + level + get(t, V_R_WEAPON_MOD));
    put(t, V_ATTACK_R, 4 * level + 5 * get(t, V_R_WEAPON));
    put(t, V_HIT_L, base + level + get(t, V_L_WEAPON_MOD) - 4);
    put(t, V_ATTACK_L, 4 * level + 5 * get(t, V_L_WEAPON));
    put(t, V_DEFENSE, 3 * level + 5 * get(t, V_ARMOR_DEF) + get(t, V_R_SHIELD) + get(t, V_L_SHIELD) + get(t, V_ACC_DEF));
    put(t, V_SPEED, get(t, V_BASE_SPEED) + level + get(t, V_ARMOR_MOD) + get(t, V_ACC_SPEED) + get(t, V_R_SHIELD_MOD) +
                        get(t, V_L_SHIELD_MOD));
}

static void equip(IK *t, int slot, int item, bool remove) {
    static const int limit[4] = {HAND_ITEMS, HAND_ITEMS, ARMORS, ACCESSORIES};
    if (slot < 0 || slot > 3) return;
    int v = V_HAND_R + slot;
    if (remove) {
        if (slot < 2 && two_handed(get(t, v))) put(t, V_HAND_R + (1 - slot), 0);
        put(t, v, 0);
        return;
    }
    bool ok = item >= 0 && item <= limit[slot];
    put(t, v, ok ? item : 0);
    if (ok && slot < 2 && two_handed(item)) put(t, V_HAND_R + (1 - slot), item);
}

bool equip_exec(IK *t, u16 op, const u8 *a, int n) {
    switch (op) {
    case 0x0f00:
    case 0x0f01: {
        int base = (int)ik_value(t, rd32(a));
        for (int k = 0; k < 22; k++) {
            if (base + k < 0 || base + k >= t->varCount) break;
            if (op == 0x0f00) {
                int item = get(t, k < 6 ? V_HAND_R : V_HAND_L);
                bool armed = item > 0 && item < FIRST_SHIELD;
                t->vars[base + k] = k >= 4 && k < 8 && !armed ? 0 : get(t, saved[k]);
            }
            else put(t, saved[k], t->vars[base + k]);
        }
        break;
    }
    case 0x0f03: recompute(t); break;
    case 0x0f04:
        if (n >= 12) equip(t, (int)ik_value(t, rd32(a)), (int)ik_value(t, rd32(a + 4)), ik_value(t, rd32(a + 8)) != 0);
        break;
    default: return false;
    }
    return true;
}

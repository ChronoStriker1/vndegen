#include "sa.h"

enum {
    ART_BAR,
    ART_BACKS,
    ART_FORWARD,
    ART_START,
    ART_AUTO,
    ART_GAUGE,
    ART_LOG,
    ART_NONE_ACTION,
    ART_NONE_TARGET,
    ART_HP,
    ART_MP,
    ART_SLOT,
    ART_EXP,
    ART_ACT_HEAD,
    ART_ACT_BOTTOM,
    ART_ACT_ROW,
    ART_AIM_HEAD,
    ART_AIM_BOTTOM,
    ART_AIM_ROW,
    ART_ICONS,
    ART_GAUGE_OFF,
    ART_GAUGE_VARY,
    ART_NAMED,
    ART_MODE0 = ART_NAMED,
    ART_AIM0 = ART_MODE0 + 7,
    ART_SIDE0 = ART_AIM0 + AIM_COUNT,
    ART_COUNT = ART_SIDE0 + 12,
};

static const char *const artNames[ART_NAMED] = {
    "bat_a00.agf", "bat_j01.agf", "bat_j00.agf", "bat_l00.agf", "bat_l01.agf", "bat_p00.agf", "bat_m00.agf",
    "bat_f99.agf", "bat_h99.agf", "bat_k01.agf", "bat_k02.agf", "bat_j02.agf", "bat_k03.agf", "bat_e01.agf",
    "bat_e02.agf", "bat_e03.agf", "bat_g01.agf", "bat_g02.agf", "bat_g03.agf", "pun_d00.agf", "bat_p01.agf", "bat_p02.agf",
};

static const char *const iconOrder[] = {"斬", "打", "射", "無", "光", "闇", "火", "水", "土", "風", "雷", "回", "異"};

static const int pickModes[] = {ORDER_FREE, ORDER_ATTACK, ORDER_DEFEND, ORDER_SUPPORT, ORDER_SWAP, ORDER_FLEE};
static const int pickAims[] = {AIM_FRONT, AIM_MIDDLE, AIM_BACK, AIM_RIGHT, AIM_CENTRE, AIM_LEFT};

enum {
    PARTY_BOARD_X = 193,
    PARTY_BOARD_Y = 299,
    GRID_X = 627,
    GRID_Y = 202,
    STEP_X = 57,
    STEP_Y = 29,
    ROW_X = -81,
    ROW_Y = 41,
    GRID_ROWS = 3,
    GRID_COLS = 5,
    BACKS_X = 103,
    FORWARD_X = 603,
    PANEL_Y = 541,
    SLOT_X = 13,
    BOX_X = 6,
    BOX_Y = 563,
    SLOT_PITCH = 163,
    LABEL_Y = 569,
    TARGET_Y = 595,
    LABEL_W = 148,
    LABEL_H = 20,
    HP_X = 47,
    HP_Y = 634,
    MP_Y = 658,
    EXP_Y = 674,
    EXP_W = 100,
    LV_RIGHT = 44,
    LV_Y = 666,
    NAME_X = 76,
    FLOOR_NAME_X = 300,
    NAME_Y = 15,
    START_X = 500,
    START_Y = 497,
    START_W = 204,
    START_H = 44,
    AUTO_X = 706,
    GAUGE_X = 998,
    TOGGLE_Y = 510,
    TOGGLE_W = 104,
    TOGGLE_H = 29,
    LOG_X = 1105,
    LOG_Y = 270,
    LOG_TEXT_X = 1113,
    LOG_TEXT_Y = 302,
    LOG_PITCH = 18,
    LOG_INDENT = 16,
    LOG_LINES = 23,
    LOG_SIZE = 12,
    SIDE_X = 0,
    SIDE_W = 100,
    SIDE_H = 19,
    PICK_OFFSET = 158,
    PICK_W = 164,
    PICK_ROW = 29,
    PICK_BOTTOM = 716,
    PICK_HEAD = 23,
    PICK_INSET_X = 7,
    PICK_INSET_Y = 4,
    ICON = 20,
    SKILL_SIZE = 12,
    ACTION_MS = 1500,
    TILE_PLAIN = 0,
    TILE_BLUE = 2,
    TILE_YELLOW = 3,
    HOVER_START = 1,
    HOVER_AUTO = 2,
    HOVER_GAUGE = 3,
    HOVER_SIDE = 100,
    HOVER_PICK = 200,
    HOVER_LABEL = 300,
};

#define NO_EDGE 0xffffffffu

static const int sideY[12] = {414, 434, 474, 494, 514, 554, 574, 594, 634, 654, 674, 694};

static const struct {
    int x, y;
} boards[4] = {{0, 0}, {406, 116}, {326, 130}, {250, 80}};

static const char *text_of(const Table *t, int row, int col) { return sa_table_text(t, row, col); }

static int row_by_text(const Table *t, int col, const char *s) {
    for (int r = 0; r < t->rows; r++)
        if (!strcmp(text_of(t, r, col), s)) return r;
    return -1;
}

static int growth(const Table *lvup, const char *type, const char *stat, int level) {
    int r = row_by_text(lvup, 1, type);
    if (r < 0 || level <= 1) return 0;
    const char *s = text_of(lvup, r, 24);
    const char *p = strstr(s, stat);
    if (!p) return 0;
    int add = 0, per = 0;
    if (sscanf(p + strlen(stat), "+%d/%d", &add, &per) != 2 || per <= 0) return 0;
    return (level - 1) / per * add;
}

static void fighter_stats(Battle *b, Fighter *f) {
    const Table *u = &b->party->units;
    const char *type = text_of(u, f->row, 19);
    for (int i = 0; i < 8; i++) f->stat[i] = sa_table_int(u, f->row, 25 + i);
    f->stat[STAT_ATTACK] += growth(&b->lvup, type, "攻撃", f->level);
    f->stat[STAT_DEFENCE] += growth(&b->lvup, type, "防御", f->level);
    f->stat[STAT_SPEED] += growth(&b->lvup, type, "速度", f->level);
    f->stat[STAT_EVASION] += growth(&b->lvup, type, "回避", f->level);
    f->maxHp = sa_table_int(u, f->row, 23);
    f->maxMp = sa_table_int(u, f->row, 24);
}

static Image *sprite_for(Vfs *vfs, int number, bool enemy) {
    char name[24];
    snprintf(name, sizeof name, enemy ? "u%04da2.agf" : "u%04da0.agf", number);
    Image *im = sa_load_image(vfs, name);
    if (!im && enemy) {
        snprintf(name, sizeof name, "u%04da0.agf", number);
        im = sa_load_image(vfs, name);
    }
    return im;
}

bool sa_battle_init(Battle *b, Vfs *vfs, Party *party) {
    memset(b, 0, sizeof *b);
    b->party = party;
    if (!sa_table_load(&b->groups, vfs, "party.obj") || !sa_table_load(&b->skills, vfs, "skill.obj") ||
        !sa_table_load(&b->lvup, vfs, "lvup.obj") || !sa_table_load(&b->items, vfs, "item.obj"))
        return false;
    sa_popup_load(b->pop, vfs);
    for (int i = 0; i < ART_NAMED; i++) b->art[i] = sa_load_image(vfs, artNames[i]);
    char name[16];
    for (int i = 0; i < 7; i++) {
        snprintf(name, sizeof name, "bat_f%02d.agf", i);
        b->art[ART_MODE0 + i] = sa_load_image(vfs, name);
    }
    for (int i = 0; i < AIM_COUNT; i++) {
        snprintf(name, sizeof name, "bat_h%02d.agf", i);
        b->art[ART_AIM0 + i] = sa_load_image(vfs, name);
    }
    for (int i = 0; i < 12; i++) {
        snprintf(name, sizeof name, "bat_d%02d.agf", i);
        b->art[ART_SIDE0 + i] = sa_load_image(vfs, name);
    }
    return true;
}

static void clear_fighters(Battle *b) {
    for (int i = 0; i < b->count; i++) sa_image_free(b->f[i].sprite);
    memset(b->f, 0, sizeof b->f);
    b->count = 0;
    for (int i = 0; i < b->logCount; i++) free(b->log[i].text);
    b->logCount = 0;
}

void sa_battle_free(Battle *b) {
    clear_fighters(b);
    for (int i = 0; i < BATTLE_ART; i++) sa_image_free(b->art[i]);
    for (int i = 0; i < 4; i++) sa_image_free(b->board[i]);
    for (int i = 0; i < 4; i++) sa_image_free(b->pop[i]);
    sa_image_free(b->back);
    sa_table_free(&b->groups);
    sa_table_free(&b->skills);
    sa_table_free(&b->lvup);
    sa_table_free(&b->items);
    memset(b, 0, sizeof *b);
}

static u32 rnd(Battle *b) {
    b->rng ^= b->rng << 13;
    b->rng ^= b->rng >> 17;
    b->rng ^= b->rng << 5;
    return b->rng;
}

static int range(Battle *b, int lo, int hi) { return hi <= lo ? lo : lo + (int)(rnd(b) % (u32)(hi - lo + 1)); }

static bool board_has(const Battle *b, int col) { return b->boardSize != 1 || (col >= 1 && col <= 3); }

static void add_enemy(Battle *b, Vfs *vfs, const char *key, int row, int col) {
    if (b->count >= BATTLE_FIGHTERS) return;
    int r = row_by_text(&b->party->units, 1, key);
    if (r < 0) return;
    Fighter *f = &b->f[b->count++];
    f->enemy = true;
    f->row = r;
    f->id = sa_table_int(&b->party->units, r, 0);
    f->level = sa_table_int(&b->party->units, r, 17);
    f->gridRow = row;
    f->gridCell = col;
    fighter_stats(b, f);
    f->hp = f->maxHp;
    f->mp = f->maxMp;
    f->sprite = sprite_for(vfs, sa_table_int(&b->party->units, r, 11), true);
    f->frames = f->sprite ? sa_sprite_split(f->sprite, f->frame, SPRITE_FRAMES) : 0;
}

static void pick_from_list(Battle *b, const char *list, char *out, size_t size) {
    int n = 1;
    for (const char *p = list; *p; p++) n += *p == '|';
    int k = (int)(rnd(b) % (u32)n);
    const char *p = list;
    while (k-- > 0) p = strchr(p, '|') + 1;
    const char *e = strchr(p, '|');
    size_t len = e ? (size_t)(e - p) : strlen(p);
    if (len >= size) len = size - 1;
    memcpy(out, p, len);
    out[len] = 0;
}

static void place_group(Battle *b, Vfs *vfs, int g) {
    for (int i = 0; i < GRID_ROWS; i++) {
        int base = 18 + i * 8, row = GRID_ROWS - 1 - i;
        bool used[GRID_COLS] = {false};
        for (int c = 0; c < GRID_COLS; c++) {
            const char *k = text_of(&b->groups, g, base + c);
            if (!*k || !strcmp(k, "*")) continue;
            add_enemy(b, vfs, k, row, c);
            used[c] = true;
        }
        const char *list = text_of(&b->groups, g, base + 5);
        if (!*list || !strcmp(list, "*")) continue;
        int n = range(b, sa_table_int(&b->groups, g, base + 6), sa_table_int(&b->groups, g, base + 7));
        for (int j = 0; j < n; j++) {
            int avail[GRID_COLS], count = 0;
            for (int c = 0; c < GRID_COLS; c++)
                if (!used[c] && board_has(b, c)) avail[count++] = c;
            if (!count) break;
            int c = avail[rnd(b) % (u32)count];
            used[c] = true;
            char key[64];
            pick_from_list(b, list, key, sizeof key);
            add_enemy(b, vfs, key, row, c);
        }
    }
}

bool sa_battle_start(Battle *b, Vfs *vfs, const char *group, const char *backdrop, u32 seed) {
    return sa_battle_begin(b, vfs, group, backdrop ? sa_load_image(vfs, backdrop) : NULL, 0, seed);
}

void sa_battle_set_names(Battle *b, const char *dungeon, const char *floor) {
    snprintf(b->dungeonName, sizeof b->dungeonName, "%s", dungeon);
    snprintf(b->floorName, sizeof b->floorName, "%s", floor);
}

bool sa_battle_begin(Battle *b, Vfs *vfs, const char *group, Image *backdrop, int boardSet, u32 seed) {
    clear_fighters(b);
    b->dungeonName[0] = b->floorName[0] = 0;
    b->rng = seed ? seed : 0x9e3779b9u;
    int g = row_by_text(&b->groups, 2, group);
    if (g < 0) return false;
    for (int s = 0; s < PARTY_SLOTS; s++) {
        if (!b->party->unit[s]) continue;
        Fighter *f = &b->f[b->count++];
        f->row = sa_party_unit_row(b->party, b->party->unit[s]);
        f->id = b->party->unit[s];
        f->slot = s;
        f->level = b->party->level[s] > 0 ? b->party->level[s] : 1;
        fighter_stats(b, f);
        f->hp = b->party->hp[s];
        f->mp = b->party->mp[s];
    }
    b->group = g;
    int size = sa_table_int(&b->groups, g, 17);
    b->boardSize = size == 1 ? 1 : size == 2 ? 2 : 3;
    b->gainResources = b->gainExp = 0;
    b->gainItem[0] = 0;
    place_group(b, vfs, g);
    if (b->boardSet != boardSet)
        for (int i = 0; i < 4; i++) {
            sa_image_free(b->board[i]);
            b->board[i] = NULL;
        }
    b->boardSet = boardSet;
    char name[24];
    if (!b->board[0]) {
        snprintf(name, sizeof name, "m%03dbt1a.agf", boardSet);
        b->board[0] = sa_load_image(vfs, name);
    }
    if (!b->board[b->boardSize]) {
        snprintf(name, sizeof name, "m%03dbt2%c.agf", boardSet, "?abc"[b->boardSize]);
        b->board[b->boardSize] = sa_load_image(vfs, name);
    }
    sa_image_free(b->back);
    b->back = backdrop;
    b->open = true;
    b->result = BATTLE_RUNNING;
    b->acting = false;
    b->autoBattle = false;
    b->picker = PICK_NONE;
    b->hover = 0;
    b->round = 0;
    return true;
}

static void log_line(Battle *b, const char *who, bool enemy, const char *rest, bool indent) {
    if (b->logCount == BATTLE_LOG) {
        free(b->log[0].text);
        memmove(b->log, b->log + 1, sizeof b->log[0] * (BATTLE_LOG - 1));
        b->logCount--;
    }
    BattleLine *l = &b->log[b->logCount++];
    l->text = sa_fmt("%s\t%s", who, rest);
    l->enemy = enemy;
    l->indent = indent;
}

static const char *name_of(Battle *b, const Fighter *f) { return text_of(&b->party->units, f->row, 2); }

static bool alive(const Fighter *f) { return f->hp > 0; }

static int count_side(Battle *b, bool enemy) {
    int n = 0;
    for (int i = 0; i < b->count; i++)
        if (b->f[i].enemy == enemy && alive(&b->f[i])) n++;
    return n;
}

static bool in_area(Battle *b, const Fighter *f, int aim) {
    if (f->enemy) {
        int edge = -1;
        for (int i = 0; i < b->count; i++) {
            const Fighter *o = &b->f[i];
            if (!o->enemy || !alive(o)) continue;
            if (aim == AIM_RIGHTMOST && o->gridCell > edge) edge = o->gridCell;
            if (aim == AIM_LEFTMOST && (edge < 0 || o->gridCell < edge)) edge = o->gridCell;
        }
        switch (aim) {
        case AIM_FRONT: return f->gridRow == 2;
        case AIM_MIDDLE: return f->gridRow == 1;
        case AIM_BACK: return f->gridRow == 0;
        case AIM_RIGHT: return f->gridCell >= 3;
        case AIM_CENTRE: return f->gridCell == 2;
        case AIM_LEFT: return f->gridCell <= 1;
        case AIM_RIGHTMOST:
        case AIM_LEFTMOST: return f->gridCell == edge;
        default: return true;
        }
    }
    int k = f->slot % 3;
    switch (aim) {
    case AIM_FRONT: return f->slot >= PARTY_FRONT;
    case AIM_MIDDLE: return false;
    case AIM_BACK: return f->slot < PARTY_FRONT;
    case AIM_RIGHT:
    case AIM_RIGHTMOST: return k == 2;
    case AIM_CENTRE: return k == 1;
    case AIM_LEFT:
    case AIM_LEFTMOST: return k == 0;
    default: return true;
    }
}

static int candidates(Battle *b, bool enemySide, int aim, const bool *taken, int *list) {
    int n = 0;
    for (int pass = 0; pass < 2 && !n; pass++)
        for (int i = 0; i < b->count; i++) {
            Fighter *f = &b->f[i];
            if (f->enemy != enemySide || !alive(f) || (taken && taken[i])) continue;
            if (!pass && !in_area(b, f, aim)) continue;
            list[n++] = i;
        }
    return n;
}

static int pick_target(Battle *b, bool enemySide, int aim, const bool *taken) {
    int list[BATTLE_FIGHTERS];
    int n = candidates(b, enemySide, aim, taken, list);
    return n ? list[rnd(b) % (u32)n] : -1;
}

static int most_hurt(Battle *b, bool enemySide, int aim) {
    int list[BATTLE_FIGHTERS], best = -1;
    int n = candidates(b, enemySide, aim, NULL, list);
    for (int j = 0; j < n; j++) {
        Fighter *f = &b->f[list[j]];
        if (f->maxHp <= 0) continue;
        if (best < 0 || f->hp * b->f[best].maxHp < b->f[best].hp * f->maxHp) best = list[j];
    }
    return best;
}

static bool wounded(Battle *b, bool enemySide) {
    int i = most_hurt(b, enemySide, AIM_FREE);
    return i >= 0 && b->f[i].hp * 2 <= b->f[i].maxHp;
}

static int skill_row(Battle *b, const char *name) {
    int r = row_by_text(&b->skills, 1, name);
    return r >= 0 ? r : row_by_text(&b->skills, 2, name);
}

static bool for_allies(Battle *b, int s) { return !strncmp(text_of(&b->skills, s, 15), "味", strlen("味")); }

static bool supportive(Battle *b, int s) { return !strcmp(text_of(&b->skills, s, 14), "回復") || for_allies(b, s); }

static int unit_skills(Battle *b, int unitRow, int *rows, int *weights) {
    const Table *u = &b->party->units;
    int n = 0;
    for (int k = 0; k < 8; k++) {
        const char *name = text_of(u, unitRow, 41 + k * 3);
        if (!*name || !strcmp(name, "*")) continue;
        int r = skill_row(b, name);
        if (r < 0) continue;
        rows[n] = r;
        weights[n++] = sa_table_int(u, unitRow, 42 + k * 3);
    }
    return n;
}

static int choose_skill(Battle *b, Fighter *f, int mode) {
    int all[8], w[8], rows[8], weights[8], n = 0, total = 0;
    int count = unit_skills(b, f->row, all, w);
    for (int k = 0; k < count; k++) {
        int r = all[k];
        if (sa_table_int(&b->skills, r, 13) > f->mp) continue;
        bool support = supportive(b, r);
        if (mode == ORDER_ATTACK && support) continue;
        if (mode == ORDER_SUPPORT && !support) continue;
        if (!strcmp(text_of(&b->skills, r, 14), "回復") && !wounded(b, f->enemy)) continue;
        rows[n] = r;
        weights[n] = w[k] > 0 ? w[k] : 1;
        total += weights[n++];
    }
    if (!n) return mode == ORDER_SUPPORT ? choose_skill(b, f, ORDER_FREE) : -1;
    int pick = (int)(rnd(b) % (u32)total);
    for (int i = 0; i < n; i++) {
        if (pick < weights[i]) return rows[i];
        pick -= weights[i];
    }
    return rows[0];
}

static bool physical(const char *attribute) {
    return !strcmp(attribute, "斬撃") || !strcmp(attribute, "打撃") || !strcmp(attribute, "射撃");
}

static int hits_of(const char *target) {
    if (!strcmp(target, "敵３")) return 3;
    if (!strcmp(target, "敵５")) return 5;
    if (!strcmp(target, "敵７")) return 7;
    if (!strcmp(target, "敵９")) return 9;
    if (!strcmp(target, "敵４")) return 4;
    return 1;
}

static void use_skill(Battle *b, int who, int s, int aim) {
    Fighter *f = &b->f[who];
    char line[128];
    snprintf(line, sizeof line, "の%s", text_of(&b->skills, s, 2));
    log_line(b, name_of(b, f), f->enemy, line, false);
    f->mp -= sa_table_int(&b->skills, s, 13);
    const char *attribute = text_of(&b->skills, s, 14);
    const char *target = text_of(&b->skills, s, 15);
    int lo = sa_table_int(&b->skills, s, 17), hi = sa_table_int(&b->skills, s, 18);
    if (!strcmp(attribute, "回復")) {
        int t = most_hurt(b, f->enemy, aim);
        if (t < 0) t = who;
        int heal = range(b, lo, hi);
        Fighter *tf = &b->f[t];
        tf->hp = tf->hp + heal > tf->maxHp ? tf->maxHp : tf->hp + heal;
        snprintf(line, sizeof line, "の体力が%d回復", heal);
        log_line(b, name_of(b, tf), tf->enemy, line, true);
        return;
    }
    int hits = hits_of(target), landed = 0;
    bool taken[BATTLE_FIGHTERS] = {false};
    for (int h = 0; h < hits; h++) {
        int t = pick_target(b, !f->enemy, aim, taken);
        if (t < 0) break;
        taken[t] = true;
        Fighter *tf = &b->f[t];
        int accuracy = sa_table_int(&b->skills, s, 16) - tf->stat[STAT_EVASION];
        if ((int)(rnd(b) % 100) >= accuracy) {
            log_line(b, name_of(b, tf), tf->enemy, "は回避した", true);
            continue;
        }
        bool phys = physical(attribute);
        int damage = range(b, lo, hi) + f->stat[phys ? STAT_ATTACK : STAT_MAGIC] - tf->stat[phys ? STAT_DEFENCE : STAT_RESIST];
        damage >>= landed++;
        if (tf->guard) damage /= 2;
        if (damage < 1) damage = 1;
        tf->hp = tf->hp > damage ? tf->hp - damage : 0;
        snprintf(line, sizeof line, "に%dダメージ", damage);
        log_line(b, name_of(b, tf), tf->enemy, line, true);
        if (!alive(tf)) log_line(b, name_of(b, tf), tf->enemy, tf->enemy ? "を倒した" : "が倒された", false);
    }
}

static void act(Battle *b, int who) {
    Fighter *f = &b->f[who];
    if (!alive(f)) return;
    if (f->enemy) {
        int s = choose_skill(b, f, ORDER_FREE);
        if (s >= 0) use_skill(b, who, s, AIM_FREE);
        return;
    }
    const Order *o = &b->party->order[f->slot];
    int s = -1;
    switch (o->mode) {
    case ORDER_FLEE:
        log_line(b, name_of(b, f), false, "は逃走した", false);
        b->result = BATTLE_FLED;
        return;
    case ORDER_DEFEND:
        f->guard = true;
        log_line(b, name_of(b, f), false, "は防御した", false);
        return;
    case ORDER_SWAP:
    case ORDER_INTRUDE: return;
    case ORDER_SKILL:
        s = o->skill >= 0 && sa_table_int(&b->skills, o->skill, 13) <= f->mp ? o->skill : choose_skill(b, f, ORDER_FREE);
        break;
    default: s = choose_skill(b, f, o->mode); break;
    }
    if (s >= 0) use_skill(b, who, s, o->aim);
}

static void plan_round(Battle *b) {
    b->orderCount = 0;
    for (int i = 0; i < b->count; i++) {
        b->f[i].guard = false;
        if (alive(&b->f[i])) b->order[b->orderCount++] = i;
    }
    for (int i = 0; i < b->orderCount; i++)
        for (int j = i + 1; j < b->orderCount; j++) {
            int a = b->order[i], c = b->order[j];
            int sa = b->f[a].stat[STAT_SPEED] * 4 + (int)(rnd(b) % 4), sc = b->f[c].stat[STAT_SPEED] * 4 + (int)(rnd(b) % 4);
            if (sc > sa) {
                b->order[i] = c;
                b->order[j] = a;
            }
        }
    b->next = 0;
    b->ms = ACTION_MS;
    b->acting = true;
    b->picker = PICK_NONE;
    b->round++;
}

static bool has_tag(const char *list, const char *tag) {
    size_t n = strlen(tag);
    for (const char *p = list; p; p = strchr(p, '|') ? strchr(p, '|') + 1 : NULL)
        if (!strncmp(p, tag, n) && (p[n] == '|' || !p[n])) return true;
    return false;
}

int sa_item_pick(const Table *items, const char *table, u32 roll) {
    int total = 0;
    for (int r = 0; r < items->rows; r++)
        if (has_tag(text_of(items, r, 30), table)) total += sa_table_int(items, r, 31);
    if (total <= 0) return -1;
    int pick = (int)(roll % (u32)total);
    for (int r = 0; r < items->rows; r++) {
        if (!has_tag(text_of(items, r, 30), table)) continue;
        pick -= sa_table_int(items, r, 31);
        if (pick < 0) return r;
    }
    return -1;
}

static void drop_item(Battle *b, const char *table) {
    int r = sa_item_pick(&b->items, table, rnd(b));
    if (r < 0) return;
    snprintf(b->gainItem, sizeof b->gainItem, "%s", text_of(&b->items, r, 1));
    sa_party_add_item(b->party, sa_table_int(&b->items, r, 0), 1);
}

int sa_battle_exp_needed(const Battle *b, int unitRow, int level) {
    int r = row_by_text(&b->lvup, 1, text_of(&b->party->units, unitRow, 19));
    int base = r >= 0 ? sa_table_int(&b->lvup, r, 2) : 0;
    return base > 0 ? base * level : 0;
}

static void reward(Battle *b) {
    const Table *g = &b->groups;
    b->gainResources = range(b, sa_table_int(g, b->group, 63), sa_table_int(g, b->group, 64));
    int sum = 0;
    for (int i = 0; i < b->count; i++)
        if (b->f[i].enemy) sum += sa_table_int(&b->party->units, b->f[i].row, 72);
    b->gainExp = (sum * range(b, 110, 125) + 50) / 100;
    b->gainItem[0] = 0;
    if ((int)(rnd(b) % 100) < sa_table_int(g, b->group, 65)) drop_item(b, text_of(g, b->group, 67));
    for (int i = 0; i < b->count; i++) {
        Fighter *f = &b->f[i];
        if (f->enemy || !alive(f)) continue;
        int s = f->slot;
        b->party->exp[s] += b->gainExp;
        int need;
        while ((need = sa_battle_exp_needed(b, f->row, b->party->level[s])) > 0 && b->party->exp[s] >= need) {
            b->party->exp[s] -= need;
            b->party->level[s]++;
        }
    }
}

static void finish_check(Battle *b) {
    if (b->result == BATTLE_RUNNING) {
        if (!count_side(b, true)) {
            b->result = BATTLE_WON;
            reward(b);
        } else if (!count_side(b, false))
            b->result = BATTLE_LOST;
    }
    if (b->result != BATTLE_RUNNING) b->acting = false;
    for (int i = 0; i < b->count; i++)
        if (!b->f[i].enemy) {
            b->party->hp[b->f[i].slot] = b->f[i].hp;
            b->party->mp[b->f[i].slot] = b->f[i].mp;
        }
}

bool sa_battle_tick(Battle *b, u32 ms) {
    b->animMs += ms;
    bool changed = b->animMs / PARTY_CELL_MS != (b->animMs - ms) / PARTY_CELL_MS;
    if (!b->acting) {
        if (b->autoBattle && b->result == BATTLE_RUNNING) {
            plan_round(b);
            return true;
        }
        return changed;
    }
    b->ms += ms;
    while (b->acting && b->ms >= ACTION_MS) {
        b->ms -= ACTION_MS;
        if (b->next >= b->orderCount) {
            b->acting = false;
            break;
        }
        act(b, b->order[b->next++]);
        finish_check(b);
        changed = true;
    }
    return changed;
}

static bool inside(int x, int y, int rx, int ry, int w, int h) { return x >= rx && x < rx + w && y >= ry && y < ry + h; }

static int label_x(int slot) { return (slot >= PARTY_FRONT ? FORWARD_X : BACKS_X) + SLOT_X + slot % 3 * SLOT_PITCH; }

static int box_x(int slot) { return (slot >= PARTY_FRONT ? FORWARD_X : BACKS_X) + BOX_X + slot % 3 * SLOT_PITCH; }

static int pick_x(const Battle *b) {
    int x = box_x(b->pickSlot) + PICK_OFFSET;
    return x + PICK_W > SA_W ? SA_W - PICK_W : x;
}

static int pick_top(const Battle *b) { return PICK_BOTTOM - b->pickCount * PICK_ROW; }

static int first_aim(Battle *b, const Order *o) {
    if (o->mode != ORDER_SKILL || o->skill < 0) return AIM_FREE;
    return for_allies(b, o->skill) ? AIM_ALLY : AIM_ENEMY;
}

static void open_picker(Battle *b, int slot, int kind) {
    b->picker = kind;
    b->pickSlot = slot;
    b->pickCount = 0;
    if (kind == PICK_ACTION) {
        int rows[8], weights[8];
        int n = unit_skills(b, sa_party_unit_row(b->party, b->party->unit[slot]), rows, weights);
        for (int i = 0; i < n; i++) b->pick[b->pickCount++] = (Order){ORDER_SKILL, rows[i], 0};
        for (size_t i = 0; i < sizeof pickModes / sizeof pickModes[0]; i++) b->pick[b->pickCount++] = (Order){pickModes[i], -1, 0};
        return;
    }
    b->pick[b->pickCount++] = (Order){0, -1, first_aim(b, &b->party->order[slot])};
    for (size_t i = 0; i < sizeof pickAims / sizeof pickAims[0]; i++) b->pick[b->pickCount++] = (Order){0, -1, pickAims[i]};
}

static void choose(Battle *b, int i) {
    Order *o = &b->party->order[b->pickSlot];
    if (b->picker == PICK_ACTION) {
        o->mode = b->pick[i].mode;
        o->skill = b->pick[i].skill;
        if (o->aim == AIM_FREE || o->aim == AIM_ENEMY || o->aim == AIM_ALLY) o->aim = first_aim(b, o);
    } else
        o->aim = b->pick[i].aim;
    b->picker = PICK_NONE;
}

static void side_button(Battle *b, int i) {
    Party *p = b->party;
    for (int s = 0; s < PARTY_SLOTS; s++) {
        if (!p->unit[s]) continue;
        Order *o = &p->order[s];
        if (i == 0 || i == 1) {
            o->mode = i ? ORDER_FLEE : ORDER_FREE;
            o->skill = -1;
            if (o->aim == AIM_ENEMY || o->aim == AIM_ALLY) o->aim = AIM_FREE;
        } else if (i >= 8)
            o->aim = i == 8 ? first_aim(b, o) : i == 9 ? AIM_FRONT : i == 10 ? AIM_MIDDLE : AIM_BACK;
    }
    if (i >= 2 && i <= 4) {
        memcpy(p->preset[i - 2], p->order, sizeof p->order);
        p->presetSaved[i - 2] = true;
    }
    if (i >= 5 && i <= 7 && p->presetSaved[i - 5]) memcpy(p->order, p->preset[i - 5], sizeof p->order);
}

static int hit(Battle *b, int x, int y) {
    if (b->result != BATTLE_RUNNING) return 0;
    if (b->picker) {
        int px = pick_x(b), top = pick_top(b);
        for (int i = 0; i < b->pickCount; i++)
            if (inside(x, y, px, top + i * PICK_ROW, PICK_W, PICK_ROW)) return HOVER_PICK + i;
        return 0;
    }
    if (b->acting) return 0;
    if (inside(x, y, START_X, START_Y, START_W, START_H)) return HOVER_START;
    if (inside(x, y, AUTO_X, TOGGLE_Y, TOGGLE_W, TOGGLE_H)) return HOVER_AUTO;
    if (inside(x, y, GAUGE_X, TOGGLE_Y, TOGGLE_W, TOGGLE_H)) return HOVER_GAUGE;
    for (int i = 0; i < 12; i++)
        if (inside(x, y, SIDE_X, sideY[i], SIDE_W, SIDE_H)) return HOVER_SIDE + i;
    for (int s = 0; s < PARTY_SLOTS; s++) {
        if (!b->party->unit[s]) continue;
        if (inside(x, y, label_x(s), LABEL_Y, LABEL_W, LABEL_H)) return HOVER_LABEL + s * 2;
        if (inside(x, y, label_x(s), TARGET_Y, LABEL_W, LABEL_H)) return HOVER_LABEL + s * 2 + 1;
    }
    return 0;
}

int sa_battle_click(Battle *b, int x, int y) {
    if (b->result != BATTLE_RUNNING) {
        b->open = false;
        return b->result;
    }
    int h = hit(b, x, y);
    if (b->picker) {
        if (h >= HOVER_PICK) choose(b, h - HOVER_PICK);
        else b->picker = PICK_NONE;
        return BATTLE_RUNNING;
    }
    if (h == HOVER_AUTO) b->autoBattle = !b->autoBattle;
    else if (h == HOVER_GAUGE) b->gaugeMode = (b->gaugeMode + 1) % 3;
    else if (h == HOVER_START) plan_round(b);
    else if (h >= HOVER_LABEL) open_picker(b, (h - HOVER_LABEL) / 2, (h - HOVER_LABEL) % 2 ? PICK_TARGET : PICK_ACTION);
    else if (h >= HOVER_SIDE) side_button(b, h - HOVER_SIDE);
    return BATTLE_RUNNING;
}

void sa_battle_cancel(Battle *b) { b->picker = PICK_NONE; }

bool sa_battle_hover(Battle *b, int x, int y) {
    int h = hit(b, x, y);
    if (h == b->hover) return false;
    b->hover = h;
    return true;
}

static void tile_center(const Fighter *f, int *x, int *y) {
    if (f->enemy) {
        *x = GRID_X + f->gridCell * STEP_X + f->gridRow * ROW_X;
        *y = GRID_Y + f->gridCell * STEP_Y + f->gridRow * ROW_Y;
        return;
    }
    sa_party_tile_center(f->slot, PARTY_BOARD_X, PARTY_BOARD_Y, x, y);
}

static void draw_fighter(Battle *b, u32 *fb, Fighter *f) {
    if (!alive(f)) return;
    const Image *im = f->enemy ? f->sprite : b->party->sprite[f->slot];
    if (!im) return;
    SpriteFrame fr;
    if (f->enemy) {
        if (!f->frames) return;
        fr = f->frame[b->animMs / PARTY_CELL_MS % (u32)f->frames];
    } else {
        int cols, rows, cells = b->party->cells[f->slot];
        sa_sprite_layout(im, &cols, &rows);
        int cw = im->w / cols, ch = im->h / rows;
        int cell = cells > 0 ? (int)(b->animMs / PARTY_CELL_MS % (u32)cells) : 0;
        fr = (SpriteFrame){cell % cols * cw, cell / cols * ch, cw, ch};
    }
    int cx, cy;
    tile_center(f, &cx, &cy);
    int x = cx - fr.w / 2, y = cy + 17 - fr.h;
    sa_blit_rect(fb, im, fr.x, fr.y, fr.w, fr.h, x, y);
    if (b->gaugeMode == 0 || (b->gaugeMode == 2 && b->acting)) sa_party_draw_bars(b->party, fb, cx - 22, y, f->hp, f->maxHp, f->mp, f->maxMp);
}

static void draw_log(Battle *b, u32 *fb) {
    sa_blit_rect(fb, b->art[ART_LOG], 0, 0, b->art[ART_LOG] ? b->art[ART_LOG]->w : 0, 450, LOG_X, LOG_Y);
    int first = b->logCount > LOG_LINES ? b->logCount - LOG_LINES : 0;
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    for (int i = first; i < b->logCount; i++) {
        BattleLine *l = &b->log[i];
        int y = LOG_TEXT_Y + (i - first) * LOG_PITCH;
        char *tab = strchr(l->text, '\t');
        int x = LOG_TEXT_X + (l->indent ? LOG_INDENT : 0);
        int nameLen = 0;
        for (const char *q = l->text; q < tab; nameLen++) {
            u32 cp;
            q += sa_utf8_decode(q, &cp);
        }
        u32 nameColor = l->enemy ? 0xff0000u : 0x0000ffu;
        x += sa_text_draw(fb, SA_W, SA_H, l->text, nameLen, x, y, LOG_SIZE, nameColor, NO_EDGE);
        sa_text_draw(fb, SA_W, SA_H, tab + 1, -1, x, y, LOG_SIZE, 0x000000, NO_EDGE);
    }
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
}

static int icon_of(Battle *b, int skill) {
    const char *a = text_of(&b->skills, skill, 14);
    for (size_t i = 0; i < sizeof iconOrder / sizeof iconOrder[0]; i++)
        if (!strncmp(a, iconOrder[i], strlen(iconOrder[i]))) return (int)i;
    return 3;
}

static void draw_skill_button(Battle *b, u32 *fb, int skill, int x, int y, bool lit) {
    sa_blit_rect(fb, b->art[ART_NONE_ACTION], 0, lit ? LABEL_H : 0, LABEL_W, LABEL_H, x, y);
    sa_blit_rect(fb, b->art[ART_ICONS], 0, icon_of(b, skill) * ICON, ICON, ICON, x - 1, y - 1);
    const char *name = text_of(&b->skills, skill, 2);
    sa_font_set_spacing(4);
    int w = sa_text_width(name, -1, SKILL_SIZE);
    sa_text_draw(fb, SA_W, SA_H, name, -1, x + (LABEL_W - w) / 2, y + 4, SKILL_SIZE, 0xffffff, 0x000000);
    sa_font_set_spacing(1);
    char n[8];
    snprintf(n, sizeof n, "%d", sa_table_int(&b->skills, skill, 13));
    sa_text_draw(fb, SA_W, SA_H, n, -1, x + LABEL_W - 4 - sa_text_width(n, -1, 14), y + 3, 14, 0xc0c0c0, NO_EDGE);
}

static void draw_order(Battle *b, u32 *fb, const Order *o, int x, int y, bool lit) {
    if (o->mode == ORDER_SKILL && o->skill >= 0) draw_skill_button(b, fb, o->skill, x, y, lit);
    else sa_blit_rect(fb, b->art[ART_MODE0 + o->mode], 0, lit ? LABEL_H : 0, LABEL_W, LABEL_H, x, y);
}

static void draw_slot(Battle *b, u32 *fb, int slot) {
    int x = label_x(slot);
    const Fighter *f = NULL;
    for (int i = 0; i < b->count; i++)
        if (!b->f[i].enemy && b->f[i].slot == slot) f = &b->f[i];
    sa_blit(fb, b->art[ART_SLOT], box_x(slot), BOX_Y);
    if (!f) {
        sa_blit_rect(fb, b->art[ART_NONE_ACTION], 0, 0, LABEL_W, LABEL_H, x, LABEL_Y);
        return;
    }
    const Order *o = &b->party->order[slot];
    bool running = b->result == BATTLE_RUNNING && !b->acting;
    bool litAction = running && (b->hover == HOVER_LABEL + slot * 2 || (b->picker == PICK_ACTION && b->pickSlot == slot));
    bool litTarget = running && (b->hover == HOVER_LABEL + slot * 2 + 1 || (b->picker == PICK_TARGET && b->pickSlot == slot));
    draw_order(b, fb, o, x, LABEL_Y, litAction);
    sa_blit_rect(fb, b->art[ART_AIM0 + o->aim], 0, litTarget ? LABEL_H : 0, LABEL_W, LABEL_H, x, TARGET_Y);
    int hpw = f->maxHp > 0 ? f->hp * 100 / f->maxHp : 0, mpw = f->maxMp > 0 ? f->mp * 100 / f->maxMp : 0;
    int band = f->hp * 2 > f->maxHp ? (f->hp == f->maxHp ? 0 : 1) : 2;
    sa_blit_rect(fb, b->art[ART_HP], 0, band * 7, hpw, 7, x + HP_X, HP_Y);
    sa_blit_rect(fb, b->art[ART_MP], 0, 0, mpw, 7, x + HP_X, MP_Y);
    char n[32];
    snprintf(n, sizeof n, "%d/%d", f->hp, f->maxHp);
    sa_text_draw(fb, SA_W, SA_H, n, -1, x + 148 - sa_text_width(n, -1, 15), HP_Y - 17, 15, 0x503720, NO_EDGE);
    snprintf(n, sizeof n, "%d/%d", f->mp, f->maxMp);
    sa_text_draw(fb, SA_W, SA_H, n, -1, x + 148 - sa_text_width(n, -1, 15), MP_Y - 17, 15, 0x503720, NO_EDGE);
    int need = sa_battle_exp_needed(b, f->row, b->party->level[slot]);
    int expw = need > 0 ? b->party->exp[slot] * EXP_W / need : 0;
    sa_blit_rect(fb, b->art[ART_EXP], 0, 0, expw > EXP_W ? EXP_W : expw, 7, x + HP_X, EXP_Y);
    snprintf(n, sizeof n, "%d", f->level);
    sa_text_draw(fb, SA_W, SA_H, n, -1, x + LV_RIGHT - sa_text_width(n, -1, 15), LV_Y, 15, 0x503720, NO_EDGE);
    sa_text_draw(fb, SA_W, SA_H, name_of(b, f), -1, x + 28, 690, 16, 0xffffff, 0x000000);
}

static void draw_picker(Battle *b, u32 *fb) {
    int x = pick_x(b), top = pick_top(b);
    bool action = b->picker == PICK_ACTION;
    sa_blit(fb, b->art[action ? ART_ACT_HEAD : ART_AIM_HEAD], x, top - PICK_HEAD);
    for (int i = 0; i < b->pickCount; i++) {
        int y = top + i * PICK_ROW;
        bool lit = b->hover == HOVER_PICK + i;
        sa_blit(fb, b->art[action ? ART_ACT_ROW : ART_AIM_ROW], x, y);
        if (action) draw_order(b, fb, &b->pick[i], x + PICK_INSET_X, y + PICK_INSET_Y, lit);
        else
            sa_blit_rect(fb, b->art[ART_AIM0 + b->pick[i].aim], 0, lit ? LABEL_H : 0, LABEL_W, LABEL_H, x + PICK_INSET_X,
                         y + PICK_INSET_Y);
    }
    sa_blit(fb, b->art[action ? ART_ACT_BOTTOM : ART_AIM_BOTTOM], x, PICK_BOTTOM);
}

static int aim_shown(const Battle *b) {
    if (b->picker != PICK_TARGET) return -1;
    if (b->hover >= HOVER_PICK && b->hover < HOVER_PICK + b->pickCount) return b->pick[b->hover - HOVER_PICK].aim;
    return b->party->order[b->pickSlot].aim;
}

static void draw_board(Battle *b, u32 *fb) {
    int aim = aim_shown(b);
    bool allySide = b->picker == PICK_TARGET && first_aim(b, &b->party->order[b->pickSlot]) == AIM_ALLY;
    bool ordering = b->result == BATTLE_RUNNING && !b->acting;
    int frames[PARTY_SLOTS];
    for (int s = 0; s < PARTY_SLOTS; s++) {
        Fighter probe = {.slot = s};
        if (!ordering) frames[s] = TILE_PLAIN;
        else if (b->picker == PICK_TARGET) frames[s] = allySide && in_area(b, &probe, aim) ? TILE_YELLOW : TILE_PLAIN;
        else if (b->picker == PICK_ACTION && b->pickSlot == s) frames[s] = TILE_YELLOW;
        else frames[s] = TILE_BLUE;
    }
    sa_blit(fb, b->board[b->boardSize], boards[b->boardSize].x, boards[b->boardSize].y);
    sa_blit(fb, b->board[0], PARTY_BOARD_X, PARTY_BOARD_Y);
    sa_party_draw_tiles(b->party, fb, PARTY_BOARD_X, PARTY_BOARD_Y, frames);
    for (int r = 0; r < GRID_ROWS; r++)
        for (int c = 0; c < GRID_COLS; c++) {
            if (!board_has(b, c)) continue;
            Fighter probe = {.enemy = true, .gridRow = r, .gridCell = c};
            int cx, cy;
            tile_center(&probe, &cx, &cy);
            bool lit = ordering && aim >= 0 && !allySide && in_area(b, &probe, aim);
            sa_party_draw_tile(b->party, fb, cx, cy, lit ? TILE_YELLOW : TILE_PLAIN);
        }
    int order[BATTLE_FIGHTERS], ys[BATTLE_FIGHTERS];
    for (int i = 0; i < b->count; i++) {
        int cx;
        tile_center(&b->f[i], &cx, &ys[i]);
        order[i] = i;
    }
    for (int i = 1; i < b->count; i++)
        for (int j = i; j > 0 && ys[order[j]] < ys[order[j - 1]]; j--) {
            int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    for (int i = 0; i < b->count; i++) draw_fighter(b, fb, &b->f[order[i]]);
}

static bool side_lit(const Battle *b, int i) {
    if (i > 1) return false;
    int mode = i ? ORDER_FLEE : ORDER_FREE;
    bool any = false;
    for (int s = 0; s < PARTY_SLOTS; s++) {
        if (!b->party->unit[s]) continue;
        if (b->party->order[s].mode != mode) return false;
        any = true;
    }
    return any;
}

void sa_battle_draw(Battle *b, u32 *fb) {
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u;
    if (b->back) sa_blit(fb, b->back, 0, 0);
    draw_board(b, fb);
    sa_blit_rect(fb, b->art[ART_BAR], 0, 0, 448, 28, 10, 10);
    sa_text_draw(fb, SA_W, SA_H, b->dungeonName, -1, NAME_X, NAME_Y, 15, 0x000000, NO_EDGE);
    sa_text_draw(fb, SA_W, SA_H, b->floorName, -1, FLOOR_NAME_X, NAME_Y, 15, 0x000000, NO_EDGE);
    for (int i = 0; i < 12; i++)
        sa_blit_rect(fb, b->art[ART_SIDE0 + i], 0, b->hover == HOVER_SIDE + i || side_lit(b, i) ? SIDE_H : 0, SIDE_W, SIDE_H, SIDE_X,
                     sideY[i]);
    sa_blit(fb, b->art[ART_BACKS], BACKS_X, PANEL_Y);
    sa_blit(fb, b->art[ART_FORWARD], FORWARD_X, PANEL_Y);
    for (int s = 0; s < PARTY_SLOTS; s++) draw_slot(b, fb, s);
    if (b->result == BATTLE_RUNNING && !b->acting)
        sa_blit_rect(fb, b->art[ART_START], 0, b->hover == HOVER_START ? START_H : 0, START_W, START_H, START_X, START_Y);
    if (b->result == BATTLE_RUNNING)
        sa_blit_rect(fb, b->art[ART_AUTO], b->autoBattle ? TOGGLE_W : 0, b->hover == HOVER_AUTO ? TOGGLE_H : 0, TOGGLE_W, TOGGLE_H,
                     AUTO_X, TOGGLE_Y);
    static const int gaugeArt[3] = {ART_GAUGE, ART_GAUGE_OFF, ART_GAUGE_VARY};
    sa_blit_rect(fb, b->art[gaugeArt[b->gaugeMode]], 0, b->hover == HOVER_GAUGE ? TOGGLE_H : 0, TOGGLE_W, TOGGLE_H, GAUGE_X, TOGGLE_Y);
    draw_log(b, fb);
    if (b->picker) draw_picker(b, fb);
    if (b->result == BATTLE_RUNNING) return;
    sa_dim(fb);
    if (b->result != BATTLE_WON) return;
    char *lines[3];
    int n = 0;
    lines[n++] = sa_fmt(POP_GREEN "資源" POP_WHITE "を" POP_GREEN "%d材" POP_WHITE "手に入れた", b->gainResources);
    lines[n++] = sa_fmt(POP_YELLOW "%dP" POP_WHITE "の経験値を獲得した", b->gainExp);
    if (b->gainItem[0]) lines[n++] = sa_fmt(POP_YELLOW "%s" POP_WHITE "を手に入れた", b->gainItem);
    sa_popup_draw(b->pop, fb, (const char *const *)lines, n);
    for (int i = 0; i < n; i++) free(lines[i]);
}

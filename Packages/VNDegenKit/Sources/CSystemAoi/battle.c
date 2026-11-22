#include "sa.h"

enum {
    ART_BAR,
    ART_BACKS,
    ART_FORWARD,
    ART_START,
    ART_AUTO,
    ART_GAUGE,
    ART_LOG,
    ART_ACTION,
    ART_TARGET,
    ART_NONE_ACTION,
    ART_NONE_TARGET,
    ART_HP,
    ART_MP,
    ART_SLOT,
    ART_SIDE0,
    ART_COUNT = ART_SIDE0 + 12,
};

static const char *const artNames[ART_SIDE0] = {
    "bat_a00.agf", "bat_j01.agf", "bat_j00.agf", "bat_l00.agf", "bat_p01.agf", "bat_p00.agf", "bat_m00.agf",
    "bat_f00.agf", "bat_h00.agf", "bat_f99.agf", "bat_h99.agf", "bat_k01.agf", "bat_k02.agf", "bat_j02.agf",
};

enum {
    PARTY_BOARD_X = 193,
    PARTY_BOARD_Y = 299,
    ENEMY_BOARD_X = 406,
    ENEMY_BOARD_Y = 116,
    ENEMY_CX = 684,
    ENEMY_CY = 232,
    HALF_X = 57,
    HALF_Y = 29,
    ROW_X = -81,
    ROW_Y = 41,
    BACKS_X = 103,
    FORWARD_X = 603,
    PANEL_Y = 541,
    SLOT_X = 13,
    BOX_X = 6,
    BOX_Y = 563,
    SLOT_PITCH = 163,
    LABEL_Y = 569,
    TARGET_Y = 595,
    HP_X = 47,
    HP_Y = 634,
    MP_Y = 658,
    START_X = 500,
    START_Y = 497,
    START_W = 204,
    START_H = 44,
    LOG_X = 1105,
    LOG_Y = 270,
    LOG_TEXT_X = 1113,
    LOG_TEXT_Y = 302,
    LOG_PITCH = 18,
    LOG_INDENT = 16,
    LOG_LINES = 23,
    LOG_SIZE = 12,
    SIDE_X = 0,
    ACTION_MS = 1500,
};

#define NO_EDGE 0xffffffffu

static const int sideY[12] = {414, 434, 474, 494, 514, 554, 574, 594, 634, 654, 674, 694};

static const char *text_of(const Table *t, int row, int col) { return sa_table_text(t, row, col); }

static int row_by_text(const Table *t, int col, const char *s) {
    for (int r = 0; r < t->rows; r++)
        if (!strcmp(text_of(t, r, col), s)) return r;
    return -1;
}

static int growth(const Table *lvup, const char *type, const char *stat, int level) {
    int r = row_by_text(lvup, 1, type);
    if (r < 0 || level <= 1) return 0;
    const char *s = text_of(lvup, r, 22);
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
    b->hover = -1;
    if (!sa_table_load(&b->groups, vfs, "party.obj") || !sa_table_load(&b->skills, vfs, "skill.obj") ||
        !sa_table_load(&b->lvup, vfs, "lvup.obj"))
        return false;
    for (int i = 0; i < ART_SIDE0; i++) b->art[i] = sa_load_image(vfs, artNames[i]);
    for (int i = 0; i < 12; i++) {
        char name[16];
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
    for (int i = 0; i < 2; i++) sa_image_free(b->board[i]);
    sa_image_free(b->back);
    sa_table_free(&b->groups);
    sa_table_free(&b->skills);
    sa_table_free(&b->lvup);
    memset(b, 0, sizeof *b);
}

static u32 rnd(Battle *b) {
    b->rng ^= b->rng << 13;
    b->rng ^= b->rng >> 17;
    b->rng ^= b->rng << 5;
    return b->rng;
}

static int range(Battle *b, int lo, int hi) { return hi <= lo ? lo : lo + (int)(rnd(b) % (u32)(hi - lo + 1)); }

static void add_enemy(Battle *b, Vfs *vfs, const char *key, int row, int cell) {
    if (b->count >= BATTLE_FIGHTERS) return;
    int r = row_by_text(&b->party->units, 1, key);
    if (r < 0) return;
    Fighter *f = &b->f[b->count++];
    f->enemy = true;
    f->row = r;
    f->id = sa_table_int(&b->party->units, r, 0);
    f->level = sa_table_int(&b->party->units, r, 17);
    f->gridRow = row;
    f->gridCell = cell;
    fighter_stats(b, f);
    f->hp = f->maxHp;
    f->mp = f->maxMp;
    f->sprite = sprite_for(vfs, sa_table_int(&b->party->units, r, 11), true);
    f->cells = f->sprite ? sa_sprite_filled_cells(f->sprite) : 0;
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
    bool random = !strcmp(text_of(&b->groups, g, 7), "ランダム") && sa_table_int(&b->groups, g, 8) < 0;
    for (int row = 0; row < 3; row++) {
        int base = 18 + row * 8;
        if (!random) {
            for (int c = 0; c < 6; c++) {
                const char *k = text_of(&b->groups, g, base + c);
                if (*k && strcmp(k, "*")) add_enemy(b, vfs, k, row, c);
            }
            continue;
        }
        const char *list = text_of(&b->groups, g, base + 5);
        if (!*list || !strcmp(list, "*")) continue;
        int n = range(b, sa_table_int(&b->groups, g, base + 6), sa_table_int(&b->groups, g, base + 7));
        bool used[3] = {false, false, false};
        for (int i = 0; i < n && i < 3; i++) {
            int c;
            do c = (int)(rnd(b) % 3);
            while (used[c]);
            used[c] = true;
            char key[64];
            pick_from_list(b, list, key, sizeof key);
            add_enemy(b, vfs, key, row, c * 2 + (row & 1));
        }
    }
}

bool sa_battle_start(Battle *b, Vfs *vfs, const char *group, const char *backdrop, u32 seed) {
    clear_fighters(b);
    b->rng = seed ? seed : 0x9e3779b9u;
    int g = row_by_text(&b->groups, 2, group);
    if (g < 0) return false;
    for (int s = 0; s < PARTY_SLOTS; s++) {
        if (!b->party->unit[s]) continue;
        Fighter *f = &b->f[b->count++];
        f->row = sa_party_unit_row(b->party, b->party->unit[s]);
        f->id = b->party->unit[s];
        f->slot = s;
        f->level = 1;
        fighter_stats(b, f);
        f->hp = b->party->hp[s];
        f->mp = b->party->mp[s];
    }
    place_group(b, vfs, g);
    if (!b->board[0]) b->board[0] = sa_load_image(vfs, "m000bt1a.agf");
    if (!b->board[1]) b->board[1] = sa_load_image(vfs, "m000bt2a.agf");
    sa_image_free(b->back);
    b->back = backdrop ? sa_load_image(vfs, backdrop) : NULL;
    b->open = true;
    b->result = BATTLE_RUNNING;
    b->acting = false;
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

static int pick_target(Battle *b, bool enemySide, const bool *taken) {
    int n = 0, list[BATTLE_FIGHTERS];
    for (int i = 0; i < b->count; i++)
        if (b->f[i].enemy == enemySide && alive(&b->f[i]) && !taken[i]) list[n++] = i;
    return n ? list[rnd(b) % (u32)n] : -1;
}

static int most_hurt(Battle *b, bool enemySide) {
    int best = -1;
    for (int i = 0; i < b->count; i++) {
        Fighter *f = &b->f[i];
        if (f->enemy != enemySide || !alive(f) || f->maxHp <= 0) continue;
        if (best < 0 || f->hp * b->f[best].maxHp < b->f[best].hp * f->maxHp) best = i;
    }
    return best;
}

static bool wounded(Battle *b, bool enemySide) {
    int i = most_hurt(b, enemySide);
    return i >= 0 && b->f[i].hp * 2 <= b->f[i].maxHp;
}

static int skill_row(Battle *b, const char *name) {
    int r = row_by_text(&b->skills, 1, name);
    return r >= 0 ? r : row_by_text(&b->skills, 2, name);
}

static int choose_skill(Battle *b, Fighter *f) {
    const Table *u = &b->party->units;
    int rows[8], weights[8], n = 0, total = 0;
    for (int k = 0; k < 8; k++) {
        const char *name = text_of(u, f->row, 41 + k * 3);
        if (!*name || !strcmp(name, "*")) continue;
        int r = skill_row(b, name);
        if (r < 0 || sa_table_int(&b->skills, r, 13) > f->mp) continue;
        int w = sa_table_int(u, f->row, 42 + k * 3);
        if (w <= 0) w = 1;
        if (!strcmp(text_of(&b->skills, r, 14), "回復") && !wounded(b, f->enemy)) continue;
        rows[n] = r;
        weights[n++] = w;
        total += w;
    }
    if (!n) return -1;
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

static void act(Battle *b, int who) {
    Fighter *f = &b->f[who];
    if (!alive(f)) return;
    int s = choose_skill(b, f);
    if (s < 0) return;
    char line[128];
    snprintf(line, sizeof line, "の%s", text_of(&b->skills, s, 2));
    log_line(b, name_of(b, f), f->enemy, line, false);
    f->mp -= sa_table_int(&b->skills, s, 13);
    const char *attribute = text_of(&b->skills, s, 14);
    const char *target = text_of(&b->skills, s, 15);
    int lo = sa_table_int(&b->skills, s, 17), hi = sa_table_int(&b->skills, s, 18);
    if (!strcmp(attribute, "回復")) {
        int t = most_hurt(b, f->enemy);
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
        int t = pick_target(b, !f->enemy, taken);
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
        if (damage < 1) damage = 1;
        tf->hp = tf->hp > damage ? tf->hp - damage : 0;
        snprintf(line, sizeof line, "に%dダメージ", damage);
        log_line(b, name_of(b, tf), tf->enemy, line, true);
        if (!alive(tf)) log_line(b, name_of(b, tf), tf->enemy, "を倒した", false);
    }
}

static void plan_round(Battle *b) {
    b->orderCount = 0;
    for (int i = 0; i < b->count; i++)
        if (alive(&b->f[i])) b->order[b->orderCount++] = i;
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
    b->ms = 0;
    b->acting = true;
    b->round++;
}

static void finish_check(Battle *b) {
    if (!count_side(b, true)) b->result = BATTLE_WON;
    else if (!count_side(b, false)) b->result = BATTLE_LOST;
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
    if (!b->acting) return changed;
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

int sa_battle_click(Battle *b, int x, int y) {
    if (b->result != BATTLE_RUNNING) {
        b->open = false;
        return b->result;
    }
    if (!b->acting && x >= START_X && x < START_X + START_W && y >= START_Y && y < START_Y + START_H) {
        plan_round(b);
        b->ms = ACTION_MS;
    }
    return BATTLE_RUNNING;
}

bool sa_battle_hover(Battle *b, int x, int y) {
    int h = !b->acting && x >= START_X && x < START_X + START_W && y >= START_Y && y < START_Y + START_H ? 0 : -1;
    if (h == b->hover) return false;
    b->hover = h;
    return true;
}

static void tile_center(const Fighter *f, int *x, int *y) {
    if (f->enemy) {
        *x = ENEMY_CX + f->gridCell * HALF_X / 2 + f->gridRow * ROW_X;
        *y = ENEMY_CY + f->gridCell * HALF_Y / 2 + f->gridRow * ROW_Y;
        return;
    }
    sa_party_tile_center(f->slot, PARTY_BOARD_X, PARTY_BOARD_Y, x, y);
}

static void draw_fighter(Battle *b, u32 *fb, Fighter *f) {
    if (!alive(f)) return;
    const Image *im = f->enemy ? f->sprite : b->party->sprite[f->slot];
    int cells = f->enemy ? f->cells : b->party->cells[f->slot];
    if (!im) return;
    int cols, rows;
    sa_sprite_layout(im, &cols, &rows);
    int cw = im->w / cols, ch = im->h / rows;
    int cell = cells > 0 ? (int)(b->animMs / PARTY_CELL_MS % (u32)cells) : 0;
    int cx, cy;
    tile_center(f, &cx, &cy);
    int x = cx - cw / 2, y = cy + 17 - ch;
    sa_blit_rect(fb, im, cell % cols * cw, cell / cols * ch, cw, ch, x, y);
    sa_party_draw_bars(b->party, fb, x + cw / 2 - 22, y, f->hp, f->maxHp, f->mp, f->maxMp);
}

static void draw_log(Battle *b, u32 *fb) {
    sa_blit_rect(fb, b->art[ART_LOG], 0, 0, b->art[ART_LOG] ? b->art[ART_LOG]->w : 0, 450, LOG_X, LOG_Y);
    int first = b->logCount > LOG_LINES ? b->logCount - LOG_LINES : 0;
    sa_font_set_spacing(0);
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
        sa_text_draw(fb, SA_W, SA_H, l->text, nameLen, x, y, LOG_SIZE, nameColor, NO_EDGE);
        x += sa_text_width(l->text, nameLen, LOG_SIZE);
        sa_text_draw(fb, SA_W, SA_H, tab + 1, -1, x, y, LOG_SIZE, 0x000000, NO_EDGE);
    }
    sa_font_set_spacing(1);
}

static void draw_slot(Battle *b, u32 *fb, int slot) {
    int panelX = slot >= PARTY_FRONT ? FORWARD_X : BACKS_X;
    int x = panelX + SLOT_X + slot % 3 * SLOT_PITCH;
    const Fighter *f = NULL;
    for (int i = 0; i < b->count; i++)
        if (!b->f[i].enemy && b->f[i].slot == slot) f = &b->f[i];
    sa_blit(fb, b->art[ART_SLOT], panelX + BOX_X + slot % 3 * SLOT_PITCH, BOX_Y);
    if (!f) {
        sa_blit_rect(fb, b->art[ART_NONE_ACTION], 0, 0, 148, 20, x, LABEL_Y);
        return;
    }
    sa_blit_rect(fb, b->art[ART_ACTION], 0, 0, 148, 20, x, LABEL_Y);
    sa_blit_rect(fb, b->art[ART_TARGET], 0, 0, 148, 20, x, TARGET_Y);
    int hpw = f->maxHp > 0 ? f->hp * 100 / f->maxHp : 0, mpw = f->maxMp > 0 ? f->mp * 100 / f->maxMp : 0;
    int band = f->hp * 2 > f->maxHp ? (f->hp == f->maxHp ? 0 : 1) : 2;
    sa_blit_rect(fb, b->art[ART_HP], 0, band * 7, hpw, 7, x + HP_X, HP_Y);
    sa_blit_rect(fb, b->art[ART_MP], 0, 0, mpw, 7, x + HP_X, MP_Y);
    char n[32];
    snprintf(n, sizeof n, "%d/%d", f->hp, f->maxHp);
    sa_text_draw(fb, SA_W, SA_H, n, -1, x + 148 - sa_text_width(n, -1, 15), HP_Y - 17, 15, 0x503720, NO_EDGE);
    snprintf(n, sizeof n, "%d/%d", f->mp, f->maxMp);
    sa_text_draw(fb, SA_W, SA_H, n, -1, x + 148 - sa_text_width(n, -1, 15), MP_Y - 17, 15, 0x503720, NO_EDGE);
    const char *name = name_of(b, f);
    sa_text_draw(fb, SA_W, SA_H, name, -1, x + 28, 690, 16, 0xffffff, 0x000000);
}

void sa_battle_draw(Battle *b, u32 *fb) {
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u;
    if (b->back) sa_blit(fb, b->back, 0, 0);
    sa_blit(fb, b->board[1], ENEMY_BOARD_X, ENEMY_BOARD_Y);
    sa_blit(fb, b->board[0], PARTY_BOARD_X, PARTY_BOARD_Y);
    sa_party_draw_tiles(b->party, fb, PARTY_BOARD_X, PARTY_BOARD_Y);
    for (int r = 0; r < 3; r++)
        for (int c = r & 1; c < 6; c += 2) {
            Fighter probe = {.enemy = true, .gridRow = r, .gridCell = c};
            int cx, cy;
            tile_center(&probe, &cx, &cy);
            sa_party_draw_tile(b->party, fb, cx, cy);
        }
    for (int depth = 0; depth < 16; depth++)
        for (int i = 0; i < b->count; i++) {
            Fighter *f = &b->f[i];
            int d = f->enemy ? f->gridRow * 2 + f->gridCell / 2 : 8 + f->slot % 3 + (f->slot >= PARTY_FRONT ? 0 : 3);
            if (d == depth) draw_fighter(b, fb, f);
        }
    sa_blit_rect(fb, b->art[ART_BAR], 0, 0, 448, 28, 10, 10);
    for (int i = 0; i < 12; i++) sa_blit_rect(fb, b->art[ART_SIDE0 + i], 0, 0, 100, 19, SIDE_X, sideY[i]);
    sa_blit(fb, b->art[ART_BACKS], BACKS_X, PANEL_Y);
    sa_blit(fb, b->art[ART_FORWARD], FORWARD_X, PANEL_Y);
    for (int s = 0; s < PARTY_SLOTS; s++) draw_slot(b, fb, s);
    if (b->result == BATTLE_RUNNING && !b->acting)
        sa_blit_rect(fb, b->art[ART_START], 0, b->hover == 0 ? START_H : 0, START_W, START_H, START_X, START_Y);
    sa_blit_rect(fb, b->art[ART_AUTO], 0, 0, 104, 29, 706, 510);
    sa_blit_rect(fb, b->art[ART_GAUGE], 0, 0, 104, 29, 998, 510);
    draw_log(b, fb);
}

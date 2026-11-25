#include "sa_private.h"

enum {
    BAR_X = 397,
    BAR_Y = 10,
    PANEL_X = 397,
    PANEL_Y = 45,
    MAP_X = 535,
    MAP_Y = 155,
    MAP_SCALE = 2,
    NAME_X = 462,
    FLOOR_X = 688,
    NAMES_Y = 17,
    STEPS_RIGHT = 986,
    STEPS_Y = 20,
    STOCK_X = 1065,
    STOCK_Y = 92,
    STOCK_RIGHT = 1255,
    STOCK_ROW0 = 148,
    STOCK_PITCH = 20,
    STOCK_BLOCK = 104,
    DIGIT_W = 10,
    DIGIT_H = 13,
    BACK_X = 1120,
    BACK_Y = 670,
    BACK_W = 152,
    BACK_H = 44,
    BIG_W = 204,
    BIG_H = 54,
    SMALL_W = 184,
    SMALL_H = 29,
    SGP_X = 337,
    SGP_Y = 25,
    SGP_POINTS_RIGHT = 1013,
    SGP_POINTS_Y = 36,
    SGP_ROW_X = 346,
    SGP_ROW_Y = 71,
    SGP_ROW_DX = 356,
    SGP_ROW_DY = 75,
    SGP_ROW_W = 352,
    SGP_ROW_H = 70,
    SGP_NAME_X = 27,
    SGP_NAME_Y = 13,
    SGP_DESC_Y = 31,
    SGP_ROWS = 12,
    SGP_PER_COL = 6,
    SGP_DIGIT_W = 17,
    SGP_BACK_X = 900,
    SGP_BACK_Y = 524,
    HOVER_BACK = 1,
    HOVER_BUTTON = 10,
    HOVER_SGP_ROW = 100,
    HOVER_SGP_BACK = 200,
    HOVER_RUN = 201,
    HOVER_CANCEL = 202,
    HOVER_SK_ROW = 300,
    HOVER_SK_BACK = 400,
    HOVER_SK_UNIT = 500,
    SK_X = 304,
    SK_Y = 21,
    SK_TAB_X = 324,
    SK_TAB_Y = 26,
    SK_TAB_DX = 80,
    SK_TAB_W = 76,
    SK_TAB_H = 21,
    SK_ROW_Y = 50,
    SK_ROW_H = 82,
    SK_ROW_W = 664,
    SK_ROWS = 8,
    SK_ICON_DX = 6,
    SK_ICON_DY = 5,
    SK_NAME_DX = 32,
    SK_NAME_DY = 7,
    SK_OWNER_DX = 290,
    SK_MP_RIGHT = 544,
    SK_DESC_DX = 41,
    SK_DESC2_DX = 361,
    SK_DESC_DY = 31,
    SK_LINE = 14,
    SK_USE_X = 308,
    SK_USE_Y = 475,
    SK_USE_ROW_Y = 523,
    SK_USE_BACK_X = 787,
    SK_USE_BACK_Y = 651,
    SK_BOARD_DX = 432,
    SK_BOARD_DY = -350,
    HUD_BOARD_X = -62,
    HUD_BOARD_Y = 504,
    SK_FLOAT_MS = 1200,
    CARD_X = 997,
    CARD_Y = 21,
    FLOAT_RISE = 60,
    UNIT_HIT_W = 70,
    UNIT_HIT_H = 90,
    UNIT_HIT_FOOT = 20,
    TILE_LIT = 3,
    CONFIRM_X = 297,
    CONFIRM_Y = 112,
    CONFIRM_CX = 684,
    CONFIRM_NAME_Y = 173,
    CONFIRM_NAME_SIZE = 22,
    CONFIRM_TEXT_X = 491,
    CONFIRM_TEXT_Y = 221,
    CONFIRM_NOTE_Y = 311,
    CONFIRM_TEXT_SIZE = 15,
    CONFIRM_BTN_Y = 406,
    RUN_X = 628,
    RUN_W = 252,
    CANCEL_X = 900,
};

enum { BTN_SGP, BTN_EQUIP, BTN_SKILL, BTN_ORDER, BTN_LOG, BTN_SAVE, BTN_LOAD, BTN_OPTION, BUTTONS };
enum { ART_BAR, ART_PANEL, ART_STOCK, ART_DIGITS, ART_BACK, ART_BTN0, ART_SGP = ART_BTN0 + BUTTONS, ART_SGP_ROW, ART_SGP_BACK, ART_SGP_DIGITS, ART_CONFIRM, ART_RUN, ART_CANCEL, ART_SK_LIST, ART_SK_ROW, ART_SK_TAB0, ART_SK_USE = ART_SK_TAB0 + 3, ART_SK_BACK, ART_ICONS, ART_COUNT };

static const char *const artNames[ART_COUNT] = {"cmp_b00.agf", "cmp_b01.agf", "cmp_f00.agf", "flw_c15.agf", "cmp_g00.agf", "cmp_d00.agf", "cmp_d01.agf",
                                                "cmp_d02.agf", "cmp_d03.agf", "cmp_e00.agf", "cmp_e01.agf", "cmp_e02.agf", "cmp_e03.agf", "sgn_a00.agf",
                                                "sgn_a01.agf", "sgn_a02.agf", "sgn_c01.agf", "sgn_b00.agf", "sgn_b01.agf", "sgn_b02.agf", "sku_a00.agf", "sku_a01.agf", "sku_b01.agf", "sku_b02.agf", "sku_b03.agf", "sku_c00.agf", "sku_c01.agf", "pun_d00.agf"};

static const struct {
    int x, y;
    bool big;
} buttonAt[BUTTONS] = {{396, 598, true}, {602, 598, true}, {396, 657, true}, {602, 655, true},
                       {808, 597, false}, {808, 627, false}, {808, 657, false}, {808, 687, false}};

#define NO_EDGE 0xffffffffu

void sa_camp_init(SA *s) {
    Camp *c = &s->camp;
    memset(c, 0, sizeof *c);
    for (int i = 0; i < ART_COUNT; i++) c->art[i] = sa_load_image(&s->vfs, artNames[i]);
    sa_table_load(&c->sgp, &s->vfs, "SpGyun.obj");
}

void sa_camp_free(SA *s) {
    Camp *c = &s->camp;
    for (int i = 0; i < ART_COUNT; i++) sa_image_free(c->art[i]);
    sa_image_free(c->backdrop);
    sa_table_free(&c->sgp);
    memset(c, 0, sizeof *c);
}

static bool in(int x, int y, int rx, int ry, int w, int h) { return x >= rx && x < rx + w && y >= ry && y < ry + h; }

int sa_camp_points(const SA *s) { return s->vm.c[C_SGP] > 0 ? s->vm.c[C_SGP] : 0; }

static bool sgp_open(const SA *s, int row) {
    int flag = sa_table_int(&s->camp.sgp, row, 2);
    return !flag || (flag > 0 && flag < SA_F_VARS && s->vm.f[flag]);
}

static int skill_hit(SA *s, int x, int y);

static int hit(SA *s, int x, int y) {
    Camp *c = &s->camp;
    if (c->skillPage) return skill_hit(s, x, y);
    if (c->confirm >= 0) {
        if (in(x, y, RUN_X, CONFIRM_BTN_Y, RUN_W, BACK_H)) return HOVER_RUN;
        if (in(x, y, CANCEL_X, CONFIRM_BTN_Y, BACK_W, BACK_H)) return HOVER_CANCEL;
        return 0;
    }
    if (c->sgpOpen) {
        if (in(x, y, SGP_BACK_X, SGP_BACK_Y, BACK_W, BACK_H)) return HOVER_SGP_BACK;
        for (int i = 0; i < SGP_ROWS && i < c->sgp.rows; i++)
            if (in(x, y, SGP_ROW_X + i / SGP_PER_COL * SGP_ROW_DX, SGP_ROW_Y + i % SGP_PER_COL * SGP_ROW_DY, SGP_ROW_W, SGP_ROW_H)) return HOVER_SGP_ROW + i;
        return 0;
    }
    if (in(x, y, BACK_X, BACK_Y, BACK_W, BACK_H)) return HOVER_BACK;
    for (int i = 0; i < BUTTONS; i++)
        if (in(x, y, buttonAt[i].x, buttonAt[i].y, buttonAt[i].big ? BIG_W : SMALL_W, buttonAt[i].big ? BIG_H : SMALL_H)) return HOVER_BUTTON + i;
    return 0;
}

static void digits(SA *s, u32 *fb, int v, bool dash, int right, int y) {
    char t[16];
    snprintf(t, sizeof t, "%d", v);
    sa_draw_number(fb, s->camp.art[ART_DIGITS], dash ? "--" : t, right, y, 0, DIGIT_W, DIGIT_H);
}

static void draw_stock(SA *s, u32 *fb) {
    const VM *vm = &s->vm;
    sa_blit(fb, s->camp.art[ART_STOCK], STOCK_X, STOCK_Y);
    int goal[3];
    bool hasGoal = sa_mission_target(s, goal);
    for (int i = 0; i < 4; i++) {
        int y = STOCK_ROW0 + i * STOCK_BLOCK, town = vm->c[C_RESIDENTS + i], found = vm->c[C_FINDS + i];
        digits(s, fb, town, false, STOCK_RIGHT, y);
        digits(s, fb, found, false, STOCK_RIGHT, y + STOCK_PITCH);
        digits(s, fb, town + found, false, STOCK_RIGHT, y + 2 * STOCK_PITCH);
        digits(s, fb, i ? goal[i - 1] : 0, i == 0 || !hasGoal, STOCK_RIGHT, y + 3 * STOCK_PITCH);
    }
}

static void halve(u32 *fb) {
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u | (fb[i] >> 1 & 0x7f7f7fu);
}

static void draw_sgp(SA *s, u32 *fb) {
    Camp *c = &s->camp;
    halve(fb);
    sa_blit(fb, c->art[ART_SGP], SGP_X, SGP_Y);
    char t[16];
    snprintf(t, sizeof t, "%d", sa_camp_points(s));
    const Image *dg = c->art[ART_SGP_DIGITS];
    sa_draw_number(fb, dg, t, SGP_POINTS_RIGHT, SGP_POINTS_Y, 0, SGP_DIGIT_W, dg ? dg->h : 0);
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    for (int i = 0; i < SGP_ROWS && i < c->sgp.rows; i++) {
        int x = SGP_ROW_X + i / SGP_PER_COL * SGP_ROW_DX, y = SGP_ROW_Y + i % SGP_PER_COL * SGP_ROW_DY;
        bool open = sgp_open(s, i), afford = sa_table_int(&c->sgp, i, 3) <= sa_camp_points(s);
        int frame = !open ? 2 : c->hover == HOVER_SGP_ROW + i && afford ? 1 : 0;
        sa_blit_rect(fb, c->art[ART_SGP_ROW], 0, frame * SGP_ROW_H, SGP_ROW_W, SGP_ROW_H, x, y);
        if (!open) continue;
        sa_text_draw(fb, SA_W, SA_H, sa_table_text(&c->sgp, i, 1), -1, x + SGP_NAME_X, y + SGP_NAME_Y, 16, 0x000000, NO_EDGE);
        sa_text_draw(fb, SA_W, SA_H, sa_table_text(&c->sgp, i, 6), -1, x + SGP_NAME_X + 4, y + SGP_DESC_Y, 12, 0x000000, NO_EDGE);
    }
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    sa_blit_rect(fb, c->art[ART_SGP_BACK], 0, c->hover == HOVER_SGP_BACK ? BACK_H : 0, BACK_W, BACK_H, SGP_BACK_X, SGP_BACK_Y);
    if (c->confirm < 0) return;
    halve(fb);
    sa_blit(fb, c->art[ART_CONFIRM], CONFIRM_X, CONFIRM_Y);
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    const char *name = sa_table_text(&c->sgp, c->confirm, 1);
    sa_text_draw(fb, SA_W, SA_H, name, -1, CONFIRM_CX - sa_text_width(name, -1, CONFIRM_NAME_SIZE) / 2, CONFIRM_NAME_Y, CONFIRM_NAME_SIZE, 0x000000, NO_EDGE);
    sa_text_draw(fb, SA_W, SA_H, sa_table_text(&c->sgp, c->confirm, 6), -1, CONFIRM_TEXT_X, CONFIRM_TEXT_Y, CONFIRM_TEXT_SIZE, 0x000000, NO_EDGE);
    sa_text_draw(fb, SA_W, SA_H, sa_table_text(&c->sgp, c->confirm, 7), -1, CONFIRM_TEXT_X, CONFIRM_NOTE_Y, CONFIRM_TEXT_SIZE, 0x000000, NO_EDGE);
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    sa_blit_rect(fb, c->art[ART_RUN], 0, c->hover == HOVER_RUN ? BACK_H : 0, RUN_W, BACK_H, RUN_X, CONFIRM_BTN_Y);
    sa_blit_rect(fb, c->art[ART_CANCEL], 0, c->hover == HOVER_CANCEL ? BACK_H : 0, BACK_W, BACK_H, CANCEL_X, CONFIRM_BTN_Y);
}

typedef struct CampSkill {
    int member, row;
} CampSkill;

static int skill_row(const SA *s, const char *name) {
    const Table *k = &s->battle.skills;
    for (int i = 0; i < k->rows; i++)
        if (!strcmp(sa_table_text(k, i, 1), name)) return i;
    return -1;
}

static bool usable_here(const SA *s, int r) {
    const char *attribute = sa_table_text(&s->battle.skills, r, 14);
    return !strcmp(attribute, "回復");
}

static int camp_skills(SA *s, Member *ms, int *count, CampSkill *out) {
    *count = sa_party_member_list(&s->party, ms);
    int n = 0;
    for (int m = 0; m < *count && n < SK_ROWS; m++) {
        int u = sa_party_unit_row(&s->party, ms[m].unit);
        if (u < 0) continue;
        bool fixed = !strncmp(sa_table_text(&s->party.units, u, 41), "固定／", strlen("固定／"));
        for (int k = 0; k < 8 && n < SK_ROWS && (!fixed || k < 1); k++) {
            int r = skill_row(s, sa_table_text(&s->party.units, u, 41 + k * 3));
            if (r >= 0 && usable_here(s, r)) out[n++] = (CampSkill){m, r};
        }
    }
    return n;
}

static const char *attribute_order[] = {"斬", "打", "射", "無", "光", "闇", "火", "水", "土", "風", "雷", "回", "異"};

static int icon_index(const char *attribute) {
    for (int i = 0; i < 13; i++)
        if (!strncmp(attribute, attribute_order[i], strlen(attribute_order[i]))) return i;
    return 3;
}

static void wrap_lines(u32 *fb, const char *t, int x, int y) {
    for (int line = 0; *t && line < 3; line++) {
        const char *e = strchr(t, '|');
        int n = 0;
        for (const char *q = t; *q && q != e; n++) {
            u32 cp;
            q += sa_utf8_decode(q, &cp);
        }
        sa_text_draw(fb, SA_W, SA_H, t, n, x, y + line * SK_LINE, 13, 0x000000, NO_EDGE);
        if (!e) break;
        t = e + 1;
    }
}

static void skill_row_draw(SA *s, u32 *fb, const Member *ms, const CampSkill *k, int x, int y, int frame) {
    Camp *c = &s->camp;
    const Table *t = &s->battle.skills;
    sa_blit_rect(fb, c->art[ART_SK_ROW], 0, frame * SK_ROW_H, SK_ROW_W, SK_ROW_H, x, y);
    const Image *icons = c->art[ART_ICONS];
    if (icons) sa_blit_rect(fb, icons, 0, icon_index(sa_table_text(t, k->row, 14)) * 20, 20, 20, x + SK_ICON_DX, y + SK_ICON_DY);
    sa_text_draw(fb, SA_W, SA_H, sa_table_text(t, k->row, 1), -1, x + SK_NAME_DX, y + SK_NAME_DY, 16, 0x000000, NO_EDGE);
    int u = sa_party_unit_row(&s->party, ms[k->member].unit);
    sa_text_draw(fb, SA_W, SA_H, sa_table_text(&s->party.units, u, 2), -1, x + SK_OWNER_DX, y + SK_NAME_DY, 16, 0x000000, NO_EDGE);
    char mp[12];
    snprintf(mp, sizeof mp, "%d", sa_table_int(t, k->row, 13));
    sa_text_draw(fb, SA_W, SA_H, mp, -1, x + SK_MP_RIGHT - sa_text_width(mp, -1, 14), y + SK_NAME_DY + 1, 14, 0x000000, NO_EDGE);
    wrap_lines(fb, sa_table_text(t, k->row, 121), x + SK_DESC_DX, y + SK_DESC_DY);
    wrap_lines(fb, sa_table_text(t, k->row, 122), x + SK_DESC2_DX, y + SK_DESC_DY);
}

static void draw_skills(SA *s, u32 *fb) {
    Camp *c = &s->camp;
    Member ms[PARTY_SLOTS + PARTY_BENCH];
    CampSkill list[SK_ROWS];
    int count, n = camp_skills(s, ms, &count, list);
    halve(fb);
    sa_blit(fb, c->art[ART_SK_LIST], SK_X, SK_Y);
    for (int i = 0; i < 3; i++) sa_blit_rect(fb, c->art[ART_SK_TAB0 + i], 0, 0, SK_TAB_W, SK_TAB_H, SK_TAB_X + i * SK_TAB_DX, SK_TAB_Y);
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    const Member *card = NULL;
    if (c->skillPick < 0) {
        for (int i = 0; i < n; i++) skill_row_draw(s, fb, ms, &list[i], SK_X, SK_ROW_Y + i * SK_ROW_H, c->hover == HOVER_SK_ROW + i ? 3 : 0);
        if (c->hover >= HOVER_SK_ROW && c->hover < HOVER_SK_ROW + n) card = &ms[list[c->hover - HOVER_SK_ROW].member];
    } else if (c->skillPick < n) {
        const CampSkill *k = &list[c->skillPick];
        card = &ms[k->member];
        for (int i = 0; i < n; i++) skill_row_draw(s, fb, ms, &list[i], SK_X, SK_ROW_Y + i * SK_ROW_H, i == c->skillPick ? 3 : 0);
        sa_blit_rect(fb, c->art[ART_SK_BACK], 0, 0, BACK_W, BACK_H, BACK_X, BACK_Y);
        halve(fb);
        halve(fb);
        int frames[PARTY_SLOTS] = {0};
        if (c->hover >= HOVER_SK_UNIT && c->hover < HOVER_SK_UNIT + PARTY_SLOTS) frames[c->hover - HOVER_SK_UNIT] = TILE_LIT;
        sa_party_draw_board(&s->party, fb, sa_dungeon_platform(&s->dungeon), SK_BOARD_DX, SK_BOARD_DY, frames);
        if (c->floatMs > 0 && c->floatSlot >= 0) {
            int cx, cy;
            sa_party_tile_center(c->floatSlot, HUD_BOARD_X + SK_BOARD_DX, HUD_BOARD_Y + SK_BOARD_DY, &cx, &cy);
            char t[12];
            snprintf(t, sizeof t, "%d", c->floatValue);
            sa_text_draw(fb, SA_W, SA_H, t, -1, cx - 12, cy - FLOAT_RISE, 22, 0x30d060, 0x003010);
        }
        sa_blit(fb, c->art[ART_SK_USE], SK_USE_X, SK_USE_Y);
        skill_row_draw(s, fb, ms, k, SK_USE_X, SK_USE_ROW_Y, 0);
        sa_blit_rect(fb, c->art[ART_SK_BACK], 0, c->hover == HOVER_SK_BACK ? BACK_H : 0, BACK_W, BACK_H, SK_USE_BACK_X, SK_USE_BACK_Y);
    }
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    sa_card_draw(s, fb, card, CARD_X, CARD_Y);
    if (c->skillPick < 0) sa_blit_rect(fb, c->art[ART_SK_BACK], 0, c->hover == HOVER_SK_BACK ? BACK_H : 0, BACK_W, BACK_H, BACK_X, BACK_Y);
}

static void cast_on(SA *s, int slot) {
    Camp *c = &s->camp;
    Member ms[PARTY_SLOTS + PARTY_BENCH];
    CampSkill list[SK_ROWS];
    int count, n = camp_skills(s, ms, &count, list);
    if (c->skillPick < 0 || c->skillPick >= n || !s->party.unit[slot]) return;
    const CampSkill *k = &list[c->skillPick];
    Member *caster = &ms[k->member];
    int cost = sa_table_int(&s->battle.skills, k->row, 13);
    if (*caster->mp < cost) return;
    *caster->mp -= cost;
    int lo = sa_table_int(&s->battle.skills, k->row, 17), hi = sa_table_int(&s->battle.skills, k->row, 18);
    int heal = hi > lo ? lo + (int)(sa_vm_random(&s->vm) % (u32)(hi - lo + 1)) : lo, most = sa_party_max_hp(&s->party, slot);
    s->party.hp[slot] = s->party.hp[slot] + heal > most ? most : s->party.hp[slot] + heal;
    c->floatSlot = slot;
    c->floatValue = heal;
    c->floatMs = SK_FLOAT_MS;
    sa_log(s, "camp skill %s on slot %d: +%d", sa_table_text(&s->battle.skills, k->row, 1), slot, heal);
}

static int skill_hit(SA *s, int x, int y) {
    Camp *c = &s->camp;
    if (c->skillPick < 0) {
        if (in(x, y, BACK_X, BACK_Y, BACK_W, BACK_H)) return HOVER_SK_BACK;
        Member ms[PARTY_SLOTS + PARTY_BENCH];
        CampSkill list[SK_ROWS];
        int count, n = camp_skills(s, ms, &count, list);
        for (int i = 0; i < n; i++)
            if (in(x, y, SK_X, SK_ROW_Y + i * SK_ROW_H, SK_ROW_W, SK_ROW_H)) return HOVER_SK_ROW + i;
        return 0;
    }
    if (in(x, y, SK_USE_BACK_X, SK_USE_BACK_Y, BACK_W, BACK_H)) return HOVER_SK_BACK;
    for (int slot = 0; slot < PARTY_SLOTS; slot++) {
        if (!s->party.unit[slot]) continue;
        int cx, cy;
        sa_party_tile_center(slot, HUD_BOARD_X + SK_BOARD_DX, HUD_BOARD_Y + SK_BOARD_DY, &cx, &cy);
        if (in(x, y, cx - UNIT_HIT_W / 2, cy - UNIT_HIT_H, UNIT_HIT_W, UNIT_HIT_H + UNIT_HIT_FOOT)) return HOVER_SK_UNIT + slot;
    }
    return 0;
}

static void draw_backdrop(SA *s, u32 *fb) {
    sa_dungeon_draw(&s->dungeon, fb);
    halve(fb);
    s->party.sgp = sa_camp_points(s);
    sa_dungeon_draw_frame(&s->dungeon, fb, &s->party, s->vm.c[C_RANK_MAX]);
    s->party.sgp = -1;
    sa_town_draw_status_goal(&s->town, &s->vm, fb);
    Camp *c = &s->camp;
    sa_blit(fb, c->art[ART_BAR], BAR_X, BAR_Y);
    sa_blit(fb, c->art[ART_PANEL], PANEL_X, PANEL_Y);
    const char *dungeon = NULL, *floor = NULL;
    sa_exp_place_names(s, &dungeon, &floor);
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    if (dungeon) sa_text_draw(fb, SA_W, SA_H, dungeon, -1, NAME_X, NAMES_Y, 16, 0x000000, NO_EDGE);
    if (floor) sa_text_draw(fb, SA_W, SA_H, floor, -1, FLOOR_X, NAMES_Y, 16, 0x000000, NO_EDGE);
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    digits(s, fb, s->dungeon.steps, false, STEPS_RIGHT, STEPS_Y);
    sa_dungeon_draw_map(&s->dungeon, fb, MAP_X, MAP_Y, MAP_SCALE);
    draw_stock(s, fb);
}

void sa_camp_draw_backdrop(SA *s, u32 *fb) {
    Camp *c = &s->camp;
    if (c->backdrop) memcpy(fb, c->backdrop->px, sizeof(u32) * (size_t)SA_W * SA_H);
    else draw_backdrop(s, fb);
}

static void draw(SA *s) {
    Camp *c = &s->camp;
    u32 *fb = s->fb;
    draw_backdrop(s, fb);
    for (int i = 0; i < BUTTONS; i++) {
        int w = buttonAt[i].big ? BIG_W : SMALL_W, h = buttonAt[i].big ? BIG_H : SMALL_H;
        sa_blit_rect(fb, c->art[ART_BTN0 + i], 0, !c->sgpOpen && c->hover == HOVER_BUTTON + i ? h : 0, w, h, buttonAt[i].x, buttonAt[i].y);
    }
    sa_blit_rect(fb, c->art[ART_BACK], 0, !c->sgpOpen && c->hover == HOVER_BACK ? BACK_H : 0, BACK_W, BACK_H, BACK_X, BACK_Y);
    if (c->sgpOpen) draw_sgp(s, fb);
    if (c->skillPage) draw_skills(s, fb);
}

static void keep_backdrop(SA *s) {
    Camp *c = &s->camp;
    if (!c->backdrop) c->backdrop = sa_image_new(SA_W, SA_H);
    draw_backdrop(s, c->backdrop->px);
    for (int i = 0; i < BUTTONS; i++) {
        int w = buttonAt[i].big ? BIG_W : SMALL_W, h = buttonAt[i].big ? BIG_H : SMALL_H;
        sa_blit_rect(c->backdrop->px, c->art[ART_BTN0 + i], 0, 0, w, h, buttonAt[i].x, buttonAt[i].y);
    }
}

void sa_camp_open(SA *s) {
    Camp *c = &s->camp;
    c->active = true;
    c->sgpOpen = false;
    c->skillPage = false;
    c->confirm = -1;
    c->hover = 0;
    sa_image_free(c->backdrop);
    c->backdrop = NULL;
    s->mode = MODE_CAMP;
    s->dirty = true;
}

void sa_camp_return(SA *s) {
    s->mode = MODE_CAMP;
    s->camp.hover = 0;
    sa_image_free(s->camp.backdrop);
    s->camp.backdrop = NULL;
    s->dirty = true;
}

int sa_back_mode(const SA *s) { return s->camp.active ? MODE_CAMP : MODE_TOWN; }

static void leave(SA *s) {
    Camp *c = &s->camp;
    c->active = false;
    sa_image_free(c->backdrop);
    c->backdrop = NULL;
    s->mode = MODE_DUNGEON;
    s->dirty = true;
}

static void use_sgp(SA *s, int row) {
    Camp *c = &s->camp;
    int cost = sa_table_int(&c->sgp, row, 3);
    if (!sgp_open(s, row) || cost > sa_camp_points(s)) return;
    if (!sa_exp_special(s, sa_table_int(&c->sgp, row, 0), sa_table_int(&c->sgp, row, 4), sa_table_int(&c->sgp, row, 5))) return;
    s->vm.c[C_SGP] -= cost;
    sa_log(s, "special: %s", sa_table_text(&c->sgp, row, 1));
    if (s->mode != MODE_CAMP) {
        c->active = false;
        return;
    }
}

static void press(SA *s, int button) {
    switch (button) {
    case BTN_SGP:
        s->camp.sgpOpen = true;
        break;
    case BTN_EQUIP:
        keep_backdrop(s);
        sa_equip_open(s);
        break;
    case BTN_SKILL:
        s->camp.skillPage = true;
        s->camp.skillPick = -1;
        s->camp.floatMs = 0;
        s->card.showDetail = false;
        break;
    case BTN_ORDER:
        keep_backdrop(s);
        sa_deploy_open(s);
        break;
    case BTN_SAVE:
    case BTN_LOAD:
        keep_backdrop(s);
        sa_savescreen_open(s, button == BTN_SAVE);
        break;
    default:
        sa_log(s, "camp button %d not built yet", button);
        break;
    }
}

bool sa_camp_frame(SA *s) {
    if (s->mode != MODE_CAMP) return false;
    Camp *c = &s->camp;
    if (sa_party_tick(&s->party, 16)) s->dirty = true;
    int h = hit(s, s->mouseX, s->mouseY);
    if (h != c->hover) {
        c->hover = h;
        s->dirty = true;
    }
    if (c->floatMs > 0) {
        c->floatMs = c->floatMs > 16 ? c->floatMs - 16 : 0;
        s->dirty = true;
    }
    if (c->skillPage) sa_card_input(s);
    if (s->rightClicked) {
        s->rightClicked = false;
        if (c->skillPage) {
            if (c->skillPick >= 0) c->skillPick = -1;
            else c->skillPage = false;
        } else if (c->confirm >= 0) c->confirm = -1;
        else if (c->sgpOpen) c->sgpOpen = false;
        else leave(s);
        s->dirty = true;
    }
    if (s->clicked) {
        s->clicked = false;
        if (c->skillPage) {
            if (h == HOVER_SK_BACK) {
                if (c->skillPick >= 0) c->skillPick = -1;
                else c->skillPage = false;
            } else if (h >= HOVER_SK_UNIT)
                cast_on(s, h - HOVER_SK_UNIT);
            else if (h >= HOVER_SK_ROW)
                c->skillPick = h - HOVER_SK_ROW;
        } else if (c->confirm >= 0) {
            int row = c->confirm;
            if (h == HOVER_RUN || h == HOVER_CANCEL) c->confirm = -1;
            if (h == HOVER_RUN) use_sgp(s, row);
        } else if (c->sgpOpen) {
            if (h == HOVER_SGP_BACK) c->sgpOpen = false;
            else if (h >= HOVER_SGP_ROW && h < HOVER_SGP_BACK && sgp_open(s, h - HOVER_SGP_ROW) && sa_table_int(&c->sgp, h - HOVER_SGP_ROW, 3) <= sa_camp_points(s))
                c->confirm = h - HOVER_SGP_ROW;
        } else if (h == HOVER_BACK)
            leave(s);
        else if (h >= HOVER_BUTTON)
            press(s, h - HOVER_BUTTON);
        s->dirty = true;
    }
    if (s->mode == MODE_CAMP && s->dirty) {
        draw(s);
        s->serial++;
        s->dirty = false;
    }
    return true;
}

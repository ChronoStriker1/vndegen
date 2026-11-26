#include "sa_private.h"

enum {
    CW = 10,
    CH = 13,
    NAME_X = 28,
    NAME_Y = 8,
    RACE_Y = 31,
    BADGE_X = 181,
    BADGE_Y = 50,
    RIGHT_A = 262,
    RIGHT_B = 232,
    RIGHT_C = 102,
    ROW0 = 9,
    PITCH = 20,
    SKILL_Y = 276,
    SKILL_ICON_X = 61,
    SKILL_ICON_DY = -3,
    STAR_X = 48,
    ATTR_X = 242,
    ATTR_Y = 208,
    RAISE_DX = 30,
    RAISE_ROW = 2,
    LOWER_ROW = 1,
    CARD_LABELS = 22,
    SKILL_NAME_X = 91,
    SKILL_MP_RIGHT = 256,
    SKILLS = 6,
    SLOTS_X = 8,
    SLOTS_Y = 395,
    ABILITY_RIGHT = 253,
    ABILITY_Y = 417,
    SPRITE_ANCHOR_X = 75,
    SPRITE_ANCHOR_Y = 162,
    DESC_X = 997,
    DESC_Y = 521,
    DESC_TEXT_X = 1005,
    DESC_TEXT_Y = 545,
    DESC_W = 250,
    DESC_SIZE = 16,
    TEXT = 15,
    DETAIL_NAME_X = 81,
    DETAIL_Y = 37,
    DETAIL_PITCH = 140,
    DETAIL_TEXT_X = 12,
    DETAIL_TEXT_DY = 24,
    DETAIL_W = 248,
    DETAIL_LINE = 16,
    DETAIL_SIZE = 14,
    DETAIL_HEAD_DY = 2,
    DETAIL_ITEM_Y = 477,
    TOGGLE_X = 1191,
    TOGGLE_Y = 2,
    TOGGLE_W = 72,
    TOGGLE_H = 18,
};

#define NO_EDGE 0xffffffffu

#define FIXED_PREFIX "固定／"

static const char *const iconOrder[] = {"斬", "打", "射", "無", "光", "闇", "火", "水", "土", "風", "雷", "回", "異"};

void sa_card_init(Card *c, Vfs *vfs) {
    memset(c, 0, sizeof *c);
    c->frame = sa_load_image(vfs, "grw_f00.agf");
    c->digits = sa_load_image(vfs, "grw_i00.agf");
    for (int i = 0; i < CARD_LABELS; i++) {
        char n[16];
        snprintf(n, sizeof n, "uts_h%02d.agf", i < 20 ? i : 78 + i);
        c->label[i] = sa_load_image(vfs, n);
    }
    c->desc = sa_load_image(vfs, "pun_b00.agf");
    c->detail = sa_load_image(vfs, "pun_a02.agf");
    c->toggle = sa_load_image(vfs, "pun_e00.agf");
    Image *icons = sa_load_image(vfs, "pun_d00.agf");
    if (icons) {
        for (int i = 0; i < CARD_ICONS && (i + 1) * 20 <= icons->h; i++) {
            c->icon[i] = sa_image_new(20, 20);
            for (int y = 0; y < 20; y++) memcpy(c->icon[i]->px + (size_t)y * 20, icons->px + (size_t)(i * 20 + y) * icons->w, 20 * 4);
        }
        sa_image_free(icons);
    }
}

void sa_card_free(Card *c) {
    sa_image_free(c->frame);
    sa_image_free(c->digits);
    for (int i = 0; i < CARD_LABELS; i++) sa_image_free(c->label[i]);
    sa_image_free(c->desc);
    sa_image_free(c->detail);
    sa_image_free(c->toggle);
    sa_image_free(c->portrait);
    for (int i = 0; i < CARD_ICONS; i++) sa_image_free(c->icon[i]);
    memset(c, 0, sizeof *c);
}

static void digits(const Card *c, u32 *fb, const char *s, int right, int y) { sa_draw_number(fb, c->digits, s, right, y, 0, CW, CH); }

static void value(const Card *c, u32 *fb, int v, int right, int y) {
    char s[16];
    snprintf(s, sizeof s, "%d", v);
    digits(c, fb, s, right, y);
}

static void signed_value(const Card *c, u32 *fb, int v, int right, int y) {
    char t[16];
    snprintf(t, sizeof t, "%+d", v);
    sa_draw_number(fb, c->digits, t, right, y, v < 0 ? LOWER_ROW : RAISE_ROW, CW, CH);
}

static void pair(const Card *c, u32 *fb, int a, int b, int right, int y) {
    char s[24];
    snprintf(s, sizeof s, "%d/%3d", a, b);
    for (char *p = s; *p; p++)
        if (*p == ' ') *p = '?';
    sa_draw_number(fb, c->digits, s, right, y, 0, CW, CH);
}

static void text_right(u32 *fb, const char *t, int right, int y, int size) {
    sa_text_draw(fb, SA_W, SA_H, t, -1, right - sa_text_width(t, -1, size), y, size, 0x000000, NO_EDGE);
}

static int icon_of(const char *attribute) {
    for (size_t i = 0; i < sizeof iconOrder / sizeof iconOrder[0]; i++)
        if (!strncmp(attribute, iconOrder[i], strlen(iconOrder[i]))) return (int)i;
    return 3;
}

static void draw_status(SA *s, u32 *fb, const Member *m, int x, int y);

static void wrap(u32 *fb, const char *d, int x, int y, int width, int size, int pitch, int lines) {
    int line = 0;
    while (*d && line < lines) {
        int n = 0, w = 0;
        const char *q = d;
        while (*q && *q != '|') {
            u32 cp;
            int len = sa_utf8_decode(q, &cp);
            int cwid = sa_char_width(cp, size);
            if (n && w + cwid > width) break;
            w += cwid;
            q += len;
            n++;
        }
        sa_text_draw(fb, SA_W, SA_H, d, n, x, y + line * pitch, size, 0x000000, NO_EDGE);
        line++;
        d = *q == '|' ? q + 1 : q;
    }
}

static const char *ability_text(const SA *s, const char *name) {
    const Table *a = &s->party.abilities;
    for (int r = 0; r < a->rows; r++)
        if (!strcmp(sa_table_text(a, r, 2), name)) return sa_table_text(a, r, 6);
    return "";
}

static const char *item_name(const SA *s, int id) {
    for (int i = 0; i < s->equip.items.rows; i++)
        if (sa_table_int(&s->equip.items, i, 0) == id) return sa_table_text(&s->equip.items, i, 1);
    return NULL;
}

static void draw_detail(SA *s, u32 *fb, const Member *m, int x, int y) {
    const Card *c = &s->card;
    const Party *p = &s->party;
    sa_blit(fb, c->detail, x, y);
    sa_blit(fb, c->desc, DESC_X, DESC_Y);
    if (!m) return;
    int r = sa_party_unit_row(p, m->unit);
    const Table *u = &p->units;
    sa_text_draw(fb, SA_W, SA_H, sa_table_text(u, r, 2), -1, x + NAME_X, y + NAME_Y + DETAIL_HEAD_DY, 16, 0x000000, NO_EDGE);
    value(c, fb, *m->level, x + RIGHT_A - 2, y + ROW0 + DETAIL_HEAD_DY);
    for (int k = 0; k < 3; k++) {
        const char *name = sa_table_text(u, r, 34 + k);
        int sy = y + DETAIL_Y + k * DETAIL_PITCH;
        sa_text_draw(fb, SA_W, SA_H, name, -1, x + DETAIL_NAME_X, sy, TEXT, 0x000000, NO_EDGE);
        wrap(fb, ability_text(s, name), x + DETAIL_TEXT_X, sy + DETAIL_TEXT_DY, DETAIL_W, DETAIL_SIZE, DETAIL_LINE, 6);
    }
    const char *item = *m->equip ? item_name(s, *m->equip) : NULL;
    text_right(fb, item ? item : "なし", x + ABILITY_RIGHT, y + DETAIL_ITEM_Y, TEXT);
}

static void draw_toggle(SA *s, u32 *fb) {
    const Card *c = &s->card;
    sa_blit_rect(fb, c->toggle, c->showDetail ? 0 : TOGGLE_W, c->toggleHover ? TOGGLE_H : 0, TOGGLE_W, TOGGLE_H, TOGGLE_X, TOGGLE_Y);
}

static bool toggle_hit(int x, int y) { return x >= TOGGLE_X && x < TOGGLE_X + TOGGLE_W && y >= TOGGLE_Y && y < TOGGLE_Y + TOGGLE_H; }

void sa_card_input(SA *s) {
    Card *c = &s->card;
    bool on = toggle_hit(s->mouseX, s->mouseY);
    if (on != c->toggleHover) {
        c->toggleHover = on;
        s->dirty = true;
    }
    if (on && s->clicked) {
        s->clicked = false;
        c->showDetail = !c->showDetail;
        s->dirty = true;
    }
}

void sa_card_draw(SA *s, u32 *fb, const Member *m, int x, int y) {
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    draw_toggle(s, fb);
    if (s->card.showDetail) {
        draw_detail(s, fb, m, x, y);
        if (m) wrap(fb, sa_table_text(&s->party.units, sa_party_unit_row(&s->party, m->unit), 76), DESC_TEXT_X, DESC_TEXT_Y, DESC_W, DESC_SIZE, 17, 8);
        sa_font_set_plain(false);
        sa_font_set_spacing(1);
        return;
    }
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    draw_status(s, fb, m, x, y);
}

Image *sa_sheet_load(Vfs *vfs, const char *name, SpriteSheet *sheet) {
    size_t n;
    u8 *d = sa_vfs_read(vfs, name, &n);
    Image *im = d ? sa_agf_decode(d, n) : NULL;
    *sheet = (SpriteSheet){1, im ? im->w : 0, im ? im->h : 0, im ? 1 : 0, im ? im->w / 2 : 0, im ? im->h : 0};
    if (im && n >= 0x68) {
        float f[5];
        memcpy(f, d + 0x2c, sizeof f);
        int usedW = (int)rd32(d + 0x24), usedH = (int)rd32(d + 0x28), cw = (int)f[3], ch = (int)f[4], frames = (int)rd32(d + 0x64);
        if (cw > 0 && ch > 0 && usedW >= cw && usedH >= ch && usedW <= im->w && usedH <= im->h && frames > 0 && frames <= (usedW / cw) * (usedH / ch))
            *sheet = (SpriteSheet){usedW / cw, cw, ch, frames, (int)f[0], (int)f[1]};
    }
    free(d);
    return im;
}

void sa_sheet_draw(u32 *fb, const Image *im, const SpriteSheet *sheet, int frame, int x, int y) {
    if (!im || sheet->frames <= 0) return;
    frame %= sheet->frames;
    sa_blit_rect(fb, im, frame % sheet->cols * sheet->cellW, frame / sheet->cols * sheet->cellH, sheet->cellW, sheet->cellH, x - sheet->anchorX, y - sheet->anchorY);
}

static void draw_portrait(SA *s, u32 *fb, int number, int x, int y) {
    static SpriteSheet sheet;
    Card *c = &s->card;
    if (c->portraitNumber != number || !c->portrait) {
        sa_image_free(c->portrait);
        char name[24];
        snprintf(name, sizeof name, "u%04da2.agf", number);
        c->portrait = sa_sheet_load(&s->vfs, name, &sheet);
        c->portraitNumber = number;
    }
    sa_sheet_draw(fb, c->portrait, &sheet, (int)(s->party.ms / PARTY_CELL_MS), x + SPRITE_ANCHOR_X, y + SPRITE_ANCHOR_Y);
}

static void draw_status(SA *s, u32 *fb, const Member *m, int x, int y) {
    const Card *c = &s->card;
    Party *p = &s->party;
    sa_blit(fb, c->frame, x, y);
    sa_blit(fb, c->desc, DESC_X, DESC_Y);
    if (!m) return;
    int r = sa_party_unit_row(p, m->unit);
    const Table *u = &p->units;
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    sa_text_draw(fb, SA_W, SA_H, sa_table_text(u, r, 2), -1, x + NAME_X, y + NAME_Y, 16, 0x000000, NO_EDGE);
    sa_text_draw(fb, SA_W, SA_H, sa_table_text(u, r, 4), -1, x + NAME_X - 1, y + RACE_Y, TEXT, 0x000000, NO_EDGE);
    value(c, fb, *m->level, x + RIGHT_A - 2, y + ROW0);
    int need = s->battle.lvup.rows ? sa_battle_exp_needed(&s->battle, r, *m->level) - *m->exp : 0;
    value(c, fb, need, x + RIGHT_A, y + ROW0 + 22);
    int badge = sa_party_member_badge(p, m);
    int bi = badge < 0 ? -1 : badge < 20 ? badge : badge - 78;
    if (bi >= 0 && bi < CARD_LABELS) sa_blit(fb, c->label[bi], x + BADGE_X, y + BADGE_Y);
    const Image *attr = c->icon[icon_of(sa_table_text(u, r, 14))];
    if (attr) sa_blit(fb, attr, x + ATTR_X, y + ATTR_Y);
    value(c, fb, sa_table_int(u, r, 15), x + RIGHT_A, y + ROW0 + 63);
    pair(c, fb, *m->hp, sa_party_member_value(p, m, 0), x + RIGHT_A, y + ROW0 + 83);
    pair(c, fb, *m->mp, sa_party_member_value(p, m, 1), x + RIGHT_A, y + ROW0 + 103);
    value(c, fb, *m->sp, x + RIGHT_A, y + ROW0 + 123);
    value(c, fb, sa_party_member_value(p, m, 2 + STAT_SPEED), x + RIGHT_B, y + ROW0 + 143);
    value(c, fb, sa_party_member_value(p, m, 2 + STAT_EVASION), x + RIGHT_B, y + ROW0 + 163);
    int gov = sa_party_member_value(p, m, 2 + STAT_GOVERN), lead = sa_party_member_value(p, m, 2 + STAT_COMMAND);
    value(c, fb, gov, x + RIGHT_C, y + ROW0 + 183);
    value(c, fb, lead, x + RIGHT_B, y + ROW0 + 183);
    int shown[2], tone[2];
    sa_deploy_govern(p, m->unit, sa_party_post_of(p, m->unit), shown, tone);
    if (shown[0] != gov) signed_value(c, fb, shown[0] - gov, x + RIGHT_C + RAISE_DX, y + ROW0 + 183);
    if (shown[1] != lead) signed_value(c, fb, shown[1] - lead, x + RIGHT_B + RAISE_DX, y + ROW0 + 183);
    value(c, fb, sa_party_member_value(p, m, 2 + STAT_ATTACK), x + RIGHT_C, y + ROW0 + 223);
    value(c, fb, sa_party_member_value(p, m, 2 + STAT_MAGIC), x + RIGHT_B, y + ROW0 + 223);
    value(c, fb, sa_party_member_value(p, m, 2 + STAT_DEFENCE), x + RIGHT_C, y + ROW0 + 243);
    value(c, fb, sa_party_member_value(p, m, 2 + STAT_RESIST), x + RIGHT_B, y + ROW0 + 243);
    int known = 0;
    bool fixed = !strncmp(sa_table_text(u, r, 41), FIXED_PREFIX, strlen(FIXED_PREFIX));
    for (int k = 0; k < 8 && known < SKILLS && (!fixed || known < 1); k++) {
        const char *name = sa_table_text(u, r, 41 + k * 3);
        if (!*name || !strcmp(name, "*")) continue;
        int sr = -1;
        for (int i = 0; i < s->battle.skills.rows && sr < 0; i++)
            if (!strcmp(sa_table_text(&s->battle.skills, i, 1), name)) sr = i;
        int ry = y + SKILL_Y + known * PITCH;
        if (sr >= 0) {
            const Image *ic = c->icon[icon_of(sa_table_text(&s->battle.skills, sr, 14))];
            if (ic) sa_blit(fb, ic, x + SKILL_ICON_X, ry + SKILL_ICON_DY);
            value(c, fb, sa_table_int(&s->battle.skills, sr, 13), x + SKILL_MP_RIGHT, ry + 1);
        }
        bool isFixed = !strncmp(name, FIXED_PREFIX, strlen(FIXED_PREFIX));
        if (isFixed) sa_text_draw(fb, SA_W, SA_H, "★", -1, x + STAR_X, ry, TEXT, 0x000000, NO_EDGE);
        sa_text_draw(fb, SA_W, SA_H, isFixed ? name + strlen(FIXED_PREFIX) : name, -1, x + SKILL_NAME_X, ry, TEXT, 0x000000, NO_EDGE);
        known++;
    }
    char slots[16];
    snprintf(slots, sizeof slots, "%d/%d", known, sa_table_int(u, r, 38) + (fixed ? 1 : 0));
    digits(c, fb, slots, x + SLOTS_X + (int)strlen(slots) * CW, y + SLOTS_Y);
    for (int k = 0; k < 3; k++) text_right(fb, sa_table_text(u, r, 34 + k), x + ABILITY_RIGHT, y + ABILITY_Y + k * PITCH, TEXT);
    const char *item = *m->equip ? item_name(s, *m->equip) : NULL;
    text_right(fb, item ? item : "なし", x + ABILITY_RIGHT, y + ABILITY_Y + 3 * PITCH, TEXT);
    draw_portrait(s, fb, sa_table_int(u, r, 11), x, y);
    wrap(fb, sa_table_text(u, r, 76), DESC_TEXT_X, DESC_TEXT_Y, DESC_W, DESC_SIZE, 17, 8);
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
}

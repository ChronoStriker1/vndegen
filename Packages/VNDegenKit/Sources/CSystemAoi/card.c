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
    SKILL_ICON_X = 64,
    SKILL_NAME_X = 91,
    SKILL_MP_RIGHT = 256,
    SKILLS = 6,
    SLOTS_X = 8,
    SLOTS_Y = 395,
    ABILITY_RIGHT = 253,
    ABILITY_Y = 417,
    SPRITE_X = 75,
    SPRITE_FEET = 171,
    DESC_X = 997,
    DESC_Y = 521,
    DESC_TEXT_X = 1005,
    DESC_TEXT_Y = 545,
    DESC_W = 250,
    DESC_SIZE = 16,
    TEXT = 15,
};

#define NO_EDGE 0xffffffffu

static const char *const iconOrder[] = {"斬", "打", "射", "無", "光", "闇", "火", "水", "土", "風", "雷", "回", "異"};

void sa_card_init(Card *c, Vfs *vfs) {
    memset(c, 0, sizeof *c);
    c->frame = sa_load_image(vfs, "grw_f00.agf");
    c->digits = sa_load_image(vfs, "grw_i00.agf");
    c->badge = sa_load_image(vfs, "uts_h98.agf");
    c->desc = sa_load_image(vfs, "pun_b00.agf");
    Image *icons = sa_load_image(vfs, "pun_d00.agf");
    if (icons) {
        for (int i = 0; i < CARD_ICONS && (i + 1) * 20 <= icons->h; i++) {
            Image *one = sa_image_new(20, 20);
            for (int y = 0; y < 20; y++) memcpy(one->px + (size_t)y * 20, icons->px + (size_t)(i * 20 + y) * icons->w, 20 * 4);
            c->icon[i] = sa_image_scale(one, 16, 16);
            sa_image_free(one);
        }
        sa_image_free(icons);
    }
}

void sa_card_free(Card *c) {
    sa_image_free(c->frame);
    sa_image_free(c->digits);
    sa_image_free(c->badge);
    sa_image_free(c->desc);
    for (int i = 0; i < CARD_ICONS; i++) sa_image_free(c->icon[i]);
    memset(c, 0, sizeof *c);
}

static void digits(const Card *c, u32 *fb, const char *s, int right, int y) { sa_draw_number(fb, c->digits, s, right, y, 0, CW, CH); }

static void value(const Card *c, u32 *fb, int v, int right, int y) {
    char s[16];
    snprintf(s, sizeof s, "%d", v);
    digits(c, fb, s, right, y);
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

void sa_card_draw(SA *s, u32 *fb, int slot, int x, int y) {
    const Card *c = &s->card;
    const Party *p = &s->party;
    sa_blit(fb, c->frame, x, y);
    sa_blit(fb, c->desc, DESC_X, DESC_Y);
    if (slot < 0 || !p->unit[slot]) return;
    int r = sa_party_unit_row(p, p->unit[slot]);
    const Table *u = &p->units;
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    sa_text_draw(fb, SA_W, SA_H, sa_table_text(u, r, 2), -1, x + NAME_X, y + NAME_Y, 16, 0x000000, NO_EDGE);
    sa_text_draw(fb, SA_W, SA_H, sa_table_text(u, r, 4), -1, x + NAME_X - 1, y + RACE_Y, TEXT, 0x000000, NO_EDGE);
    value(c, fb, p->level[slot], x + RIGHT_A - 2, y + ROW0);
    int need = s->battle.lvup.rows ? sa_battle_exp_needed(&s->battle, r, p->level[slot]) - p->exp[slot] : 0;
    value(c, fb, need, x + RIGHT_A, y + ROW0 + 22);
    sa_blit(fb, c->badge, x + BADGE_X, y + BADGE_Y);
    value(c, fb, sa_table_int(u, r, 15), x + RIGHT_A, y + ROW0 + 63);
    pair(c, fb, p->hp[slot], sa_party_max_hp(p, slot), x + RIGHT_A, y + ROW0 + 83);
    pair(c, fb, p->mp[slot], sa_party_max_mp(p, slot), x + RIGHT_A, y + ROW0 + 103);
    value(c, fb, p->sp[slot], x + RIGHT_A, y + ROW0 + 123);
    value(c, fb, sa_party_stat(p, slot, STAT_SPEED), x + RIGHT_B, y + ROW0 + 143);
    value(c, fb, sa_party_stat(p, slot, STAT_EVASION), x + RIGHT_B, y + ROW0 + 163);
    value(c, fb, sa_party_stat(p, slot, STAT_GOVERN), x + RIGHT_C, y + ROW0 + 183);
    value(c, fb, sa_party_stat(p, slot, STAT_COMMAND), x + RIGHT_B, y + ROW0 + 183);
    value(c, fb, sa_party_stat(p, slot, STAT_ATTACK), x + RIGHT_C, y + ROW0 + 223);
    value(c, fb, sa_party_stat(p, slot, STAT_MAGIC), x + RIGHT_B, y + ROW0 + 223);
    value(c, fb, sa_party_stat(p, slot, STAT_DEFENCE), x + RIGHT_C, y + ROW0 + 243);
    value(c, fb, sa_party_stat(p, slot, STAT_RESIST), x + RIGHT_B, y + ROW0 + 243);
    int known = 0;
    for (int k = 0; k < 8 && known < SKILLS; k++) {
        const char *name = sa_table_text(u, r, 41 + k * 3);
        if (!*name || !strcmp(name, "*")) continue;
        int sr = -1;
        for (int i = 0; i < s->battle.skills.rows && sr < 0; i++)
            if (!strcmp(sa_table_text(&s->battle.skills, i, 1), name)) sr = i;
        int ry = y + SKILL_Y + known * PITCH;
        if (sr >= 0) {
            const Image *ic = c->icon[icon_of(sa_table_text(&s->battle.skills, sr, 14))];
            if (ic) sa_blit(fb, ic, x + SKILL_ICON_X, ry + 1);
            value(c, fb, sa_table_int(&s->battle.skills, sr, 13), x + SKILL_MP_RIGHT, ry + 1);
        }
        sa_text_draw(fb, SA_W, SA_H, name, -1, x + SKILL_NAME_X, ry, TEXT, 0x000000, NO_EDGE);
        known++;
    }
    char slots[16];
    snprintf(slots, sizeof slots, "%d/%d", known, sa_table_int(u, r, 38));
    digits(c, fb, slots, x + SLOTS_X + (int)strlen(slots) * CW, y + SLOTS_Y);
    for (int k = 0; k < 3; k++) text_right(fb, sa_table_text(u, r, 34 + k), x + ABILITY_RIGHT, y + ABILITY_Y + k * PITCH, TEXT);
    if (p->equip[slot]) {
        for (int i = 0; i < s->equip.items.rows; i++)
            if (sa_table_int(&s->equip.items, i, 0) == p->equip[slot]) {
                text_right(fb, sa_table_text(&s->equip.items, i, 1), x + ABILITY_RIGHT, y + ABILITY_Y + 3 * PITCH, TEXT);
                break;
            }
    }
    const Image *im = p->sprite[slot];
    if (im) {
        int cols, rows;
        sa_sprite_layout(im, &cols, &rows);
        int cw = im->w / cols, ch = im->h / rows;
        sa_blit_rect(fb, im, 0, 0, cw, ch, x + SPRITE_X - cw / 2, y + SPRITE_FEET - ch);
    }
    const char *d = sa_table_text(u, r, 76);
    int line = 0;
    while (*d && line < 8) {
        int n = 0, w = 0;
        const char *q = d;
        while (*q && *q != '|') {
            u32 cp;
            int len = sa_utf8_decode(q, &cp);
            int cwid = sa_char_width(cp, DESC_SIZE);
            if (n && w + cwid > DESC_W) break;
            w += cwid;
            q += len;
            n++;
        }
        sa_text_draw(fb, SA_W, SA_H, d, n, DESC_TEXT_X, DESC_TEXT_Y + line * 17, DESC_SIZE, 0x000000, NO_EDGE);
        line++;
        d = *q == '|' ? q + 1 : q;
    }
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
}

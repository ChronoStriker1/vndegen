#include "sa_private.h"

enum {
    UNIT_X = 2,
    ITEM_X = 625,
    PANEL_Y = 20,
    ROW_Y = 76,
    ROW_H = 26,
    UNIT_W = 584,
    ITEM_W = 276,
    TAB_Y = 27,
    BAR_Y = 76,
    BAR_TOP_Y = 56,
    BAR_END_Y = 686,
    BADGE_X = 10,
    BADGE_DY = 4,
    NAME_X = 103,
    STAT_X = 262,
    STAT_W = 34,
    EQUIP_X = 404,
    ICON_X = 631,
    ICON_DY = 1,
    ICON = 22,
    ITEM_NAME_X = 660,
    COUNT_RIGHT = 894,
    TEXT_DY = 5,
    TEXT_SIZE = 18,
    NUM_SIZE = 15,
    UNEQUIP_X = 934,
    BACK_X = 1117,
    BTN_Y = 675,
    BTN_W = 164,
    BTN_H = 44,
    ROWS = 23,
    HOVER_BACK = 1,
    HOVER_UNEQUIP = 2,
    HOVER_UNIT = 100,
    HOVER_ITEM = 200,
    I_ID = 0,
    I_NAME = 1,
    I_ICON = 3,
    I_KIND = 5,
};

enum {
    ART_UNITS,
    ART_UNIT_ROWS,
    ART_ITEMS,
    ART_ITEM_ROWS,
    ART_UNEQUIP,
    ART_BACK,
    ART_BADGE,
    ART_BAR,
    ART_BAR_TOP,
    ART_BAR_END,
    ART_TAB0,
    ART_COUNT = ART_TAB0 + 10,
};

static const char *const artNames[ART_COUNT] = {"ite_c00.agf", "ite_c01.agf", "ite_g00.agf", "ite_g01.agf", "ite_j00.agf", "ite_j01.agf", "uts_h98.agf",
                                                "ite_c03.agf", "ite_c04.agf", "ite_c05.agf", "ite_d01.agf", "ite_d02.agf", "ite_d03.agf", "ite_d04.agf",
                                                "ite_d05.agf", "ite_d06.agf", "ite_d07.agf", "ite_l01.agf", "ite_l02.agf", "ite_l03.agf"};

static const int tabX[10] = {24, 86, 148, 210, 272, 334, 396, 648, 710, 772};

#define NO_EDGE 0xffffffffu

void sa_equip_init(Equip *e, Vfs *vfs) {
    memset(e, 0, sizeof *e);
    sa_table_load(&e->items, vfs, "item.obj");
    e->bg = sa_load_image(vfs, "bg009a0.agf");
    for (int i = 0; i < ART_COUNT; i++) e->art[i] = sa_load_image(vfs, artNames[i]);
    e->unit = e->item = e->hover = -1;
}

void sa_equip_free(Equip *e) {
    for (int i = 0; i < ART_COUNT; i++) sa_image_free(e->art[i]);
    for (int i = 0; i < EQUIP_ICONS; i++) sa_image_free(e->icon[i]);
    sa_image_free(e->bg);
    sa_table_free(&e->items);
    memset(e, 0, sizeof *e);
}

static int item_row(const Equip *e, int id) {
    for (int r = 0; r < e->items.rows; r++)
        if (sa_table_int(&e->items, r, I_ID) == id) return r;
    return -1;
}

static int equipped_count(const Party *p, int id) {
    int n = 0;
    for (int s = 0; s < PARTY_SLOTS; s++)
        if (p->unit[s] && p->equip[s] == id) n++;
    return n;
}

static int units(const SA *s, int *slots) {
    int n = 0;
    for (int i = PARTY_FRONT; i < PARTY_SLOTS; i++)
        if (s->party.unit[i]) slots[n++] = i;
    for (int i = 0; i < PARTY_FRONT; i++)
        if (s->party.unit[i]) slots[n++] = i;
    return n;
}

static int equipable(const SA *s, int *rows) {
    int n = 0;
    for (int i = 0; i < s->party.items && n < ROWS; i++) {
        int r = item_row(&s->equip, s->party.itemId[i]);
        if (r >= 0 && strcmp(sa_table_text(&s->equip.items, r, I_KIND), "宝石"))
            rows[n++] = i;
    }
    return n;
}

void sa_equip_open(SA *s) {
    Equip *e = &s->equip;
    e->unit = e->item = e->hover = -1;
    s->mode = MODE_EQUIP;
    s->dirty = true;
}

static bool in(int x, int y, int rx, int ry, int w, int h) { return x >= rx && x < rx + w && y >= ry && y < ry + h; }

static int hit(SA *s, int x, int y) {
    if (in(x, y, BACK_X, BTN_Y, BTN_W, BTN_H)) return HOVER_BACK;
    if (in(x, y, UNEQUIP_X, BTN_Y, BTN_W, BTN_H)) return HOVER_UNEQUIP;
    int slots[PARTY_SLOTS], nu = units(s, slots);
    for (int i = 0; i < nu; i++)
        if (in(x, y, UNIT_X, ROW_Y + i * ROW_H, UNIT_W, ROW_H)) return HOVER_UNIT + i;
    int rows[ROWS], ni = equipable(s, rows);
    for (int i = 0; i < ni; i++)
        if (in(x, y, ITEM_X, ROW_Y + i * ROW_H, ITEM_W, ROW_H)) return HOVER_ITEM + i;
    return -1;
}

static Image *icon_for(SA *s, int row) {
    int n = sa_table_int(&s->equip.items, row, I_ICON);
    if (n < 0 || n >= EQUIP_ICONS) return NULL;
    if (!s->equip.icon[n]) {
        char name[16];
        snprintf(name, sizeof name, "item%02d.agf", n);
        Image *big = sa_load_image(&s->vfs, name);
        s->equip.icon[n] = big ? sa_image_scale(big, ICON, ICON) : NULL;
        sa_image_free(big);
    }
    return s->equip.icon[n];
}

static void text(u32 *fb, const char *t, int x, int y, int size, u32 color) {
    sa_text_draw(fb, SA_W, SA_H, t, -1, x, y, size, color, NO_EDGE);
}

static void number(u32 *fb, int v, int right, int y, u32 color) {
    char n[16];
    snprintf(n, sizeof n, "%d", v);
    sa_text_draw(fb, SA_W, SA_H, n, -1, right - sa_text_width(n, -1, NUM_SIZE), y, NUM_SIZE, color, NO_EDGE);
}

static void draw(SA *s) {
    Equip *e = &s->equip;
    Party *p = &s->party;
    u32 *fb = s->fb;
    sa_blit(fb, e->bg, 0, 0);
    sa_blit(fb, e->art[ART_UNITS], UNIT_X, PANEL_Y);
    sa_blit(fb, e->art[ART_ITEMS], ITEM_X, PANEL_Y);
    for (int i = 0; i < 10; i++) sa_blit_rect(fb, e->art[ART_TAB0 + i], 0, 0, 60, 20, tabX[i], TAB_Y);
    static const int barX[2] = {586, 900};
    for (int i = 0; i < 2; i++) {
        sa_blit(fb, e->art[ART_BAR], barX[i], BAR_Y);
        sa_blit_rect(fb, e->art[ART_BAR_TOP], 0, 10, 20, 10, barX[i], BAR_TOP_Y);
        sa_blit_rect(fb, e->art[ART_BAR_END], 0, 0, 20, 10, barX[i], BAR_END_Y);
    }
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    int slots[PARTY_SLOTS], nu = units(s, slots);
    for (int i = 0; i < nu; i++) {
        int sl = slots[i], y = ROW_Y + i * ROW_H;
        int frame = e->unit == i ? 2 : e->hover == HOVER_UNIT + i ? 3 : 0;
        sa_blit_rect(fb, e->art[ART_UNIT_ROWS], 0, frame * ROW_H, UNIT_W, ROW_H, UNIT_X, y);
        sa_blit(fb, e->art[ART_BADGE], BADGE_X, y + BADGE_DY);
        int r = sa_party_unit_row(p, p->unit[sl]);
        text(fb, sa_table_text(&p->units, r, 2), NAME_X, y + TEXT_DY, TEXT_SIZE, 0x000000);
        const int stats[4] = {p->level[sl], sa_table_int(&p->units, r, 15), sa_table_int(&p->units, r, 31), sa_table_int(&p->units, r, 32)};
        for (int k = 0; k < 4; k++) number(fb, stats[k], STAT_X + (k + 1) * STAT_W - 4, y + TEXT_DY + 1, 0x000000);
        int ir = p->equip[sl] ? item_row(e, p->equip[sl]) : -1;
        if (ir >= 0) text(fb, sa_table_text(&e->items, ir, I_NAME), EQUIP_X, y + TEXT_DY, TEXT_SIZE, 0x000000);
    }
    int rows[ROWS], ni = equipable(s, rows);
    for (int i = 0; i < ni; i++) {
        int idx = rows[i], y = ROW_Y + i * ROW_H, id = p->itemId[idx], total = p->itemCount[idx];
        int free = total - equipped_count(p, id);
        int frame = free <= 0 ? 4 : e->item == i ? 2 : e->hover == HOVER_ITEM + i ? 3 : 0;
        sa_blit_rect(fb, e->art[ART_ITEM_ROWS], 0, frame * ROW_H, ITEM_W, ROW_H, ITEM_X, y);
        int r = item_row(e, id);
        Image *ic = icon_for(s, r);
        if (ic) sa_blit(fb, ic, ICON_X, y + ICON_DY);
        text(fb, sa_table_text(&e->items, r, I_NAME), ITEM_NAME_X, y + TEXT_DY, TEXT_SIZE, 0x000000);
        char c[24];
        snprintf(c, sizeof c, "/ %d", total);
        int w = sa_text_width(c, -1, NUM_SIZE);
        text(fb, c, COUNT_RIGHT - w, y + TEXT_DY + 1, NUM_SIZE, 0x000000);
        number(fb, free, COUNT_RIGHT - w - 2, y + TEXT_DY + 1, free <= 0 ? 0xff0000 : 0x000000);
    }
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    sa_blit_rect(fb, e->art[ART_UNEQUIP], 0, e->hover == HOVER_UNEQUIP ? BTN_H : 0, BTN_W, BTN_H, UNEQUIP_X, BTN_Y);
    sa_blit_rect(fb, e->art[ART_BACK], 0, e->hover == HOVER_BACK ? BTN_H : 0, BTN_W, BTN_H, BACK_X, BTN_Y);
}

static void apply(SA *s) {
    Equip *e = &s->equip;
    Party *p = &s->party;
    if (e->unit < 0 || e->item < 0) return;
    int slots[PARTY_SLOTS], rows[ROWS];
    units(s, slots);
    equipable(s, rows);
    int sl = slots[e->unit], idx = rows[e->item], id = p->itemId[idx];
    if (p->itemCount[idx] - equipped_count(p, id) > 0 || p->equip[sl] == id) p->equip[sl] = id;
    e->unit = e->item = -1;
}

bool sa_equip_frame(SA *s) {
    if (s->mode != MODE_EQUIP) return false;
    Equip *e = &s->equip;
    int h = hit(s, s->mouseX, s->mouseY);
    if (h != e->hover) {
        e->hover = h;
        s->dirty = true;
    }
    if (s->rightClicked) {
        s->rightClicked = false;
        if (e->unit >= 0 || e->item >= 0) e->unit = e->item = -1;
        else s->mode = MODE_TOWN;
        s->dirty = true;
    }
    if (s->clicked) {
        s->clicked = false;
        if (h == HOVER_BACK) s->mode = MODE_TOWN;
        else if (h == HOVER_UNEQUIP) {
            int slots[PARTY_SLOTS];
            units(s, slots);
            if (e->unit >= 0) s->party.equip[slots[e->unit]] = 0;
            e->unit = e->item = -1;
        } else if (h >= HOVER_ITEM) {
            e->item = h - HOVER_ITEM;
            apply(s);
        } else if (h >= HOVER_UNIT) {
            e->unit = h - HOVER_UNIT;
            apply(s);
        }
        s->dirty = true;
    }
    if (s->mode == MODE_EQUIP && s->dirty) {
        draw(s);
        s->serial++;
        s->dirty = false;
    }
    if (s->mode == MODE_TOWN) {
        s->town.hover = -1;
        s->dirty = true;
    }
    return true;
}

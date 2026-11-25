#include "sa_private.h"

enum {
    LIST_X = 241,
    LIST_Y = 21,
    TAB_Y = 27,
    ROW_Y = 74,
    ROW_H = 35,
    ROW_W = 692,
    ROWS = 18,
    ICON_X = 249,
    ICON_DY = 6,
    ICON = 24,
    NAME_X = 282,
    TEXT_DY = 10,
    NUM_DY = 11,
    BUY_RIGHT = 532,
    SELL_RIGHT = 606,
    OWN_RIGHT = 635,
    TRADE_RIGHT = 681,
    BTN_DY = 5,
    BUY_X = 693,
    SELLB_X = 765,
    CANCEL_X = 837,
    BTN_H = 25,
    BAR_X = 933,
    BAR_Y = 63,
    BAR_TOP_Y = 52,
    BAR_END_Y = 687,
    CARD_X = 968,
    CARD_Y = 86,
    STAR_X = 975,
    STAR_Y = 127,
    CARD_NAME_X = 1010,
    CARD_NAME_Y = 131,
    CARD_ICON_X = 977,
    CARD_ICON_Y = 159,
    CARD_RIGHT = 1268,
    CARD_PRICE_Y = 199,
    CARD_SELL_Y = 219,
    CARD_EFFECT_Y = 267,
    CARD_DESC_Y = 372,
    CARD_TEXT_X = 978,
    CARD_TEXT_W = 290,
    GOLD_X = 1077,
    GOLD_Y = 31,
    GOLD_RIGHT = 1254,
    GOLD_TEXT_Y = 35,
    TOTAL_X = 968,
    TOTAL_Y = 530,
    TOTAL_RIGHT = 1237,
    TOTAL_ROW_Y = 561,
    TOTAL_PITCH = 20,
    OK_X = 1048,
    OK_Y = 620,
    BACK_X = 1120,
    BACK_Y = 672,
    BTN_W = 152,
    BIG_H = 44,
    SMALL = 12,
    HOVER_OK = 1,
    HOVER_BACK = 2,
    HOVER_ROW = 100,
    HOVER_BUY = 200,
    HOVER_SELL = 300,
    HOVER_CANCEL = 400,
    I_ID = 0,
    I_NAME = 1,
    I_ICON = 3,
    I_STOCK = 7,
    I_PRICE = 9,
    I_EFFECT_TEXT = 32,
    I_DESC = 33,
    SELL_DIVISOR = 4,
    FIGURE_X = -105,
};

enum {
    ART_LIST,
    ART_ROWS,
    ART_BUY,
    ART_SELL,
    ART_CANCEL,
    ART_BAR,
    ART_BAR_TOP,
    ART_BAR_END,
    ART_CARD,
    ART_STAR,
    ART_GOLD,
    ART_TOTALS,
    ART_OK,
    ART_BACK,
    ART_FIGURE,
    ART_BG,
    ART_TAB0,
    ART_COUNT = ART_TAB0 + 6,
};

static const char *const artNames[ART_COUNT] = {"shp_a00.agf", "shp_a01.agf", "shp_g00.agf", "shp_g01.agf", "shp_g02.agf", "shp_c00.agf",
                                                "shp_c02.agf", "shp_c03.agf", "pit_a00.agf", "pit_a01.agf", "flw_a02.agf", "shp_d00.agf",
                                                "shp_d01.agf", "shp_e00.agf", "ta01a000.agf", "bg009a0.agf", "shp_b01.agf", "shp_b02.agf",
                                                "shp_b03.agf", "shp_b04.agf", "shp_b05.agf", "shp_b06.agf"};

static const int tabX[6] = {263, 342, 421, 500, 579, 658};

#define NO_EDGE 0xffffffffu

void sa_shop_init(Shop *p, Vfs *vfs) {
    memset(p, 0, sizeof *p);
    for (int i = 0; i < ART_COUNT; i++) p->art[i] = sa_load_image(vfs, artNames[i]);
    p->hover = p->focus = -1;
}

void sa_shop_free(Shop *p) {
    for (int i = 0; i < ART_COUNT; i++) sa_image_free(p->art[i]);
    for (int i = 0; i < SHOP_ICONS; i++) {
        sa_image_free(p->icon[i]);
        sa_image_free(p->bigIcon[i]);
    }
    memset(p, 0, sizeof *p);
}

static const Table *items(const SA *s) { return &s->equip.items; }

static bool for_sale(const SA *s, int r) {
    int stock = sa_table_int(items(s), r, I_STOCK);
    return stock == 0 || (stock > 0 && stock < SA_F_VARS && s->vm.f[stock]);
}

static int list(SA *s, int *rows) {
    Shop *p = &s->shop;
    int n = 0;
    for (int r = 0; r < items(s)->rows && n < SHOP_ROWS; r++) {
        int id = sa_table_int(items(s), r, I_ID);
        if (id <= 0 || (!for_sale(s, r) && !sa_party_item_count(&s->party, id))) continue;
        rows[n++] = r;
    }
    if (p->scroll > n - ROWS) p->scroll = n - ROWS > 0 ? n - ROWS : 0;
    return n;
}

static int price(const SA *s, int r) { return sa_table_int(items(s), r, I_PRICE); }

static int sell_price(const SA *s, int r) { return price(s, r) / SELL_DIVISOR; }

static void totals(SA *s, int *buy, int *sell) {
    *buy = *sell = 0;
    for (int r = 0; r < items(s)->rows && r < SHOP_ROWS_MAX; r++) {
        int t = s->shop.trade[r];
        if (t > 0) *buy += t * price(s, r);
        if (t < 0) *sell += -t * sell_price(s, r);
    }
}

static int sellable(const SA *s, int r) {
    int id = sa_table_int(items(s), r, I_ID), equipped = 0;
    for (int i = 0; i < PARTY_SLOTS; i++)
        if (s->party.unit[i] && s->party.equip[i] == id) equipped++;
    return sa_party_item_count(&s->party, id) - equipped;
}

void sa_shop_open(SA *s) {
    Shop *p = &s->shop;
    memset(p->trade, 0, sizeof p->trade);
    p->hover = p->focus = -1;
    p->scroll = 0;
    s->mode = MODE_SHOP;
    s->dirty = true;
}

static bool in(int x, int y, int rx, int ry, int w, int h) { return x >= rx && x < rx + w && y >= ry && y < ry + h; }

static int hit(SA *s, int x, int y) {
    if (in(x, y, OK_X, OK_Y, BTN_W, BIG_H)) return HOVER_OK;
    if (in(x, y, BACK_X, BACK_Y, BTN_W, BIG_H)) return HOVER_BACK;
    int rows[SHOP_ROWS], n = list(s, rows);
    for (int i = 0; i < ROWS && s->shop.scroll + i < n; i++) {
        int y0 = ROW_Y + i * ROW_H, k = s->shop.scroll + i;
        if (in(x, y, BUY_X, y0 + BTN_DY, 72, BTN_H)) return HOVER_BUY + k;
        if (in(x, y, SELLB_X, y0 + BTN_DY, 72, BTN_H)) return HOVER_SELL + k;
        if (in(x, y, CANCEL_X, y0 + BTN_DY, 92, BTN_H)) return HOVER_CANCEL + k;
        if (in(x, y, LIST_X, y0, ROW_W, ROW_H)) return HOVER_ROW + k;
    }
    return -1;
}

static Image *icon(SA *s, int r, bool big) {
    int n = sa_table_int(items(s), r, I_ICON);
    if (n < 0 || n >= SHOP_ICONS) return NULL;
    Image **slot = big ? &s->shop.bigIcon[n] : &s->shop.icon[n];
    if (!*slot) {
        char name[16];
        snprintf(name, sizeof name, "item%02d.agf", n);
        Image *im = sa_load_image(&s->vfs, name);
        *slot = im && !big ? sa_image_scale(im, ICON, ICON) : im;
        if (im && !big) sa_image_free(im);
    }
    return *slot;
}

static void num(u32 *fb, int v, int right, int y, bool dashes) {
    char t[24];
    if (dashes) snprintf(t, sizeof t, "--------");
    else snprintf(t, sizeof t, "%d", v);
    sa_text_draw(fb, SA_W, SA_H, t, -1, right - sa_text_width(t, -1, 14), y, 14, 0x000000, NO_EDGE);
}

static void wrapped(u32 *fb, const char *text, int x, int y) {
    int line = 0;
    const char *p = text;
    while (*p && line < 5) {
        int n = 0, w = 0;
        const char *q = p;
        while (*q && *q != '|') {
            u32 cp;
            int len = sa_utf8_decode(q, &cp);
            int cw = sa_char_width(cp, SMALL);
            if (n && w + cw > CARD_TEXT_W) break;
            w += cw;
            q += len;
            n++;
        }
        sa_text_draw(fb, SA_W, SA_H, p, n, x, y + line * 16, SMALL, 0x000000, NO_EDGE);
        line++;
        p = *q == '|' ? q + 1 : q;
    }
}

static void draw_card(SA *s, u32 *fb, int r) {
    Shop *p = &s->shop;
    sa_blit(fb, p->art[ART_CARD], CARD_X, CARD_Y);
    if (r < 0) return;
    sa_blit_rect(fb, p->art[ART_STAR], 0, clampi(sa_table_int(items(s), r, 34), 0, 3) * 32, 32, 32, STAR_X, STAR_Y - 8);
    sa_font_set_spacing(6);
    sa_text_draw(fb, SA_W, SA_H, sa_table_text(items(s), r, I_NAME), -1, CARD_NAME_X, CARD_NAME_Y, 14, 0x000000, NO_EDGE);
    sa_font_set_spacing(0);
    Image *ic = icon(s, r, true);
    if (ic) sa_blit(fb, ic, CARD_ICON_X, CARD_ICON_Y);
    num(fb, price(s, r), CARD_RIGHT, CARD_PRICE_Y, !for_sale(s, r));
    num(fb, sell_price(s, r), CARD_RIGHT, CARD_SELL_Y, false);
    wrapped(fb, sa_table_text(items(s), r, I_EFFECT_TEXT), CARD_TEXT_X, CARD_EFFECT_Y);
    wrapped(fb, sa_table_text(items(s), r, I_DESC), CARD_TEXT_X, CARD_DESC_Y);
}

static void draw(SA *s) {
    Shop *p = &s->shop;
    u32 *fb = s->fb;
    sa_blit(fb, p->art[ART_BG], 0, 0);
    if (p->art[ART_FIGURE]) sa_blit(fb, p->art[ART_FIGURE], FIGURE_X, SA_H - p->art[ART_FIGURE]->h);
    sa_blit(fb, p->art[ART_LIST], LIST_X, LIST_Y);
    for (int i = 0; i < 6; i++) sa_blit_rect(fb, p->art[ART_TAB0 + i], 0, 0, 76, 21, tabX[i], TAB_Y);
    sa_blit(fb, p->art[ART_BAR], BAR_X, BAR_Y);
    sa_blit_rect(fb, p->art[ART_BAR_TOP], 0, 10, 20, 10, BAR_X, BAR_TOP_Y);
    sa_blit_rect(fb, p->art[ART_BAR_END], 0, 0, 20, 10, BAR_X, BAR_END_Y);
    int rows[SHOP_ROWS], n = list(s, rows);
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    for (int i = 0; i < ROWS && p->scroll + i < n; i++) {
        int k = p->scroll + i, r = rows[k], y = ROW_Y + i * ROW_H, id = sa_table_int(items(s), r, I_ID);
        int frame = p->focus == k ? 2 : 0;
        sa_blit_rect(fb, p->art[ART_ROWS], 0, frame * ROW_H, ROW_W, ROW_H, LIST_X, y);
        Image *ic = icon(s, r, false);
        if (ic) sa_blit(fb, ic, ICON_X, y + ICON_DY);
        sa_text_draw(fb, SA_W, SA_H, sa_table_text(items(s), r, I_NAME), -1, NAME_X, y + TEXT_DY, 16, 0x000000, NO_EDGE);
        num(fb, price(s, r), BUY_RIGHT, y + NUM_DY, !for_sale(s, r));
        num(fb, sell_price(s, r), SELL_RIGHT, y + NUM_DY, false);
        num(fb, sa_party_item_count(&s->party, id), OWN_RIGHT, y + NUM_DY, false);
        num(fb, p->trade[r] < 0 ? -p->trade[r] : p->trade[r], TRADE_RIGHT, y + NUM_DY, false);
        bool canBuy = for_sale(s, r), canSell = sellable(s, r) + (p->trade[r] < 0 ? p->trade[r] : 0) > 0 && p->trade[r] <= 0,
             canCancel = p->trade[r] != 0;
        canBuy = canBuy && p->trade[r] >= 0;
        sa_blit_rect(fb, p->art[ART_BUY], 0, !canBuy ? 2 * BTN_H : p->hover == HOVER_BUY + k ? BTN_H : 0, 72, BTN_H, BUY_X, y + BTN_DY);
        sa_blit_rect(fb, p->art[ART_SELL], 0, !canSell ? 2 * BTN_H : p->hover == HOVER_SELL + k ? BTN_H : 0, 72, BTN_H, SELLB_X, y + BTN_DY);
        sa_blit_rect(fb, p->art[ART_CANCEL], 0, !canCancel ? 2 * BTN_H : p->hover == HOVER_CANCEL + k ? BTN_H : 0, 92, BTN_H, CANCEL_X,
                     y + BTN_DY);
    }
    int show = p->hover >= HOVER_ROW && p->hover < HOVER_ROW + SHOP_ROWS ? p->hover - HOVER_ROW
               : p->hover >= HOVER_BUY && p->hover < HOVER_BUY + SHOP_ROWS ? p->hover - HOVER_BUY
               : p->hover >= HOVER_SELL && p->hover < HOVER_SELL + SHOP_ROWS ? p->hover - HOVER_SELL
               : p->hover >= HOVER_CANCEL ? p->hover - HOVER_CANCEL
                                          : p->focus;
    draw_card(s, fb, show >= 0 && show < n ? rows[show] : -1);
    sa_blit(fb, p->art[ART_GOLD], GOLD_X, GOLD_Y);
    char g[24];
    snprintf(g, sizeof g, "%d", s->vm.c[C_MONEY]);
    sa_text_draw(fb, SA_W, SA_H, g, -1, GOLD_RIGHT - sa_text_width(g, -1, 14), GOLD_TEXT_Y, 14, 0x000000, NO_EDGE);
    sa_blit(fb, p->art[ART_TOTALS], TOTAL_X, TOTAL_Y);
    int buy, sell;
    totals(s, &buy, &sell);
    const int t[3] = {buy, sell, sell - buy};
    for (int i = 0; i < 3; i++) num(fb, t[i], TOTAL_RIGHT, TOTAL_ROW_Y + i * TOTAL_PITCH, false);
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    sa_town_draw_status_goal(&s->town, &s->vm, fb);
    sa_blit_rect(fb, p->art[ART_OK], 0, p->hover == HOVER_OK ? BIG_H : 0, BTN_W, BIG_H, OK_X, OK_Y);
    sa_blit_rect(fb, p->art[ART_BACK], 0, p->hover == HOVER_BACK ? BIG_H : 0, BTN_W, BIG_H, BACK_X, BACK_Y);
}

static void settle(SA *s) {
    int buy, sell;
    totals(s, &buy, &sell);
    if (s->vm.c[C_MONEY] + sell - buy < 0) return;
    s->vm.c[C_MONEY] += sell - buy;
    for (int r = 0; r < items(s)->rows && r < SHOP_ROWS_MAX; r++) {
        int t = s->shop.trade[r];
        if (t) sa_party_add_item(&s->party, sa_table_int(items(s), r, I_ID), t);
        if (t > 0) sa_records_item(s, sa_table_int(items(s), r, I_ID));
        s->shop.trade[r] = 0;
    }
    sa_log(s, "shop: bought %d, sold %d", buy, sell);
}

static void press(SA *s, int h) {
    Shop *p = &s->shop;
    int rows[SHOP_ROWS], n = list(s, rows);
    if (h == HOVER_OK) settle(s);
    else if (h == HOVER_BACK) {
        memset(p->trade, 0, sizeof p->trade);
        s->mode = MODE_TOWN;
    } else if (h >= HOVER_CANCEL && h - HOVER_CANCEL < n) p->trade[rows[h - HOVER_CANCEL]] = 0;
    else if (h >= HOVER_SELL && h - HOVER_SELL < n) {
        int r = rows[h - HOVER_SELL];
        if (p->trade[r] <= 0 && sellable(s, r) + p->trade[r] > 0) p->trade[r]--;
    } else if (h >= HOVER_BUY && h - HOVER_BUY < n) {
        int r = rows[h - HOVER_BUY], buy, sell;
        totals(s, &buy, &sell);
        if (p->trade[r] >= 0 && for_sale(s, r) && s->vm.c[C_MONEY] + sell - buy - price(s, r) >= 0) p->trade[r]++;
    } else if (h >= HOVER_ROW && h - HOVER_ROW < n)
        p->focus = h - HOVER_ROW;
}

bool sa_shop_frame(SA *s) {
    if (s->mode != MODE_SHOP) return false;
    Shop *p = &s->shop;
    int h = hit(s, s->mouseX, s->mouseY);
    if (h != p->hover) {
        p->hover = h;
        s->dirty = true;
    }
    if (s->rightClicked) {
        s->rightClicked = false;
        memset(p->trade, 0, sizeof p->trade);
        s->mode = MODE_TOWN;
        s->dirty = true;
    }
    if (s->clicked) {
        s->clicked = false;
        press(s, h);
        s->dirty = true;
    }
    if (s->mode == MODE_SHOP && s->dirty) {
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

void sa_shop_wheel(SA *s, int notches) {
    if (s->mode != MODE_SHOP) return;
    s->shop.scroll -= notches;
    if (s->shop.scroll < 0) s->shop.scroll = 0;
    s->dirty = true;
}

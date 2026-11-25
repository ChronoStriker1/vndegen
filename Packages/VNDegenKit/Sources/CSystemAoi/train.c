#include "sa_private.h"

enum {
    LIST_X = 10,
    LIST_Y = 16,
    ROW_X = 10,
    ROW_Y = 45,
    ROW_H = 30,
    ROW_W = 264,
    LV_RIGHT = 43,
    NAME_X = 56,
    SP_RIGHT = 266,
    TEXT_DY = 7,
    PANEL_X = 304,
    PANEL_Y = 16,
    TRAIN_X = 304,
    TRAIN_Y = 45,
    TRAIN_H = 82,
    TRAIN_PITCH = 83,
    TRAIN_W = 664,
    TRAIN_ROWS = 8,
    TITLE_X = 335,
    TITLE_DY = 8,
    DESC_X = 345,
    DESC_DY = 30,
    COST_RIGHT = 950,
    MP_RIGHT = 852,
    CARD_X = 997,
    CARD_Y = 21,
    BACK_X = 1120,
    BACK_Y = 670,
    BTN_W = 152,
    BTN_H = 44,
    CONFIRM_X = 290,
    CONFIRM_Y = 260,
    CONFIRM_ROW_Y = 308,
    CONFIRM_SP_RIGHT = 935,
    CONFIRM_SP_Y = 425,
    OK_X = 632,
    CANCEL_X = 792,
    CONFIRM_BTN_Y = 447,
    HOVER_BACK = 1,
    HOVER_OK = 2,
    HOVER_CANCEL = 3,
    HOVER_MEMBER = 100,
    HOVER_TRAIN = 200,
    CW = 10,
    CH = 13,
};

enum { ART_LIST, ART_ROWS, ART_PANEL, ART_TRAIN, ART_BACK, ART_CONFIRM, ART_OK, ART_CANCEL, ART_BAR, ART_BAR_TOP, ART_BAR_END, ART_BG, ART_DIGITS, ART_TAB0, ART_COUNT = ART_TAB0 + 3 };

static const char *const artNames[ART_COUNT] = {"grw_a00.agf", "grw_a01.agf", "grw_d00.agf", "grw_d01.agf", "grw_h00.agf",
                                                "grw_j00.agf", "grw_g01.agf", "grw_g02.agf", "grw_c01.agf", "grw_c02.agf",
                                                "grw_c03.agf", "bg009a0.agf", "grw_i00.agf", "grw_b01.agf", "grw_b02.agf", "grw_b03.agf"};

static const char *const trainNames[10] = {"体力ＵＰ", "気力ＵＰ", "物理攻撃力ＵＰ", "魔法攻撃力ＵＰ", "物理防御力ＵＰ", "魔法防御力ＵＰ", "速度ＵＰ", "回避ＵＰ", "統治ＵＰ", "統率ＵＰ"};
static const char *const statNames[10] = {"体力", "気力", "物理攻撃力", "魔法攻撃力", "物理防御力", "魔法防御力", "速度", "回避", "統治", "統率"};

#define NO_EDGE 0xffffffffu

void sa_train_init(Train *t, Vfs *vfs) {
    memset(t, 0, sizeof *t);
    sa_table_load(&t->costs, vfs, "StUp.obj");
    for (int i = 0; i < ART_COUNT; i++) t->art[i] = sa_load_image(vfs, artNames[i]);
    t->member = t->pick = t->hover = -1;
}

void sa_train_free(Train *t) {
    for (int i = 0; i < ART_COUNT; i++) sa_image_free(t->art[i]);
    sa_table_free(&t->costs);
    memset(t, 0, sizeof *t);
}

static int cost(const Train *t, int k) { return sa_table_int(&t->costs, 0, 2 + k * 2); }

static int gain(const Train *t, int k) { return sa_table_int(&t->costs, 0, 3 + k * 2); }

static int members(SA *s, Member *out) { return sa_party_member_list(&s->party, out); }

void sa_train_open(SA *s) {
    Train *t = &s->train;
    t->member = t->pick = t->hover = -1;
    s->card.showDetail = false;
    s->mode = MODE_TRAIN;
    s->dirty = true;
}

static bool in(int x, int y, int rx, int ry, int w, int h) { return x >= rx && x < rx + w && y >= ry && y < ry + h; }

static int hit(SA *s, int x, int y) {
    Train *t = &s->train;
    if (t->pick >= 0) {
        if (in(x, y, OK_X, CONFIRM_BTN_Y, BTN_W, BTN_H)) return HOVER_OK;
        if (in(x, y, CANCEL_X, CONFIRM_BTN_Y, BTN_W, BTN_H)) return HOVER_CANCEL;
        return -1;
    }
    if (in(x, y, BACK_X, BACK_Y, BTN_W, BTN_H)) return HOVER_BACK;
    Member ms[PARTY_SLOTS + PARTY_BENCH];
    int n = members(s, ms);
    for (int i = 0; i < n; i++)
        if (in(x, y, ROW_X, ROW_Y + i * ROW_H, ROW_W, ROW_H)) return HOVER_MEMBER + i;
    if (t->member >= 0)
        for (int k = 0; k < TRAIN_ROWS; k++)
            if (in(x, y, TRAIN_X, TRAIN_Y + k * TRAIN_PITCH, TRAIN_W, TRAIN_H)) return HOVER_TRAIN + k;
    return -1;
}

static void num(const Train *t, u32 *fb, int v, int right, int y, int row) {
    char n[16];
    snprintf(n, sizeof n, "%d", v);
    sa_draw_number(fb, t->art[ART_DIGITS], n, right, y, row, CW, CH);
}

static void draw_row(SA *s, u32 *fb, int k, int y, int frame, const Member *m, int x) {
    Train *t = &s->train;
    int dx = x - TRAIN_X;
    sa_blit_rect(fb, t->art[ART_TRAIN], 0, frame * TRAIN_H, TRAIN_W, TRAIN_H, x, y);
    sa_text_draw(fb, SA_W, SA_H, trainNames[k], -1, TITLE_X + dx, y + TITLE_DY, 16, 0x000000, NO_EDGE);
    char d[96];
    snprintf(d, sizeof d, "ステータスの%sを%d上昇させます。", statNames[k], gain(t, k));
    sa_text_draw(fb, SA_W, SA_H, d, -1, DESC_X + dx, y + DESC_DY, 14, 0x000000, NO_EDGE);
    sa_draw_number(fb, t->art[ART_DIGITS], "---", MP_RIGHT + dx, y + TITLE_DY + 2, 0, CW, CH);
    num(t, fb, cost(t, k), COST_RIGHT + dx, y + TITLE_DY + 2, cost(t, k) > *m->sp ? 1 : 0);
}

static void draw(SA *s) {
    Train *t = &s->train;
    u32 *fb = s->fb;
    sa_blit(fb, t->art[ART_BG], 0, 0);
    sa_blit(fb, t->art[ART_LIST], LIST_X, LIST_Y);
    static const int tabX[3] = {31, 110, 189};
    for (int i = 0; i < 3; i++) sa_blit_rect(fb, t->art[ART_TAB0 + i], 0, 0, 76, 21, tabX[i], 21);
    sa_blit(fb, t->art[ART_BAR], 274, 44);
    sa_blit_rect(fb, t->art[ART_BAR_TOP], 0, 0, 20, 20, 274, 24);
    sa_blit_rect(fb, t->art[ART_BAR_END], 0, 0, 20, 20, 274, 689);
    sa_blit(fb, t->art[ART_PANEL], PANEL_X, PANEL_Y);
    sa_blit(fb, t->art[ART_BAR], 966, 44);
    sa_blit_rect(fb, t->art[ART_BAR_TOP], 0, 0, 20, 20, 966, 24);
    sa_blit_rect(fb, t->art[ART_BAR_END], 0, 0, 20, 20, 966, 689);
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    Member ms[PARTY_SLOTS + PARTY_BENCH];
    int n = members(s, ms);
    for (int i = 0; i < n; i++) {
        int y = ROW_Y + i * ROW_H;
        int frame = t->member == i ? 2 : t->hover == HOVER_MEMBER + i ? 1 : 0;
        sa_blit_rect(fb, t->art[ART_ROWS], 0, frame * ROW_H, ROW_W, ROW_H, ROW_X, y);
        num(t, fb, *ms[i].level, LV_RIGHT, y + TEXT_DY + 1, 0);
        int r = sa_party_unit_row(&s->party, ms[i].unit);
        sa_text_draw(fb, SA_W, SA_H, sa_table_text(&s->party.units, r, 2), -1, NAME_X, y + TEXT_DY - 1, 16, 0x000000, NO_EDGE);
        num(t, fb, *ms[i].sp, SP_RIGHT, y + TEXT_DY + 1, 2);
    }
    const Member *m = t->member >= 0 && t->member < n ? &ms[t->member] : NULL;
    if (m)
        for (int k = 0; k < TRAIN_ROWS; k++) {
            bool can = cost(t, k) <= *m->sp;
            draw_row(s, fb, k, TRAIN_Y + k * TRAIN_PITCH, !can ? 1 : t->hover == HOVER_TRAIN + k ? 3 : 0, m, TRAIN_X);
        }
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    int shown = t->member >= 0 ? t->member : t->hover >= HOVER_MEMBER && t->hover < HOVER_TRAIN ? t->hover - HOVER_MEMBER : -1;
    sa_card_draw(s, fb, shown >= 0 && shown < n ? &ms[shown] : NULL, CARD_X, CARD_Y);
    sa_blit_rect(fb, t->art[ART_BACK], 0, t->hover == HOVER_BACK ? BTN_H : 0, BTN_W, BTN_H, BACK_X, BACK_Y);
    if (t->pick < 0 || !m) return;
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u | (fb[i] >> 1 & 0x7f7f7fu);
    sa_blit(fb, t->art[ART_CONFIRM], CONFIRM_X, CONFIRM_Y);
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    draw_row(s, fb, t->pick, CONFIRM_ROW_Y, 0, m, CONFIRM_X);
    num(t, fb, *m->sp, CONFIRM_SP_RIGHT, CONFIRM_SP_Y, 0);
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    sa_blit_rect(fb, t->art[ART_OK], 0, t->hover == HOVER_OK ? BTN_H : 0, BTN_W, BTN_H, OK_X, CONFIRM_BTN_Y);
    sa_blit_rect(fb, t->art[ART_CANCEL], 0, t->hover == HOVER_CANCEL ? BTN_H : 0, BTN_W, BTN_H, CANCEL_X, CONFIRM_BTN_Y);
}

static void apply(SA *s) {
    Train *t = &s->train;
    Member ms[PARTY_SLOTS + PARTY_BENCH];
    int n = members(s, ms), k = t->pick;
    t->pick = -1;
    if (t->member < 0 || t->member >= n) return;
    Member *m = &ms[t->member];
    if (cost(t, k) > *m->sp) return;
    *m->sp -= cost(t, k);
    m->trained[k] += gain(t, k);
    if (k == 0) *m->hp += gain(t, k);
    if (k == 1) *m->mp += gain(t, k);
    sa_log(s, "train: unit %d %s +%d", m->unit, trainNames[k], gain(t, k));
}

bool sa_train_frame(SA *s) {
    if (s->mode != MODE_TRAIN) return false;
    Train *t = &s->train;
    if (t->pick < 0) sa_card_input(s);
    int h = hit(s, s->mouseX, s->mouseY);
    if (h != t->hover) {
        t->hover = h;
        s->dirty = true;
    }
    if (s->rightClicked) {
        s->rightClicked = false;
        if (t->pick >= 0) t->pick = -1;
        else s->mode = MODE_TOWN;
        s->dirty = true;
    }
    if (s->clicked) {
        s->clicked = false;
        if (h == HOVER_BACK) s->mode = MODE_TOWN;
        else if (h == HOVER_OK) apply(s);
        else if (h == HOVER_CANCEL) t->pick = -1;
        else if (h >= HOVER_TRAIN) t->pick = h - HOVER_TRAIN;
        else if (h >= HOVER_MEMBER) t->member = h - HOVER_MEMBER;
        s->dirty = true;
    }
    if (s->mode == MODE_TRAIN && s->dirty) {
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

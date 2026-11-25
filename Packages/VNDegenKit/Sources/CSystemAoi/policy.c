#include "sa_private.h"

enum {
    LIST_X = 176,
    WIN_Y = 84,
    ROW_Y = 123,
    ROW_H = 40,
    LIST_W = 352,
    BACK_X = 366,
    BACK_Y = 254,
    BTN_W = 152,
    BTN_H = 44,
    ACT_X = 531,
    ACT_W = 400,
    DESC_X = 531,
    DESC_Y = 316,
    DESC_TEXT_X = 547,
    DESC_TEXT_Y = 347,
    TEXT_SIZE = 16,
    TEXT_PITCH = 18,
    TEXT_WIDTH = 368,
    CONFIRM_X = 362,
    CONFIRM_Y = 185,
    CONFIRM_ROW_Y = 215,
    CONFIRM_TEXT_X = 378,
    CONFIRM_TEXT_Y = 266,
    CONFIRM_COST_Y = 400,
    RUN_X = 397,
    CANCEL_X = 577,
    CONFIRM_BTN_Y = 435,
    HOVER_BACK = 100,
    HOVER_CATEGORY = 200,
    HOVER_ACTION = 300,
    HOVER_RUN = 400,
    HOVER_CANCEL = 401,
    E_NAME = 1,
    E_CATEGORY = 2,
    E_PICTURE = 3,
    E_FLAG = 4,
    E_FACILITY = 6,
    E_RESIDENTS_MIN = 8,
    E_EVENT = 14,
    E_RESIDENTS = 15,
    E_FOOD = 16,
    E_MONEY = 17,
    E_RESOURCES = 18,
    E_EXP = 20,
    E_TEXT = 21,
    E_EFFECT = 22,
    F_EVENTS = 41,
    F_EXCHANGE = 42,
    C_EVENT = 492,
    POLICY_HOOK = 31,
    POLICY_ENTRY = 2,
};

enum { EXCHANGE_DESC_DY = 121, TRADES = 6, CAT_EXCHANGE = 2, TRADE_GIVE = 1000, NUM_SIZE = 20, NUM_SPACING = 6, GIVE_RIGHT = 178, GET_RIGHT = 378, NUM_DY = 9 };

enum { ART_LIST, ART_BACK, ART_DESC, ART_CONFIRM, ART_RUN, ART_CANCEL, ART_COSTS, ART_CAT0, ART_HEAD0 = ART_CAT0 + POLICY_CATEGORIES, ART_TRADE0 = ART_HEAD0 + POLICY_CATEGORIES, ART_COUNT = ART_TRADE0 + TRADES };

static const struct {
    int from, to, get;
} trades[TRADES] = {{1, 2, 300}, {1, 0, 300}, {2, 1, 350}, {2, 0, 350}, {0, 1, 200}, {0, 2, 200}};

static const char *const tradeText = "物々交換です。";

static const char *const artNames[ART_COUNT] = {"dom_a00.agf", "dom_a04.agf", "dom_a05.agf", "dom_a06.agf", "dom_a07.agf", "dom_a08.agf", "dom_a10.agf",
                                                "dom_a01.agf", "dom_a02.agf", "dom_a03.agf", "dom_b00.agf", "dom_c00.agf", "dom_d00.agf",
                                                "dom_dz00.agf", "dom_dz01.agf", "dom_dz02.agf", "dom_dz03.agf", "dom_dz04.agf", "dom_dz05.agf"};

#define NO_EDGE 0xffffffffu

void sa_policy_init(Policy *p, Vfs *vfs) {
    memset(p, 0, sizeof *p);
    sa_table_load(&p->events, vfs, "DomeEvnt.obj");
    p->bg = sa_load_image(vfs, "bg010a0.agf");
    for (int i = 0; i < ART_COUNT; i++) p->art[i] = sa_load_image(vfs, artNames[i]);
    p->pictures = sa_alloc(sizeof(Image *) * (size_t)(p->events.rows ? p->events.rows : 1));
    p->hover = -1;
    p->pick = -1;
}

void sa_policy_free(Policy *p) {
    for (int i = 0; i < ART_COUNT; i++) sa_image_free(p->art[i]);
    for (int i = 0; i < p->events.rows; i++) sa_image_free(p->pictures[i]);
    free(p->pictures);
    sa_image_free(p->bg);
    sa_table_free(&p->events);
    memset(p, 0, sizeof *p);
}

static bool category_open(const SA *s, int c) {
    if (c == 0) return true;
    return s->vm.f[c == 1 ? F_EVENTS : F_EXCHANGE] != 0;
}

static bool facility_built(const SA *s, const char *name) {
    if (!*name || !strcmp(name, "*")) return true;
    for (int c = 0; c < TOWN_CELLS; c++) {
        int r = s->town.cell[c].type ? sa_town_type_row(&s->town, s->town.cell[c].type) : -1;
        if (r >= 0 && !strcmp(sa_table_text(&s->town.build, r, 1), name)) return true;
    }
    return false;
}

static bool event_open(const SA *s, int r) {
    const Table *t = &s->policy.events;
    int flag = sa_table_int(t, r, E_FLAG), min = sa_table_int(t, r, E_RESIDENTS_MIN);
    if (flag > 0 && !s->vm.f[flag]) return false;
    if (min > 0 && s->vm.c[C_RESIDENTS] < min) return false;
    return facility_built(s, sa_table_text(t, r, E_FACILITY));
}

static int list_events(const SA *s, int *rows) {
    int n = 0;
    if (s->policy.category == CAT_EXCHANGE) {
        for (int i = 0; i < TRADES; i++) rows[n++] = -2 - i;
        return n;
    }
    for (int r = 0; r < s->policy.events.rows && n < POLICY_ROWS; r++)
        if (sa_table_int(&s->policy.events, r, E_CATEGORY) == s->policy.category && event_open(s, r)) rows[n++] = r;
    return n;
}

static int list_categories(const SA *s, int *cats) {
    int n = 0;
    for (int c = 0; c < POLICY_CATEGORIES; c++)
        if (category_open(s, c)) cats[n++] = c;
    return n;
}

static bool affordable(const SA *s, int r) {
    if (r < 0) return s->vm.c[C_FOOD + trades[-2 - r].from] >= TRADE_GIVE;
    const Table *t = &s->policy.events;
    return s->vm.c[C_FOOD] + sa_table_int(t, r, E_FOOD) >= 0 && s->vm.c[C_MONEY] + sa_table_int(t, r, E_MONEY) >= 0 &&
           s->vm.c[C_RESOURCES] + sa_table_int(t, r, E_RESOURCES) >= 0;
}

void sa_policy_open(SA *s) {
    Policy *p = &s->policy;
    p->category = 0;
    p->hover = -1;
    p->pick = -1;
    s->mode = MODE_POLICY;
    s->dirty = true;
}

static Image *picture(SA *s, int r) {
    Policy *p = &s->policy;
    if (!p->pictures[r]) p->pictures[r] = sa_load_image(&s->vfs, sa_table_text(&p->events, r, E_PICTURE));
    return p->pictures[r];
}

static bool in(int x, int y, int rx, int ry, int w, int h) { return x >= rx && x < rx + w && y >= ry && y < ry + h; }

static int hit(SA *s, int x, int y) {
    Policy *p = &s->policy;
    if (p->pick != -1) {
        if (in(x, y, RUN_X, CONFIRM_BTN_Y, BTN_W, BTN_H)) return HOVER_RUN;
        if (in(x, y, CANCEL_X, CONFIRM_BTN_Y, BTN_W, BTN_H)) return HOVER_CANCEL;
        return -1;
    }
    if (in(x, y, BACK_X, BACK_Y, BTN_W, BTN_H)) return HOVER_BACK;
    int cats[POLICY_CATEGORIES], nc = list_categories(s, cats);
    for (int i = 0; i < nc; i++)
        if (in(x, y, LIST_X, ROW_Y + i * ROW_H, LIST_W, ROW_H)) return HOVER_CATEGORY + cats[i];
    int rows[POLICY_ROWS], n = list_events(s, rows);
    for (int i = 0; i < n; i++)
        if (in(x, y, ACT_X, ROW_Y + i * ROW_H, ACT_W, ROW_H)) return HOVER_ACTION + i;
    return -1;
}

static void number(u32 *fb, int v, int right, int y, u32 color) {
    char t[16];
    snprintf(t, sizeof t, "%d", v);
    sa_font_set_spacing(NUM_SPACING);
    sa_text_draw(fb, SA_W, SA_H, t, -1, right - sa_text_width(t, -1, NUM_SIZE) + NUM_SPACING, y, NUM_SIZE, color, NO_EDGE);
    sa_font_set_spacing(1);
}

static void draw_row(SA *s, u32 *fb, int r, int frame, int x, int y) {
    if (r >= 0) {
        Image *im = picture(s, r);
        if (im) sa_blit_rect(fb, im, 0, frame * ROW_H, im->w, ROW_H, x, y);
        return;
    }
    int i = -2 - r;
    sa_blit_rect(fb, s->policy.art[ART_TRADE0 + i], 0, frame * ROW_H, ACT_W, ROW_H, x, y);
    number(fb, TRADE_GIVE, x + GIVE_RIGHT, y + NUM_DY, affordable(s, r) ? 0x321a02 : 0xff0000);
    number(fb, trades[i].get, x + GET_RIGHT, y + NUM_DY, 0x321a02);
}

static void text_block(u32 *fb, const Table *t, int r, int x, int y) {
    if (r < 0) {
        sa_font_set_spacing(0);
        sa_font_set_plain(true);
        sa_text_draw(fb, SA_W, SA_H, tradeText, -1, x, y, TEXT_SIZE, 0x000000, NO_EDGE);
        sa_font_set_plain(false);
        sa_font_set_spacing(1);
        return;
    }
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    int line = 0;
    for (int col = E_TEXT; col <= E_EFFECT; col++) {
        const char *p = sa_table_text(t, r, col);
        while (*p) {
            int n = 0, w = 0;
            const char *q = p;
            while (*q && *q != '|') {
                u32 cp;
                int len = sa_utf8_decode(q, &cp);
                int cw = sa_char_width(cp, TEXT_SIZE);
                if (n && w + cw > TEXT_WIDTH) break;
                w += cw;
                q += len;
                n++;
            }
            sa_text_draw(fb, SA_W, SA_H, p, n, x, y + line * TEXT_PITCH, TEXT_SIZE, 0x000000, NO_EDGE);
            line++;
            p = *q == '|' ? q + 1 : q;
        }
        line++;
    }
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
}

static void draw(SA *s) {
    Policy *p = &s->policy;
    u32 *fb = s->fb;
    sa_blit(fb, p->bg, 0, 0);
    sa_town_draw_status(&s->town, &s->vm, fb);
    sa_blit(fb, p->art[ART_LIST], LIST_X, WIN_Y);
    int cats[POLICY_CATEGORIES], nc = list_categories(s, cats);
    for (int i = 0; i < nc; i++) {
        int frame = cats[i] == p->category ? 2 : p->hover == HOVER_CATEGORY + cats[i] ? 1 : 0;
        sa_blit_rect(fb, p->art[ART_CAT0 + cats[i]], 0, frame * ROW_H, LIST_W, ROW_H, LIST_X, ROW_Y + i * ROW_H);
    }
    sa_blit_rect(fb, p->art[ART_BACK], 0, p->hover == HOVER_BACK ? BTN_H : 0, BTN_W, BTN_H, BACK_X, BACK_Y);
    sa_blit(fb, p->art[ART_HEAD0 + p->category], ACT_X, WIN_Y);
    int rows[POLICY_ROWS], n = list_events(s, rows);
    for (int i = 0; i < n; i++)
        draw_row(s, fb, rows[i], !affordable(s, rows[i]) && rows[i] >= 0 ? 2 : p->hover == HOVER_ACTION + i ? 1 : 0, ACT_X, ROW_Y + i * ROW_H);
    int dy = p->category == CAT_EXCHANGE ? EXCHANGE_DESC_DY : 0;
    sa_blit(fb, p->art[ART_DESC], DESC_X, DESC_Y + dy);
    if (p->pick == -1 && p->hover >= HOVER_ACTION && p->hover < HOVER_ACTION + n)
        text_block(fb, &p->events, rows[p->hover - HOVER_ACTION], DESC_TEXT_X, DESC_TEXT_Y + dy);
    if (p->pick == -1) return;
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u | (fb[i] >> 1 & 0x7f7f7fu);
    sa_blit(fb, p->art[ART_CONFIRM], CONFIRM_X, CONFIRM_Y);
    draw_row(s, fb, p->pick, 0, CONFIRM_X, CONFIRM_ROW_Y);
    text_block(fb, &p->events, p->pick, CONFIRM_TEXT_X, CONFIRM_TEXT_Y);
    const Table *t = &p->events;
    int food = p->pick >= 0 ? sa_table_int(t, p->pick, E_FOOD) : 0, money = p->pick >= 0 ? sa_table_int(t, p->pick, E_MONEY) : 0,
        res = p->pick >= 0 ? sa_table_int(t, p->pick, E_RESOURCES) : 0;
    if (food || money || res) {
        sa_blit(fb, p->art[ART_COSTS], CONFIRM_X, CONFIRM_COST_Y);
        static const int right[3] = {114, 246, 378};
        const int value[3] = {food, money, res};
        for (int i = 0; i < 3; i++) {
            char v[16];
            snprintf(v, sizeof v, "%d", value[i]);
            sa_text_draw(fb, SA_W, SA_H, v, -1, CONFIRM_X + right[i] - sa_text_width(v, -1, 14), CONFIRM_COST_Y + 6, 14, 0x000000, NO_EDGE);
        }
    }
    bool can = affordable(s, p->pick);
    sa_blit_rect(fb, p->art[ART_RUN], 0, !can ? 2 * BTN_H : p->hover == HOVER_RUN ? BTN_H : 0, BTN_W, BTN_H, RUN_X, CONFIRM_BTN_Y);
    sa_blit_rect(fb, p->art[ART_CANCEL], 0, p->hover == HOVER_CANCEL ? BTN_H : 0, BTN_W, BTN_H, CANCEL_X, CONFIRM_BTN_Y);
}

static void trade(SA *s, int i) {
    VM *vm = &s->vm;
    int before[4] = {vm->c[C_RESIDENTS], vm->c[C_FOOD], vm->c[C_MONEY], vm->c[C_RESOURCES]};
    vm->c[C_FOOD + trades[i].from] -= TRADE_GIVE;
    vm->c[C_FOOD + trades[i].to] += trades[i].get;
    int after[4] = {vm->c[C_RESIDENTS], vm->c[C_FOOD], vm->c[C_MONEY], vm->c[C_RESOURCES]};
    sa_log(s, "trade %d", i);
    s->mode = MODE_TOWN;
    s->town.mapOnly = true;
    sa_dialog_open_turn(&s->dialog, before, after, "交換しました。");
    s->endTurnAfterDialog = true;
}

static void run(SA *s) {
    Policy *p = &s->policy;
    const Table *t = &p->events;
    int r = p->pick;
    p->pick = -1;
    if (r <= -2) {
        trade(s, -2 - r);
        return;
    }
    s->vm.c[C_RESIDENTS] += sa_table_int(t, r, E_RESIDENTS);
    s->vm.c[C_FOOD] += sa_table_int(t, r, E_FOOD);
    s->vm.c[C_MONEY] += sa_table_int(t, r, E_MONEY);
    s->vm.c[C_RESOURCES] += sa_table_int(t, r, E_RESOURCES);
    if (sa_table_int(t, r, E_EXP) > 0) sa_town_grow_all(&s->town, &s->vm, sa_table_int(t, r, E_EXP));
    s->vm.c[C_EVENT] = sa_table_int(t, r, E_EVENT);
    sa_log(s, "policy: %s", sa_table_text(t, r, E_NAME));
    const char *box = s->vm.boxSlot[POLICY_HOOK];
    s->afterScript = TOWN_POLICY_DONE;
    sa_reset_scene(s);
    if (!box || !sa_start_script(s, box, POLICY_ENTRY)) {
        s->afterScript = 0;
        s->mode = MODE_TOWN;
        sa_end_turn(s);
    }
}

bool sa_policy_frame(SA *s) {
    if (s->mode != MODE_POLICY) return false;
    Policy *p = &s->policy;
    int h = hit(s, s->mouseX, s->mouseY);
    if (h != p->hover) {
        p->hover = h;
        s->dirty = true;
    }
    if (s->rightClicked) {
        s->rightClicked = false;
        if (p->pick != -1) p->pick = -1;
        else s->mode = MODE_TOWN;
        s->dirty = true;
    }
    if (s->clicked) {
        s->clicked = false;
        int rows[POLICY_ROWS], n = list_events(s, rows);
        if (h == HOVER_BACK) s->mode = MODE_TOWN;
        else if (h == HOVER_CANCEL) p->pick = -1;
        else if (h == HOVER_RUN && affordable(s, p->pick)) run(s);
        else if (h >= HOVER_ACTION && h < HOVER_ACTION + n) p->pick = rows[h - HOVER_ACTION];
        else if (h >= HOVER_CATEGORY && h < HOVER_ACTION) p->category = h - HOVER_CATEGORY;
        s->dirty = true;
    }
    if (s->mode == MODE_POLICY && s->dirty) {
        draw(s);
        s->serial++;
        s->dirty = false;
    }
    if (s->mode == MODE_TOWN) {
        s->town.hover = -1;
        s->dirty = true;
    }
    return s->mode == MODE_POLICY || s->mode == MODE_TOWN;
}

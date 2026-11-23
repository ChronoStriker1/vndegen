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
    LVUP_GROUP = 5,
};

enum { ART_LIST, ART_BACK, ART_DESC, ART_CONFIRM, ART_RUN, ART_CANCEL, ART_COSTS, ART_CAT0, ART_HEAD0 = ART_CAT0 + POLICY_CATEGORIES, ART_COUNT = ART_HEAD0 + POLICY_CATEGORIES };

static const char *const artNames[ART_COUNT] = {"dom_a00.agf", "dom_a04.agf", "dom_a05.agf", "dom_a06.agf", "dom_a07.agf", "dom_a08.agf", "dom_a10.agf",
                                                "dom_a01.agf", "dom_a02.agf", "dom_a03.agf", "dom_b00.agf", "dom_c00.agf", "dom_d00.agf"};

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
    if (p->pick >= 0) {
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

static void text_block(u32 *fb, const Table *t, int r, int x, int y) {
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
    for (int i = 0; i < n; i++) {
        Image *im = picture(s, rows[i]);
        if (!im) continue;
        int frame = !affordable(s, rows[i]) ? 2 : p->hover == HOVER_ACTION + i ? 1 : 0;
        sa_blit_rect(fb, im, 0, frame * ROW_H, im->w, ROW_H, ACT_X, ROW_Y + i * ROW_H);
    }
    sa_blit(fb, p->art[ART_DESC], DESC_X, DESC_Y);
    if (p->pick < 0 && p->hover >= HOVER_ACTION && p->hover < HOVER_ACTION + n)
        text_block(fb, &p->events, rows[p->hover - HOVER_ACTION], DESC_TEXT_X, DESC_TEXT_Y);
    if (p->pick < 0) return;
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u | (fb[i] >> 1 & 0x7f7f7fu);
    sa_blit(fb, p->art[ART_CONFIRM], CONFIRM_X, CONFIRM_Y);
    Image *im = picture(s, p->pick);
    if (im) sa_blit_rect(fb, im, 0, 0, im->w, ROW_H, CONFIRM_X, CONFIRM_ROW_Y);
    text_block(fb, &p->events, p->pick, CONFIRM_TEXT_X, CONFIRM_TEXT_Y);
    const Table *t = &p->events;
    int food = sa_table_int(t, p->pick, E_FOOD), money = sa_table_int(t, p->pick, E_MONEY), res = sa_table_int(t, p->pick, E_RESOURCES);
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

static void grow_facilities(SA *s, int amount) {
    if (amount <= 0) return;
    const Table *lv = &s->town.lvup;
    for (int c = 0; c < TOWN_CELLS; c++) {
        Facility *f = &s->town.cell[c];
        if (!f->type || !sa_town_staffed(&s->town, &s->vm, c)) continue;
        int group = sa_table_int(&s->town.build, sa_town_type_row(&s->town, f->type), LVUP_GROUP);
        f->exp += amount;
        for (bool up = true; up;) {
            up = false;
            for (int r = 0; r < lv->rows; r++)
                if (sa_table_int(lv, r, 0) == group && sa_table_int(lv, r, 1) == f->level + 1 && f->exp >= sa_table_int(lv, r, 2)) {
                    f->level++;
                    up = true;
                    break;
                }
        }
    }
}

static void run(SA *s) {
    Policy *p = &s->policy;
    const Table *t = &p->events;
    int r = p->pick;
    p->pick = -1;
    s->vm.c[C_RESIDENTS] += sa_table_int(t, r, E_RESIDENTS);
    s->vm.c[C_FOOD] += sa_table_int(t, r, E_FOOD);
    s->vm.c[C_MONEY] += sa_table_int(t, r, E_MONEY);
    s->vm.c[C_RESOURCES] += sa_table_int(t, r, E_RESOURCES);
    grow_facilities(s, sa_table_int(t, r, E_EXP));
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
        if (p->pick >= 0) p->pick = -1;
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

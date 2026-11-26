#include "sa.h"

enum { ART_LIST, ART_ROW, ART_LABELS, ART_BACK, ART_DECIDE, ART_UNDO, ART_RESET, ART_INFO, ART_BOARD, ART_CURSOR, ART_DIGITS, ART_COUNT };

static const char *const artNames[ART_COUNT] = {"bul_a00.agf", "bul_a01.agf", "bul_a02.agf", "bul_a03.agf",
                                                "bul_a04.agf", "bul_a05.agf", "bul_a06.agf", "bul_a07.agf",
                                                "map_00a.agf", "map_cu0.agf", "bul1013.agf"};

enum {
    LIST_X = 10,
    LIST_Y = 9,
    ROW_X = 10,
    ROW_Y = 62,
    ROW_H = 22,
    ROW_W = 308,
    LABEL_X = 22,
    BOARD_X = 339,
    BOARD_Y = 49,
    CELL_X = 348,
    CELL_Y = 56,
    CELL_PITCH = 64,
    CELL = 60,
    INFO_X = 996,
    INFO_Y = 327,
    INFO_H = 288,
    UNSTAFFED = 130,
    LINE_CHARS = 16,
    LINE_H = 16,
    TEXT_X = 1007,
    DESC_Y = 482,
    EFFECT_Y = 542,
    COL_MAZE = 10,
    MAZE_STATS = 5,
    MAZE_RIGHT = 1048,
    MAZE_PITCH = 50,
    MAZE_DIGIT_Y = 648,
    BTN_Y = 674,
    BTN_W = 152,
    BTN_H = 44,
};

enum { B_RESET, B_UNDO, B_DECIDE, B_BACK, B_COUNT };
static const int btnX[B_COUNT] = {615, 782, 949, 1116};
static const int btnArt[B_COUNT] = {ART_RESET, ART_UNDO, ART_DECIDE, ART_BACK};

void sa_build_init(BuildScreen *b, Vfs *vfs) {
    memset(b, 0, sizeof *b);
    for (int i = 0; i < ART_COUNT; i++) b->art[i] = sa_load_image(vfs, artNames[i]);
    b->held = b->listHover = b->cellHover = b->buttonHover = -1;
}

void sa_build_free(BuildScreen *b) {
    for (int i = 0; i < 16; i++) sa_image_free(b->art[i]);
    for (int i = 0; i < 40; i++) sa_image_free(b->icon[i]);
    memset(b, 0, sizeof *b);
}

void sa_build_open(BuildScreen *b) {
    b->open = true;
    b->held = -1;
    b->pendingCount = 0;
}

static int list_rows(const Town *t, const VM *vm, int *rows) {
    int n = 0;
    for (int r = 0; r < t->build.rows && n < 40; r++) {
        int flag = sa_table_int(&t->build, r, 6);
        if (flag < 0) continue;
        if (flag > 0 && (flag >= SA_F_VARS || !vm->f[flag])) continue;
        rows[n++] = r;
    }
    int order[40];
    for (int i = 0; i < n; i++) order[i] = rows[i];
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            int a = sa_table_int(&t->build, order[i], 0), c = sa_table_int(&t->build, order[j], 0);
            int ka = a == 99 ? -1 : a, kc = c == 99 ? -1 : c;
            if (kc < ka) {
                int x = order[i];
                order[i] = order[j];
                order[j] = x;
            }
        }
    for (int i = 0; i < n; i++) rows[i] = order[i];
    return n;
}

static int cell_at(int x, int y) {
    if (x < CELL_X || y < CELL_Y) return -1;
    int col = (x - CELL_X) / CELL_PITCH, row = (y - CELL_Y) / CELL_PITCH;
    if ((x - CELL_X) % CELL_PITCH >= CELL || (y - CELL_Y) % CELL_PITCH >= CELL) return -1;
    if (col >= TOWN_W || row >= TOWN_H) return -1;
    return row * TOWN_W + col;
}

static int pending_type(const BuildScreen *b, int cell) {
    for (int i = b->pendingCount - 1; i >= 0; i--)
        if (b->pendingCell[i] == cell) return b->pendingType[i];
    return -1;
}

static int cost_of(const Town *t, int type) {
    int r = sa_town_type_row(t, type);
    return r >= 0 ? sa_table_int(&t->build, r, 18) : 0;
}

static int pending_cost(const BuildScreen *b, const Town *t) {
    int n = 0;
    for (int i = 0; i < b->pendingCount; i++) n += cost_of(t, b->pendingType[i]);
    return n;
}

static bool cell_open(const Town *t, int cell) {
    int ring = sa_table_int(&t->mass, cell, 1);
    return ring != 99 && ring <= t->stage && sa_table_int(&t->mass, cell, 0) == cell;
}

static int count_of(const Town *t, const BuildScreen *b, int type) {
    int n = 0;
    for (int c = 0; c < TOWN_CELLS; c++) {
        int p = pending_type(b, c);
        if ((p >= 0 ? p : t->cell[c].type) == type) n++;
    }
    return n;
}

static bool can_place(const BuildScreen *b, const Town *t, const VM *vm, int row, int cell) {
    if (cell < 0 || !cell_open(t, cell)) return false;
    int type = sa_table_int(&t->build, row, 0);
    int current = pending_type(b, cell);
    if (current < 0) current = t->cell[cell].type;
    if (current == 100 || current == type) return false;
    if (type != 99 && current != 0 && current != 99) return false;
    if (count_of(t, b, type) >= sa_table_int(&t->build, row, 8)) return false;
    return pending_cost(b, t) + sa_table_int(&t->build, row, 18) <= vm->c[C_RESOURCES];
}

static int button_at(int x, int y) {
    if (y < BTN_Y || y >= BTN_Y + BTN_H) return -1;
    for (int i = 0; i < B_COUNT; i++)
        if (x >= btnX[i] && x < btnX[i] + BTN_W) return i;
    return -1;
}

static bool button_enabled(const BuildScreen *b, int i) { return i == B_BACK || b->pendingCount > 0; }

static void commit(BuildScreen *b, Town *t, VM *vm) {
    vm->c[C_RESOURCES] -= pending_cost(b, t);
    for (int i = 0; i < b->pendingCount; i++) {
        Facility *f = &t->cell[b->pendingCell[i]];
        f->type = b->pendingType[i] == 99 ? 0 : b->pendingType[i];
        f->level = f->type ? 1 : 0;
        f->exp = 0;
        f->built = f->type ? vm->c[C_TURN] : 0;
    }
    b->pendingCount = 0;
}

int sa_build_click(BuildScreen *b, Town *t, VM *vm, int x, int y, bool right) {
    if (right) {
        if (b->held >= 0) b->held = -1;
        else if (b->pendingCount) b->pendingCount--;
        return BUILD_STAY;
    }
    int btn = button_at(x, y);
    if (btn >= 0) {
        if (!button_enabled(b, btn)) return BUILD_STAY;
        switch (btn) {
        case B_RESET:
            b->pendingCount = 0;
            break;
        case B_UNDO:
            b->pendingCount--;
            break;
        case B_DECIDE:
            commit(b, t, vm);
            b->held = -1;
            return BUILD_DECIDED;
        case B_BACK:
            b->open = false;
            b->pendingCount = 0;
            b->held = -1;
            return BUILD_CLOSED;
        }
        return BUILD_STAY;
    }
    int rows[40], n = list_rows(t, vm, rows);
    if (x >= ROW_X && x < ROW_X + ROW_W && y >= ROW_Y && y < ROW_Y + n * ROW_H) {
        int i = (y - ROW_Y) / ROW_H;
        b->held = rows[i];
        return BUILD_STAY;
    }
    int cell = cell_at(x, y);
    if (b->held >= 0 && can_place(b, t, vm, b->held, cell) && b->pendingCount < BUILD_PENDING) {
        b->pendingCell[b->pendingCount] = cell;
        b->pendingType[b->pendingCount] = sa_table_int(&t->build, b->held, 0);
        b->pendingCount++;
        b->held = -1;
    }
    return BUILD_STAY;
}

bool sa_build_hover(BuildScreen *b, Town *t, VM *vm, int x, int y) {
    int rows[40], n = list_rows(t, vm, rows);
    int lh = x >= ROW_X && x < ROW_X + ROW_W && y >= ROW_Y && y < ROW_Y + n * ROW_H ? rows[(y - ROW_Y) / ROW_H] : -1;
    int ch = cell_at(x, y), bh = button_at(x, y);
    bool changed = lh != b->listHover || ch != b->cellHover || bh != b->buttonHover || b->held >= 0;
    b->listHover = lh;
    b->cellHover = ch;
    b->buttonHover = bh;
    return changed;
}

static Image *icon(BuildScreen *b, Town *t, Vfs *vfs, int type) {
    int r = sa_town_type_row(t, type);
    if (r < 0) return NULL;
    int k = sa_table_int(&t->build, r, 36);
    if (k < 0 || k >= 40) return NULL;
    if (!b->icon[k]) b->icon[k] = sa_load_image(vfs, sa_table_text(&t->build, r, 35));
    return b->icon[k];
}

static void blit_scaled(u32 *fb, const Image *im, int x, int y, u32 level) {
    if (!im) return;
    for (int j = 0; j < im->h; j++)
        for (int i = 0; i < im->w; i++) {
            int dx = x + i, dy = y + j;
            u32 p = im->px[(size_t)j * im->w + i], a = p >> 24;
            if (!a || dx < 0 || dy < 0 || dx >= SA_W || dy >= SA_H) continue;
            u32 q = fb[(size_t)dy * SA_W + dx], ia = 255 - a, out = 0xff000000u;
            for (int k = 0; k < 24; k += 8) out |= (((p >> k & 0xff) * level / 256 * a + (q >> k & 0xff) * ia) / 255) << k;
            fb[(size_t)dy * SA_W + dx] = out;
        }
}

static void draw_lines(u32 *fb, const char *text, int x, int y) {
    char line[LINE_CHARS * 4 + 1];
    int n = 0, chars = 0;
    for (const char *p = text;;) {
        bool brk = *p == '|';
        if (*p && !brk) {
            int k = sa_utf8_decode(p, &(u32){0});
            memcpy(line + n, p, (size_t)k);
            n += k;
            p += k;
            chars++;
        }
        if (brk || chars == LINE_CHARS || (!*p && n)) {
            line[n] = 0;
            if (n) sa_text_draw(fb, SA_W, SA_H, line, -1, x, y, 15, 0x000000, 0xffffffffu);
            y += LINE_H;
            n = chars = 0;
        }
        if (brk) p++;
        else if (!*p) break;
    }
}

static void field(u32 *fb, const Image *dg, int v, int dashes, int right, int y, int row) {
    char t[16];
    if (v) snprintf(t, sizeof t, "%d", v);
    else snprintf(t, sizeof t, "%.*s", dashes, "----------");
    sa_draw_number(fb, dg, t, right, y, row, 10, 13);
}

static void draw_info(BuildScreen *b, Town *t, const VM *vm, u32 *fb, int row, const Facility *f) {
    const Image *panel = b->art[ART_INFO];
    bool maze = vm->f[F_MAZE] != 0;
    if (panel && maze) sa_blit(fb, panel, INFO_X, INFO_Y);
    else if (panel) {
        sa_blit_rect(fb, panel, 0, 0, panel->w, INFO_H - 6, INFO_X, INFO_Y);
        sa_blit_rect(fb, panel, 0, panel->h - 6, panel->w, 6, INFO_X, INFO_Y + INFO_H - 6);
    }
    if (row < 0) return;
    const Image *dg = b->art[ART_DIGITS];
    int ox = INFO_X - 995, oy = INFO_Y - 326, level = f && f->level > 0 ? f->level : 1;
    sa_text_draw(fb, SA_W, SA_H, sa_table_text(&t->build, row, 1), -1, 1048 + ox, 359 + oy, 15, 0x000000, 0xffffffffu);
    sa_text_draw(fb, SA_W, SA_H, sa_table_text(&t->build, row, 2), -1, 1191 + ox, 359 + oy, 15, 0x000000, 0xffffffffu);
    field(fb, dg, level, 1, 1088 + ox, 381 + oy, 0);
    field(fb, dg, f ? f->exp : 0, 9, 1268 + ox, 381 + oy, 0);
    field(fb, dg, sa_table_int(&t->build, row, 19), 3, 1138 + ox, 401 + oy, 0);
    field(fb, dg, sa_table_int(&t->build, row, 9), 7, 1268 + ox, 401 + oy, 0);
    const int px[3] = {1098 + ox, 1168 + ox, 1238 + ox};
    for (int i = 0; i < 3; i++) {
        field(fb, dg, sa_table_int(&t->build, row, 25 + i * 2), 3, px[i], 437 + oy, 0);
        field(fb, dg, i == 2 ? sa_table_int(&t->build, row, 34) : 0, 3, px[i], 459 + oy, 0);
    }
    for (int i = 0; maze && i < MAZE_STATS; i++) {
        int v = sa_town_value(t, row, level, COL_MAZE + i);
        sa_draw_int(fb, dg, v, false, MAZE_RIGHT + ox + i * MAZE_PITCH, MAZE_DIGIT_Y + oy, v > 0 ? 2 : v < 0 ? 1 : 0, 10, 13);
    }
    draw_lines(fb, sa_table_text(&t->build, row, 66), TEXT_X + ox, DESC_Y + oy);
    draw_lines(fb, sa_table_text(&t->build, row, 67), TEXT_X + ox, EFFECT_Y + oy);
}

void sa_build_draw(BuildScreen *b, Town *t, Vfs *vfs, VM *vm, u32 *fb) {
    sa_blit(fb, b->art[ART_LIST], LIST_X, LIST_Y);
    int rows[40], n = list_rows(t, vm, rows);
    const Image *dg = b->art[ART_DIGITS];
    for (int i = 0; i < n; i++) {
        int r = rows[i], y = ROW_Y + i * ROW_H;
        int cost = sa_table_int(&t->build, r, 18);
        int state = b->held == r ? 2 : cost + pending_cost(b, t) > vm->c[C_RESOURCES] ? 1 : b->listHover == r ? 3 : 0;
        sa_blit_rect(fb, b->art[ART_ROW], 0, state * ROW_H, ROW_W, ROW_H, ROW_X, y);
        sa_blit_rect(fb, b->art[ART_LABELS], 0, sa_table_int(&t->build, r, 36) * 16, 100, 16, LABEL_X, y + 2);
        sa_draw_int(fb, dg, cost, false, 216, y + 5, 0, 10, 13);
        sa_draw_int(fb, dg, sa_table_int(&t->build, r, 19), false, 306, y + 5, 0, 10, 13);
    }
    sa_blit(fb, b->art[ART_BOARD], BOARD_X, BOARD_Y);
    for (int c = 0; c < TOWN_CELLS; c++) {
        int p = pending_type(b, c);
        int type = p >= 0 ? (p == 99 ? 0 : p) : t->cell[c].type;
        if (!type) continue;
        int x = CELL_X + (c % TOWN_W) * CELL_PITCH, y = CELL_Y + (c / TOWN_W) * CELL_PITCH;
        if (p >= 0) {
            sa_fill(fb, x, y, CELL, CELL, 0xe0806868u);
            sa_blit_alpha(fb, icon(b, t, vfs, type), x, y, 110);
        } else {
            blit_scaled(fb, icon(b, t, vfs, type), x, y, sa_town_staffed(t, vm, c) ? 256 : UNSTAFFED);
        }
    }
    if (b->held >= 0 && b->cellHover >= 0) {
        int c = b->cellHover;
        int x = CELL_X + (c % TOWN_W) * CELL_PITCH, y = CELL_Y + (c / TOWN_W) * CELL_PITCH;
        if (can_place(b, t, vm, b->held, c)) sa_blit(fb, icon(b, t, vfs, sa_table_int(&t->build, b->held, 0)), x, y);
        sa_blit(fb, b->art[ART_CURSOR], x, y);
    }
    int saved = vm->c[C_RESOURCES];
    Facility keep[TOWN_CELLS];
    memcpy(keep, t->cell, sizeof keep);
    for (int i = 0; i < b->pendingCount; i++) {
        t->cell[b->pendingCell[i]].type = b->pendingType[i] == 99 ? 0 : b->pendingType[i];
        t->cell[b->pendingCell[i]].level = 1;
    }
    vm->c[C_RESOURCES] -= pending_cost(b, t);
    sa_town_draw_info(t, vm, fb);
    vm->c[C_RESOURCES] = saved;
    memcpy(t->cell, keep, sizeof keep);
    int infoRow = b->held >= 0 ? b->held : b->listHover;
    const Facility *placed = NULL;
    if (infoRow < 0 && b->cellHover >= 0) {
        int p = pending_type(b, b->cellHover);
        infoRow = sa_town_type_row(t, p >= 0 ? p : t->cell[b->cellHover].type);
        if (p < 0 && t->cell[b->cellHover].type) placed = &t->cell[b->cellHover];
    }
    draw_info(b, t, vm, fb, infoRow, placed);
    for (int i = 0; i < B_COUNT; i++) {
        int frame = !button_enabled(b, i) ? 2 : b->buttonHover == i ? 1 : 0;
        sa_blit_rect(fb, b->art[btnArt[i]], 0, frame * BTN_H, BTN_W, BTN_H, btnX[i], BTN_Y);
    }
}

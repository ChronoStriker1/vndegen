#include "sa.h"

enum {
    UI_CLOSE,
    UI_BAR,
    UI_EXPEDITION,
    UI_POLICY,
    UI_REST,
    UI_BUILD,
    UI_DEPLOY,
    UI_EQUIP,
    UI_TRAIN,
    UI_SHOP,
    UI_SUBMIT,
    UI_OPTION,
    UI_SAVE,
    UI_LOAD,
    UI_LOG,
    UI_GOAL,
    UI_FAC1,
    UI_FAC2,
    UI_TOWNINFO,
    UI_DIGITS,
    UI_SMALLDIGITS,
    UI_TOWNSEC0,
    UI_TOWNSEC1,
    UI_TOWNSEC2,
    UI_TOWNSEC3,
    UI_TAB0,
    UI_MAZE_BTN = UI_TAB0 + 6,
    UI_MAZE_WIN,
    UI_MAZE_DIGITS,
    UI_FAME,
    UI_MARK,
    UI_ROLES_WIDE,
    UI_ROLES_NARROW,
    UI_ROLES_DIGITS,
    UI_MROLES_WIDE,
    UI_MROLES_NARROW,
    UI_MROLES_DIGITS,
    UI_MROLES_TITLE,
    UI_MROLES_NARROW_TITLE,
    UI_POST_ICON,
    UI_FOLD0,
    UI_BTN0 = UI_FOLD0 + TOWN_WINDOWS,
    UI_SECFOLD0 = UI_BTN0 + TOWN_WINDOWS * 3,
    UI_COUNT = UI_SECFOLD0 + 11,
};

static const char *const uiNames[UI_COUNT] = {
    "mai_a00.agf", "mai_d00.agf", "mai_b00.agf", "mai_b01.agf", "mai_b02.agf", "mai_b03.agf", "mai_b04.agf",
    "mai_b05.agf", "mai_b06.agf", "mai_b07.agf", "mai_b08.agf", "mai_c00.agf", "mai_c01.agf", "mai_c02.agf",
    "mai_c03.agf", "flw_a01.agf", "flw_b02.agf", "flw_b52.agf", "flw_c02.agf", "flw_c15.agf", "flw_b21.agf",
    "flw_c03.agf", "flw_c04.agf", "flw_c05.agf", "flw_c06.agf",
    "mai_d01.agf", "mai_d02.agf", "mai_d03.agf", "mai_d04.agf", "mai_d05.agf", "mai_d06.agf", "mai_e00.agf",
    "flw_d01.agf", "flw_d05.agf", "flw_d10.agf",
    "flw_d11.agf", "flw_e01.agf", "flw_e03.agf", "flw_e08.agf", "flw_f01.agf", "flw_f03.agf", "flw_f08.agf",
    "flw_f02.agf", "flw_f04.agf", "uts_i02.agf", "flw_b00.agf", "flw_b50.agf", "flw_c00.agf", "flw_d00.agf", "flw_e00.agf", "flw_f00.agf",
    "flw_b18.agf", "flw_b19.agf", "flw_b20.agf", "flw_b68.agf", "flw_b69.agf", "flw_b70.agf", "flw_c12.agf",
    "flw_c13.agf", "flw_c14.agf", "flw_d03.agf", "flw_d03.agf", "flw_d04.agf", "flw_e05.agf", "flw_e06.agf",
    "flw_e07.agf", "flw_f05.agf", "flw_f06.agf", "flw_f07.agf", "flw_b11.agf", "flw_b12.agf", "flw_b13.agf",
    "flw_b14.agf", "flw_b15.agf", "flw_b16.agf", "flw_b17.agf", "flw_c08.agf", "flw_c09.agf", "flw_c10.agf",
    "flw_c11.agf",
};

typedef struct Command {
    int action, ui, flag, x, y, w, h;
} Command;

static const Command commands[] = {
    {TOWN_CMD_EXPEDITION, UI_EXPEDITION, 0x1e, 16, 39, 156, 44}, {TOWN_CMD_POLICY, UI_POLICY, 0x1f, 16, 85, 156, 44},
    {TOWN_CMD_REST, UI_REST, 0x28, 16, 131, 156, 44},             {TOWN_CMD_BUILD, UI_BUILD, 0x21, 16, 180, 136, 39},
    {TOWN_CMD_DEPLOY, UI_DEPLOY, 0x22, 16, 220, 136, 39},         {TOWN_CMD_EQUIP, UI_EQUIP, 0x23, 16, 260, 136, 39},
    {TOWN_CMD_TRAIN, UI_TRAIN, 0x24, 16, 300, 136, 39},           {TOWN_CMD_SHOP, UI_SHOP, 0x25, 16, 340, 136, 39},
};

enum {
    FAC1_X = 180,
    FAC2_X = 390,
    WIN_Y = 39,
    SEC_Y = 68,
    ROW_H = 20,
    HEADER_H = 23,
    INFO_X = 1077,
    INFO_Y = 39,
    GOAL_X = 1077,
    SUBMIT_X = 16,
    SUBMIT_Y = 420,
    MISSION_TARGET = 12,
    GOAL_Y = 3,
    SIDE_X = 1159,
    SIDE_Y = 571,
    SIDE_STEP = 35,
    GRID_X = 738,
    GRID_Y = 48,
    CELL_DX = 76,
    CELL_DY = 38,
    BURST_W = 40,
    BURST_FRAMES = 3,
    BURST_FRAME_MS = 150,
    BURST_DX = 19,
    BURST_DY = 34,
    TITLE_DY = 1,
    BTN_DY = 7,
    FOLD_W = 180,
    FOLDED_DY = 4,
    MAZE_X = 868,
    MAZE_Y = 70,
    ROLES_X = 400,
    ROLES_Y = 493,
    MROLES_X = 750,
    MROLES_Y = 453,
    ROLE_ROW_Y = 32,
    ROLE_ICON_X = 94,
    ROLE_NAME_X = 124,
    ROLE_NAME_DY = -2,
    ROLE_NAME_SIZE = 16,
    ICON_W = 24,
    ICON_H = 20,
    MARK_W = 24,
    MARK_H = 15,
    FAME_W = 68,
    BAR_X = 54,
    BAR_Y = 3,
    TAB_X = 68,
    TAB_Y = 7,
    TAB_DX = 42,
    TAB_W = 40,
    TAB_H = 21,
    MAZE_BTN_X = 832,
    MAZE_BTN_Y = 4,
    CLOSE_X = 14,
    CLOSE_Y = 1,
};

#define NO_EDGE 0xffffffffu

Image *sa_load_image(Vfs *vfs, const char *name) {
    size_t n;
    u8 *d = sa_vfs_read(vfs, name, &n);
    Image *im = d ? sa_agf_decode(d, n) : NULL;
    free(d);
    return im;
}

bool sa_town_init(Town *t, Vfs *vfs) {
    memset(t, 0, sizeof *t);
    t->hover = -1;
    t->vfs = vfs;
    if (!sa_table_load(&t->build, vfs, "TownBuil.obj") || !sa_table_load(&t->mass, vfs, "TownMass.obj") ||
        !sa_table_load(&t->lvup, vfs, "TownLvup.obj"))
        return false;
    for (int i = 0; i < UI_COUNT; i++) t->ui[i] = sa_load_image(vfs, uiNames[i]);
    t->tile = sa_alloc(sizeof(Image *) * (size_t)t->build.rows);
    return true;
}

void sa_town_free(Town *t) {
    for (int i = 0; i < 8; i++) sa_image_free(t->bg[i]);
    sa_image_free(t->burst);
    for (int i = 0; i < UI_COUNT; i++) sa_image_free(t->ui[i]);
    if (t->tile)
        for (int i = 0; i < t->build.rows; i++) sa_image_free(t->tile[i]);
    free(t->tile);
    sa_table_free(&t->build);
    sa_table_free(&t->mass);
    sa_table_free(&t->lvup);
    memset(t, 0, sizeof *t);
}

int sa_town_type_row(const Town *t, int id) {
    for (int r = 0; r < t->build.rows; r++)
        if (sa_table_int(&t->build, r, 0) == id) return r;
    return -1;
}

void sa_town_new_game(Town *t) {
    memset(t->cell, 0, sizeof t->cell);
    t->stage = 1;
    t->condition = 100;
    int maou = sa_town_type_row(t, 100);
    int start = maou >= 0 ? sa_table_int(&t->build, maou, 7) : 44;
    if (start >= 0 && start < TOWN_CELLS) {
        t->cell[start].type = 100;
        t->cell[start].level = 1;
    }
}

static int workers_needed(const Town *t, int type) {
    int r = sa_town_type_row(t, type);
    return r >= 0 ? sa_table_int(&t->build, r, 19) : 0;
}

enum { LVUP_SHIFT = 6, LVUP_EXP = 2 };

static int lvup_row(const Town *t, int group, int level) {
    for (int i = 0; i < t->lvup.rows; i++)
        if (sa_table_int(&t->lvup, i, 0) == group && sa_table_int(&t->lvup, i, 1) == level) return i;
    return -1;
}

int sa_town_value(const Town *t, int r, int level, int col) {
    int v = sa_table_int(&t->build, r, col), group = sa_table_int(&t->build, r, 5);
    for (int lv = 2; lv <= level; lv++) {
        int i = lvup_row(t, group, lv);
        if (i >= 0) v += sa_table_int(&t->lvup, i, col - LVUP_SHIFT);
    }
    return v;
}

void sa_town_add_exp(Town *t, int cell, int amount) {
    Facility *f = &t->cell[cell];
    int r = f->type ? sa_town_type_row(t, f->type) : -1;
    if (r < 0) return;
    f->exp += amount;
    if (f->exp < 0) f->exp = 0;
}

void sa_town_level_up(Town *t) {
    for (int c = 0; c < TOWN_CELLS; c++) {
        Facility *f = &t->cell[c];
        int r = f->type ? sa_town_type_row(t, f->type) : -1;
        if (r < 0) continue;
        int group = sa_table_int(&t->build, r, 5), i;
        while ((i = lvup_row(t, group, f->level + 1)) >= 0 && f->exp >= sa_table_int(&t->lvup, i, LVUP_EXP)) f->level++;
    }
}

void sa_town_grow_all(Town *t, const VM *vm, int amount) {
    for (int c = 0; c < TOWN_CELLS; c++)
        if (t->cell[c].type && sa_town_staffed(t, vm, c)) sa_town_add_exp(t, c, amount);
}

int sa_town_staffed(const Town *t, const VM *vm, int cell) {
    int left = vm->c[C_RESIDENTS];
    for (int i = 0; i < TOWN_CELLS; i++) {
        int type = t->cell[i].type;
        if (!type) continue;
        int need = workers_needed(t, type);
        bool ok = need <= left;
        if (ok) left -= need;
        if (i == cell) return ok;
    }
    return 0;
}

static void cell_center(int cell, int *x, int *y) {
    int row = cell / TOWN_W, col = cell % TOWN_W;
    *x = GRID_X + (col - row) * CELL_DX;
    *y = GRID_Y + (col + row) * CELL_DY;
}

static Image *tile_for(Town *t, Vfs *vfs, int type) {
    int r = sa_town_type_row(t, type);
    if (r < 0) return NULL;
    if (!t->tile[r]) t->tile[r] = sa_load_image(vfs, sa_table_text(&t->build, r, 37));
    return t->tile[r];
}

static void blit_dim(u32 *fb, const Image *im, int x, int y, u32 level) {
    if (!im) return;
    for (int j = 0; j < im->h; j++) {
        int dy = y + j;
        if (dy < 0 || dy >= SA_H) continue;
        for (int i = 0; i < im->w; i++) {
            int dx = x + i;
            if (dx < 0 || dx >= SA_W) continue;
            u32 p = im->px[(size_t)j * im->w + i], a = p >> 24;
            if (!a) continue;
            u32 r = (p & 0xff) * level / 256, g = (p >> 8 & 0xff) * level / 256, b = (p >> 16 & 0xff) * level / 256;
            u32 q = fb[(size_t)dy * SA_W + dx], ia = 255 - a;
            r = (r * a + (q & 0xff) * ia) / 255;
            g = (g * a + (q >> 8 & 0xff) * ia) / 255;
            b = (b * a + (q >> 16 & 0xff) * ia) / 255;
            fb[(size_t)dy * SA_W + dx] = 0xff000000u | b << 16 | g << 8 | r;
        }
    }
}

static int digit_index(char c) {
    static const char set[] = "0123456789+-/%";
    const char *p = strchr(set, c);
    return p && c ? (int)(p - set) : -1;
}

void sa_draw_number(u32 *fb, const Image *font, const char *s, int right, int y, int row, int cw, int ch) {
    int n = (int)strlen(s);
    int x = right - n * cw;
    for (int i = 0; i < n; i++, x += cw) {
        int k = digit_index(s[i]);
        if (k >= 0) sa_blit_rect(fb, font, k * cw, row * ch, cw, ch, x, y);
    }
}

void sa_draw_int(u32 *fb, const Image *font, int v, bool sign, int right, int y, int row, int cw, int ch) {
    char buf[24];
    snprintf(buf, sizeof buf, sign && v > 0 ? "+%d" : "%d", v);
    sa_draw_number(fb, font, buf, right, y, row, cw, ch);
}

static Image *row_art(Town *t, Vfs *vfs, int area, int index) {
    static Image *cache[10][12];
    static Vfs *owner;
    if (owner != vfs) {
        memset(cache, 0, sizeof cache);
        owner = vfs;
    }
    if (area < 0 || area >= 10 || index < 0 || index >= 12) return NULL;
    if (!cache[area][index]) {
        char n[24];
        snprintf(n, sizeof n, "flw_b0%d%d.agf", area, index);
        cache[area][index] = sa_load_image(vfs, n);
    }
    return cache[area][index];
}

static Image *section_art(Vfs *vfs, int area) {
    static Image *cache[10];
    if (area < 0 || area >= 10) return NULL;
    if (!cache[area]) {
        char n[24];
        snprintf(n, sizeof n, "flw_b0%d.agf", area);
        cache[area] = sa_load_image(vfs, n);
    }
    return cache[area];
}

static const int fac1Areas[] = {3, 4, 8, 9};
static const int fac2Areas[] = {5, 6, 7};

static int area_of(const Town *t, int r) {
    const char *a = sa_table_text(&t->build, r, 2);
    static const char *const names[] = {"行政", "住居", "農業", "工業", "商業", "環境", "学園"};
    static const int ids[] = {3, 4, 5, 6, 7, 8, 9};
    for (int i = 0; i < 7; i++)
        if (!strcmp(a, names[i])) return ids[i];
    return -1;
}

typedef struct WinFrame {
    int x, y, w, btnX, btnY, buttons;
} WinFrame;

static WinFrame frame_of(const Town *t, int w) {
    switch (w) {
    case TW_FAC1: return (WinFrame){FAC1_X, WIN_Y, 200, FAC1_X + 135, WIN_Y + BTN_DY, 3};
    case TW_FAC2: return (WinFrame){FAC2_X, WIN_Y, 200, FAC2_X + 135, WIN_Y + BTN_DY, 3};
    case TW_INFO: return (WinFrame){INFO_X, INFO_Y, 200, INFO_X + 134, INFO_Y + BTN_DY, 3};
    case TW_MAZE: return (WinFrame){MAZE_X, MAZE_Y, 200, MAZE_X + 135, MAZE_Y + 6, 2};
    case TW_ROLES: return (WinFrame){ROLES_X, ROLES_Y, t->narrow[w] ? 272 : 332, ROLES_X + 205, ROLES_Y + 6, 3};
    default: return (WinFrame){MROLES_X, MROLES_Y, t->narrow[w] ? 272 : 332, MROLES_X + 205, MROLES_Y + 6, 3};
    }
}

static bool window_open(const Town *t, const VM *vm, int w) {
    if (w == TW_MAZE || w == TW_MAZE_ROLES) return vm->f[F_MAZE] != 0;
    if (w == TW_ROLES) return vm->f[F_POSTS] != 0;
    (void)t;
    return true;
}

static bool window_shown(const Town *t, const VM *vm, int w) { return window_open(t, vm, w) && !t->shut[w]; }

static int first_button(const Town *t, int w) { return t->folded[w] || w == TW_MAZE ? 1 : 0; }

static void draw_buttons(Town *t, u32 *fb, int w) {
    WinFrame f = frame_of(t, w);
    for (int i = first_button(t, w); i < 3; i++) {
        int k = TOWN_WINBTN0 + w * 3 + i;
        sa_blit_rect(fb, t->ui[UI_BTN0 + w * 3 + i], 0, t->hover == k ? 19 : 0, 20, 19, f.btnX + i * 20, f.btnY);
    }
}

static bool draw_folded(Town *t, u32 *fb, int w) {
    if (!t->folded[w]) return false;
    WinFrame f = frame_of(t, w);
    sa_blit(fb, t->ui[UI_FOLD0 + w], f.x + f.w - FOLD_W, f.y);
    draw_buttons(t, fb, w);
    return true;
}

static void draw_window(Town *t, Vfs *vfs, const VM *vm, u32 *fb, int w, int titleUi, const int *areas, int areaCount) {
    if (draw_folded(t, fb, w)) return;
    int x = frame_of(t, w).x, y = SEC_Y;
    sa_blit(fb, t->ui[titleUi], x, WIN_Y + TITLE_DY);
    for (int a = 0; a < areaCount; a++) {
        int area = areas[a];
        bool shut = t->sectionFolded[w][a];
        Image *sec = shut ? t->ui[UI_SECFOLD0 + area - 3] : section_art(vfs, area);
        sa_blit(fb, sec, x, y);
        int built = 0, slot = 0, index = 0;
        for (int r = 0; r < t->build.rows; r++) {
            if (area_of(t, r) != area) continue;
            int type = sa_table_int(&t->build, r, 0);
            if (type == 0 || type == 99) continue;
            index++;
            int count = 0, working = 0, level = 0;
            for (int c = 0; c < TOWN_CELLS; c++)
                if (t->cell[c].type == type) {
                    count++;
                    working += sa_town_staffed(t, vm, c);
                    if (t->cell[c].level > level) level = t->cell[c].level;
                }
            built += count;
            int flag = sa_table_int(&t->build, r, 6);
            bool known = count || (flag >= 0 && flag < SA_F_VARS && vm->f[flag]);
            if (!known || shut) continue;
            int ry = y + HEADER_H + slot * ROW_H;
            sa_blit(fb, row_art(t, vfs, area, index), x, ry);
            char buf[16];
            snprintf(buf, sizeof buf, "%d", working);
            sa_draw_number(fb, t->ui[UI_SMALLDIGITS], buf, x + 117, ry + 5, 0, 10, 13);
            snprintf(buf, sizeof buf, "%d", count);
            sa_draw_number(fb, t->ui[UI_SMALLDIGITS], buf, x + 147, ry + 5, 0, 10, 13);
            snprintf(buf, sizeof buf, "%d", level);
            sa_draw_number(fb, t->ui[UI_SMALLDIGITS], buf, x + 193, ry + 5, 0, 10, 13);
            slot++;
        }
        sa_draw_int(fb, t->ui[UI_SMALLDIGITS], built, false, x + 193, y + (shut ? FOLDED_DY : 5), 0, 10, 13);
        y += sec ? sec->h : HEADER_H;
    }
    draw_buttons(t, fb, w);
}

static int section_height(const Town *t, Vfs *vfs, int w, int a, int area) {
    const Image *im = w == TW_INFO ? (t->sectionFolded[w][a] ? t->ui[UI_SECFOLD0 + 7 + a] : t->ui[UI_TOWNSEC0 + a])
                                   : (t->sectionFolded[w][a] ? t->ui[UI_SECFOLD0 + area - 3] : section_art(vfs, area));
    return im ? im->h : HEADER_H;
}

static int housing(const Town *t) {
    int n = 0;
    for (int c = 0; c < TOWN_CELLS; c++) {
        int r = t->cell[c].type ? sa_town_type_row(t, t->cell[c].type) : -1;
        if (r >= 0) n += sa_table_int(&t->build, r, 9);
    }
    return n;
}

static void income(const Town *t, const VM *vm, int *food, int *money, int *res, int *people) {
    *food = *money = *res = *people = 0;
    for (int c = 0; c < TOWN_CELLS; c++) {
        if (!t->cell[c].type || !sa_town_staffed(t, vm, c)) continue;
        int r = sa_town_type_row(t, t->cell[c].type);
        if (r < 0) continue;
        *food += sa_table_int(&t->build, r, 25);
        *money += sa_table_int(&t->build, r, 27);
        *res += sa_table_int(&t->build, r, 29) - sa_table_int(&t->build, r, 34);
        *people += sa_table_int(&t->build, r, 24);
    }
}

static bool targets(const Town *t, const VM *vm, int *out) {
    if (!t->missions || !vm->c[C_MISSION_ID]) return false;
    for (int r = 0; r < t->missions->rows; r++)
        if (sa_table_int(t->missions, r, 0) == vm->c[C_MISSION_ID]) {
            for (int i = 0; i < 3; i++) out[i] = sa_table_int(t->missions, r, MISSION_TARGET + i);
            return true;
        }
    return false;
}

static void draw_goal(Town *t, const VM *vm, u32 *fb) {
    sa_blit(fb, t->ui[UI_GOAL], GOAL_X, GOAL_Y);
    char s[16];
    snprintf(s, sizeof s, "%d", vm->c[C_MISSION_DUE]);
    sa_draw_number(fb, t->ui[UI_DIGITS], vm->c[C_MISSION_ID] ? s : "---", GOAL_X + 84, GOAL_Y + 9, 0, 10, 13);
    snprintf(s, sizeof s, "%d", vm->c[C_TURN]);
    sa_draw_number(fb, t->ui[UI_DIGITS], s, GOAL_X + 183, GOAL_Y + 9, 0, 10, 13);
}

static void draw_info(Town *t, const VM *vm, u32 *fb, bool live) {
    if (live && draw_folded(t, fb, TW_INFO)) return;
    const Image *dg = t->ui[UI_DIGITS];
    int jobs = 0;
    for (int c = 0; c < TOWN_CELLS; c++)
        if (t->cell[c].type) jobs += workers_needed(t, t->cell[c].type);
    int food, money, res, people;
    income(t, vm, &food, &money, &res, &people);
    int y = INFO_Y + 29;
    int right = INFO_X + 191;
    const int values[4] = {vm->c[C_RESIDENTS], vm->c[C_FOOD], vm->c[C_MONEY], vm->c[C_RESOURCES]};
    const int incomes[4] = {0, food - vm->c[C_RESIDENTS] / 10, money, res};
    int goal[3];
    bool hasGoal = targets(t, vm, goal);
    for (int i = 0; i < 4; i++) {
        bool shut = live && t->sectionFolded[TW_INFO][i];
        const Image *sec = t->ui[shut ? UI_SECFOLD0 + 7 + i : UI_TOWNSEC0 + i];
        int sign = i == 0 ? (vm->c[C_RESIDENTS] - jobs < 0 ? 1 : 2) : incomes[i] > 0 ? 2 : incomes[i] < 0 ? 1 : 0;
        sa_blit(fb, sec, INFO_X, y);
        sa_draw_int(fb, dg, values[i], false, right, y + (shut ? FOLDED_DY : 5), sign, 10, 13);
        if (!shut && i == 0) {
            sa_draw_int(fb, dg, vm->c[C_RESIDENTS] - jobs, true, right, y + 25, vm->c[C_RESIDENTS] - jobs < 0 ? 1 : 2, 10, 13);
            sa_draw_int(fb, dg, housing(t), false, right, y + 45, 0, 10, 13);
        } else if (!shut) {
            sa_draw_int(fb, dg, incomes[i], true, right, y + 25, sign, 10, 13);
            if (hasGoal) sa_draw_int(fb, dg, goal[i - 1], false, right, y + 45, values[i] >= goal[i - 1] ? 2 : 1, 10, 13);
            else sa_draw_number(fb, dg, "--", right, y + 45, 0, 10, 13);
        }
        y += sec ? sec->h : 64;
    }
    sa_blit(fb, t->ui[UI_TOWNINFO], INFO_X, INFO_Y + TITLE_DY);
    if (live) draw_buttons(t, fb, TW_INFO);
    else
        for (int i = 0; i < 3; i++) sa_blit_rect(fb, t->ui[UI_BTN0 + TW_INFO * 3 + i], 0, 0, 20, 19, INFO_X + 134 + i * 20, INFO_Y + BTN_DY);
}

void sa_town_draw_info(Town *t, const VM *vm, u32 *fb) { draw_info(t, vm, fb, false); }

static void draw_maze(Town *t, const VM *vm, u32 *fb) {
    if (draw_folded(t, fb, TW_MAZE)) return;
    const Maze *m = t->maze;
    const Image *dg = t->ui[UI_MAZE_DIGITS];
    sa_blit(fb, t->ui[UI_MAZE_WIN], MAZE_X, MAZE_Y);
    if (m) {
        sa_draw_int(fb, dg, m->floors, false, MAZE_X + 163, MAZE_Y + 33, 0, 10, 13);
        const int stats[4] = {m->guards, m->passages, m->traps, m->chests};
        for (int i = 0; i < 4; i++) {
            sa_blit_rect(fb, t->ui[UI_MARK], 0, m->marks[i] * MARK_H, MARK_W, MARK_H, MAZE_X + 122, MAZE_Y + 52 + i * 20);
            sa_draw_int(fb, dg, stats[i], false, MAZE_X + 192, MAZE_Y + 54 + i * 20, 0, 10, 13);
        }
        const int money[3] = {m->food, m->money, m->resources};
        for (int i = 0; i < 3; i++) sa_draw_int(fb, dg, money[i], false, MAZE_X + 192, MAZE_Y + 158 + i * 20, money[i] > 0 ? 2 : money[i] < 0 ? 1 : 0, 10, 13);
        sa_blit_rect(fb, t->ui[UI_FAME], 0, m->fame * MARK_H, FAME_W, MARK_H, MAZE_X + 122, MAZE_Y + 219);
        sa_draw_int(fb, dg, m->intruders, false, MAZE_X + 173, MAZE_Y + 241, 0, 10, 13);
    }
    draw_buttons(t, fb, TW_MAZE);
}

static const Image *role_label(Town *t, int index, bool narrow) {
    static Image *cache[2][11];
    static Vfs *owner;
    if (owner != t->vfs) {
        memset(cache, 0, sizeof cache);
        owner = t->vfs;
    }
    if (!cache[narrow][index]) {
        char n[24];
        snprintf(n, sizeof n, "flw_f%d%02d.agf", narrow ? 1 : 0, index);
        cache[narrow][index] = sa_load_image(t->vfs, n);
    }
    return cache[narrow][index];
}

static void draw_roles(Town *t, const VM *vm, u32 *fb, int w) {
    if (draw_folded(t, fb, w)) return;
    WinFrame f = frame_of(t, w);
    bool maze = w == TW_MAZE_ROLES, narrow = t->narrow[w];
    int first = maze ? 10 : 1, count = maze ? 11 : 9;
    sa_blit(fb, t->ui[maze ? (narrow ? UI_MROLES_NARROW : UI_MROLES_WIDE) : (narrow ? UI_ROLES_NARROW : UI_ROLES_WIDE)], f.x, f.y);
    if (maze) sa_blit(fb, t->ui[narrow ? UI_MROLES_NARROW_TITLE : UI_MROLES_TITLE], f.x, f.y);
    const Image *dg = t->ui[maze ? UI_MROLES_DIGITS : UI_ROLES_DIGITS];
    const Party *p = t->party;
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    for (int i = 0; i < count; i++) {
        int post = first + i, y = f.y + ROLE_ROW_Y + i * ROW_H;
        if (!sa_maze_post_open(t->maze, vm, post)) continue;
        if (maze) sa_blit(fb, role_label(t, i, narrow), f.x, y - 2);
        int unit = p ? p->post[post] : 0, r = unit ? sa_party_unit_row(p, unit) : -1;
        if (r >= 0) {
            int icon = sa_party_post_icon(p, unit, post);
            if (icon >= 0) sa_blit_rect(fb, t->ui[UI_POST_ICON], icon % 2 * ICON_W, icon / 2 * ICON_H, ICON_W, ICON_H, f.x + ROLE_ICON_X, y - 4);
            sa_text_draw(fb, SA_W, SA_H, sa_table_text(&p->units, r, 2), -1, f.x + ROLE_NAME_X, y + ROLE_NAME_DY, ROLE_NAME_SIZE, 0x000000, NO_EDGE);
        }
        if (narrow) continue;
        if (r >= 0) {
            int gov, lead, row = sa_party_post_stats(p, unit, post, &gov, &lead) ? 2 : 0;
            sa_draw_int(fb, dg, gov, false, f.x + 290, y, row, 10, 13);
            sa_draw_int(fb, dg, lead, false, f.x + 320, y, row, 10, 13);
        } else {
            sa_draw_number(fb, dg, "--", f.x + 290, y, 0, 10, 13);
            sa_draw_number(fb, dg, "--", f.x + 320, y, 0, 10, 13);
        }
    }
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    draw_buttons(t, fb, w);
}

static void draw_tabs(Town *t, const VM *vm, u32 *fb) {
    sa_blit(fb, t->ui[UI_BAR], BAR_X, BAR_Y);
    for (int w = 0; w < TOWN_WINDOWS; w++) {
        if (!window_open(t, vm, w)) continue;
        int k = TOWN_TAB0 + w;
        sa_blit_rect(fb, t->ui[UI_TAB0 + w], t->shut[w] ? 0 : TAB_W, t->hover == k ? TAB_H : 0, TAB_W, TAB_H, TAB_X + w * TAB_DX, TAB_Y);
    }
}

typedef struct Placed {
    int action, ui, x, y, w, h, frames;
} Placed;

static int layout(const Town *t, const VM *vm, Placed *out) {
    int n = 0;
    int count = (int)(sizeof commands / sizeof commands[0]);
    int built = 0;
    for (int c = 0; c < TOWN_CELLS; c++)
        if (t->cell[c].type && t->cell[c].type != 100) built++;
    for (int i = 0; i < count; i++) {
        const Command *c = &commands[i];
        if (!vm->f[c->flag]) continue;
        if (c->action == TOWN_CMD_REST && !built && vm->c[C_TURN] == 1) continue;
        out[n++] = (Placed){c->action, c->ui, c->x, c->y, c->w, c->h, 3};
    }
    if (vm->c[C_MISSION_ID]) out[n++] = (Placed){TOWN_CMD_SUBMIT, UI_SUBMIT, SUBMIT_X, SUBMIT_Y, 164, 44, 3};
    out[n++] = (Placed){TOWN_BTN_LOG, UI_LOG, SIDE_X, SIDE_Y, 116, 34, 2};
    out[n++] = (Placed){TOWN_BTN_SAVE, UI_SAVE, SIDE_X, SIDE_Y + SIDE_STEP, 116, 34, 2};
    out[n++] = (Placed){TOWN_BTN_LOAD, UI_LOAD, SIDE_X, SIDE_Y + 2 * SIDE_STEP, 116, 34, 2};
    out[n++] = (Placed){TOWN_BTN_OPTION, UI_OPTION, SIDE_X, SIDE_Y + 3 * SIDE_STEP, 116, 34, 2};
    out[n++] = (Placed){TOWN_BTN_HIDE, UI_CLOSE, CLOSE_X, CLOSE_Y, 36, 34, 2};
    if (vm->f[F_MAZE]) out[n++] = (Placed){TOWN_CMD_MAZE, UI_MAZE_BTN, MAZE_BTN_X, MAZE_BTN_Y, 224, 56, 2};
    return n;
}

void sa_town_burst(Town *t, VM *vm, int count) {
    int cells[TOWN_CELLS], n = 0;
    for (int c = 0; c < TOWN_CELLS; c++)
        if (t->cell[c].type) cells[n++] = c;
    t->burstCount = 0;
    t->burstMs = 0;
    while (t->burstCount < count && n > 0) {
        int k = (int)(sa_vm_random(vm) % (u32)n);
        t->burstCell[t->burstCount++] = cells[k];
        cells[k] = cells[--n];
    }
}

bool sa_town_burst_tick(Town *t, u32 elapsedMs) {
    if (!t->burstCount) return false;
    t->burstMs += elapsedMs;
    if (t->burstMs >= SA_TOWN_BURST_MS) t->burstCount = 0;
    return true;
}

static void draw_bursts(Town *t, Vfs *vfs, u32 *fb) {
    if (!t->burstCount) return;
    if (!t->burst) t->burst = sa_load_image(vfs, "event01.agf");
    if (!t->burst) return;
    int frame = (int)(t->burstMs / BURST_FRAME_MS) % BURST_FRAMES;
    for (int i = 0; i < t->burstCount; i++) {
        int cx, cy;
        cell_center(t->burstCell[i], &cx, &cy);
        sa_blit_rect(fb, t->burst, frame * BURST_W, 0, BURST_W, t->burst->h, cx - BURST_DX, cy - BURST_DY);
    }
}

void sa_town_draw_map(Town *t, Vfs *vfs, const VM *vm, u32 *fb) {
    int stage = t->stage < 1 ? 1 : t->stage;
    static const char *const bgNames[] = {"tmap00a.agf", "tmap00a.agf", "tmap01a.agf", "tmap02a.agf", "tmap00b.agf", "tmap00b.agf", "tmap01b.agf", "tmap02b.agf"};
    int bi = (stage > 3 ? 3 : stage) + (vm->c[C_TOWN_MAP] ? 4 : 0);
    if (!t->bg[bi]) t->bg[bi] = sa_load_image(vfs, bgNames[bi]);
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u;
    sa_blit(fb, t->bg[bi], 0, 0);
    for (int sum = 0; sum < TOWN_W + TOWN_H; sum++)
        for (int c = 0; c < TOWN_CELLS; c++) {
            if (c / TOWN_W + c % TOWN_W != sum || !t->cell[c].type) continue;
            Image *im = tile_for(t, vfs, t->cell[c].type);
            if (!im) continue;
            int cx, cy;
            cell_center(c, &cx, &cy);
            blit_dim(fb, im, cx - im->w / 2, cy + 34 - im->h, sa_town_staffed(t, vm, c) ? 256 : 192);
        }
    draw_bursts(t, vfs, fb);
}

void sa_town_draw(Town *t, Vfs *vfs, const VM *vm, u32 *fb) {
    sa_town_draw_map(t, vfs, vm, fb);
    if (t->mapOnly) {
        draw_goal(t, vm, fb);
        return;
    }
    Placed p[24];
    int n = layout(t, vm, p);
    for (int i = 0; i < n; i++) {
        if (t->hidden && p[i].action != TOWN_BTN_HIDE) continue;
        sa_blit_rect(fb, t->ui[p[i].ui], 0, t->hover == p[i].action ? p[i].h : 0, p[i].w, p[i].h, p[i].x, p[i].y);
    }
    if (t->hidden) return;
    if (t->party) sa_party_draw_diorama(t->party, fb, vm->c[C_RANK_MAX], NULL);
    draw_tabs(t, vm, fb);
    if (window_shown(t, vm, TW_FAC1)) draw_window(t, vfs, vm, fb, TW_FAC1, UI_FAC1, fac1Areas, 4);
    if (window_shown(t, vm, TW_FAC2)) draw_window(t, vfs, vm, fb, TW_FAC2, UI_FAC2, fac2Areas, 3);
    if (window_shown(t, vm, TW_MAZE)) draw_maze(t, vm, fb);
    if (window_shown(t, vm, TW_INFO)) draw_info(t, vm, fb, true);
    draw_goal(t, vm, fb);
    if (window_shown(t, vm, TW_ROLES)) draw_roles(t, vm, fb, TW_ROLES);
    if (window_shown(t, vm, TW_MAZE_ROLES)) draw_roles(t, vm, fb, TW_MAZE_ROLES);
}

void sa_town_draw_status(Town *t, const VM *vm, u32 *fb) {
    sa_town_draw_info(t, vm, fb);
    sa_town_draw_status_goal(t, vm, fb);
}

void sa_town_draw_status_goal(Town *t, const VM *vm, u32 *fb) { draw_goal(t, vm, fb); }

static bool inside(int x, int y, int rx, int ry, int w, int h) { return x >= rx && x < rx + w && y >= ry && y < ry + h; }

static int window_height(const Town *t, int w) {
    switch (w) {
    case TW_MAZE: return t->ui[UI_MAZE_WIN] ? t->ui[UI_MAZE_WIN]->h : 0;
    case TW_ROLES: return t->ui[UI_ROLES_WIDE] ? t->ui[UI_ROLES_WIDE]->h : 0;
    case TW_MAZE_ROLES: return t->ui[UI_MROLES_WIDE] ? t->ui[UI_MROLES_WIDE]->h : 0;
    default: return 0;
    }
}

static int windows_hit(const Town *t, const VM *vm, const int *order, int count, int x, int y) {
    for (int i = 0; i < count; i++) {
        int w = order[i];
        if (!window_shown(t, vm, w)) continue;
        WinFrame f = frame_of(t, w);
        for (int b = first_button(t, w); b < 3; b++)
            if (inside(x, y, f.btnX + b * 20, f.btnY, 20, 19)) return TOWN_WINBTN0 + w * 3 + b;
        if (t->folded[w]) {
            if (inside(x, y, f.x + f.w - FOLD_W, f.y, FOLD_W, 32)) return TOWN_NONE;
            continue;
        }
        if (w == TW_FAC1 || w == TW_FAC2 || w == TW_INFO) {
            const int *areas = w == TW_FAC1 ? fac1Areas : fac2Areas;
            int n = w == TW_FAC1 ? 4 : w == TW_FAC2 ? 3 : 4, sy = w == TW_INFO ? INFO_Y + 29 : SEC_Y;
            for (int a = 0; a < n; a++) {
                int h = section_height(t, t->vfs, w, a, w == TW_INFO ? 0 : areas[a]);
                if (inside(x, y, f.x, sy, f.w, t->sectionFolded[w][a] ? h : HEADER_H)) return TOWN_SECTION0 + w * TOWN_SECTIONS + a;
                sy += h;
            }
            continue;
        }
        if (inside(x, y, f.x, f.y, f.w, window_height(t, w))) return TOWN_NONE;
    }
    return -1;
}

int sa_town_role_window_hit(const Town *t, const VM *vm, int x, int y) {
    static const int only[1] = {TW_MAZE_ROLES};
    return windows_hit(t, vm, only, 1, x, y);
}

void sa_town_draw_role_window(Town *t, const VM *vm, u32 *fb) {
    if (window_shown(t, vm, TW_MAZE_ROLES)) draw_roles(t, vm, fb, TW_MAZE_ROLES);
}

static int window_hit(const Town *t, const VM *vm, int x, int y) {
    static const int order[TOWN_WINDOWS] = {TW_MAZE_ROLES, TW_ROLES, TW_INFO, TW_MAZE, TW_FAC2, TW_FAC1};
    int h = windows_hit(t, vm, order, TOWN_WINDOWS, x, y);
    if (h >= 0) return h;
    for (int w = 0; w < TOWN_WINDOWS; w++)
        if (window_open(t, vm, w) && inside(x, y, TAB_X + w * TAB_DX, TAB_Y, TAB_W, TAB_H)) return TOWN_TAB0 + w;
    return -1;
}

bool sa_town_toggle(Town *t, int action) {
    if (action >= TOWN_TAB0 && action < TOWN_TAB0 + TOWN_WINDOWS) {
        int w = action - TOWN_TAB0;
        t->shut[w] = !t->shut[w];
        return true;
    }
    if (action >= TOWN_WINBTN0 && action < TOWN_SECTION0) {
        int w = (action - TOWN_WINBTN0) / 3, b = (action - TOWN_WINBTN0) % 3;
        if (b == 2) t->shut[w] = true;
        else if (b == 1) t->folded[w] = !t->folded[w];
        else if (w == TW_ROLES || w == TW_MAZE_ROLES) t->narrow[w] = !t->narrow[w];
        else {
            bool any = false;
            for (int a = 0; a < TOWN_SECTIONS; a++) any |= !t->sectionFolded[w][a];
            for (int a = 0; a < TOWN_SECTIONS; a++) t->sectionFolded[w][a] = any;
        }
        return true;
    }
    if (action >= TOWN_SECTION0 && action < TOWN_ACTIONS) {
        int k = action - TOWN_SECTION0;
        t->sectionFolded[k / TOWN_SECTIONS][k % TOWN_SECTIONS] = !t->sectionFolded[k / TOWN_SECTIONS][k % TOWN_SECTIONS];
        return true;
    }
    return false;
}

int sa_town_hit(const Town *t, const VM *vm, int x, int y) {
    if (!t->hidden && !t->mapOnly) {
        int w = window_hit(t, vm, x, y);
        if (w >= 0) return w;
    }
    Placed p[24];
    int n = layout(t, vm, p);
    for (int i = 0; i < n; i++)
        if (x >= p[i].x && x < p[i].x + p[i].w && y >= p[i].y && y < p[i].y + p[i].h) {
            if (t->hidden && p[i].action != TOWN_BTN_HIDE) continue;
            return p[i].action;
        }
    return TOWN_NONE;
}

bool sa_town_hover(Town *t, const VM *vm, int x, int y) {
    int h = sa_town_hit(t, vm, x, y);
    if (h == t->hover) return false;
    t->hover = h;
    return true;
}

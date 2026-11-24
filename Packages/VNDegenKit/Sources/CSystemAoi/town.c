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
    UI_WINBTN0,
    UI_WINBTN1,
    UI_WINBTN2,
    UI_TOWNSEC0,
    UI_TOWNSEC1,
    UI_TOWNSEC2,
    UI_TOWNSEC3,
    UI_COUNT,
};

static const char *const uiNames[UI_COUNT] = {
    "mai_a00.agf", "mai_d00.agf", "mai_b00.agf", "mai_b01.agf", "mai_b02.agf", "mai_b03.agf", "mai_b04.agf",
    "mai_b05.agf", "mai_b06.agf", "mai_b07.agf", "mai_b08.agf", "mai_c00.agf", "mai_c01.agf", "mai_c02.agf",
    "mai_c03.agf", "flw_a01.agf", "flw_b02.agf", "flw_b52.agf", "flw_c02.agf", "flw_c15.agf", "flw_b21.agf",
    "flw_b18.agf", "flw_b19.agf", "flw_b20.agf", "flw_c03.agf", "flw_c04.agf", "flw_c05.agf", "flw_c06.agf",
};

typedef struct Command {
    int action, ui, flag, x, y, w, h;
} Command;

static const Command commands[] = {
    {TOWN_CMD_EXPEDITION, UI_EXPEDITION, 0x1e, 4, 36, 156, 44}, {TOWN_CMD_POLICY, UI_POLICY, 0x1f, 4, 82, 156, 44},
    {TOWN_CMD_REST, UI_REST, 0x28, 4, 128, 156, 44},             {TOWN_CMD_BUILD, UI_BUILD, 0x21, 15, 179, 136, 39},
    {TOWN_CMD_DEPLOY, UI_DEPLOY, 0x22, 15, 220, 136, 39},        {TOWN_CMD_EQUIP, UI_EQUIP, 0x23, 15, 261, 136, 39},
    {TOWN_CMD_TRAIN, UI_TRAIN, 0x24, 15, 302, 136, 39},          {TOWN_CMD_SHOP, UI_SHOP, 0x25, 15, 343, 136, 39},
};

enum {
    FAC1_X = 179,
    FAC2_X = 389,
    WIN_Y = 39,
    SEC_Y = 68,
    ROW_H = 20,
    HEADER_H = 23,
    INFO_X = 1076,
    INFO_Y = 39,
    GOAL_X = 1076,
    SUBMIT_X = 16,
    SUBMIT_Y = 420,
    MISSION_TARGET = 12,
    GOAL_Y = 2,
    SIDE_X = 1158,
    SIDE_Y = 570,
    SIDE_STEP = 35,
    GRID_X = 738,
    GRID_Y = 48,
    CELL_DX = 76,
    CELL_DY = 38,
};

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
    if (!sa_table_load(&t->build, vfs, "TownBuil.obj") || !sa_table_load(&t->mass, vfs, "TownMass.obj") ||
        !sa_table_load(&t->lvup, vfs, "TownLvup.obj"))
        return false;
    for (int i = 0; i < UI_COUNT; i++) t->ui[i] = sa_load_image(vfs, uiNames[i]);
    t->tile = sa_alloc(sizeof(Image *) * (size_t)t->build.rows);
    return true;
}

void sa_town_free(Town *t) {
    for (int i = 0; i < 8; i++) sa_image_free(t->bg[i]);
    for (int i = 0; i < 40; i++) sa_image_free(t->ui[i]);
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

static void draw_window(Town *t, Vfs *vfs, const VM *vm, u32 *fb, int x, int titleUi, const int *areas, int areaCount) {
    int y = SEC_Y;
    for (int a = 0; a < areaCount; a++) {
        int area = areas[a];
        Image *sec = section_art(vfs, area);
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
            if (!known) continue;
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
        sa_draw_int(fb, t->ui[UI_SMALLDIGITS], built, false, x + 193, y + 5, 0, 10, 13);
        y += sec ? sec->h : HEADER_H;
    }
    sa_blit(fb, t->ui[titleUi], x, WIN_Y);
    for (int i = 0; i < 3; i++) sa_blit_rect(fb, t->ui[UI_WINBTN0 + i], 0, 0, 20, 19, x + 135 + i * 20, WIN_Y + 6);
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

void sa_town_draw_info(Town *t, const VM *vm, u32 *fb) {
    const Image *dg = t->ui[UI_DIGITS];
    int jobs = 0;
    for (int c = 0; c < TOWN_CELLS; c++)
        if (t->cell[c].type) jobs += workers_needed(t, t->cell[c].type);
    int food, money, res, people;
    income(t, vm, &food, &money, &res, &people);
    int y = INFO_Y + 29;
    int right = INFO_X + 191;
    sa_blit(fb, t->ui[UI_TOWNSEC0], INFO_X, y);
    sa_draw_int(fb, dg, vm->c[C_RESIDENTS], false, right, y + 5, 2, 10, 13);
    sa_draw_int(fb, dg, vm->c[C_RESIDENTS] - jobs, true, right, y + 25, vm->c[C_RESIDENTS] - jobs < 0 ? 1 : 2, 10, 13);
    sa_draw_int(fb, dg, housing(t), false, right, y + 45, 0, 10, 13);
    y += t->ui[UI_TOWNSEC0] ? t->ui[UI_TOWNSEC0]->h : 64;
    const int values[3] = {vm->c[C_FOOD], vm->c[C_MONEY], vm->c[C_RESOURCES]};
    const int incomes[3] = {food - vm->c[C_RESIDENTS] / 10, money, res};
    int goal[3];
    bool hasGoal = targets(t, vm, goal);
    for (int i = 0; i < 3; i++) {
        const Image *sec = t->ui[UI_TOWNSEC1 + i];
        int sign = incomes[i] > 0 ? 2 : incomes[i] < 0 ? 1 : 0;
        sa_blit(fb, sec, INFO_X, y);
        sa_draw_int(fb, dg, values[i], false, right, y + 5, sign, 10, 13);
        sa_draw_int(fb, dg, incomes[i], true, right, y + 25, sign, 10, 13);
        if (hasGoal) sa_draw_int(fb, dg, goal[i], false, right, y + 45, values[i] >= goal[i] ? 2 : 1, 10, 13);
        else sa_draw_number(fb, dg, "--", right, y + 45, 0, 10, 13);
        y += sec ? sec->h : 64;
    }
    sa_blit(fb, t->ui[UI_TOWNINFO], INFO_X, INFO_Y);
    for (int i = 0; i < 3; i++) sa_blit_rect(fb, t->ui[UI_WINBTN0 + i], 0, 0, 20, 19, INFO_X + 135 + i * 20, INFO_Y + 6);
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
    out[n++] = (Placed){TOWN_BTN_HIDE, UI_CLOSE, 13, 0, 36, 34, 2};
    return n;
}

void sa_town_draw_map(Town *t, Vfs *vfs, const VM *vm, u32 *fb) {
    int stage = t->stage < 1 ? 1 : t->stage;
    static const char *const bgNames[] = {"tmap00a.agf", "tmap00a.agf", "tmap01a.agf", "tmap02a.agf"};
    int bi = stage > 3 ? 3 : stage;
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
            blit_dim(fb, im, cx - 68, cy + 34 - im->h, sa_town_staffed(t, vm, c) ? 256 : 192);
        }
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
    sa_blit(fb, t->ui[UI_BAR], 57, 2);
    draw_window(t, vfs, vm, fb, FAC1_X, UI_FAC1, fac1Areas, 4);
    draw_window(t, vfs, vm, fb, FAC2_X, UI_FAC2, fac2Areas, 3);
    sa_town_draw_status(t, vm, fb);
}

void sa_town_draw_status(Town *t, const VM *vm, u32 *fb) {
    sa_town_draw_info(t, vm, fb);
    sa_town_draw_status_goal(t, vm, fb);
}

void sa_town_draw_status_goal(Town *t, const VM *vm, u32 *fb) { draw_goal(t, vm, fb); }

int sa_town_hit(const Town *t, const VM *vm, int x, int y) {
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

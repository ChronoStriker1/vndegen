#include "sa_private.h"

enum {
    M_ID = 0,
    M_FIRST = 6,
    M_LAST = 7,
    M_TURNS = 11,
    M_TARGET = 12,
    C_MISSION = 55,
    C_MISSION_START = 56,
    C_MISSION_END = 57,
    C_MISSION_STATE = 58,
    PANEL_X = 340,
    PANEL_Y = 130,
    SUBMIT_X = 570,
    CANCEL_X = 780,
    BTN_Y = 350,
    SUBMIT_W = 200,
    CANCEL_W = 152,
    BTN_H = 44,
    TURN_RIGHT = 525,
    TURN_Y = 172,
    TURN_DY = 20,
    SMALL_W = 10,
    SMALL_H = 13,
    BIG_W = 21,
    BIG_H = 30,
    ROW_Y = 234,
    ROW_DY = 40,
    COL_RIGHT = 580,
    COL_DX = 169,
    HOVER_SUBMIT = 1,
    HOVER_CANCEL = 2,
    COMMAND_HOOK = 31,
    CONFIRM_X = 540,
};

enum { ART_BG, ART_PANEL, ART_SUBMIT, ART_CANCEL, ART_SMALL, ART_BIG, ART_CONFIRM, ART_COUNT };

void sa_mission_init(SA *s) {
    Missions *m = &s->missions;
    memset(m, 0, sizeof *m);
    if (!sa_table_load(&m->table, &s->vfs, "mission.obj")) sa_log(s, "can't read mission.obj");
    static const char *const names[ART_COUNT] = {"bg009a0.agf", "pay_a00.agf", "pay_b00.agf", "pay_b01.agf", "pay_c00.agf", "pay_c01.agf", "pay_b02.agf"};
    for (int i = 0; i < ART_COUNT; i++) m->art[i] = sa_load_image(&s->vfs, names[i]);
    s->town.missions = &m->table;
}

void sa_mission_free(SA *s) {
    Missions *m = &s->missions;
    sa_table_free(&m->table);
    for (int i = 0; i < ART_COUNT; i++) sa_image_free(m->art[i]);
    memset(m, 0, sizeof *m);
}

void sa_mission_reset(SA *s) { s->missions.logCount = 0; }

static int row_of(const SA *s, int id) {
    for (int r = 0; r < s->missions.table.rows; r++)
        if (sa_table_int(&s->missions.table, r, M_ID) == id) return r;
    return -1;
}

static bool logged(const SA *s, int id) {
    for (int i = 0; i < s->missions.logCount; i++)
        if (s->missions.log[i][0] == id) return true;
    return false;
}

static int available(SA *s, int *rows) {
    const Table *t = &s->missions.table;
    int turn = s->vm.c[C_TURN], n = 0;
    for (int r = 0; r < t->rows && n < MISSION_LOG; r++) {
        int id = sa_table_int(t, r, M_ID);
        if (turn >= sa_table_int(t, r, M_FIRST) && turn <= sa_table_int(t, r, M_LAST) && !logged(s, id)) rows[n++] = r;
    }
    return n;
}

int sa_mission_available(SA *s) {
    int rows[MISSION_LOG];
    return available(s, rows);
}

int sa_mission_start(SA *s) {
    int rows[MISSION_LOG], n = available(s, rows);
    if (!n || s->missions.logCount >= MISSION_LOG) return 0;
    int r = rows[sa_vm_random(&s->vm) % (u32)n];
    const Table *t = &s->missions.table;
    int id = sa_table_int(t, r, M_ID);
    s->vm.c[C_MISSION] = id;
    s->vm.c[C_MISSION_START] = s->vm.c[C_TURN];
    s->vm.c[C_MISSION_END] = s->vm.c[C_TURN] + sa_table_int(t, r, M_TURNS) - 1;
    s->vm.c[C_MISSION_STATE] = 0;
    int *e = s->missions.log[s->missions.logCount++];
    e[0] = id, e[1] = 1, e[2] = 0;
    sa_log(s, "mission %d set, due turn %d", id, s->vm.c[C_MISSION_END]);
    return id;
}

bool sa_mission_target(SA *s, int *out) {
    int r = s->vm.c[C_MISSION] ? row_of(s, s->vm.c[C_MISSION]) : -1;
    if (r < 0) return false;
    for (int i = 0; i < 3; i++) out[i] = sa_table_int(&s->missions.table, r, M_TARGET + i);
    return true;
}

static bool met(SA *s) {
    int target[3];
    if (!sa_mission_target(s, target)) return false;
    for (int i = 0; i < 3; i++)
        if (s->vm.c[C_FOOD + i] < target[i]) return false;
    return true;
}

static void pay(SA *s) {
    int target[3];
    if (!sa_mission_target(s, target)) return;
    for (int i = 0; i < 3; i++) s->vm.c[C_FOOD + i] -= target[i];
}

void sa_mission_check(SA *s) {
    int state = 0;
    if (s->vm.c[C_MISSION] && s->vm.c[C_TURN] > s->vm.c[C_MISSION_END]) {
        state = met(s) ? 1 : 2;
        if (state == 1) pay(s);
    }
    s->vm.c[C_MISSION_STATE] = state;
}

void sa_mission_end(SA *s, int result) {
    int id = s->vm.c[C_MISSION];
    for (int i = s->missions.logCount - 1; i >= 0 && id; i--)
        if (s->missions.log[i][0] == id) {
            s->missions.log[i][2] = result;
            break;
        }
    s->vm.c[C_MISSION] = s->vm.c[C_MISSION_START] = s->vm.c[C_MISSION_END] = 0;
    s->vm.c[C_MISSION_STATE] = 0;
    sa_log(s, "mission %d ended: %d", id, result);
}

static bool affordable(SA *s) { return met(s); }

void sa_mission_open(SA *s) {
    s->missions.confirm = false;
    s->missions.hover = 0;
    s->mode = MODE_SUBMIT;
    s->dirty = true;
}

static int hit(SA *s, int x, int y) {
    if (affordable(s) && x >= SUBMIT_X && x < SUBMIT_X + SUBMIT_W && y >= BTN_Y && y < BTN_Y + BTN_H) return HOVER_SUBMIT;
    if (x >= CANCEL_X && x < CANCEL_X + CANCEL_W && y >= BTN_Y && y < BTN_Y + BTN_H) return HOVER_CANCEL;
    return 0;
}

static void draw_panel(SA *s, u32 *fb) {
    Missions *m = &s->missions;
    sa_blit(fb, m->art[ART_PANEL], PANEL_X, PANEL_Y);
    sa_draw_int(fb, m->art[ART_SMALL], s->vm.c[C_TURN], false, TURN_RIGHT, TURN_Y, 0, SMALL_W, SMALL_H);
    sa_draw_int(fb, m->art[ART_SMALL], s->vm.c[C_MISSION_END], false, TURN_RIGHT, TURN_Y + TURN_DY, 0, SMALL_W, SMALL_H);
    int target[3] = {0, 0, 0};
    sa_mission_target(s, target);
    for (int i = 0; i < 3; i++) {
        int y = ROW_Y + i * ROW_DY, now = s->vm.c[C_FOOD + i], after = now - target[i];
        sa_draw_int(fb, m->art[ART_BIG], now, false, COL_RIGHT, y, 0, BIG_W, BIG_H);
        sa_draw_int(fb, m->art[ART_BIG], target[i], false, COL_RIGHT + COL_DX, y, 0, BIG_W, BIG_H);
        sa_draw_int(fb, m->art[ART_BIG], after, false, COL_RIGHT + 2 * COL_DX, y, after < 0 ? 1 : 2, BIG_W, BIG_H);
    }
}

static bool on_confirm(int x, int y) { return x >= CONFIRM_X && x < CONFIRM_X + SUBMIT_W && y >= BTN_Y && y < BTN_Y + BTN_H; }

void sa_mission_draw_confirm(SA *s, u32 *fb) {
    Missions *m = &s->missions;
    if (!m->confirm) return;
    draw_panel(s, fb);
    sa_blit_rect(fb, m->art[ART_CONFIRM], 0, m->hover ? BTN_H : 0, SUBMIT_W, BTN_H, CONFIRM_X, BTN_Y);
}

bool sa_mission_confirm_input(SA *s) {
    Missions *m = &s->missions;
    if (!m->confirm) return false;
    bool changed = false;
    int h = on_confirm(s->mouseX, s->mouseY);
    if (h != m->hover) {
        m->hover = h;
        changed = true;
    }
    s->rightClicked = false;
    if (s->clicked) {
        s->clicked = false;
        if (h) {
            m->confirm = false;
            if (s->vm.state == VM_WAIT_DIALOG) s->vm.state = VM_RUN;
            changed = true;
        }
    }
    return changed;
}

static void draw(SA *s) {
    Missions *m = &s->missions;
    u32 *fb = s->fb;
    sa_blit(fb, m->art[ART_BG], 0, 0);
    draw_panel(s, fb);
    if (affordable(s)) sa_blit_rect(fb, m->art[ART_SUBMIT], 0, m->hover == HOVER_SUBMIT ? BTN_H : 0, SUBMIT_W, BTN_H, SUBMIT_X, BTN_Y);
    sa_blit_rect(fb, m->art[ART_CANCEL], 0, m->hover == HOVER_CANCEL ? BTN_H : 0, CANCEL_W, BTN_H, CANCEL_X, BTN_Y);
}

static void submit(SA *s) {
    pay(s);
    s->mode = MODE_TOWN;
    const char *box = s->vm.boxSlot[COMMAND_HOOK];
    sa_reset_scene(s);
    if (!box || !sa_start_script(s, box, 0)) sa_mission_end(s, 1);
}

bool sa_mission_frame(SA *s) {
    if (s->mode != MODE_SUBMIT) return false;
    Missions *m = &s->missions;
    int h = hit(s, s->mouseX, s->mouseY);
    if (h != m->hover) {
        m->hover = h;
        s->dirty = true;
    }
    if (s->rightClicked) {
        s->rightClicked = false;
        s->mode = MODE_TOWN;
    }
    if (s->clicked) {
        s->clicked = false;
        if (h == HOVER_SUBMIT) submit(s);
        else if (h == HOVER_CANCEL) s->mode = MODE_TOWN;
        s->dirty = true;
    }
    if (s->mode == MODE_SUBMIT && s->dirty) {
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

#include "sa_private.h"

enum {
    HOOK_SETUP = 10,
    HOOK_FLOOR = 11,
    HOOK_STEP = 12,
    HOOK_RETURN = 17,
    HOOK_DUNGEON = 25,
    HOOK_BATTLE_START = 26,
    HOOK_BATTLE_END = 27,
};

enum { AFTER_NONE, AFTER_WALK, AFTER_BATTLE, AFTER_HOME, AFTER_LOST };

enum {
    C_EXP_X = 0x49,
    C_EXP_Y = 0x4a,
    C_EXP_DIR = 0x4b,
    C_EXP_DUNGEON = 0x46,
    C_EXP_FLOOR = 0x47,
    C_EXP_GROUP = 0x96,
    C_EXP_DUNGEON_BGM = 0x97,
    C_EXP_RESULT = 0x9b,
    C_FIND0 = 0xd2,
};

enum {
    PLATE_X = 340,
    PLATE_Y = 40,
    PLATE_W = 404,
    PLATE_H = 54,
    PLATE_PITCH = 60,
    AREA_X = 760,
    BADGE_X = 354,
    BADGE_DY = 12,
    BACK_X = 444,
    BACK_Y = 640,
    BACK_W = 204,
    BACK_H = 54,
    HOVER_BACK = 100,
    HOVER_AREA = 200,
    F_DUNGEON = 0,
    F_FLOOR = 1,
    F_NAME = 4,
    F_MAP = 5,
    F_SET = 6,
    F_MIN_STEPS = 19,
    F_GROUPS = 31,
    F_RATE = 32,
};

#define NO_EDGE 0xffffffffu

static const char *text(const Table *t, int r, int c) { return sa_table_text(t, r, c); }

void sa_exp_init(SA *s) {
    Expedition *e = &s->exp;
    memset(e, 0, sizeof *e);
    if (!sa_table_load(&e->floors, &s->vfs, "floor.obj") || !sa_table_load(&e->dungeons, &s->vfs, "dungeon.obj"))
        sa_log(s, "can't read the dungeon tables");
    e->hall = sa_load_image(&s->vfs, "bg005a0.agf");
    for (int i = 0; i < 12; i++) {
        char n[16];
        snprintf(n, sizeof n, "gat_b%02d.agf", i);
        e->plate[i] = sa_load_image(&s->vfs, n);
    }
    e->areaPlate = sa_load_image(&s->vfs, "gat_a01.agf");
    e->back = sa_load_image(&s->vfs, "gat_c00.agf");
    e->badge = sa_load_image(&s->vfs, "gat_d01.agf");
    e->hover = -1;
}

void sa_exp_free(SA *s) {
    Expedition *e = &s->exp;
    sa_table_free(&e->floors);
    sa_table_free(&e->dungeons);
    sa_image_free(e->hall);
    for (int i = 0; i < 12; i++) sa_image_free(e->plate[i]);
    sa_image_free(e->areaPlate);
    sa_image_free(e->back);
    sa_image_free(e->badge);
    memset(e, 0, sizeof *e);
}

static int floor_row(const Expedition *e, int dungeon, int floor) {
    for (int r = 0; r < e->floors.rows; r++)
        if (sa_table_int(&e->floors, r, F_DUNGEON) == dungeon && sa_table_int(&e->floors, r, F_FLOOR) == floor) return r;
    return -1;
}

static int open_dungeons(const SA *s) { return s->exp.dungeons.rows ? 1 : 0; }

static int open_floors(const SA *s, int dungeon) {
    int n = 0;
    while (floor_row(&s->exp, dungeon, n + 1) >= 0 && n < 1) n++;
    return n;
}

void sa_exp_open_select(SA *s) {
    s->exp.picked = 0;
    s->exp.hover = -1;
    s->mode = MODE_SELECT;
    s->dirty = true;
}

static int select_hit(const SA *s, int x, int y) {
    if (x >= BACK_X && x < BACK_X + BACK_W && y >= BACK_Y && y < BACK_Y + BACK_H) return HOVER_BACK;
    for (int i = 0; i < open_dungeons(s); i++)
        if (x >= PLATE_X && x < PLATE_X + PLATE_W && y >= PLATE_Y + i * PLATE_PITCH && y < PLATE_Y + i * PLATE_PITCH + PLATE_H) return i + 1;
    if (s->exp.picked)
        for (int i = 0; i < open_floors(s, s->exp.picked); i++)
            if (x >= AREA_X && x < AREA_X + PLATE_W && y >= PLATE_Y + i * PLATE_PITCH && y < PLATE_Y + i * PLATE_PITCH + PLATE_H)
                return HOVER_AREA + i + 1;
    return -1;
}

static void draw_select(SA *s) {
    Expedition *e = &s->exp;
    u32 *fb = s->fb;
    sa_blit(fb, e->hall, 0, 0);
    for (int i = 0; i < open_dungeons(s); i++) {
        int frame = e->picked == i + 1 ? 2 : e->hover == i + 1 ? 1 : 0;
        sa_blit_rect(fb, e->plate[i + 1], 0, frame * PLATE_H, PLATE_W, PLATE_H, PLATE_X, PLATE_Y + i * PLATE_PITCH);
        if (!e->visited[i]) sa_blit_rect(fb, e->badge, 0, 0, e->badge ? e->badge->w : 0, 28, BADGE_X, PLATE_Y + i * PLATE_PITCH + BADGE_DY);
    }
    if (e->picked)
        for (int i = 0; i < open_floors(s, e->picked); i++) {
            int y = PLATE_Y + i * PLATE_PITCH, frame = e->hover == HOVER_AREA + i + 1 ? 1 : 0;
            sa_blit_rect(fb, e->areaPlate, 0, frame * PLATE_H, PLATE_W, PLATE_H, AREA_X, y);
            const char *name = text(&e->floors, floor_row(e, e->picked, i + 1), F_NAME);
            int w = sa_text_width(name, -1, 22);
            sa_text_draw(fb, SA_W, SA_H, name, -1, AREA_X + (PLATE_W - w) / 2, y + 15, 22, 0x503010, 0xffffff);
        }
    sa_blit_rect(fb, e->back, 0, e->hover == HOVER_BACK ? BACK_H : 0, BACK_W, BACK_H, BACK_X, BACK_Y);
    sa_party_draw_diorama(&s->party, fb, s->vm.c[C_RANK_MAX], NULL);
}

static bool run_next_hook(SA *s) {
    Expedition *e = &s->exp;
    while (e->hookCount) {
        int h = e->hooks[0];
        memmove(e->hooks, e->hooks + 1, sizeof(int) * (size_t)--e->hookCount);
        const char *box = h >= 0 && h < 256 ? s->vm.boxSlot[h] : NULL;
        if (!box) continue;
        char name[32];
        snprintf(name, sizeof name, "%s", box);
        sa_log(s, "hook %d: %s", h, name);
        return sa_start_script(s, name, 0);
    }
    return false;
}

static void walk_view(SA *s) {
    s->mode = MODE_DUNGEON;
    s->dungeon.hover = -1;
    s->dirty = true;
}

static void finish_after(SA *s) {
    Expedition *e = &s->exp;
    int after = e->after;
    e->after = AFTER_NONE;
    switch (after) {
    case AFTER_WALK: walk_view(s); break;
    case AFTER_BATTLE:
        s->mode = MODE_BATTLE;
        s->dirty = true;
        break;
    case AFTER_HOME:
        for (int i = 0; i < 4; i++) {
            s->vm.c[C_RESIDENTS + i] += s->vm.c[C_FIND0 + i];
            s->vm.c[C_FIND0 + i] = 0;
        }
        e->active = false;
        s->mode = MODE_TOWN;
        sa_end_turn(s);
        break;
    case AFTER_LOST:
        for (int i = 0; i < 4; i++) s->vm.c[C_FIND0 + i] = 0;
        e->active = false;
        s->mode = MODE_TOWN;
        sa_end_turn(s);
        break;
    }
}

static void run_hooks(SA *s, const int *hooks, int n, int after) {
    Expedition *e = &s->exp;
    if (s->mode == MODE_DUNGEON) {
        sa_dungeon_draw(&s->dungeon, s->fb);
        sa_dungeon_draw_hud(&s->dungeon, s->fb, &s->party, s->vm.c[C_RANK_MAX]);
        sa_reset_scene(s);
        sa_show_frame_as_bg(s, s->fb, "*dungeon");
    }
    e->hookCount = 0;
    for (int i = 0; i < n && i < EXP_HOOKS; i++) e->hooks[e->hookCount++] = hooks[i];
    e->after = after;
    if (!run_next_hook(s)) finish_after(s);
}

bool sa_exp_script_done(SA *s) {
    if (!s->exp.after) return false;
    if (!run_next_hook(s)) finish_after(s);
    return true;
}

static void sync_position(SA *s) {
    s->vm.c[C_EXP_X] = s->dungeon.x;
    s->vm.c[C_EXP_Y] = s->dungeon.y;
    s->vm.c[C_EXP_DIR] = s->dungeon.dir;
}

void sa_exp_enter(SA *s, int dungeon, int floor) {
    Expedition *e = &s->exp;
    int r = floor_row(e, dungeon, floor);
    if (r < 0) {
        sa_log(s, "no floor %d of dungeon %d", floor, dungeon);
        return;
    }
    char set[24], platform[24];
    snprintf(set, sizeof set, "m%03dset.pvm", sa_table_int(&e->floors, r, F_SET));
    snprintf(platform, sizeof platform, "m%03dbt1a.agf", dungeon - 1);
    if (!sa_dungeon_load(&s->dungeon, &s->vfs, set, text(&e->floors, r, F_MAP))) {
        sa_log(s, "can't load %s with %s", text(&e->floors, r, F_MAP), set);
        return;
    }
    sa_dungeon_art(&s->dungeon, &s->vfs, platform);
    int x = 0, y = 0, dir = 2;
    sa_dungeon_find_entrance(&s->dungeon, &x, &y, &dir);
    sa_dungeon_place(&s->dungeon, x, y, dir);
    e->active = true;
    e->dungeon = dungeon;
    e->floor = floor;
    e->row = r;
    e->sinceBattle = 0;
    s->vm.c[C_EXP_DUNGEON] = dungeon;
    s->vm.c[C_EXP_FLOOR] = floor;
    s->vm.c[C_EXP_DUNGEON_BGM] = dungeon;
    sync_position(s);
    sa_log(s, "expedition: %s %s", text(&e->dungeons, dungeon - 1, 1), text(&e->floors, r, F_NAME));
    s->mode = MODE_DUNGEON;
    static const int hooks[] = {HOOK_SETUP, HOOK_DUNGEON, HOOK_FLOOR};
    run_hooks(s, hooks, 3, AFTER_WALK);
}

static void pick_group(SA *s, char *out, size_t size) {
    const char *list = text(&s->exp.floors, s->exp.row, F_GROUPS);
    int n = 1;
    for (const char *p = list; *p; p++) n += *p == '|';
    int k = (int)(sa_vm_random(&s->vm) % (u32)n);
    const char *p = list;
    while (k-- > 0) p = strchr(p, '|') + 1;
    const char *end = strchr(p, '|');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len >= size) len = size - 1;
    memcpy(out, p, len);
    out[len] = 0;
}

static void start_battle(SA *s) {
    char group[64];
    pick_group(s, group, sizeof group);
    sa_dungeon_draw(&s->dungeon, s->fb);
    Image *view = sa_image_new(SA_W, SA_H);
    memcpy(view->px, s->fb, sizeof(u32) * SA_W * SA_H);
    if (!sa_battle_begin(&s->battle, &s->vfs, group, view, s->exp.dungeon - 1, sa_vm_random(&s->vm))) {
        sa_log(s, "no enemy group %s", group);
        return;
    }
    sa_battle_set_names(&s->battle, text(&s->exp.dungeons, s->exp.dungeon - 1, 1), text(&s->exp.floors, s->exp.row, F_NAME));
    s->exp.sinceBattle = 0;
    s->vm.c[C_EXP_RESULT] = 1;
    s->vm.c[C_EXP_GROUP] = sa_table_int(&s->battle.groups, s->battle.group, 0);
    static const int hooks[] = {HOOK_BATTLE_START};
    run_hooks(s, hooks, 1, AFTER_BATTLE);
}

void sa_exp_battle_over(SA *s, int result) {
    if (!s->exp.active) {
        s->mode = MODE_TOWN;
        s->town.mapOnly = false;
        s->dirty = true;
        return;
    }
    s->vm.c[C_EXP_RESULT] = result == BATTLE_WON ? 1 : result == BATTLE_LOST ? 2 : 3;
    static const int hooks[] = {HOOK_BATTLE_END};
    s->mode = MODE_DUNGEON;
    run_hooks(s, hooks, 1, result == BATTLE_LOST ? AFTER_LOST : AFTER_WALK);
}

void sa_exp_gain(SA *s, const int gain[4]) {
    if (!s->exp.active) return;
    for (int i = 0; i < 4; i++) s->vm.c[C_FIND0 + i] += gain[i];
}

static void go_home(SA *s) {
    static const int hooks[] = {HOOK_RETURN};
    run_hooks(s, hooks, 1, AFTER_HOME);
}

static void after_move(SA *s, int r) {
    Dungeon *d = &s->dungeon;
    if (r == DUN_REST) {
        go_home(s);
        return;
    }
    if (d->bumped) {
        d->bumped = false;
        sync_position(s);
        if (sa_dungeon_facing_exit(d)) {
            go_home(s);
            return;
        }
        static const int hooks[] = {HOOK_STEP};
        run_hooks(s, hooks, 1, AFTER_WALK);
        return;
    }
    if (r != DUN_MOVED) return;
    sync_position(s);
    s->exp.sinceBattle++;
    int rate = sa_table_int(&s->exp.floors, s->exp.row, F_RATE);
    if (s->exp.sinceBattle >= sa_table_int(&s->exp.floors, s->exp.row, F_MIN_STEPS) && (int)(sa_vm_random(&s->vm) % 100) < rate) {
        start_battle(s);
        return;
    }
    if (sa_dungeon_object(d) >= 0) {
        static const int hooks[] = {HOOK_STEP};
        run_hooks(s, hooks, 1, AFTER_WALK);
    }
}

void sa_exp_key(SA *s, int key) {
    if (s->mode == MODE_DUNGEON && !s->dungeon.animDur) after_move(s, sa_dungeon_key(&s->dungeon, key));
    s->dirty = true;
}

bool sa_exp_frame(SA *s, u32 ms) {
    if (s->mode == MODE_SELECT) {
        int h = select_hit(s, s->mouseX, s->mouseY);
        if (h != s->exp.hover) {
            s->exp.hover = h;
            s->dirty = true;
        }
        if (sa_party_tick(&s->party, ms)) s->dirty = true;
        if (s->clicked || s->rightClicked) {
            bool back = s->rightClicked || h == HOVER_BACK;
            s->clicked = s->rightClicked = false;
            if (back) {
                s->mode = MODE_TOWN;
                s->town.mapOnly = false;
            } else if (h >= HOVER_AREA) {
                if (s->exp.picked) s->exp.visited[s->exp.picked - 1] = true;
                sa_exp_enter(s, s->exp.picked, h - HOVER_AREA);
            } else if (h > 0)
                s->exp.picked = h;
            s->dirty = true;
        }
        if (s->mode == MODE_SELECT && s->dirty) {
            draw_select(s);
            s->serial++;
            s->dirty = false;
        }
        return true;
    }
    if (s->mode != MODE_DUNGEON) return false;
    Dungeon *d = &s->dungeon;
    if (sa_dungeon_tick(d, ms)) s->dirty = true;
    if (sa_party_tick(&s->party, ms)) s->dirty = true;
    if (sa_dungeon_hover(d, s->mouseX, s->mouseY)) s->dirty = true;
    s->rightClicked = false;
    if (s->clicked) {
        s->clicked = false;
        if (!d->animDur) after_move(s, sa_dungeon_click(d, s->mouseX, s->mouseY));
        s->dirty = true;
    }
    if (s->mode == MODE_DUNGEON && s->dirty) {
        sa_dungeon_draw(d, s->fb);
        sa_dungeon_draw_hud(d, s->fb, &s->party, s->vm.c[C_RANK_MAX]);
        s->serial++;
        s->dirty = false;
    }
    return true;
}

void sa_exp_force_battle(SA *s) {
    if (s->mode == MODE_DUNGEON && s->exp.active) start_battle(s);
}

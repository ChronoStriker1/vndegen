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

enum { SPECIAL_RETURN = 1, SPECIAL_HP, SPECIAL_MP, SPECIAL_CURE, SPECIAL_NO_ENEMY, SPECIAL_NO_TRAP, SPECIAL_ATTACK, SPECIAL_DEFENCE, SPECIAL_EVASION, SPECIAL_SPEED, SPECIAL_RECRUIT, SPECIAL_SEARCH };
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
    C_AREA_STEPS = 0x44,
    C_STEPS = 0x45,
    C_EXP_STEPS = 0x43,
    C_ALL_STEPS = 0x42,
    C_ALL_ACTIONS = 0x3d,
    C_EXP_ACTIONS = 0x3e,
    C_AREA_ACTIONS = 0x3f,
    C_VIEW_RANGE = 0x4c,
    C_MAP_RANGE = 0x4d,
    VIEW_RANGE = 2,
    MAP_RANGE = 3,
};

enum {
    GROUP_SPAWN_DISTANCE = 8,
    GROUP_COOLDOWN = 40,
    GROUP_FLED = 2,
    ENCOUNTER_FROM = 40,
    GLOW_GAIN_X100 = 155,
};

enum {
    PLATE_X = 340,
    PLATE_Y = 40,
    PLATE_W = 404,
    PLATE_H = 54,
    PLATE_PITCH = 60,
    AREA_X = 765,
    BADGE_DX = 14,
    AREA_TEXT_Y = 16,
    AREA_SIZE = 22,
    AREA_SPACING = 4,
    F_SEEN = 731,
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
    F_SPREAD = 12,
    F_STEPS = 13,
    F_FOG = 10,
    F_BACKDROP = 8,
    PLACE_BASE = 41,
    F_GLOW = 9,
    F_ENTRY = 14,
    D_OPEN_FLAG = 3,
    F_BOSSES = 23,
    BOSS_SLOTS = 4,
    P_MOVE = 7,
    F_GROUP_LISTS = 30,
    F_GROUP_WEIGHTS = 32,
    F_MORE_GROUPS = 35,
    F_CHESTS = 36,
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
    for (int i = 1; i < 11; i++) {
        char n[16];
        snprintf(n, sizeof n, "gat_a%02d.agf", i);
        e->areaPlate[i] = sa_load_image(&s->vfs, n);
    }
    e->back = sa_load_image(&s->vfs, "gat_c00.agf");
    e->badge = sa_load_image(&s->vfs, "gat_d01.agf");
    e->seen = sa_load_image(&s->vfs, "gat_d02.agf");
    e->platform = sa_load_image(&s->vfs, "m998bt1a.agf");
    e->hover = -1;
    e->fighting = e->arriveSide = e->resumeX = -1;
    e->explored = sa_alloc(sizeof(*e->explored) * EXP_DUNGEONS);
    e->passages = sa_alloc(sizeof(*e->passages) * EXP_DUNGEONS);
    e->floorInfo = sa_alloc(sizeof(*e->floorInfo) * EXP_DUNGEONS);
}

void sa_exp_free(SA *s) {
    Expedition *e = &s->exp;
    sa_table_free(&e->floors);
    sa_table_free(&e->dungeons);
    sa_image_free(e->hall);
    for (int i = 0; i < 12; i++) sa_image_free(e->plate[i]);
    for (int i = 0; i < 11; i++) sa_image_free(e->areaPlate[i]);
    sa_image_free(e->back);
    sa_image_free(e->badge);
    sa_image_free(e->seen);
    sa_image_free(e->platform);
    free(e->explored);
    free(e->passages);
    free(e->floorInfo);
    memset(e, 0, sizeof *e);
}

static int floor_row(const Expedition *e, int dungeon, int floor) {
    for (int r = 0; r < e->floors.rows; r++)
        if (sa_table_int(&e->floors, r, F_DUNGEON) == dungeon && sa_table_int(&e->floors, r, F_FLOOR) == floor) return r;
    return -1;
}

static bool flag_on(const SA *s, int flag) { return flag == 0 || (flag > 0 && flag < SA_F_VARS && s->vm.f[flag]); }

static int open_dungeons(const SA *s, int *out);

static int open_dungeons_list(const SA *s, int *out) {
    int n = 0;
    for (int r = 0; r < s->exp.dungeons.rows && n < EXP_DUNGEONS; r++) {
        int flag = sa_table_int(&s->exp.dungeons, r, D_OPEN_FLAG);
        if (flag > 0 && s->vm.f[flag]) out[n++] = sa_table_int(&s->exp.dungeons, r, 0);
    }
    if (!n && s->exp.dungeons.rows) out[n++] = 1;
    return n;
}

static bool floor_seen(const SA *s, int dungeon, int floor) {
    if (!s->exp.explored || dungeon <= 0 || dungeon >= EXP_DUNGEONS || floor <= 0 || floor >= EXP_FLOORS) return false;
    for (int c = 0; c < DUN_CELLS; c++)
        if (s->exp.explored[dungeon][floor][c]) return true;
    return false;
}

static bool dungeon_seen(const SA *s, int dungeon) {
    for (int f = 1; f < EXP_FLOORS; f++)
        if (floor_seen(s, dungeon, f)) return true;
    return false;
}

static int open_dungeons(const SA *s, int *out) { return open_dungeons_list(s, out); }

static int area_top(const SA *s) {
    int ids[EXP_DUNGEONS], n = open_dungeons(s, ids);
    for (int i = 0; i < n; i++)
        if (ids[i] == s->exp.picked) return PLATE_Y + i * PLATE_PITCH;
    return PLATE_Y;
}

static int open_entries(const SA *s, int dungeon, int *rows) {
    int n = 0;
    for (int r = 0; r < s->exp.floors.rows && n < EXP_FLOORS; r++)
        if (sa_table_int(&s->exp.floors, r, F_DUNGEON) == dungeon && sa_table_int(&s->exp.floors, r, F_ENTRY) >= 0 &&
            flag_on(s, sa_table_int(&s->exp.floors, r, F_ENTRY)))
            rows[n++] = r;
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
    int ids[EXP_DUNGEONS], rows[EXP_FLOORS], n = open_dungeons(s, ids);
    for (int i = 0; i < n; i++)
        if (x >= PLATE_X && x < PLATE_X + PLATE_W && y >= PLATE_Y + i * PLATE_PITCH && y < PLATE_Y + i * PLATE_PITCH + PLATE_H) return ids[i];
    if (s->exp.picked)
        for (int i = 0; i < open_entries(s, s->exp.picked, rows); i++)
            if (x >= AREA_X && x < AREA_X + PLATE_W && y >= area_top(s) + i * PLATE_PITCH && y < area_top(s) + i * PLATE_PITCH + PLATE_H)
                return HOVER_AREA + i + 1;
    return -1;
}

static void draw_select(SA *s) {
    Expedition *e = &s->exp;
    u32 *fb = s->fb;
    sa_blit(fb, e->hall, 0, 0);
    int ids[EXP_DUNGEONS], rows[EXP_FLOORS], n = open_dungeons(s, ids);
    for (int i = 0; i < n; i++) {
        int d = ids[i], frame = e->picked == d ? 2 : e->hover == d ? 1 : 0;
        if (d < 12) sa_blit_rect(fb, e->plate[d], 0, frame * PLATE_H, PLATE_W, PLATE_H, PLATE_X, PLATE_Y + i * PLATE_PITCH);
        sa_blit(fb, dungeon_seen(s, d) ? e->seen : e->badge, PLATE_X + BADGE_DX, PLATE_Y + i * PLATE_PITCH + BADGE_DY);
    }
    if (e->picked)
        for (int i = 0; i < open_entries(s, e->picked, rows); i++) {
            int y = area_top(s) + i * PLATE_PITCH, frame = e->hover == HOVER_AREA + i + 1 ? 1 : 0;
            const Image *plate = e->picked < 11 && e->areaPlate[e->picked] ? e->areaPlate[e->picked] : e->areaPlate[1];
            sa_blit_rect(fb, plate, 0, frame * PLATE_H, PLATE_W, PLATE_H, AREA_X, y);
            int floor = sa_table_int(&e->floors, rows[i], F_FLOOR);
            sa_blit(fb, floor_seen(s, e->picked, floor) ? e->seen : e->badge, AREA_X + BADGE_DX, y + BADGE_DY);
            const char *name = text(&e->floors, rows[i], F_NAME);
            sa_font_set_spacing(AREA_SPACING);
            int w = sa_text_width(name, -1, AREA_SIZE);
            sa_text_draw(fb, SA_W, SA_H, name, -1, AREA_X + (PLATE_W - w) / 2, y + AREA_TEXT_Y, AREA_SIZE, 0xffffff, 0x0b0d0a);
            sa_font_set_spacing(1);
        }
    sa_blit_rect(fb, e->back, 0, e->hover == HOVER_BACK ? BACK_H : 0, BACK_W, BACK_H, BACK_X, BACK_Y);
    sa_party_draw_diorama(&s->party, fb, s->vm.c[C_RANK_MAX], e->platform);
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
        e->last[0] = 0;
        for (int i = 0; i < 4; i++) {
            s->vm.c[C_RESIDENTS + i] += s->vm.c[C_FIND0 + i];
            s->vm.c[C_FIND0 + i] = 0;
        }
        e->active = false;
        s->mode = MODE_TOWN;
        sa_end_turn(s);
        break;
    case AFTER_LOST:
        e->last[0] = 0;
        for (int i = 0; i < 4; i++) s->vm.c[C_FIND0 + i] = 0;
        e->active = false;
        s->mode = MODE_TOWN;
        sa_end_turn(s);
        break;
    }
}

void sa_exp_draw_view(SA *s, u32 *fb) {
    sa_dungeon_draw(&s->dungeon, fb);
    s->party.sgp = sa_camp_points(s);
    sa_dungeon_draw_hud(&s->dungeon, fb, &s->party, s->vm.c[C_RANK_MAX]);
    s->party.sgp = -1;
    sa_town_draw_status_goal(&s->town, &s->vm, fb);
    if (s->exp.noticeCount) {
        const char *lines[EXP_NOTICE_LINES];
        for (int i = 0; i < s->exp.noticeCount; i++) lines[i] = s->exp.notice[i];
        sa_popup_draw(s->battle.pop, fb, lines, s->exp.noticeCount);
    }
}

static void run_hooks(SA *s, const int *hooks, int n, int after) {
    Expedition *e = &s->exp;
    if (s->mode == MODE_DUNGEON) {
        sa_exp_draw_view(s, s->fb);
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
    Expedition *e = &s->exp;
    s->vm.c[C_EXP_X] = s->dungeon.x;
    s->vm.c[C_EXP_Y] = s->dungeon.y;
    s->vm.c[C_EXP_DIR] = s->dungeon.dir;
    e->last[0] = e->active ? e->dungeon << 16 | e->floor << 8 : 0;
    e->last[1] = s->dungeon.x;
    e->last[2] = s->dungeon.y;
    e->last[3] = s->dungeon.dir;
}

static void sync_steps(SA *s) {
    s->vm.c[C_STEPS] = s->exp.steps + 1;
    s->vm.c[C_EXP_STEPS] = s->exp.steps;
    s->vm.c[C_AREA_STEPS] = s->exp.steps - s->exp.areaStart;
    s->dungeon.steps = s->exp.steps;
}

static u32 mix(u32 h) {
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    return h ^ h >> 16;
}

static int distances(const Dungeon *d, int x, int y, int *dist) {
    int queue[DUN_CELLS], head = 0, tail = 0;
    for (int c = 0; c < DUN_CELLS; c++) dist[c] = -1;
    dist[y * DUN_SIDE + x] = 0;
    queue[tail++] = y * DUN_SIDE + x;
    while (head < tail) {
        int c = queue[head++];
        for (int side = 0; side < 4; side++) {
            int n = sa_dungeon_neighbour(d, c % DUN_SIDE, c / DUN_SIDE, side);
            if (n < 0 || dist[n] >= 0) continue;
            dist[n] = dist[c] + 1;
            queue[tail++] = n;
        }
    }
    return tail;
}

static bool free_cell(const Dungeon *d, int c) {
    int x = c % DUN_SIDE, y = c / DUN_SIDE;
    return sa_dungeon_get(d, "床物", x, y, 0) < 0 && !sa_dungeon_cell_is_passage(d, x, y);
}

static int pick_cell(const Dungeon *d, int fromX, int fromY, int minDist, u32 h) {
    int dist[DUN_CELLS], n = 0, pick[DUN_CELLS];
    distances(d, fromX, fromY, dist);
    for (int c = 0; c < DUN_CELLS; c++)
        if (dist[c] >= minDist && free_cell(d, c)) pick[n++] = c;
    if (!n)
        for (int c = 0; c < DUN_CELLS; c++)
            if (dist[c] > 0 && free_cell(d, c)) pick[n++] = c;
    return n ? pick[mix(h) % (u32)n] : -1;
}

static const char *group_name(SA *s, int party) {
    for (int r = 0; r < s->battle.groups.rows; r++)
        if (sa_table_int(&s->battle.groups, r, 0) == party) return text(&s->battle.groups, r, 2);
    return NULL;
}

static int group_id(SA *s, const char *name) {
    for (int r = 0; r < s->battle.groups.rows; r++)
        if (!strcmp(text(&s->battle.groups, r, 2), name)) return sa_table_int(&s->battle.groups, r, 0);
    return -1;
}

static void sync_marks(SA *s);
static void reset_countdown(SA *s);
static bool party_has(SA *s, const char *ability);
static void sync_status(SA *s);

static void set_target(ExpGroup *g, int c) {
    for (int i = 0; i < EXP_TARGETS; i++) g->target[i][0] = g->target[i][1] = -1;
    if (c < 0) return;
    g->target[0][0] = c % DUN_SIDE;
    g->target[0][1] = c / DUN_SIDE;
}

static void spawn_group(SA *s, const char *name) {
    Expedition *e = &s->exp;
    int id = group_id(s, name);
    if (id < 0 || e->groupCount >= EXP_GROUPS) {
        sa_log(s, "no enemy group %s", name);
        return;
    }
    ExpGroup *g = &e->group[e->groupCount++];
    memset(g, 0, sizeof *g);
    g->alive = true;
    g->party = id;
    g->number = e->groupCount;
    g->dir = DIR_E;
    g->flags = g->number > 1 ? 0x100 | 1 : 1;
    u32 h = (u32)(e->dungeon * 977 + e->floor * 131 + g->number * 7919);
    int c = pick_cell(&s->dungeon, s->dungeon.x, s->dungeon.y, GROUP_SPAWN_DISTANCE, h);
    g->x = c < 0 ? s->dungeon.x : c % DUN_SIDE;
    g->y = c < 0 ? s->dungeon.y : c / DUN_SIDE;
    set_target(g, pick_cell(&s->dungeon, g->x, g->y, GROUP_SPAWN_DISTANCE, h ^ 0x5bd1e995u));
    sa_battle_roll(&s->battle, name, mix(h ^ 0x27d4eb2fu), g->cells);
    for (int r = 0; r < s->battle.groups.rows; r++)
        if (sa_table_int(&s->battle.groups, r, 0) == id) g->stay = !strcmp(text(&s->battle.groups, r, P_MOVE), "停止");
    sa_log(s, "group %d (%s) at %d,%d", g->number, name, g->x, g->y);
}

int sa_exp_place_code(SA *s) {
    const char *bg = text(&s->exp.floors, s->exp.row, F_BACKDROP);
    int n = 0;
    return sscanf(bg, "bg%3d", &n) == 1 ? n + PLACE_BASE : PLACE_BASE + 21;
}

int sa_exp_area_registered(SA *s) {
    if (!s->exp.active) return 0;
    int entry = sa_table_int(&s->exp.floors, s->exp.row, F_ENTRY);
    return entry > 0 && entry < SA_F_VARS && s->vm.f[entry];
}

void sa_exp_add_group(SA *s, const char *name) {
    if (!s->exp.active) return;
    spawn_group(s, name);
    sync_marks(s);
}

void sa_exp_add_group_at(SA *s, const char *name, int x, int y) {
    if (!s->exp.active) return;
    int before = s->exp.groupCount;
    spawn_group(s, name);
    if (s->exp.groupCount > before && x >= 0 && y >= 0) {
        ExpGroup *g = &s->exp.group[s->exp.groupCount - 1];
        g->x = x;
        g->y = y;
        g->stay = true;
    }
    sync_marks(s);
}

static void spawn_floor_groups(SA *s) {
    Expedition *e = &s->exp;
    e->groupCount = 0;
    const char *list = text(&e->floors, e->row, F_GROUP_LISTS);
    char buf[256];
    snprintf(buf, sizeof buf, "%s", *list && strcmp(list, "*") ? list : "");
    for (char *p = strtok(buf, "|"); p; p = strtok(NULL, "|")) spawn_group(s, p);
    for (int k = 0; k < BOSS_SLOTS; k++) {
        const char *boss = text(&e->floors, e->row, F_BOSSES + 2 * k);
        int x, y;
        if (!*boss || !strcmp(boss, "*") || !sa_dungeon_find_mark(&s->dungeon, k + 1, &x, &y)) continue;
        int before = e->groupCount;
        spawn_group(s, boss);
        if (e->groupCount == before) continue;
        ExpGroup *g = &e->group[e->groupCount - 1];
        g->x = x;
        g->y = y;
        sa_log(s, "group %d stands at %d,%d", g->number, x, y);
    }
}

static void sync_marks(SA *s) {
    Dungeon *d = &s->dungeon;
    d->markCount = 0;
    for (int i = 0; i < s->exp.groupCount && d->markCount < DUN_MARKS; i++) {
        const ExpGroup *g = &s->exp.group[i];
        if (!g->alive) continue;
        d->markX[d->markCount] = g->x;
        d->markY[d->markCount++] = g->y;
    }
}

static void move_group(SA *s, ExpGroup *g) {
    Dungeon *d = &s->dungeon;
    if (g->stay) return;
    if (g->target[0][0] < 0 || (g->x == g->target[0][0] && g->y == g->target[0][1]))
        set_target(g, pick_cell(d, g->x, g->y, GROUP_SPAWN_DISTANCE, sa_vm_random(&s->vm)));
    if (g->target[0][0] < 0) return;
    int dist[DUN_CELLS];
    distances(d, g->target[0][0], g->target[0][1], dist);
    int here = dist[g->y * DUN_SIDE + g->x];
    for (int side = 0; side < 4; side++) {
        int n = sa_dungeon_neighbour(d, g->x, g->y, side);
        if (n < 0 || dist[n] < 0 || (here >= 0 && dist[n] >= here)) continue;
        g->x = n % DUN_SIDE;
        g->y = n / DUN_SIDE;
        g->dir = side;
        return;
    }
}

static int group_here(SA *s) {
    for (int i = 0; i < s->exp.groupCount; i++) {
        const ExpGroup *g = &s->exp.group[i];
        if (g->alive && !g->cooldown && g->x == s->dungeon.x && g->y == s->dungeon.y) return i;
    }
    return -1;
}

static void enter(SA *s, int dungeon, int floor, bool autosave) {
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
    sa_dungeon_sense(party_has(s, "感知"));
    int entry = sa_table_int(&e->floors, r, F_ENTRY);
    if (entry > 0 && entry < SA_F_VARS && s->vm.f[entry]) sa_dungeon_open_doors(&s->dungeon);
    u32 fog = (u32)sa_table_int(&e->floors, r, F_FOG);
    s->dungeon.r3.fogColor = (fog >> 16 & 0xff) | (fog & 0xff00) | (fog & 0xff) << 16;
    s->dungeon.floorGain = sa_table_int(&e->floors, r, F_GLOW) == 1 ? GLOW_GAIN_X100 / 100.0f : 1;
    if (e->explored && dungeon < EXP_DUNGEONS && floor < EXP_FLOORS) {
        s->dungeon.visited = e->explored[dungeon][floor];
        s->dungeon.passage = e->passages[dungeon][floor];
    }
    bool fresh = !e->active;
    e->active = true;
    e->dungeon = dungeon;
    e->floor = floor;
    e->row = r;
    if (fresh) {
        e->steps = 0;
        reset_countdown(s);
        s->vm.c[C_EXP_ACTIONS] = 1;
        if (!e->resuming) {
            int left = s->vm.c[C_RANK_MAX] - sa_party_rank(&s->party);
            s->vm.c[C_SGP] = left > 0 ? left : 0;
            memset(e->special, 0, sizeof e->special);
        }
        e->blindSteps = e->supportSteps = 0;
        memset(e->curse, 0, sizeof e->curse);
    }
    e->alarm = e->noResidents = false;
    e->noticeCount = 0;
    sync_status(s);
    if (!e->resuming) s->vm.c[C_AREA_ACTIONS] = 1;
    e->areaStart = e->steps;
    if (e->arriveSide >= 0) {
        if (!sa_dungeon_arrive(&s->dungeon, e->arriveSide, e->arriveAlong)) e->arriveSide = -1;
    } else if (e->resumeX >= 0) {
        sa_dungeon_place(&s->dungeon, e->resumeX, e->resumeY, e->resumeDir);
    }
    if (e->arriveSide < 0 && e->resumeX < 0) {
        int x = 0, y = 0, dir = DIR_S;
        sa_dungeon_find_entrance(&s->dungeon, &x, &y, &dir);
        sa_dungeon_place(&s->dungeon, x, y, dir);
        e->entry[0] = dungeon << 16 | floor << 8;
        e->entry[1] = 0;
        e->entry[2] = x;
        e->entry[3] = y;
    }
    if (!e->keepGroups) spawn_floor_groups(s);
    e->arriveSide = e->resumeX = -1;
    e->keepGroups = false;
    sync_marks(s);
    sync_steps(s);
    s->vm.c[C_EXP_DUNGEON] = dungeon;
    s->vm.c[C_EXP_FLOOR] = floor;
    if (!s->vm.c[C_VIEW_RANGE]) s->vm.c[C_VIEW_RANGE] = VIEW_RANGE;
    if (!s->vm.c[C_MAP_RANGE]) s->vm.c[C_MAP_RANGE] = MAP_RANGE;
    s->vm.c[C_EXP_DUNGEON_BGM] = dungeon;
    sync_position(s);
    sa_log(s, "expedition: %s %s", text(&e->dungeons, dungeon - 1, 1), text(&e->floors, r, F_NAME));
    s->mode = MODE_DUNGEON;
    if (autosave) sa_autosave(s);
    static const int first[] = {HOOK_SETUP, HOOK_DUNGEON, HOOK_FLOOR}, next[] = {HOOK_FLOOR}, resumed[] = {HOOK_DUNGEON};
    if (e->resuming) run_hooks(s, resumed, 1, AFTER_WALK);
    else if (fresh) run_hooks(s, first, 3, AFTER_WALK);
    else run_hooks(s, next, 1, AFTER_WALK);
    e->resuming = false;
}

void sa_exp_enter(SA *s, int dungeon, int floor) {
    s->exp.arriveSide = s->exp.resumeX = -1;
    s->exp.keepGroups = false;
    s->exp.active = false;
    enter(s, dungeon, floor, true);
}

void sa_exp_resume(SA *s, int dungeon, int floor, int x, int y, int dir, bool keepGroups) {
    Expedition *e = &s->exp;
    int steps = s->vm.c[C_STEPS] > 0 ? s->vm.c[C_STEPS] - 1 : 0, area = s->vm.c[C_AREA_STEPS];
    e->arriveSide = -1;
    e->resumeX = x;
    e->resumeY = y;
    e->resumeDir = dir;
    e->keepGroups = keepGroups;
    e->active = false;
    e->resuming = true;
    enter(s, dungeon, floor, false);
    e->steps = steps;
    e->areaStart = steps - area;
    sync_steps(s);
}

void sa_exp_names(SA *s, int dungeon, int floor, const char **dungeonName, const char **floorName) {
    const Expedition *e = &s->exp;
    int r = floor_row(e, dungeon, floor);
    *dungeonName = dungeon > 0 && dungeon <= e->dungeons.rows ? text(&e->dungeons, dungeon - 1, 1) : NULL;
    *floorName = r >= 0 ? text(&e->floors, r, F_NAME) : NULL;
}

const char *sa_exp_floor_map(SA *s, int dungeon, int floor) {
    int r = floor_row(&s->exp, dungeon, floor);
    return r >= 0 ? text(&s->exp.floors, r, F_MAP) : NULL;
}

void sa_exp_place_names(SA *s, const char **dungeon, const char **floor) {
    if (s->exp.active) sa_exp_names(s, s->exp.dungeon, s->exp.floor, dungeon, floor);
    else *dungeon = *floor = NULL;
}

static void pick_group(SA *s, char *out, size_t size) {
    static const int lists[3] = {F_GROUP_LISTS, F_GROUP_LISTS + 1, F_MORE_GROUPS};
    int weight[3], total = 0;
    for (int i = 0; i < 3; i++) {
        const char *l = text(&s->exp.floors, s->exp.row, lists[i]);
        weight[i] = *l && strcmp(l, "*") ? sa_table_int(&s->exp.floors, s->exp.row, F_GROUP_WEIGHTS + i) : 0;
        total += weight[i];
    }
    int which = 1;
    if (total > 0) {
        int roll = (int)(sa_vm_random(&s->vm) % (u32)total);
        for (which = 0; which < 2 && roll >= weight[which]; which++) roll -= weight[which];
    }
    const char *list = text(&s->exp.floors, s->exp.row, lists[which]);
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

static void reset_countdown(SA *s) {
    int steps = sa_table_int(&s->exp.floors, s->exp.row, F_STEPS), spread = sa_table_int(&s->exp.floors, s->exp.row, F_SPREAD);
    s->exp.stepsLeft = steps - spread + (int)(sa_vm_random(&s->vm) % (u32)(2 * spread + 1));
    if (s->exp.alarm) s->exp.stepsLeft /= 2;
}

static void begin_battle(SA *s, const char *group, const int *cells) {
    sa_dungeon_draw(&s->dungeon, s->fb);
    Image *view = sa_image_new(SA_W, SA_H);
    memcpy(view->px, s->fb, sizeof(u32) * SA_W * SA_H);
    if (!sa_battle_begin(&s->battle, &s->vfs, group, cells, view, s->exp.dungeon - 1, sa_vm_random(&s->vm))) {
        sa_log(s, "no enemy group %s", group);
        return;
    }
    static const int buffs[4][2] = {{SPECIAL_ATTACK, STAT_ATTACK}, {SPECIAL_DEFENCE, STAT_DEFENCE}, {SPECIAL_EVASION, STAT_EVASION}, {SPECIAL_SPEED, STAT_SPEED}};
    for (int k = 0; k < 4; k++) {
        if (!sa_exp_special_active(s, buffs[k][0])) continue;
        int amount = sa_exp_special_value(s, buffs[k][0]);
        for (int i = 0; i < s->battle.count; i++) {
            Fighter *f = &s->battle.f[i];
            if (f->enemy) continue;
            f->stat[buffs[k][1]] += amount;
            if (buffs[k][1] == STAT_ATTACK) f->stat[STAT_MAGIC] += amount;
            if (buffs[k][1] == STAT_DEFENCE) f->stat[STAT_RESIST] += amount;
        }
        s->exp.special[buffs[k][0] - 1]--;
    }
    for (int i = 0; i < s->battle.count; i++) {
        Fighter *f = &s->battle.f[i];
        if (f->enemy) continue;
        f->stat[STAT_DEFENCE] -= s->exp.curse[0];
        f->stat[STAT_RESIST] -= s->exp.curse[0];
        f->stat[STAT_EVASION] -= s->exp.curse[1];
    }
    memset(s->exp.curse, 0, sizeof s->exp.curse);
    sa_battle_set_names(&s->battle, text(&s->exp.dungeons, s->exp.dungeon - 1, 1), text(&s->exp.floors, s->exp.row, F_NAME));
    sa_log(s, "battle: %s, %d fighters", group, s->battle.count);
    s->vm.c[C_EXP_RESULT] = 1;
    s->vm.c[C_EXP_GROUP] = sa_table_int(&s->battle.groups, s->battle.group, 0);
    reset_countdown(s);
    static const int hooks[] = {HOOK_BATTLE_START};
    run_hooks(s, hooks, 1, AFTER_BATTLE);
}

static void start_battle(SA *s, int which) {
    Expedition *e = &s->exp;
    if (which < 0 || which >= e->groupCount) return;
    ExpGroup *g = &e->group[which];
    const char *group = group_name(s, g->party);
    if (!group) return;
    e->fighting = which;
    begin_battle(s, group, g->cells);
}

static void random_battle(SA *s) {
    char group[64];
    pick_group(s, group, sizeof group);
    s->exp.fighting = -1;
    begin_battle(s, group, NULL);
}

void sa_exp_battle_over(SA *s, int result) {
    if (!s->exp.active) {
        s->mode = MODE_TOWN;
        s->town.mapOnly = false;
        s->dirty = true;
        return;
    }
    s->vm.c[C_EXP_RESULT] = result == BATTLE_WON ? 1 : result == BATTLE_LOST ? 2 : 3;
    Expedition *e = &s->exp;
    if (e->fighting >= 0 && e->fighting < e->groupCount) {
        ExpGroup *g = &e->group[e->fighting];
        if (result == BATTLE_WON) g->alive = false;
        else {
            int c = pick_cell(&s->dungeon, s->dungeon.x, s->dungeon.y, GROUP_SPAWN_DISTANCE, sa_vm_random(&s->vm));
            if (c >= 0) {
                g->x = c % DUN_SIDE;
                g->y = c / DUN_SIDE;
            }
            g->fought++;
        }
        for (int i = 0; i < e->groupCount; i++) {
            e->group[i].cooldown = GROUP_COOLDOWN;
            e->group[i].flags |= GROUP_FLED;
        }
        e->fighting = -1;
        sync_marks(s);
    }
    static const int hooks[] = {HOOK_BATTLE_END};
    s->mode = MODE_DUNGEON;
    run_hooks(s, hooks, 1, result == BATTLE_LOST ? AFTER_LOST : AFTER_WALK);
}

void sa_exp_gain(SA *s, const int gain[4]) {
    if (!s->exp.active) return;
    for (int i = 0; i < 4; i++) {
        if (!i && s->exp.noResidents && gain[i] > 0) continue;
        int boost = sa_exp_special_value(s, i ? SPECIAL_SEARCH : SPECIAL_RECRUIT);
        s->vm.c[C_FIND0 + i] += gain[i] + gain[i] * boost / 100;
    }
}

static void go_home(SA *s) {
    static const int hooks[] = {HOOK_RETURN};
    run_hooks(s, hooks, 1, AFTER_HOME);
}

static void heal_all(SA *s, bool hp, bool mp) {
    Member ms[PARTY_SLOTS + PARTY_BENCH];
    int n = sa_party_member_list(&s->party, ms);
    for (int i = 0; i < n; i++) {
        if (hp) *ms[i].hp = sa_party_member_value(&s->party, &ms[i], 0);
        if (mp) *ms[i].mp = sa_party_member_value(&s->party, &ms[i], 1);
    }
}

bool sa_exp_special(SA *s, int id, int a, int b) {
    Expedition *e = &s->exp;
    if (!e->active || id <= 0 || id > EXP_SPECIALS) return false;
    switch (id) {
    case SPECIAL_RETURN: {
        int dungeon = e->entry[0] >> 16, floor = e->entry[0] >> 8 & 0xff;
        if (dungeon != e->dungeon || floor <= 0) return false;
        s->camp.active = false;
        if (floor == e->floor) {
            sa_dungeon_place(&s->dungeon, e->entry[2], e->entry[3], s->dungeon.dir);
            sync_position(s);
            s->mode = MODE_DUNGEON;
            s->dirty = true;
            return true;
        }
        e->resumeX = e->entry[2];
        e->resumeY = e->entry[3];
        e->resumeDir = s->dungeon.dir;
        e->keepGroups = false;
        enter(s, e->dungeon, floor, true);
        return true;
    }
    case SPECIAL_HP:
        heal_all(s, true, false);
        return true;
    case SPECIAL_MP:
        heal_all(s, false, true);
        return true;
    case SPECIAL_CURE:
        return true;
    case SPECIAL_ATTACK:
    case SPECIAL_DEFENCE:
    case SPECIAL_EVASION:
    case SPECIAL_SPEED:
        e->special[id - 1]++;
        e->specialValue[id - 1] = a;
        return true;
    default:
        e->special[id - 1] = a;
        e->specialValue[id - 1] = b;
        return true;
    }
}

static void count_down_specials(Expedition *e) {
    static const int stepped[] = {SPECIAL_NO_ENEMY, SPECIAL_NO_TRAP, SPECIAL_RECRUIT, SPECIAL_SEARCH};
    for (size_t i = 0; i < sizeof stepped / sizeof stepped[0]; i++)
        if (e->special[stepped[i] - 1] > 0) e->special[stepped[i] - 1]--;
}

bool sa_exp_special_active(const SA *s, int id) { return s->exp.active && id > 0 && id <= EXP_SPECIALS && s->exp.special[id - 1] > 0; }

int sa_exp_special_value(const SA *s, int id) { return sa_exp_special_active(s, id) ? s->exp.specialValue[id - 1] : 0; }

static void open_crystal(SA *s) {
    int slot = sa_dungeon_crystal_index(&s->dungeon);
    if (slot > 7) slot = 7;
    int col = F_CHESTS + slot * 4;
    const char *table = text(&s->exp.floors, s->exp.row, col);
    int lo = sa_table_int(&s->exp.floors, s->exp.row, col + 1), hi = sa_table_int(&s->exp.floors, s->exp.row, col + 2);
    int n = hi > lo ? lo + (int)(sa_vm_random(&s->vm) % (u32)(hi - lo + 1)) : lo;
    char msg[256] = "";
    for (int i = 0; i < n; i++) {
        int r = sa_item_pick(&s->battle.items, table, sa_vm_random(&s->vm));
        if (r < 0) continue;
        sa_party_add_item(&s->party, sa_table_int(&s->battle.items, r, 0), 1);
        size_t len = strlen(msg);
        snprintf(msg + len, sizeof msg - len, "%s『%s』を手に入れた。", len ? "|" : "", text(&s->battle.items, r, 1));
    }
    sa_dungeon_clear_object(&s->dungeon);
    sa_log(s, "crystal %d (%s): %s", slot, table, msg);
    if (msg[0]) sa_dialog_open(&s->dialog, msg);
}

static int area_at(SA *s, int dx, int dy) {
    Expedition *e = &s->exp;
    for (int r = 0; r < e->floors.rows; r++) {
        if (sa_table_int(&e->floors, r, F_DUNGEON) != e->dungeon) continue;
        const char *name = text(&e->floors, r, F_NAME), *of = strstr(name, "の");
        int x = 0, y = 0, lx = 0, ly = 0;
        if (!of) {
            if (sa_table_int(&e->floors, r, F_FLOOR) != 1) continue;
        } else {
            for (const char *p = of + strlen("の"); *p;) {
                if (!strncmp(p, "東", 3)) lx = 1, ly = 0;
                else if (!strncmp(p, "西", 3)) lx = -1, ly = 0;
                else if (!strncmp(p, "南", 3)) lx = 0, ly = 1;
                else if (!strncmp(p, "北", 3)) lx = 0, ly = -1;
                else if (strncmp(p, "奥", 3)) break;
                x += lx;
                y += ly;
                p += 3;
            }
        }
        if (x == dx && y == dy) return r;
    }
    return -1;
}

static bool area_pos(SA *s, int row, int *px, int *py) {
    for (int dy = -9; dy <= 9; dy++)
        for (int dx = -9; dx <= 9; dx++)
            if (area_at(s, dx, dy) == row) {
                *px = dx;
                *py = dy;
                return true;
            }
    return false;
}

static int map_has_passage(SA *s, int row, int side, int along) {
    Dungeon probe;
    memset(&probe, 0, sizeof probe);
    if (!sa_dungeon_read_map(&probe, &s->vfs, text(&s->exp.floors, row, F_MAP))) return false;
    return sa_dungeon_has_passage(&probe, side, along);
}

static void cross_area(SA *s, int side) {
    Expedition *e = &s->exp;
    static const int sx[4] = {0, 1, 0, -1}, sy[4] = {-1, 0, 1, 0};
    int along = side & 1 ? s->dungeon.y : s->dungeon.x, back = (side + 2) & 3, x, y, to = -1;
    if (area_pos(s, e->row, &x, &y)) {
        int r = area_at(s, x + sx[side], y + sy[side]);
        if (r >= 0 && map_has_passage(s, r, back, along)) to = r;
    }
    for (int r = 0; r < e->floors.rows && to < 0; r++)
        if (r != e->row && sa_table_int(&e->floors, r, F_DUNGEON) == e->dungeon && map_has_passage(s, r, back, along)) to = r;
    if (to < 0) {
        sa_log(s, "no area beyond side %d at %d", side, along);
        return;
    }
    sa_log(s, "expedition: to %s", text(&e->floors, to, F_NAME));
    e->arriveSide = back;
    e->arriveAlong = along;
    e->resumeX = -1;
    e->keepGroups = false;
    enter(s, e->dungeon, sa_table_int(&e->floors, to, F_FLOOR), true);
}

static void follow_link(SA *s, const DunLink *l) {
    Expedition *e = &s->exp;
    int floor = l->target < 0 ? e->floor : l->target >> 8 & 0xff;
    if (floor == e->floor) {
        sa_dungeon_place(&s->dungeon, l->tx, l->ty, l->dir);
        sync_position(s);
        return;
    }
    sa_log(s, "expedition: link to floor %d at %d,%d", floor, l->tx, l->ty);
    e->arriveSide = -1;
    e->resumeX = l->tx;
    e->resumeY = l->ty;
    e->resumeDir = l->dir;
    e->keepGroups = false;
    enter(s, e->dungeon, floor, true);
}

static void change_floor(SA *s) {
    DunLink l;
    if (sa_dungeon_link(&s->dungeon, DUN_LINK_STAIRS, &l)) follow_link(s, &l);
    else sa_log(s, "no stairs to follow from %s", text(&s->exp.floors, s->exp.row, F_NAME));
}

enum { TRAP_HAZARD, TRAP_BLIND, TRAP_MAP, TRAP_RESIDENTS, TRAP_SUPPORT, TRAP_PURGE, TRAP_SGP, TRAP_ALARM, TRAP_PHANTOM, TRAP_CURSE, TRAP_TELEPORT, TRAP_SPIN };
enum { HIT_WALK = 1, HIT_FLY = 2, HIT_ALL = 3 };
enum { TRAP_STEPS = 50, AVOID_PERCENT = 20, AVOID_FINDER_PERCENT = 50, UNIT_FLYING = 13 };

static const struct {
    int object, kind, arg, hp, mp;
    const char *name, *line;
} traps[] = {
    {1, TRAP_HAZARD, HIT_WALK, 30, 0, "棘の床", "徒歩タイプにダメージ！"},
    {2, TRAP_HAZARD, HIT_FLY, 30, 0, "棘の風", "飛行タイプにダメージ！"},
    {3, TRAP_HAZARD, HIT_ALL, 0, 20, "嫌な音", "全員の気力が減った！"},
    {4, TRAP_HAZARD, HIT_ALL, 23, 20, "不快な爆破", "全員にダメージ！|全員の気力が減った！"},
    {5, TRAP_HAZARD, HIT_WALK, 23, 20, "不快なヌメリ", "徒歩タイプにダメージ！|徒歩タイプの気力が減った！"},
    {6, TRAP_HAZARD, HIT_FLY, 23, 20, "不快な風", "飛行タイプにダメージ！|飛行タイプの気力が減った！"},
    {10, TRAP_BLIND, 0, 0, 0, "ブラインド罠", NULL},
    {12, TRAP_CURSE, 0, 10, 0, "防御力低下罠", NULL},
    {13, TRAP_CURSE, 1, 20, 0, "回避力低下罠", NULL},
    {20, TRAP_MAP, 0, 0, 0, "地図破壊罠", NULL},
    {21, TRAP_RESIDENTS, 0, 0, 0, "大警告罠", "このフロアから住民の気配が消えた！"},
    {22, TRAP_SUPPORT, 0, 0, 0, "サポート分離罠", "しばらくスペシャルギュンギュスカーが使えない！"},
    {23, TRAP_PURGE, 0, 0, 0, "浄化罠", "スペシャルギュンギュスカー効果が消えた！"},
    {24, TRAP_SGP, 1, 0, 0, "格減らし罠", "スペシャルギュンギュスカーで利用できる格が1減った！"},
    {25, TRAP_SGP, 2, 0, 0, "格枯らし罠", "スペシャルギュンギュスカーで利用できる格が2減った！"},
    {30, TRAP_ALARM, 0, 0, 0, "警報", NULL},
    {32, TRAP_PHANTOM, 0, 0, 0, "幻影罠", NULL},
    {33, TRAP_SPIN, 0, 0, 0, NULL, NULL},
    {39, TRAP_TELEPORT, 0, 0, 0, NULL, "空間が歪み、運ばれた！"},
};

static void notice(SA *s, const char *text) {
    Expedition *e = &s->exp;
    e->noticeCount = 0;
    for (const char *p = text; p && e->noticeCount < EXP_NOTICE_LINES;) {
        const char *bar = strchr(p, '|');
        size_t n = bar ? (size_t)(bar - p) : strlen(p);
        if (n >= sizeof e->notice[0]) n = sizeof e->notice[0] - 1;
        memcpy(e->notice[e->noticeCount], p, n);
        e->notice[e->noticeCount++][n] = 0;
        p = bar ? bar + 1 : NULL;
    }
}

static bool party_has(SA *s, const char *ability) {
    for (int i = 0; i < PARTY_SLOTS; i++)
        if (s->party.unit[i] && !strcmp(sa_table_text(&s->party.units, sa_party_unit_row(&s->party, s->party.unit[i]), 34), ability)) return true;
    for (int i = 0; i < s->party.benchCount; i++)
        if (s->party.bench[i].tile >= 0 && !strcmp(sa_table_text(&s->party.units, sa_party_unit_row(&s->party, s->party.bench[i].unit), 35), ability))
            return true;
    return false;
}

static void hurt(SA *s, int who, int hpPercent, int mpPercent) {
    Member ms[PARTY_SLOTS + PARTY_BENCH];
    int n = sa_party_member_list(&s->party, ms);
    for (int i = 0; i < n; i++) {
        if (ms[i].slot < 0 && (ms[i].bench < 0 || s->party.bench[ms[i].bench].tile < 0)) continue;
        bool flying = sa_table_int(&s->party.units, sa_party_unit_row(&s->party, ms[i].unit), UNIT_FLYING) == 1;
        if (!(who & (flying ? HIT_FLY : HIT_WALK))) continue;
        int hp = *ms[i].hp - sa_party_member_value(&s->party, &ms[i], 0) * hpPercent / 100;
        int mp = *ms[i].mp - sa_party_member_value(&s->party, &ms[i], 1) * mpPercent / 100;
        *ms[i].hp = hp > 1 ? hp : 1;
        *ms[i].mp = mp > 0 ? mp : 0;
    }
}

static void forget_area(SA *s) {
    Dungeon *d = &s->dungeon;
    if (!d->visited) return;
    memset(d->visited, 0, sizeof(bool) * DUN_CELLS);
    d->visited[d->y * DUN_SIDE + d->x] = true;
}

static void sync_status(SA *s) {
    Dungeon *d = &s->dungeon;
    d->statusCount = 0;
    d->blind = s->exp.blindSteps > 0;
    if (s->exp.blindSteps > 0) d->status[d->statusCount++] = (DunStatus){"ブラインド", s->exp.blindSteps};
    if (s->exp.supportSteps > 0) d->status[d->statusCount++] = (DunStatus){"ＳＰ分断", s->exp.supportSteps};
}

static void count_down_traps(SA *s) {
    if (s->exp.blindSteps > 0) s->exp.blindSteps--;
    if (s->exp.supportSteps > 0) s->exp.supportSteps--;
    sync_status(s);
}

bool sa_exp_support_cut(const SA *s) { return s->exp.active && s->exp.supportSteps > 0; }

static bool spring_trap(SA *s) {
    Expedition *e = &s->exp;
    Dungeon *d = &s->dungeon;
    int object = sa_dungeon_object(d), t = -1;
    for (size_t i = 0; i < sizeof traps / sizeof traps[0] && t < 0; i++)
        if (traps[i].object == object) t = (int)i;
    if (t < 0) return false;
    int kind = traps[t].kind;
    char msg[256];
    if (kind == TRAP_SPIN) {
        sa_dungeon_place(d, d->x, d->y, (int)(sa_vm_random(&s->vm) % 4));
        sync_position(s);
        return false;
    }
    sa_dungeon_reveal(d);
    if (kind == TRAP_TELEPORT) {
        DunLink l;
        if (!sa_dungeon_link(d, DUN_LINK_TELEPORT, &l)) return false;
        bool away = l.target >= 0 && (l.target >> 8 & 0xff) != e->floor;
        follow_link(s, &l);
        notice(s, traps[t].line);
        return away;
    }
    if (kind != TRAP_HAZARD) {
        int avoid = sa_exp_special_active(s, SPECIAL_NO_TRAP) ? 100 : party_has(s, "罠発見") ? AVOID_FINDER_PERCENT : AVOID_PERCENT;
        if ((int)(sa_vm_random(&s->vm) % 100) < avoid) {
            snprintf(msg, sizeof msg, "%sを回避した", traps[t].name);
            notice(s, msg);
            return false;
        }
    }
    snprintf(msg, sizeof msg, "%sに引っかかった！%s%s", traps[t].name, traps[t].line ? "|" : "", traps[t].line ? traps[t].line : "");
    notice(s, msg);
    switch (kind) {
    case TRAP_HAZARD:
        hurt(s, traps[t].arg, traps[t].hp, traps[t].mp);
        break;
    case TRAP_BLIND:
        e->blindSteps = TRAP_STEPS;
        break;
    case TRAP_MAP:
        forget_area(s);
        break;
    case TRAP_RESIDENTS:
        e->noResidents = true;
        break;
    case TRAP_SUPPORT:
        e->supportSteps = TRAP_STEPS;
        break;
    case TRAP_PURGE:
        memset(e->special, 0, sizeof e->special);
        break;
    case TRAP_SGP:
        s->vm.c[C_SGP] = s->vm.c[C_SGP] > traps[t].arg ? s->vm.c[C_SGP] - traps[t].arg : 0;
        break;
    case TRAP_ALARM:
        e->alarm = true;
        e->stepsLeft /= 2;
        break;
    case TRAP_PHANTOM: {
        char group[64];
        pick_group(s, group, sizeof group);
        spawn_group(s, group);
        sync_marks(s);
        break;
    }
    case TRAP_CURSE:
        e->curse[traps[t].arg] = traps[t].hp;
        break;
    }
    sync_status(s);
    return false;
}

static bool run_step_hook(SA *s);

static void after_move(SA *s, int r) {
    Dungeon *d = &s->dungeon;
    if (r == DUN_REST) {
        sa_camp_open(s);
        return;
    }
    if (r == DUN_ACTION) {
        int act = sa_dungeon_action(d);
        if (act == DUN_ACT_HOME) go_home(s);
        else if (act == DUN_ACT_OPEN) open_crystal(s);
        else change_floor(s);
        return;
    }
    if (d->bumped) {
        d->bumped = false;
        sync_position(s);
        s->exp.stepHook = true;
        return;
    }
    if (r == DUN_MOVED || r == DUN_TURNED) {
        s->vm.c[C_ALL_ACTIONS]++;
        s->vm.c[C_EXP_ACTIONS]++;
        s->vm.c[C_AREA_ACTIONS]++;
    }
    if (r != DUN_MOVED) return;
    Expedition *e = &s->exp;
    s->vm.c[C_ALL_STEPS]++;
    e->steps++;
    sync_steps(s);
    sync_position(s);
    int side = sa_dungeon_passage_side(d);
    if (side >= 0) {
        cross_area(s, side);
        return;
    }
    count_down_traps(s);
    if (spring_trap(s)) return;
    int hit = group_here(s);
    for (int i = 0; i < e->groupCount && hit < 0; i++) {
        ExpGroup *g = &e->group[i];
        if (!g->alive) continue;
        if (g->cooldown > 0) g->cooldown--;
        move_group(s, g);
        if (!g->cooldown && g->x == d->x && g->y == d->y) hit = i;
    }
    sync_marks(s);
    if (hit >= 0) {
        start_battle(s, hit);
        return;
    }
    bool calm = sa_exp_special_active(s, SPECIAL_NO_ENEMY) && (int)(sa_vm_random(&s->vm) % 100) >= sa_exp_special_value(s, SPECIAL_NO_ENEMY);
    count_down_specials(e);
    if (e->steps >= ENCOUNTER_FROM && --e->stepsLeft <= 0) {
        if (calm) reset_countdown(s);
        else {
            random_battle(s);
            return;
        }
    }
    e->stepHook = true;
}

static bool run_step_hook(SA *s) {
    if (!s->exp.stepHook || s->dungeon.animDur) return false;
    s->exp.stepHook = false;
    static const int hooks[] = {HOOK_STEP};
    run_hooks(s, hooks, 1, AFTER_WALK);
    return true;
}

void sa_exp_key(SA *s, int key) {
    if (s->mode == MODE_DUNGEON && s->exp.noticeCount) s->exp.noticeCount = 0;
    else if (s->mode == MODE_DUNGEON && !s->dungeon.animDur && !s->exp.stepHook) after_move(s, sa_dungeon_key(&s->dungeon, key));
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
                int rows[EXP_FLOORS], n = open_entries(s, s->exp.picked, rows);
                if (h - HOVER_AREA - 1 < n) sa_exp_enter(s, s->exp.picked, sa_table_int(&s->exp.floors, rows[h - HOVER_AREA - 1], F_FLOOR));
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
    if (run_step_hook(s)) return true;
    if (sa_party_tick(&s->party, ms)) s->dirty = true;
    if (sa_dungeon_hover(d, s->mouseX, s->mouseY)) s->dirty = true;
    s->rightClicked = false;
    if (s->clicked) {
        s->clicked = false;
        if (s->exp.noticeCount) s->exp.noticeCount = 0;
        else if (!d->animDur && !s->exp.stepHook) after_move(s, sa_dungeon_click(d, s->mouseX, s->mouseY));
        s->dirty = true;
    }
    if (s->mode == MODE_DUNGEON && s->dirty) {
        sa_exp_draw_view(s, s->fb);
        s->serial++;
        s->dirty = false;
    }
    return true;
}

void sa_exp_force_battle(SA *s) {
    if (s->mode != MODE_DUNGEON || !s->exp.active) return;
    for (int i = 0; i < s->exp.groupCount; i++)
        if (s->exp.group[i].alive) {
            start_battle(s, i);
            return;
        }
}

void sa_exp_debug_place(SA *s, int x, int y, int dir) {
    if (!s->exp.active) return;
    sa_dungeon_place(&s->dungeon, x, y, dir);
    sync_position(s);
    if (sa_dungeon_object(&s->dungeon) >= 0) {
        static const int hooks[] = {HOOK_STEP};
        run_hooks(s, hooks, 1, AFTER_WALK);
    }
    s->dirty = true;
}

int sa_exp_floors_reached(const SA *s, int dungeon) {
    int rows[EXP_FLOORS];
    return open_entries(s, dungeon, rows);
}

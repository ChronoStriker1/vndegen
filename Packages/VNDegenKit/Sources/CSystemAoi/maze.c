#include "sa_private.h"

enum {
    SHAFT_X = 225,
    SHAFT_Y = 121,
    TUNNEL_X = 244,
    TUNNEL_DY = 29,
    BAR_X = 54,
    BAR_Y = 4,
    TAB_X = 68,
    TAB_Y = 8,
    TAB_DX = 63,
    TAB_W = 60,
    TAB_H = 21,
    RECENT_X = 30,
    RECENT_Y = 374,
    BACK_X = 1117,
    BACK_Y = 675,
    BACK_W = 152,
    BACK_H = 44,
    DIGIT_W = 10,
    DIGIT_H = 13,
    HOVER_BACK = 1,
    HOVER_TAB = 2,
    THROUGH_X = 362,
    THROUGH_Y = 564,
};

enum { ART_BG, ART_RECENT, ART_BACK, ART_DIGITS, ART_BAR, ART_TAB_ROLES, ART_TAB_RECENT, ART_THROUGH, ART_COUNT };
enum { PIECE_FLOOR, PIECE_STAIRS, PIECE_TUNNEL, PIECE_SHAFT };

static const struct {
    int y, stairsX, stairsY;
    int x[MAZE_SIZES];
} floorAt[MAZE_FLOORS] = {
    {162, 914, 120, {828, 828, 638}},
    {242, 977, 167, {799, 531}},
    {312, 1040, 247, {801, 801, 629, 445}},
    {382, 833, 317, {790, 632, 632, 475, 326}},
    {462, 941, 387, {727, 727, 412}},
    {542, 779, 467, {740, 583, 425, 425, 270}},
    {622, 930, 547, {813, 655, 493, 335, 170}},
};

enum { HOLDERS = 11, HOLDER_SCALE = 4, HOLDER_COLS = 5, HOLDER_ROWS = 4, HOLDER_FRAMES = 18, HOLDER_FRAME_MS = 100, HOLDER_DY = 13, HOLDER_MARGIN = 20 };
enum { RULE_FLOORS = 1, RULE_SIZE, RULE_MAX_SIZE, RULE_TURNS = 5, RULE_INTRUDERS, DEPTH_OFFSET = 42, THROUGH_SHARE = 400, FAME_OFFSET = 11, BASE_INTRUDERS = 8 };

static int rule(const SA *s, int id, int col) {
    const Table *t = &s->mazeView.rules;
    for (int r = 0; r < t->rows; r++)
        if (sa_table_int(t, r, 0) == id) return sa_table_int(t, r, col + 2);
    return 0;
}

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void sa_maze_reset(SA *s) {
    memset(&s->maze, 0, sizeof s->maze);
    s->vm.c[C_MAZE_FLOORS] = 2;
    for (int k = 0; k < 2; k++) s->vm.c[C_MAZE_SIZE + k] = 1;
    sa_maze_update(s);
}

void sa_maze_init(SA *s) {
    MazeView *v = &s->mazeView;
    memset(v, 0, sizeof *v);
    static const char *const names[ART_COUNT] = {"dmap00.agf", "dmz_b00.agf", "dmz_c02.agf", "dmz_d00.agf", "dmz_e00.agf", "dmz_e01.agf", "dmz_e02.agf", "dmz_f00.agf"};
    for (int i = 0; i < ART_COUNT; i++) v->art[i] = sa_load_image(&s->vfs, names[i]);
    sa_table_load(&v->rules, &s->vfs, "maze.obj");
    sa_table_load(&v->areas, &s->vfs, "dmappos.obj");
    s->town.maze = &s->maze;
    sa_maze_reset(s);
}

void sa_maze_free(SA *s) {
    MazeView *v = &s->mazeView;
    for (int i = 0; i < ART_COUNT; i++) sa_image_free(v->art[i]);
    sa_table_free(&v->rules);
    sa_table_free(&v->areas);
    for (int i = 0; i < 13; i++) sa_image_free(v->icon[i]);
    for (int i = 0; i < HOLDERS; i++) sa_image_free(v->holder[i]);
    for (int k = 0; k < MAZE_FLOORS; k++)
        for (int z = 0; z < MAZE_SIZES; z++)
            for (int p = 0; p < 4; p++) sa_image_free(v->piece[k][z][p]);
    memset(v, 0, sizeof *v);
}

bool sa_maze_post_open(const Maze *m, const VM *vm, int post) {
    if (post >= 1 && post <= 9) return vm->f[F_POSTS] != 0;
    if (!vm->f[F_MAZE] || !m) return false;
    if (post == 10) return true;
    if (post == 11) return m->floors >= 5;
    if (post == 19) return m->floors >= 6;
    if (post == 20) return m->floors >= 4;
    return post >= 12 && post <= 18 && post - 11 <= m->floors;
}

enum { POST_COMMERCE = 4, POST_INDUSTRY = 5, POST_GUARD = 6, POST_SCHOOL = 7, POST_HEALTH = 9, POST_HEAD = 10, POST_DEPUTY = 11, POST_FLOOR1 = 12, POST_RAID = 19, POST_SUPPORT = 20, EXP_PER_INTRUDER = 2, PARTY_SIZE = 3 };
enum { COL_MAZE = 10, COL_GUARD, COL_CHEST, COL_TRAP, COL_RENOWN };

static void post_stats(const SA *s, int post, int *gov, int *lead) {
    *gov = *lead = 0;
    int unit = s->party.post[post];
    if (unit) sa_party_post_stats(&s->party, unit, post, gov, lead);
}

static int fame_of(int renown) {
    static const int steps[] = {50, 100, 150, 270, 370, 470};
    int v = renown + FAME_OFFSET, level = 1;
    for (int i = 0; i < 6 && v >= steps[i]; i++) level++;
    return level;
}

static int mark_of(int v) {
    int m = 0;
    for (int step = 300; m < 4 && v >= step; step *= 2) m++;
    return m;
}

void sa_maze_update(SA *s) {
    Maze *m = &s->maze;
    m->floors = clamp(s->vm.c[C_MAZE_FLOORS], 1, MAZE_FLOORS);
    for (int k = 0; k < MAZE_FLOORS; k++) {
        int most = rule(s, RULE_MAX_SIZE, k + 2);
        s->mazeView.size[k] = clamp(s->vm.c[C_MAZE_SIZE + k], 1, most > 0 ? most : MAZE_SIZES);
    }
    m->intruders = s->vm.c[C_MAZE_INTRUDERS];
    m->food = s->vm.c[C_MAZE_FOOD];
    m->money = s->vm.c[C_MAZE_FOOD + 1];
    m->resources = s->vm.c[C_MAZE_FOOD + 2];
    const Town *t = &s->town;
    int sum[5] = {0};
    for (int c = 0; c < TOWN_CELLS; c++) {
        int r = t->cell[c].type ? sa_town_type_row(t, t->cell[c].type) : -1;
        if (r < 0 || !sa_town_staffed(t, &s->vm, c)) continue;
        for (int i = 0; i < 5; i++) sum[i] += sa_town_value(t, r, t->cell[c].level, COL_MAZE + i);
    }
    m->depthPoints = sum[0];
    m->renown = sum[4];
    m->fame = fame_of(m->renown);
    int guard = sum[COL_GUARD - COL_MAZE], chest = sum[COL_CHEST - COL_MAZE], trap = sum[COL_TRAP - COL_MAZE];
    int gov, lead;
    post_stats(s, POST_COMMERCE, &gov, &lead);
    chest += gov;
    post_stats(s, POST_INDUSTRY, &gov, &lead);
    trap += gov;
    post_stats(s, POST_GUARD, &gov, &lead);
    guard += gov;
    trap += gov;
    post_stats(s, POST_SCHOOL, &gov, &lead);
    guard += gov;
    post_stats(s, POST_HEALTH, &gov, &lead);
    guard += gov;
    m->townGuard = guard;
    m->townTrap = trap;
    m->townChest = chest;
    int heads = 0, chief = 0;
    for (int p = POST_HEAD; p <= POST_DEPUTY; p++) {
        post_stats(s, p, &gov, &lead);
        trap += lead;
        chest += 3 * gov;
        chief += lead;
    }
    int floors = m->floors;
    for (int k = 0; k < floors; k++) {
        post_stats(s, POST_FLOOR1 + k, &gov, &lead);
        heads += lead;
    }
    m->guards = 12 * floors * (10 * guard + 15 * heads) / 100;
    m->traps = 3 * floors * trap;
    m->chests = 15 * floors * chest / 4;
    m->passages = floors * (10 * chief + 15 * heads);
    const int v[4] = {m->guards, m->passages, m->traps, m->chests};
    for (int i = 0; i < 4; i++) m->marks[i] = mark_of(v[i]);
}

static const Image *piece(SA *s, int floor, int size, int part) {
    MazeView *v = &s->mazeView;
    if (floor < 0 || floor >= MAZE_FLOORS || size < 1 || size > MAZE_SIZES) return NULL;
    Image **slot = &v->piece[floor][size - 1][part];
    if (!*slot) {
        char n[24];
        snprintf(n, sizeof n, "d%d_%d%c.agf", floor + 1, size, "fsxy"[part]);
        *slot = sa_load_image(&s->vfs, n);
    }
    return *slot;
}

enum { ICON_LOST = 9, ICON_FOUND_WAY = 10, ICON_EXIT = 11, EXIT_DX = 60, EVENT_LOST = 500, RESULT_WIN = 10 };

static int rule_row(const SA *s, int id) {
    const Table *t = &s->mazeView.rules;
    for (int r = 0; r < t->rows; r++)
        if (sa_table_int(t, r, 0) == id) return r;
    return -1;
}

static void grow(SA *s) {
    VM *vm = &s->vm;
    Maze *m = &s->maze;
    int points = m->depthPoints + vm->c[C_RESIDENTS] / 2 - DEPTH_OFFSET, since = vm->c[C_TURN] - vm->c[C_MAZE_OPENED];
    while (vm->c[C_MAZE_FLOORS] < MAZE_FLOORS) {
        int next = vm->c[C_MAZE_FLOORS] + 1, need = rule(s, RULE_FLOORS, next + 1);
        if (need <= 0 || since < rule(s, RULE_TURNS, next + 1) || points < need) break;
        vm->c[C_MAZE_FLOORS] = next;
        if (vm->c[C_MAZE_SIZE + next - 1] < 1) vm->c[C_MAZE_SIZE + next - 1] = 1;
    }
    for (int k = 0; k < vm->c[C_MAZE_FLOORS]; k++) {
        int gov, lead;
        post_stats(s, POST_FLOOR1 + k, &gov, &lead);
        vm->c[C_MAZE_WIDTH + k] += lead;
        int size = 1, most = rule(s, RULE_MAX_SIZE, k + 2);
        for (int z = 2; z <= MAZE_SIZES; z++) {
            int step = rule(s, RULE_SIZE, z + 1);
            if (step > 0 && vm->c[C_MAZE_WIDTH + k] >= step) size = z;
        }
        vm->c[C_MAZE_SIZE + k] = size < most || most <= 0 ? size : most;
    }
}

static int roll(SA *s, int n);

static int intruders(SA *s) {
    int value = s->maze.renown + FAME_OFFSET, n = BASE_INTRUDERS;
    int chance = value / 2 + 5 < 90 ? value / 2 + 5 : 90;
    if (value > 0 && roll(s, 100) < chance) n += 1 + roll(s, 1 + value / 50);
    int most = rule(s, RULE_INTRUDERS, s->maze.floors + 1);
    return most > 0 && n > most ? most : n;
}

static int roll(SA *s, int n) { return n > 0 ? (int)(sa_vm_random(&s->vm) % (u32)n) : 0; }

static int pick(SA *s, const int *weight, int count) {
    int total = 0;
    for (int i = 0; i < count; i++) total += weight[i] > 0 ? weight[i] : 0;
    int r = roll(s, total);
    for (int i = 0; i < count; i++) {
        if (weight[i] <= 0) continue;
        if (r < weight[i]) return i;
        r -= weight[i];
    }
    return -1;
}

static int holder_lead(const SA *s, int post) {
    int gov, lead;
    post_stats(s, post, &gov, &lead);
    return s->party.post[post] && sa_maze_post_open(&s->maze, &s->vm, post) ? lead : -1;
}

static int holder_gov(const SA *s, int post) {
    int gov, lead;
    post_stats(s, post, &gov, &lead);
    return s->party.post[post] && sa_maze_post_open(&s->maze, &s->vm, post) ? gov : -1;
}

enum { KIND_BATTLE, KIND_TRAP, KIND_CHEST, KIND_LOST, KINDS };

static void resolve(SA *s, int floor, int left, int *e) {
    const Maze *m = &s->maze;
    int head = holder_lead(s, POST_FLOOR1 + floor), chief = holder_lead(s, POST_HEAD), deputy = holder_lead(s, POST_DEPUTY);
    int raid = holder_lead(s, POST_RAID), support = holder_lead(s, POST_SUPPORT);
    int w[KINDS] = {20 * m->townGuard + (head >= 0 ? 18 * head : 0), 20 * m->townTrap + (chief >= 0 ? 300 : 0) + (deputy >= 0 ? 300 : 0),
                    20 * m->townChest + (chief >= 0 ? 200 : 0) + (deputy >= 0 ? 200 : 0),
                    (chief >= 0 ? 180 + 4 * chief : 0) + (head >= 0 ? 15 * head : 0)};
    int total = 0;
    for (int i = 0; i < KINDS; i++) total += w[i];
    e[0] = -1;
    int r = roll(s, total > 2000 ? total : 2000), kind = -1;
    for (int i = 0; i < KINDS && kind < 0; i++) {
        if (r < w[i]) kind = i;
        r -= w[i];
    }
    if (kind < 0) return;
    int id = 0, all = 0;
    if (kind == KIND_BATTLE) {
        static const int ids[] = {200, 201, 202, 203, 204, 205, 206};
        int bw[7] = {head < 0 ? 4 * m->townGuard : 0, head >= 0 ? 3 * head : 0, raid >= 0 ? 3 * raid : 0, support >= 0 ? 3 * support : 0, raid >= 0 ? 10 : 0, 150,
                     head < 0 ? 100 : 0};
        int i = pick(s, bw, 7);
        id = ids[i < 0 ? 5 : i];
        all = i >= 1 && i <= 4;
    } else if (kind == KIND_TRAP) {
        static const int ids[] = {400, 401, 402, 403};
        int tw[4] = {6 * m->townTrap, support >= 0 ? 3 * support : 0, head >= 0 ? 3 * head : 0, 100};
        int i = pick(s, tw, 4);
        id = ids[i < 0 ? 3 : i];
    } else if (kind == KIND_CHEST) {
        static const int ids[] = {300, 301, 302, 303, 304, 305, 306, 307, 308};
        int govChief = holder_gov(s, POST_HEAD), govDeputy = holder_gov(s, POST_DEPUTY);
        int cw[9] = {3 * m->townChest, chief >= 0 ? 30 + govChief : 0, deputy >= 0 ? 30 + govDeputy : 0, 10, chief < 0 ? 30 : 0, 10, chief < 0 ? 30 : 0, 10, chief < 0 ? 30 : 0};
        int i = pick(s, cw, 9);
        id = ids[i < 0 ? 3 : i];
        all = i == 1 || i == 2 || (i >= 3 && chief >= 0);
    } else
        id = roll(s, 108) < 105 ? EVENT_LOST : -EVENT_LOST;
    e[0] = id < 0 ? -id : id;
    int row = rule_row(s, e[0]);
    int result = row >= 0 ? rule_row(s, sa_table_int(&s->mazeView.rules, row, 3)) : -1;
    bool takes = id > 0 && result >= 0 && (sa_table_int(&s->mazeView.rules, result, 2) == RESULT_WIN || kind == KIND_CHEST);
    e[1] = takes ? (all ? left : left < PARTY_SIZE ? left : PARTY_SIZE) : 0;
    e[2] = 0;
    if (result < 0 || !takes) return;
    const Table *t = &s->mazeView.rules;
    for (int i = 0; i < 3; i++) {
        int lo = sa_table_int(t, result, 7 + 2 * i), hi = sa_table_int(t, result, 8 + 2 * i);
        int v = hi > lo ? lo + roll(s, hi - lo + 1) : lo;
        if (v && kind == KIND_BATTLE && i == 1) v += (m->townGuard + 19) / 20;
        if (v && kind == KIND_TRAP && i == 2) v += (m->townTrap + 9) / 10;
        e[3 + i] = sa_table_int(t, result, 2) == RESULT_WIN ? v : -v;
    }
}

static void give_exp(SA *s, int unit, int amount) {
    Party *p = &s->party;
    int *exp = NULL, *level = NULL;
    for (int i = 0; i < PARTY_SLOTS && !exp; i++)
        if (p->unit[i] == unit) exp = &p->exp[i], level = &p->level[i];
    for (int i = 0; i < p->benchCount && !exp; i++)
        if (p->bench[i].unit == unit) exp = &p->bench[i].exp, level = &p->bench[i].level;
    int r = sa_party_unit_row(p, unit);
    if (!exp || r < 0) return;
    *exp += amount;
    int need;
    while ((need = sa_battle_exp_needed(&s->battle, r, *level)) > 0 && *exp >= need) {
        *exp -= need;
        ++*level;
    }
}

bool sa_maze_turn(SA *s, int net[3]) {
    VM *vm = &s->vm;
    Maze *m = &s->maze;
    memset(net, 0, 3 * sizeof *net);
    if (!vm->f[F_MAZE]) return false;
    sa_maze_update(s);
    if (!vm->c[C_MAZE_OPENED]) vm->c[C_MAZE_OPENED] = vm->c[C_TURN];
    memset(m->event, 0, sizeof m->event);
    int n = intruders(s), left = n;
    for (int k = 0; k < m->floors && left > 0; k++)
        for (int a = 0; a < s->mazeView.size[k] && left > 0; a++) {
            int *e = m->event[k][a];
            resolve(s, k, left, e);
            if (e[1] > left) e[1] = left;
            left -= e[1];
            for (int i = 0; i < 3; i++) net[i] += e[3 + i];
        }
    for (int i = 0; i < 3 && left > 0; i++) {
        long long stock = vm->c[C_FOOD + i];
        if (stock > 0) net[i] -= (int)((stock * left + THROUGH_SHARE - 1) / THROUGH_SHARE);
    }
    vm->c[C_MAZE_INTRUDERS] = n;
    vm->c[C_MAZE_REPELLED] = n - left;
    vm->c[C_MAZE_THROUGH] = left;
    for (int i = 0; i < 3; i++) vm->c[C_MAZE_FOOD + i] = net[i];
    for (int post = POST_HEAD; post <= POST_SUPPORT; post++)
        if (s->party.post[post] && sa_maze_post_open(m, vm, post)) give_exp(s, s->party.post[post], EXP_PER_INTRUDER * n);
    grow(s);
    sa_maze_update(s);
    return left > 0;
}

static int event_icon(const SA *s, int id, int people) {
    if (id == EVENT_LOST) return people > 0 ? ICON_LOST : ICON_FOUND_WAY;
    int r = rule_row(s, id);
    int result = r >= 0 ? rule_row(s, sa_table_int(&s->mazeView.rules, r, 3)) : -1;
    return result >= 0 ? sa_table_int(&s->mazeView.rules, result, 3) : -1;
}

static const Image *icon(SA *s, int i) {
    MazeView *v = &s->mazeView;
    if (i < 0 || i >= 12) return NULL;
    if (!v->icon[i]) {
        char n[16];
        snprintf(n, sizeof n, "d_ev%03d.agf", i);
        v->icon[i] = sa_load_image(&s->vfs, n);
    }
    return v->icon[i];
}

static bool area_at(const SA *s, int floor, int area, int *x, int *y) {
    const Table *t = &s->mazeView.areas;
    for (int r = 0; r < t->rows; r++) {
        if (sa_table_int(t, r, 0) != floor + 1 || 1 + area * 2 + 1 >= t->cols) continue;
        *x = sa_table_int(t, r, 1 + area * 2);
        *y = sa_table_int(t, r, 2 + area * 2);
        return *x > 0;
    }
    return false;
}

void sa_maze_draw_scene(SA *s, u32 *fb) {
    MazeView *v = &s->mazeView;
    sa_blit(fb, v->art[ART_BG], 0, 0);
    int floors = s->maze.floors;
    if (floors <= 0) return;
    int deep = floors - 1, size = v->size[deep];
    sa_blit(fb, piece(s, deep, size, PIECE_SHAFT), SHAFT_X, SHAFT_Y);
    sa_blit(fb, piece(s, deep, size, PIECE_TUNNEL), TUNNEL_X, floorAt[deep].y + TUNNEL_DY);
    for (int k = 0; k < floors; k++) {
        sa_blit(fb, piece(s, k, v->size[k], PIECE_FLOOR), floorAt[k].x[v->size[k] - 1], floorAt[k].y);
    }
    for (int k = 0; k < floors; k++) sa_blit(fb, piece(s, k, v->size[k], PIECE_STAIRS), floorAt[k].stairsX, floorAt[k].stairsY);
    for (int k = 0; k < floors; k++)
        for (int a = 0; a < v->size[k]; a++) {
            const int *e = s->maze.event[k][a];
            if (e[0] <= 0) continue;
            int x, y;
            if (area_at(s, k, a, &x, &y)) sa_blit(fb, icon(s, event_icon(s, e[0], e[1])), x, y);
        }
    int ax, ay;
    if (s->vm.c[C_MAZE_THROUGH] > 0 && area_at(s, deep, 0, &ax, &ay)) sa_blit(fb, icon(s, ICON_EXIT), floorAt[deep].x[size - 1] - EXIT_DX, ay);
}

static Image *scale_frames(const Image *sheet) {
    int fw = sheet->w / HOLDER_COLS, fh = sheet->h / HOLDER_ROWS, w = fw / HOLDER_SCALE, h = fh / HOLDER_SCALE;
    Image *out = sa_image_new(w * HOLDER_COLS, h * HOLDER_ROWS), *frame = sa_image_new(fw, fh);
    for (int f = 0; f < HOLDER_COLS * HOLDER_ROWS; f++) {
        int sx = f % HOLDER_COLS * sheet->w / HOLDER_COLS, sy = f / HOLDER_COLS * fh;
        for (int y = 0; y < fh; y++) memcpy(frame->px + (size_t)y * fw, sheet->px + (size_t)(sy + y) * sheet->w + sx, (size_t)fw * 4);
        Image *small = sa_image_scale(frame, w, h);
        for (int y = 0; y < h; y++) memcpy(out->px + (size_t)(f / HOLDER_COLS * h + y) * out->w + f % HOLDER_COLS * w, small->px + (size_t)y * w, (size_t)w * 4);
        sa_image_free(small);
    }
    sa_image_free(frame);
    return out;
}

static void place_holders(SA *s) {
    MazeView *v = &s->mazeView;
    for (int i = 0; i < HOLDERS; i++) {
        sa_image_free(v->holder[i]);
        v->holder[i] = NULL;
        int post = POST_HEAD + i, unit = s->party.post[post];
        if (!unit || !sa_maze_post_open(&s->maze, &s->vm, post)) continue;
        int r = sa_party_unit_row(&s->party, unit);
        if (r < 0) continue;
        char name[24];
        snprintf(name, sizeof name, "u%04de0.agf", sa_table_int(&s->party.units, r, 11));
        Image *sheet = sa_load_image(&s->vfs, name);
        if (!sheet) continue;
        v->holder[i] = scale_frames(sheet);
        sa_image_free(sheet);
        int k = post >= POST_FLOOR1 && post < POST_FLOOR1 + MAZE_FLOORS ? post - POST_FLOOR1 : s->maze.floors - 1;
        const Image *f = piece(s, k, v->size[k], PIECE_FLOOR);
        int left = floorAt[k].x[v->size[k] - 1], span = f ? f->w - 2 * HOLDER_MARGIN : 0;
        v->holderFloor[i] = k;
        v->holderX[i] = left + HOLDER_MARGIN + (span > 0 ? (int)(sa_vm_random(&s->vm) % (u32)span) : 0);
    }
}

static void draw_holders(SA *s, u32 *fb) {
    MazeView *v = &s->mazeView;
    for (int i = 0; i < HOLDERS; i++) {
        const Image *h = v->holder[i];
        if (!h) continue;
        int fw = h->w / HOLDER_COLS, fh = h->h / HOLDER_ROWS, frame = (int)(v->ms / HOLDER_FRAME_MS + (u32)i * 7) % HOLDER_FRAMES;
        sa_blit_rect(fb, h, frame % HOLDER_COLS * fw, frame / HOLDER_COLS * fh, fw, fh, v->holderX[i], floorAt[v->holderFloor[i]].y - HOLDER_DY);
    }
}

void sa_maze_open(SA *s) {
    place_holders(s);
    s->mazeView.hover = 0;
    s->mode = MODE_MAZE;
    s->dirty = true;
}

static bool in(int x, int y, int rx, int ry, int w, int h) { return x >= rx && x < rx + w && y >= ry && y < ry + h; }

static int hit(SA *s, int x, int y) {
    int w = sa_town_role_window_hit(&s->town, &s->vm, x, y);
    if (w >= 0) return -w - 10;
    if (in(x, y, BACK_X, BACK_Y, BACK_W, BACK_H)) return HOVER_BACK;
    for (int i = 0; i < 2; i++)
        if (in(x, y, TAB_X + i * TAB_DX, TAB_Y, TAB_W, TAB_H)) return HOVER_TAB + i;
    return 0;
}

static void number(SA *s, u32 *fb, int v, int right, int y, int row) {
    sa_draw_int(fb, s->mazeView.art[ART_DIGITS], v, false, RECENT_X + right, RECENT_Y + y, row, DIGIT_W, DIGIT_H);
}

enum { DIGIT_BLACK, DIGIT_RED, DIGIT_BLUE };

static bool event_won(const SA *s, const int *e) {
    if (e[0] == EVENT_LOST) return e[1] > 0;
    int r = rule_row(s, e[0]);
    int result = r >= 0 ? rule_row(s, sa_table_int(&s->mazeView.rules, r, 3)) : -1;
    return result >= 0 && sa_table_int(&s->mazeView.rules, result, 2) == RESULT_WIN;
}

static int event_kind(const SA *s, int id) {
    int r = rule_row(s, id);
    return r >= 0 ? sa_table_int(&s->mazeView.rules, r, 2) : 0;
}

void sa_maze_summary(const SA *s, Recent *r) {
    memset(r, 0, sizeof *r);
    const VM *vm = &s->vm;
    r->parties = vm->c[C_MAZE_INTRUDERS];
    r->repelled = vm->c[C_MAZE_REPELLED];
    r->broken = vm->c[C_MAZE_THROUGH];
    for (int k = 0; k < MAZE_FLOORS; k++)
        for (int a = 0; a < 7; a++) {
            const int *e = s->maze.event[k][a];
            if (e[0]) r->deepest = k + 1;
            if (e[0] <= 0) continue;
            int kind = event_kind(s, e[0]);
            bool won = event_won(s, e);
            int *pair = kind == 2 ? r->fights : kind == 3 ? r->chests : kind == 4 ? r->traps : kind == 5 ? r->lost : NULL;
            if (pair) {
                pair[0] += won;
                pair[1]++;
            }
            if (kind == 3 && !won) r->opened++;
            for (int i = 0; i < 3; i++) {
                if (won) r->flow[i][0] += e[3 + i];
                else r->flow[i][1] -= e[3 + i];
            }
        }
    if (r->broken > 0) r->deepest = s->maze.floors;
    for (int i = 0; i < 3; i++) {
        r->flow[i][3] = vm->c[C_MAZE_FOOD + i];
        r->flow[i][2] = r->flow[i][0] - r->flow[i][1] - r->flow[i][3];
    }
}

static void draw_recent(SA *s, u32 *fb) {
    Recent r;
    sa_maze_summary(s, &r);
    sa_blit(fb, s->mazeView.art[ART_RECENT], RECENT_X, RECENT_Y);
    number(s, fb, r.parties, 414, 55, DIGIT_BLACK);
    const int *pairs[4] = {r.fights, r.chests, r.traps, r.lost};
    for (int i = 0; i < 4; i++) {
        number(s, fb, pairs[i][0], 364, 75 + i * 20, DIGIT_BLACK);
        number(s, fb, pairs[i][1], 414, 75 + i * 20, DIGIT_BLACK);
    }
    number(s, fb, r.deepest, 414, 175, DIGIT_BLACK);
    if (r.broken > 0) sa_blit(fb, s->mazeView.art[ART_THROUGH], THROUGH_X, THROUGH_Y);
    const int counts[3] = {r.repelled, r.opened, r.broken};
    for (int i = 0; i < 3; i++) number(s, fb, counts[i], 144 + i * 80, 235, DIGIT_BLACK);
    for (int row = 0; row < 3; row++) {
        const int *f = r.flow[row];
        number(s, fb, f[0], 144, 255 + row * 20, f[0] > 0 ? DIGIT_BLUE : DIGIT_BLACK);
        for (int i = 1; i < 3; i++) number(s, fb, f[i], 144 + i * 80, 255 + row * 20, f[i] > 0 ? DIGIT_RED : DIGIT_BLACK);
        number(s, fb, f[3], 394, 255 + row * 20, f[3] < 0 ? DIGIT_RED : f[3] > 0 ? DIGIT_BLUE : DIGIT_BLACK);
    }
}

static void draw(SA *s) {
    MazeView *v = &s->mazeView;
    u32 *fb = s->fb;
    sa_maze_draw_scene(s, fb);
    draw_holders(s, fb);
    sa_blit(fb, v->art[ART_BAR], BAR_X, BAR_Y);
    const bool on[2] = {!s->town.shut[TW_MAZE_ROLES], !v->hideRecent};
    for (int i = 0; i < 2; i++)
        sa_blit_rect(fb, v->art[ART_TAB_ROLES + i], on[i] ? TAB_W : 0, v->hover == HOVER_TAB + i ? TAB_H : 0, TAB_W, TAB_H, TAB_X + i * TAB_DX, TAB_Y);
    if (!v->hideRecent) draw_recent(s, fb);
    s->town.hover = v->hover <= -10 ? -v->hover - 10 : -1;
    sa_town_draw_role_window(&s->town, &s->vm, fb);
    sa_blit_rect(fb, v->art[ART_BACK], 0, v->hover == HOVER_BACK ? BACK_H : 0, BACK_W, BACK_H, BACK_X, BACK_Y);
}

bool sa_maze_frame(SA *s, u32 elapsedMs) {
    if (s->mode != MODE_MAZE) return false;
    MazeView *v = &s->mazeView;
    u32 before = v->ms / HOLDER_FRAME_MS;
    v->ms += elapsedMs;
    for (int i = 0; i < HOLDERS; i++)
        if (v->holder[i] && v->ms / HOLDER_FRAME_MS != before) s->dirty = true;
    int h = hit(s, s->mouseX, s->mouseY);
    if (h != v->hover) {
        v->hover = h;
        s->dirty = true;
    }
    if (s->rightClicked) {
        s->rightClicked = false;
        s->mode = MODE_TOWN;
    }
    if (s->clicked) {
        s->clicked = false;
        if (h == HOVER_BACK) s->mode = MODE_TOWN;
        else if (h == HOVER_TAB) s->town.shut[TW_MAZE_ROLES] = !s->town.shut[TW_MAZE_ROLES];
        else if (h == HOVER_TAB + 1) v->hideRecent = !v->hideRecent;
        else if (h <= -10) sa_town_toggle(&s->town, -h - 10);
        s->dirty = true;
    }
    if (s->mode == MODE_MAZE && s->dirty) {
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

void sa_debug_maze(VNSystemAoi *sa, int turns) {
    SA *s = (SA *)sa;
    int saved[SA_C_VARS];
    memcpy(saved, s->vm.c, sizeof saved);
    int count[1000] = {0}, quiet = 0, people[1000][9] = {{0}}, outcome[9][9] = {{0}};
    for (int t = 0; t < turns; t++) {
        memcpy(s->vm.c, saved, sizeof saved);
        int net[3];
        sa_maze_turn(s, net);
        for (int k = 0; k < MAZE_FLOORS; k++)
            for (int a = 0; a < 7; a++) {
                const int *e = s->maze.event[k][a];
                if (e[0] == -1) quiet++;
                if (e[0] > 0 && e[0] < 1000) {
                    count[e[0]]++;
                    people[e[0]][e[1] < 8 ? e[1] : 8]++;
                }
            }
        int rep = s->vm.c[C_MAZE_REPELLED], thr = s->vm.c[C_MAZE_THROUGH];
        if (rep < 9 && thr < 9) outcome[rep][thr]++;
    }
    memcpy(s->vm.c, saved, sizeof saved);
    printf("maze %d turns quiet %d\n", turns, quiet);
    for (int i = 0; i < 1000; i++)
        if (count[i]) {
            printf("  %d %d people", i, count[i]);
            for (int p = 0; p < 9; p++)
                if (people[i][p]) printf(" %d:%d", p, people[i][p]);
            printf("\n");
        }
    for (int r = 0; r < 9; r++)
        for (int b = 0; b < 9; b++)
            if (outcome[r][b]) printf("  rep %d thr %d: %d\n", r, b, outcome[r][b]);
}

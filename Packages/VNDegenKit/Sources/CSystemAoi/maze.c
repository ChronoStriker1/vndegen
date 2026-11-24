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
};

enum { ART_BG, ART_RECENT, ART_BACK, ART_DIGITS, ART_BAR, ART_TAB_ROLES, ART_TAB_RECENT, ART_COUNT };
enum { PIECE_FLOOR, PIECE_STAIRS, PIECE_TUNNEL, PIECE_SHAFT };

static const struct {
    int y, right, stairsX, stairsY;
} floorAt[MAZE_FLOORS] = {{162, 1060, 914, 120}, {242, 1119, 977, 167}};

void sa_maze_reset(SA *s) {
    memset(&s->maze, 0, sizeof s->maze);
    s->maze.floors = 2;
    s->maze.fame = 1;
    memset(&s->mazeView.recent, 0, sizeof s->mazeView.recent);
    for (int k = 0; k < MAZE_FLOORS; k++) s->mazeView.size[k] = 1;
}

void sa_maze_init(SA *s) {
    MazeView *v = &s->mazeView;
    memset(v, 0, sizeof *v);
    static const char *const names[ART_COUNT] = {"dmap00.agf", "dmz_b00.agf", "dmz_c02.agf", "dmz_d00.agf", "dmz_e00.agf", "dmz_e01.agf", "dmz_e02.agf"};
    for (int i = 0; i < ART_COUNT; i++) v->art[i] = sa_load_image(&s->vfs, names[i]);
    s->town.maze = &s->maze;
    sa_maze_reset(s);
}

void sa_maze_free(SA *s) {
    MazeView *v = &s->mazeView;
    for (int i = 0; i < ART_COUNT; i++) sa_image_free(v->art[i]);
    for (int k = 0; k < MAZE_FLOORS; k++)
        for (int z = 0; z < MAZE_SIZES; z++)
            for (int p = 0; p < 4; p++) sa_image_free(v->piece[k][z][p]);
    memset(v, 0, sizeof *v);
}

bool sa_maze_post_open(const Maze *m, const VM *vm, int post) {
    if (post >= 1 && post <= 9) return vm->f[F_POSTS] != 0;
    if (!vm->f[F_MAZE] || !m) return false;
    if (post == 10) return true;
    return post >= 12 && post <= 18 && post - 11 <= m->floors;
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

void sa_maze_draw_scene(SA *s, u32 *fb) {
    MazeView *v = &s->mazeView;
    sa_blit(fb, v->art[ART_BG], 0, 0);
    int floors = s->maze.floors < MAZE_FLOORS ? s->maze.floors : MAZE_FLOORS;
    while (floors > 0 && !floorAt[floors - 1].right) floors--;
    if (floors <= 0) return;
    int deep = floors - 1, size = v->size[deep];
    sa_blit(fb, piece(s, deep, size, PIECE_SHAFT), SHAFT_X, SHAFT_Y);
    sa_blit(fb, piece(s, deep, size, PIECE_TUNNEL), TUNNEL_X, floorAt[deep].y + TUNNEL_DY);
    for (int k = 0; k < floors; k++) {
        const Image *f = piece(s, k, v->size[k], PIECE_FLOOR);
        if (f) sa_blit(fb, f, floorAt[k].right - f->w, floorAt[k].y);
    }
    for (int k = 0; k < floors; k++) sa_blit(fb, piece(s, k, v->size[k], PIECE_STAIRS), floorAt[k].stairsX, floorAt[k].stairsY);
}

void sa_maze_open(SA *s) {
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

static void number(SA *s, u32 *fb, int v, int right, int y) {
    sa_draw_int(fb, s->mazeView.art[ART_DIGITS], v, false, RECENT_X + right, RECENT_Y + y, 0, DIGIT_W, DIGIT_H);
}

static void draw_recent(SA *s, u32 *fb) {
    const Recent *r = &s->mazeView.recent;
    sa_blit(fb, s->mazeView.art[ART_RECENT], RECENT_X, RECENT_Y);
    number(s, fb, r->parties, 414, 55);
    const int *pairs[4] = {r->fights, r->chests, r->traps, r->lost};
    for (int i = 0; i < 4; i++) {
        number(s, fb, pairs[i][0], 364, 75 + i * 20);
        number(s, fb, pairs[i][1], 414, 75 + i * 20);
    }
    number(s, fb, r->deepest, 414, 175);
    const int counts[3] = {r->repelled, r->opened, r->broken};
    for (int i = 0; i < 3; i++) number(s, fb, counts[i], 144 + i * 80, 235);
    for (int row = 0; row < 3; row++) {
        for (int i = 0; i < 3; i++) number(s, fb, r->flow[row][i], 144 + i * 80, 255 + row * 20);
        number(s, fb, r->flow[row][3], 394, 255 + row * 20);
    }
}

static void draw(SA *s) {
    MazeView *v = &s->mazeView;
    u32 *fb = s->fb;
    sa_maze_draw_scene(s, fb);
    sa_blit(fb, v->art[ART_BAR], BAR_X, BAR_Y);
    const bool on[2] = {!s->town.shut[TW_MAZE_ROLES], !v->hideRecent};
    for (int i = 0; i < 2; i++)
        sa_blit_rect(fb, v->art[ART_TAB_ROLES + i], on[i] ? TAB_W : 0, v->hover == HOVER_TAB + i ? TAB_H : 0, TAB_W, TAB_H, TAB_X + i * TAB_DX, TAB_Y);
    if (!v->hideRecent) draw_recent(s, fb);
    s->town.hover = v->hover <= -10 ? -v->hover - 10 : -1;
    sa_town_draw_role_window(&s->town, &s->vm, fb);
    sa_blit_rect(fb, v->art[ART_BACK], 0, v->hover == HOVER_BACK ? BACK_H : 0, BACK_W, BACK_H, BACK_X, BACK_Y);
}

bool sa_maze_frame(SA *s) {
    if (s->mode != MODE_MAZE) return false;
    MazeView *v = &s->mazeView;
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

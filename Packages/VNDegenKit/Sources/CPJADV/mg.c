#include "pj.h"

#include <math.h>

enum { T_CLEANER, T_BRUSH, T_BROOM, T_MOP, T_CLOTH, TOOLS };
enum { D_CRAYON, D_MUSHROOM, D_BANANA, D_DUST, D_SLIME, DIRTS };
enum { PH_READY, PH_GO, PH_PLAY, PH_CLEAR, PH_TIMEUP, PH_RESULT, PH_DONE };

static const int toolForDirt[DIRTS] = {T_BRUSH, T_CLOTH, T_BROOM, T_CLEANER, T_MOP};
static const int dirtAnim[DIRTS] = {2, 5, 3, 1, 4};
static const int tips1Cell[TOOLS] = {0, 2, 3, 1, 4};
static const int nextCell[TOOLS][2] = {{0, 0}, {0, 254}, {254, 254}, {254, 0}, {0, 508}};

static const int digitRect[10][4] = {
    {131, 255, 67, 84}, {19, 390, 35, 80}, {73, 385, 71, 86}, {151, 383, 59, 87}, {130, 509, 69, 88},
    {5, 637, 63, 83},   {78, 638, 61, 87}, {152, 639, 56, 81}, {259, 130, 63, 85}, {260, 384, 61, 87},
};
static const int digitAt[10][2] = {
    {1790, 43}, {1806, 50}, {1788, 45}, {1794, 44}, {1789, 43}, {1792, 43}, {1793, 44}, {1795, 45}, {1792, 44}, {1793, 44},
};

#define GRID_X 281
#define GRID_Y 188
#define GRID_DX 110
#define GRID_DY 120
#define GRID_COLS 14
#define GRID_ROWS 7
#define MAX_ICONS 256
#define MAX_FX 48
#define HIT 75
#define WIPE_DIST 60
#define WIPE_GAP 120

typedef struct Icon {
    bool alive;
    int type, x, y, hp;
    int acc, gap;
    bool inside;
    Layer spr;
} Icon;

typedef struct Fx {
    bool used;
    Layer ring, star[4];
    int t;
} Fx;

typedef struct Row {
    int upTo, count, depth, points, seconds;
} Row;

struct Game {
    int kind, chapter, stage;
    u32 var;
    int phase, t;
    int timeMs, totalMs;
    int depth;
    Icon icon[MAX_ICONS];
    int count, left;
    int tool;
    Fx fx[MAX_FX];
    Image *back, *tips1, *tips2, *tips3, *grTips, *gpTips, *thumb, *white, *light[2], *cur[5][TOOLS];
    Anm dirt[DIRTS];
    Anm ring, star[4];
    Def ringDef, starDef[4];
    int lastX, lastY;
    bool down;
    int curFrame, curT, hitFlash;
    bool paused;
    int pauseHover;
    int resultHover;
    int finalSeconds;
    bool cleared;
    int result;
    bool bgmStarted;
    Row rows[8];
    int rowCount;
};

static Image *img(PJ *p, const char *name) { return pj_image(p, name); }

static void load_anm(PJ *p, Anm *a, const char *name) {
    size_t n;
    u8 *d = pj_pac_read(&p->pac, name, &n);
    if (d && !pj_anm_parse(a, d, n)) {
        free(d);
        memset(a, 0, sizeof *a);
    }
}

static void load_def(PJ *p, Def *d, const char *name) {
    char file[48];
    snprintf(file, sizeof file, "%s.def", name);
    size_t n;
    u8 *data = pj_pac_read(&p->pac, file, &n);
    memset(d, 0, sizeof *d);
    if (data) pj_def_parse(d, name, data, n);
    free(data);
}

static void read_ini(PJ *p, Game *g) {
    size_t n;
    u8 *d = pj_pac_read(&p->pac, "mg02_ini.txt", &n);
    if (d) {
        Ini c = {0};
        pj_ini_parse(&c, d, n);
        for (int i = 0; i < c.count && g->rowCount < 8; i++) {
            if (!pj_ieq(c.key[i], "StageSetting")) continue;
            int v[5];
            if (pj_ini_ints(c.val[i], v, 5) != 5) continue;
            g->rows[g->rowCount++] = (Row){v[0], v[1], v[2], v[3], v[4]};
        }
        pj_ini_free(&c);
        free(d);
    }
    if (!g->rowCount) g->rows[g->rowCount++] = (Row){99, 10, 1, 10, 30};
}

static Row stage_row(Game *g) {
    for (int i = 0; i < g->rowCount; i++)
        if (g->stage <= g->rows[i].upTo) return g->rows[i];
    return g->rows[g->rowCount - 1];
}

static void sfx(PJ *p, const char *name) {
    char file[32];
    snprintf(file, sizeof file, "%s.ogg", name);
    pj_play(p, CH_SE, 8, file, false);
}

static void place(PJ *p, Game *g) {
    Row r = stage_row(g);
    g->depth = r.depth > 0 ? r.depth : 1;
    g->totalMs = g->timeMs = r.seconds * 1000;
    int cells = GRID_COLS * GRID_ROWS, order[GRID_COLS * GRID_ROWS];
    for (int i = 0; i < cells; i++) order[i] = i;
    for (int i = cells - 1; i > 0; i--) {
        int j = (int)(pj_rand(p) % (u32)(i + 1));
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (int i = 0; i < MAX_ICONS; i++) pj_sprite_free(&g->icon[i].spr);
    memset(g->icon, 0, sizeof g->icon);
    g->count = clampi(r.count, 1, MAX_ICONS);
    for (int i = 0; i < g->count; i++) {
        Icon *c = &g->icon[i];
        int cell = order[i % cells];
        c->alive = true;
        c->type = (int)(pj_rand(p) % DIRTS);
        c->x = GRID_X + (cell % GRID_COLS) * GRID_DX;
        c->y = GRID_Y + (cell / GRID_COLS) * GRID_DY;
        c->hp = g->depth;
        Layer *l = &c->spr;
        l->used = l->shown = true;
        l->img = g->tips2;
        l->sx = 254 * (c->type) + 1;
        l->sy = 1;
        l->sw = l->sh = 252;
        l->x = c->x - 97;
        l->y = c->y - 97;
        l->alpha = 255;
        l->scale = 100;
        l->parent = -1;
        const Anm *a = &g->dirt[c->type];
        if (a->kind == ANM_ONE) {
            l->tr[0].live = false;
            pj_track_start(l, 0, a->start, a->end);
            l->animating = true;
        }
    }
    g->left = g->count;
}

static void start_stage(PJ *p, Game *g) {
    g->phase = PH_READY;
    g->t = 0;
    g->tool = T_CLEANER;
    g->paused = false;
    g->cleared = false;
    for (int i = 0; i < MAX_FX; i++) g->fx[i].used = false;
    place(p, g);
    sfx(p, "SE_205");
}

Game *pj_game_new(PJ *p, int kind, int chapter, int stage, i32 resultVar) {
    Game *g = pj_alloc(sizeof *g);
    g->kind = kind;
    g->chapter = chapter;
    g->stage = stage;
    g->var = (u32)resultVar;
    char name[32];
    snprintf(name, sizeof name, "gg_back%d%d.EPA", chapter, stage);
    g->back = img(p, name);
    snprintf(name, sizeof name, "gr_thumb%d%d.EPA", chapter, stage);
    g->thumb = img(p, name);
    const char *l = p->lang == VNPJADV_LANG_JAPANESE ? "j" : p->lang == VNPJADV_LANG_CHINESE ? "c" : "e";
    snprintf(name, sizeof name, "gg_tips1_%s.EPA", l);
    g->tips1 = img(p, name);
    g->tips2 = img(p, "gg_tips2.EPA");
    g->tips3 = img(p, "gg_tips3.EPA");
    snprintf(name, sizeof name, "gr_tips_%s.EPA", l);
    g->grTips = img(p, name);
    snprintf(name, sizeof name, "gp_tips_%s.EPA", l);
    g->gpTips = img(p, name);
    g->white = img(p, "gg_white1.EPA");
    g->light[0] = img(p, "gg_light1.EPA");
    g->light[1] = img(p, "gg_light2.EPA");
    for (int f = 0; f < 5; f++)
        for (int t = 0; t < TOOLS; t++) {
            snprintf(name, sizeof name, "gg_cur%d%d.EPA", f + 1, t + 1);
            g->cur[f][t] = img(p, name);
        }
    for (int i = 0; i < DIRTS; i++) {
        snprintf(name, sizeof name, "mg02_dust%02d.anm", dirtAnim[i]);
        load_anm(p, &g->dirt[i], name);
    }
    load_anm(p, &g->ring, "ga_eferase.anm");
    load_def(p, &g->ringDef, "ga_eferase");
    for (int i = 0; i < 4; i++) {
        snprintf(name, sizeof name, "ga_eferase%d.anm", i + 1);
        load_anm(p, &g->star[i], name);
        snprintf(name, sizeof name, "ga_eferase%d", i + 1);
        load_def(p, &g->starDef[i], name);
    }
    read_ini(p, g);
    g->pauseHover = g->resultHover = -1;
    g->lastX = p->mx;
    g->lastY = p->my;
    pj_log(p, "minigame %d chapter %d stage %d", kind, chapter, stage);
    start_stage(p, g);
    return g;
}

void pj_game_free(Game *g) {
    if (!g) return;
    for (int i = 0; i < MAX_ICONS; i++) pj_sprite_free(&g->icon[i].spr);
    for (int i = 0; i < DIRTS; i++) pj_anm_free(&g->dirt[i]);
    pj_anm_free(&g->ring);
    for (int i = 0; i < 4; i++) pj_anm_free(&g->star[i]);
    free(g);
}

static void fx_layer(Layer *l, Image *im, const Def *d, int ox, int oy) {
    memset(l, 0, sizeof *l);
    l->used = l->shown = true;
    l->img = im;
    l->sx = d->rx;
    l->sy = d->ry;
    l->sw = d->rw;
    l->sh = d->rh;
    l->x = ox + d->x;
    l->y = oy + d->y;
    l->alpha = clampi(d->alpha, 0, 255);
    l->scale = d->scale;
    l->hasDef = true;
    l->def = *d;
    l->parent = -1;
}

static void erase_fx(PJ *p, Game *g, const Icon *c) {
    Fx *f = NULL;
    for (int i = 0; i < MAX_FX && !f; i++)
        if (!g->fx[i].used) f = &g->fx[i];
    if (!f) return;
    memset(f, 0, sizeof *f);
    f->used = true;
    Image *eff = img(p, "ga_effect.EPA");
    int ox = c->x - 98, oy = c->y - 98;
    fx_layer(&f->ring, eff, &g->ringDef, ox, oy);
    if (g->ring.kind == ANM_ONE) {
        pj_track_start(&f->ring, 0, g->ring.start, g->ring.end);
        f->ring.animating = true;
    }
    for (int i = 0; i < 4; i++) {
        fx_layer(&f->star[i], eff, &g->starDef[i], ox, oy);
        if (g->star[i].kind == ANM_ONE) {
            pj_track_start(&f->star[i], 0, g->star[i].start, g->star[i].end);
            f->star[i].animating = true;
        }
    }
}

static bool in_rect(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }

static void finish(PJ *p, Game *g) {
    g->phase = PH_DONE;
    g->result = g->cleared ? 1 : 0;
    if (g->var < PJ_GLOBALS) p->g[g->var] = 1;
    pj_stop(p, CH_BGM, 0, 300);
}

static void wipe(PJ *p, Game *g, int dist, int ms) {
    for (int i = 0; i < g->count; i++) {
        Icon *c = &g->icon[i];
        if (!c->alive) continue;
        c->gap -= ms;
        bool inside = g->down && abs(p->mx - c->x) <= HIT && abs(p->my - c->y) <= HIT;
        if (!inside || toolForDirt[c->type] != g->tool) {
            c->inside = inside;
            if (!inside) c->acc = 0;
            continue;
        }
        if (!c->inside) c->acc = WIPE_DIST;
        c->inside = true;
        c->acc += dist;
        if (c->acc < WIPE_DIST || c->gap > 0) continue;
        c->acc = 0;
        c->gap = WIPE_GAP;
        c->hp--;
        g->hitFlash = 120;
        if (c->hp > 0) {
            c->spr.alpha = 255 * c->hp / g->depth;
            sfx(p, "SE_203");
            continue;
        }
        c->alive = false;
        g->left--;
        erase_fx(p, g, c);
        sfx(p, "SE_201");
    }
}

static int pause_button(int x, int y) {
    if (in_rect(x, y, 585, 293, 111, 111)) return 0;
    if (in_rect(x, y, 741, 374, 444, 142)) return 1;
    if (in_rect(x, y, 737, 541, 444, 142)) return 2;
    return -1;
}

void pj_game_mouse(PJ *p, Game *g, int button, bool down) {
    if (button != VNPJADV_MOUSE_LEFT) return;
    int x = p->mx, y = p->my;
    if (g->paused) {
        if (down) return;
        int b = pause_button(x, y);
        sfx(p, "SE_207");
        if (b == 0) g->paused = false;
        else if (b == 1) start_stage(p, g);
        else if (b == 2) {
            g->cleared = false;
            finish(p, g);
        }
        return;
    }
    if (g->phase == PH_RESULT) {
        if (!down && in_rect(x, y, 801, 687, 444, 142)) {
            pj_play(p, CH_SE, 15, "SES004.wav", false);
            finish(p, g);
        }
        return;
    }
    if (g->phase != PH_PLAY) {
        g->down = down;
        return;
    }
    if (down && in_rect(x, y, 32, 25, 105, 106)) {
        g->paused = true;
        g->pauseHover = -1;
        sfx(p, "SE_207");
        g->down = false;
        return;
    }
    if (down && in_rect(x, y, 51, 783, 263, 264)) {
        g->tool = (g->tool + 1) % TOOLS;
        sfx(p, "SE_204");
        g->down = false;
        return;
    }
    g->down = down;
    g->lastX = x;
    g->lastY = y;
    if (down) wipe(p, g, 0, 0);
}

bool pj_game_tick(PJ *p, Game *g, int ms) {
    if (!g->bgmStarted) {
        pj_play(p, CH_BGM, 0, "BGM_FC25.ogg", true);
        g->bgmStarted = true;
    }
    if (g->phase == PH_DONE) return false;
    int dx = p->mx - g->lastX, dy = p->my - g->lastY;
    int dist = (int)sqrt((double)(dx * dx + dy * dy));
    g->lastX = p->mx;
    g->lastY = p->my;
    if (g->paused) {
        g->pauseHover = pause_button(p->mx, p->my);
        return true;
    }
    g->t += ms;
    for (int i = 0; i < g->count; i++)
        if (g->icon[i].alive) pj_sprite_tick(p, &g->icon[i].spr, ms);
    for (int i = 0; i < MAX_FX; i++) {
        Fx *f = &g->fx[i];
        if (!f->used) continue;
        pj_sprite_tick(p, &f->ring, ms);
        f->t += ms;
        for (int k = 0; k < 4; k++) pj_sprite_tick(p, &f->star[k], ms);
        if (f->t > 2000) f->used = false;
    }
    if (g->hitFlash > 0) g->hitFlash -= ms;
    if (g->down && dist > 0) {
        g->curT += ms;
        if (g->curT > 70) {
            g->curT = 0;
            g->curFrame = (g->curFrame + 1) % 4;
        }
    } else if (!g->down)
        g->curFrame = 0;
    switch (g->phase) {
    case PH_READY:
        if (g->t >= 1300) {
            g->phase = PH_GO;
            g->t = 0;
            sfx(p, "SE_206");
        }
        break;
    case PH_GO:
        if (g->t >= 700) {
            g->phase = PH_PLAY;
            g->t = 0;
        }
        break;
    case PH_PLAY:
        if (g->down) wipe(p, g, dist, ms);
        else
            for (int i = 0; i < g->count; i++) {
                g->icon[i].inside = false;
                g->icon[i].gap -= ms;
            }
        if (g->left <= 0) {
            g->cleared = true;
            g->finalSeconds = (g->timeMs + 999) / 1000;
            g->phase = PH_CLEAR;
            g->t = 0;
            g->down = false;
            sfx(p, "SE_208");
            break;
        }
        g->timeMs -= ms;
        if (g->timeMs <= 0) {
            g->timeMs = 0;
            g->finalSeconds = 0;
            g->phase = PH_TIMEUP;
            g->t = 0;
            g->down = false;
            sfx(p, "SE_202");
        }
        break;
    case PH_CLEAR:
    case PH_TIMEUP:
        if (g->t >= 2000) {
            g->phase = PH_RESULT;
            g->t = 0;
            int c = clampi(g->chapter, 0, 4), s = clampi(g->stage, 0, 5);
            if (g->cleared) {
                p->stageClear[c][s] = 1;
                if (g->finalSeconds > p->stageBest[c][s]) p->stageBest[c][s] = g->finalSeconds;
                pj_save_store(p);
            }
        }
        break;
    case PH_RESULT:
        g->resultHover = in_rect(p->mx, p->my, 801, 687, 444, 142) ? 0 : -1;
        break;
    }
    return true;
}

static void blit(u32 *dst, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int alpha) {
    pj_blit(dst, PJ_W, PJ_H, im, sx, sy, sw, sh, dx, dy, alpha, 100, 0, 0);
}

static void blit_scaled(u32 *dst, const Image *im, int sx, int sy, int sw, int sh, int cx, int cy, int alpha, int scale) {
    pj_blit(dst, PJ_W, PJ_H, im, sx, sy, sw, sh, cx - sw / 2, cy - sh / 2, alpha, scale, sw / 2, sh / 2);
}

static void dim(u32 *dst, int alpha) {
    u32 k = (u32)(255 - alpha);
    for (size_t i = 0; i < (size_t)PJ_W * PJ_H; i++) {
        u32 c = dst[i];
        dst[i] = 0xff000000u | (((c & 0xff00ff) * k >> 8) & 0xff00ff) | (((c & 0x00ff00) * k >> 8) & 0x00ff00);
    }
}

static void draw_sprite(u32 *dst, const Layer *l) {
    if (!l->used || !l->img) return;
    pj_blit(dst, PJ_W, PJ_H, l->img, l->sx, l->sy, l->sw, l->sh, l->x + l->ox, l->y + l->oy, l->alpha, l->scale, l->hasDef ? l->def.cx : 0,
            l->hasDef ? l->def.cy : 0);
}

static void timer_digits(u32 *dst, const Game *g) {
    int s = (g->timeMs + 999) / 1000;
    if (g->phase == PH_CLEAR || g->phase == PH_RESULT) s = g->finalSeconds;
    if (g->phase == PH_TIMEUP) s = 0;
    s = clampi(s, 0, 99);
    int d[2] = {s / 10, s % 10};
    for (int k = 0; k < 2; k++) {
        const int *r = digitRect[d[k]];
        blit(dst, g->tips3, r[0], r[1], r[2], r[3], digitAt[d[k]][0] - (k ? 0 : 65), digitAt[d[k]][1], 255);
    }
}

static void small_number(u32 *dst, const Game *g, int value, int y) {
    if (value < 0) return;
    value = clampi(value, 0, 99);
    int d[2] = {value / 10, value % 10};
    for (int k = value >= 10 ? 0 : 1; k < 2; k++)
        blit(dst, g->grTips, (d[k] % 5) * 44, 1358 + (d[k] / 5) * 48, 44, 48, k ? 1396 : 1347, y, 255);
}

static int pop_scale(int t, int in) { return t >= in ? 100 : 60 + 40 * t / in; }

void pj_game_draw(PJ *p, Game *g, u32 *dst) {
    if (g->phase == PH_DONE) return;
    if (g->back) blit(dst, g->back, 0, 0, g->back->w, g->back->h, 0, 0, 255);
    if (g->phase >= PH_PLAY || g->phase == PH_GO)
        for (int i = 0; i < g->count; i++)
            if (g->icon[i].alive) draw_sprite(dst, &g->icon[i].spr);
    for (int i = 0; i < MAX_FX; i++)
        if (g->fx[i].used) {
            draw_sprite(dst, &g->fx[i].ring);
            for (int k = 0; k < 4; k++) draw_sprite(dst, &g->fx[i].star[k]);
        }
    if (g->tips3) {
        bool hp = g->phase == PH_PLAY && !g->paused && in_rect(p->mx, p->my, 32, 25, 105, 106);
        blit(dst, g->tips3, hp ? 103 : 2, 129, 99, 102, 34, 26, 255);
        int next = (g->tool + 1) % TOOLS;
        blit(dst, g->tips3, nextCell[next][0], nextCell[next][1], 128, 128, 19, 634, 255);
        timer_digits(dst, g);
    }
    if (g->tips1) {
        bool hover = g->phase == PH_PLAY && !g->paused && in_rect(p->mx, p->my, 51, 783, 263, 264);
        blit(dst, g->tips1, 263 * tips1Cell[g->tool], hover ? 264 : 0, 263, 264, 51, 783, 255);
    }
    if (g->tips3) {
        if (g->phase == PH_READY) blit_scaled(dst, g->tips3, 509, 0, 652, 264, 960, 540, g->t < 200 ? 255 * g->t / 200 : 255, pop_scale(g->t, 200));
        else if (g->phase == PH_GO) blit_scaled(dst, g->tips3, 1160, 0, 520, 230, 960, 540, g->t > 500 ? 255 * (700 - g->t) / 200 : 255, pop_scale(g->t, 150));
        else if (g->phase == PH_TIMEUP && g->t < 1800) blit_scaled(dst, g->tips3, 509, 263, 965, 212, 960, 540, 255, pop_scale(g->t, 200));
        else if (g->phase == PH_CLEAR) {
            if (g->t < 400 && g->white) {
                u32 a = (u32)(200 * (400 - g->t) / 400);
                for (size_t i = 0; i < (size_t)PJ_W * PJ_H; i++) {
                    u32 c = dst[i], ia = 255 - a;
                    u32 rb = ((0xff00ffu * a + (c & 0xff00ff) * ia) >> 8) & 0xff00ff;
                    u32 gg = ((0x00ff00u * a + (c & 0x00ff00) * ia) >> 8) & 0x00ff00;
                    dst[i] = 0xff000000u | rb | gg;
                }
            }
            Image *li = g->light[(g->t / 150) & 1];
            if (li) blit(dst, li, 0, 0, li->w, li->h, 0, 0, 255);
            blit_scaled(dst, g->tips3, 509, 509, 849, 211, 960, 540, 255, pop_scale(g->t, 250));
        }
    }
    if (g->phase == PH_RESULT && g->grTips) {
        dim(dst, 128);
        blit(dst, g->grTips, 1, 1, 1336, 713, 305, 257, 255);
        if (g->cleared) {
            blit(dst, g->grTips, 0, 762, 992, 222, 461, 133, 255);
            if (g->thumb) blit(dst, g->thumb, 0, 0, g->thumb->w, g->thumb->h, 468, 393, 255);
            blit(dst, g->grTips, 1, 1271, 168, 89, 597, 593, 255);
        } else
            blit(dst, g->grTips, 1, 1017, 881, 184, 528, 168, 255);
        small_number(dst, g, g->finalSeconds, 461);
        int c = clampi(g->chapter, 0, 4), s = clampi(g->stage, 0, 5);
        if (p->stageClear[c][s]) small_number(dst, g, p->stageBest[c][s], 525);
        if (g->resultHover == 0) blit(dst, g->grTips, 1018, 1018, 460, 149, 793, 684, 255);
        else blit(dst, g->grTips, 1026, 771, 444, 142, 801, 687, 255);
    }
    if (g->paused && g->gpTips) {
        dim(dst, 128);
        blit(dst, g->gpTips, 1, 1017, 644, 476, 638, 301, 255);
        blit(dst, g->gpTips, g->pauseHover == 0 ? 115 : 2, 1, 111, 111, 585, 293, 255);
        if (g->pauseHover == 1) blit(dst, g->gpTips, 2, 510, 460, 149, 733, 371, 255);
        else blit(dst, g->gpTips, 10, 263, 444, 142, 741, 374, 255);
        if (g->pauseHover == 2) blit(dst, g->gpTips, 510, 256, 460, 149, 729, 538, 255);
        else blit(dst, g->gpTips, 264, 9, 444, 142, 737, 541, 255);
    }
    if (g->phase == PH_PLAY && !g->paused) {
        int f = g->hitFlash > 0 ? 4 : g->curFrame;
        Image *c = g->cur[f][g->tool];
        if (!c) c = g->cur[0][g->tool];
        if (c) {
            int x = clampi(p->mx - c->w / 2, 0, PJ_W - c->w), y = clampi(p->my - c->h / 2, 0, PJ_H - c->h);
            blit(dst, c, 0, 0, c->w, c->h, x, y, 255);
        }
    }
}

int pj_game_result(Game *g) { return g->result; }

bool pj_stage_cleared(PJ *p, int chapter, int stage) {
    if (chapter < 0 || chapter > 4 || stage < 0 || stage > 5) return false;
    return p->stageClear[chapter][stage] != 0;
}

int pj_game_icons(const Game *g, int *xs, int *ys, int *tools, int max) {
    int n = 0;
    for (int i = 0; i < g->count && n < max; i++)
        if (g->icon[i].alive) {
            xs[n] = g->icon[i].x;
            ys[n] = g->icon[i].y;
            tools[n] = toolForDirt[g->icon[i].type];
            n++;
        }
    return n;
}

int pj_game_tool(const Game *g) { return g->tool; }

int pj_game_phase(const Game *g) { return g->phase; }

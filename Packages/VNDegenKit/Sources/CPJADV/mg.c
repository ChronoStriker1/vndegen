#include "pj.h"

struct Game {
    int kind, chapter, stage;
    i32 var;
    int t;
    int result;
};

Game *pj_game_new(PJ *p, int kind, int chapter, int stage, i32 resultVar) {
    Game *g = pj_alloc(sizeof *g);
    g->kind = kind;
    g->chapter = chapter;
    g->stage = stage;
    g->var = resultVar;
    pj_log(p, "minigame %d chapter %d stage %d", kind, chapter, stage);
    return g;
}

void pj_game_free(Game *g) { free(g); }

bool pj_game_tick(PJ *p, Game *g, int ms) {
    g->t += ms;
    if (g->t > 1000 && !g->result) {
        g->result = 1;
        u32 n = (u32)g->var & 0x0fffffffu;
        if (n < PJ_GLOBALS) p->g[n] = 1;
    }
    return true;
}

void pj_game_draw(PJ *p, Game *g, u32 *dst) {
    (void)p;
    (void)g;
    (void)dst;
}

int pj_game_result(Game *g) { return g->result; }

bool pj_stage_cleared(PJ *p, int chapter, int stage) {
    (void)p;
    return chapter == 1 && stage == 1;
}

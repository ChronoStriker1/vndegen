#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "png.h"
#include "sg.h"

#ifdef SG_SDL
#include <SDL.h>
#endif

static void step(SG *g, int ms) {
    for (int t = 0; t < ms; t += 16) vnsmilegame_frame((VNSmileGame *)g, 16);
}

static const HeroDef *hero_named(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->heroCount; i++)
        if (strcmp(t->heroes[i].name, name) == 0) return &t->heroes[i];
    return NULL;
}

static bool set_party(SG *g, const char *spec) {
    Party *p = &sgb(g)->party;
    p->count = 0;
    char buf[512];
    snprintf(buf, sizeof buf, "%s", spec);
    for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
        int level = 0;
        char *at = strchr(tok, '@');
        if (at) {
            *at = 0;
            level = atoi(at + 1);
        }
        const HeroDef *h = hero_named(g, tok);
        if (!h) {
            fprintf(stderr, "no hero %s\n", tok);
            return false;
        }
        party_add_hero(g, h->id);
        Member *m = &p->members[p->count - 1];
        if (level > 0) {
            m->level = level;
            m->exp = exp_for_level(level);
        }
        m->hp = member_max_hp(&sgb(g)->tables, m);
        m->mp = member_max_mp(&sgb(g)->tables, m);
    }
    return p->count > 0;
}

static bool find_troop(SG *g, const char *prefix, BattleSetup *out) {
    for (int f = 0; f < g->data.mapCount; f++) {
        Sgr *s = &g->data.maps[f];
        for (int i = 0; i < s->count; i++) {
            const SgrRec *r = &s->recs[i];
            if (r->type != SGR_EVENT || r->sub != 16 || r->size < 16) continue;
            char str[33];
            for (int k = 0; k < 16; k++) snprintf(str + k * 2, 3, "%02x", r->body[k]);
            if (strncmp(str, prefix, strlen(prefix)) != 0) continue;
            Script sc;
            if (!script_parse(&sc, r)) continue;
            bool ok = false;
            for (int k = 0; k < sc.count && !ok; k++)
                if (sc.cmds[k].type == 65) ok = battle_setup_cmd(out, &sc.cmds[k]);
            script_free(&sc);
            if (ok) return true;
        }
    }
    return false;
}

static bool find_monsters(SG *g, const char *spec, BattleSetup *out) {
    memset(out, 0, sizeof *out);
    char buf[512];
    snprintf(buf, sizeof buf, "%s", spec);
    Tables *t = &sgb(g)->tables;
    for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
        bool found = false;
        for (int i = 0; i < t->monsterCount && !found; i++)
            if (strcmp(t->monsters[i].name, tok) == 0 && out->count < BATTLE_MAX_MONSTERS) {
                out->monsters[out->count++] = t->monsters[i].id;
                found = true;
            }
        if (!found) {
            fprintf(stderr, "no monster %s\n", tok);
            return false;
        }
    }
    return out->count > 0;
}

static void shot(SG *g, const char *dir, const char *name) {
    char path[1400];
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    png_write(path, g->fb, g->w, g->h);
    printf("shot %s phase %s\n", path, battle_phase(g));
}

static void press(SG *g, int vk) {
    vnsmilegame_key((VNSmileGame *)g, vk, true);
    step(g, 48);
    vnsmilegame_key((VNSmileGame *)g, vk, false);
    step(g, 32);
}

static int usage(void) {
    fprintf(stderr, "usage: sgbattle <game> <out|-> [seed:n] party:Name[@lv],... troop:<script guid prefix>|monsters:Name,... [bg:<map name>]\n"
                    "       [wait:ms] [key:vk] [shot:name] [attack:n] [until:phase]\n");
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 4) return usage();
    const char *out = argv[2];
    bool play = strcmp(out, "-") == 0;
    char log[1024];
    snprintf(log, sizeof log, "%s/sgbattle.log", play ? "." : out);
    VNSmileGameConfig cfg = {.gameDir = argv[1], .saveDir = play ? NULL : out, .logPath = play ? "/dev/stderr" : log, .audioRate = 44100};
    char err[512];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) {
        fprintf(stderr, "open failed: %s\n", err);
        return 1;
    }
    BattleSetup setup = {0};
    bool haveTroop = false;
    int i = 3;
    for (; i < argc; i++) {
        const char *c = argv[i];
        if (strncmp(c, "seed:", 5) == 0) battle_seed(g, (u32)strtoul(c + 5, NULL, 0));
        else if (strncmp(c, "party:", 6) == 0) {
            press(g, 0x0d);
            interp_free(&g->interp);
            g->fadeLevel = g->fadeTo = 0;
            g->fadeTotal = 0;
            if (!set_party(g, c + 6)) return 1;
        } else if (strncmp(c, "troop:", 6) == 0) {
            haveTroop = find_troop(g, c + 6, &setup);
            if (!haveTroop) fprintf(stderr, "no battle command in script %s\n", c + 6);
        } else if (strncmp(c, "monsters:", 9) == 0) {
            haveTroop = find_monsters(g, c + 9, &setup);
        } else if (strncmp(c, "bg:", 3) == 0) {
            for (int f = 0; f < g->data.mapCount; f++) {
                Map m;
                const SgrRec *r = NULL;
                for (int k = 0; k < g->data.maps[f].count && !r; k++)
                    if (g->data.maps[f].recs[k].type == SGR_MAP) r = &g->data.maps[f].recs[k];
                if (r && map_parse(&m, r)) {
                    if (strcmp(m.name, c + 3) == 0) setup.background = m.id;
                    map_free(&m);
                }
            }
        } else {
            break;
        }
    }
    if (!haveTroop) return usage();
    sg_battle_start(g, &setup);
#ifdef SG_SDL
    if (play) {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_Window *win = SDL_CreateWindow("sgbattle", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g->w, g->h, SDL_WINDOW_RESIZABLE);
        SDL_Renderer *r = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
        SDL_RenderSetLogicalSize(r, g->w, g->h);
        SDL_Texture *tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, g->w, g->h);
        Uint32 last = SDL_GetTicks();
        for (bool run = true; run;) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) run = false;
                else if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && !e.key.repeat) {
                    int k = -1;
                    switch (e.key.keysym.sym) {
                    case SDLK_RETURN: case SDLK_z: k = 0x0d; break;
                    case SDLK_ESCAPE: case SDLK_x: k = 0x1b; break;
                    case SDLK_LEFT: k = 0x25; break;
                    case SDLK_UP: k = 0x26; break;
                    case SDLK_RIGHT: k = 0x27; break;
                    case SDLK_DOWN: k = 0x28; break;
                    default: break;
                    }
                    if (k >= 0) vnsmilegame_key((VNSmileGame *)g, k, e.type == SDL_KEYDOWN);
                }
            }
            Uint32 now = SDL_GetTicks();
            if (!vnsmilegame_frame((VNSmileGame *)g, now - last)) run = false;
            last = now;
            SDL_UpdateTexture(tex, NULL, g->fb, g->w * 4);
            SDL_RenderClear(r);
            SDL_RenderCopy(r, tex, NULL, NULL);
            SDL_RenderPresent(r);
        }
        SDL_Quit();
        vnsmilegame_close((VNSmileGame *)g);
        return 0;
    }
#endif
    step(g, 16);
    for (; i < argc; i++) {
        const char *c = argv[i];
        int k, ms;
        char name[256];
        if (sscanf(c, "wait:%d", &ms) == 1) step(g, ms);
        else if (sscanf(c, "key:%i", &k) == 1) press(g, k);
        else if (sscanf(c, "shot:%255s", name) == 1) shot(g, out, name);
        else if (sscanf(c, "attack:%d", &k) == 1) {
            for (int n = 0; n < k && sg_battle_active(g); n++) {
                for (int t = 0; t < 400 && sg_battle_active(g) && strcmp(battle_phase(g), "command") != 0 &&
                                strcmp(battle_phase(g), "result") != 0 && strcmp(battle_phase(g), "gameover") != 0;
                     t++)
                    step(g, 16);
                if (strcmp(battle_phase(g), "command") == 0) {
                    press(g, 0x0d);
                    press(g, 0x0d);
                } else if (sg_battle_active(g)) {
                    press(g, 0x0d);
                }
            }
        } else if (sscanf(c, "until:%255s", name) == 1) {
            for (int t = 0; t < 4000 && sg_battle_active(g) && strcmp(battle_phase(g), name) != 0; t++) step(g, 16);
        } else {
            fprintf(stderr, "unknown command %s\n", c);
        }
    }
    printf("phase %s result %d active %d\n", battle_phase(g), sg_battle_result(g), sg_battle_active(g));
    Party *p = &sgb(g)->party;
    for (int m = 0; m < p->count; m++) {
        const HeroDef *h = hero_def(&sgb(g)->tables, p->members[m].hero);
        printf("member %s lv %d exp %d hp %d mp %d\n", h ? h->name : "?", p->members[m].level, p->members[m].exp, p->members[m].hp,
               p->members[m].mp);
    }
    printf("money %d\n", p->money);
    vnsmilegame_close((VNSmileGame *)g);
    return 0;
}

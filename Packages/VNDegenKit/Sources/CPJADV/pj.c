#include "pj.h"

#include <stdarg.h>
#include <time.h>

void pj_log(PJ *p, const char *fmt, ...) {
    if (!p || !p->log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(p->log, fmt, ap);
    va_end(ap);
    fputc('\n', p->log);
    fflush(p->log);
}

void pj_fail(PJ *p, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(p->problem, sizeof p->problem, fmt, ap);
    va_end(ap);
    pj_log(p, "problem: %s", p->problem);
    p->halted = true;
}

u32 pj_rand(PJ *p) {
    p->rng ^= p->rng << 13;
    p->rng ^= p->rng >> 17;
    p->rng ^= p->rng << 5;
    return p->rng;
}

static void read_title(Pac *a, char *title, size_t size) {
    size_t n;
    u8 *ini = pj_pac_read(a, "GAME.ini", &n);
    if (!ini) return;
    Ini c = {0};
    pj_ini_parse(&c, ini, n);
    const char *t = pj_ini_get(&c, "", "gametitle");
    if (t) snprintf(title, size, "%s", t);
    pj_ini_free(&c);
    free(ini);
}

bool vnpjadv_probe(const char *gameDir, char *title, size_t titleSize) {
    Pac a;
    if (!pj_pac_open(&a, gameDir, NULL)) return false;
    bool ok = pj_pac_find(&a, "scenario.dat") && pj_pac_find(&a, "textdata.bin");
    if (ok && title && titleSize) {
        title[0] = 0;
        read_title(&a, title, titleSize);
    }
    pj_pac_close(&a);
    return ok;
}

VNPJADV *vnpjadv_open(const VNPJADVConfig *config, char *err, size_t errSize) {
    PJ *p = pj_alloc(sizeof *p);
    p->cfg = *config;
    p->gameDir = pj_strdup(config->gameDir);
    p->saveDir = config->saveDir ? pj_strdup(config->saveDir) : pj_fmt("%s/save", config->gameDir);
    if (config->logPath) p->log = !strcmp(config->logPath, "/dev/stderr") ? stderr : fopen(config->logPath, "w");
    p->lang = config->language >= 1 && config->language <= 3 ? config->language : VNPJADV_LANG_ENGLISH;
    p->rng = config->seed ? config->seed : (u32)time(NULL) | 1u;
    if (!pj_pac_open(&p->pac, p->gameDir, p)) {
        snprintf(err, errSize, "no PJADV archive.dat in %s", p->gameDir);
        vnpjadv_close((VNPJADV *)p);
        return NULL;
    }
    if (!pj_script_load(&p->sc, &p->pac, err, errSize)) {
        vnpjadv_close((VNPJADV *)p);
        return NULL;
    }
    snprintf(p->title, sizeof p->title, "PJADV");
    read_title(&p->pac, p->title, sizeof p->title);
    size_t n;
    u8 *bgm = pj_pac_read(&p->pac, "BGM.ini", &n);
    if (bgm) {
        pj_ini_parse(&p->bgmIni, bgm, n);
        free(bgm);
    }
    p->screen = pj_alloc((size_t)PJ_W * PJ_H * 4);
    p->shown = pj_alloc((size_t)PJ_W * PJ_H * 4);
    p->from = pj_alloc((size_t)PJ_W * PJ_H * 4);
    for (size_t i = 0; i < (size_t)PJ_W * PJ_H; i++) p->shown[i] = 0xff000000u;
    p->hover = -1;
    p->pressBtn = -1;
    p->groupMs = -1;
    for (int i = 0; i < PJ_LAYERS + PJ_SUBS; i++) p->layer[i].parent = -1;
    if (config->audioRate > 0) p->mix = pj_mixer_new(config->audioRate);
    pj_msg_init(p);
    pj_font_init(p);
    pj_save_load(p);
    p->pc = 0x20;
    p->dirty = true;
    pj_log(p, "opened %s, language %d", p->title, p->lang);
    return (VNPJADV *)p;
}

void vnpjadv_close(VNPJADV *v) {
    PJ *p = (PJ *)v;
    if (!p) return;
    if (p->game) pj_game_free(p->game);
    for (int i = 0; i < PJ_LAYERS + PJ_SUBS; i++) pj_sprite_free(&p->layer[i]);
    for (int i = 0; i < PJ_BUTTONS; i++) pj_sprite_free(&p->btn[i].spr);
    pj_sprite_free(&p->msgFrame);
    for (size_t i = 0; i < sizeof p->cache / sizeof p->cache[0]; i++) pj_image_free(p->cache[i].img);
    pj_mixer_free(p->mix);
    pj_script_free(&p->sc);
    pj_pac_close(&p->pac);
    pj_ini_free(&p->bgmIni);
    free(p->msgName);
    free(p->msgText);
    free(p->screen);
    free(p->shown);
    free(p->from);
    free(p->gameDir);
    free(p->saveDir);
    if (p->log && p->log != stderr) fclose(p->log);
    free(p);
}

static int button_at(PJ *p, int x, int y) {
    int best = -1;
    u32 z = 0;
    for (int i = 0; i < PJ_BUTTONS; i++) {
        Button *b = &p->btn[i];
        if (!b->used || x < b->x1 || x >= b->x2 || y < b->y1 || y >= b->y2) continue;
        if (best < 0 || b->spr.z > z) {
            best = i;
            z = b->spr.z;
        }
    }
    return best;
}

static void update_hover(PJ *p) {
    int h = p->wait == W_BUTTON ? button_at(p, p->mx, p->my) : -1;
    if (h >= 0 && p->btn[h].group != p->waitGroup) h = -1;
    for (int i = 0; i < PJ_BUTTONS; i++) {
        Button *b = &p->btn[i];
        if (!b->used) continue;
        int state = i == h ? (p->mdown && p->pressBtn == i ? 4 : 2) : 0;
        if (b->spr.btnState != state) {
            pj_layer_set_state(p, &b->spr, state);
            p->dirty = true;
        }
    }
    p->hover = h;
}

static void handle_click(PJ *p, bool right) {
    if (p->game) return;
    int key = right ? 1 : 0;
    if (p->wait == W_BUTTON && !right) {
        int b = button_at(p, p->mx, p->my);
        if (b >= 0 && b == p->pressBtn && p->btn[b].group == p->waitGroup) {
            p->wait = W_NONE;
            pj_call_handler(p, p->btn[b].handler);
            p->stack[p->sp - 2] = W_NONE;
        }
        return;
    }
    if (p->keyHandler[key] && p->wait != W_BUTTON) {
        pj_call_handler(p, p->keyHandler[key]);
        return;
    }
    if (p->wait == W_TEXT && !right) {
        if (p->msgShownChars < p->msgTotalChars) {
            p->msgShownChars = p->msgTotalChars;
            p->dirty = true;
        } else
            p->wait = W_NONE;
    }
}

void vnpjadv_mouse_move(VNPJADV *v, int x, int y) {
    PJ *p = (PJ *)v;
    p->mx = x;
    p->my = y;
}

void vnpjadv_mouse_button(VNPJADV *v, int button, bool down) {
    PJ *p = (PJ *)v;
    if (button == VNPJADV_MOUSE_LEFT) {
        p->mdown = down;
        if (down) p->pressBtn = button_at(p, p->mx, p->my);
        else {
            handle_click(p, false);
            p->pressBtn = -1;
        }
    } else if (button == VNPJADV_MOUSE_RIGHT) {
        p->rdown = down;
        if (!down) handle_click(p, true);
    }
}

void vnpjadv_key(VNPJADV *v, int vk, bool down) {
    PJ *p = (PJ *)v;
    if (!down) return;
    if (vk == 0x0d || vk == 0x20) {
        if (p->wait == W_TEXT) handle_click(p, false);
    } else if (vk == 0x1b)
        handle_click(p, true);
}

void vnpjadv_wheel(VNPJADV *v, int notches) {
    PJ *p = (PJ *)v;
    if (notches < 0 && p->wait == W_TEXT) handle_click(p, false);
}

bool vnpjadv_frame(VNPJADV *v, uint32_t elapsedMs) {
    PJ *p = (PJ *)v;
    if (p->quit) return false;
    if (p->paused) return true;
    int ms = (int)(elapsedMs > 200 ? 200 : elapsedMs);
    if (!p->halted) pj_vm_run(p, ms);
    pj_layers_tick(p, ms);
    pj_msg_tick(p, ms);
    if (p->game) {
        pj_game_tick(p, p->game, ms);
        p->dirty = true;
    }
    update_hover(p);
    pj_frame_tick(p, ms);
    return !p->quit;
}

const uint32_t *vnpjadv_pixels(VNPJADV *v, int *width, int *height) {
    PJ *p = (PJ *)v;
    *width = PJ_W;
    *height = PJ_H;
    return p->shown;
}

uint32_t vnpjadv_frame_serial(VNPJADV *v) { return ((PJ *)v)->serial; }

void vnpjadv_audio(VNPJADV *v, float *out, int frames) {
    PJ *p = (PJ *)v;
    if (!p->mix) {
        memset(out, 0, sizeof(float) * (size_t)frames * 2);
        return;
    }
    pj_mixer_mix(p->mix, out, frames, p->paused);
}

void vnpjadv_set_paused(VNPJADV *v, bool paused) { ((PJ *)v)->paused = paused; }

void vnpjadv_request_quit(VNPJADV *v) {
    PJ *p = (PJ *)v;
    pj_save_store(p);
    p->quit = true;
}

const char *vnpjadv_title(VNPJADV *v) { return ((PJ *)v)->title; }

const char *vnpjadv_problem(VNPJADV *v) {
    PJ *p = (PJ *)v;
    return p->problem[0] ? p->problem : NULL;
}

#include "anim.h"

#include <dirent.h>

static char *find_base(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return NULL;
    char *base = NULL;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n > 8 && an_ieq(e->d_name + n - 8, "_sce.dat")) {
            base = an_alloc(n - 7);
            memcpy(base, e->d_name, n - 8);
            break;
        }
    }
    closedir(d);
    return base;
}

static bool has_file(const char *dir, const char *base, const char *tail) {
    char *p = an_fmt("%s/%s%s", dir, base, tail);
    bool ok = an_file_exists(p);
    free(p);
    return ok;
}

bool vnanim_probe(const char *gameDir, char *title, size_t titleSize) {
    char *base = find_base(gameDir);
    if (!base) return false;
    bool ok = has_file(gameDir, base, "_define.dat") && has_file(gameDir, base, "_system.dat");
    char *emote = an_fmt("%s/cg/psbpack.dat", gameDir);
    ok = ok && an_file_exists(emote);
    free(emote);
    if (ok && title && titleSize) snprintf(title, titleSize, "%s", base);
    free(base);
    return ok;
}

VNANIM *vnanim_open(const VNANIMConfig *cfg, char *err, size_t errSize) {
    char *base = find_base(cfg->gameDir);
    if (!base) {
        snprintf(err, errSize, "no ANIM script in %s", cfg->gameDir);
        return NULL;
    }
    AN *a = an_alloc(sizeof *a);
    a->dir = an_strdup(cfg->gameDir);
    a->saveDir = an_strdup(cfg->saveDir ? cfg->saveDir : cfg->gameDir);
    a->base = base;
    a->game = !strcmp(base, "mgsw") ? GAME_MGSW : GAME_DDD;
    a->rate = cfg->audioRate > 0 ? cfg->audioRate : 44100;
    if (cfg->logPath) a->log = fopen(cfg->logPath, "a");
    snprintf(a->title, sizeof a->title, "%s", base);
    char *p = an_fmt("%s/%s_sce.dat", a->dir, base);
    a->sc = an_script_load(p, err, errSize);
    free(p);
    if (!a->sc) {
        vnanim_close((VNANIM *)a);
        return NULL;
    }
    p = an_fmt("%s/%s_define.dat", a->dir, base);
    if (!an_define_load(&a->def, p)) an_log(a, "can't read %s", p);
    free(p);
    an_flow_load(a);
    p = an_fmt("%s/voice/%s_wave.pck", a->dir, base);
    if (!an_pack_open(&a->voice, p, false)) an_log(a, "no voice archive %s", p);
    free(p);
    p = an_fmt("%s/cg/psbpack.dat", a->dir);
    if (!an_pack_open(&a->psb, p, true)) an_log(a, "no E-mote archive %s", p);
    free(p);
    a->W = 1280;
    a->H = 720;
    size_t n = (size_t)a->W * (size_t)a->H;
    a->fb = an_alloc(sizeof(u32) * n);
    a->prev = an_alloc(sizeof(u32) * n);
    a->shot = an_alloc(sizeof(u32) * n);
    a->lastSlot = -1;
    a->emoteScale = 1.0f;
    an_volume_defaults(a);
    an_settings_defaults(a);
    an_system_load(a);
    an_text_init(a);
    an_snd_init(a);
    an_vm_start(a, 8);
    srand((unsigned)cfg->startTime);
    an_log(a, "opened %s: %d labels, %d strings, %d sprite names, %d voices, %d E-mote models", base, a->sc->labelCount,
           a->sc->strCount, a->def.spriteCount, a->voice.count, a->psb.count);
    return (VNANIM *)a;
}

void vnanim_close(VNANIM *n) {
    AN *a = (AN *)n;
    if (!a) return;
    an_movie_stop(a);
    an_emote_free(a);
    an_snd_free(a);
    an_text_free(a);
    an_image_cache_clear(a);
    an_script_free(a->sc);
    an_define_free(&a->def);
    an_pack_close(&a->voice);
    an_pack_close(&a->psb);
    free(a->logs);
    free(a->flow);
    free(a->fb);
    free(a->prev);
    free(a->shot);
    free(a->stencil);
    for (int i = 0; i < 180; i++) an_image_free(a->thumbs[i]);
    free(a->saves);
    free(a->sys);
    free(a->dir);
    free(a->saveDir);
    free(a->base);
    if (a->log) fclose(a->log);
    free(a);
}

bool vnanim_frame(VNANIM *n, uint32_t elapsedMs) {
    AN *a = (AN *)n;
    if (a->quit) return false;
    if (a->paused) return true;
    a->now += elapsedMs;
    if (an_screens_input(a) || an_flow_input(a)) {
    } else if (a->phase == PH_TITLE) an_title_input(a);
    else if (!an_ui_input(a)) an_game_input(a);
    if (a->phase == PH_GAME && a->screen == SCR_NONE) an_vm_step(a);
    if (an_movie_looping(a)) an_movie_update(a);
    a->clicked = false;
    a->rclicked = false;
    a->wheel = 0;
    an_gfx_render(a);
    return !a->quit;
}

const uint32_t *vnanim_pixels(VNANIM *n, int *width, int *height) {
    AN *a = (AN *)n;
    *width = a->W;
    *height = a->H;
    return a->fb;
}

uint32_t vnanim_frame_serial(VNANIM *n) { return ((AN *)n)->serial; }

void vnanim_mouse_move(VNANIM *n, int x, int y) {
    AN *a = (AN *)n;
    a->mx = x;
    a->my = y;
}

void vnanim_mouse_button(VNANIM *n, int button, bool down) {
    AN *a = (AN *)n;
    if (button == VNANIM_MOUSE_LEFT) {
        if (a->down && !down) {
            a->clicked = true;
            a->lastInput = a->now;
        }
        a->down = down;
    } else if (button == VNANIM_MOUSE_RIGHT && down) {
        a->rclicked = true;
        a->lastInput = a->now;
    }
}

void vnanim_key(VNANIM *n, int vk, bool down) {
    AN *a = (AN *)n;
    if (vk == 0x11) a->ctrl = down;
    if (down) a->lastInput = a->now;
    if ((vk == 0x0d || vk == 0x20) && !down) a->clicked = true;
}

void vnanim_wheel(VNANIM *n, int notches) {
    AN *a = (AN *)n;
    a->wheel += notches;
}

void vnanim_audio(VNANIM *n, float *out, int frames) { an_snd_mix((AN *)n, out, frames); }

void vnanim_set_paused(VNANIM *n, bool paused) { ((AN *)n)->paused = paused; }

void vnanim_request_quit(VNANIM *n) { ((AN *)n)->quit = true; }

const char *vnanim_title(VNANIM *n) { return ((AN *)n)->title; }

const char *vnanim_take_page(VNANIM *n) {
    AN *a = (AN *)n;
    if (!a->page[0]) return NULL;
    static _Thread_local char out[sizeof a->page];
    snprintf(out, sizeof out, "%s", a->page);
    a->page[0] = 0;
    return out;
}

const char *vnanim_problem(VNANIM *n) {
    AN *a = (AN *)n;
    return a->problem[0] ? a->problem : NULL;
}

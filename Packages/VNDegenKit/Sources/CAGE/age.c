#include "age.h"

void vm_register_all(void) {
    ops_core_register();
    gfx_register();
    input_register();
    snd_register();
    text_register();
    movie_register();
}

VNAge *vnage_open(const VNAgeConfig *cfg, char *err, size_t errSize) {
    AG *g = ag_alloc(sizeof *g);
    if (cfg->logPath) g->log = fopen(cfg->logPath, "a");
    g->gameDir = ag_strdup(cfg->gameDir);
    g->saveDir = cfg->saveDir ? ag_strdup(cfg->saveDir) : NULL;
    if (!store_open(&g->st, cfg->gameDir, err, errSize)) { vnage_close(g); return NULL; }
    g->width = 1024;
    g->height = 576;
    const char *sx = store_setting(&g->st, "SCREENX"), *sy = store_setting(&g->st, "SCREENY");
    if (sx && atoi(sx) > 0) g->width = atoi(sx);
    if (sy && atoi(sy) > 0) g->height = atoi(sy);
    g->screen = ag_alloc(sizeof(u32) * (size_t)g->width * (size_t)g->height);
    for (int i = 0; i < g->width * g->height; i++) g->screen[i] = 0xff000000u;
    snprintf(g->title, sizeof g->title, "Eushully AGE");
    gfx_init(g);
    input_init(g);
    snd_init(g, cfg->audioRate);
    text_init(g);
    if (!vm_init(g)) {
        snprintf(err, errSize, "%s", g->problem ? g->problem : "the root script can't be loaded");
        vnage_close(g);
        return NULL;
    }
    return g;
}

void vnage_close(VNAge *g) {
    if (!g) return;
    vm_close(g);
    movie_stop_all(g);
    gfx_close(g);
    snd_close(g);
    text_close(g);
    store_close(&g->st);
    free(g->screen);
    free(g->gameDir);
    free(g->saveDir);
    free(g->problem);
    if (g->log && g->log != stderr) fclose(g->log);
    free(g);
}

bool vnage_probe(const char *gameDir, char *title, size_t titleSize) {
    char *p = ag_fmt("%s/SYS4INI.BIN", gameDir);
    size_t n = 0;
    u8 *d = ag_read_file(p, &n);
    free(p);
    bool ok = d && n > 8 && !memcmp(d, "S4IC", 4);
    free(d);
    if (ok && title && titleSize) snprintf(title, titleSize, "Eushully AGE");
    return ok;
}

static void service(AG *g) {
    if (g->wait == W_SLEEP && (i32)(g->now - g->waitUntil) >= 0) g->wait = W_NONE;
    if (g->wait == W_YIELD && !gfx_busy(g)) g->wait = W_NONE;
    if (g->wait == W_FRAME) g->wait = W_NONE;
    if (g->wait == W_MOVIE) {
        if ((g->in.clicks & 1) || (g->in.pressed & 0x10) || g->in.skip) movie_modal_skip(g);
        if (movie_modal_done(g)) g->wait = W_NONE;
    }
    if (g->wait == W_TRANS && (gfx_transition_done(g) || (g->in.clicks & 1) || (g->in.pressed & 0x10))) { gfx_transition_finish(g); g->wait = W_NONE; }
    if (g->wait == W_TEXT || g->wait == W_INPUT || g->wait == W_HIDDEN) input_service(g);
}

bool vnage_frame(VNAge *g, uint32_t elapsedMs) {
    u32 end = g->now + elapsedMs;
    for (int guard = 0; guard < 64; guard++) {
        u32 step = end - g->now;
        if (g->wait == W_SLEEP && (i32)(g->waitUntil - g->now) > 0 && (i32)(g->waitUntil - end) < 0) step = g->waitUntil - g->now;
        g->now += step;
        gfx_tick(g);
        snd_tick(g);
        text_tick(g);
        movie_tick(g);
        service(g);
        vm_run(g, 500000);
        if (g->halted || g->quit || g->now == end) break;
    }
    g->now = end;
    gfx_tick(g);
    text_tick(g);
    g->needRender = true;
    g->frameSerial++;
    input_end_frame(g);
    return !g->halted && !g->quit;
}

const uint32_t *vnage_pixels(VNAge *g, int *w, int *h) {
    if (g->needRender) {
        g->needRender = false;
        u32 serial = g->frameSerial;
        if (!gfx_transition_frame(g) && !g->gfx.hold) gfx_render(g, 1000, 0, -1);
        g->frameSerial = serial;
    }
    *w = g->width;
    *h = g->height;
    return g->screen;
}

uint32_t vnage_frame_serial(VNAge *g) { return g->frameSerial; }
void vnage_mouse_move(VNAge *g, int x, int y) { g->in.mx = x; g->in.my = y; }
void vnage_mouse_button(VNAge *g, int button, bool down) {
    int b = button == VNAGE_MOUSE_LEFT ? 0 : button == VNAGE_MOUSE_RIGHT ? 1 : -1;
    if (b < 0) return;
    if (down) {
        g->in.buttons |= 1u << b;
        g->in.clicks |= 1u << b;
        g->in.pressed |= 1u << ((g->in.mouseSlot[b] + 4) & 31);
    } else g->in.buttons &= ~(1u << b);
}
void vnage_key(VNAge *g, int vk, bool down) {
    if (vk < 0 || vk >= 256) return;
    if (down && !g->in.keys[vk] && g->in.keyAction[vk] < 32) g->in.pressed |= 1u << g->in.keyAction[vk];
    g->in.keys[vk] = down;
}
void vnage_wheel(VNAge *g, int notches) { g->in.wheel += notches * 120; }
void vnage_audio(VNAge *g, float *out, int frames) { mixer_mix(g->snd.mixer, out, frames, g->snd.paused); }
void vnage_set_paused(VNAge *g, bool paused) { g->snd.paused = paused; }
void vnage_request_quit(VNAge *g) { g->quit = true; }
const char *vnage_title(VNAge *g) { return g->title; }
const char *vnage_problem(VNAge *g) { return g->problem; }

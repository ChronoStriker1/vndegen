#include "ikura.h"

#include <time.h>

#define TICK_MS (1000.0 / 60.0)

bool vnikura_probe(const char *gameDir, char *title, size_t titleSize) {
    Store s;
    if (!store_open(&s, gameDir, NULL, 0)) return false;
    size_t n;
    u8 *d = store_load(&s, AR_SNR, "START.SNR", &n);
    bool ok = d != NULL;
    free(d);
    if (title && titleSize) title[0] = 0;
    store_close(&s);
    return ok;
}

VNIkura *vnikura_open(const VNIkuraConfig *cfg, char *err, size_t errSize) {
    IK *t = ik_alloc(sizeof *t);
    if (cfg->logPath) t->log = fopen(cfg->logPath, "a");
    t->gameDir = ik_strdup(cfg->gameDir);
    t->saveDir = ik_strdup(cfg->saveDir ? cfg->saveDir : cfg->gameDir);
    t->audioRate = cfg->audioRate > 0 ? cfg->audioRate : 44100;
    t->trace = getenv("IKURA_TRACE") != NULL;
    if (!store_open(&t->store, t->gameDir, err, errSize)) {
        if (t->log) fclose(t->log);
        free(t->gameDir);
        free(t->saveDir);
        free(t);
        return NULL;
    }
    t->W = 800;
    t->H = 600;
    gfx_init(t);
    snd_init(t);
    t->msg.layer = -1;
    t->msg.name = -1;
    t->msg.msPerChar = 50;
    vm_start(t, "START.SNR");
    if (!t->sc) {
        if (err) snprintf(err, errSize, "%s", t->problem);
        vnikura_close((VNIkura *)t);
        return NULL;
    }
    ik_log(t, "opened %s", t->gameDir);
    return (VNIkura *)t;
}

void vnikura_close(VNIkura *n) {
    IK *t = (IK *)n;
    if (!t) return;
    snd_close(t);
    gfx_close(t);
    msg_close(t);
    text_close(t);
    free(t->flags);
    for (int i = 0; i < t->scriptCount; i++) script_free(t->scripts[i]);
    free(t->scripts);
    free(t->vars);
    store_close(&t->store);
    if (t->log) fclose(t->log);
    free(t->gameDir);
    free(t->saveDir);
    free(t);
}

static void tick(IK *t) {
    buttons_input(t);
    gfx_update(t);
    msg_tick(t);
    switch (t->wait) {
    case W_TIME:
        if ((i32)(t->now - t->waitUntil) >= 0) t->wait = W_NONE;
        break;
    case W_ANIM:
        if (!gfx_animating(t)) t->wait = W_NONE;
        break;
    case W_VOICE:
        if (!snd_voice_playing(t) || t->click) t->wait = W_NONE;
        break;
    case W_QUIT: t->quit = true; break;
    default: break;
    }
    vm_run(t);
    t->click = false;
    t->rclick = false;
    t->wheel = 0;
    memset(t->keyPress, 0, sizeof t->keyPress);
}

bool vnikura_frame(VNIkura *n, uint32_t elapsedMs) {
    IK *t = (IK *)n;
    if (t->paused) return !t->quit;
    if (elapsedMs > 250) elapsedMs = 250;
    t->tickAcc += elapsedMs;
    int ticks = 0;
    while (t->tickAcc >= TICK_MS && ticks < 8 && !t->quit) {
        t->tickAcc -= TICK_MS;
        t->clockMs += TICK_MS;
        t->now = (u32)t->clockMs;
        tick(t);
        ticks++;
    }
    if (t->tickAcc > TICK_MS * 8) t->tickAcc = 0;
    if (t->dirty) {
        gfx_compose(t);
        t->dirty = false;
    }
    return !t->quit;
}

const uint32_t *vnikura_pixels(VNIkura *n, int *width, int *height) {
    IK *t = (IK *)n;
    *width = t->W;
    *height = t->H;
    return t->screen;
}

uint32_t vnikura_frame_serial(VNIkura *n) { return ((IK *)n)->serial; }

void vnikura_mouse_move(VNIkura *n, int x, int y) {
    IK *t = (IK *)n;
    if (x != t->mx || y != t->my) t->dirty = true;
    t->mx = x;
    t->my = y;
}

void vnikura_mouse_button(VNIkura *n, int button, bool down) {
    IK *t = (IK *)n;
    if (button == VNIKURA_MOUSE_LEFT) {
        if (t->lDown && !down) t->click = true;
        t->lDown = down;
    } else if (button == VNIKURA_MOUSE_RIGHT) {
        if (t->rDown && !down) t->rclick = true;
        t->rDown = down;
    }
}

void vnikura_key(VNIkura *n, int vk, bool down) {
    IK *t = (IK *)n;
    if (vk < 0 || vk > 255) return;
    if (down && !t->keyDown[vk]) t->keyPress[vk] = true;
    t->keyDown[vk] = down;
    if (down && (vk == 0x0d || vk == 0x20)) t->click = true;
}

void vnikura_wheel(VNIkura *n, int notches) { ((IK *)n)->wheel += notches; }

void vnikura_audio(VNIkura *n, float *out, int frames) { snd_mix((IK *)n, out, frames); }

void vnikura_set_paused(VNIkura *n, bool paused) { ((IK *)n)->paused = paused; }

void vnikura_request_quit(VNIkura *n) { ((IK *)n)->quit = true; }

const char *vnikura_title(VNIkura *n) {
    (void)n;
    return "";
}

const char *vnikura_problem(VNIkura *n) {
    IK *t = (IK *)n;
    return t->problem[0] ? t->problem : NULL;
}

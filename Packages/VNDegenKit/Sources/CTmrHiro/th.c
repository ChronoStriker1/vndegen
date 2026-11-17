#include "th.h"

#include <stdarg.h>
#include <time.h>

void th_log(TH *t, const char *fmt, ...) {
    if (!t || !t->log) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(t->log, "[%7u] ", t->now);
    vfprintf(t->log, fmt, ap);
    fputc('\n', t->log);
    fflush(t->log);
    va_end(ap);
}

static bool load_ini(Store *s, Ini *ini, const char *name) {
    size_t size;
    u8 *d = store_load(s, PAC_SCRIPT, name, &size);
    if (!d) return false;
    bool ok = ini_load(ini, d, size);
    free(d);
    return ok;
}

bool vnth_probe(const char *gameDir, char *title, size_t titleSize) {
    Store s;
    if (!store_open(&s, gameDir, NULL, 0)) return false;
    Ini ini;
    bool ok = load_ini(&s, &ini, "_system") && ini_section(&ini, "GAMESYSTEM");
    if (ok && title && titleSize) {
        const char *g = ini_get(&ini, "GAMESYSTEM", "GAME_TITLE");
        char *u = th_sjis_to_utf8(g ? g : "");
        snprintf(title, titleSize, "%s", u);
        free(u);
    }
    if (ok) ini_free(&ini);
    store_close(&s);
    return ok;
}

VNTH *vnth_open(const VNTHConfig *cfg, char *err, size_t errSize) {
    TH *t = th_alloc(sizeof *t);
    if (cfg->logPath) t->log = fopen(cfg->logPath, "a");
    t->gameDir = th_strdup(cfg->gameDir);
    t->saveDir = th_strdup(cfg->saveDir ? cfg->saveDir : cfg->gameDir);
    t->audioRate = cfg->audioRate > 0 ? cfg->audioRate : 44100;
    t->startTime = cfg->startTime ? cfg->startTime : (long long)time(NULL);
    t->rng = (u32)t->startTime;
    if (!store_open(&t->store, t->gameDir, err, errSize)) {
        if (t->log) fclose(t->log);
        free(t->gameDir);
        free(t->saveDir);
        free(t);
        return NULL;
    }
    if (!load_ini(&t->store, &t->sys, "_system")) {
        if (err) snprintf(err, errSize, "This game's _system settings couldn't be read.");
        store_close(&t->store);
        if (t->log) fclose(t->log);
        free(t->gameDir);
        free(t->saveDir);
        free(t);
        return NULL;
    }
    load_ini(&t->store, &t->fileMacro, "_filemacro");
    load_ini(&t->store, &t->charaMacro, "_charafilemacro");
    load_ini(&t->store, &t->varIni, "_var");
    load_ini(&t->store, &t->charaView, "_charaview");
    const char *g = ini_get(&t->sys, "GAMESYSTEM", "GAME_TITLE");
    t->title = th_sjis_to_utf8(g ? g : "Tmr-Hiro ADV");
    t->W = ini_int(&t->sys, "GAMESYSTEM", "DEF_WINDOW_WIDTH", 1280);
    t->H = ini_int(&t->sys, "GAMESYSTEM", "DEF_WINDOW_HEIGHT", 720);
    int vol[4] = {100, 100, 100, 100};
    ini_ints(&t->sys, "SYSTEM", "SYS_DEF_SOUND_VOLUME", vol, 4);
    for (int i = 0; i < 4; i++) t->sysVol[i] = vol[i];
    t->textSpeed = ini_int(&t->sys, "SYSTEM", "SYS_DEF_TEXTSPEED", 6);
    t->autoSpeed = ini_int(&t->sys, "SYSTEM", "SYS_DEF_AUTOSPEED", 6);
    const char *ws = ini_get(&t->sys, "SYSTEM", "SYS_DEF_WINDOW_STAT");
    t->winStat = ws ? (u32)strtoul(ws, NULL, 16) : 0x80000000u;
    t->winBaseDraw = ini_int(&t->sys, "SYSTEM", "SYS_DEF_WINDOW_BASE_DRAW", 1) != 0;
    t->readColor = ini_int(&t->sys, "SYSTEM", "SYS_DEF_READ_TEXT_COLOR", 1) != 0;
    t->mixer = mixer_new(t->audioRate);
    th_mkdirs(t->saveDir);
    gfx_init(t);
    text_init(t);
    load_system(t);
    th_log(t, "opened %s (%dx%d)", t->title, t->W, t->H);
    t->hover = -1;
    t->savePc = -1;
    memset(t->slotFlags, 1, sizeof t->slotFlags);
    vm_start(t, "start", NULL);
    t->dirty = true;
    t->sceneDirty = true;
    return (VNTH *)t;
}

void vnth_close(VNTH *v) {
    TH *t = (TH *)v;
    if (!t) return;
    save_system(t);
    ui_free(t);
    gallery_free(t);
    snd_close(t);
    gfx_close(t);
    text_close(t);
    for (int i = 0; i < t->sceneCount; i++) scene_free(t->scenes[i]);
    free(t->scenes);
    for (int i = 0; i < t->backlogCount; i++) {
        free(t->backlog[i].name);
        free(t->backlog[i].text);
    }
    free(t->backlog);
    free(t->msgName);
    free(t->msgText);
    ini_free(&t->sys);
    ini_free(&t->fileMacro);
    ini_free(&t->charaMacro);
    ini_free(&t->varIni);
    ini_free(&t->charaView);
    store_close(&t->store);
    free(t->system000);
    free(t->readMarks);
    free(t->galleryMarks);
    free(t->title);
    free(t->gameDir);
    free(t->saveDir);
    if (t->log) fclose(t->log);
    free(t);
}

static void update_text(TH *t) {
    if (t->wait != W_TEXT) return;
    if (!msg_complete(t)) {
        int per = t->textSpeed >= 9 ? 0 : (9 - t->textSpeed) * 8;
        int shown = per ? (int)((t->now - t->msgStart) / (u32)per) : t->msgChars;
        if (shown > t->msgChars) shown = t->msgChars;
        if (shown != t->msgShown) {
            t->msgShown = shown;
            t->dirty = true;
        }
    }
    if (t->wheel > 0 && t->backlogCount > 0) {
        ui_open(t, UI_BACKLOG);
        return;
    }
    bool advance = t->click || t->keyPress[0x0d] || t->keyPress[0x20] || t->wheel < 0;
    if (advance && (t->autoMode || t->skipMode)) {
        t->autoMode = t->skipMode = false;
        t->dirty = true;
        return;
    }
    if (t->rclick) {
        t->winHidden = !t->winHidden;
        t->dirty = true;
        return;
    }
    if (t->winHidden) {
        if (advance) {
            t->winHidden = false;
            t->dirty = true;
        }
        return;
    }
    if (t->skipMode && !t->msgRead && !ini_int(&t->sys, "SYSTEM", "SYS_DEF_SKIP", 0)) {
        t->skipMode = false;
        t->dirty = true;
    }
    if (t->skipMode || t->keyDown[0x11]) {
        t->msgShown = t->msgChars;
        msg_advance(t);
        return;
    }
    if (t->autoMode && msg_complete(t) && !snd_playing(t, CH_VOICE)) {
        if (!t->autoAt) t->autoAt = t->now + (u32)(10 - t->autoSpeed) * 300 + (u32)t->msgChars * 20;
        if (t->now >= t->autoAt) {
            msg_advance(t);
            return;
        }
    }
    if (advance) {
        if (msg_complete(t) && !t->skipMode) snd_stop(t, CH_VOICE, 0);
        msg_advance(t);
    }
}

bool vnth_frame(VNTH *v, uint32_t elapsedMs) {
    TH *t = (TH *)v;
    if (t->quit) return false;
    if (t->paused) return true;
    t->now += elapsedMs > 100 ? 100 : elapsedMs;
    if (t->uiMode != UI_NONE) {
        ui_update(t);
    } else if (!ui_bar_update(t)) {
        switch (t->wait) {
        case W_TIME:
            if (t->now >= t->waitUntil || (t->waitSkippable && (t->click || t->keyDown[0x11]))) {
                t->wait = W_NONE;
                t->waitSkippable = false;
                if (t->pan.active) {
                    t->pan.active = false;
                    t->bg = t->pan.img;
                    t->pan.img = NULL;
                    t->sceneDirty = true;
                }
            }
            break;
        case W_FLASH:
            if (t->now >= t->waitUntil || t->click) t->wait = W_NONE;
            break;
        case W_TRANS:
            if (gfx_transition_done(t) || t->click || t->keyDown[0x11]) {
                gfx_finish_transition(t);
                t->wait = W_NONE;
            }
            break;
        case W_TEXT: update_text(t); break;
        case W_BUTTONS:
        case W_CHOICE:
        case W_MAP: buttons_update(t); break;
        case W_GALLERY:
        case W_CGVIEW: gallery_update(t); break;
        default: break;
        }
    }
    if (t->pan.active && t->wait != W_TIME && t->now - t->pan.start >= t->pan.dur) {
        t->pan.active = false;
        t->bg = t->pan.img;
        t->pan.img = NULL;
        t->sceneDirty = true;
    }
    vm_run(t);
    t->click = t->rclick = false;
    t->mouseMoved = false;
    t->wheel = 0;
    memset(t->keyPress, 0, sizeof t->keyPress);
    if (t->quit) return false;
    bool animating = t->transActive || t->pan.active || (t->flashDur && t->now - t->flashStart < t->flashDur + 100) ||
                     (t->wait == W_TEXT) || t->dirty || t->sceneDirty;
    if (animating) {
        gfx_compose(t);
        t->dirty = false;
    }
    return true;
}

const uint32_t *vnth_pixels(VNTH *v, int *w, int *h) {
    TH *t = (TH *)v;
    *w = t->W;
    *h = t->H;
    return t->screen;
}

uint32_t vnth_frame_serial(VNTH *v) { return ((TH *)v)->serial; }

void vnth_mouse_move(VNTH *v, int x, int y) {
    TH *t = (TH *)v;
    if (x != t->mx || y != t->my) {
        t->dirty = true;
        t->mouseMoved = true;
    }
    t->mx = x;
    t->my = y;
}

void vnth_mouse_button(VNTH *v, int button, bool down) {
    TH *t = (TH *)v;
    if (button == VNTH_MOUSE_LEFT) {
        if (down && !t->lDown) t->click = true;
        t->lDown = down;
    } else if (button == VNTH_MOUSE_RIGHT) {
        if (down && !t->rDown) t->rclick = true;
        t->rDown = down;
    }
}

void vnth_key(VNTH *v, int vk, bool down) {
    TH *t = (TH *)v;
    if (vk < 0 || vk > 255) return;
    if (down && !t->keyDown[vk]) t->keyPress[vk] = true;
    t->keyDown[vk] = down;
}

void vnth_wheel(VNTH *v, int notches) { ((TH *)v)->wheel += notches; }

void vnth_audio(VNTH *v, float *out, int frames) {
    TH *t = (TH *)v;
    mixer_mix(t->mixer, out, frames, t->paused);
}

void vnth_set_paused(VNTH *v, bool paused) { ((TH *)v)->paused = paused; }

void vnth_request_quit(VNTH *v) { ((TH *)v)->quit = true; }

const char *vnth_title(VNTH *v) { return ((TH *)v)->title; }

const char *vnth_problem(VNTH *v) {
    TH *t = (TH *)v;
    return t->problem[0] ? t->problem : NULL;
}

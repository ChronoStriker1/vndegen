#include "ipac.h"

#include <time.h>

#define TICK_MS (1000.0 / 60.0)

static bool game_info(Store *s, char **brand, char **title) {
    size_t n;
    u8 *d = store_load(s, PK_SCR, "seenINIT", ".ORG", &n);
    if (!d) return false;
    Script *sc = script_parse("seenINIT", d, n);
    free(d);
    bool found = false;
    for (int i = 0; i < sc->count && !found; i++) {
        const char *c = sc->st[i].s;
        if (sc->st[i].kind != S_CMD || strncmp(c, "GAMEINFO", 8)) continue;
        c += 8;
        while (*c == ' ' || *c == '\t') c++;
        const char *e = c;
        while (*e && *e != ' ' && *e != '\t') e += ip_sjis_len((const u8 *)e);
        char *b = ip_alloc((size_t)(e - c) + 1);
        memcpy(b, c, (size_t)(e - c));
        while (*e == ' ' || *e == '\t') e++;
        *brand = b;
        *title = ip_strdup(e);
        found = true;
    }
    script_free(sc);
    return found;
}

bool vnipac_probe(const char *gameDir, char *title, size_t titleSize) {
    Store s;
    if (!store_open(&s, gameDir, NULL, 0)) return false;
    bool ok = store_has(&s, PK_SCR, "TITLE", ".ORG") && store_has(&s, PK_SCR, "seenINIT", ".ORG");
    if (ok && title && titleSize) {
        char *b = NULL, *tt = NULL;
        title[0] = 0;
        if (game_info(&s, &b, &tt)) {
            char *u = ip_sjis_to_utf8(tt);
            snprintf(title, titleSize, "%s", u);
            free(u);
        }
        free(b);
        free(tt);
    }
    store_close(&s);
    return ok;
}

bool vnipac_save_folder(const char *gameDir, char *brand, size_t brandSize, char *title, size_t titleSize) {
    Store s;
    if (!store_open(&s, gameDir, NULL, 0)) return false;
    char *b = NULL, *tt = NULL;
    bool ok = game_info(&s, &b, &tt);
    if (ok) {
        char *ub = ip_sjis_to_utf8(b), *ut = ip_sjis_to_utf8(tt);
        snprintf(brand, brandSize, "%s", ub);
        snprintf(title, titleSize, "%s", ut);
        free(ub);
        free(ut);
    }
    free(b);
    free(tt);
    store_close(&s);
    return ok;
}

VNIPAC *vnipac_open(const VNIPACConfig *cfg, char *err, size_t errSize) {
    IP *t = ip_alloc(sizeof *t);
    if (cfg->logPath) t->log = fopen(cfg->logPath, "a");
    t->gameDir = ip_strdup(cfg->gameDir);
    t->saveDir = ip_strdup(cfg->saveDir ? cfg->saveDir : cfg->gameDir);
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
    game_info(&t->store, &t->brand, &t->title);
    if (!t->title) t->title = ip_strdup("");
    t->W = 800;
    t->H = 600;
    t->fontSize = 16;
    t->pitch = 24;
    t->textSpeed = 1;
    t->volBgm = t->volSe = t->volVoice = 255;
    t->bgmOn = t->seOn = t->voiceOn = true;
    t->menuEnabled = true;
    gfx_init(t);
    text_init(t);
    ui_init(t);
    t->mixer = mixer_new(t->audioRate);
    snd_volumes(t);
    load_system(t);
    vm_start(t, "seenINIT", NULL);
    if (!t->sc) {
        if (err) snprintf(err, errSize, "%s", t->problem);
        vnipac_close((VNIPAC *)t);
        return NULL;
    }
    ip_log(t, "opened %s", t->gameDir);
    return (VNIPAC *)t;
}

void vnipac_close(VNIPAC *n) {
    IP *t = (IP *)n;
    if (!t) return;
    mixer_free(t->mixer);
    ui_free(t);
    msg_clear(t);
    msg_reset();
    free(t->pendingText);
    for (int i = 0; i < t->logCount; i++) {
        free(t->logs[i].name);
        free(t->logs[i].text);
    }
    free(t->logs);
    for (int i = 0; i < t->vlistCount; i++) free(t->vlist[i].img);
    for (int i = 0; i < t->configCount; i++) {
        free(t->config[i].menu);
        free(t->config[i].item);
    }
    gfx_close(t);
    text_close();
    for (int i = 0; i < t->scriptCount; i++) script_free(t->scripts[i]);
    free(t->scripts);
    store_close(&t->store);
    if (t->log) fclose(t->log);
    free(t->gameDir);
    free(t->saveDir);
    free(t->title);
    free(t->brand);
    free(t);
}

static void advance_text(IP *t) {
    snd_voice_stop(t);
    t->wait = W_NONE;
}

static void handle_input(IP *t) {
    if (t->uiMode != UI_NONE) {
        ui_update(t);
        return;
    }
    if (t->rclick && t->wait == W_TEXT) {
        t->rclick = false;
        t->winHidden = !t->winHidden;
        t->dirty = true;
        return;
    }
    if (t->winHidden && t->click) {
        t->click = false;
        t->winHidden = false;
        t->dirty = true;
        return;
    }
    if (t->wait == W_TEXT && t->click) {
        t->click = false;
        int b = msg_button_at(t, t->mx, t->my);
        if (b) {
            ip_log(t, "window button %02x", b);
            return;
        }
        if (!msg_complete(t)) msg_reveal(t);
        else advance_text(t);
        return;
    }
    if (t->wait == W_CHOICE && t->click) {
        t->click = false;
        if (t->choiceHover >= 0) {
            t->sel = t->choiceHover + 1;
            t->choiceCount = 0;
            t->wait = W_NONE;
        }
    }
}

static void tick(IP *t) {
    if (t->clickMap) t->clk = gfx_click_at(t, t->mx, t->my);
    handle_input(t);
    gfx_update(t);
    switch (t->wait) {
    case W_FRAME: t->wait = W_NONE; break;
    case W_TIME:
        if ((i32)(t->now - t->waitUntil) >= 0) t->wait = W_NONE;
        break;
    case W_TRANS:
        if (gfx_transition_done(t)) t->wait = W_NONE;
        break;
    case W_TEXT: msg_update(t); break;
    default: break;
    }
    if (t->uiMode == UI_NONE) vm_run(t);
    t->click = false;
    t->rclick = false;
    t->wheel = 0;
    memset(t->keyPress, 0, sizeof t->keyPress);
}

bool vnipac_frame(VNIPAC *n, uint32_t elapsedMs) {
    IP *t = (IP *)n;
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
    if (t->dirty || t->transActive) {
        gfx_compose(t);
        t->dirty = false;
    }
    image_cache_trim(t);
    return !t->quit;
}

const uint32_t *vnipac_pixels(VNIPAC *n, int *width, int *height) {
    IP *t = (IP *)n;
    *width = t->W;
    *height = t->H;
    return t->screen;
}

uint32_t vnipac_frame_serial(VNIPAC *n) { return ((IP *)n)->serial; }

void vnipac_mouse_move(VNIPAC *n, int x, int y) {
    IP *t = (IP *)n;
    if (x != t->mx || y != t->my) t->dirty = true;
    t->mx = x;
    t->my = y;
}

void vnipac_mouse_button(VNIPAC *n, int button, bool down) {
    IP *t = (IP *)n;
    if (button == VNIPAC_MOUSE_LEFT) {
        if (t->lDown && !down) t->click = true;
        t->lDown = down;
    } else if (button == VNIPAC_MOUSE_RIGHT) {
        if (t->rDown && !down) t->rclick = true;
        t->rDown = down;
    }
}

void vnipac_key(VNIPAC *n, int vk, bool down) {
    IP *t = (IP *)n;
    if (vk < 0 || vk > 255) return;
    if (down && !t->keyDown[vk]) t->keyPress[vk] = true;
    t->keyDown[vk] = down;
    if (down && (vk == 0x0d || vk == 0x20)) t->click = true;
}

void vnipac_wheel(VNIPAC *n, int notches) { ((IP *)n)->wheel += notches; }

void vnipac_audio(VNIPAC *n, float *out, int frames) {
    IP *t = (IP *)n;
    mixer_mix(t->mixer, out, frames, t->paused);
}

void vnipac_set_paused(VNIPAC *n, bool paused) { ((IP *)n)->paused = paused; }

void vnipac_request_quit(VNIPAC *n) { ((IP *)n)->quit = true; }

const char *vnipac_title(VNIPAC *n) {
    IP *t = (IP *)n;
    static char buf[256];
    char *u = ip_sjis_to_utf8(t->title ? t->title : "");
    snprintf(buf, sizeof buf, "%s", u);
    free(u);
    return buf;
}

const char *vnipac_problem(VNIPAC *n) {
    IP *t = (IP *)n;
    return t->problem[0] ? t->problem : NULL;
}

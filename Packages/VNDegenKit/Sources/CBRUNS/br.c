#include "br.h"

#define TICK_MS 16

static const char *cfg_text(Node *res, const char *key, const char *dflt) {
    const char *s = sx_str(sx_cdr_value(sx_find(res, key)));
    return s ? s : dflt;
}

static int cfg_num(Node *res, const char *key, int dflt) { return sx_int(sx_cdr_value(sx_find(res, key)), dflt); }

static bool cfg_load(BR *e) {
    size_t n;
    u8 *d = br_read_game(e, "ams.cfg", &n);
    if (!d) return false;
    Cfg *c = &e->cfg;
    c->root = sx_parse((const char *)d, n);
    free(d);
    Node *res = sx_find(c->root, ":resource");
    snprintf(c->scriptDir, sizeof c->scriptDir, "%s", cfg_text(res, ":script-basedir", "scene"));
    snprintf(c->scriptBase, sizeof c->scriptBase, "%s", cfg_text(res, ":script-basename", "brs"));
    snprintf(c->scriptExt, sizeof c->scriptExt, "%s", cfg_text(res, ":script-extension", "bso"));
    c->scriptDigits = cfg_num(res, ":script-num-digits", 6);
    snprintf(c->imageDir, sizeof c->imageDir, "%s", cfg_text(res, ":storage-basedir", "image"));
    snprintf(c->partsDir, sizeof c->partsDir, "%s", cfg_text(res, ":parts-basedir", "parts"));
    snprintf(c->partsBase, sizeof c->partsBase, "%s", cfg_text(res, ":parts-basename", "pt"));
    c->partsDigits = cfg_num(res, ":parts-num-digits", 6);
    snprintf(c->musicDir, sizeof c->musicDir, "%s", cfg_text(res, ":music-basedir", "music"));
    snprintf(c->musicBase, sizeof c->musicBase, "%s", cfg_text(res, ":music-basename", "bruns"));
    c->musicDigits = cfg_num(res, ":music-num-digits", 2);
    snprintf(c->effectDir, sizeof c->effectDir, "%s", cfg_text(res, ":effect-basedir", "effect"));
    snprintf(c->effectBase, sizeof c->effectBase, "%s", cfg_text(res, ":effect-basename", "efc"));
    c->effectDigits = cfg_num(res, ":effect-num-digits", 4);
    snprintf(c->voiceDir, sizeof c->voiceDir, "%s", cfg_text(res, ":voice-basedir", "voice"));
    c->voiceDigits = cfg_num(res, ":voice-num-digits", 8);
    c->voiceSceneDigits = cfg_num(res, ":voice-scene-num-digits", 1);
    Node *consts = sx_find(c->root, ":flag-constants");
    c->initialScript = sx_int(sx_cdr_value(sx_assoc(consts, 1)), 9000);
    c->titleScript = 9000;
    Node *win = sx_find(c->root, ":window");
    const char *t = cfg_text(win, ":window-title", "");
    snprintf(e->title, sizeof e->title, "%s", t);
    return true;
}

static bool has_game(const char *dir) {
    char *a = br_fmt("%s/ams.cfg", dir), *b = br_fmt("%s/scene/brs009000.bso", dir), *c = br_fmt("%s/binary/bruns.exe", dir);
    bool ok = br_file_exists(a) && br_file_exists(b) && br_file_exists(c);
    free(a);
    free(b);
    free(c);
    return ok;
}

bool vnbruns_probe(const char *gameDir, char *title, size_t titleSize) {
    if (!has_game(gameDir)) return false;
    if (title && titleSize) {
        BR *tmp = br_alloc(sizeof *tmp);
        tmp->gameDir = (char *)gameDir;
        cfg_load(tmp);
        char *u = br_sjis_to_utf8(tmp->title);
        snprintf(title, titleSize, "%s", u);
        free(u);
        sx_free(tmp->cfg.root);
        free(tmp);
    }
    return true;
}

bool vnbruns_product(const char *gameDir, char *author, size_t authorSize, char *product, size_t productSize) {
    if (!has_game(gameDir)) return false;
    BR *tmp = br_alloc(sizeof *tmp);
    tmp->gameDir = (char *)gameDir;
    if (!cfg_load(tmp)) {
        free(tmp);
        return false;
    }
    Node *info = sx_find(tmp->cfg.root, ":product-info");
    const char *a = cfg_text(info, ":author-name", ""), *p = cfg_text(info, ":product-name", "");
    char *ua = br_sjis_to_utf8(a), *up = br_sjis_to_utf8(p);
    snprintf(author, authorSize, "%s", ua);
    snprintf(product, productSize, "%s", up);
    bool ok = *ua && *up;
    free(ua);
    free(up);
    sx_free(tmp->cfg.root);
    free(tmp);
    return ok;
}

VNBruns *vnbruns_open(const VNBrunsConfig *cfg, char *err, size_t errSize) {
    if (!cfg || !cfg->gameDir || !has_game(cfg->gameDir)) {
        if (err && errSize) snprintf(err, errSize, "No Bruns game was found in this folder.");
        return NULL;
    }
    BR *e = br_alloc(sizeof *e);
    e->gameDir = br_strdup(cfg->gameDir);
    e->saveDir = br_strdup(cfg->saveDir ? cfg->saveDir : cfg->gameDir);
    if (cfg->logPath) e->log = fopen(cfg->logPath, "a");
    e->audioRate = cfg->audioRate > 0 ? cfg->audioRate : 44100;
    e->trace = getenv("BRUNS_TRACE") != NULL;
    e->word = br_alloc(sizeof(i32) * BR_WORDS);
    e->screen = br_alloc(sizeof(u32) * BR_W * BR_H);
    e->unknownSeen = br_alloc(10000);
    e->nextScript = -2;
    e->rng = (u32)cfg->startTime;
    e->textSpeed = 1;
    e->menu.show = true;
    e->menu.autohide = true;
    e->menu.rclick = true;
    e->mixer = mixer_new();
    e->volMaster = 100;
    for (int i = 0; i < 3; i++) e->vol[i] = 100;
    for (int i = 0; i < BR_CHANNELS; i++) e->chanGain[i] = 1;
    for (int i = 0; i < 4; i++) e->music[i] = -1;
    e->autoMs = 1500;
    if (!cfg_load(e)) {
        if (err && errSize) snprintf(err, errSize, "The game's ams.cfg can't be read.");
        vnbruns_close((VNBruns *)e);
        return NULL;
    }
    br_log(e, "opened %s", e->gameDir);
    sys_load(e);
    cfg_apply(e);
    e->systemMode = true;
    vm_start(e, e->cfg.initialScript);
    if (!e->sp) {
        if (err && errSize) snprintf(err, errSize, "%s", e->problem[0] ? e->problem : "The game's first script can't be read.");
        vnbruns_close((VNBruns *)e);
        return NULL;
    }
    return (VNBruns *)e;
}

void vnbruns_close(VNBruns *n) {
    BR *e = (BR *)n;
    if (!e) return;
    if (e->sysDirty) sys_save(e);
    sx_free(e->sysRoot);
    sx_free(e->gameSnap);
    sx_free(e->loadedState);
    script_free_all(e);
    gfx_free(e);
    parts_free_all(e);
    font_free(e);
    msg_free(e);
    history_free(e);
    choice_free(e);
    mixer_free(e->mixer);
    image_cache_clear();
    sx_free(e->cfg.root);
    for (int i = 0; i < BR_STRINGS; i++) free(e->str[i]);
    if (e->log) fclose(e->log);
    free(e->word);
    free(e->screen);
    free(e->unknownSeen);
    free(e->gameDir);
    free(e->saveDir);
    free(e);
}

static void tick(BR *e) {
    if (e->skipMode && e->flag[257] == 1 && e->msg.unread && e->wait == W_TEXT) e->skipMode = false;
    e->skipping = e->ctrlHeld || e->skipMode;
    msg_update(e);
    if (menu_input(e)) e->lPress = false;
    if (!e->systemMode && e->wait == W_TEXT && e->rPress && e->menu.rclick) {
        e->rPress = false;
        sys_call(e, 9007);
    } else if (!e->systemMode && e->wait == W_TEXT && e->wheel > 0) {
        e->wheel = 0;
        sys_call(e, 9004);
    }
    uf_tick(e);
    vm_run(e);
    if (e->sysDirty && e->now - e->sysSavedAt > 2000) {
        sys_save(e);
        e->sysSavedAt = e->now;
    }
    e->lPress = e->rPress = false;
    e->wheel = 0;
}

bool vnbruns_frame(VNBruns *n, uint32_t elapsedMs) {
    BR *e = (BR *)n;
    if (e->paused) return !e->quit;
    if (elapsedMs > 250) elapsedMs = 250;
    e->tickAcc += elapsedMs;
    int ticks = 0;
    while (e->tickAcc >= TICK_MS && ticks < 8 && !e->quit && !e->ended) {
        e->tickAcc -= TICK_MS;
        e->now += TICK_MS;
        tick(e);
        ticks++;
    }
    if (e->tickAcc > TICK_MS * 8) e->tickAcc = 0;
    gfx_frame(e);
    return !e->quit && !e->ended;
}

const uint32_t *vnbruns_pixels(VNBruns *n, int *width, int *height) {
    *width = BR_W;
    *height = BR_H;
    return ((BR *)n)->screen;
}

uint32_t vnbruns_frame_serial(VNBruns *n) { return ((BR *)n)->serial; }

void vnbruns_mouse_move(VNBruns *n, int x, int y) {
    BR *e = (BR *)n;
    e->mouseX = x;
    e->mouseY = y;
}

void vnbruns_mouse_button(VNBruns *n, int button, bool down) {
    BR *e = (BR *)n;
    if (button == VNBRUNS_MOUSE_LEFT) {
        if (!e->lDown && down) e->lPress = true;
        e->lDown = down;
    } else if (button == VNBRUNS_MOUSE_RIGHT && down) e->rPress = true;
}

void vnbruns_key(VNBruns *n, int vk, bool down) {
    BR *e = (BR *)n;
    if (vk == 0x11) {
        e->ctrlHeld = down;
    }
    e->keyEvent = 65536 + vk * 4 + (down ? 0 : 1);
    if (!down) return;
    if (vk == 0x0d || vk == 0x20) e->lPress = true;
    if (vk == 0x1b) e->rPress = true;
}

void vnbruns_wheel(VNBruns *n, int notches) { ((BR *)n)->wheel += notches; }

void vnbruns_audio(VNBruns *n, float *out, int frames) {
    BR *e = (BR *)n;
    mixer_mix(e->mixer, out, frames, e->paused);
}

void vnbruns_set_paused(VNBruns *n, bool paused) { ((BR *)n)->paused = paused; }

void vnbruns_request_quit(VNBruns *n) { ((BR *)n)->quit = true; }

const char *vnbruns_title(VNBruns *n) {
    static char buf[256];
    char *u = br_sjis_to_utf8(((BR *)n)->title);
    snprintf(buf, sizeof buf, "%s", u);
    free(u);
    return buf;
}

const char *vnbruns_problem(VNBruns *n) {
    BR *e = (BR *)n;
    return e->problem[0] ? e->problem : NULL;
}

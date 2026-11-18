#include "eagls.h"
#include "eg_casepath.h"

#include <dirent.h>
#include <time.h>

#define TICK_MS (1000.0 / 60.0)

static bool has_data(const char *dir) {
    char path[2048];
    snprintf(path, sizeof path, "%s/Script/SCPACK.idx", dir);
    if (!eg_file_exists(path)) return false;
    snprintf(path, sizeof path, "%s/CG/CGPACK.idx", dir);
    return eg_file_exists(path);
}

static char *find_data_dir(const char *gameDir) {
    if (has_data(gameDir)) return eg_strdup(gameDir);
    DIR *d = opendir(gameDir);
    if (!d) return NULL;
    char *found = NULL;
    struct dirent *de;
    while (!found && (de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char sub[2048];
        snprintf(sub, sizeof sub, "%s/%s", gameDir, de->d_name);
        if (has_data(sub)) found = eg_strdup(sub);
    }
    closedir(d);
    return found;
}

static char *read_title(const char *dataDir) {
    char path[2048];
    snprintf(path, sizeof path, "%s/EAGLS.ini", dataDir);
    size_t n;
    u8 *d = eg_read_file(path, &n);
    if (!d) return NULL;
    Ini ini;
    ini_parse(&ini, d, n);
    free(d);
    const char *t = ini_get(&ini, "WindowTitle");
    char *r = t ? eg_strdup(t) : NULL;
    ini_free(&ini);
    return r;
}

bool vneagls_probe(const char *gameDir, char *title, size_t titleSize) {
    char *dir = find_data_dir(gameDir);
    if (!dir) return false;
    if (title && titleSize) {
        char *t = read_title(dir);
        char *u = t ? eg_sjis_to_utf8(t) : eg_strdup("");
        snprintf(title, titleSize, "%s", u);
        free(u);
        free(t);
    }
    free(dir);
    return true;
}

VNEAGLS *vneagls_open(const VNEAGLSConfig *cfg, char *err, size_t errSize) {
    EG *e = eg_alloc(sizeof *e);
    if (cfg->logPath) e->log = fopen(cfg->logPath, "a");
    e->gameDir = eg_strdup(cfg->gameDir);
    e->dataDir = find_data_dir(cfg->gameDir);
    if (!e->dataDir) {
        if (err) snprintf(err, errSize, "no EAGLS game in %s", cfg->gameDir);
        if (e->log) fclose(e->log);
        free(e->gameDir);
        free(e);
        return NULL;
    }
    e->saveDir = eg_strdup(cfg->saveDir ? cfg->saveDir : e->dataDir);
    e->audioRate = cfg->audioRate > 0 ? cfg->audioRate : 44100;
    e->startTime = cfg->startTime ? cfg->startTime : (long long)time(NULL);
    e->rng = (u32)e->startTime;
    e->trace = getenv("EAGLS_TRACE") != NULL;
    static const char *paks[PK_COUNT][2] = {{"CG", "CGPACK"}, {"Script", "SCPACK"}, {"Wave", "WAVEPACK"}};
    for (int i = 0; i < PK_COUNT; i++) {
        char dir[2048];
        snprintf(dir, sizeof dir, "%s/%s", e->dataDir, paks[i][0]);
        if (!pak_open(&e->pak[i], dir, paks[i][1]) && i != PK_WAVE) {
            if (err) snprintf(err, errSize, "can't read %s/%s", paks[i][0], paks[i][1]);
            vneagls_close((VNEAGLS *)e);
            return NULL;
        }
    }
    char path[2048];
    snprintf(path, sizeof path, "%s/EAGLS.ini", e->dataDir);
    size_t n;
    u8 *d = eg_read_file(path, &n);
    ini_parse(&e->ini, d, n);
    free(d);
    const char *t = ini_get(&e->ini, "WindowTitle");
    e->title = eg_strdup(t ? t : "");
    int size[2] = {800, 600};
    ini_ints(&e->ini, "ScreenXsize", &size[0], 1);
    ini_ints(&e->ini, "ScreenYsize", &size[1], 1);
    e->W = size[0] > 0 ? size[0] : 800;
    e->H = size[1] > 0 ? size[1] : 600;
    e->textSpeedSlider = 50;
    e->textSpeed = 20;
    e->autoSpeed = 50;
    e->autoWaitVoice = 1;
    e->volMaster = e->volBgm = e->volSe = e->volVoice = e->volMovie = 100;
    e->mesAlpha = 255;
    gfx_init(e);
    text_init(e);
    ui_init(e);
    e->mixer = mixer_new(e->audioRate);
    global_load(e);
    snd_volumes(e);
    vm_init(e);
    if (!e->th[0].active) {
        if (err) snprintf(err, errSize, "can't start IniScriptPri.dat");
        vneagls_close((VNEAGLS *)e);
        return NULL;
    }
    eg_log(e, "opened %s", e->dataDir);
    return (VNEAGLS *)e;
}

void vneagls_close(VNEAGLS *n) {
    EG *e = (EG *)n;
    if (!e) return;
    if (e->ui) config_save(e);
    movie_close_all(e);
    image_free(e->thumb);
    mixer_free(e->mixer);
    ui_free(e);
    msg_clear(e);
    free(e->text);
    for (int i = 0; i < e->logCount; i++) {
        free(e->logs[i].name);
        free(e->logs[i].text);
        free(e->logs[i].voice);
    }
    free(e->logs);
    for (int i = 0; i < 8; i++) {
        image_free(e->mesImg[i]);
        free(e->choice[i].text);
    }
    for (int i = 0; i < 4; i++) free(e->nameFmt[i]);
    if (e->screen) gfx_close(e);
    for (int i = 0; i < e->scriptCount; i++) script_free(e->scripts[i]);
    free(e->scripts);
    var_clear(&e->vars);
    for (int i = 0; i < PK_COUNT; i++) pak_close(&e->pak[i]);
    ini_free(&e->ini);
    free(e->global);
    if (e->log) fclose(e->log);
    free(e->gameDir);
    free(e->dataDir);
    free(e->saveDir);
    free(e->title);
    free(e);
}

static i32 sysvar(EG *e, const char *name) {
    Var *v = var_find(&e->vars, name);
    return v ? var_at(v, 0)->i : 0;
}

static void set_sysvar(EG *e, const char *name, i32 value) {
    Var *v = var_get(&e->vars, name, true);
    var_at(v, 0)->i = value;
}

static void advance(EG *e) {
    e->th[0].wait = W_NONE;
    e->autoAt = 0;
}

static void text_wait(EG *e) {
    Thread *t = &e->th[0];
    if (t->wait != W_TEXT) return;
    msg_update(e);
    bool skip = sysvar(e, ":SkipFlg") || e->keyDown[0x11];
    if (e->editing) return;
    if (skip) {
        msg_reveal(e);
        advance(e);
        return;
    }
    if (e->click) {
        e->click = false;
        if (sysvar(e, ":AutoFlg")) set_sysvar(e, ":AutoFlg", 0);
        if (!msg_complete(e)) msg_reveal(e);
        else advance(e);
        return;
    }
    if (sysvar(e, ":AutoFlg") && msg_complete(e)) {
        if (e->autoWaitVoice && snd_playing(e, 6)) return;
        if (!e->autoAt) e->autoAt = e->now + 500 + (u32)(100 - e->autoSpeed) * 70;
        if ((i32)(e->now - e->autoAt) >= 0) advance(e);
    }
}

void game_to_title(EG *e) {
    snd_stop_all(e);
    movie_close_all(e);
    msg_clear(e);
    e->bgmName[0][0] = e->bgmName[1][0] = 0;
    e->choiceCount = 0;
    e->winHidden = false;
    e->editing = false;
    e->uiMode = UI_NONE;
    e->scriptUi = false;
    for (int i = 1; i < EG_THREADS; i++) e->th[i].active = false;
    Thread *t = &e->th[0];
    t->sp = 0;
    t->wait = W_NONE;
    vm_goto(e, t, "TitleGame.dat", "TitleGame");
    config_save(e);
}

static void edit_key(EG *e, int vk) {
    size_t n = strlen(e->editBuf);
    if (vk == 0x0d) {
        e->editing = false;
        return;
    }
    if (vk == 0x08) {
        if (n) {
            size_t k = 0, last = 0;
            while (k < n) {
                last = k;
                k += (size_t)eg_sjis_len((const u8 *)e->editBuf + k);
            }
            e->editBuf[last] = 0;
        }
        return;
    }
    char c = 0;
    if (vk >= 'A' && vk <= 'Z') c = (char)(e->keyDown[0x10] ? vk : vk + 32);
    else if (vk >= '0' && vk <= '9') c = (char)vk;
    else if (vk == 0x20) c = ' ';
    if (!c || (int)n >= e->editMax || n + 1 >= sizeof e->editBuf) return;
    e->editBuf[n] = c;
    e->editBuf[n + 1] = 0;
}

static void tick(EG *e) {
    if (e->scriptUi && e->scriptUiDone) {
        e->scriptUi = false;
        if (e->th[0].wait == W_UI) e->th[0].wait = W_NONE;
    }
    if (e->editing) {
        for (int k = 0; k < 256; k++)
            if (e->keyPress[k]) edit_key(e, k);
        if (e->click && e->mx >= e->editRect[0] && e->mx < e->editRect[2] && e->my >= e->editRect[1] && e->my < e->editRect[3]) e->editing = false;
        e->click = false;
    }
    if (e->uiMode != UI_NONE) ui_update(e);
    else {
        win_update(e);
        if (e->th[0].wait == W_CHOICE) choice_update(e);
        if (e->rclick && e->th[0].wait == W_TEXT && !sysvar(e, ":RightClickFlg")) {
            e->rclick = false;
            e->winHidden = !e->winHidden;
        }
        if (e->winHidden && e->click) {
            e->click = false;
            e->winHidden = false;
        }
        text_wait(e);
        if (e->click) e->clicked = true;
        if (e->rclick) e->rclicked = true;
    }
    if (e->uiMode == UI_NONE) vm_run(e);
    movie_update(e);
    e->click = false;
    e->rclick = false;
    e->lPress = false;
    e->wheel = 0;
    memset(e->keyPress, 0, sizeof e->keyPress);
}

bool vneagls_frame(VNEAGLS *n, uint32_t elapsedMs) {
    EG *e = (EG *)n;
    if (e->paused) return !e->quit;
    if (elapsedMs > 250) elapsedMs = 250;
    e->tickAcc += elapsedMs;
    int ticks = 0;
    while (e->tickAcc >= TICK_MS && ticks < 8 && !e->quit) {
        e->tickAcc -= TICK_MS;
        e->clockMs += TICK_MS;
        e->now = (u32)e->clockMs;
        tick(e);
        ticks++;
        bool any = false;
        for (int i = 0; i < EG_THREADS; i++) any |= e->th[i].active;
        if (!any) e->quit = true;
    }
    if (e->tickAcc > TICK_MS * 8) e->tickAcc = 0;
    gfx_frame(e);
    return !e->quit;
}

const uint32_t *vneagls_pixels(VNEAGLS *n, int *width, int *height) {
    EG *e = (EG *)n;
    *width = e->W;
    *height = e->H;
    return e->screen;
}

uint32_t vneagls_frame_serial(VNEAGLS *n) { return ((EG *)n)->serial; }

void vneagls_mouse_move(VNEAGLS *n, int x, int y) {
    EG *e = (EG *)n;
    e->mx = x;
    e->my = y;
}

void vneagls_mouse_button(VNEAGLS *n, int button, bool down) {
    EG *e = (EG *)n;
    if (button == VNEAGLS_MOUSE_LEFT) {
        if (e->lDown && !down) e->click = true;
        if (!e->lDown && down) e->lPress = true;
        e->lDown = down;
    } else if (button == VNEAGLS_MOUSE_RIGHT) {
        if (e->rDown && !down) e->rclick = true;
        e->rDown = down;
    }
}

void vneagls_key(VNEAGLS *n, int vk, bool down) {
    EG *e = (EG *)n;
    if (vk < 0 || vk > 255) return;
    if (down && !e->keyDown[vk]) e->keyPress[vk] = true;
    e->keyDown[vk] = down;
    if (e->editing) return;
    if (down && (vk == 0x0d || vk == 0x20)) e->click = true;
    if (down && vk == 0x1b) e->rclick = true;
}

void vneagls_wheel(VNEAGLS *n, int notches) { ((EG *)n)->wheel += notches; }

void vneagls_audio(VNEAGLS *n, float *out, int frames) {
    EG *e = (EG *)n;
    mixer_mix(e->mixer, out, frames, e->paused);
}

void vneagls_set_paused(VNEAGLS *n, bool paused) { ((EG *)n)->paused = paused; }

void vneagls_request_quit(VNEAGLS *n) { ((EG *)n)->quit = true; }

const char *vneagls_title(VNEAGLS *n) {
    EG *e = (EG *)n;
    static char buf[256];
    char *u = eg_sjis_to_utf8(e->title ? e->title : "");
    snprintf(buf, sizeof buf, "%s", u);
    free(u);
    return buf;
}

const char *vneagls_problem(VNEAGLS *n) {
    EG *e = (EG *)n;
    return e->problem[0] ? e->problem : NULL;
}

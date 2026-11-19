#include "ex.h"

#include <time.h>

#define TICK_MS 16

static char *ini_value(const char *ini, const char *key) {
    size_t kl = strlen(key);
    for (const char *p = ini; p && *p;) {
        const char *eol = strpbrk(p, "\r\n");
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        if (len > kl && !strncmp(p, key, kl) && p[kl] == '=') {
            char *v = ex_alloc(len - kl);
            memcpy(v, p + kl + 1, len - kl - 1);
            return v;
        }
        if (!eol) break;
        p = eol + 1;
        while (*p == '\r' || *p == '\n') p++;
    }
    return NULL;
}

static char *read_ini(const char *gameDir) {
    char *path = ex_fmt("%s/ExHIBIT.ini", gameDir);
    size_t n;
    char *d = (char *)ex_read_file(path, &n);
    free(path);
    return d;
}

static bool has_game(const char *dir) {
    char *a = ex_fmt("%s/ExHIBIT.ini", dir), *b = ex_fmt("%s/rld/start.rld", dir), *c = ex_fmt("%s/resident.dll", dir);
    bool ok = ex_file_exists(a) && ex_file_exists(b) && ex_file_exists(c);
    free(a);
    free(b);
    free(c);
    return ok;
}

bool vnexhibit_probe(const char *gameDir, char *title, size_t titleSize) {
    if (!has_game(gameDir)) return false;
    if (title && titleSize) {
        char *ini = read_ini(gameDir);
        char *t = ini ? ini_value(ini, "TITLE") : NULL;
        char *u = t ? ex_sjis_to_utf8(t) : ex_strdup("");
        snprintf(title, titleSize, "%s", u);
        free(u);
        free(t);
        free(ini);
    }
    return true;
}

VNExHibit *vnexhibit_open(const VNExHibitConfig *cfg, char *err, size_t errSize) {
    if (!cfg || !cfg->gameDir || !has_game(cfg->gameDir)) {
        if (err && errSize) snprintf(err, errSize, "No ExHibit game was found in this folder.");
        return NULL;
    }
    EX *e = ex_alloc(sizeof *e);
    e->gameDir = ex_strdup(cfg->gameDir);
    e->saveDir = ex_strdup(cfg->saveDir ? cfg->saveDir : cfg->gameDir);
    e->cacheDir = cfg->cacheDir ? ex_strdup(cfg->cacheDir) : NULL;
    if (cfg->logPath) e->log = fopen(cfg->logPath, "a");
    e->audioRate = cfg->audioRate > 0 ? cfg->audioRate : 44100;
    e->trace = getenv("EXHIBIT_TRACE") != NULL;
    char *ini = read_ini(cfg->gameDir);
    char *v;
    e->nR = (v = ini_value(ini, "N_REG")) ? atoi(v) : 2048;
    free(v);
    e->nS = (v = ini_value(ini, "N_SYSREG")) ? atoi(v) : 10240;
    free(v);
    if (e->nR < 2048) e->nR = 2048;
    if (e->nS < 10240) e->nS = 10240;
    e->title = ini_value(ini, "TITLE");
    char *entry = ini_value(ini, "entry");
    free(ini);
    e->R = ex_alloc(sizeof(i32) * (size_t)e->nR);
    e->S = ex_alloc(sizeof(i32) * (size_t)e->nS);
    e->screen = ex_alloc(sizeof(u32) * EX_W * EX_H);
    e->cgColor = 0xff000000u;
    e->volMaster = e->volBgm = e->volSe = e->volVoice = 100;
    e->mixer = mixer_new(e->audioRate);
    e->textMs = 20;
    e->msg.tempA = 255;
    e->cam.w = EX_W;
    e->cam.h = EX_H;
    ex_log(e, "opened %s", e->gameDir);
    rld_init(e);
    e->bootEntry = ex_strdup(entry ? entry : "start");
    free(entry);
    if (rld_get(e, "def")) vm_goto_script(e, "def");
    else {
        vm_goto_script(e, e->bootEntry);
        free(e->bootEntry);
        e->bootEntry = NULL;
    }
    if (!e->sc) {
        if (err && errSize) snprintf(err, errSize, "%s", e->problem[0] ? e->problem : "The game's first script can't be read.");
        vnexhibit_close((VNExHibit *)e);
        return NULL;
    }
    return (VNExHibit *)e;
}

void vnexhibit_close(VNExHibit *n) {
    EX *e = (EX *)n;
    if (!e) return;
    mixer_free(e->mixer);
    view_destroy(e);
    for (int i = 0; i < EX_BILLBOARDS; i++)
        if (e->bb[i].used) {
            if (e->bb[i].ownImg) image_free(e->bb[i].img);
            image_free(e->bb[i].mask);
        }
    image_free(e->cg);
    image_free(e->cgPrev);
    msg_free(e);
    chara_free(e);
    text_free(e);
    free(e->fontFile);
    free(e->bootEntry);
    rld_free_all(e);
    if (e->log) fclose(e->log);
    free(e->R);
    free(e->S);
    free(e->screen);
    free(e->title);
    free(e->unknownSeen);
    free(e->gameDir);
    free(e->saveDir);
    free(e->cacheDir);
    free(e);
}

static void tick(EX *e) {
    view_input(e);
    if (e->wait == W_TEXT || e->msg.userHidden) {
        if (e->rPress && !e->view.active) {
            e->msg.userHidden = !e->msg.userHidden;
            e->rPress = false;
        }
        if (e->msg.userHidden && e->lPress) {
            e->msg.userHidden = false;
            e->lPress = false;
        }
    }
    vm_run(e);
    e->lPress = false;
    e->rPress = false;
}

bool vnexhibit_frame(VNExHibit *n, uint32_t elapsedMs) {
    EX *e = (EX *)n;
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

const uint32_t *vnexhibit_pixels(VNExHibit *n, int *width, int *height) {
    EX *e = (EX *)n;
    *width = EX_W;
    *height = EX_H;
    return e->screen;
}

uint32_t vnexhibit_frame_serial(VNExHibit *n) { return ((EX *)n)->serial; }

void vnexhibit_mouse_move(VNExHibit *n, int x, int y) {
    EX *e = (EX *)n;
    e->mouseX = x;
    e->mouseY = y;
}

void vnexhibit_mouse_button(VNExHibit *n, int button, bool down) {
    EX *e = (EX *)n;
    if (button == VNEXHIBIT_MOUSE_LEFT) {
        if (!e->lDown && down) e->lPress = true;
        e->lDown = down;
    } else if (button == VNEXHIBIT_MOUSE_RIGHT && down) e->rPress = true;
}

void vnexhibit_key(VNExHibit *n, int vk, bool down) {
    EX *e = (EX *)n;
    if (!down) return;
    if (vk == 0x0d || vk == 0x20) e->lPress = true;
    if (vk == 0x1b) e->rPress = true;
}

void vnexhibit_wheel(VNExHibit *n, int notches) { ((EX *)n)->wheel += notches; }

void vnexhibit_audio(VNExHibit *n, float *out, int frames) {
    EX *e = (EX *)n;
    mixer_mix(e->mixer, out, frames, e->paused);
}

void vnexhibit_set_paused(VNExHibit *n, bool paused) { ((EX *)n)->paused = paused; }

void vnexhibit_request_quit(VNExHibit *n) { ((EX *)n)->quit = true; }

const char *vnexhibit_title(VNExHibit *n) {
    EX *e = (EX *)n;
    static char buf[256];
    char *u = ex_sjis_to_utf8(e->title ? e->title : "");
    snprintf(buf, sizeof buf, "%s", u);
    free(u);
    return buf;
}

const char *vnexhibit_problem(VNExHibit *n) {
    EX *e = (EX *)n;
    return e->problem[0] ? e->problem : NULL;
}

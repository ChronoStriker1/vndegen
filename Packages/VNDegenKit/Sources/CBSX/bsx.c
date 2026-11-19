#include "bsx.h"

#include <dirent.h>
#include <time.h>

#define TICK_MS (1000.0 / 60.0)

static bool has_game(const char *dir) {
    char path[2048];
    snprintf(path, sizeof path, "%s/bsxx.dat", dir);
    size_t n;
    u8 *d = bx_read_file(path, &n);
    bool ok = d && n > 16 && !memcmp(d, "BSXScript 3.", 12);
    free(d);
    return ok;
}

static char *find_game_dir(const char *gameDir) {
    if (has_game(gameDir)) return bx_strdup(gameDir);
    DIR *d = opendir(gameDir);
    if (!d) return NULL;
    char *found = NULL;
    struct dirent *de;
    while (!found && (de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char sub[2048];
        snprintf(sub, sizeof sub, "%s/%s", gameDir, de->d_name);
        if (has_game(sub)) found = bx_strdup(sub);
    }
    closedir(d);
    return found;
}

static bool hex_id(const u8 *d, size_t n) {
    if (n < 32) return false;
    for (int i = 0; i < 32; i++)
        if (!((d[i] >= '0' && d[i] <= '9') || (d[i] >= 'a' && d[i] <= 'f'))) return false;
    return true;
}

static void find_save_id(BX *b) {
    static const char *files[] = {"common.dat", "no-01", "no000", "no015"};
    for (int i = 0; i < 4; i++) {
        char path[2048];
        snprintf(path, sizeof path, "%s/save/%s", b->saveDir, files[i]);
        size_t n;
        u8 *d = bx_read_file(path, &n);
        if (!d) {
            snprintf(path, sizeof path, "%s/save/%s", b->gameDir, files[i]);
            d = bx_read_file(path, &n);
        }
        bool ok = d && hex_id(d, n);
        if (ok) memcpy(b->saveId, d, 32);
        free(d);
        if (ok) return;
    }
    static const char *known[][2] = {{"MKIEDL@BISHOP", "ee0732d311f15480cd6877c1557f682c"},
                                     {"MK2IEDL@BISHOP", "be8b3a08df96f11b4d0745bd4253dbdc"},
                                     {"MK4@BISHOP", "1b5ab07b7a199b7ae35af245e0374b6c"}};
    char path[2048];
    snprintf(path, sizeof path, "%s/bsxx.ini", b->gameDir);
    size_t n;
    u8 *d = bx_read_file(path, &n);
    size_t len = 0;
    while (d && len + 1 < n && (d[len] || d[len + 1])) len += 2;
    char *product = d ? bx_utf16_to_utf8(d, len / 2) : bx_strdup("");
    free(d);
    for (size_t i = 0; i < sizeof known / sizeof *known; i++)
        if (!strcmp(product, known[i][0])) memcpy(b->saveId, known[i][1], 32);
    if (!b->saveId[0]) bx_log(b, "no save id for %s; the PC engine won't load these saves", product);
    free(product);
}

static char *read_title(const char *dir) {
    char path[2048];
    snprintf(path, sizeof path, "%s/bsxx.ini", dir);
    size_t n;
    u8 *d = bx_read_file(path, &n);
    if (!d) return NULL;
    size_t start = 0, field = 0;
    char *title = NULL;
    for (size_t i = 0; i + 1 < n; i += 2) {
        if (d[i] || d[i + 1]) continue;
        if (field == 1 && i > start) title = bx_utf16_to_utf8(d + start, (i - start) / 2);
        field++;
        start = i + 2;
        if (field > 1) break;
    }
    free(d);
    return title;
}

bool vnbsx_probe(const char *gameDir, char *title, size_t titleSize) {
    char *dir = find_game_dir(gameDir);
    if (!dir) return false;
    if (title && titleSize) {
        char *t = read_title(dir);
        snprintf(title, titleSize, "%s", t ? t : "");
        free(t);
    }
    free(dir);
    return true;
}

VNBSX *vnbsx_open(const VNBSXConfig *cfg, char *err, size_t errSize) {
    BX *b = bx_alloc(sizeof *b);
    if (cfg->logPath) b->log = fopen(cfg->logPath, "a");
    b->gameDir = find_game_dir(cfg->gameDir);
    if (!b->gameDir) {
        if (err) snprintf(err, errSize, "no BSX game in %s", cfg->gameDir);
        if (b->log) fclose(b->log);
        free(b);
        return NULL;
    }
    b->saveDir = bx_strdup(cfg->saveDir ? cfg->saveDir : b->gameDir);
    b->audioRate = cfg->audioRate > 0 ? cfg->audioRate : 44100;
    b->startTime = cfg->startTime ? cfg->startTime : (long long)time(NULL);
    b->rng = (u32)b->startTime;
    b->trace = getenv("BSX_TRACE") != NULL;
    char path[2048];
    snprintf(path, sizeof path, "%s/bsxx.dat", b->gameDir);
    b->sc = bx_script_load(path, err, errSize);
    if (!b->sc || !bx_archives_open(&b->arc, b->gameDir)) {
        if (b->sc && err) snprintf(err, errSize, "can't read the game's .bsa archives");
        vnbsx_close((VNBSX *)b);
        return NULL;
    }
    snprintf(path, sizeof path, "%s/bsxx.cfg", b->gameDir);
    size_t n;
    u8 *d = bx_read_file(path, &n);
    bx_cfg_parse(&b->cfg, d, n);
    free(d);
    b->W = bx_cfg_int(&b->cfg, "main window", "width", 800);
    b->H = bx_cfg_int(&b->cfg, "main window", "height", 600);
    b->title = read_title(b->gameDir);
    find_save_id(b);
    if (!b->title) b->title = bx_strdup("");
    bx_gfx_init(b);
    bx_text_init(b);
    bx_msg_init(b);
    bx_ui_init(b);
    b->mixer = bx_mixer_new(b->audioRate);
    bx_settings_load(b);
    bx_common_load(b);
    bx_vm_init(b);
    bx_log(b, "opened %s, %s, %d labels, %d messages", b->gameDir, b->sc->version, b->sc->labelCount, b->sc->msgCount);
    return (VNBSX *)b;
}

void vnbsx_close(VNBSX *n) {
    BX *b = (BX *)n;
    if (!b) return;
    if (b->sc) bx_common_save(b);
    free(b->common);
    bx_mixer_free(b->mixer);
    bx_ui_free(b);
    bx_msg_clear(b);
    for (int i = 0; i < b->logCount; i++) {
        free(b->logs[i].name);
        free(b->logs[i].text);
        free(b->logs[i].voice);
    }
    free(b->logs);
    if (b->screen) bx_gfx_close(b);
    bx_script_free(b->sc);
    bx_archives_close(&b->arc);
    bx_cfg_free(&b->cfg);
    free(b->globals);
    if (b->log) fclose(b->log);
    free(b->gameDir);
    free(b->saveDir);
    free(b->title);
    free(b);
}

static void advance(BX *b) {
    b->th.wait = W_NONE;
    b->autoAt = 0;
}

static bool skip_to_label(BX *b) {
    if (b->th.skipLabel < 0) return false;
    int l = b->th.skipLabel;
    b->th.skipLabel = -1;
    b->transActive = false;
    bx_transition(b, 0, 0, NULL);
    b->th.wait = W_NONE;
    bx_vm_goto(b, l);
    return true;
}

static void text_wait(BX *b) {
    bx_msg_update(b);
    bool skip = b->skipMode || b->keyDown[0x11];
    if (skip) {
        bx_msg_reveal(b);
        if (b->click) {
            b->click = false;
            b->skipMode = false;
        }
        advance(b);
        return;
    }
    if (b->click) {
        b->click = false;
        if (b->autoMode) b->autoMode = false;
        if (!bx_msg_complete(b)) bx_msg_reveal(b);
        else advance(b);
        return;
    }
    if (b->autoMode && bx_msg_complete(b)) {
        if (bx_snd_playing(b, CH_VOICE)) return;
        if (!b->autoAt) b->autoAt = b->now + 500 + (u32)(100 - clampi(b->set.autoSpeed, 0, 100)) * 30;
        if ((i32)(b->now - b->autoAt) >= 0) advance(b);
    }
}

static void tick(BX *b) {
    bx_anim_update(b);
    if (b->phase != PH_NONE) bx_phase_update(b);
    else {
        bx_ui_update(b);
        Thread *t = &b->th;
        if (b->rclick && t->wait == W_TEXT) {
            b->rclick = false;
            b->winHidden = !b->winHidden;
        }
        if (b->winHidden && b->click) {
            b->click = false;
            b->winHidden = false;
        }
        if (t->wait == W_TEXT && (b->wheel > 0 || b->keyPress[0x21]) && bx_log_open(b)) {
            b->wheel = 0;
        } else if (t->wait == W_TEXT && b->keyPress[0x70]) {
            bx_save_capture_thumb(b);
            bx_save_slot(b, -1);
        } else if (t->wait == W_TEXT && b->keyPress[0x71]) bx_load_slot(b, -1);
        switch (t->wait) {
        case W_TEXT:
            text_wait(b);
            break;
        case W_CHOICE:
            bx_choice_update(b);
            break;
        case W_TIME:
            if (b->click && skip_to_label(b)) b->click = false;
            else if ((i32)(b->now - t->until) >= 0 || b->skipMode) t->wait = W_NONE;
            break;
        case W_TRANS:
            if (b->click && skip_to_label(b)) b->click = false;
            else if (!b->transActive) t->wait = W_NONE;
            break;
        case W_ANIM:
            if (!bx_anim_busy(b) || b->skipMode) t->wait = W_NONE;
            break;
        case W_CLICK:
            if (b->click || b->skipMode) {
                b->click = false;
                if (!skip_to_label(b)) t->wait = W_NONE;
            }
            break;
        default:
            break;
        }
    }
    if (b->phase == PH_NONE && b->th.wait == W_TEXT && b->wheel < 0) {
        b->click = true;
        b->wheel = 0;
        text_wait(b);
    }
    if (b->phase == PH_NONE) bx_vm_run(b);
    if (b->th.halted && !b->problem[0]) snprintf(b->problem, sizeof b->problem, "the script stopped");
    b->click = false;
    b->rclick = false;
    b->lPress = false;
    b->wheel = 0;
    memset(b->keyPress, 0, sizeof b->keyPress);
}

bool vnbsx_frame(VNBSX *n, uint32_t elapsedMs) {
    BX *b = (BX *)n;
    if (b->paused) return !b->quit;
    if (elapsedMs > 250) elapsedMs = 250;
    b->tickAcc += elapsedMs;
    int ticks = 0;
    while (b->tickAcc >= TICK_MS && ticks < 8 && !b->quit) {
        b->tickAcc -= TICK_MS;
        b->clockMs += TICK_MS;
        b->now = (u32)b->clockMs;
        tick(b);
        ticks++;
        if (b->th.halted) b->quit = true;
    }
    if (b->tickAcc > TICK_MS * 8) b->tickAcc = 0;
    bx_gfx_frame(b);
    return !b->quit;
}

const uint32_t *vnbsx_pixels(VNBSX *n, int *width, int *height) {
    BX *b = (BX *)n;
    *width = b->W;
    *height = b->H;
    return b->screen;
}

uint32_t vnbsx_frame_serial(VNBSX *n) { return ((BX *)n)->serial; }

void vnbsx_mouse_move(VNBSX *n, int x, int y) {
    BX *b = (BX *)n;
    b->mx = x;
    b->my = y;
}

void vnbsx_mouse_button(VNBSX *n, int button, bool down) {
    BX *b = (BX *)n;
    if (button == VNBSX_MOUSE_LEFT) {
        if (b->lDown && !down) b->click = true;
        if (!b->lDown && down) b->lPress = true;
        b->lDown = down;
    } else if (button == VNBSX_MOUSE_RIGHT) {
        if (b->rDown && !down) b->rclick = true;
        b->rDown = down;
    }
}

void vnbsx_key(VNBSX *n, int vk, bool down) {
    BX *b = (BX *)n;
    if (vk < 0 || vk > 255) return;
    if (down && !b->keyDown[vk]) b->keyPress[vk] = true;
    b->keyDown[vk] = down;
    if (down && (vk == 0x0d || vk == 0x20)) b->click = true;
    if (down && (vk == 0x1b || vk == 0x09)) b->rclick = true;
}

void vnbsx_wheel(VNBSX *n, int notches) { ((BX *)n)->wheel += notches; }

void vnbsx_audio(VNBSX *n, float *out, int frames) {
    BX *b = (BX *)n;
    bx_mixer_mix(b->mixer, out, frames, b->paused);
}

void vnbsx_set_paused(VNBSX *n, bool paused) { ((BX *)n)->paused = paused; }

void vnbsx_request_quit(VNBSX *n) { ((BX *)n)->quit = true; }

const char *vnbsx_title(VNBSX *n) { return ((BX *)n)->title; }

const char *vnbsx_problem(VNBSX *n) {
    BX *b = (BX *)n;
    return b->problem[0] ? b->problem : NULL;
}

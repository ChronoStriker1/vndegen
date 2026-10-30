// The public interface (vns3.h): opening a game, stepping it, input, and the screen.
#include "vm.h"
#include "vnfont.h"
#include <dirent.h>
#include <time.h>

static bool ends_with_nocase(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    if (a < b) return false;
    for (size_t i = 0; i < b; i++) {
        char x = s[a - b + i], y = suffix[i];
        if (x >= 'a' && x <= 'z') x -= 32;
        if (y >= 'a' && y <= 'z') y -= 32;
        if (x != y) return false;
    }
    return true;
}

// The ALD name prefix, from the scenario archive: RanceSA.ALD gives "Rance".
static bool find_prefix(S3 *s) {
    DIR *d = opendir(s->gameDir);
    if (!d) return false;
    struct dirent *e;
    bool found = false;
    while ((e = readdir(d))) {
        if (ends_with_nocase(e->d_name, "SA.ALD")) {
            size_t n = strlen(e->d_name) - 6;
            if (n >= sizeof s->prefix) continue;
            memcpy(s->prefix, e->d_name, n);
            s->prefix[n] = 0;
            found = true;
            break;
        }
    }
    closedir(d);
    return found;
}

static void defaults(S3 *s) {
    s->zh = 0;
    s->fontSize = 16;
    s->menuFontSize = 16;
    s->lineSpace = 2;
    s->fontWeight = 7;
    s->msgColor = s->menuColor = s->menuFrame = s->msgFrame = s->hitColor = 255;
    s->menuBg = s->msgBg = 0;
    s->menuSelColor = 255;
    s->palBank = 1;
    s->msgClearOnSelect = true;
    s->pcMode = 7;
    s->pdLevel = 255;
    s->flip = true;
    s->menuBuild = -1;
    s->curMsg = s->curSel = 1;
    for (int i = 0; i < 128; i++) {
        s->msgWin[i] = (S3Win){ 100, 300, 400, 90, 0 };
        s->selWin[i] = (S3Win){ 450, 100, 150, 100, 1 };
    }
    s->strLen = 512;
    s->strCount = S3_MAX_STRINGS - 2; // strings 0 to 4999: the save counts 5000
    s->y3Cancel = true;
    s->rand = 0x12345678u;
    s->tx = s->msgWin[1].x;
    s->ty = s->msgWin[1].y;
    // A plain grey ramp until a CG sets the palette, so early text is visible.
    for (int i = 0; i < 256; i++) for (int c = 0; c < 3; c++) s->pal[i][c] = s->shownPal[i][c] = (u8)i;
}

static void set_err(char *err, size_t size, const char *msg) {
    if (err && size) snprintf(err, size, "%s", msg);
}

VNS3 *vns3_open(const VNS3Config *config, char *err, size_t errSize) {
    S3 *s = s3_alloc(sizeof *s);
    s->cfg = *config;
    s->gameDir = s3_strdup(config->gameDir);
    s->saveDir = s3_strdup(config->saveDir ? config->saveDir : config->gameDir);
    s->cfg.gameDir = s->gameDir;
    s->cfg.saveDir = s->saveDir;
    if (config->logPath) s->log = fopen(config->logPath, "a");
    s->trace = getenv("S3_TRACE") != NULL;
    s->startTime = config->startTime ? config->startTime : (long long)time(NULL);
    s->vars = s3_alloc(65536 * sizeof(u16));
    s->bind = s3_alloc(16384 * sizeof(S3Bind));
    defaults(s);

    char *ain = s3_find_file(s->gameDir, "System39.ain");
    if (!ain) {
        // A SYSTEM3.EXE game: scenario in ADISK.DAT, CGs in ACG.DAT and the volumes after it.
        s->sys30 = true;
        if (!s3_open30(s)) { set_err(err, errSize, "This folder has neither System39.ain nor ADISK.DAT."); vns3_close((VNS3 *)s); return NULL; }
    } else {
        s->hasAin = s3_ain_open(&s->ain, ain);
        free(ain);
    }
    if (!s->sys30 && (!find_prefix(s) || !s3_archive_open(&s->sa, s->gameDir, s->prefix, 'S'))) {
        set_err(err, errSize, "The game's scenario archive (*SA.ALD) is missing or unreadable.");
        vns3_close((VNS3 *)s);
        return NULL;
    }
    if (!s->sys30) s3_archive_open(&s->ga, s->gameDir, s->prefix, 'G');
    if (!s->sys30) s3_archive_open(&s->wa, s->gameDir, s->prefix, 'W');
    s3_archive_open(&s->ma, s->gameDir, s->prefix, 'M');
    s3_archive_open(&s->da, s->gameDir, s->prefix, 'D');
    s3_log(s, "opened %s: prefix %s, %u pages, %u messages", s->gameDir, s->prefix, s->sa.count, s->ain.msgCount);

    const char *fontPath = config->fontPath;
    int fontIndex = config->fontIndex;
    vnfont_match match;
    if (!fontPath && vnfont_resolve("MS Gothic", 0, 0, &match)) { fontPath = match.path; fontIndex = match.face; }
    if (!s3_font_open(s, fontPath, fontIndex)) s3_log(s, "no font: text will not show");

    // System 3.0 games draw on a 640x400 screen and keep a second, hidden one below it (Y30, Y31).
    s3_gfx_init(s, 640, s->sys30 ? 960 : 480, 8);
    s->viewW = 640;
    s->viewH = s->sys30 ? 400 : 480;
    s3_audio_init(s);
    if (!s3_vm_start(s)) { set_err(err, errSize, s->problem); vns3_close((VNS3 *)s); return NULL; }
    return (VNS3 *)s;
}

void vns3_close(VNS3 *s3) {
    S3 *s = (S3 *)s3;
    if (!s) return;
    if (s->trace && s->dib.p8) {
        int i = s->dib.p8[(size_t)20 * s->dib.w + 20];
        s3_log(s, "wait %d menus %d pc %d:%x", s->wait, s->menuCount, s->page, s->pc);
        s3_log(s, "pixel 20,20: index %d pal %d,%d,%d shown %d,%d,%d", i, s->pal[i][0], s->pal[i][1], s->pal[i][2], s->shownPal[i][0], s->shownPal[i][1], s->shownPal[i][2]);
    }
    s3_audio_free(s);
    s3_font_close(s);
    s3_unitmap_free(s);
    s3_close30(s);
    s3_sprite_free(s);
    s3_archive_close(&s->sa);
    s3_archive_close(&s->ga);
    s3_archive_close(&s->wa);
    s3_archive_close(&s->ma);
    s3_archive_close(&s->da);
    s3_ain_close(&s->ain);
    for (int i = 0; i < 256; i++) free(s->arr[i].v);
    for (int i = 0; i < S3_MAX_STRINGS; i++) free(s->str[i]);
    free(s->vars);
    free(s->bind);
    free(s->dib.p8);
    free(s->dib.p32);
    free(s->screen);
    free(s->menuSave);
    if (s->log) fclose(s->log);
    free(s->gameDir);
    free(s->saveDir);
    free(s);
}

bool vns3_frame(VNS3 *s3, uint32_t elapsedMs) {
    S3 *s = (S3 *)s3;
    if (s->quit) return false;
    if (s->paused) return true;
    s->clockMs += elapsedMs > 200 ? 200 : elapsedMs;
    s3_sprite_update(s);
    // A frame may finish several short waits in a row, such as a fade followed by a timed pause.
    for (int round = 0; round < 8 && !s->quit; round++) {
        if (s->wait != S3_WAIT_NONE) {
            s3_service_wait(s);
            if (s->wait != S3_WAIT_NONE) break;
        }
        s3_vm_run(s, 200000);
        if (s->wait == S3_WAIT_NONE) break; // the budget or a busy loop ended the frame
    }
    s3_gfx_present(s);
    return !s->quit;
}

const uint32_t *vns3_pixels(VNS3 *s3, int *width, int *height) {
    S3 *s = (S3 *)s3;
    if (!s->screen) s3_gfx_present(s);
    *width = s->screenW;
    *height = s->screenH;
    return s->screen;
}

uint32_t vns3_frame_serial(VNS3 *s3) { return ((S3 *)s3)->serial; }

void vns3_mouse_move(VNS3 *s3, int x, int y) {
    S3 *s = (S3 *)s3;
    s->mouseX = x;
    s->mouseY = y;
}

void vns3_mouse_button(VNS3 *s3, int button, bool down) {
    S3 *s = (S3 *)s3;
    if (button == VNS3_MOUSE_LEFT) { s->mouseL = down; if (down) { s->pressedMask |= 16 | 0x10000; s->clickLatch |= 16; } }
    if (button == VNS3_MOUSE_RIGHT) { s->mouseR = down; if (down) { s->pressedMask |= 32 | 0x20000; s->clickLatch |= 32; } }
}

// The IK bit for a Windows virtual key, 0 for keys IK doesn't report.
static u32 key_bit(int vk) {
    switch (vk) {
    case 0x26: case 0x68: return 1;
    case 0x28: case 0x62: return 2;
    case 0x25: case 0x64: return 4;
    case 0x27: case 0x66: return 8;
    case 0x0d: return 16;
    case 0x20: return 32;
    case 0x1b: return 64;
    case 0x09: return 128;
    default: return 0;
    }
}

void vns3_key(VNS3 *s3, int vk, bool down) {
    S3 *s = (S3 *)s3;
    if (vk < 0 || vk > 255) return;
    s->keyDown[vk] = down;
    if (down) s->pressedMask |= key_bit(vk);
}

void vns3_wheel(VNS3 *s3, int notches) { ((S3 *)s3)->wheel += notches; }

void vns3_set_paused(VNS3 *s3, bool paused) {
    S3 *s = (S3 *)s3;
    s->paused = paused;
    s3_audio_pause(s, paused);
}

void vns3_request_quit(VNS3 *s3) { ((S3 *)s3)->quit = true; }

const char *vns3_title(VNS3 *s3) { return ((S3 *)s3)->title; }

const char *vns3_problem(VNS3 *s3) {
    S3 *s = (S3 *)s3;
    return s->problem[0] ? s->problem : NULL;
}

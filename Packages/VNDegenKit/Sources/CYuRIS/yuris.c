// The public interface (vnyuris.h).
#include "yr.h"
#include "vm.h"
#include <time.h>

static void free_all(YR *y) {
    for (int i = 0; i < y->taskCount; i++) yr_task_free(y->tasks[i]);
    free(y->tasks);
    yr_script_close(y);
    yr_fs_close(y);
    free(y->text);
    free(y->textFlag);
    for (int i = 0; i < 1000; i++) free(y->sysStrs[i]);
    free(y->screen);
    free(y->gameDir);
    free(y->saveDir);
    free(y->fontDir);
    free(y->fontTable);
    if (y->log) fclose(y->log);
    free(y);
}

VNY *vny_open(const VNYConfig *config, char *err, size_t errSize) {
    YR *y = yr_alloc(sizeof(YR));
    y->gameDir = yr_strdup(config->gameDir);
    y->saveDir = yr_strdup(config->saveDir ? config->saveDir : config->gameDir);
    if (config->fontDir) y->fontDir = yr_strdup(config->fontDir);
    if (config->fontTable) y->fontTable = yr_strdup(config->fontTable);
    y->audioRate = config->audioRate > 0 ? config->audioRate : 44100;
    y->startUnix = config->startTime ? config->startTime : (i64)time(NULL);
    if (config->logPath) y->log = fopen(config->logPath, "a");
    y->fps = 60;
    y->traceGosub = getenv("YR_TRACE_GOSUB") != NULL;
    bool ok = yr_fs_open(y) && yr_script_open(y);
    if (ok) {
        yr_sysvars_init(y, y->scriptVersion);
        yr_vars_init_strings(y);
        y->screen = yr_alloc(sizeof(uint32_t) * (size_t)y->width * (size_t)y->height);
        ok = yr_vm_start(y);
    }
    if (!ok) {
        if (err && errSize) snprintf(err, errSize, "%s", y->problem[0] ? y->problem : "not a YU-RIS game");
        free_all(y);
        return NULL;
    }
    return (VNY *)y;
}

void vny_close(VNY *v) { if (v) free_all((YR *)v); }

bool vny_frame(VNY *v, uint32_t elapsedMs) {
    YR *y = (YR *)v;
    if (y->quit) return false;
    if (y->paused) return true;
    y->clock += elapsedMs;
    y->frameNo++;
    yr_vm_step(y);
    return !y->quit;
}

const uint32_t *vny_pixels(VNY *v, int *width, int *height) {
    YR *y = (YR *)v;
    *width = y->width;
    *height = y->height;
    return y->screen;
}

uint32_t vny_frame_serial(VNY *v) { return ((YR *)v)->serial; }

void vny_mouse_move(VNY *v, int x, int y_) {
    YR *y = (YR *)v;
    y->mouseX = x;
    y->mouseY = y_;
}

static void press(YR *y, int code, bool down) {
    if (code <= 0 || code >= YR_CODE_COUNT) return;
    if (down && !y->down[code]) { y->pressedThisFrame[code] = true; y->pressCount[code]++; }
    y->down[code] = down;
}

void vny_mouse_button(VNY *v, int button, bool down) {
    YR *y = (YR *)v;
    int b = button == VNY_MOUSE_LEFT ? 0 : button == VNY_MOUSE_RIGHT ? 1 : 2;
    press(y, yr_code_mouse(b), down);
    if (down) y->lastMouse = yr_code_mouse(b);
}

void vny_key(VNY *v, int vk, bool down) {
    YR *y = (YR *)v;
    press(y, yr_code_for_vk(vk), down);
    if (down) y->lastKey = yr_code_for_vk(vk);
}

void vny_wheel(VNY *v, int notches) { ((YR *)v)->wheel += notches; }

void vny_audio(VNY *v, float *out, int frames) {
    (void)v;
    memset(out, 0, sizeof(float) * 2 * (size_t)frames);
}

void vny_set_paused(VNY *v, bool paused) { ((YR *)v)->paused = paused; }
void vny_request_quit(VNY *v) { ((YR *)v)->quit = true; }
const char *vny_title(VNY *v) { return ((YR *)v)->title; }
const char *vny_problem(VNY *v) { YR *y = (YR *)v; return y->problem[0] ? y->problem : NULL; }

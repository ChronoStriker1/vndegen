#ifndef VNEXHIBIT_H
#define VNEXHIBIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNExHibit VNExHibit;

typedef struct VNExHibitConfig {
    const char *gameDir;
    const char *saveDir;
    const char *logPath;
    const char *cacheDir;
    int audioRate;
    long long startTime;
} VNExHibitConfig;

VNExHibit *vnexhibit_open(const VNExHibitConfig *config, char *err, size_t errSize);
void vnexhibit_close(VNExHibit *ex);

bool vnexhibit_probe(const char *gameDir, char *title, size_t titleSize);

bool vnexhibit_frame(VNExHibit *ex, uint32_t elapsedMs);
const uint32_t *vnexhibit_pixels(VNExHibit *ex, int *width, int *height);
uint32_t vnexhibit_frame_serial(VNExHibit *ex);

enum { VNEXHIBIT_MOUSE_LEFT = 1, VNEXHIBIT_MOUSE_RIGHT = 2 };
void vnexhibit_mouse_move(VNExHibit *ex, int x, int y);
void vnexhibit_mouse_button(VNExHibit *ex, int button, bool down);
void vnexhibit_key(VNExHibit *ex, int virtualKey, bool down);
void vnexhibit_wheel(VNExHibit *ex, int notches);

void vnexhibit_audio(VNExHibit *ex, float *out, int frames);
void vnexhibit_set_paused(VNExHibit *ex, bool paused);
void vnexhibit_request_quit(VNExHibit *ex);

const char *vnexhibit_title(VNExHibit *ex);
const char *vnexhibit_problem(VNExHibit *ex);

#ifdef __cplusplus
}
#endif

#endif

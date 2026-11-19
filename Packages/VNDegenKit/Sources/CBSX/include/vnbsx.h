#ifndef VNBSX_H
#define VNBSX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNBSX VNBSX;

typedef struct VNBSXConfig {
    const char *gameDir;
    const char *saveDir;
    const char *logPath;
    int audioRate;
    long long startTime;
} VNBSXConfig;

VNBSX *vnbsx_open(const VNBSXConfig *config, char *err, size_t errSize);
void vnbsx_close(VNBSX *bx);

bool vnbsx_probe(const char *gameDir, char *title, size_t titleSize);

bool vnbsx_frame(VNBSX *bx, uint32_t elapsedMs);
const uint32_t *vnbsx_pixels(VNBSX *bx, int *width, int *height);
uint32_t vnbsx_frame_serial(VNBSX *bx);

enum { VNBSX_MOUSE_LEFT = 1, VNBSX_MOUSE_RIGHT = 2 };
void vnbsx_mouse_move(VNBSX *bx, int x, int y);
void vnbsx_mouse_button(VNBSX *bx, int button, bool down);
void vnbsx_key(VNBSX *bx, int virtualKey, bool down);
void vnbsx_wheel(VNBSX *bx, int notches);

void vnbsx_audio(VNBSX *bx, float *out, int frames);
void vnbsx_set_paused(VNBSX *bx, bool paused);
void vnbsx_request_quit(VNBSX *bx);

const char *vnbsx_title(VNBSX *bx);
const char *vnbsx_problem(VNBSX *bx);

#ifdef __cplusplus
}
#endif

#endif

#ifndef VNTMRHIRO_H
#define VNTMRHIRO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNTH VNTH;

typedef struct VNTHConfig {
    const char *gameDir;
    const char *saveDir;
    const char *logPath;
    int audioRate;
    long long startTime;
} VNTHConfig;

VNTH *vnth_open(const VNTHConfig *config, char *err, size_t errSize);
void vnth_close(VNTH *th);

bool vnth_probe(const char *gameDir, char *title, size_t titleSize);

bool vnth_frame(VNTH *th, uint32_t elapsedMs);
const uint32_t *vnth_pixels(VNTH *th, int *width, int *height);
uint32_t vnth_frame_serial(VNTH *th);

enum { VNTH_MOUSE_LEFT = 1, VNTH_MOUSE_RIGHT = 2 };
void vnth_mouse_move(VNTH *th, int x, int y);
void vnth_mouse_button(VNTH *th, int button, bool down);
void vnth_key(VNTH *th, int virtualKey, bool down);
void vnth_wheel(VNTH *th, int notches);

void vnth_audio(VNTH *th, float *out, int frames);
void vnth_set_paused(VNTH *th, bool paused);
void vnth_request_quit(VNTH *th);

const char *vnth_title(VNTH *th);
const char *vnth_problem(VNTH *th);

#ifdef __cplusplus
}
#endif

#endif

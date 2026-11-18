#ifndef VNEAGLS_H
#define VNEAGLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNEAGLS VNEAGLS;

typedef struct VNEAGLSConfig {
    const char *gameDir;
    const char *saveDir;
    const char *logPath;
    int audioRate;
    long long startTime;
} VNEAGLSConfig;

VNEAGLS *vneagls_open(const VNEAGLSConfig *config, char *err, size_t errSize);
void vneagls_close(VNEAGLS *eg);

bool vneagls_probe(const char *gameDir, char *title, size_t titleSize);

bool vneagls_frame(VNEAGLS *eg, uint32_t elapsedMs);
const uint32_t *vneagls_pixels(VNEAGLS *eg, int *width, int *height);
uint32_t vneagls_frame_serial(VNEAGLS *eg);

enum { VNEAGLS_MOUSE_LEFT = 1, VNEAGLS_MOUSE_RIGHT = 2 };
void vneagls_mouse_move(VNEAGLS *eg, int x, int y);
void vneagls_mouse_button(VNEAGLS *eg, int button, bool down);
void vneagls_key(VNEAGLS *eg, int virtualKey, bool down);
void vneagls_wheel(VNEAGLS *eg, int notches);

void vneagls_audio(VNEAGLS *eg, float *out, int frames);
void vneagls_set_paused(VNEAGLS *eg, bool paused);
void vneagls_request_quit(VNEAGLS *eg);

const char *vneagls_title(VNEAGLS *eg);
const char *vneagls_problem(VNEAGLS *eg);

#ifdef __cplusplus
}
#endif

#endif

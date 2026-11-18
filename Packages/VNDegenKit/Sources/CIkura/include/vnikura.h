#ifndef VNIKURA_H
#define VNIKURA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNIkura VNIkura;

typedef struct VNIkuraConfig {
    const char *gameDir;
    const char *saveDir;
    const char *logPath;
    int audioRate;
    long long startTime;
} VNIkuraConfig;

VNIkura *vnikura_open(const VNIkuraConfig *config, char *err, size_t errSize);
void vnikura_close(VNIkura *ip);

bool vnikura_probe(const char *gameDir, char *title, size_t titleSize);

bool vnikura_frame(VNIkura *ip, uint32_t elapsedMs);
const uint32_t *vnikura_pixels(VNIkura *ip, int *width, int *height);
uint32_t vnikura_frame_serial(VNIkura *ip);

enum { VNIKURA_MOUSE_LEFT = 1, VNIKURA_MOUSE_RIGHT = 2 };
void vnikura_mouse_move(VNIkura *ip, int x, int y);
void vnikura_mouse_button(VNIkura *ip, int button, bool down);
void vnikura_key(VNIkura *ip, int virtualKey, bool down);
void vnikura_wheel(VNIkura *ip, int notches);

void vnikura_audio(VNIkura *ip, float *out, int frames);
void vnikura_set_paused(VNIkura *ip, bool paused);
void vnikura_request_quit(VNIkura *ip);

const char *vnikura_title(VNIkura *ip);
const char *vnikura_problem(VNIkura *ip);

#ifdef __cplusplus
}
#endif

#endif

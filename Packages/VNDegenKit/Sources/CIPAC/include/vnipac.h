#ifndef VNIPAC_H
#define VNIPAC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNIPAC VNIPAC;

typedef struct VNIPACConfig {
    const char *gameDir;
    const char *saveDir;
    const char *logPath;
    int audioRate;
    long long startTime;
} VNIPACConfig;

VNIPAC *vnipac_open(const VNIPACConfig *config, char *err, size_t errSize);
void vnipac_close(VNIPAC *ip);

bool vnipac_probe(const char *gameDir, char *title, size_t titleSize);
bool vnipac_save_folder(const char *gameDir, char *brand, size_t brandSize, char *title, size_t titleSize);

bool vnipac_frame(VNIPAC *ip, uint32_t elapsedMs);
const uint32_t *vnipac_pixels(VNIPAC *ip, int *width, int *height);
uint32_t vnipac_frame_serial(VNIPAC *ip);

enum { VNIPAC_MOUSE_LEFT = 1, VNIPAC_MOUSE_RIGHT = 2 };
void vnipac_mouse_move(VNIPAC *ip, int x, int y);
void vnipac_mouse_button(VNIPAC *ip, int button, bool down);
void vnipac_key(VNIPAC *ip, int virtualKey, bool down);
void vnipac_wheel(VNIPAC *ip, int notches);

void vnipac_audio(VNIPAC *ip, float *out, int frames);
void vnipac_set_paused(VNIPAC *ip, bool paused);
void vnipac_request_quit(VNIPAC *ip);

const char *vnipac_title(VNIPAC *ip);
const char *vnipac_problem(VNIPAC *ip);

#ifdef __cplusplus
}
#endif

#endif

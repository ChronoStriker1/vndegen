#ifndef VNAGE_H
#define VNAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNAge VNAge;

typedef struct VNAgeConfig {
    const char *gameDir;
    const char *saveDir;
    const char *logPath;
    const char *cacheDir;
    int audioRate;
    long long startTime;
} VNAgeConfig;

VNAge *vnage_open(const VNAgeConfig *config, char *err, size_t errSize);
void vnage_close(VNAge *ag);

bool vnage_probe(const char *gameDir, char *title, size_t titleSize);

bool vnage_frame(VNAge *ag, uint32_t elapsedMs);
const uint32_t *vnage_pixels(VNAge *ag, int *width, int *height);
uint32_t vnage_frame_serial(VNAge *ag);

enum { VNAGE_MOUSE_LEFT = 1, VNAGE_MOUSE_RIGHT = 2 };
void vnage_mouse_move(VNAge *ag, int x, int y);
void vnage_mouse_button(VNAge *ag, int button, bool down);
void vnage_key(VNAge *ag, int virtualKey, bool down);
void vnage_wheel(VNAge *ag, int notches);

void vnage_audio(VNAge *ag, float *out, int frames);
void vnage_set_paused(VNAge *ag, bool paused);
void vnage_request_quit(VNAge *ag);

const char *vnage_title(VNAge *ag);
const char *vnage_problem(VNAge *ag);

#ifdef __cplusplus
}
#endif

#endif

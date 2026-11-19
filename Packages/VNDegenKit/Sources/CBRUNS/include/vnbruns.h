#ifndef VNBRUNS_H
#define VNBRUNS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNBruns VNBruns;

typedef struct VNBrunsConfig {
    const char *gameDir;
    const char *saveDir;
    const char *logPath;
    int audioRate;
    long long startTime;
} VNBrunsConfig;

VNBruns *vnbruns_open(const VNBrunsConfig *config, char *err, size_t errSize);
void vnbruns_close(VNBruns *br);

bool vnbruns_probe(const char *gameDir, char *title, size_t titleSize);

bool vnbruns_frame(VNBruns *br, uint32_t elapsedMs);
const uint32_t *vnbruns_pixels(VNBruns *br, int *width, int *height);
uint32_t vnbruns_frame_serial(VNBruns *br);

enum { VNBRUNS_MOUSE_LEFT = 1, VNBRUNS_MOUSE_RIGHT = 2 };
void vnbruns_mouse_move(VNBruns *br, int x, int y);
void vnbruns_mouse_button(VNBruns *br, int button, bool down);
void vnbruns_key(VNBruns *br, int virtualKey, bool down);
void vnbruns_wheel(VNBruns *br, int notches);

void vnbruns_audio(VNBruns *br, float *out, int frames);
void vnbruns_set_paused(VNBruns *br, bool paused);
void vnbruns_request_quit(VNBruns *br);

const char *vnbruns_title(VNBruns *br);
const char *vnbruns_problem(VNBruns *br);

#ifdef __cplusplus
}
#endif

#endif

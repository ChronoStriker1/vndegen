#ifndef VNSYSTEMAOI_H
#define VNSYSTEMAOI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNSystemAoi VNSystemAoi;

typedef struct VNSystemAoiConfig {
    const char *gameDir;
    const char *saveDir;
    const char *logPath;
    int audioRate;
    unsigned int seed;
} VNSystemAoiConfig;

VNSystemAoi *vnsystemaoi_open(const VNSystemAoiConfig *config, char *err, size_t errSize);
void vnsystemaoi_close(VNSystemAoi *sa);

bool vnsystemaoi_probe(const char *gameDir, char *title, size_t titleSize);

bool vnsystemaoi_frame(VNSystemAoi *sa, uint32_t elapsedMs);
const uint32_t *vnsystemaoi_pixels(VNSystemAoi *sa, int *width, int *height);
uint32_t vnsystemaoi_frame_serial(VNSystemAoi *sa);

enum { VNSYSTEMAOI_MOUSE_LEFT = 1, VNSYSTEMAOI_MOUSE_RIGHT = 2 };
void vnsystemaoi_mouse_move(VNSystemAoi *sa, int x, int y);
void vnsystemaoi_mouse_button(VNSystemAoi *sa, int button, bool down);
void vnsystemaoi_key(VNSystemAoi *sa, int virtualKey, bool down);
void vnsystemaoi_wheel(VNSystemAoi *sa, int notches);

void vnsystemaoi_audio(VNSystemAoi *sa, float *out, int frames);
void vnsystemaoi_set_paused(VNSystemAoi *sa, bool paused);
void vnsystemaoi_request_quit(VNSystemAoi *sa);

const char *vnsystemaoi_title(VNSystemAoi *sa);
const char *vnsystemaoi_problem(VNSystemAoi *sa);

#ifdef __cplusplus
}
#endif

#endif

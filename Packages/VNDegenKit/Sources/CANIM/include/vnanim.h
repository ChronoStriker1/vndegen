#ifndef VNANIM_H
#define VNANIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNANIM VNANIM;

typedef struct VNANIMConfig {
    const char *gameDir;
    const char *saveDir;
    const char *logPath;
    int audioRate;
    long long startTime;
} VNANIMConfig;

VNANIM *vnanim_open(const VNANIMConfig *config, char *err, size_t errSize);
void vnanim_close(VNANIM *an);

bool vnanim_probe(const char *gameDir, char *title, size_t titleSize);

bool vnanim_frame(VNANIM *an, uint32_t elapsedMs);
const uint32_t *vnanim_pixels(VNANIM *an, int *width, int *height);
uint32_t vnanim_frame_serial(VNANIM *an);

enum { VNANIM_MOUSE_LEFT = 1, VNANIM_MOUSE_RIGHT = 2 };
void vnanim_mouse_move(VNANIM *an, int x, int y);
void vnanim_mouse_button(VNANIM *an, int button, bool down);
void vnanim_key(VNANIM *an, int virtualKey, bool down);
void vnanim_wheel(VNANIM *an, int notches);

void vnanim_audio(VNANIM *an, float *out, int frames);
void vnanim_set_paused(VNANIM *an, bool paused);
void vnanim_request_quit(VNANIM *an);

const char *vnanim_title(VNANIM *an);
const char *vnanim_take_page(VNANIM *an);
const char *vnanim_problem(VNANIM *an);

int vnanim_take_comment_request(VNANIM *an, char *comment, size_t size);
bool vnanim_set_comment(VNANIM *an, int slot, const char *comment);

#ifdef __cplusplus
}
#endif

#endif

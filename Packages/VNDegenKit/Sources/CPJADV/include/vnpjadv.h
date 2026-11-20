#ifndef VNPJADV_H
#define VNPJADV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNPJADV VNPJADV;

enum { VNPJADV_LANG_ENGLISH = 1, VNPJADV_LANG_JAPANESE = 2, VNPJADV_LANG_CHINESE = 3 };

typedef struct VNPJADVConfig {
    const char *gameDir;
    const char *saveDir;
    const char *logPath;
    int audioRate;
    int language;
    unsigned int seed;
} VNPJADVConfig;

VNPJADV *vnpjadv_open(const VNPJADVConfig *config, char *err, size_t errSize);
void vnpjadv_close(VNPJADV *pj);

bool vnpjadv_probe(const char *gameDir, char *title, size_t titleSize);

bool vnpjadv_frame(VNPJADV *pj, uint32_t elapsedMs);
const uint32_t *vnpjadv_pixels(VNPJADV *pj, int *width, int *height);
uint32_t vnpjadv_frame_serial(VNPJADV *pj);

enum { VNPJADV_MOUSE_LEFT = 1, VNPJADV_MOUSE_RIGHT = 2 };
void vnpjadv_mouse_move(VNPJADV *pj, int x, int y);
void vnpjadv_mouse_button(VNPJADV *pj, int button, bool down);
void vnpjadv_key(VNPJADV *pj, int virtualKey, bool down);
void vnpjadv_wheel(VNPJADV *pj, int notches);

void vnpjadv_audio(VNPJADV *pj, float *out, int frames);
void vnpjadv_set_paused(VNPJADV *pj, bool paused);
void vnpjadv_request_quit(VNPJADV *pj);

const char *vnpjadv_title(VNPJADV *pj);
const char *vnpjadv_problem(VNPJADV *pj);

#ifdef __cplusplus
}
#endif

#endif

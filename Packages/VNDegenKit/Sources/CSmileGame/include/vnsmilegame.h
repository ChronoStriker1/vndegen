#ifndef VNSMILEGAME_H
#define VNSMILEGAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNSmileGame VNSmileGame;

typedef struct VNSmileGameConfig {
    const char *gameDir;
    const char *saveDir;
    const char *logPath;
    int audioRate;
    const char *fallbackFont;
} VNSmileGameConfig;

VNSmileGame *vnsmilegame_open(const VNSmileGameConfig *config, char *err, size_t errSize);
void vnsmilegame_close(VNSmileGame *g);

bool vnsmilegame_probe(const char *gameDir, char *title, size_t titleSize);

bool vnsmilegame_frame(VNSmileGame *g, uint32_t elapsedMs);
const uint32_t *vnsmilegame_pixels(VNSmileGame *g, int *width, int *height);
uint32_t vnsmilegame_frame_serial(VNSmileGame *g);

void vnsmilegame_key(VNSmileGame *g, int virtualKey, bool down);

enum { VNSMILEGAME_POINTER_DOWN, VNSMILEGAME_POINTER_UP, VNSMILEGAME_POINTER_CANCEL };
void vnsmilegame_pointer(VNSmileGame *g, int phase);
void vnsmilegame_audio(VNSmileGame *g, float *out, int frames);
bool vnsmilegame_save(VNSmileGame *g, int slot);
bool vnsmilegame_load(VNSmileGame *g, int slot);

const char *vnsmilegame_title(VNSmileGame *g);
const char *vnsmilegame_problem(VNSmileGame *g);
const char *vnsmilegame_scene(VNSmileGame *g);

#ifdef __cplusplus
}
#endif

#endif

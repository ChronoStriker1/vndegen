#ifndef VNEMOTE_H
#define VNEMOTE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vnemote vnemote;

typedef struct vnemote_vertex {
    float x, y, u, v, r, g, b, a;
} vnemote_vertex;

enum { VNEMOTE_PASS_NORMAL = 0, VNEMOTE_PASS_MASK = 1, VNEMOTE_PASS_COMPOSITE_MASK = 2, VNEMOTE_PASS_FILTERED = 3 };

typedef struct vnemote_draw {
    uint32_t texture;
    uint32_t blend;
    uint32_t pass;
    uint32_t first, count;
    float clip[4];
    uint32_t stencil_first, stencil_count;
    uint32_t stencil_initial, stencil_final;
} vnemote_draw;

enum { VNEMOTE_STENCIL_INNER = 1, VNEMOTE_STENCIL_OUTER = 2, VNEMOTE_STENCIL_NEXT = 9 };

typedef struct vnemote_stencil {
    uint32_t phase;
    uint32_t texture;
    uint32_t first, count;
} vnemote_stencil;

typedef struct vnemote_api_v1 {
    uint32_t version;
    vnemote *(*open)(const uint8_t *data, size_t size, uint32_t key, char *error, size_t errorSize);
    void (*close)(vnemote *model);
    void (*set_variable)(vnemote *model, const char *name, float value, float ticks, float easing);
    int (*play_timeline)(vnemote *model, const char *name, uint32_t flags, int loop);
    void (*stop_timeline)(vnemote *model, const char *name);
    int (*timeline_playing)(vnemote *model, const char *name);
    void (*progress)(vnemote *model, float ticks);
    int (*build)(vnemote *model);
    const vnemote_draw *(*draws)(vnemote *model, int *count);
    const vnemote_vertex *(*vertices)(vnemote *model, int *count);
    const uint8_t *(*texture)(vnemote *model, uint32_t index, int *width, int *height);
    int (*bounds)(vnemote *model, float out[4]);
    int (*variable_count)(vnemote *model);
    const char *(*variable_name)(vnemote *model, int index);
    const vnemote_stencil *(*stencils)(vnemote *model, int *count);
    void (*set_detail)(vnemote *model, float pixelsPerUnit);
    void (*reset)(vnemote *model);
} vnemote_api_v1;

const vnemote_api_v1 *vnemote_get_api_v1(void);

#ifdef __cplusplus
}
#endif

#endif

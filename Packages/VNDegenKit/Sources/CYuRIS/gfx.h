// CG layers, from the YU-RIS manual's layer page: layers have IDs, layer sets hold layers like folders, ":" is the
// root and ":/W0" the main window's set, which is also where relative IDs start. Higher Z draws in front.
#ifndef YR_GFX_H
#define YR_GFX_H

#include "vm.h"

typedef struct YLayer YLayer;
struct YLayer {
    char *name;
    YLayer *parent;
    YLayer **kids;
    int nkids, kidCap;
    bool isSet;
    u64 order;            // creation order, for layers of equal Z
    // Attributes
    bool e;
    int a;                // opacity, 0 to 256
    int x, y, z;
    int mode;             // blend mode, 0 normal
    int sx, sy;           // a set's clip size; 0 leaves the set unclipped
    int t, ta;            // transition progress and softness, with tid as the rule image
    char *tid, *mid;
    i64 lint, lint2;
    // The camera transform ERIS drives through the F* keywords: scale in 1/10000, center in 1/10000 of the size,
    // rotation in 1/100 degree, and an offset in pixels.
    bool f;
    double fx, fy, fbx, fby, fcx, fcy, frx, fry, frz;
    // Pixels of an image layer: straight-alpha RGBA, R in the low byte.
    int w, h;
    uint32_t *px;
    bool opaque;          // every pixel has alpha 255, so drawing it can copy rows
    char *file;           // the image it was loaded from, for CGINFO
};

struct YGfx {
    YLayer *root;
    YLayer *cur;          // where relative IDs start (:/W0)
    u64 order;
    bool dirty;
};

void yr_gfx_open(YR *y);
void yr_gfx_close(YR *y);
// Finds a layer by ID. With create, missing layers and sets on the way are made; the last part is a set when the ID
// ends in '/' or set is true.
YLayer *yr_layer(YR *y, const char *id, bool create, bool set);
void yr_layer_free(YLayer *l);
void yr_layer_remove(YR *y, YLayer *l);
void yr_layer_pixels(YLayer *l, int w, int h);    // replaces the image with a transparent w x h one
void yr_layer_check_opaque(YLayer *l);
bool yr_layer_load(YR *y, YLayer *l, const char *file);
// The layer's position in screen coordinates (its own x, y plus its sets').
void yr_layer_origin(const YLayer *l, int *x, int *y);
// Draws a set and everything in it into a w x h straight-alpha image, as it would show.
uint32_t *yr_render_set(YR *y, YLayer *set, int w, int h);
// Composites :/W0 into y->screen when anything changed.
void yr_gfx_present(YR *y);
void yr_gfx_dump(YR *y);   // logs the layer tree

static inline u32 yr_mul8(u32 a, u32 b) { u32 t = a * b + 128; return (t + (t >> 8)) >> 8; }

#endif

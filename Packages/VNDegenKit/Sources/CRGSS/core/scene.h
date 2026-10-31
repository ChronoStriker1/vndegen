// The objects RGSS draws: sprites, planes, windows, tilemaps and viewports, and the compositor that draws them in z
// order onto the screen. The Ruby classes in binding_*.c own these structs.
#ifndef RGSS_SCENE_H
#define RGSS_SCENE_H

#include "rgss.h"

typedef struct Bitmap {
    Image image;
    bool disposed;
    // Bumped on every change, so tilemaps and windows know when to redraw what they cache.
    uint64_t generation;
} Bitmap;

typedef struct Table {
    int dims, xs, ys, zs;
    int16_t *data;
} Table;

typedef enum { DRAW_SPRITE, DRAW_PLANE, DRAW_WINDOW, DRAW_TILEMAP, DRAW_VIEWPORT } DrawKind;

typedef struct Container Container;

// What every drawn object has: its place in a container's list, and the order it was created in, which breaks z ties.
typedef struct Drawable {
    DrawKind kind;
    Container *container;
    struct Drawable *prev, *next;
    uint64_t serial;
} Drawable;

struct Container {
    Drawable *first, *last;
};

typedef struct {
    ColorF color;
    int duration, count; // count runs down from duration
    bool hide;           // a flash with a nil color hides the object
} Flash;

typedef struct Viewport {
    Drawable d;
    Container children;
    IRect *rect;
    bool visible, disposed;
    int z, ox, oy;
    ColorF *color;
    ToneF *tone;
    Flash flash;
} Viewport;

typedef struct Sprite {
    Drawable d;
    Viewport *viewport;
    Bitmap *bitmap;
    IRect *srcRect;
    bool visible, mirror, disposed;
    int x, y, z, ox, oy;
    double zoomX, zoomY, angle;
    int bushDepth, bushOpacity, opacity, blendType;
    ColorF *color;
    ToneF *tone;
    int waveAmp, waveLength, waveSpeed;
    double wavePhase;
    Flash flash;
} Sprite;

typedef struct Plane {
    Drawable d;
    Viewport *viewport;
    Bitmap *bitmap;
    bool visible, disposed;
    int z, ox, oy;
    double zoomX, zoomY;
    int opacity, blendType;
    ColorF *color;
    ToneF *tone;
} Plane;

typedef struct Window {
    Drawable d;
    Viewport *viewport;
    Bitmap *windowskin, *contents;
    IRect *cursorRect;
    bool stretch, active, visible, pause, disposed, arrowsVisible;
    int x, y, z, width, height, ox, oy, padding, paddingBottom;
    int opacity, backOpacity, contentsOpacity, openness;
    ToneF *tone;
    int cursorCount, pauseCount;
} Window;

typedef struct Tilemap {
    Drawable d;
    Viewport *viewport;
    Bitmap *bitmaps[9]; // XP: [0..6] autotiles, tileset in `tileset`
    Bitmap *tileset;
    Table *mapData, *flashData, *flags; // flags: XP priorities, VX passages, Ace flags
    bool visible, disposed;
    int ox, oy;
    int animCount;
} Tilemap;

// Every object starts in the screen's container or its viewport's.
extern Container rgss_screen;
void drawable_attach(Drawable *d, DrawKind kind, Viewport *viewport);
void drawable_detach(Drawable *d);

void flash_start(Flash *flash, const ColorF *color, int duration);
void flash_update(Flash *flash);

// The screen size and the frame the compositor draws.
extern int rgss_screen_w, rgss_screen_h;
void scene_render(Image *target);

// Drawing helpers shared by the object types.
typedef struct {
    const ColorF *color;
    const ToneF *tone;
    const Flash *flash;
    int opacity, blendType;
} Effects;

void draw_image_rect(Image *target, IRect clip, const Image *src, IRect srcRect, int dx, int dy, const Effects *fx);
void window_draw(Window *window, Image *target, IRect clip, int offsetX, int offsetY);
void window_update(Window *window);
// XP's tilemap draws in several layers at different z, so it lists them for the compositor.
int tilemap_layers(Tilemap *tilemap, int *zs, int max);
void tilemap_draw(Tilemap *tilemap, int layer, Image *target, IRect clip, int offsetX, int offsetY);
void tilemap_update(Tilemap *tilemap);

#endif

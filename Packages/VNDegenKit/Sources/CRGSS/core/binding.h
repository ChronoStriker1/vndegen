// The Ruby side of the runtime: the RGSS classes and modules, defined fresh for each game.
#ifndef RGSS_BINDING_H
#define RGSS_BINDING_H

#include <ruby.h>

#include "scene.h"

// The classes the game sees. They are redefined for each game, so these change between games.
extern VALUE rgss_cColor, rgss_cTone, rgss_cRect, rgss_cTable, rgss_cFont, rgss_cBitmap, rgss_cViewport, rgss_cSprite,
    rgss_cPlane, rgss_cWindow, rgss_cTilemap, rgss_eRGSSError, rgss_eReset;

typedef struct {
    ColorF c;
} RbColor;

typedef struct {
    ToneF t;
} RbTone;

typedef struct {
    IRect r;
} RbRect;

typedef struct {
    VALUE name; // a String or an Array of them
    int size;
    bool bold, italic, shadow, outline;
    VALUE color, outColor;
} RbFont;

typedef struct {
    Bitmap b;
    VALUE font;
} RbBitmap;

void rgss_define_values(void);   // Color, Tone, Rect, Table
void rgss_define_font(void);
void rgss_define_bitmap(void);
void rgss_define_drawables(void); // Viewport, Sprite, Plane, Window, Tilemap
void rgss_define_modules(void);   // Graphics, Input, Audio, the global functions

// Accessors that raise TypeError for the wrong class.
RbColor *rgss_color(VALUE v);
RbTone *rgss_tone(VALUE v);
RbRect *rgss_rect(VALUE v);
Table *rgss_table(VALUE v);
RbFont *rgss_font(VALUE v);
// A Bitmap's struct, raising RGSSError when it is disposed.
RbBitmap *rgss_bitmap(VALUE v);
// The same, or NULL for nil.
Bitmap *rgss_bitmap_or_null(VALUE v);

VALUE rgss_color_new(double r, double g, double b, double a);
VALUE rgss_tone_new(double r, double g, double b, double gray);
VALUE rgss_rect_new(int x, int y, int w, int h);
VALUE rgss_bitmap_wrap_image(Image image);
VALUE rgss_font_new_default(void);
VALUE rgss_font_copy(VALUE font);

// The game's file reading for Ruby: load_data and Bitmap.new.
VALUE rgss_read_file(VALUE path, const char *const *extensions);

void rgss_raise_disposed(const char *what) __attribute__((noreturn));

// Graphics state the drawables read.
extern int rgss_frame_rate;
extern long rgss_frame_count;

// The tag Graphics.update throws when the host stops the game; game.c catches it around the scripts.
VALUE rgss_stop_tag(void);
// Clears Graphics and Input state between games.
void rgss_modules_reset(void);

#endif

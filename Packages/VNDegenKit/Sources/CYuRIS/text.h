// Fonts and text drawing (FONT, FONTINFO, CGACT[TEXT=1]).
#ifndef YR_TEXT_H
#define YR_TEXT_H

#include "gfx.h"

enum { YR_FONT_SLOTS = 32 };

typedef struct YFontSlot {
    char *name;           // Windows font name, in the game's encoding
    int sx, sy;
    i64 color, color2;
    bool bold, aa;
    int drawmode;
} YFontSlot;

struct YText {
    YFontSlot slots[YR_FONT_SLOTS];
    bool outputManual;    // TEXT[OUTPUTMODE=0]
    int wait;
};

void yr_text_open(YR *y);
void yr_text_close(YR *y);
YFontSlot *yr_font_slot(YR *y, int bno);
// Draws s with the slot's font at x, y (the top left of the first character cell) into l's pixels.
void yr_text_draw(YR *y, YLayer *l, int bno, const char *s, int x, int y_);
// Width of s in pixels with the slot's font.
int yr_text_width(YR *y, int bno, const char *s);

#endif

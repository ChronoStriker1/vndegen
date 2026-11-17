#include "th.h"

void ui_open(TH *t, int mode) {
    t->uiMode = mode;
    t->dirty = true;
}

void ui_close(TH *t) {
    t->uiMode = UI_NONE;
    t->dirty = true;
}

void ui_update(TH *t) {
    if (t->uiMode != UI_NONE && t->rclick) ui_close(t);
}

void ui_draw(TH *t) { (void)t; }

bool ui_bar_update(TH *t) {
    (void)t;
    return false;
}

void ui_bar_draw(TH *t) { (void)t; }

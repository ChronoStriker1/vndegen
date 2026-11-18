#include "eagls.h"

void ui_init(EG *e) { (void)e; }
void ui_free(EG *e) { (void)e; }
void ui_open(EG *e, int mode) {
    eg_log(e, "screen %d not supported yet", mode);
    e->uiMode = UI_NONE;
}
void ui_update(EG *e) { (void)e; }
void ui_draw(EG *e, u32 *dst) {
    (void)e;
    (void)dst;
}
void win_update(EG *e) { (void)e; }
void win_draw(EG *e, u32 *dst) {
    (void)e;
    (void)dst;
}

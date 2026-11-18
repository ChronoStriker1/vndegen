#include "ipac.h"

struct UI {
    Image *sheet, *map;
    int x, y;
    int hover;
};

static void ui_close(IP *t) {
    if (t->ui) {
        image_free(t->ui->sheet);
        image_free(t->ui->map);
        free(t->ui);
        t->ui = NULL;
    }
    t->uiMode = UI_NONE;
    t->dirty = true;
}

void ui_init(IP *t) { (void)t; }

void ui_free(IP *t) { ui_close(t); }

void ui_open(IP *t, int mode) {
    ui_close(t);
    if (mode != UI_EXIT) {
        ip_log(t, "screen %d not built yet", mode);
        return;
    }
    UI *u = ip_alloc(sizeof *u);
    u->sheet = image_load(t, PK_PARTS, "SysEnd");
    u->map = image_load(t, PK_PARTS, "SysEndA");
    if (!u->sheet) {
        free(u);
        t->quit = true;
        return;
    }
    u->x = (t->W - u->sheet->w) / 2;
    u->y = (t->H - 164) / 2;
    u->hover = 0;
    t->ui = u;
    t->uiMode = mode;
    t->dirty = true;
}

static u32 map_at(const Image *m, int x, int y) {
    if (!m || x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    u32 p = m->px[(size_t)y * m->w + x];
    return (p & 0xff) << 16 | (p & 0xff00) | (p >> 16 & 0xff);
}

void ui_update(IP *t) {
    UI *u = t->ui;
    if (!u) return;
    if (t->uiMode == UI_EXIT) {
        u32 c = map_at(u->map, t->mx - u->x, t->my - u->y);
        int hover = (c & 0xff00ff) == 0 && (c >> 8 & 0xff) >= 0xfa ? (int)(c >> 8 & 0xff) : 0;
        if (hover != u->hover) {
            u->hover = hover;
            t->dirty = true;
        }
        if (t->click) {
            t->click = false;
            if (hover == 0xfa) {
                t->quit = true;
                ui_close(t);
            } else if (hover == 0xfb) ui_close(t);
        }
        if (t->rclick) {
            t->rclick = false;
            ui_close(t);
        }
    }
}

void ui_draw(IP *t) {
    UI *u = t->ui;
    if (!u) return;
    if (t->uiMode == UI_EXIT) {
        blit_part(t->screen, t->W, t->H, u->sheet, 0, 0, u->sheet->w, 164, u->x, u->y, 255);
        if (u->hover == 0xfa) blit_part(t->screen, t->W, t->H, u->sheet, 0, 171, 112, 28, u->x + 21, u->y + 106, 255);
        if (u->hover == 0xfb) blit_part(t->screen, t->W, t->H, u->sheet, 119, 171, 112, 28, u->x + 140, u->y + 106, 255);
    }
}

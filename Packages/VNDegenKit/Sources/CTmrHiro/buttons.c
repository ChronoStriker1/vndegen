#include "th.h"

static bool inside(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && y >= ry && x < rx + rw && y < ry + rh; }

static void choose_label(TH *t, const char *label) {
    t->wait = W_NONE;
    t->buttonCount = 0;
    t->choiceCount = 0;
    t->mapCount = 0;
    t->sceneDirty = true;
    t->dirty = true;
    vm_goto(t, NULL, label);
}

static void choice_rect(TH *t, int i, int *x, int *y, int *w, int *h) {
    *w = ini_int(&t->sys, "SELECT", "SELECT_WIDTH", 800);
    *h = ini_int(&t->sys, "SELECT", "SELECT_HEIGHT", 60);
    int top = ini_int(&t->sys, "SELECT", "SELECT_POSY", 200);
    int ny = ini_int(&t->sys, "SELECT", "SELECT_NEXT_Y", 8), nx = ini_int(&t->sys, "SELECT", "SELECT_NEXT_X", 0);
    *x = (t->W - *w) / 2 + nx * i;
    *y = top + (*h + ny) * i;
}

void buttons_update(TH *t) {
    int old = t->hover;
    if (t->wait == W_BUTTONS) {
        t->hover = -1;
        for (int i = 0; i < t->buttonCount; i++)
            if (inside(t->mx, t->my, t->buttons[i].x, t->buttons[i].y, t->buttons[i].w, t->buttons[i].h)) t->hover = i;
        if (t->click && t->hover >= 0) {
            char label[64];
            snprintf(label, sizeof label, "%s", t->buttons[t->hover].label);
            choose_label(t, label);
        }
    } else if (t->wait == W_CHOICE) {
        t->hover = -1;
        for (int i = 0; i < t->choiceCount; i++) {
            int x, y, w, h;
            choice_rect(t, i, &x, &y, &w, &h);
            if (inside(t->mx, t->my, x, y, w, h)) t->hover = i;
        }
        if (t->click && t->hover >= 0) {
            char label[64];
            snprintf(label, sizeof label, "%s", t->choices[t->hover].label);
            for (int i = 0; i < t->choiceCount; i++) free(t->choices[i].text);
            choose_label(t, label);
        }
    } else if (t->wait == W_MAP) {
        t->hover = -1;
        for (int i = 0; i < t->mapCount; i++)
            if (t->maps[i].kind == 0x20 && inside(t->mx, t->my, t->maps[i].x, t->maps[i].y, t->maps[i].w, t->maps[i].h)) t->hover = i;
        if (t->click && t->hover >= 0) {
            MapButton *m = &t->maps[t->hover];
            if (m->voice[0]) snd_play(t, CH_SYSVOICE, PAC_VOICE, m->voice, false, 0);
            char label[64];
            snprintf(label, sizeof label, "%s", m->label);
            if (label[0]) choose_label(t, label);
        }
    }
    if (old != t->hover) t->dirty = true;
}

void buttons_draw(TH *t) {
    if (t->wait == W_BUTTONS) {
        Image *n = t->buttonImg[0] ? image_get(t, t->buttonImg) : NULL;
        Image *h = t->buttonImgOn[0] ? image_get(t, t->buttonImgOn) : NULL;
        if (n) blit(t->screen, t->W, t->H, n, 0, 0, 255);
        if (h && t->hover >= 0) {
            Button *b = &t->buttons[t->hover];
            blit_part(t->screen, t->W, t->H, h, b->x, b->y, b->w, b->h, b->x, b->y, 255);
        }
    } else if (t->wait == W_CHOICE) {
        Image *sel = image_get(t, "sys_select");
        int size = ini_int(&t->sys, "SELECT", "SELECT_TEXT_FONT_SIZE", 24);
        int px = ini_int(&t->sys, "SELECT", "SELECT_TEXT_PLUS_X", 0), py = ini_int(&t->sys, "SELECT", "SELECT_TEXT_PLUS_Y", 0);
        for (int i = 0; i < t->choiceCount; i++) {
            int x, y, w, h;
            choice_rect(t, i, &x, &y, &w, &h);
            if (sel) blit_part(t->screen, t->W, t->H, sel, 0, i == t->hover ? h : 0, w, h, x, y, 255);
            int tw = text_width(t, t->choices[i].text, size);
            text_draw(t, t->screen, t->W, t->H, t->choices[i].text, x + (w - tw) / 2 + px, y + (h - size) / 2 + py, size,
                      0xffffffffu, true, -1);
        }
    } else if (t->wait == W_MAP) {
        Image *base = t->mapBase[0] ? image_get(t, t->mapBase) : NULL;
        Image *sheet = t->mapSheet[0] ? image_get(t, t->mapSheet) : NULL;
        if (base) blit(t->screen, t->W, t->H, base, 0, 0, 255);
        for (int i = 0; i < t->mapCount; i++) {
            MapButton *m = &t->maps[i];
            if (!sheet || !m->w || !m->h) continue;
            bool hot = i == t->hover;
            blit_part(t->screen, t->W, t->H, sheet, hot ? m->hx : m->sx, hot ? m->hy : m->sy, m->w, m->h, m->x, m->y, 255);
        }
    }
}

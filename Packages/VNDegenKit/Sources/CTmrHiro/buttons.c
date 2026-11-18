#include "th.h"

#include <math.h>

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
    int total = t->choiceCount * *h + (t->choiceCount - 1) * ny;
    *x = (t->W - *w) / 2 + nx * i;
    *y = top - *h / 2 - total / 2 + (*h + ny) * i;
}

static int key_step(TH *t) {
    if (t->keyPress[0x25] || t->keyPress[0x26]) return -1;
    if (t->keyPress[0x27] || t->keyPress[0x28]) return 1;
    return 0;
}

static int nav(TH *t, int hover, int count, bool (*usable)(TH *, int)) {
    int step = key_step(t);
    if (!step || count <= 0) return hover;
    int h = hover;
    for (int k = 0; k < count; k++) {
        h = h < 0 ? (step > 0 ? 0 : count - 1) : (h + step + count) % count;
        if (!usable || usable(t, h)) return h;
    }
    return hover;
}

bool map_place(TH *t, const MapButton *m, int *x, int *y) {
    if (m->form == MAP_SLOT) return false;
    if (m->form != MAP_ITEM) {
        if (t->mapMode == 1 && m->index < 100 && !t->vars[3][m->index]) return false;
        *x = m->x;
        *y = m->y;
        return true;
    }
    int slot = t->vars[3][m->index];
    if (!slot) return false;
    for (int i = 0; i < t->mapCount; i++)
        if (t->maps[i].form == MAP_SLOT && t->maps[i].index == slot) {
            *x = t->maps[i].x;
            *y = t->maps[i].y;
            return true;
        }
    return false;
}

static bool map_usable(TH *t, int i) {
    int x, y;
    return t->maps[i].kind == 0x20 && t->maps[i].form != MAP_SLOT && map_place(t, &t->maps[i], &x, &y);
}

static bool map_hit(TH *t, int i) {
    int x, y;
    return map_usable(t, i) && map_place(t, &t->maps[i], &x, &y) && inside(t->mx, t->my, x, y, t->maps[i].w, t->maps[i].h);
}

static void map_sound(TH *t, const char *name) {
    if (!name[0]) return;
    if (!strncmp(name, "se_", 3) || !strncmp(name, "SE_", 3)) snd_play(t, CH_SE, PAC_SE, name, false, 0);
    else snd_play(t, CH_SYSVOICE, PAC_VOICE, name, false, 0);
}

void buttons_update(TH *t) {
    int old = t->hover;
    bool enter = t->keyPress[0x0d] || t->keyPress[0x20];
    if (t->wait == W_BUTTONS) {
        t->hover = -1;
        for (int i = 0; i < t->buttonCount; i++)
            if (inside(t->mx, t->my, t->buttons[i].x, t->buttons[i].y, t->buttons[i].w, t->buttons[i].h)) t->hover = i;
        if (t->hover < 0 && old >= 0 && !t->mouseMoved) t->hover = old;
        t->hover = nav(t, t->hover, t->buttonCount, NULL);
        if ((t->click || enter) && t->hover >= 0) {
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
        if (t->hover < 0 && old >= 0 && !t->mouseMoved) t->hover = old;
        t->hover = nav(t, t->hover, t->choiceCount, NULL);
        if ((t->click || enter) && t->hover >= 0) {
            char label[64];
            snprintf(label, sizeof label, "%s", t->choices[t->hover].label);
            for (int i = 0; i < t->choiceCount; i++) free(t->choices[i].text);
            choose_label(t, label);
        }
    } else if (t->wait == W_MAP) {
        if (t->rclick && t->mapCancel[0]) {
            char label[64];
            snprintf(label, sizeof label, "%s", t->mapCancel);
            choose_label(t, label);
            return;
        }
        t->hover = -1;
        for (int i = 0; i < t->mapCount; i++)
            if (map_hit(t, i)) t->hover = i;
        if (t->hover < 0 && old >= 0 && !t->mouseMoved) t->hover = old;
        t->hover = nav(t, t->hover, t->mapCount, map_usable);
        if (t->hover != old && t->hover >= 0) map_sound(t, t->maps[t->hover].form == MAP_ITEM ? t->maps[t->hover].hoverSe : t->maps[t->hover].voice);
        if ((t->click || enter) && t->hover >= 0) {
            MapButton *m = &t->maps[t->hover];
            char label[64];
            snprintf(label, sizeof label, "%s", m->label);
            if (label[0]) {
                map_sound(t, m->clickSe);
                gfx_capture(t, 1);
                Image *cap = image_get(t, "capdata001");
                if (cap) map_draw(t, cap->px, false);
                choose_label(t, label);
            }
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
        map_draw(t, t->screen, true);
    }
    if (t->wait == W_ROULETTE) roulette_draw(t);
}

void map_draw(TH *t, u32 *dst, bool card) {
    {
        Image *bg = t->mapBg[0] ? image_get(t, t->mapBg) : NULL;
        Image *base = t->mapBase[0] ? image_get(t, t->mapBase) : NULL;
        Image *sheet = t->mapSheet[0] ? image_get(t, t->mapSheet) : NULL;
        if (bg) blit(dst, t->W, t->H, bg, 0, 0, 255);
        if (base) blit(dst, t->W, t->H, base, 0, 0, 255);
        for (int i = 0; i < t->mapCount; i++) {
            MapButton *m = &t->maps[i];
            int x, y;
            if (!sheet || !m->w || !m->h || m->form == MAP_SLOT || !map_place(t, m, &x, &y)) continue;
            if (m->form == MAP_GAUGE) {
                if (!t->gaugeOn) continue;
                blit_part(dst, t->W, t->H, sheet, m->sx, m->sy, m->w, m->h, x, y, 255);
                const char *v = m->var[0] == '$' ? m->var + 1 : m->var;
                int value = 0;
                if (!strncmp(v, "glo", 3)) value = clampi(t->vars[2][clampi(atoi(v + 3), 0, TH_VARS - 1)], 0, 100);
                blit_part(dst, t->W, t->H, sheet, m->bsx, m->bsy, m->bw * value / 100, m->bh, m->bx, m->by, 255);
                continue;
            }
            bool hot = i == t->hover;
            blit_part(dst, t->W, t->H, sheet, hot ? m->hx : m->sx, hot ? m->hy : m->sy, m->w, m->h, x, y, 255);
        }
        if (card && t->hover >= 0 && t->maps[t->hover].form == MAP_ITEM && t->maps[t->hover].pic[0]) {
            Image *pic = image_get(t, t->maps[t->hover].pic);
            if (pic) blit(dst, t->W, t->H, pic, t->maps[t->hover].picX, 0, 255);
        }
    }
}

static bool mini_clickable(TH *t) {
    return t->wait == W_TEXT && t->uiMode == UI_NONE && !t->winHidden && !t->menuBlocked && !t->autoMode && !t->skipMode;
}

bool mini_update(TH *t) {
    int hover = -1;
    if (mini_clickable(t))
        for (int i = 0; i < 2; i++) {
            MiniButton *m = &t->mini[i];
            if (m->defined && m->shown && inside(t->mx, t->my, m->x, m->y, m->w, m->h)) hover = i;
        }
    if (hover != t->miniHover) {
        t->miniHover = hover;
        t->sceneDirty = true;
    }
    if (hover < 0 || !t->click) return false;
    t->miniHover = -1;
    t->sceneDirty = true;
    t->wait = W_NONE;
    t->pc = t->savePc;
    snd_stop(t, CH_VOICE, 0);
    vm_call(t, hover ? "miniscenario3" : "miniscenario2", NULL);
    return true;
}

void mini_draw(TH *t, u32 *dst) {
    Image *sheet = NULL;
    for (int i = 0; i < 2; i++) {
        MiniButton *m = &t->mini[i];
        if (!m->defined || !m->shown) continue;
        if (!sheet) sheet = image_get(t, "sys_window_button");
        if (!sheet) return;
        blit_part(dst, t->W, t->H, sheet, m->sx, m->sy + (i == t->miniHover ? m->h : 0), m->w, m->h, m->x, m->y, 255);
    }
}

void roulette_start(TH *t, const char *section) {
    memset(&t->roulette, 0, sizeof t->roulette);
    snprintf(t->roulette.wheel, sizeof t->roulette.wheel, "%s", ini_get(&t->rouletteIni, section, "G_Roulette01") ? ini_get(&t->rouletteIni, section, "G_Roulette01") : "");
    snprintf(t->roulette.needle, sizeof t->roulette.needle, "%s", ini_get(&t->rouletteIni, section, "G_Roulette02") ? ini_get(&t->rouletteIni, section, "G_Roulette02") : "");
    const char *stop = ini_get(&t->rouletteIni, section, "RouletteStopSE");
    snprintf(t->roulette.stopSe, sizeof t->roulette.stopSe, "%s", stop ? stop : "");
    int pos[2] = {0};
    ini_ints(&t->rouletteIni, section, "G_Roulette_Pos", pos, 2);
    t->roulette.nx = pos[0];
    t->roulette.ny = pos[1];
    t->roulette.speed = ini_int(&t->rouletteIni, section, "RouletSpeed", 10);
    t->roulette.start = t->now;
    t->roulette.stopAt = t->now + (u32)ini_int(&t->rouletteIni, section, "ClickStartWait", 1500);
    const char *start = ini_get(&t->rouletteIni, section, "RouletteStartSE");
    if (start) snd_play(t, CH_SE, PAC_SE, start, false, 0);
    t->sceneDirty = true;
}

void roulette_update(TH *t) {
    if (!t->roulette.stopped) {
        t->roulette.angle = fmodf((float)(t->now - t->roulette.start) * (float)t->roulette.speed * 0.06f, 360.0f);
        t->dirty = true;
        if ((t->click || t->keyPress[0x0d] || t->keyPress[0x20]) && t->now >= t->roulette.stopAt) {
            t->roulette.stopped = true;
            t->roulette.stopAt = t->now + 1000;
            if (t->roulette.stopSe[0]) snd_play(t, CH_SE, PAC_SE, t->roulette.stopSe, false, 0);
            t->vars[2][0] = clampi((int)(t->roulette.angle * 100.0f / 360.0f), 0, 99);
        }
        return;
    }
    if (t->now >= t->roulette.stopAt) {
        t->wait = W_NONE;
        t->sceneDirty = true;
    }
}

void roulette_draw(TH *t) {
    Image *wheel = t->roulette.wheel[0] ? image_get(t, t->roulette.wheel) : NULL;
    Image *needle = t->roulette.needle[0] ? image_get(t, t->roulette.needle) : NULL;
    if (wheel) blit(t->screen, t->W, t->H, wheel, 0, 0, 255);
    if (needle) blit_rotated(t->screen, t->W, t->H, needle, t->roulette.nx + needle->cw / 2, t->roulette.ny + needle->ch / 2, t->roulette.angle);
}

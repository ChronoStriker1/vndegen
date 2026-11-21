#include "anim.h"

enum { BAR_Y = 696, BAR_W = 56, BAR_H = 22, BAR_PITCH = 58, BAR_GAP = 10, LOCK_Y = 695 };

static const UILayout UI[GAME_COUNT] = {
    [GAME_DDD] = {24, 194, 583, 180, 629, 583, 46, 884,
                  606, 11, {2, 7, 10},
                  {BAR_QSAVE, BAR_QLOAD, BAR_SAVE, BAR_LOAD, BAR_FLOW, BAR_SET, BAR_VOL, BAR_SKIP, BAR_AUTO, BAR_HIST, BAR_TITLE},
                  568, 712, 578, 204, 80, 19, 23},
    [GAME_MGSW] = {19, 169, 583, 164, 626, 626, 41, 700,
                   664, 10, {2, 6, 9},
                   {BAR_QSAVE, BAR_QLOAD, BAR_SAVE, BAR_LOAD, BAR_SET, BAR_VOL, BAR_SKIP, BAR_AUTO, BAR_HIST, BAR_TITLE},
                   624, 656, 636, 203, 51, 18, 23},
};

const UILayout *an_ui(const AN *a) { return &UI[a->game]; }

static int bar_x(const UILayout *u, int i) {
    int x = u->barX + BAR_PITCH * i;
    for (int g = 0; g < 3; g++)
        if (i >= u->barGap[g]) x += BAR_GAP;
    return x;
}

static int bar_index(AN *a) {
    const UILayout *u = an_ui(a);
    if (a->my < BAR_Y || a->my >= BAR_Y + BAR_H) return -1;
    for (int i = 0; i < u->barCount; i++)
        if (a->mx >= bar_x(u, i) && a->mx < bar_x(u, i) + BAR_W) return i;
    return -1;
}

int an_bar_hit(AN *a) {
    int i = bar_index(a);
    return i < 0 ? -1 : an_ui(a)->barAction[i];
}

static bool in_rect(AN *a, int x, int y, int w, int h) { return a->mx >= x && a->my >= y && a->mx < x + w && a->my < y + h; }

void an_bar_draw(AN *a, Image *ui) {
    const UILayout *u = an_ui(a);
    an_gfx_blit(a->fb, a->W, a->H, ui, 2, 118, u->stripW, 26, u->stripX, BAR_Y - 2, 255);
    an_gfx_blit(a->fb, a->W, a->H, ui, u->lockAX, u->lockAY, u->lockW, u->lockH, u->lockX, LOCK_Y, 255);
    int hover = bar_index(a);
    for (int i = 0; i < u->barCount; i++) {
        int action = u->barAction[i];
        bool lit = i == hover || (action == BAR_SKIP && a->skipMode) || (action == BAR_AUTO && a->autoMode);
        if (lit) an_gfx_blit(a->fb, a->W, a->H, ui, 3 + BAR_PITCH * i, 27, 55, 21, bar_x(u, i) + 1, BAR_Y + 1, 255);
        else an_gfx_blit(a->fb, a->W, a->H, ui, 2 + BAR_PITCH * i, 2, BAR_W, BAR_H, bar_x(u, i), BAR_Y, 255);
    }
}

bool an_close_hit(AN *a) { return in_rect(a, 1243, 563, 28, 28); }
bool an_voice_hit(AN *a) { return a->voiceName && a->name && in_rect(a, 92, 563, 76, 28); }

enum { LOG_TOP = 68, LOG_LINE = 46, LOG_END = 666, LOG_TEXT_X = 115, THUMB_X = 1225, THUMB_Y = 60, THUMB_W = 30, THUMB_H = 601, THUMB_MIN = 12 };

typedef struct LogLayout {
    int size, nameX, namedTextX, pitch, trackAX, trackAY, trackW, trackH, trackX, trackY;
    bool fractional;
} LogLayout;

static const LogLayout LOG[GAME_COUNT] = {
    [GAME_DDD] = {24, 128, 115, 46, 5, 22, 35, 610, 1223, 56, false},
    [GAME_MGSW] = {20, 132, 127, 41, 3, 19, 38, 615, 1221, 53, true},
};

static int entry_lines(AN *a, const LogEntry *e) {
    int lines = 1;
    for (const char *p = e->text; *p; p++)
        if (p[0] == '@' && p[1] == 'n') lines++;
    return lines + (e->name ? 1 : 0) + 1;
}

static int log_first_for_bottom(AN *a) {
    int h = 0, i = a->logCount - 1;
    for (; i >= 0; i--) {
        h += entry_lines(a, &a->logs[i]) * LOG_LINE;
        if (h > LOG_END - LOG_TOP) return i + 1;
    }
    return 0;
}

static int log_y(AN *a) {
    if (a->logTop < log_first_for_bottom(a)) return LOG_TOP;
    int h = 0;
    for (int i = a->logTop; i < a->logCount; i++) h += entry_lines(a, &a->logs[i]) * LOG_LINE;
    return LOG_END - h;
}

static void log_open(AN *a) {
    if (!a->logCount) return;
    a->screen = SCR_LOG;
    a->logTop = log_first_for_bottom(a);
}

static void draw_thumb(AN *a, Image *b, int shown) {
    int last = log_first_for_bottom(a);
    int h = a->logCount ? clampi(THUMB_H * shown / a->logCount, THUMB_MIN, THUMB_H) : THUMB_H;
    int y = THUMB_Y + (last ? (THUMB_H - h) * a->logTop / last : 0);
    an_gfx_blit_fit(a->fb, a->W, a->H, b, 49, 26, THUMB_W, THUMB_H, THUMB_X, y, THUMB_W, h);
}

static void draw_log(AN *a) {
    Image *veil = an_image_load(a, "spBackLog");
    if (veil) an_gfx_blit(a->fb, a->W, a->H, veil, 0, 0, a->W, a->H, 0, 0, 255);
    Image *b = an_image_load(a, "spBackLogButton");
    const LogLayout *L = &LOG[a->game];
    int y = log_y(a), shown = 0;
    for (int i = a->logTop; i < a->logCount; i++) {
        const LogEntry *e = &a->logs[i];
        int h = entry_lines(a, e) * LOG_LINE;
        if (y + h > LOG_END) break;
        shown++;
        bool voice = e->voice && e->voice[0];
        if (b) {
            an_gfx_blit(a->fb, a->W, a->H, b, 2, in_rect(a, 21, y, 32, 32) ? 925 : 891, 32, 32, 21, y, 255);
            if (voice) an_gfx_blit(a->fb, a->W, a->H, b, 36, in_rect(a, 71, y, 32, 32) ? 925 : 891, 32, 32, 71, y, 255);
        }
        int base = y + 23, x = LOG_TEXT_X;
        a->fractionalText = L->fractional;
        if (e->name) {
            an_text_draw(a, L->nameX, base, e->name, L->size, 0xffffffffu, 0x90000000u, 0, L->pitch);
            base += L->pitch;
            x = L->namedTextX;
        }
        an_text_draw(a, x, base, e->text, L->size, 0xffffffffu, 0x90000000u, 1060, L->pitch);
        a->fractionalText = false;
        y += h;
    }
    if (b) {
        an_gfx_blit(a->fb, a->W, a->H, b, L->trackAX, L->trackAY, L->trackW, L->trackH, L->trackX, L->trackY, 255);
        draw_thumb(a, b, shown);
        an_gfx_blit(a->fb, a->W, a->H, b, 3, in_rect(a, 1221, 14, 38, 38) ? 696 : 654, 38, 38, 1221, 14, 255);
        an_gfx_blit(a->fb, a->W, a->H, b, 45, in_rect(a, 1221, 671, 38, 38) ? 696 : 654, 38, 38, 1221, 671, 255);
        an_gfx_blit(a->fb, a->W, a->H, b, 3, in_rect(a, 63, 670, 278, 40) ? 813 : 771, 278, 40, 63, 670, 255);
    }
}

static void log_scroll(AN *a, int delta) {
    int last = log_first_for_bottom(a);
    a->logTop = clampi(a->logTop + delta, 0, last);
}

static void log_input(AN *a) {
    if (a->wheel) {
        if (a->wheel < 0 && a->logTop >= log_first_for_bottom(a)) a->screen = SCR_NONE;
        else log_scroll(a, -a->wheel);
        a->wheel = 0;
    }
    if (a->rclicked) {
        a->rclicked = false;
        a->screen = SCR_NONE;
        return;
    }
    if (!a->clicked) return;
    a->clicked = false;
    if (in_rect(a, 63, 670, 278, 40)) a->screen = SCR_NONE;
    else if (in_rect(a, 1221, 14, 38, 38)) log_scroll(a, -1);
    else if (in_rect(a, 1221, 671, 38, 38)) log_scroll(a, 1);
    else {
        int y = log_y(a);
        for (int i = a->logTop; i < a->logCount; i++) {
            const LogEntry *e = &a->logs[i];
            int h = entry_lines(a, e) * LOG_LINE;
            if (y + h > LOG_END) break;
            if (e->voice && e->voice[0] && in_rect(a, 71, y, 32, 32)) an_snd_play(a, CH_VOICE, e->voice, false, true);
            y += h;
        }
    }
}

void an_ui_render(AN *a) {
    if (a->screen == SCR_LOG) draw_log(a);
}

static void bar_action(AN *a, int i) {
    switch (i) {
    case BAR_SKIP: a->skipMode = !a->skipMode; break;
    case BAR_AUTO: a->autoMode = !a->autoMode; break;
    case BAR_HIST: log_open(a); break;
    case BAR_SAVE: an_slots_open(a, true); break;
    case BAR_LOAD: an_slots_open(a, false); break;
    case BAR_QSAVE:
        an_capture_shot(a);
        an_quick_save(a);
        break;
    case BAR_QLOAD: an_quick_load(a); break;
    case BAR_TITLE: an_confirm(a, MSG_TITLE, ACT_TITLE, 0); break;
    case BAR_FLOW: an_flow_open(a, false); break;
    case BAR_VOL: an_volume_open(a); break;
    case BAR_SET: an_settings_open(a); break;
    default: an_log(a, "menu bar button %d isn't built yet", i); break;
    }
}

bool an_ui_input(AN *a) {
    if (a->screen == SCR_LOG) {
        log_input(a);
        return true;
    }
    if (a->screen == SCR_HIDDEN) {
        if (a->clicked || a->rclicked || a->wheel) a->screen = SCR_NONE;
        a->clicked = a->rclicked = false;
        a->wheel = 0;
        return true;
    }
    if (a->phase != PH_GAME || !a->scene.window || a->th.wait == W_MOVIE) return false;
    if (a->wheel > 0) {
        a->wheel = 0;
        log_open(a);
        return true;
    }
    a->wheel = 0;
    if (a->rclicked) {
        a->rclicked = false;
        if (a->skipMode || a->autoMode) a->skipMode = a->autoMode = false;
        else a->screen = SCR_HIDDEN;
        return true;
    }
    if (!a->clicked) return false;
    int hit = an_bar_hit(a);
    if (hit >= 0) {
        a->clicked = false;
        bar_action(a, hit);
        return true;
    }
    if (an_close_hit(a)) {
        a->clicked = false;
        a->screen = SCR_HIDDEN;
        return true;
    }
    if (an_voice_hit(a)) {
        a->clicked = false;
        an_snd_play(a, CH_VOICE, a->voiceName, false, true);
        return true;
    }
    if (a->skipMode || a->autoMode) {
        a->skipMode = a->autoMode = false;
        a->clicked = false;
        return true;
    }
    return false;
}

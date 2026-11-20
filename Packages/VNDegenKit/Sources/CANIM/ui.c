#include "anim.h"

enum { BAR_COUNT = 11, BAR_Y = 696, BAR_W = 56, BAR_H = 22 };

static int bar_x(int i) { return 606 + 58 * i + (i >= 2) * 10 + (i >= 7) * 10 + (i >= 10) * 10; }

int an_bar_hit(AN *a) {
    if (a->my < BAR_Y || a->my >= BAR_Y + BAR_H) return -1;
    for (int i = 0; i < BAR_COUNT; i++)
        if (a->mx >= bar_x(i) && a->mx < bar_x(i) + BAR_W) return i;
    return -1;
}

static bool in_rect(AN *a, int x, int y, int w, int h) { return a->mx >= x && a->my >= y && a->mx < x + w && a->my < y + h; }

void an_bar_draw(AN *a, Image *ui) {
    int hover = an_bar_hit(a);
    for (int i = 0; i < BAR_COUNT; i++) {
        bool lit = i == hover || (i == BAR_SKIP && a->skipMode) || (i == BAR_AUTO && a->autoMode);
        if (lit) an_gfx_blit(a->fb, a->W, a->H, ui, 3 + 58 * i, 27, 55, 21, bar_x(i) + 1, BAR_Y + 1, 255);
        else an_gfx_blit(a->fb, a->W, a->H, ui, 2 + 58 * i, 2, BAR_W, BAR_H, bar_x(i), BAR_Y, 255);
    }
}

bool an_close_hit(AN *a) { return in_rect(a, 1243, 563, 28, 28); }
bool an_voice_hit(AN *a) { return a->voiceName && a->name && in_rect(a, 92, 563, 76, 28); }

enum { LOG_TOP = 68, LOG_LINE = 46, LOG_BOTTOM = 650, LOG_TEXT_X = 115, LOG_NAME_X = 128, LOG_SIZE = 24 };

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
        if (h > LOG_BOTTOM - LOG_TOP) return i + 1;
    }
    return 0;
}

static void log_open(AN *a) {
    if (!a->logCount) return;
    a->screen = SCR_LOG;
    a->logTop = log_first_for_bottom(a);
}

static void draw_log(AN *a) {
    Image *veil = an_image_load(a, "spBackLog");
    if (veil) an_gfx_blit(a->fb, a->W, a->H, veil, 0, 0, a->W, a->H, 0, 0, 255);
    Image *b = an_image_load(a, "spBackLogButton");
    int y = LOG_TOP;
    for (int i = a->logTop; i < a->logCount; i++) {
        const LogEntry *e = &a->logs[i];
        int h = entry_lines(a, e) * LOG_LINE;
        if (y + h - LOG_LINE > LOG_BOTTOM) break;
        if (b) {
            an_gfx_blit(a->fb, a->W, a->H, b, 2, 891, 32, 32, 21, y, 255);
            if (e->voice && e->voice[0]) an_gfx_blit(a->fb, a->W, a->H, b, 2, 925, 32, 32, 71, y, 255);
        }
        int base = y + 23;
        if (e->name) {
            an_text_draw(a, LOG_NAME_X, base, e->name, LOG_SIZE, 0xffffffffu, 0x90000000u, 0, LOG_LINE);
            base += LOG_LINE;
        }
        an_text_draw(a, LOG_TEXT_X, base, e->text, LOG_SIZE, 0xffffffffu, 0x90000000u, 1060, LOG_LINE);
        y += h;
    }
    if (b) {
        an_gfx_blit(a->fb, a->W, a->H, b, 5, 22, 35, 610, 1223, 56, 255);
        an_gfx_blit(a->fb, a->W, a->H, b, in_rect(a, 1221, 14, 38, 38) ? 45 : 3, 654, 38, 38, 1221, 14, 255);
        an_gfx_blit(a->fb, a->W, a->H, b, in_rect(a, 1221, 671, 38, 38) ? 45 : 3, 696, 38, 38, 1221, 671, 255);
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
        int y = LOG_TOP;
        for (int i = a->logTop; i < a->logCount; i++) {
            const LogEntry *e = &a->logs[i];
            int h = entry_lines(a, e) * LOG_LINE;
            if (y + h - LOG_LINE > LOG_BOTTOM) break;
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

#include "anim.h"

static bool in_rect(AN *a, int x, int y, int w, int h) { return a->mx >= x && a->my >= y && a->mx < x + w && a->my < y + h; }

typedef struct Rect {
    int x, y, w, h;
} Rect;

static const Rect MESSAGES[] = {
    [MSG_TITLE] = {60, 452, 390, 76},  [MSG_QUIT] = {60, 534, 390, 76},  [MSG_OVERWRITE] = {60, 712, 390, 42},
    [MSG_DELETE] = {556, 712, 400, 42}, [MSG_JUMP] = {556, 794, 410, 44}, [MSG_RESET_FLOW] = {556, 630, 400, 42},
};

enum { BOX_X = 318, BOX_Y = 202, YES_X = 387, NO_X = 677, YESNO_Y = 336, YESNO_W = 216, YESNO_H = 72 };

void an_confirm(AN *a, int message, int action, int arg) {
    if (a->option[OPT_CONFIRM] == 1 && action != ACT_SAVE) {
        an_do_action(a, action, arg);
        return;
    }
    a->confirmMsg = message;
    a->confirmAction = action;
    a->confirmArg = arg;
    a->confirmOpen = true;
}

static void draw_confirm(AN *a) {
    Image *veil = an_image_load(a, "spMsgboxBase");
    if (veil) an_gfx_blit(a->fb, a->W, a->H, veil, 0, 0, a->W, a->H, 0, 0, 255);
    Image *m = an_image_load(a, "spMsgbox");
    if (!m) return;
    an_gfx_blit(a->fb, a->W, a->H, m, 12, 13, 644, 254, BOX_X, BOX_Y, 255);
    const Rect *r = &MESSAGES[a->confirmMsg];
    int ty = r->h > 50 ? 240 : 262;
    an_gfx_blit(a->fb, a->W, a->H, m, r->x, r->y, r->w, r->h, 640 - r->w / 2, ty, 255);
    bool yes = in_rect(a, YES_X, YESNO_Y, YESNO_W, YESNO_H), no = in_rect(a, NO_X, YESNO_Y, YESNO_W, YESNO_H);
    an_gfx_blit(a->fb, a->W, a->H, m, 6, yes ? 370 : 288, YESNO_W, YESNO_H, YES_X, YESNO_Y, 255);
    an_gfx_blit(a->fb, a->W, a->H, m, 232, no ? 370 : 288, YESNO_W, YESNO_H, NO_X, YESNO_Y, 255);
}

static void confirm_input(AN *a) {
    if (a->rclicked) {
        a->rclicked = false;
        a->confirmOpen = false;
        return;
    }
    if (!a->clicked) return;
    a->clicked = false;
    if (in_rect(a, NO_X, YESNO_Y, YESNO_W, YESNO_H)) a->confirmOpen = false;
    else if (in_rect(a, YES_X, YESNO_Y, YESNO_W, YESNO_H)) {
        a->confirmOpen = false;
        an_do_action(a, a->confirmAction, a->confirmArg);
    }
}

enum { CARD_X = 28, CARD_Y = 120, CARD_DX = 310, CARD_DY = 273, CARD_W = 296, CARD_H = 260 };
enum { PAGE_X = 201, PAGE_Y = 16, PAGE_DX = 82, PAGE_DY = 45, PAGE_W = 78, PAGE_H = 38 };

static Rect card(int i) { return (Rect){CARD_X + CARD_DX * (i % 4), CARD_Y + CARD_DY * (i / 4), CARD_W - 10, CARD_H - 10}; }
static Rect page_button(int i) { return (Rect){PAGE_X + PAGE_DX * (i % 10), PAGE_Y + PAGE_DY * (i / 10), PAGE_W, PAGE_H}; }

static const int BIG_DIGIT_X[10] = {598, 641, 682, 724, 767, 809, 850, 893, 934, 976};

static Image *thumb(AN *a, int slot) {
    if (slot < 0 || slot >= 180) return NULL;
    if (a->thumbs[slot] && a->thumbsSerial[slot] == a->saveSerial) return a->thumbs[slot];
    an_image_free(a->thumbs[slot]);
    a->thumbs[slot] = NULL;
    char *p = an_fmt("%s/cg/pSaveThumb%03d.png", a->saveDir, slot);
    size_t n;
    u8 *d = an_read_file(p, &n);
    free(p);
    if (d) {
        a->thumbs[slot] = an_image_png(d, n);
        free(d);
    }
    a->thumbsSerial[slot] = a->saveSerial;
    return a->thumbs[slot];
}

enum { CARD_LINES = 2, CARD_LINE_CHARS = 20 };

static void card_comment(const char *in, char *out, size_t cap) {
    size_t n = 0;
    int lines = 0;
    out[0] = 0;
    while (*in && lines < CARD_LINES) {
        size_t len = 0, cut = 0;
        while (in[len] && !(in[len] == '@' && in[len + 1] == 'n') && len < CARD_LINE_CHARS) {
            if (in[len] == ' ') cut = len;
            len++;
        }
        bool more = in[len] && !(in[len] == '@' && in[len + 1] == 'n');
        if (more && in[len] != ' ' && cut) len = cut;
        if (lines && n + 2 < cap) {
            out[n++] = '@';
            out[n++] = 'n';
        }
        for (size_t i = 0; i < len && n + 1 < cap; i++) out[n++] = in[i];
        out[n] = 0;
        in += len;
        if (in[0] == '@' && in[1] == 'n') in += 2;
        else
            while (*in == ' ') in++;
        lines++;
    }
}

static void draw_thumb(AN *a, const Image *t, int x, int y) {
    enum { TW = 184, TH = 104 };
    for (int j = 0; j < TH; j++) {
        int sy = j * 144 / TH;
        u32 *d = a->fb + (size_t)(y + j) * (size_t)a->W + (size_t)x;
        for (int i = 0; i < TW; i++) {
            u32 c = an_image_at(t, i * 256 / TW, sy);
            if (c >> 24) d[i] = c | 0xff000000u;
        }
    }
}

static void dot(AN *a, int x, int y) {
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++) {
            int px = x + i, py = y + j;
            if (px >= 0 && py >= 0 && px < a->W && py < a->H) a->fb[(size_t)py * (size_t)a->W + (size_t)px] = 0xffffffffu;
        }
}

static void vertical(AN *a, Image *b, const char *s, int x, int y) {
    for (int i = (int)strlen(s) - 1; i >= 0; i--) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            an_gfx_blit(a->fb, a->W, a->H, b, 599 + 18 * (c - '0'), 516, 15, 11, x, y, 255);
            y += 11;
        } else if (c == '.') {
            dot(a, x + 1, y + 1);
            y += 6;
        } else {
            dot(a, x + 4, y + 1);
            dot(a, x + 9, y + 1);
            y += 6;
        }
    }
}

static void draw_slots(AN *a, bool saving) {
    Image *veil = an_image_load(a, saving ? "spSave" : "spLoad");
    if (veil) an_gfx_blit(a->fb, a->W, a->H, veil, 0, 0, a->W, a->H, 0, 0, 255);
    Image *b = an_image_load(a, saving ? "spSaveButton" : "spLoadButton");
    if (!b) return;
    for (int i = 0; i < 20; i++) {
        Rect r = page_button(i);
        int row = i < 10 ? 3 : 129;
        int fy = i == a->slotPage ? row + 84 : in_rect(a, r.x, r.y, r.w, r.h) ? row + 42 : row;
        an_gfx_blit(a->fb, a->W, a->H, b, 3 + 82 * (i % 10), fy, PAGE_W, PAGE_H, r.x, r.y, 255);
    }
    for (int i = 0; i < 8; i++) {
        int slot = a->slotPage * 8 + i;
        Rect r = card(i);
        SlotInfo info;
        bool used = an_slot_info(a, slot, &info);
        bool hover = in_rect(a, r.x, r.y, r.w, r.h);
        an_gfx_blit(a->fb, a->W, a->H, b, used ? 300 : 2, 482, 296, 260, r.x, r.y, 255);
        if (slot == a->lastSlot && used) {
            an_gfx_blit(a->fb, a->W, a->H, b, 301, 745, 294, 258, r.x + 1, r.y + 1, 255);
            an_gfx_blit(a->fb, a->W, a->H, b, 816, 528, 48, 22, r.x + 63, r.y + 16, 255);
        }
        if (hover) an_gfx_blit(a->fb, a->W, a->H, b, 3, 745, 294, 258, r.x + 1, r.y + 1, 255);
        int n = slot + 1;
        for (int k = 0; k < 3; k++) {
            int digit = k == 0 ? n % 10 : k == 1 ? n / 10 % 10 : n / 100 % 10;
            an_gfx_blit(a->fb, a->W, a->H, b, BIG_DIGIT_X[digit], 482, 39, 34, r.x + 10, r.y + 143 + 42 * k, 255);
        }
        if (!used) continue;
        Image *t = thumb(a, slot);
        if (t) draw_thumb(a, t, r.x + 78, r.y + 46);
        char date[24], clock[24];
        snprintf(date, sizeof date, "%04d.%02d.%02d", info.year, info.month, info.day);
        snprintf(clock, sizeof clock, "%02d:%02d:%02d", info.hour, info.minute, info.second);
        vertical(a, b, date, r.x + 14, r.y + 19);
        vertical(a, b, clock, r.x + 34, r.y + 20);
        char lines[128];
        card_comment(info.comment, lines, sizeof lines);
        an_text_draw(a, r.x + 78, r.y + 180, lines, 16, 0xff503040u, 0, 0, 18);
    }
    an_gfx_blit(a->fb, a->W, a->H, b, 3, in_rect(a, 62, 670, 278, 40) ? 296 : 254, 278, 40, 62, 670, 255);
    an_gfx_blit(a->fb, a->W, a->H, b, 285, in_rect(a, 501, 670, 278, 40) ? 296 : 254, 278, 40, 501, 670, 255);
    an_gfx_blit(a->fb, a->W, a->H, b, 567, in_rect(a, 940, 670, 278, 40) ? 296 : 254, 278, 40, 940, 670, 255);
}

void an_slots_open(AN *a, bool saving) {
    if (saving) an_capture_shot(a);
    a->screen = saving ? SCR_SAVE : SCR_LOAD;
    a->slotPage = a->lastSlot >= 0 && a->lastSlot < 160 ? a->lastSlot / 8 : 0;
}

static void slots_input(AN *a, bool saving) {
    if (a->rclicked) {
        a->rclicked = false;
        a->screen = a->phase == PH_TITLE ? SCR_NONE : SCR_NONE;
        return;
    }
    if (!a->clicked) return;
    a->clicked = false;
    for (int i = 0; i < 20; i++) {
        Rect r = page_button(i);
        if (in_rect(a, r.x, r.y, r.w, r.h)) {
            a->slotPage = i;
            return;
        }
    }
    if (in_rect(a, 62, 670, 278, 40)) {
        a->screen = SCR_NONE;
        return;
    }
    if (in_rect(a, 501, 670, 278, 40)) {
        if (a->phase == PH_GAME) an_confirm(a, MSG_TITLE, ACT_TITLE, 0);
        else a->screen = SCR_NONE;
        return;
    }
    if (in_rect(a, 940, 670, 278, 40)) {
        if (a->phase == PH_GAME) an_slots_open(a, !saving);
        return;
    }
    for (int i = 0; i < 8; i++) {
        Rect r = card(i);
        if (!in_rect(a, r.x, r.y, r.w, r.h)) continue;
        int slot = a->slotPage * 8 + i;
        SlotInfo info;
        bool used = an_slot_info(a, slot, &info);
        if (saving) {
            if (used) an_confirm(a, MSG_OVERWRITE, ACT_SAVE, slot);
            else an_do_action(a, ACT_SAVE, slot);
        } else if (used)
            an_do_action(a, ACT_LOAD, slot);
        return;
    }
}

void an_do_action(AN *a, int action, int arg) {
    switch (action) {
    case ACT_SAVE:
        if (an_save_slot(a, arg)) a->saveSerial++;
        break;
    case ACT_LOAD:
        if (an_load_slot(a, arg)) a->screen = SCR_NONE;
        break;
    case ACT_TITLE:
        an_go_title(a);
        break;
    case ACT_QUIT: a->quit = true; break;
    case ACT_JUMP: an_flow_jump(a, arg); break;
    }
}

static const int SLIDER_Y[5] = {173, 273, 373, 473, 573};
enum { LEFT_X = 51, RIGHT_X = 680, KNOB_W = 19, KNOB_H = 32, SLIDER_W = 500 };

void an_volume_defaults(AN *a) {
    static const int defaults[5] = {80, 50, 70, 70, 70};
    memcpy(a->volume, defaults, sizeof defaults);
    for (int i = 0; i < 3; i++) {
        a->voiceVolume[i] = 70;
        a->bgvVolume[i] = 70;
        a->talkVolume[i] = 20;
    }
}

void an_volume_open(AN *a) {
    a->screen = SCR_VOLUME;
    a->dragSlider = -1;
}

static int *slider_value(AN *a, int i) {
    if (i < 5) return &a->volume[i];
    int c = a->volumeChar;
    return i == 5 ? &a->voiceVolume[c] : i == 6 ? &a->bgvVolume[c] : &a->talkVolume[c];
}

static void slider_pos(int i, int *x, int *y) {
    *x = i < 5 ? LEFT_X : RIGHT_X;
    *y = i < 5 ? SLIDER_Y[i] : SLIDER_Y[i - 3];
}

static void draw_volume(AN *a) {
    Image *panel = an_image_load(a, "spvolume");
    if (panel) an_gfx_blit(a->fb, a->W, a->H, panel, 0, 0, a->W, a->H, 0, 0, 255);
    Image *b = an_image_load(a, "spVolumeButton");
    if (!b) return;
    an_gfx_blit(a->fb, a->W, a->H, b, 2 + 594 * a->volumeChar, 425, 592, 464, 644, 176, 255);
    for (int c = 0; c < 3; c++) {
        int fy = c == a->volumeChar ? 343 : in_rect(a, 646 + 90 * c, 97, 80, 80) ? 261 : 179;
        an_gfx_blit(a->fb, a->W, a->H, b, 2 + 82 * c, fy, 80, 80, 646 + 90 * c, 97, 255);
    }
    for (int i = 0; i < 8; i++) {
        int x, y;
        slider_pos(i, &x, &y);
        int kx = x + *slider_value(a, i) * SLIDER_W / 100;
        an_gfx_blit(a->fb, a->W, a->H, b, 848, a->dragSlider == i ? 163 : in_rect(a, kx, y, KNOB_W, KNOB_H) ? 129 : 95, KNOB_W, KNOB_H, kx, y, 255);
    }
    an_gfx_blit(a->fb, a->W, a->H, b, 3, in_rect(a, 940, 5, 278, 40) ? 137 : 95, 278, 40, 940, 5, 255);
    an_gfx_blit(a->fb, a->W, a->H, b, 567, in_rect(a, 62, 670, 278, 40) ? 137 : 95, 278, 40, 62, 670, 255);
    an_gfx_blit(a->fb, a->W, a->H, b, 285, in_rect(a, 940, 670, 278, 40) ? 137 : 95, 278, 40, 940, 670, 255);
}

static void apply_volumes(AN *a) {
    an_snd_set_volume(a, CH_BGM, an_channel_volume(a, CH_BGM, NULL, 0));
    for (int c = CH_SE0; c < CH_BGV0; c++) an_snd_set_volume(a, c, an_channel_volume(a, c, NULL, 0));
    an_snd_set_volume(a, CH_VOICE, an_channel_volume(a, CH_VOICE, a->voiceName, 0));
}

static void volume_input(AN *a) {
    if (a->dragSlider >= 0) {
        int x, y;
        slider_pos(a->dragSlider, &x, &y);
        *slider_value(a, a->dragSlider) = clampi((a->mx - x - KNOB_W / 2) * 100 / SLIDER_W, 0, 100);
        apply_volumes(a);
        if (!a->down) a->dragSlider = -1;
        a->clicked = false;
        return;
    }
    if (a->rclicked) {
        a->rclicked = false;
        a->screen = SCR_NONE;
        return;
    }
    if (a->down)
        for (int i = 0; i < 8; i++) {
            int x, y;
            slider_pos(i, &x, &y);
            if (in_rect(a, x - 10, y, SLIDER_W + KNOB_W + 20, KNOB_H)) {
                a->dragSlider = i;
                return;
            }
        }
    if (!a->clicked) return;
    a->clicked = false;
    for (int c = 0; c < 3; c++)
        if (in_rect(a, 646 + 90 * c, 97, 80, 80)) a->volumeChar = c;
    if (in_rect(a, 940, 5, 278, 40)) {
        an_volume_defaults(a);
        apply_volumes(a);
    }
    if (in_rect(a, 62, 670, 278, 40)) a->screen = SCR_NONE;
}

typedef struct Option {
    int x, y, ax, ay;
} Option;

static const Option OPTIONS[11] = {
    {36, 100, 5, 6},   {36, 190, 313, 6},  {36, 280, 621, 6},  {36, 370, 5, 156},  {36, 460, 313, 156}, {36, 550, 621, 156},
    {372, 100, 313, 456}, {372, 190, 5, 306}, {372, 280, 313, 306}, {372, 370, 621, 306}, {372, 460, 5, 456},
};
static const int OPTION_DEFAULT[11] = {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0};
enum { OPT_W = 147, OPT_H = 41, OPT_DX = 154, SET_KNOB_X = 710, SET_KNOB_RANGE = 430 };
static const int SET_SLIDER_Y[3] = {281, 371, 461};

void an_settings_defaults(AN *a) {
    memcpy(a->option, OPTION_DEFAULT, sizeof a->option);
    a->windowOpacity = 58;
    a->textSpeed = 99;
    a->autoSpeed = 0;
}

void an_settings_open(AN *a) {
    a->screen = SCR_SETTINGS;
    a->dragSlider = -1;
}

static int *set_slider(AN *a, int i) { return i == 0 ? &a->windowOpacity : i == 1 ? &a->textSpeed : &a->autoSpeed; }

static void draw_settings(AN *a) {
    Image *panel = an_image_load(a, "spconfig");
    if (panel) an_gfx_blit(a->fb, a->W, a->H, panel, 0, 0, a->W, a->H, 0, 0, 255);
    Image *b = an_image_load(a, "spconfigButton");
    if (!b) return;
    for (int i = 0; i < 11; i++) {
        const Option *o = &OPTIONS[i];
        for (int k = 0; k < 2; k++) {
            int x = o->x + OPT_DX * k;
            int row = a->option[i] == k ? 100 : in_rect(a, x, o->y, OPT_W, OPT_H) ? 50 : 0;
            an_gfx_blit(a->fb, a->W, a->H, b, o->ax + OPT_DX * k, o->ay + row, OPT_W, OPT_H, x, o->y, 255);
        }
    }
    for (int i = 0; i < 3; i++) {
        int kx = SET_KNOB_X + *set_slider(a, i) * SET_KNOB_RANGE / 100, y = SET_SLIDER_Y[i];
        an_gfx_blit(a->fb, a->W, a->H, b, 284, a->dragSlider == 10 + i ? 754 : in_rect(a, kx, y, 19, 32) ? 720 : 686, 19, 32, kx, y, 255);
    }
    an_gfx_blit(a->fb, a->W, a->H, b, 285, in_rect(a, 62, 670, 278, 40) ? 644 : 602, 278, 40, 62, 670, 255);
    an_gfx_blit(a->fb, a->W, a->H, b, 3, in_rect(a, 940, 670, 278, 40) ? 728 : 686, 278, 40, 940, 670, 255);
    an_gfx_blit(a->fb, a->W, a->H, b, 3, in_rect(a, 940, 5, 278, 40) ? 644 : 602, 278, 40, 940, 5, 255);
}

static void settings_input(AN *a) {
    if (a->dragSlider >= 10) {
        *set_slider(a, a->dragSlider - 10) = clampi((a->mx - SET_KNOB_X - 9) * 100 / SET_KNOB_RANGE, 0, 100);
        if (!a->down) a->dragSlider = -1;
        a->clicked = false;
        return;
    }
    if (a->rclicked) {
        a->rclicked = false;
        a->screen = SCR_NONE;
        return;
    }
    if (a->down)
        for (int i = 0; i < 3; i++)
            if (in_rect(a, SET_KNOB_X - 10, SET_SLIDER_Y[i], SET_KNOB_RANGE + 40, 32)) {
                a->dragSlider = 10 + i;
                return;
            }
    if (!a->clicked) return;
    a->clicked = false;
    for (int i = 0; i < 11; i++)
        for (int k = 0; k < 2; k++)
            if (in_rect(a, OPTIONS[i].x + OPT_DX * k, OPTIONS[i].y, OPT_W, OPT_H)) a->option[i] = k;
    if (in_rect(a, 62, 670, 278, 40)) a->screen = SCR_NONE;
    if (in_rect(a, 940, 670, 278, 40)) an_volume_open(a);
    if (in_rect(a, 940, 5, 278, 40)) an_settings_defaults(a);
}

void an_screens_render(AN *a) {
    if (a->screen == SCR_SETTINGS) draw_settings(a);
    if (a->screen == SCR_VOLUME) draw_volume(a);
    if (a->screen == SCR_SAVE || a->screen == SCR_LOAD) draw_slots(a, a->screen == SCR_SAVE);
    if (a->confirmOpen) draw_confirm(a);
}

bool an_screens_input(AN *a) {
    if (a->confirmOpen) {
        confirm_input(a);
        return true;
    }
    if (a->screen == SCR_SAVE || a->screen == SCR_LOAD) {
        slots_input(a, a->screen == SCR_SAVE);
        return true;
    }
    if (a->screen == SCR_VOLUME) {
        volume_input(a);
        return true;
    }
    if (a->screen == SCR_SETTINGS) {
        settings_input(a);
        return true;
    }
    return false;
}

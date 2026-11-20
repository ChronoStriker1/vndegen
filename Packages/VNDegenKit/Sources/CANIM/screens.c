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
        an_text_draw(a, r.x + 82, r.y + 194, info.comment, 14, 0xff503040u, 0, 200, 22);
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
    }
}

void an_screens_render(AN *a) {
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
    return false;
}

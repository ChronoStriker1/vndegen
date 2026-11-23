#include "anim.h"

#include <math.h>

enum { ROOM_LAYER = 0, CELLS = 5, CELL_X = 68, CELL_DX = 50, CELL = 48, ARROW_X = 322, ARROW = 32 };
enum { SLIDERS = 10, TRACK_X = 975, TRACK_W = 180, KNOB_W = 19, KNOB_H = 32, DIGIT_W = 12, DIGIT_H = 19 };
enum { PICK_CHAR, PICK_EXPR, PICK_OUTFIT, PICK_BG, PICKS };
enum { SW_LIP, SW_IDLE, SW_TEARS, SW_HAIR, SW_CREST, SWITCHES };
enum { HELP_NONE = -1, DEFAULT_OUTFIT = 2 };

static const int PICK_Y[PICKS] = {159, 249, 339, 429};
static const int PICK_ATLAS_Y[PICKS] = {620, 770, 920, 1070};
static const int SLIDER_Y[SLIDERS] = {74, 117, 161, 204, 247, 290, 333, 376, 419, 462};

typedef struct Switch {
    int onX, offX, y, onAX, offAX, ay, w, h, rowDY;
} Switch;

typedef struct RoomLayout {
    int tabAX, tabW, tabX, buttonAX, buttonW, buttonX, valueRight, zoomRight, digitY;
    Switch sw[SWITCHES];
    int help[PICKS + SLIDERS + SWITCHES + 7];
    int onHelp[SWITCHES], offHelp[SWITCHES];
} RoomLayout;

enum { H_PICK = 0, H_SLIDER = PICKS, H_SWITCH = PICKS + SLIDERS, H_SAVE = H_SWITCH + SWITCHES, H_LOAD, H_HIDE, H_SHOT, H_FOLDER, H_RESET, H_TITLE };

#define HELP(col, row) ((col) * 100 + (row))

static const RoomLayout ROOM[GAME_COUNT] = {
    [GAME_MGSW] = {4, 180, 263, 296, 92, 42, 1233, 1233, 315,
                   {{195, 261, 496, 916, 982, 215, 62, 27, 34},
                    {195, 261, 534, 916, 982, 215, 62, 27, 34},
                    {920, 986, 510, 916, 982, 215, 62, 27, 34},
                    {1115, 1181, 510, 916, 982, 215, 62, 27, 34},
                    {920, 986, 546, 916, 982, 215, 62, 27, 34}},
                   {HELP(0, 0), HELP(0, 1), HELP(0, 2), HELP(0, 3), HELP(0, 6), HELP(0, 7), HELP(0, 8), HELP(0, 9), HELP(1, 0),
                    HELP(1, 1), HELP(1, 2), HELP(1, 3), HELP(1, 4), HELP(1, 5), HELP(0, 4), HELP(0, 5), HELP(1, 6), HELP(1, 6),
                    HELP(1, 6), HELP(1, 7), HELP(1, 8), HELP(2, 0), HELP(2, 1), HELP(2, 2), HELP(2, 3), HELP_NONE},
                   {HELP_NONE, HELP_NONE, HELP_NONE, HELP_NONE, HELP_NONE},
                   {HELP_NONE, HELP_NONE, HELP_NONE, HELP_NONE, HELP_NONE}},
    [GAME_DDD] = {3, 182, 262, 295, 94, 41, 1230, 1230, 315,
                  {{100, 230, 489, 1160, 1282, 213, 118, 30, 34},
                   {100, 230, 534, 1404, 1526, 213, 118, 30, 34},
                   {982, 1112, 505, 916, 1038, 213, 118, 30, 34},
                   {982, 1112, 548, 1648, 1770, 213, 118, 30, 34},
                   {0, 0, -1, 0, 0, 0, 0, 0, 0}},
                  {HELP(0, 0), HELP(0, 1), HELP(0, 2), HELP(0, 3), HELP(1, 1), HELP(1, 2), HELP(1, 3), HELP(1, 4), HELP(1, 5),
                   HELP(1, 6), HELP(1, 7), HELP(1, 8), HELP(1, 9), HELP(2, 0), HELP(0, 4), HELP(3, 1), HELP(2, 9), HELP(3, 0),
                   HELP_NONE, HELP(0, 7), HELP(0, 8), HELP(0, 9), HELP(2, 1), HELP(2, 2), HELP(2, 3), HELP(1, 0)},
                  {HELP(0, 5), HELP(3, 2), HELP_NONE, HELP_NONE, HELP_NONE},
                  {HELP(0, 6), HELP(3, 3), HELP_NONE, HELP_NONE, HELP_NONE}},
};

typedef struct Room {
    int ch, scroll[PICKS], pick[PICKS], px[SLIDERS], drag;
    bool sw[SWITCHES], hidden;
    u32 talkStart;
} Room;

static Room room;

static bool in_rect(AN *a, int x, int y, int w, int h) { return a->mx >= x && a->my >= y && a->mx < x + w && a->my < y + h; }

static RoomChar *cur(AN *a) { return a->def.roomCount ? &a->def.room[room.ch] : NULL; }

static int pick_count(AN *a, int k) {
    RoomChar *c = cur(a);
    if (!c) return 0;
    return k == PICK_CHAR ? a->def.roomCount : k == PICK_EXPR ? c->exprCount : k == PICK_OUTFIT ? c->outfitCount : a->def.roomBgCount;
}

static float slider_var(int i, int px) {
    switch (i) {
    case 5: return (float)px / 3.0f - 10.0f;
    case 6: return (float)(px * 53 / TRACK_W);
    case 7: return px * 46 / TRACK_W >= 46 ? 46.0f : (float)(px * 46 / TRACK_W / 10 * 10);
    case 8: return (float)px / 36.0f;
    case 9: return (float)px / 90.0f;
    default: return (float)px / 3.0f - 30.0f;
    }
}

static int slider_px(int i, float v) {
    float px = i == 5 ? (v + 10.0f) * 3.0f : i == 6 ? ceilf(v * TRACK_W / 53.0f) : i == 7 ? v * TRACK_W / 46.0f : i == 8 ? v * 36.0f : i == 9 ? v * 90.0f : (v + 30.0f) * 3.0f;
    return clampi((int)lroundf(px), 0, TRACK_W);
}

static const char *const SLIDER_VAR[SLIDERS] = {NULL,          "head_slant",   "body_slant", "face_eye_UD", "face_eye_LR",
                                                "face_eye_open", "face_eyebrow", "face_mouth", "face_talk",   "face_cheek"};

static float preset_value(AN *a, const char *preset, const char *var, float fallback) {
    const EmotePreset *p = an_define_preset(&a->def, preset);
    if (!p) return fallback;
    for (int i = 0; i < p->count; i++)
        if (!strcmp(p->vars[i].name, var)) return p->vars[i].value;
    return fallback;
}

static void apply(AN *a) {
    RoomChar *c = cur(a);
    if (!c || !c->outfitCount) return;
    Layer *l = &a->scene.layer[ROOM_LAYER];
    const char *model = c->outfit[clampi(room.pick[PICK_OUTFIT], 0, c->outfitCount - 1)];
    char name[64];
    snprintf(name, sizeof name, "%s.psb", model);
    if (strcmp(l->name, name)) {
        memset(l, 0, sizeof *l);
        l->kind = LK_EMOTE;
        snprintf(l->name, sizeof l->name, "%s", name);
        l->alpha = l->fromAlpha = l->toAlpha = 255;
        l->fresh = true;
    }
    an_layer_apply_preset(a, l, "AllReset", 0);
    an_layer_apply_preset(a, l, room.sw[SW_HAIR] ? "emViewDefault1" : "emViewDefault2", 0);
    for (int i = 0; i < a->def.armCount; i++)
        if (!strcmp(a->def.armModel[i], model)) an_layer_apply_preset(a, l, a->def.armPreset[i], 0);
    if (c->exprCount) an_layer_apply_preset(a, l, c->expr[clampi(room.pick[PICK_EXPR], 0, c->exprCount - 1)], 0);
    for (int i = 1; i < SLIDERS; i++) an_layer_set_var(l, SLIDER_VAR[i], slider_var(i, room.px[i]));
    an_layer_set_var(l, "face_tears", room.sw[SW_TEARS] ? 30.0f : 0.0f);
    an_layer_set_var(l, "fade_x", room.sw[SW_CREST] ? 1.0f : 0.0f);
    snprintf(l->idle, sizeof l->idle, "%s", room.sw[SW_IDLE] ? "sPauseWait01" : "sNoIdle");
    l->scale = 500 + 1500 * room.px[0] / TRACK_W;
    l->varSerial++;
    if (a->def.roomBgCount) snprintf(a->scene.bg, sizeof a->scene.bg, "%s", a->def.roomBg[clampi(room.pick[PICK_BG], 0, a->def.roomBgCount - 1)]);
}

static void load_expression(AN *a) {
    RoomChar *c = cur(a);
    if (!c || !c->exprCount) return;
    const char *e = c->expr[clampi(room.pick[PICK_EXPR], 0, c->exprCount - 1)];
    for (int i = 1; i < SLIDERS; i++) room.px[i] = slider_px(i, preset_value(a, e, SLIDER_VAR[i], 0.0f));
}

static void reset_sliders(AN *a) {
    room.px[0] = 60;
    load_expression(a);
}

static void select_char(AN *a, int ch) {
    room.ch = ch;
    room.pick[PICK_EXPR] = room.scroll[PICK_EXPR] = 0;
    RoomChar *c = cur(a);
    room.pick[PICK_OUTFIT] = c && c->outfitCount ? clampi(DEFAULT_OUTFIT, 0, c->outfitCount - 1) : 0;
    room.scroll[PICK_OUTFIT] = room.pick[PICK_OUTFIT] >= CELLS ? room.pick[PICK_OUTFIT] - CELLS + 1 : 0;
    reset_sliders(a);
    apply(a);
}

void an_room_open(AN *a) {
    memset(&room, 0, sizeof room);
    room.drag = -1;
    room.sw[SW_HAIR] = true;
    a->emoteScale = 0.5f;
    a->slotX[ROOM_LAYER] = 640;
    a->slotY[ROOM_LAYER] = 0;
    select_char(a, 0);
}

void an_room_close(AN *a) {
    memset(&a->scene, 0, sizeof a->scene);
    a->emoteScale = 1.0f;
}

enum { SAVE_SLIDERS = 0x7e9e4, SAVE_OUTFIT = 0x7ee94, SAVE_EXPR_NAME = 0x7ef0c, SAVE_EXPR = 0x7efd4, SAVE_BG = 0x7f04c,
       SAVE_KEEP = 0x7f13c, SAVE_TEARS = 0x7f1b4, SAVE_HAIR = 0x7f22c, SAVE_CREST = 0x7f2a4, SAVE_LIP = 0x7f31c, SAVE_END = 0x7f3a0 };

static void room_save(AN *a) {
    if (!a->sys || a->sysSize < SAVE_END) return;
    u8 *s = a->sys;
    int c = room.ch;
    for (int i = 0; i < SLIDERS; i++) wr32(s + SAVE_SLIDERS + 40 * c + 4 * i, (u32)room.px[i]);
    wr32(s + SAVE_OUTFIT + 4 * c, (u32)room.pick[PICK_OUTFIT]);
    RoomChar *rc = cur(a);
    memset(s + SAVE_EXPR_NAME, 0, 20);
    if (rc && rc->exprCount) snprintf((char *)s + SAVE_EXPR_NAME, 20, "%s", rc->expr[room.pick[PICK_EXPR]]);
    wr32(s + SAVE_EXPR + 4 * c, (u32)room.pick[PICK_EXPR]);
    wr32(s + SAVE_BG + 4 * c, (u32)room.pick[PICK_BG]);
    wr32(s + SAVE_KEEP + 4 * c, 40);
    wr32(s + SAVE_TEARS + 4 * c, room.sw[SW_TEARS]);
    wr32(s + SAVE_HAIR + 4 * c, room.sw[SW_HAIR]);
    wr32(s + SAVE_CREST + 4 * c, room.sw[SW_CREST]);
    wr32(s + SAVE_LIP + 4 * c, room.sw[SW_LIP]);
    an_system_save(a);
}

static void room_load(AN *a) {
    if (!a->sys || a->sysSize < SAVE_END) return;
    const u8 *s = a->sys;
    int c = room.ch;
    i32 outfit = (i32)rd32(s + SAVE_OUTFIT + 4 * c);
    RoomChar *rc = cur(a);
    if (outfit < 0 || !rc || outfit >= rc->outfitCount) return;
    for (int i = 0; i < SLIDERS; i++) room.px[i] = clampi((int)rd32(s + SAVE_SLIDERS + 40 * c + 4 * i), 0, TRACK_W);
    room.pick[PICK_OUTFIT] = outfit;
    room.pick[PICK_EXPR] = clampi((int)rd32(s + SAVE_EXPR + 4 * c), 0, rc->exprCount ? rc->exprCount - 1 : 0);
    room.pick[PICK_BG] = clampi((int)rd32(s + SAVE_BG + 4 * c), 0, a->def.roomBgCount ? a->def.roomBgCount - 1 : 0);
    room.sw[SW_TEARS] = rd32(s + SAVE_TEARS + 4 * c) != 0;
    room.sw[SW_HAIR] = rd32(s + SAVE_HAIR + 4 * c) != 0;
    room.sw[SW_CREST] = rd32(s + SAVE_CREST + 4 * c) != 0;
    room.sw[SW_LIP] = rd32(s + SAVE_LIP + 4 * c) != 0;
    apply(a);
}

static void screenshot(AN *a) {
    char *dir = an_fmt("%s/SaveGraphic", a->saveDir);
    an_mkdirs(dir);
    for (int n = 0; n < 1000; n++) {
        char *p = an_fmt("%s/emotesave_%03d.png", dir, n);
        bool taken = an_file_exists(p);
        if (!taken && !an_png_write(p, a->shot, a->W, a->H)) an_log(a, "can't write %s", p);
        free(p);
        if (!taken) break;
    }
    free(dir);
}

static int cell_icon_x(int k, int i) { return 2 + 50 * i; }

static int pick_icon(AN *a, int k, int index) {
    RoomChar *c = cur(a);
    if (k == PICK_OUTFIT && c && index < c->outfitCount) return c->outfitIcon[index];
    return index;
}

static void draw_value(AN *a, Image *b, const RoomLayout *L, int i) {
    int v = i == 0 ? 50 + 150 * room.px[0] / TRACK_W : i >= 6 ? (int)slider_var(i, room.px[i]) : room.px[i] / 3;
    char s[8];
    int n = snprintf(s, sizeof s, "%d", v);
    int x = (i == 0 ? L->zoomRight : L->valueRight) - n * DIGIT_W;
    for (int k = 0; k < n; k++) an_gfx_blit(a->fb, a->W, a->H, b, 826 + DIGIT_W * (s[k] - '0'), L->digitY, DIGIT_W, DIGIT_H, x + DIGIT_W * k, SLIDER_Y[i] + 7, 255);
}

static int hover_help(AN *a) {
    for (int k = 0; k < PICKS; k++)
        if (in_rect(a, 20, PICK_Y[k] - 44, 340, 90)) return H_PICK + k;
    for (int i = 0; i < SLIDERS; i++)
        if (in_rect(a, 860, SLIDER_Y[i] - 5, 400, 43)) return H_SLIDER + i;
    const RoomLayout *L = &ROOM[a->game];
    for (int k = 0; k < SWITCHES; k++) {
        const Switch *w = &L->sw[k];
        if (w->y >= 0 && (in_rect(a, w->onX, w->y, w->w, w->h) || in_rect(a, w->offX, w->y, w->w, w->h))) return H_SWITCH + k;
    }
    for (int k = 0; k < SWITCHES; k++) {
        const Switch *w = &L->sw[k];
        if (w->y >= 0 && in_rect(a, w->onX < 640 ? 20 : 860, w->y - 6, w->onX < 640 ? 340 : 400, w->h + 12)) return H_SWITCH + k;
    }
    for (int k = 0; k < 3; k++)
        if (in_rect(a, L->buttonX + 106 * k, 591, L->buttonW, 52)) return H_SAVE + k;
    if (in_rect(a, 969, 615, 70, 56)) return H_SHOT;
    if (in_rect(a, 1068, 616, 73, 55)) return H_FOLDER;
    if (in_rect(a, 1166, 612, 63, 63)) return H_RESET;
    if (in_rect(a, 62, 670, 278, 40)) return H_TITLE;
    return -1;
}

void an_room_render(AN *a, Image *b) {
    const RoomLayout *L = &ROOM[a->game];
    Image *bg = an_image_load(a, a->scene.bg);
    an_gfx_fill(a->fb, a->W, a->H, 0xff000000u);
    if (bg) an_gfx_blit(a->fb, a->W, a->H, bg, 0, 0, a->W, a->H, 0, 0, 255);
    Layer *l = &a->scene.layer[ROOM_LAYER];
    if (room.sw[SW_LIP]) an_layer_set_var(l, "face_talk", 2.5f + 2.5f * sinf((float)(a->now - room.talkStart) * 0.02f)), l->varSerial++;
    an_emote_prepare(a);
    if (l->kind == LK_EMOTE) an_emote_draw(a, ROOM_LAYER, l, l->toX, l->toY, 255);
    if (room.hidden) return;
    Image *panel = an_image_load(a, "spEmoteRoomItem");
    if (panel) an_gfx_blit(a->fb, a->W, a->H, panel, 0, 0, a->W, a->H, 0, 0, 255);
    for (int k = 0; k < 5; k++) {
        int state = k == 3 ? 2 : in_rect(a, L->tabX + 194 * k, 6, L->tabW, 38) ? 1 : 0;
        an_gfx_blit(a->fb, a->W, a->H, b, L->tabAX + 186 * k, 87 + 42 * state, L->tabW, 38, L->tabX + 194 * k, 6, 255);
    }
    an_gfx_blit(a->fb, a->W, a->H, b, 3, in_rect(a, 62, 670, 278, 40) ? 44 : 2, 278, 40, 62, 670, 255);
    for (int k = 0; k < PICKS; k++) {
        int n = pick_count(a, k);
        for (int i = 0; i < CELLS && room.scroll[k] + i < n; i++) {
            int index = room.scroll[k] + i, x = CELL_X + CELL_DX * i, y = PICK_Y[k];
            bool sel = index == (k == PICK_CHAR ? room.ch : room.pick[k]), hover = in_rect(a, x, y, CELL, CELL);
            int row = sel ? 2 : hover ? 1 : 0;
            an_gfx_blit(a->fb, a->W, a->H, b, cell_icon_x(k, pick_icon(a, k, index)), PICK_ATLAS_Y[k] + 50 * row, CELL, CELL, x, y, 255);
            if (sel && a->game == GAME_MGSW) an_gfx_blit(a->fb, a->W, a->H, b, 4, 1272, 44, 44, x + 2, y + 2, 255);
        }
        if (n > CELLS) an_gfx_blit(a->fb, a->W, a->H, b, 881, in_rect(a, ARROW_X, PICK_Y[k] + 8, ARROW, ARROW) ? 246 : 212, ARROW, ARROW, ARROW_X, PICK_Y[k] + 8, 255);
    }
    for (int k = 0; k < SWITCHES; k++) {
        const Switch *w = &L->sw[k];
        if (w->y < 0) continue;
        int on = room.sw[k] ? 2 : in_rect(a, w->onX, w->y, w->w, w->h) ? 1 : 0, off = !room.sw[k] ? 2 : in_rect(a, w->offX, w->y, w->w, w->h) ? 1 : 0;
        an_gfx_blit(a->fb, a->W, a->H, b, w->onAX, w->ay + w->rowDY * on, w->w, w->h, w->onX, w->y, 255);
        an_gfx_blit(a->fb, a->W, a->H, b, w->offAX, w->ay + w->rowDY * off, w->w, w->h, w->offX, w->y, 255);
    }
    for (int i = 0; i < SLIDERS; i++) {
        int kx = TRACK_X + room.px[i];
        int ky = room.drag == i ? 280 : in_rect(a, kx, SLIDER_Y[i], KNOB_W, KNOB_H) ? 246 : 212;
        an_gfx_blit(a->fb, a->W, a->H, b, 826, ky, KNOB_W, KNOB_H, kx, SLIDER_Y[i], 255);
        draw_value(a, b, L, i);
    }
    for (int k = 0; k < 3; k++) {
        int x = L->buttonX + 106 * k;
        an_gfx_blit(a->fb, a->W, a->H, b, L->buttonAX + 98 * k, in_rect(a, x, 591, L->buttonW, 52) ? 269 : 213, L->buttonW, 52, x, 591, 255);
    }
    bool hs = in_rect(a, 969, 615, 70, 56), hf = in_rect(a, 1068, 616, 73, 55), hr = in_rect(a, 1166, 612, 63, 63);
    if (hs) an_gfx_blit(a->fb, a->W, a->H, b, 590, 279, 76, 62, 966, 612, 255);
    else an_gfx_blit(a->fb, a->W, a->H, b, 593, 216, 70, 56, 969, 615, 255);
    if (hf) an_gfx_blit(a->fb, a->W, a->H, b, 671, 280, 79, 61, 1065, 613, 255);
    else an_gfx_blit(a->fb, a->W, a->H, b, 674, 217, 73, 55, 1068, 616, 255);
    if (hr) an_gfx_blit(a->fb, a->W, a->H, b, 753, 288, 69, 69, 1163, 609, 255);
    else an_gfx_blit(a->fb, a->W, a->H, b, 756, 217, 63, 63, 1166, 612, 255);
    int h = hover_help(a);
    int code = h >= 0 ? L->help[h] : HELP_NONE;
    if (h >= H_SWITCH && h < H_SWITCH + SWITCHES) {
        const Switch *w = &L->sw[h - H_SWITCH];
        int on = L->onHelp[h - H_SWITCH], off = L->offHelp[h - H_SWITCH];
        if (on != HELP_NONE && in_rect(a, w->onX, w->y, w->w, w->h)) code = on;
        if (off != HELP_NONE && in_rect(a, w->offX, w->y, w->w, w->h)) code = off;
    }
    if (code != HELP_NONE) an_gfx_blit(a->fb, a->W, a->H, b, 6 + 314 * (code / 100), 366 + 26 * (code % 100), 300, 20, 86, 82, 255);
}

bool an_room_input(AN *a) {
    const RoomLayout *L = &ROOM[a->game];
    if (room.hidden) {
        if (a->rclicked) room.hidden = false;
        a->clicked = a->rclicked = false;
        return true;
    }
    if (room.drag >= 0) {
        room.px[room.drag] = clampi(a->mx - TRACK_X - KNOB_W / 2, 0, TRACK_W);
        apply(a);
        if (!a->down) room.drag = -1;
        a->clicked = false;
        return true;
    }
    if (a->down)
        for (int i = 0; i < SLIDERS; i++)
            if (in_rect(a, TRACK_X - 10, SLIDER_Y[i], TRACK_W + KNOB_W + 20, KNOB_H)) {
                room.drag = i;
                return true;
            }
    if (!a->clicked) return false;
    for (int k = 0; k < PICKS; k++) {
        int n = pick_count(a, k);
        for (int i = 0; i < CELLS && room.scroll[k] + i < n; i++)
            if (in_rect(a, CELL_X + CELL_DX * i, PICK_Y[k], CELL, CELL)) {
                int index = room.scroll[k] + i;
                a->clicked = false;
                if (k == PICK_CHAR) select_char(a, index);
                else {
                    room.pick[k] = index;
                    if (k == PICK_EXPR) load_expression(a);
                    apply(a);
                }
                return true;
            }
        if (n > CELLS && in_rect(a, ARROW_X, PICK_Y[k] + 8, ARROW, ARROW)) {
            room.scroll[k] = room.scroll[k] + CELLS < n ? room.scroll[k] + 1 : 0;
            a->clicked = false;
            return true;
        }
    }
    for (int k = 0; k < SWITCHES; k++) {
        const Switch *w = &L->sw[k];
        if (w->y < 0) continue;
        if (in_rect(a, w->onX, w->y, w->w, w->h) || in_rect(a, w->offX, w->y, w->w, w->h)) {
            room.sw[k] = in_rect(a, w->onX, w->y, w->w, w->h);
            room.talkStart = a->now;
            apply(a);
            a->clicked = false;
            return true;
        }
    }
    for (int k = 0; k < 3; k++)
        if (in_rect(a, L->buttonX + 106 * k, 591, L->buttonW, 52)) {
            a->clicked = false;
            if (k == 0) room_save(a);
            else if (k == 1) room_load(a);
            else room.hidden = true;
            return true;
        }
    if (in_rect(a, 969, 615, 70, 56)) {
        a->clicked = false;
        bool hidden = room.hidden;
        room.hidden = true;
        an_gfx_render(a);
        an_capture_shot(a);
        room.hidden = hidden;
        screenshot(a);
        return true;
    }
    if (in_rect(a, 1068, 616, 73, 55)) {
        a->clicked = false;
        char *dir = an_fmt("%s/SaveGraphic", a->saveDir);
        an_mkdirs(dir);
        snprintf(a->page, sizeof a->page, "%s", dir);
        free(dir);
        return true;
    }
    if (in_rect(a, 1166, 612, 63, 63)) {
        a->clicked = false;
        reset_sliders(a);
        apply(a);
        return true;
    }
    return false;
}

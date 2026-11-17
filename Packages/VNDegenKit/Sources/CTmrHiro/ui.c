#include "th.h"

enum { MSG_QUIT = 1, MSG_TITLE, MSG_OVERWRITE, MSG_LOAD, MSG_DELETE, MSG_QSAVED };

enum {
    BAR_CLOSE, BAR_AUTO, BAR_SKIP, BAR_JUMP, BAR_RETURN, BAR_BACKLOG, BAR_SYSTEM, BAR_SYSTEM2, BAR_VOICE, BAR_SAVE,
    BAR_LOAD, BAR_QSAVE, BAR_QLOAD, BAR_MENU, BAR_TITLE, BAR_EXIT, BAR_SCENE_BACK, BAR_COUNT
};

static const char *barKeys[BAR_COUNT] = {"CLOSE", "AUTO", "SKIP", "JUMP", "RETURN", "BACKLOG", "SYSTEM", "SYSTEM2", "VOICE",
                                         "SAVE", "LOAD", "QSAVE", "QLOAD", "MENU", "TITLE", "EXIT", "SCENE_BACK"};

typedef struct BarItem {
    bool on;
    int bar, x, y, sx, sy, w, h;
} BarItem;

typedef struct SysPart {
    int page;
    char id[48];
    int type;
    int x, y, sx, sy, w, h, count, cols, rows, dx, dy, sdx, sdy, ex[4];
} SysPart;

enum { SW_GROUP, SW_SLIDE, SW_ONE, SW_PUSH };

struct UI {
    BarItem bars[BAR_COUNT];
    int barShown;
    int barHover;
    int pressed;
    int under;
    int msg;
    int msgHover;
    int msgSlot;
    u32 toastUntil;
    int page;
    int hover;
    int sysPage;
    SysPart parts[96];
    int partCount;
    int values[96];
    int dragPart, dragIndex;
    int backTop;
    Image *thumbs[10];
    int thumbPage;
    char slotText[10][3][96];
};

static bool inside(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && y >= ry && x < rx + rw && y < ry + rh; }

static UI *ui_get(TH *t) {
    if (t->ui) return t->ui;
    UI *u = t->ui = th_alloc(sizeof *u);
    u->barHover = -1;
    u->hover = -1;
    u->msgHover = -1;
    u->dragPart = -1;
    u->thumbPage = -1;
    for (int i = 0; i < BAR_COUNT; i++) {
        char key[32];
        snprintf(key, sizeof key, "TWB_%s", barKeys[i]);
        int v[8] = {0};
        if (ini_ints(&t->sys, "MESSAGEWINDOW", key, v, 8) >= 8) u->bars[i] = (BarItem){v[0] != 0, v[1], v[2], v[3], v[4], v[5], v[6], v[7]};
    }
    u->bars[BAR_VOICE].on = false;
    const IniSection *s = ini_section(&t->sys, "SYSTEM");
    for (int i = 0; s && i < s->count && u->partCount < 96; i++) {
        const IniLine *l = &s->lines[i];
        if (strncmp(l->key, "SYSTEM_PARTS_", 13)) continue;
        int page = atoi(l->key + 13) / 100;
        char tmp[256];
        snprintf(tmp, sizeof tmp, "%s", l->value);
        char *f[24];
        int n = 0;
        for (char *p = strtok(tmp, ","); p && n < 24; p = strtok(NULL, ",")) f[n++] = p;
        if (n < 16 || (atoi(f[0]) == 0 && page < 98)) continue;
        SysPart *p = &u->parts[u->partCount];
        p->page = page;
        snprintf(p->id, sizeof p->id, "%s", f[1]);
        p->type = !strcmp(f[2], "SW_SLIDE") ? SW_SLIDE : !strcmp(f[2], "SW_ONE") ? SW_ONE : !strcmp(f[2], "SW_PUSH") ? SW_PUSH : SW_GROUP;
        int *v = &p->x;
        for (int k = 0; k < 13; k++) v[k] = atoi(f[3 + k]);
        for (int k = 0; k < 4; k++) p->ex[k] = 16 + k < n ? atoi(f[16 + k]) : 0;
        u->partCount++;
    }
    for (int i = 0; i < u->partCount; i++) {
        SysPart *p = &u->parts[i];
        if (!strcmp(p->id, "S_TEXTSPEED")) u->values[i] = t->textSpeed;
        else if (!strcmp(p->id, "S_AUTOSPEED")) u->values[i] = t->autoSpeed;
        else if (!strcmp(p->id, "S_WINDOW_BASE_DRAW")) u->values[i] = t->winBaseDraw;
        else if (!strcmp(p->id, "S_GRAPHIC")) u->values[i] = ini_int(&t->sys, "SYSTEM", "SYS_DEF_GRAPHIC", 1);
        else if (!strcmp(p->id, "S_RCLICK")) u->values[i] = ini_int(&t->sys, "SYSTEM", "SYS_DEF_R_CLICK", 1);
        else if (!strcmp(p->id, "S_BWHEEL")) u->values[i] = ini_int(&t->sys, "SYSTEM", "SYS_DEF_BWHEEL", 1);
        else if (!strcmp(p->id, "S_READ_TEXT_COLOR")) u->values[i] = ini_int(&t->sys, "SYSTEM", "SYS_DEF_READ_TEXT_COLOR", 1);
        else if (!strcmp(p->id, "S_VULGAR_WORD")) u->values[i] = ini_int(&t->sys, "SYSTEM", "SYS_DEF_VULGAR_WORD", 0);
        else if (!strcmp(p->id, "S_SKIP")) u->values[i] = ini_int(&t->sys, "SYSTEM", "SYS_DEF_SKIP", 0);
        else if (!strcmp(p->id, "S_AUTO_VOICE_CUT")) u->values[i] = ini_int(&t->sys, "SYSTEM", "SYS_DEF_AUTO_VOICE_CUT", 1);
        else if (!strcmp(p->id, "S_VOICE_CUT")) u->values[i] = ini_int(&t->sys, "SYSTEM", "SYS_DEF_VOICE_CUT", 1);
        else if (!strcmp(p->id, "S_FINISH_EFFECT")) u->values[i] = ini_int(&t->sys, "SYSTEM", "SYS_DEF_FINISH_EFFECT", 1);
        else if (!strcmp(p->id, "S_SCREEN")) u->values[i] = 0;
        else if (!strcmp(p->id, "S_WS_MODE")) u->values[i] = 2;
        else if (!strcmp(p->id, "S_NS_MODE")) u->values[i] = -1;
        else if (!strcmp(p->id, "S_WINDOW_STAT")) u->values[i] = (int)(t->winStat >> 24 | (t->winStat >> 16 & 0xff) << 8 | (t->winStat >> 8 & 0xff) << 16 | (t->winStat & 0xff) << 24);
        else if (!strcmp(p->id, "S_SOUND_VOLUME")) u->values[i] = t->sysVol[0] * 255 / 100 | (t->sysVol[1] * 255 / 100) << 8 | (t->sysVol[2] * 255 / 100) << 16 | (t->sysVol[3] * 255 / 100) << 24;
        else if (!strcmp(p->id, "S_MES_CONFIRM")) u->values[i] = 0xff;
        else if (!strcmp(p->id, "S_SOUND")) u->values[i] = 0xff;
        else if (!strcmp(p->id, "S_ECSTACY_NAVI")) u->values[i] = 1;
    }
    return u;
}

static int part_index(UI *u, const char *id) {
    for (int i = 0; i < u->partCount; i++)
        if (!strcmp(u->parts[i].id, id)) return i;
    return -1;
}

static bool confirm_enabled(TH *t, int which) {
    UI *u = ui_get(t);
    int i = part_index(u, "S_MES_CONFIRM");
    return i < 0 || (u->values[i] >> which & 1);
}

void ui_open(TH *t, int mode) {
    UI *u = ui_get(t);
    if (mode == UI_SAVE || mode == UI_LOAD) {
        u->thumbPage = -1;
        u->hover = -1;
    }
    if (mode == UI_BACKLOG) u->backTop = t->backlogCount > 5 ? t->backlogCount - 5 : 0;
    t->uiMode = mode;
    t->dirty = true;
}

void ui_close(TH *t) {
    UI *u = ui_get(t);
    if (t->uiMode == UI_CONFIRM && u->under != UI_NONE) t->uiMode = u->under;
    else t->uiMode = UI_NONE;
    u->under = UI_NONE;
    u->hover = -1;
    t->dirty = true;
}

static void open_msg(TH *t, int msg, int slot) {
    UI *u = ui_get(t);
    u->under = t->uiMode;
    u->msg = msg;
    u->msgSlot = slot;
    u->msgHover = -1;
    t->uiMode = UI_CONFIRM;
    t->dirty = true;
}

static void go_title(TH *t) {
    snd_stop(t, CH_BGM, 0);
    for (int c = 0; c < CH_COUNT; c++) snd_stop(t, c, 0);
    t->uiMode = UI_NONE;
    t->wait = W_NONE;
    t->winVisible = false;
    t->autoMode = t->skipMode = false;
    gfx_set_bg(t, "bl", 0);
    gfx_finish_transition(t);
    t->navi = NULL;
    vm_start(t, "start", "TITLE");
}

static void do_load(TH *t, int slot) {
    if (load_slot(t, slot)) {
        t->uiMode = UI_NONE;
        t->autoMode = t->skipMode = false;
    }
    t->dirty = true;
}

static void quick_save(TH *t) {
    UI *u = ui_get(t);
    if (save_slot(t, 999)) u->toastUntil = t->now + 1200;
}

static bool bar_visible(TH *t, int i) {
    if (i == BAR_VOICE) return false;
    if (i == BAR_SCENE_BACK) return t->replayDepth > 0;
    if (t->replayDepth > 0 && (i == BAR_SAVE || i == BAR_LOAD || i == BAR_QSAVE || i == BAR_QLOAD || i == BAR_TITLE)) return false;
    return true;
}

static void end_replay(TH *t) {
    for (int c = 0; c < CH_COUNT; c++) snd_stop(t, c, 0);
    t->sp = t->replayDepth - 1;
    t->replayDepth = 0;
    t->wait = W_NONE;
    t->winVisible = false;
    t->navi = NULL;
    gfx_set_bg(t, "bl", 0);
    gfx_finish_transition(t);
    vm_goto(t, t->replayScene, NULL);
    t->pc = t->galLoopPc;
}

static void bar_action(TH *t, int item) {
    UI *u = ui_get(t);
    switch (item) {
    case BAR_CLOSE: t->winHidden = true; break;
    case BAR_AUTO: t->autoMode = !t->autoMode; t->skipMode = false; t->autoAt = 0; break;
    case BAR_SKIP: t->skipMode = !t->skipMode; t->autoMode = false; break;
    case BAR_BACKLOG: ui_open(t, UI_BACKLOG); break;
    case BAR_SYSTEM:
    case BAR_SYSTEM2: ui_open(t, UI_SYSTEM); break;
    case BAR_SAVE: ui_open(t, UI_SAVE); break;
    case BAR_LOAD: ui_open(t, UI_LOAD); break;
    case BAR_QSAVE: quick_save(t); break;
    case BAR_QLOAD:
        if (!save_exists(t, 999)) break;
        if (confirm_enabled(t, 3)) open_msg(t, MSG_LOAD, 999);
        else do_load(t, 999);
        break;
    case BAR_TITLE: open_msg(t, MSG_TITLE, 0); break;
    case BAR_EXIT: open_msg(t, MSG_QUIT, 0); break;
    case BAR_SCENE_BACK: end_replay(t); break;
    default: break;
    }
    (void)u;
}

static bool bars_allowed(TH *t) { return t->wait == W_TEXT && !t->menuBlocked && !t->winHidden && t->uiMode == UI_NONE; }

bool ui_bar_update(TH *t) {
    UI *u = ui_get(t);
    int shown = 0;
    if (bars_allowed(t)) {
        if (t->my < 64) shown = 1;
        else if (t->my >= t->H - 64) shown = 2;
    }
    int hover = -1;
    if (shown)
        for (int i = 0; i < BAR_COUNT; i++) {
            BarItem *b = &u->bars[i];
            if (!b->on || b->bar != shown || !bar_visible(t, i)) continue;
            int by = shown == 1 ? 0 : t->H - 64;
            if (inside(t->mx, t->my, b->x, by + b->y, b->w, b->h)) hover = i;
        }
    if (shown != u->barShown || hover != u->barHover) t->dirty = true;
    u->barShown = shown;
    u->barHover = hover;
    if (u->toastUntil && t->now >= u->toastUntil) {
        u->toastUntil = 0;
        t->dirty = true;
    }
    if (!shown) return false;
    if (t->click && hover >= 0) {
        bar_action(t, hover);
        t->dirty = true;
        return true;
    }
    if (t->click) return true;
    return false;
}

void ui_bar_draw(TH *t) {
    UI *u = ui_get(t);
    if (u->toastUntil && t->now < u->toastUntil) {
        Image *m = image_get(t, "sys_message_base_06");
        int mx = ini_int(&t->sys, "MESSAGE", "MES_POS_X", 450), my = ini_int(&t->sys, "MESSAGE", "MES_POS_Y", 267);
        if (m) blit(t->screen, t->W, t->H, m, mx, my, 255);
    }
    if (!u->barShown || !bars_allowed(t)) return;
    int by = u->barShown == 1 ? 0 : t->H - 64;
    Image *bg = image_get(t, u->barShown == 1 ? "sys_window_menu_a" : "sys_window_menu_b");
    if (!bg) bg = image_get(t, "sys_window_menu");
    if (bg) blit(t->screen, t->W, t->H, bg, 0, by, 255);
    Image *sheet = image_get(t, "sys_window_button");
    if (!sheet) return;
    for (int i = 0; i < BAR_COUNT; i++) {
        BarItem *b = &u->bars[i];
        if (!b->on || b->bar != u->barShown || !bar_visible(t, i)) continue;
        int row = i == u->barHover ? 1 : 0;
        if ((i == BAR_AUTO && t->autoMode) || (i == BAR_SKIP && t->skipMode)) row = 2;
        blit_part(t->screen, t->W, t->H, sheet, b->sx, b->sy + row * b->h, b->w, b->h, b->x, by + b->y, 255);
    }
}

static int save_number(UI *u, int i) { return u->page == 10 ? 101 + i : u->page * 10 + i + 1; }

static void slot_geom(TH *t, const char *sec, int i, int *x, int *y, int *w, int *h) {
    char key[32];
    snprintf(key, sizeof key, "%s_BASE_POS", sec);
    int v[17] = {0};
    ini_ints(&t->sys, sec, key, v, 17);
    int rows = v[7] > 0 ? v[7] : 5;
    int col = i / rows, row = i % rows;
    *x = v[0] + col * v[13];
    *y = v[1] + row * v[10];
    *w = v[4];
    *h = v[5];
}

static void refresh_slots(TH *t, const char *sec) {
    UI *u = ui_get(t);
    if (u->thumbPage == u->page) return;
    for (int i = 0; i < 10; i++) {
        image_free(u->thumbs[i]);
        u->thumbs[i] = NULL;
        memset(u->slotText[i], 0, sizeof u->slotText[i]);
        int n = save_number(u, i);
        char *bmp = th_fmt("%s/savedata%03d.bmp", t->saveDir, n);
        size_t size;
        u8 *d = th_read_file(bmp, &size);
        free(bmp);
        if (d && size > 54 && d[0] == 'B' && d[1] == 'M') {
            int w = (int)rd32(d + 18), hh = (int)rd32(d + 22), bpp = rd16(d + 28);
            u32 off = rd32(d + 10);
            int ah = hh < 0 ? -hh : hh;
            size_t stride = ((size_t)w * (size_t)(bpp / 8) + 3) & ~(size_t)3;
            if ((bpp == 24 || bpp == 32) && w > 0 && ah > 0 && off + stride * (size_t)ah <= size) {
                Image *im = image_new(w, ah, 0, 0, w, ah);
                for (int y = 0; y < ah; y++) {
                    const u8 *row = d + off + stride * (size_t)(hh < 0 ? y : ah - 1 - y);
                    for (int x = 0; x < w; x++) {
                        const u8 *p = row + x * (bpp / 8);
                        im->px[(size_t)y * w + x] = (u32)p[2] | (u32)p[1] << 8 | (u32)p[0] << 16 | 0xff000000u;
                    }
                }
                im->opaque = true;
                u->thumbs[i] = im;
            }
        }
        free(d);
        char *path = th_fmt("%s/savedata.%03d", t->saveDir, n);
        u8 *s = th_read_file(path, &size);
        free(path);
        if (s && size >= 16 + 64) {
            snprintf(u->slotText[i][0], sizeof u->slotText[i][0], "%04d/%02d/%02d %02d:%02d", rd16(s), rd16(s + 2), rd16(s + 6),
                     rd16(s + 8), rd16(s + 10));
            snprintf(u->slotText[i][1], sizeof u->slotText[i][1], "%.95s", (const char *)s + 16);
        }
        free(s);
    }
    u->thumbPage = u->page;
    (void)sec;
}

static void draw_saveload(TH *t, bool save) {
    UI *u = ui_get(t);
    const char *sec = save ? "SAVE" : "LOAD";
    Image *base = image_get(t, save ? "sys_save_base" : "sys_load_base");
    Image *sheet = image_get(t, save ? "sys_save_button" : "sys_load_button");
    if (base) blit(t->screen, t->W, t->H, base, 0, 0, 255);
    if (!sheet) return;
    refresh_slots(t, sec);
    char key[32];
    int v[17] = {0};
    snprintf(key, sizeof key, "%s_BASE_POS", sec);
    ini_ints(&t->sys, sec, key, v, 17);
    int tp[3] = {0}, th[2] = {0}, nw[6] = {0};
    snprintf(key, sizeof key, "%s_B_TEXT_POS", sec);
    ini_ints(&t->sys, sec, key, tp, 3);
    snprintf(key, sizeof key, "%s_B_THUM_POS", sec);
    ini_ints(&t->sys, sec, key, th, 2);
    snprintf(key, sizeof key, "%s_B_NEW_POS", sec);
    ini_ints(&t->sys, sec, key, nw, 6);
    int newest = -1;
    for (int i = 0; i < 10; i++) {
        int x, y, w, h;
        slot_geom(t, sec, i, &x, &y, &w, &h);
        int row = u->hover == i ? 1 : 0;
        blit_part(t->screen, t->W, t->H, sheet, v[2], v[3] + row * h, w, h, x, y, 255);
        if (u->thumbs[i]) blit(t->screen, t->W, t->H, u->thumbs[i], x + th[0], y + th[1], 255);
        int size = tp[2] > 0 ? tp[2] / 2 + 2 : 12;
        char label[32];
        snprintf(label, sizeof label, "No.%03d", u->page == 10 ? i + 1 : u->page * 10 + i + 1);
        text_draw(t, t->screen, t->W, t->H, label, x + tp[0], y + tp[1] + 4, size, 0xffe0e0e0u, false, -1);
        text_draw(t, t->screen, t->W, t->H, u->slotText[i][0][0] ? u->slotText[i][0] : "----/--/-- --:--", x + tp[0] + 10,
                  y + tp[1] + 24, size, 0xffe0e0e0u, false, -1);
        if (u->slotText[i][1][0]) text_draw(t, t->screen, t->W, t->H, u->slotText[i][1], x + tp[0] - 40, y + tp[1] + 50, 16, 0xffffffffu, false, -1);
        (void)newest;
    }
    int pp[17] = {0}, ap[6] = {0}, dp[6] = {0}, rp[6] = {0};
    snprintf(key, sizeof key, "%s_PAGE_POS", sec);
    ini_ints(&t->sys, sec, key, pp, 17);
    snprintf(key, sizeof key, "%s_AUTO_POS", sec);
    ini_ints(&t->sys, sec, key, ap, 6);
    snprintf(key, sizeof key, "%s_DEL_POS", sec);
    ini_ints(&t->sys, sec, key, dp, 6);
    snprintf(key, sizeof key, "%s_RETURN_POS", sec);
    ini_ints(&t->sys, sec, key, rp, 6);
    for (int p = 0; p < pp[6]; p++) {
        int row = u->page == p ? 1 : u->hover == 20 + p ? 1 : 0;
        blit_part(t->screen, t->W, t->H, sheet, pp[2] + p * pp[9], pp[3] + row * pp[5], pp[4], pp[5], pp[0] + p * pp[9], pp[1], 255);
    }
    if (ap[4]) blit_part(t->screen, t->W, t->H, sheet, ap[2], ap[3] + (u->page == 10 || u->hover == 30 ? ap[5] : 0), ap[4], ap[5], ap[0], ap[1], 255);
    if (dp[4]) blit_part(t->screen, t->W, t->H, sheet, dp[2], dp[3] + (u->hover == 31 ? dp[5] : 0), dp[4], dp[5], dp[0], dp[1], 255);
    if (rp[4]) blit_part(t->screen, t->W, t->H, sheet, rp[2], rp[3] + (u->hover == 32 ? rp[5] : 0), rp[4], rp[5], rp[0], rp[1], 255);
}

static void update_saveload(TH *t, bool save) {
    UI *u = ui_get(t);
    const char *sec = save ? "SAVE" : "LOAD";
    char key[32];
    int pp[17] = {0}, ap[6] = {0}, dp[6] = {0}, rp[6] = {0};
    snprintf(key, sizeof key, "%s_PAGE_POS", sec);
    ini_ints(&t->sys, sec, key, pp, 17);
    snprintf(key, sizeof key, "%s_AUTO_POS", sec);
    ini_ints(&t->sys, sec, key, ap, 6);
    snprintf(key, sizeof key, "%s_DEL_POS", sec);
    ini_ints(&t->sys, sec, key, dp, 6);
    snprintf(key, sizeof key, "%s_RETURN_POS", sec);
    ini_ints(&t->sys, sec, key, rp, 6);
    int hover = -1;
    for (int i = 0; i < 10; i++) {
        int x, y, w, h;
        slot_geom(t, sec, i, &x, &y, &w, &h);
        if (inside(t->mx, t->my, x, y, w, h)) hover = i;
    }
    for (int p = 0; p < pp[6]; p++)
        if (inside(t->mx, t->my, pp[0] + p * pp[9], pp[1], pp[4], pp[5])) hover = 20 + p;
    if (ap[4] && inside(t->mx, t->my, ap[0], ap[1], ap[4], ap[5])) hover = 30;
    if (dp[4] && inside(t->mx, t->my, dp[0], dp[1], dp[4], dp[5])) hover = 31;
    if (rp[4] && inside(t->mx, t->my, rp[0], rp[1], rp[4], rp[5])) hover = 32;
    if (hover != u->hover) t->dirty = true;
    u->hover = hover;
    if (t->rclick || t->keyPress[0x1b]) {
        ui_close(t);
        return;
    }
    if (!t->click) return;
    if (hover >= 0 && hover < 10) {
        int n = save_number(u, hover);
        bool exists = save_exists(t, n);
        if (save) {
            if (u->page == 10) return;
            if (exists && confirm_enabled(t, 0)) open_msg(t, MSG_OVERWRITE, n);
            else {
                save_slot(t, n);
                u->thumbPage = -1;
            }
        } else if (exists) {
            if (confirm_enabled(t, 1)) open_msg(t, MSG_LOAD, n);
            else do_load(t, n);
        }
    } else if (hover >= 20 && hover < 30) {
        u->page = hover - 20;
    } else if (hover == 30) {
        u->page = 10;
    } else if (hover == 32) {
        ui_close(t);
    }
    t->dirty = true;
}

static void msg_buttons(TH *t, int i, int *x, int *y, int *w, int *h) {
    int mx = ini_int(&t->sys, "MESSAGE", "MES_POS_X", 450), my = ini_int(&t->sys, "MESSAGE", "MES_POS_Y", 267);
    *x = mx + ini_int(&t->sys, "MESSAGE", "MES_B_POS_X", 69) + i * ini_int(&t->sys, "MESSAGE", "MES_B_NEXT_X", 134);
    *y = my + ini_int(&t->sys, "MESSAGE", "MES_B_POS_Y", 99) + i * ini_int(&t->sys, "MESSAGE", "MES_B_NEXT_Y", 0);
    *w = ini_int(&t->sys, "MESSAGE", "MES_B_WIDTH", 108);
    *h = ini_int(&t->sys, "MESSAGE", "MES_B_HEIGHT", 37);
}

static void draw_msg(TH *t) {
    UI *u = ui_get(t);
    char name[32];
    snprintf(name, sizeof name, "sys_message_base_%02d", u->msg);
    Image *b = image_get(t, name);
    int mx = ini_int(&t->sys, "MESSAGE", "MES_POS_X", 450), my = ini_int(&t->sys, "MESSAGE", "MES_POS_Y", 267);
    if (b) blit(t->screen, t->W, t->H, b, mx, my, 255);
    Image *btn = image_get(t, "sys_message_button");
    if (!btn) return;
    for (int i = 0; i < 2; i++) {
        int x, y, w, h;
        msg_buttons(t, i, &x, &y, &w, &h);
        int sy = btn->y + (u->msgHover == i ? h : 0);
        blit_part(t->screen, t->W, t->H, btn, i * w, sy, w, h, x, y, 255);
    }
}

static void update_msg(TH *t) {
    UI *u = ui_get(t);
    int hover = -1;
    for (int i = 0; i < 2; i++) {
        int x, y, w, h;
        msg_buttons(t, i, &x, &y, &w, &h);
        if (inside(t->mx, t->my, x, y, w, h)) hover = i;
    }
    if (hover != u->msgHover) t->dirty = true;
    u->msgHover = hover;
    if (t->rclick || t->keyPress[0x1b] || (t->click && hover == 1)) {
        ui_close(t);
        return;
    }
    if (!(t->click && hover == 0) && !t->keyPress[0x0d]) return;
    int msg = u->msg, slot = u->msgSlot;
    ui_close(t);
    switch (msg) {
    case MSG_QUIT: t->quit = true; break;
    case MSG_TITLE: go_title(t); break;
    case MSG_OVERWRITE: save_slot(t, slot); u->thumbPage = -1; break;
    case MSG_LOAD: do_load(t, slot); break;
    case MSG_DELETE: break;
    default: break;
    }
}

static void backlog_metrics(TH *t, int *x, int *y, int *size, int *line, int *next, int *nameDy) {
    *x = ini_int(&t->sys, "BACKLOG", "BACKLOG_POS_X", 200);
    *y = ini_int(&t->sys, "BACKLOG", "BACKLOG_POS_Y", 80);
    *size = ini_int(&t->sys, "BACKLOG", "BACKLOG_FONT_SIZE", 24);
    *line = ini_int(&t->sys, "BACKLOG", "BACKLOG_NEXT_LINE", 30);
    *next = ini_int(&t->sys, "BACKLOG", "BACKLOG_NEXT_Y", 124);
    *nameDy = ini_int(&t->sys, "BACKLOG", "BACKLOG_NAME_Y", -30);
}

static void draw_backlog(TH *t) {
    UI *u = ui_get(t);
    Image *base = image_get(t, "sys_backlog_base");
    if (base) blit(t->screen, t->W, t->H, base, 0, 0, 255);
    int x, y, size, line, next, nameDy;
    backlog_metrics(t, &x, &y, &size, &line, &next, &nameDy);
    int max = ini_int(&t->sys, "BACKLOG", "BACKLOG_MAX", 5);
    Image *sheet = image_get(t, "sys_backlog_button");
    int vb[6] = {0};
    ini_ints(&t->sys, "BACKLOG", "BACKLOG_VOICE", vb, 6);
    for (int k = 0; k < max && u->backTop + k < t->backlogCount; k++) {
        BacklogEntry *e = &t->backlog[u->backTop + k];
        int ey = y + k * next + (nameDy < 0 ? -nameDy : 0);
        if (e->name) text_draw(t, t->screen, t->W, t->H, e->name, x, ey + nameDy, size, 0xffffffffu, true, -1);
        int lx = x, ly = ey;
        for (const u8 *p = (const u8 *)e->text; *p;) {
            if (*p == '\n') {
                p++;
                lx = x;
                ly += line;
                continue;
            }
            u32 cp;
            int len = th_sjis_decode(p, &cp);
            char ch[3] = {0};
            memcpy(ch, p, (size_t)len);
            int w = text_width(t, ch, size);
            if (lx + w > t->W - 160) {
                lx = x;
                ly += line;
            }
            lx = text_draw(t, t->screen, t->W, t->H, ch, lx, ly, size, 0xffffffffu, true, -1);
            p += len;
        }
        if (e->voice[0] && sheet && vb[4])
            blit_part(t->screen, t->W, t->H, sheet, vb[2], vb[3] + (u->hover == 100 + k ? vb[5] : 0), vb[4], vb[5], x + vb[0], ey + vb[1] - 8, 255);
    }
    int rp[6] = {0};
    ini_ints(&t->sys, "BACKLOG", "BACKLOG_RETURN_POS", rp, 6);
    if (sheet && rp[4]) blit_part(t->screen, t->W, t->H, sheet, rp[2], rp[3] + (u->hover == 200 ? rp[5] : 0), rp[4], rp[5], rp[0], rp[1], 255);
    int sp[8] = {0};
    ini_ints(&t->sys, "BACKLOG", "BACKLOG_SCROLL", sp, 8);
    if (sheet && sp[4]) {
        blit_part(t->screen, t->W, t->H, sheet, 0, 0 + (u->hover == 201 ? sp[5] : 0), sp[4], sp[5], sp[0], sp[1], 255);
        blit_part(t->screen, t->W, t->H, sheet, sp[4], 0 + (u->hover == 202 ? sp[5] : 0), sp[4], sp[5], sp[0], sp[1] + sp[7], 255);
    }
    int sl[8] = {0};
    ini_ints(&t->sys, "BACKLOG", "BACKLOG_SLIDE", sl, 8);
    if (sheet && sl[4] && t->backlogCount > max) {
        int range = sl[7] - sl[5];
        int pos = range * u->backTop / (t->backlogCount - max);
        blit_part(t->screen, t->W, t->H, sheet, sl[2], sl[3], sl[4], sl[5], sl[0], sl[1] + pos, 255);
    }
}

static void update_backlog(TH *t) {
    UI *u = ui_get(t);
    int max = ini_int(&t->sys, "BACKLOG", "BACKLOG_MAX", 5);
    int maxTop = t->backlogCount > max ? t->backlogCount - max : 0;
    int x, y, size, line, next, nameDy;
    backlog_metrics(t, &x, &y, &size, &line, &next, &nameDy);
    int vb[6] = {0}, rp[6] = {0}, sp[8] = {0};
    ini_ints(&t->sys, "BACKLOG", "BACKLOG_VOICE", vb, 6);
    ini_ints(&t->sys, "BACKLOG", "BACKLOG_RETURN_POS", rp, 6);
    ini_ints(&t->sys, "BACKLOG", "BACKLOG_SCROLL", sp, 8);
    int hover = -1;
    for (int k = 0; k < max && u->backTop + k < t->backlogCount; k++) {
        int ey = y + k * next + (nameDy < 0 ? -nameDy : 0);
        if (t->backlog[u->backTop + k].voice[0] && inside(t->mx, t->my, x + vb[0], ey + vb[1] - 8, vb[4], vb[5])) hover = 100 + k;
    }
    if (inside(t->mx, t->my, rp[0], rp[1], rp[4], rp[5])) hover = 200;
    if (sp[4] && inside(t->mx, t->my, sp[0], sp[1], sp[4], sp[5])) hover = 201;
    if (sp[4] && inside(t->mx, t->my, sp[0], sp[1] + sp[7], sp[4], sp[5])) hover = 202;
    if (hover != u->hover) t->dirty = true;
    u->hover = hover;
    int top = u->backTop;
    if (t->wheel > 0 || t->keyPress[0x26]) top--;
    if (t->wheel < 0 || t->keyPress[0x28]) {
        if (top >= maxTop) {
            ui_close(t);
            return;
        }
        top++;
    }
    if (t->click && hover == 201) top--;
    if (t->click && hover == 202) top++;
    top = clampi(top, 0, maxTop);
    if (top != u->backTop) {
        u->backTop = top;
        t->dirty = true;
    }
    if (t->rclick || t->keyPress[0x1b] || (t->click && hover == 200)) {
        ui_close(t);
        return;
    }
    if (t->click && hover >= 100 && hover < 200) snd_play(t, CH_VOICE, PAC_VOICE, t->backlog[u->backTop + hover - 100].voice, false, 0);
}

static void part_item(const SysPart *p, int i, int *x, int *y, int *sx, int *sy) {
    int cols = p->cols > 0 ? p->cols : 1;
    int c = i % cols, r = i / cols;
    *x = p->x + c * p->dx + r * p->ex[0];
    *y = p->y + c * p->dy + r * p->ex[1];
    *sx = p->sx + c * p->sdx + r * p->ex[2];
    *sy = p->sy + c * p->sdy + r * p->ex[3];
}

static bool item_selected(UI *u, int pi, int i) {
    const SysPart *p = &u->parts[pi];
    int v = u->values[pi];
    if (p->type == SW_ONE) return false;
    if (p->rows > 1 && p->cols == 2) return ((v >> (i / 2)) & 1) == (i % 2);
    return v == i;
}

static void draw_system(TH *t) {
    UI *u = ui_get(t);
    char name[32];
    snprintf(name, sizeof name, "sys_system_base_%02d", u->sysPage + 1);
    Image *base = image_get(t, name);
    if (base) blit(t->screen, t->W, t->H, base, 0, 0, 255);
    Image *nav = image_get(t, "sys_system_button_01");
    Image *sheet = image_get(t, "sys_system_button_02");
    for (int pi = 0; pi < u->partCount; pi++) {
        SysPart *p = &u->parts[pi];
        if (p->page == 98) continue;
        bool navPart = p->page == 99;
        if (!navPart && p->page != u->sysPage + 1) continue;
        Image *src = navPart ? nav : sheet;
        if (!src) continue;
        if (p->type == SW_SLIDE) {
            for (int i = 0; i < p->count; i++) {
                int x = p->x + p->ex[0] * 0 + i * p->dx, y = p->y + i * p->dy;
                int range = p->ex[2] ? p->ex[2] : 336;
                int val = (u->values[pi] >> (i * 8)) & 0xff;
                int kx = x + range * val / 255;
                blit_part(t->screen, t->W, t->H, sheet, p->sx, p->sy + (u->dragPart == pi && u->dragIndex == i ? p->h : 0), 12, p->h, kx - 6, y, 255);
            }
            continue;
        }
        if (p->type == SW_PUSH) continue;
        for (int i = 0; i < p->count; i++) {
            int x, y, sx, sy;
            part_item(p, i, &x, &y, &sx, &sy);
            int row = 0;
            if (navPart && !strcmp(p->id, "S_PAGE") && i == u->sysPage) row = 2;
            else if (!navPart && item_selected(u, pi, i)) row = 2;
            if (u->hover == pi * 32 + i) row = row ? row : 1;
            blit_part(t->screen, t->W, t->H, src, sx, sy + row * p->h, p->w, p->h, x, y, 255);
        }
    }
}

static void apply_part(TH *t, int pi) {
    UI *u = ui_get(t);
    SysPart *p = &u->parts[pi];
    int v = u->values[pi];
    if (!strcmp(p->id, "S_TEXTSPEED")) t->textSpeed = v;
    else if (!strcmp(p->id, "S_AUTOSPEED")) t->autoSpeed = v;
    else if (!strcmp(p->id, "S_WINDOW_BASE_DRAW")) t->winBaseDraw = v != 0;
    else if (!strcmp(p->id, "S_WINDOW_STAT")) t->winStat = (u32)(v & 0xff) << 24 | (u32)(v >> 8 & 0xff) << 16 | (u32)(v >> 16 & 0xff) << 8 | (u32)(v >> 24 & 0xff);
    else if (!strcmp(p->id, "S_SOUND_VOLUME")) {
        for (int i = 0; i < 4; i++) t->sysVol[i] = ((v >> (i * 8)) & 0xff) * 100 / 255;
        snd_update(t);
    }
    system_settings_changed(t);
}

static void update_system(TH *t) {
    UI *u = ui_get(t);
    int hover = -1;
    for (int pi = 0; pi < u->partCount; pi++) {
        SysPart *p = &u->parts[pi];
        if (p->page == 98 || p->type == SW_SLIDE || p->type == SW_PUSH) continue;
        if (p->page != 99 && p->page != u->sysPage + 1) continue;
        for (int i = 0; i < p->count; i++) {
            int x, y, sx, sy;
            part_item(p, i, &x, &y, &sx, &sy);
            if (inside(t->mx, t->my, x, y, p->w, p->h)) hover = pi * 32 + i;
        }
    }
    if (hover != u->hover) t->dirty = true;
    u->hover = hover;
    if (t->rclick || t->keyPress[0x1b]) {
        ui_close(t);
        return;
    }
    if (t->click && hover < 0) {
        for (int pi = 0; pi < u->partCount; pi++) {
            SysPart *p = &u->parts[pi];
            if (p->type != SW_SLIDE || p->page != u->sysPage + 1) continue;
            int range = p->ex[2] ? p->ex[2] : 336;
            for (int i = 0; i < p->count; i++) {
                int y = p->y + i * p->dy;
                if (inside(t->mx, t->my, p->x - 8, y, range + 16, p->h)) {
                    u->dragPart = pi;
                    u->dragIndex = i;
                }
            }
        }
    }
    if (u->dragPart >= 0) {
        if (!t->lDown) {
            u->dragPart = -1;
            t->dirty = true;
            return;
        }
        SysPart *p = &u->parts[u->dragPart];
        int range = p->ex[2] ? p->ex[2] : 336;
        int val = clampi((t->mx - p->x) * 255 / range, 0, 255);
        int sh = u->dragIndex * 8;
        u->values[u->dragPart] = (int)(((u32)u->values[u->dragPart] & ~(0xffu << sh)) | (u32)val << sh);
        apply_part(t, u->dragPart);
        t->dirty = true;
        return;
    }
    if (!t->click || hover < 0) return;
    int pi = hover / 32, i = hover % 32;
    SysPart *p = &u->parts[pi];
    if (!strcmp(p->id, "S_PAGE")) u->sysPage = i;
    else if (!strcmp(p->id, "S_GAME")) ui_close(t);
    else if (!strcmp(p->id, "S_TITLE")) open_msg(t, MSG_TITLE, 0);
    else if (!strcmp(p->id, "S_END")) open_msg(t, MSG_QUIT, 0);
    else if (p->type == SW_GROUP) {
        if (p->rows > 1 && p->cols == 2) u->values[pi] = (u->values[pi] & ~(1 << (i / 2))) | (i % 2) << (i / 2);
        else u->values[pi] = i;
        apply_part(t, pi);
    }
    t->dirty = true;
}

void ui_update(TH *t) {
    switch (t->uiMode) {
    case UI_SAVE: update_saveload(t, true); break;
    case UI_LOAD: update_saveload(t, false); break;
    case UI_CONFIRM: update_msg(t); break;
    case UI_BACKLOG: update_backlog(t); break;
    case UI_SYSTEM: update_system(t); break;
    default: if (t->rclick) ui_close(t); break;
    }
}

void ui_draw(TH *t) {
    UI *u = ui_get(t);
    int mode = t->uiMode == UI_CONFIRM ? u->under : t->uiMode;
    switch (mode) {
    case UI_SAVE: draw_saveload(t, true); break;
    case UI_LOAD: draw_saveload(t, false); break;
    case UI_BACKLOG: draw_backlog(t); break;
    case UI_SYSTEM: draw_system(t); break;
    default: break;
    }
    if (t->uiMode == UI_CONFIRM) draw_msg(t);
}

void ui_free(TH *t) {
    UI *u = t->ui;
    if (!u) return;
    for (int i = 0; i < 10; i++) image_free(u->thumbs[i]);
    free(u);
    t->ui = NULL;
}

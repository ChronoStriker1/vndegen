#include "sa_private.h"
#include <time.h>

enum {
    AUTO_X = 35,
    AUTO_Y = 10,
    LOG_X = 46,
    TAB_X = 288,
    TAB_DX = 80,
    BAR_Y = 376,
    TAB_W = 76,
    TAB_H = 50,
    BACK_X = 1094,
    BACK_W = 140,
    ROW_X0 = 35,
    ROW_X1 = 645,
    ROW_Y = 437,
    ROW_DY = 55,
    ROW_W = 600,
    ROW_H = 50,
    ROW_TEXT_X = 17,
    ROW_TEXT_Y = 7,
    ROW_LINE = 18,
    TURN_RIGHT = 395,
    LV_X = 445,
    COUNT_RIGHT = 582,
    DETAIL_X = 310,
    DETAIL_Y = 50,
    DETAIL_SIZE = 24,
    DETAIL_INDENT = 48,
    DETAIL_LINE = 24,
    DETAIL_VALUE_RIGHT = 515,
    DETAIL_LV_X = 562,
    COUNT_X = 262,
    AUTO_SIZE = 18,
    AUTO_TEXT_X = 55,
    AUTO_TEXT_Y = 48,
    AUTO_LINE = 20,
    YES_X = 440,
    NO_X = 640,
    YES_Y = 382,
    YES_W = 200,
    YES_H = 60,
    TEXT = 15,
    HOVER_BACK = 1,
    HOVER_AUTO = 2,
    HOVER_YES = 3,
    HOVER_NO = 4,
    HOVER_TAB = 100,
    HOVER_ROW = 200,
    SLOTS_PER_PAGE = 10,
};

enum { ART_LOAD_BG, ART_SAVE_BG, ART_AUTO, ART_LOG, ART_BACK, ART_YES, ART_NO, ART_TAB0, ART_ROW0 = ART_TAB0 + 20, ART_COUNT = ART_ROW0 + 20 };

#define NO_EDGE 0xffffffffu

void sa_savescreen_init(SaveScreen *v, Vfs *vfs) {
    memset(v, 0, sizeof *v);
    static const char *const names[ART_TAB0] = {"sav_a00.agf", "sav_a01.agf", "sav_a06.agf", "sav_a07.agf", "sav_a05.agf", "pop_b00.agf", "pop_b01.agf"};
    for (int i = 0; i < ART_TAB0; i++) v->art[i] = sa_load_image(vfs, names[i]);
    char n[16];
    for (int i = 0; i < 10; i++) {
        snprintf(n, sizeof n, "sav_b%02d.agf", i);
        v->art[ART_TAB0 + i] = sa_load_image(vfs, n);
        snprintf(n, sizeof n, "sav_d%02d.agf", i);
        v->art[ART_TAB0 + 10 + i] = sa_load_image(vfs, n);
        snprintf(n, sizeof n, "sav_c%02d.agf", i);
        v->art[ART_ROW0 + i] = sa_load_image(vfs, n);
        snprintf(n, sizeof n, "sav_e%02d.agf", i);
        v->art[ART_ROW0 + 10 + i] = sa_load_image(vfs, n);
    }
    v->hover = v->confirm = -1;
}

void sa_savescreen_free(SaveScreen *v) {
    for (int i = 0; i < SAVESCREEN_ART; i++) sa_image_free(v->art[i]);
    memset(v, 0, sizeof *v);
}

static void refresh(SA *s) {
    SaveScreen *v = &s->saves;
    v->latest = -1;
    u32 newest = 0;
    for (int i = 0; i < SAVE_SLOTS + SAVE_AUTOS; i++) {
        sa_save_summary(s, i, &v->sum[i]);
        if (i < SAVE_SLOTS && v->sum[i].used && v->sum[i].time >= newest) {
            newest = v->sum[i].time;
            v->latest = i;
        }
    }
    v->autoLatest = -1;
    for (int i = SAVE_SLOTS; i < SAVE_SLOTS + SAVE_AUTOS; i++)
        if (v->sum[i].used && (v->autoLatest < 0 || v->sum[i].count > v->sum[v->autoLatest].count)) v->autoLatest = i;
    if (s->inGame) sa_save_current(s, &v->now);
    else memset(&v->now, 0, sizeof v->now);
}

void sa_savescreen_open(SA *s, bool saving) {
    SaveScreen *v = &s->saves;
    v->saving = saving;
    v->hover = v->confirm = -1;
    v->back = s->mode;
    refresh(s);
    s->mode = MODE_SAVES;
    s->dirty = true;
}

void sa_autosave(SA *s) {
    SaveScreen *v = &s->saves;
    refresh(s);
    int count = (v->autoLatest < 0 ? 0 : v->sum[v->autoLatest].count) + 1;
    sa_save_write(s, SAVE_SLOTS + count % SAVE_AUTOS, count);
}

static bool in(int x, int y, int rx, int ry, int w, int h) { return x >= rx && x < rx + w && y >= ry && y < ry + h; }

static int row_x(int i) { return i < 5 ? ROW_X0 : ROW_X1; }

static int row_y(int i) { return ROW_Y + i % 5 * ROW_DY; }

static int hit(SA *s, int x, int y) {
    SaveScreen *v = &s->saves;
    if (v->confirm >= 0) {
        if (in(x, y, YES_X, YES_Y, YES_W, YES_H)) return HOVER_YES;
        if (in(x, y, NO_X, YES_Y, YES_W, YES_H)) return HOVER_NO;
        return -1;
    }
    if (in(x, y, BACK_X, BAR_Y, BACK_W, TAB_H)) return HOVER_BACK;
    if (!v->saving && v->autoLatest >= 0 && in(x, y, AUTO_X, AUTO_Y, 252, 355)) return HOVER_AUTO;
    for (int i = 0; i < 10; i++)
        if (in(x, y, TAB_X + i * TAB_DX, BAR_Y, TAB_W, TAB_H)) return HOVER_TAB + i;
    for (int i = 0; i < SLOTS_PER_PAGE; i++)
        if (in(x, y, row_x(i), row_y(i), ROW_W, ROW_H)) return HOVER_ROW + i;
    return -1;
}

static void text(u32 *fb, const char *t, int x, int y, u32 color) { sa_text_draw(fb, SA_W, SA_H, t, -1, x, y, TEXT, color, 0x000000); }

static void right(u32 *fb, const char *t, int r, int y, u32 color) { text(fb, t, r - sa_text_width(t, -1, TEXT), y, color); }

static void stamp(const SaveSummary *m, char *out, size_t n) {
    time_t t = (time_t)m->time;
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(out, n, "%04d年%2d月%2d日 %02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
}

static void draw_row(SA *s, u32 *fb, int i) {
    SaveScreen *v = &s->saves;
    int slot = v->page * SLOTS_PER_PAGE + i, x = row_x(i), y = row_y(i);
    int frame = v->confirm == slot ? 2 : v->hover == HOVER_ROW + i ? 1 : 0;
    sa_blit_rect(fb, v->art[ART_ROW0 + (v->saving ? 10 : 0) + v->page], 0, frame * ROW_H, ROW_W, ROW_H, x, y);
    const SaveSummary *m = &v->sum[slot];
    char t[64];
    u32 color = slot == v->latest ? 0xffff00 : 0xffffff;
    snprintf(t, sizeof t, "No.%02d", slot);
    text(fb, t, x + ROW_TEXT_X, y + ROW_TEXT_Y, color);
    if (!m->used) {
        text(fb, "NoData", x + ROW_TEXT_X + 60, y + ROW_TEXT_Y, color);
        return;
    }
    stamp(m, t, sizeof t);
    text(fb, t, x + ROW_TEXT_X + 60, y + ROW_TEXT_Y, color);
    snprintf(t, sizeof t, "%dターン", m->turn);
    right(fb, t, x + TURN_RIGHT, y + ROW_TEXT_Y, color);
    snprintf(t, sizeof t, "Lv %d", m->level);
    text(fb, t, x + LV_X, y + ROW_TEXT_Y, color);
    text(fb, "街", x + ROW_TEXT_X + 30, y + ROW_TEXT_Y + ROW_LINE, color);
}

static void atext(u32 *fb, const char *t, int x, int y, u32 color) { sa_text_draw(fb, SA_W, SA_H, t, -1, x, y, AUTO_SIZE, color, 0x000000); }

static void aright(u32 *fb, const char *t, int r, int y, u32 color) { atext(fb, t, r - sa_text_width(t, -1, AUTO_SIZE), y, color); }

static void draw_summary(u32 *fb, const SaveSummary *m, int slot, int x, int y, int line) {
    char t[96];
    sa_font_set_plain(false);
    if (slot >= SAVE_SLOTS) snprintf(t, sizeof t, "NoA%d", slot - SAVE_SLOTS);
    else snprintf(t, sizeof t, "No%02d", slot);
    atext(fb, t, x, y, 0xffffff);
    if (!m->used) {
        atext(fb, "NoData", x, y + line, 0xffffff);
        return;
    }
    if (slot >= SAVE_SLOTS) {
        snprintf(t, sizeof t, "%d", m->count);
        aright(fb, t, COUNT_X, y, 0xffffff);
    }
    time_t tt = (time_t)m->time;
    struct tm tm;
    localtime_r(&tt, &tm);
    snprintf(t, sizeof t, "%d月%d日 %02d:%02d", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
    atext(fb, t, x, y + line, 0xffffff);
    snprintf(t, sizeof t, "%dターン", m->turn);
    atext(fb, t, x, y + 2 * line, 0xffffff);
    atext(fb, m->place[0], x, y + 3 * line, 0xffffff);
    atext(fb, m->place[1], x, y + 4 * line, 0xffffff);
    static const char *const labels[4] = {"住民", "食料", "資金", "資源"};
    for (int k = 0; k < 4; k++) {
        atext(fb, labels[k], x, y + (5 + k) * line, 0xffffff);
        snprintf(t, sizeof t, "%d", m->town[k]);
        aright(fb, t, x + 127, y + (5 + k) * line, 0xffffff);
    }
    snprintf(t, sizeof t, "Lv %d ダークス", m->level);
    atext(fb, t, x, y + 10 * line, 0xffffff);
}

static void big(u32 *fb, const char *t, int x, int y) { sa_text_draw(fb, SA_W, SA_H, t, -1, x, y, DETAIL_SIZE, 0xffffff, 0x000000); }

static void draw_detail(u32 *fb, const SaveSummary *m, const char *title) {
    sa_font_set_plain(false);
    big(fb, title, DETAIL_X, DETAIL_Y);
    if (!m->used) {
        big(fb, "NoData", DETAIL_X + 10, DETAIL_Y + DETAIL_LINE);
        return;
    }
    int x = DETAIL_X + DETAIL_INDENT;
    char t[128];
    snprintf(t, sizeof t, "%dターン %s%s%s", m->turn, m->place[0], m->place[1][0] ? " " : "", m->place[1]);
    big(fb, t, x, DETAIL_Y + DETAIL_LINE);
    static const char *const labels[4] = {"住民", "食料", "資金", "資源"};
    for (int k = 0; k < 4; k++) {
        int y = DETAIL_Y + (2 + k) * DETAIL_LINE;
        big(fb, labels[k], x, y);
        snprintf(t, sizeof t, "%d", m->town[k]);
        big(fb, t, DETAIL_VALUE_RIGHT - sa_text_width(t, -1, DETAIL_SIZE), y);
    }
    snprintf(t, sizeof t, "Lv %d ダークス%s%s", m->level, m->weapon[0] ? " " : "", m->weapon);
    big(fb, t, DETAIL_LV_X, DETAIL_Y + 3 * DETAIL_LINE);
}

static void draw(SA *s) {
    SaveScreen *v = &s->saves;
    u32 *fb = s->fb;
    sa_blit(fb, v->art[v->saving ? ART_SAVE_BG : ART_LOAD_BG], 0, 0);
    sa_blit_rect(fb, v->art[ART_AUTO], 0, v->hover == HOVER_AUTO ? 355 : 0, 252, 355, AUTO_X, AUTO_Y);
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    if (v->autoLatest >= 0) draw_summary(fb, &v->sum[v->autoLatest], v->autoLatest, AUTO_TEXT_X, AUTO_TEXT_Y, AUTO_LINE);
    sa_font_set_plain(true);
    sa_blit_rect(fb, v->art[ART_LOG], 0, 0, 236, TAB_H, LOG_X, BAR_Y);
    for (int i = 0; i < 10; i++) {
        int frame = i == v->page ? 2 : v->hover == HOVER_TAB + i ? 1 : 0;
        sa_blit_rect(fb, v->art[ART_TAB0 + (v->saving ? 10 : 0) + i], 0, frame * TAB_H, TAB_W, TAB_H, TAB_X + i * TAB_DX, BAR_Y);
    }
    sa_blit_rect(fb, v->art[ART_BACK], 0, v->hover == HOVER_BACK ? TAB_H : 0, BACK_W, TAB_H, BACK_X, BAR_Y);
    for (int i = 0; i < SLOTS_PER_PAGE; i++) draw_row(s, fb, i);
    int show = v->confirm >= SAVE_SLOTS ? -1 : v->confirm >= 0 ? v->confirm : v->hover >= HOVER_ROW ? v->page * SLOTS_PER_PAGE + v->hover - HOVER_ROW : -1;
    if (show >= 0) {
        char t[16];
        snprintf(t, sizeof t, "No%02d", show);
        draw_detail(fb, &v->sum[show], t);
    } else
        draw_detail(fb, &v->now, "現在");
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    if (v->confirm < 0) return;
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u | (fb[i] >> 1 & 0x7f7f7fu);
    const char *ask[1] = {v->saving ? "セーブしますか？" : "ロードしますか？"};
    sa_popup_draw(s->battle.pop, fb, ask, 1);
    sa_blit_rect(fb, v->art[ART_YES], 0, v->hover == HOVER_YES ? YES_H : 0, YES_W, YES_H, YES_X, YES_Y);
    sa_blit_rect(fb, v->art[ART_NO], 0, v->hover == HOVER_NO ? YES_H : 0, YES_W, YES_H, NO_X, YES_Y);
}

static void close_screen(SA *s) {
    s->mode = s->saves.back == MODE_TITLE ? MODE_TITLE : MODE_TOWN;
    s->town.hover = -1;
    s->dirty = true;
}

static void confirm(SA *s) {
    SaveScreen *v = &s->saves;
    int slot = v->confirm;
    v->confirm = -1;
    if (v->saving) {
        int count = 0;
        for (int i = 0; i < SAVE_SLOTS; i++)
            if (v->sum[i].used && v->sum[i].count > count) count = v->sum[i].count;
        sa_save_write(s, slot, count + 1);
        refresh(s);
        return;
    }
    if (!v->sum[slot].used) return;
    sa_load_into_town(s, slot);
}

bool sa_savescreen_frame(SA *s) {
    if (s->mode != MODE_SAVES) return false;
    SaveScreen *v = &s->saves;
    int h = hit(s, s->mouseX, s->mouseY);
    if (h != v->hover) {
        v->hover = h;
        s->dirty = true;
    }
    if (s->rightClicked) {
        s->rightClicked = false;
        if (v->confirm >= 0) v->confirm = -1;
        else close_screen(s);
        s->dirty = true;
    }
    if (s->clicked) {
        s->clicked = false;
        if (h == HOVER_BACK) close_screen(s);
        else if (h == HOVER_YES) confirm(s);
        else if (h == HOVER_NO) v->confirm = -1;
        else if (h == HOVER_AUTO) v->confirm = v->autoLatest;
        else if (h >= HOVER_ROW) {
            int slot = v->page * SLOTS_PER_PAGE + h - HOVER_ROW;
            if (v->saving || v->sum[slot].used) v->confirm = slot;
        } else if (h >= HOVER_TAB)
            v->page = h - HOVER_TAB;
        s->dirty = true;
    }
    if (s->mode == MODE_SAVES && s->dirty) {
        draw(s);
        s->serial++;
        s->dirty = false;
    }
    return true;
}

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
    DETAIL_DATE_X = 406,
    DETAIL_TIME_X = 598,
    DETAIL_PLAY_X = 695,
    DETAIL_PLAYED_X = 850,
    DETAIL_COUNT_RIGHT = 1196,
    DETAIL_WEAPON_X = 860,
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
    OK_Y = 425,
    ACTION_X = 1037,
    DEL_X = 298,
    BUTTON_Y = 318,
    ACTION_W = 200,
    DEL_W = 120,
    BUTTON_H = 40,
    COMMENT_X = 412,
    COMMENT_Y = 294,
    COMMENT_W = 455,
    COMMENT_H = 130,
    FIELD_X = 446,
    FIELD_Y = 378,
    FIELD_W = 391,
    FIELD_H = 33,
    CARET_X = 447,
    CARET_Y = 385,
    CARET_H = 24,
    COMMENT_SIZE = 24,
    COMMENT_TEXT_Y = 384,
    COMMENT_DETAIL_X = 442,
    COMMENT_DETAIL_Y = 314,
    ROW_COMMENT_PITCH = 20,
    TEXT = 15,
    HOVER_BACK = 1,
    HOVER_AUTO = 2,
    HOVER_YES = 3,
    HOVER_NO = 4,
    HOVER_ACTION = 5,
    HOVER_DEL = 6,
    HOVER_TAB = 100,
    HOVER_ROW = 200,
    SLOTS_PER_PAGE = 10,
};

enum { ASK_NONE, ASK_LOAD, ASK_DELETE, ASK_COMMENT };

enum {
    ART_LOAD_BG,
    ART_SAVE_BG,
    ART_AUTO,
    ART_LOG,
    ART_BACK,
    ART_YES,
    ART_NO,
    ART_DATA_LOAD,
    ART_DATA_SAVE,
    ART_DEL,
    ART_OK,
    ART_CANCEL,
    ART_TAB0,
    ART_ROW0 = ART_TAB0 + 20,
    ART_COUNT = ART_ROW0 + 20
};

void sa_savescreen_init(SaveScreen *v, Vfs *vfs) {
    memset(v, 0, sizeof *v);
    static const char *const names[ART_TAB0] = {"sav_a00.agf", "sav_a01.agf", "sav_a06.agf", "sav_a07.agf", "sav_a05.agf", "pop_b00.agf",
                                                "pop_b01.agf", "sav_a02.agf", "sav_a03.agf", "sav_a04.agf", "pop_b02.agf", "pop_b03.agf"};
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
    v->hover = v->confirm = v->picked = -1;
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
    v->page = s->opt.value[OPT_SAVE_PAGE] % 10;
    v->hover = v->confirm = v->picked = -1;
    v->ask = ASK_NONE;
    v->back = s->mode;
    refresh(s);
    s->mode = MODE_SAVES;
    s->dirty = true;
}

void sa_autosave(SA *s) {
    SaveScreen *v = &s->saves;
    v->comment[0] = 0;
    refresh(s);
    int count = (v->autoLatest < 0 ? 0 : v->sum[v->autoLatest].count) + 1;
    sa_save_write(s, SAVE_SLOTS + count % SAVE_AUTOS, count);
}

static u32 full_width(u32 cp) {
    if (cp == ' ') return 0x3000;
    if (cp > ' ' && cp < 0x7f) return cp + 0xfee0;
    return cp;
}

static int utf8_chars(const char *t) {
    int n = 0;
    for (u32 cp; *t; n++) t += sa_utf8_decode(t, &cp);
    return n;
}

void sa_save_comment_put(const SA *s, u8 *img) {
    int i = 0;
    for (const char *t = s->saves.comment; *t && i < COMMENT_CHARS;) {
        u32 cp;
        t += sa_utf8_decode(t, &cp);
        if (cp > 0xffff) continue;
        img[i * 2] = (u8)cp;
        img[i * 2 + 1] = (u8)(cp >> 8);
        i++;
    }
}

void sa_save_comment_get(const u8 *img, SaveSummary *out) {
    int n = 0;
    while (n < COMMENT_CHARS && rd16(img + n * 2)) n++;
    char *t = sa_utf16_to_utf8(img, n);
    snprintf(out->comment, sizeof out->comment, "%s", t);
    free(t);
}

void sa_savescreen_text(SA *s, const char *utf8) {
    SaveScreen *v = &s->saves;
    if (s->mode != MODE_SAVES || v->ask != ASK_COMMENT) return;
    size_t len = strlen(v->comment);
    int n = utf8_chars(v->comment);
    for (const char *t = utf8; *t && n < COMMENT_CHARS;) {
        u32 cp;
        t += sa_utf8_decode(t, &cp);
        if (cp < ' ' || cp == 0x7f || (cp >= 0xe000 && cp < 0xf900)) continue;
        char buf[8];
        int k = sa_utf8_put(buf, full_width(cp));
        if (len + (size_t)k >= sizeof v->comment) break;
        memcpy(v->comment + len, buf, (size_t)k);
        len += (size_t)k;
        v->comment[len] = 0;
        n++;
    }
    s->dirty = true;
}

static void answer(SA *s);

void sa_savescreen_key(SA *s, int key) {
    SaveScreen *v = &s->saves;
    if (s->mode != MODE_SAVES || v->ask != ASK_COMMENT) return;
    if (key == 0x08) {
        size_t len = strlen(v->comment);
        while (len && ((u8)v->comment[len - 1] & 0xc0) == 0x80) len--;
        if (len) len--;
        v->comment[len] = 0;
    } else if (key == 0x0d)
        answer(s);
    else if (key == 0x1b) {
        v->ask = ASK_NONE;
        v->confirm = -1;
    }
    s->dirty = true;
}

static void cells(u32 *fb, const char *t, int x, int y, int size, int pitch, u32 color, u32 edge) {
    for (u32 cp; *t; x += pitch) {
        int k = sa_utf8_decode(t, &cp);
        sa_text_draw(fb, SA_W, SA_H, t, 1, x, y, size, color, edge);
        t += k;
    }
}

static bool in(int x, int y, int rx, int ry, int w, int h) { return x >= rx && x < rx + w && y >= ry && y < ry + h; }

static int row_x(int i) { return i < 5 ? ROW_X0 : ROW_X1; }

static int row_y(int i) { return ROW_Y + i % 5 * ROW_DY; }

static bool can_delete(const SaveScreen *v) { return v->picked >= 0 && v->sum[v->picked].used; }

static bool has_action(const SaveScreen *v) { return v->picked >= 0 && (v->saving || v->sum[v->picked].used); }

static int hit(SA *s, int x, int y) {
    SaveScreen *v = &s->saves;
    if (v->ask != ASK_NONE) {
        int by = v->ask == ASK_COMMENT ? OK_Y : YES_Y;
        if (in(x, y, YES_X, by, YES_W, YES_H)) return HOVER_YES;
        if (in(x, y, NO_X, by, YES_W, YES_H)) return HOVER_NO;
        return -1;
    }
    if (in(x, y, BACK_X, BAR_Y, BACK_W, TAB_H)) return HOVER_BACK;
    if (has_action(v) && in(x, y, ACTION_X, BUTTON_Y, ACTION_W, BUTTON_H)) return HOVER_ACTION;
    if (can_delete(v) && in(x, y, DEL_X, BUTTON_Y, DEL_W, BUTTON_H)) return HOVER_DEL;
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
    int frame = v->picked == slot ? 2 : v->hover == HOVER_ROW + i ? 1 : 0;
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
    snprintf(t, sizeof t, "%d", m->count);
    right(fb, t, x + COUNT_RIGHT, y + ROW_TEXT_Y, color);
    text(fb, m->place[0], x + ROW_TEXT_X + 30, y + ROW_TEXT_Y + ROW_LINE, color);
    if (m->comment[0])
        cells(fb, m->comment, x + ROW_TEXT_X + 30 + (utf8_chars(m->place[0]) + 1) * ROW_COMMENT_PITCH, y + ROW_TEXT_Y + ROW_LINE, TEXT,
              ROW_COMMENT_PITCH, color, 0x000000);
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

static void draw_detail(u32 *fb, const SaveSummary *m, int slot) {
    sa_font_set_plain(false);
    char t[128];
    if (slot < 0) big(fb, "現在", DETAIL_X, DETAIL_Y);
    else if (!m->used) {
        snprintf(t, sizeof t, "No%02d NoData", slot);
        big(fb, t, DETAIL_X, DETAIL_Y);
        return;
    } else {
        time_t tt = (time_t)m->time;
        struct tm tm;
        localtime_r(&tt, &tm);
        snprintf(t, sizeof t, "No.%02d", slot);
        big(fb, t, DETAIL_X, DETAIL_Y);
        snprintf(t, sizeof t, "%04d年%d月%d日", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
        big(fb, t, DETAIL_DATE_X, DETAIL_Y);
        snprintf(t, sizeof t, "%02d:%02d", tm.tm_hour, tm.tm_min);
        big(fb, t, DETAIL_TIME_X, DETAIL_Y);
        big(fb, "プレイ時間", DETAIL_PLAY_X, DETAIL_Y);
        snprintf(t, sizeof t, "%d:%02d", m->play / 3600, m->play / 60 % 60);
        big(fb, t, DETAIL_PLAYED_X, DETAIL_Y);
        snprintf(t, sizeof t, "%d", m->count);
        big(fb, t, DETAIL_COUNT_RIGHT - sa_text_width(t, -1, DETAIL_SIZE), DETAIL_Y);
    }
    if (!m->used) {
        big(fb, "NoData", DETAIL_X + 10, DETAIL_Y + DETAIL_LINE);
        return;
    }
    int x = DETAIL_X + DETAIL_INDENT;
    if (m->place[1][0]) snprintf(t, sizeof t, "%dターン %s %s %d歩目", m->turn, m->place[0], m->place[1], m->steps);
    else snprintf(t, sizeof t, "%dターン %s", m->turn, m->place[0]);
    big(fb, t, x, DETAIL_Y + DETAIL_LINE);
    static const char *const labels[4] = {"住民", "食料", "資金", "資源"};
    for (int k = 0; k < 4; k++) {
        int y = DETAIL_Y + (2 + k) * DETAIL_LINE;
        big(fb, labels[k], x, y);
        snprintf(t, sizeof t, "%d", m->town[k]);
        big(fb, t, DETAIL_VALUE_RIGHT - sa_text_width(t, -1, DETAIL_SIZE), y);
    }
    int lvY = DETAIL_Y + 3 * DETAIL_LINE;
    if (slot < 0) {
        snprintf(t, sizeof t, "Lv %d ダークス%s%s", m->level, m->weapon[0] ? " " : "", m->weapon);
        big(fb, t, DETAIL_LV_X, lvY);
        return;
    }
    snprintf(t, sizeof t, "Lv %d ダークス", m->level);
    big(fb, t, DETAIL_LV_X, lvY);
    if (m->weapon[0]) big(fb, m->weapon, DETAIL_WEAPON_X, lvY);
    if (m->comment[0]) cells(fb, m->comment, COMMENT_DETAIL_X, COMMENT_DETAIL_Y, COMMENT_SIZE, COMMENT_SIZE, 0xffffff, 0xffffffffu);
}

static void button(SaveScreen *v, u32 *fb, int art, int hover, int x, int y, int w, int h) {
    sa_blit_rect(fb, v->art[art], 0, v->hover == hover ? h : 0, w, h, x, y);
}

static void dim(u32 *fb, u32 num, u32 den) {
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) {
        u32 p = fb[i], out = 0xff000000u;
        for (int k = 0; k < 24; k += 8) out |= (((p >> k & 0xff) * num + den / 2) / den) << k;
        fb[i] = out;
    }
}

static void draw(SA *s) {
    SaveScreen *v = &s->saves;
    u32 *fb = s->fb;
    sa_blit(fb, v->art[v->saving ? ART_SAVE_BG : ART_LOAD_BG], 0, 0);
    sa_font_set_spacing(0);
    if (!v->saving) {
        sa_blit_rect(fb, v->art[ART_AUTO], 0, v->hover == HOVER_AUTO ? 355 : 0, 252, 355, AUTO_X, AUTO_Y);
        if (v->autoLatest >= 0) draw_summary(fb, &v->sum[v->autoLatest], v->autoLatest, AUTO_TEXT_X, AUTO_TEXT_Y, AUTO_LINE);
        sa_blit_rect(fb, v->art[ART_LOG], 0, 0, 236, TAB_H, LOG_X, BAR_Y);
    }
    sa_font_set_plain(true);
    for (int i = 0; i < 10; i++) {
        int frame = i == v->page ? 2 : v->hover == HOVER_TAB + i ? 1 : 0;
        sa_blit_rect(fb, v->art[ART_TAB0 + (v->saving ? 10 : 0) + i], 0, frame * TAB_H, TAB_W, TAB_H, TAB_X + i * TAB_DX, BAR_Y);
    }
    sa_blit_rect(fb, v->art[ART_BACK], 0, v->hover == HOVER_BACK ? TAB_H : 0, BACK_W, TAB_H, BACK_X, BAR_Y);
    for (int i = 0; i < SLOTS_PER_PAGE; i++) draw_row(s, fb, i);
    int show = v->confirm >= SAVE_SLOTS                ? -1
               : v->picked >= 0                        ? v->picked
               : v->hover >= HOVER_ROW && v->ask == ASK_NONE ? v->page * SLOTS_PER_PAGE + v->hover - HOVER_ROW
                                                       : -1;
    draw_detail(fb, show >= 0 ? &v->sum[show] : &v->now, show);
    if (can_delete(v)) button(v, fb, ART_DEL, HOVER_DEL, DEL_X, BUTTON_Y, DEL_W, BUTTON_H);
    if (has_action(v)) button(v, fb, v->saving ? ART_DATA_SAVE : ART_DATA_LOAD, HOVER_ACTION, ACTION_X, BUTTON_Y, ACTION_W, BUTTON_H);
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    if (v->ask == ASK_NONE) return;
    if (v->ask == ASK_COMMENT) {
        dim(fb, 15, 32);
        const char *lines[3] = {"セーブしますか？", "コメントを残す事が出来ます。", "必要であれば下記に入力してください。"};
        sa_popup_box(s->battle.pop, fb, lines, 3, COMMENT_X, COMMENT_Y, COMMENT_W, COMMENT_H);
        sa_fill(fb, FIELD_X, FIELD_Y, FIELD_W, FIELD_H, 0xff000000u);
        sa_font_set_spacing(0);
        cells(fb, v->comment, CARET_X, COMMENT_TEXT_Y, COMMENT_SIZE, COMMENT_SIZE, 0xffffff, 0xffffffffu);
        sa_font_set_spacing(1);
        sa_fill(fb, CARET_X + utf8_chars(v->comment) * COMMENT_SIZE, CARET_Y, 2, CARET_H, 0xffffffffu);
        button(v, fb, ART_OK, HOVER_YES, YES_X, OK_Y, YES_W, YES_H);
        button(v, fb, ART_CANCEL, HOVER_NO, NO_X, OK_Y, YES_W, YES_H);
        return;
    }
    dim(fb, 3, 8);
    const char *ask[1] = {v->ask == ASK_DELETE ? "削除しますか？" : "ロードしますか？"};
    sa_popup_draw(s->battle.pop, fb, ask, 1);
    button(v, fb, ART_YES, HOVER_YES, YES_X, YES_Y, YES_W, YES_H);
    button(v, fb, ART_NO, HOVER_NO, NO_X, YES_Y, YES_W, YES_H);
}

static void close_screen(SA *s) {
    s->mode = s->saves.back == MODE_TITLE ? MODE_TITLE : sa_back_mode(s);
    if (s->mode == MODE_CAMP) sa_camp_return(s);
    s->town.hover = -1;
    s->dirty = true;
}

static void answer(SA *s) {
    SaveScreen *v = &s->saves;
    int slot = v->confirm, ask = v->ask;
    v->confirm = -1;
    v->ask = ASK_NONE;
    if (ask == ASK_DELETE) {
        sa_save_delete(s, slot);
        v->picked = -1;
        refresh(s);
        return;
    }
    if (ask == ASK_COMMENT) {
        int count = 0;
        for (int i = 0; i < SAVE_SLOTS; i++)
            if (v->sum[i].used && v->sum[i].count > count) count = v->sum[i].count;
        sa_save_write(s, slot, count + 1);
        v->comment[0] = 0;
        refresh(s);
        return;
    }
    if (v->sum[slot].used) sa_load_into_town(s, slot);
}

static void ask(SaveScreen *v, int slot, int what) {
    v->confirm = slot;
    v->ask = what;
    v->comment[0] = 0;
}

static void click(SA *s, int h) {
    SaveScreen *v = &s->saves;
    if (v->ask != ASK_NONE) {
        if (h == HOVER_YES) answer(s);
        else if (h == HOVER_NO) {
            v->confirm = -1;
            v->ask = ASK_NONE;
        }
        return;
    }
    if (h == HOVER_BACK) close_screen(s);
    else if (h == HOVER_AUTO) ask(v, v->autoLatest, ASK_LOAD);
    else if (h == HOVER_ACTION) ask(v, v->picked, v->saving ? ASK_COMMENT : ASK_LOAD);
    else if (h == HOVER_DEL) ask(v, v->picked, ASK_DELETE);
    else if (h >= HOVER_ROW) {
        int slot = v->page * SLOTS_PER_PAGE + h - HOVER_ROW;
        v->picked = v->saving || v->sum[slot].used ? slot : -1;
    } else if (h >= HOVER_TAB) {
        v->page = h - HOVER_TAB;
        v->picked = -1;
        s->opt.value[OPT_SAVE_PAGE] = (u8)v->page;
        s->vm.gDirty = true;
    }
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
        if (v->ask != ASK_NONE) {
            v->ask = ASK_NONE;
            v->confirm = -1;
        } else if (v->picked >= 0)
            v->picked = -1;
        else
            close_screen(s);
        s->dirty = true;
    }
    if (s->clicked) {
        s->clicked = false;
        click(s, h);
        s->dirty = true;
    }
    if (s->mode == MODE_SAVES && s->dirty) {
        draw(s);
        s->serial++;
        s->dirty = false;
    }
    return true;
}

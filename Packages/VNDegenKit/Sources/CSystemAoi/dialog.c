#include "sa.h"

enum {
    DLG_X = 430,
    DLG_Y = 130,
    HEAD_H = 31,
    TABLE_H = 121,
    TEXT_H = 164,
    OK_X = 565,
    OK_DY = 7,
    OK_W = 152,
    OK_H = 44,
    TEXT_X = 32,
    TEXT_Y = 17,
    TEXT_SIZE = 20,
    LINE_H = 20,
    ROW_H = 31,
    NUM_Y = 5,
    NUM_SIZE = 20,
    NUM_SPACING = 6,
    DELTA_Y = 14,
    DELTA_SIZE = 12,
    BEFORE_RIGHT = 622,
    DELTA_RIGHT = 690,
    AFTER_RIGHT = 815,
};

enum { ART_FOOT, ART_OK, ART_HEAD, ART_TABLE, ART_TEXT };

void sa_dialog_init(Dialog *d, Vfs *vfs) {
    memset(d, 0, sizeof *d);
    static const char *const names[DIALOG_ART] = {"ret_a00.agf", "ret_a01.agf", "ret_a02.agf", "ret_a03.agf", "ret_a04.agf"};
    for (int i = 0; i < DIALOG_ART; i++) d->art[i] = sa_load_image(vfs, names[i]);
}

static void clear_lines(Dialog *d) {
    for (int i = 0; i < d->lineCount; i++) free(d->lines[i]);
    d->lineCount = 0;
}

void sa_dialog_free(Dialog *d) {
    clear_lines(d);
    for (int i = 0; i < DIALOG_ART; i++) sa_image_free(d->art[i]);
    memset(d, 0, sizeof *d);
}

void sa_dialog_open(Dialog *d, const char *text) {
    clear_lines(d);
    const char *p = text;
    while (d->lineCount < DIALOG_LINES) {
        const char *e = strchr(p, '|');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        char *line = sa_alloc(n + 1);
        memcpy(line, p, n);
        line[n] = 0;
        d->lines[d->lineCount++] = line;
        if (!e) break;
        p = e + 1;
    }
    d->open = true;
    d->table = false;
    d->hover = false;
}

void sa_dialog_open_turn(Dialog *d, const int before[4], const int after[4], const char *text) {
    sa_dialog_open(d, text);
    d->table = true;
    memcpy(d->before, before, sizeof d->before);
    memcpy(d->after, after, sizeof d->after);
}

static int ok_y(const Dialog *d) { return DLG_Y + HEAD_H + (d->table ? TABLE_H : 0) + TEXT_H + OK_DY; }

static bool on_ok(const Dialog *d, int x, int y) { return x >= OK_X && x < OK_X + OK_W && y >= ok_y(d) && y < ok_y(d) + OK_H; }

bool sa_dialog_hover(Dialog *d, int x, int y) {
    bool h = on_ok(d, x, y);
    if (h == d->hover) return false;
    d->hover = h;
    return true;
}

bool sa_dialog_click(Dialog *d, int x, int y) {
    if (x >= 0 && !on_ok(d, x, y)) return false;
    d->open = false;
    clear_lines(d);
    return true;
}

static void number(u32 *fb, int v, int right, int y, int size, int spacing, u32 color) {
    char s[16];
    snprintf(s, sizeof s, "%d", v);
    sa_font_set_spacing(spacing);
    sa_text_draw(fb, SA_W, SA_H, s, -1, right - sa_text_width(s, -1, size) + spacing, y, size, color, 0xffffffffu);
    sa_font_set_spacing(1);
}

void sa_dialog_draw(Dialog *d, u32 *fb) {
    if (!d->open) return;
    int y = DLG_Y;
    sa_blit(fb, d->art[ART_HEAD], DLG_X, y);
    y += HEAD_H;
    if (d->table) {
        sa_blit(fb, d->art[ART_TABLE], DLG_X, y);
        for (int i = 0; i < 4; i++) {
            int ry = y + i * ROW_H, delta = d->after[i] - d->before[i];
            u32 shade = delta > 0 ? 0x007fff : delta < 0 ? 0xff0000 : 0x321a02;
            number(fb, d->before[i], BEFORE_RIGHT, ry + NUM_Y, NUM_SIZE, NUM_SPACING, 0x321a02);
            number(fb, d->after[i], AFTER_RIGHT, ry + NUM_Y, NUM_SIZE, NUM_SPACING, shade);
            number(fb, delta < 0 ? -delta : delta, DELTA_RIGHT, ry + DELTA_Y, DELTA_SIZE, 1, shade);
        }
        y += TABLE_H;
    }
    sa_blit(fb, d->art[ART_TEXT], DLG_X, y);
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    for (int i = 0; i < d->lineCount; i++)
        sa_text_draw(fb, SA_W, SA_H, d->lines[i], -1, DLG_X + TEXT_X, y + TEXT_Y + i * LINE_H, TEXT_SIZE, 0x000000, 0xffffffffu);
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    y += TEXT_H;
    sa_blit(fb, d->art[ART_FOOT], DLG_X, y);
    sa_blit_rect(fb, d->art[ART_OK], 0, d->hover ? OK_H : 0, OK_W, OK_H, OK_X, y + OK_DY);
}

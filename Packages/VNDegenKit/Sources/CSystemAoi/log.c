#include "sa.h"

enum {
    PANEL_X = 262,
    TEXT_X = 283,
    PITCH = 30,
    LOG_SIZE = 26,
    PLAY_X = 422,
    PLAY_W = 128,
    PLAY_H = 25,
    BOTTOM = 741,
    TRACK_X = 1102,
    TRACK_Y = 35,
    TRACK_H = 650,
    BTN_X = 1148,
    FAST_Y = 555,
    AUTO_Y = 600,
    BACK_X = 1146,
    BACK_Y = 660,
    BTN_W = 130,
    BTN_H = 40,
};

static const char *const artNames[LOG_ART_COUNT] = {"log_a00.agf", "log_a01.agf", "log_a02.agf", "log_b00.agf", "log_b01.agf",
                                                    "log_b02.agf", "log_b03.agf", "log_c00.agf", "log_c01.agf"};

void sa_log_init(Backlog *b, Vfs *vfs) {
    memset(b, 0, sizeof *b);
    b->hover = -1;
    for (int i = 0; i < LOG_ART_COUNT; i++) {
        size_t n;
        u8 *d = sa_vfs_read(vfs, artNames[i], &n);
        b->art[i] = d ? sa_agf_decode(d, n) : NULL;
        free(d);
    }
}

static void entry_free(LogEntry *e) {
    free(e->name);
    for (int i = 0; i < e->lineCount; i++) free(e->lines[i]);
    free(e->lines);
    memset(e, 0, sizeof *e);
}

void sa_log_free(Backlog *b) {
    for (int i = 0; i < LOG_ENTRIES; i++) entry_free(&b->e[i]);
    for (int i = 0; i < LOG_ART_COUNT; i++) sa_image_free(b->art[i]);
    memset(b, 0, sizeof *b);
}

static LogEntry *entry(Backlog *b, int i) { return &b->e[(b->first + i) % LOG_ENTRIES]; }

void sa_log_add(Backlog *b, const char *name, const char *voice, char **lines, int lineCount) {
    if (!lineCount) return;
    LogEntry *e;
    if (b->count == LOG_ENTRIES) {
        e = entry(b, 0);
        entry_free(e);
        b->first = (b->first + 1) % LOG_ENTRIES;
    } else {
        e = entry(b, b->count++);
    }
    e->name = sa_strdup(name ? name : "");
    snprintf(e->voice, sizeof e->voice, "%s", voice ? voice : "");
    e->lines = sa_alloc(sizeof(char *) * (size_t)lineCount);
    for (int i = 0; i < lineCount; i++) e->lines[i] = sa_strdup(lines[i]);
    e->lineCount = lineCount;
}

static int entry_rows(const LogEntry *e) { return (e->name[0] ? 1 : 0) + e->lineCount + 1; }

static int total_rows(Backlog *b) {
    int n = 0;
    for (int i = 0; i < b->count; i++) n += entry_rows(entry(b, i));
    return n ? n - 1 : 0;
}

static int visible_rows(void) { return BOTTOM / PITCH; }

static int max_scroll(Backlog *b) {
    int m = total_rows(b) - visible_rows();
    return m > 0 ? m : 0;
}

void sa_log_open(Backlog *b) {
    b->open = true;
    b->scroll = 0;
    b->hover = -1;
}

void sa_log_scroll(Backlog *b, int lines) {
    b->scroll += lines;
    if (b->scroll < 0) b->scroll = 0;
    if (b->scroll > max_scroll(b)) b->scroll = max_scroll(b);
}

static int row_y(Backlog *b, int row) {
    int top = BOTTOM - PITCH * (total_rows(b) - b->scroll);
    return top + row * PITCH - 22;
}

typedef struct Hit {
    int kind;
    int entry;
} Hit;

static Hit hit_test(Backlog *b, int x, int y) {
    Hit h = {LOG_ACT_NONE, -1};
    if (x >= BACK_X && x < BACK_X + 132 && y >= BACK_Y && y < BACK_Y + BTN_H) h.kind = LOG_ACT_CLOSE;
    else if (x >= BTN_X && x < BTN_X + BTN_W && y >= FAST_Y && y < FAST_Y + BTN_H) h.kind = LOG_ACT_FAST;
    else if (x >= BTN_X && x < BTN_X + BTN_W && y >= AUTO_Y && y < AUTO_Y + BTN_H) h.kind = LOG_ACT_AUTO;
    else if (x >= PLAY_X && x < PLAY_X + PLAY_W) {
        int row = 0;
        for (int i = 0; i < b->count; i++) {
            LogEntry *e = entry(b, i);
            int ny = row_y(b, row);
            if (e->name[0] && e->voice[0] && y >= ny && y < ny + PLAY_H) {
                h.kind = LOG_ACT_VOICE;
                h.entry = i;
                break;
            }
            row += entry_rows(e);
        }
    }
    return h;
}

bool sa_log_hover(Backlog *b, int x, int y) {
    Hit h = hit_test(b, x, y);
    int v = h.kind == LOG_ACT_VOICE ? 100 + h.entry : h.kind;
    if (v == b->hover) return false;
    b->hover = v;
    return true;
}

int sa_log_click(Backlog *b, int x, int y, char *voice, size_t voiceSize) {
    Hit h = hit_test(b, x, y);
    if (h.kind == LOG_ACT_VOICE) snprintf(voice, voiceSize, "%s", entry(b, h.entry)->voice);
    if (h.kind == LOG_ACT_CLOSE) b->open = false;
    return h.kind;
}

void sa_log_draw(Backlog *b, u32 *fb, bool fast, bool autoMode) {
    sa_font_set_spacing(3);
    sa_blit_alpha(fb, b->art[LOG_ART_PANEL], PANEL_X, 0, 180);
    int row = 0;
    for (int i = 0; i < b->count; i++) {
        LogEntry *e = entry(b, i);
        int y = row_y(b, row);
        if (y > SA_H) break;
        if (y + entry_rows(e) * PITCH >= 0) {
            int r = 0;
            if (e->name[0]) {
                int ny = row_y(b, row);
                if (ny > -PITCH) {
                    sa_text_draw(fb, SA_W, SA_H, e->name, -1, TEXT_X, ny + 2, LOG_SIZE, 0xffffff, 0x000000);
                    if (e->voice[0]) {
                        int px = PLAY_X, nw = sa_text_width(e->name, -1, LOG_SIZE);
                        if (TEXT_X + nw + 16 > px) px = TEXT_X + nw + 16;
                        sa_blit_rect(fb, b->art[LOG_ART_PLAY], 0, b->hover == 100 + i ? PLAY_H : 0, PLAY_W, PLAY_H, px, ny);
                    }
                }
                r++;
            }
            for (int k = 0; k < e->lineCount; k++, r++) {
                int ly = row_y(b, row + r);
                if (ly > -PITCH && ly < SA_H) sa_text_draw(fb, SA_W, SA_H, e->lines[k], -1, TEXT_X, ly + 2, LOG_SIZE, 0xffffff, 0x000000);
            }
        }
        row += entry_rows(e);
    }
    sa_font_set_spacing(1);
    sa_blit(fb, b->art[LOG_ART_TRACK], TRACK_X, TRACK_Y);
    int rows = total_rows(b), vis = visible_rows();
    int thumb = rows > vis ? TRACK_H * vis / rows : TRACK_H;
    if (thumb < 40) thumb = 40;
    int pos = rows > vis ? (TRACK_H - thumb) * (max_scroll(b) - b->scroll) / max_scroll(b) : 0;
    sa_blit_rect(fb, b->art[LOG_ART_THUMB], 0, 0, 36, thumb, TRACK_X, TRACK_Y + pos);
    sa_blit_rect(fb, b->art[LOG_ART_UP], 0, 0, 36, 35, TRACK_X, 0);
    sa_blit_rect(fb, b->art[LOG_ART_DOWN], 0, 0, 36, 35, TRACK_X, 685);
    sa_blit_rect(fb, b->art[LOG_ART_FAST], fast ? BTN_W : 0, b->hover == LOG_ACT_FAST ? BTN_H : 0, BTN_W, BTN_H, BTN_X, FAST_Y);
    sa_blit_rect(fb, b->art[LOG_ART_AUTO], autoMode ? BTN_W : 0, b->hover == LOG_ACT_AUTO ? BTN_H : 0, BTN_W, BTN_H, BTN_X, AUTO_Y);
    sa_blit_rect(fb, b->art[LOG_ART_BACK], 0, b->hover == LOG_ACT_CLOSE ? BTN_H : 0, 132, BTN_H, BACK_X, BACK_Y);
}

#include "ipac.h"

#define WIN_X 15
#define WIN_Y 396
#define WIN_H 204

static bool find_rect(const Image *a, u32 rgb, int y0, int y1, int *x, int *y, int *w, int *h) {
    if (!a) return false;
    u32 want = (rgb >> 16 & 0xff) | (rgb & 0xff00) | (rgb & 0xff) << 16;
    int minx = a->w, miny = a->h, maxx = -1, maxy = -1;
    for (int j = y0 < 0 ? 0 : y0; j < (y1 > a->h ? a->h : y1); j++)
        for (int i = 0; i < a->w; i++)
            if ((a->px[(size_t)j * a->w + i] & 0xffffff) == want) {
                if (i < minx) minx = i;
                if (i > maxx) maxx = i;
                if (j < miny) miny = j;
                if (j > maxy) maxy = j;
            }
    if (maxx < 0) return false;
    *x = minx;
    *y = miny;
    *w = maxx - minx + 1;
    *h = maxy - miny + 1;
    return true;
}

typedef struct WinButton {
    int id;
    int x, y, w, h;
    int sx, sy;
} WinButton;

static struct {
    bool loaded;
    Image *sheet, *map, *click;
    int textX, textY, textW, textH;
    int plateX, plateY, plateW, plateH;
    WinButton buttons[16];
    int count;
} win;

static void win_load(IP *t) {
    if (win.loaded) return;
    win.loaded = true;
    win.sheet = image_load(t, PK_PARTS, "MesWindow");
    win.map = image_load(t, PK_PARTS, "MesWindowA");
    win.click = image_load(t, PK_PARTS, "Click");
    if (!find_rect(win.map, 0xffff00, 0, WIN_H, &win.textX, &win.textY, &win.textW, &win.textH)) {
        win.textX = 48;
        win.textY = 65;
        win.textW = 657;
        win.textH = 108;
    }
    if (!find_rect(win.map, 0xff00ff, WIN_H, 1 << 20, &win.plateX, &win.plateY, &win.plateW, &win.plateH)) {
        win.plateX = 2;
        win.plateY = 214;
        win.plateW = 177;
        win.plateH = 38;
    }
    for (int id = 0x60; id < 0x80 && win.count < 16; id++) {
        int x, y, w, h, sx, sy, sw, sh;
        if (!find_rect(win.map, (u32)id, 0, WIN_H, &x, &y, &w, &h)) continue;
        if (!find_rect(win.map, 0xff00u | (u32)id, WIN_H, 1 << 20, &sx, &sy, &sw, &sh)) continue;
        win.buttons[win.count++] = (WinButton){id, x, y, sw, sh, sx, sy};
    }
}

void msg_clear(IP *t) {
    free(t->msgText);
    t->msgText = NULL;
    free(t->msgName);
    t->msgName = NULL;
    t->msgLen = t->msgShown = 0;
    t->dirty = true;
}

static void log_add(IP *t) {
    if (t->logCount >= 200) {
        free(t->logs[0].name);
        free(t->logs[0].text);
        memmove(t->logs, t->logs + 1, sizeof(LogEntry) * (size_t)(t->logCount - 1));
        t->logCount--;
    }
    t->logs = ip_realloc(t->logs, sizeof(LogEntry) * (size_t)(t->logCount + 1));
    LogEntry *e = &t->logs[t->logCount++];
    e->name = t->pendingName[0] ? ip_strdup(t->pendingName) : NULL;
    e->text = ip_strdup(t->pendingText);
    snprintf(e->voice, sizeof e->voice, "%s", t->voice);
}

void msg_show(IP *t) {
    win_load(t);
    log_add(t);
    msg_clear(t);
    t->msgName = t->pendingName[0] ? ip_strdup(t->pendingName) : NULL;
    t->msgText = text_decode(t->pendingText, &t->msgLen);
    free(t->pendingText);
    t->pendingText = NULL;
    t->pendingName[0] = 0;
    if (t->voice[0]) {
        if (t->voiceOn) snd_voice(t, t->voice);
        t->voice[0] = 0;
    }
    t->winOpen = true;
    t->msgStart = t->now;
    t->msgShown = 0;
    t->wait = W_TEXT;
    t->dirty = true;
}

static int ms_per_char(IP *t) { return t->textSpeed == 0 ? 60 : t->textSpeed == 2 ? 10 : 30; }

bool msg_complete(IP *t) { return t->msgShown >= t->msgLen; }

void msg_update(IP *t) {
    if (!t->msgText || msg_complete(t)) return;
    int n = (int)((t->now - t->msgStart) / (u32)ms_per_char(t));
    if (n > t->msgLen) n = t->msgLen;
    if (n != t->msgShown) {
        t->msgShown = n;
        t->dirty = true;
    }
}

void msg_reveal(IP *t) {
    t->msgShown = t->msgLen;
    t->dirty = true;
}

static int layout(IP *t, int *lineStart, int *lineEnd, int max) {
    int lines = 0, x = 0, start = 0;
    int width = win.textW;
    for (int i = 0; i <= t->msgLen && lines < max; i++) {
        u32 cp = i < t->msgLen ? t->msgText[i] : '\n';
        if (cp == '~' || cp == '$') continue;
        int adv = text_wide(cp) ? t->pitch : t->pitch / 2;
        if (cp == '\n' || x + adv > width) {
            lineStart[lines] = start;
            lineEnd[lines] = i;
            lines++;
            start = cp == '\n' ? i + 1 : i;
            x = cp == '\n' ? 0 : adv;
            continue;
        }
        x += adv;
    }
    return lines;
}

static void draw_text(IP *t, int ox, int oy) {
    int ls[16], le[16];
    int lines = layout(t, ls, le, 16);
    int lineH = t->pitch + 8;
    for (int l = 0; l < lines; l++) {
        int x = ox, y = oy + l * lineH;
        bool inRuby = false;
        for (int i = ls[l]; i < le[l] && i < t->msgShown; i++) {
            u32 cp = t->msgText[i];
            if (cp == '~') continue;
            if (cp == '$') {
                inRuby = !inRuby;
                continue;
            }
            if (inRuby) continue;
            int adv = text_wide(cp) ? t->pitch : t->pitch / 2;
            int gx = x + (adv - (text_wide(cp) ? t->fontSize : t->fontSize / 2)) / 2;
            text_glyph(t->screen, t->W, t->H, cp, gx, y, t->fontSize, 0xffffffffu, 0xc0000000u);
            x += adv;
        }
    }
}

void msg_draw(IP *t) {
    if (!t->winOpen || t->winHidden || !t->msgText) return;
    win_load(t);
    blit_part(t->screen, t->W, t->H, win.sheet, 0, 0, win.sheet ? win.sheet->w : 0, WIN_H, WIN_X, WIN_Y, 255);
    if (t->msgName) {
        int px = WIN_X + 15, py = WIN_Y;
        blit_part(t->screen, t->W, t->H, win.sheet, win.plateX, win.plateY, win.plateW, win.plateH, px, py, 255);
        int n;
        u32 *cps = text_decode(t->msgName, &n);
        int w = text_width(cps, n, t->pitch);
        text_line(t->screen, t->W, t->H, cps, n, px + (win.plateW - w) / 2, py + (win.plateH - t->pitch) / 2, t->fontSize, t->pitch, 0xffffffffu, 0xc0000000u);
        free(cps);
    }
    for (int i = 0; i < win.count; i++) {
        WinButton *b = &win.buttons[i];
        int bx = WIN_X + b->x, by = WIN_Y + b->y;
        if (t->mx >= bx && t->mx < bx + b->w && t->my >= by && t->my < by + b->h && t->uiMode == UI_NONE)
            blit_part(t->screen, t->W, t->H, win.sheet, b->sx, b->sy, b->w, b->h, bx, by, 255);
    }
    draw_text(t, WIN_X + win.textX + 1, WIN_Y + win.textY + 5);
}

int msg_button_at(IP *t, int x, int y) {
    if (!t->winOpen || t->winHidden || !t->msgText) return 0;
    win_load(t);
    for (int i = 0; i < win.count; i++) {
        WinButton *b = &win.buttons[i];
        int bx = WIN_X + b->x, by = WIN_Y + b->y;
        if (x >= bx && x < bx + b->w && y >= by && y < by + b->h) return b->id;
    }
    return 0;
}

void msg_reset(void) {
    image_free(win.sheet);
    image_free(win.map);
    image_free(win.click);
    memset(&win, 0, sizeof win);
}

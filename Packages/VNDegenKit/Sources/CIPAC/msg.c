#include "ipac.h"

#define WIN_X 15
#define WIN_Y 396
#define WIN_H 204

static Image *sel;

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
    Image *sheet, *map, *mask, *click;
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
    win.mask = image_load(t, PK_PARTS, "MesWindowB");
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
    free(t->msgRaw);
    t->msgRaw = NULL;
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
    t->msgOff = t->pendOff;
    t->msgLine = t->pendLine;
    snprintf(t->msgVoice, sizeof t->msgVoice, "%s", t->voice);
    log_add(t);
    msg_clear(t);
    t->msgName = t->pendingName[0] ? ip_strdup(t->pendingName) : NULL;
    t->msgText = text_decode(t->pendingText, &t->msgLen);
    t->msgRaw = t->pendingText;
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

static int ms_per_char(IP *t) { return t->textSpeed; }

bool msg_complete(IP *t) { return t->msgShown >= t->msgLen; }

void msg_update(IP *t) {
    if (!t->msgText || msg_complete(t)) return;
    int n = ms_per_char(t) <= 0 ? t->msgLen : (int)((t->now - t->msgStart) / (u32)ms_per_char(t));
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

static bool no_line_start(u32 cp) {
    return cp == 0x3001 || cp == 0x3002 || cp == 0x300d || cp == 0x300f || cp == 0xff09 || cp == 0xff01 || cp == 0xff1f ||
           cp == 0xff0c || cp == 0xff0e || cp == 0x3011;
}

static bool opens_quote(u32 cp) { return cp == 0x300c || cp == 0x300e || cp == 0xff08; }

static int layout(IP *t, int *lineStart, int *lineEnd, int max) {
    int lines = 0, x = 0, start = 0;
    int width = win.textW;
    int indent = t->msgLen && opens_quote(t->msgText[0]) ? t->pitch : 0;
    bool inRuby = false;
    for (int i = 0; i <= t->msgLen && lines < max; i++) {
        u32 cp = i < t->msgLen ? t->msgText[i] : '\n';
        if (cp == '~') continue;
        if (cp == '$') {
            inRuby = !inRuby;
            continue;
        }
        if (inRuby) continue;
        int adv = text_wide(cp) ? t->pitch : t->pitch / 2;
        if (cp == '\n' || (x + adv > width && !no_line_start(cp))) {
            lineStart[lines] = start;
            lineEnd[lines] = i;
            lines++;
            start = cp == '\n' ? i + 1 : i;
            x = indent + (cp == '\n' ? 0 : adv);
            continue;
        }
        x += adv;
    }
    return lines;
}

static void draw_ruby(IP *t, const u32 *r, int n, int x0, int x1, int y) {
    int size = t->fontSize / 2, w = n * size, x = (x0 + x1 - w) / 2;
    for (int i = 0; i < n; i++) text_glyph(t->screen, t->W, t->H, r[i], x + i * size, y, size, 0xffffffffu, 0xc0000000u);
}

static void draw_text(IP *t, int ox, int oy) {
    int ls[16], le[16];
    int lines = layout(t, ls, le, 16);
    int lineH = t->pitch + 8;
    int indent = t->msgLen && opens_quote(t->msgText[0]) ? t->pitch : 0;
    for (int l = 0; l < lines; l++) {
        int x = ox + (l ? indent : 0), y = oy + l * lineH;
        int baseX = x;
        for (int i = ls[l]; i < le[l] && i < t->msgShown; i++) {
            u32 cp = t->msgText[i];
            if (cp == '~') {
                baseX = x;
                continue;
            }
            if (cp == '$') {
                int j = i + 1;
                while (j < t->msgLen && t->msgText[j] != '$') j++;
                draw_ruby(t, t->msgText + i + 1, j - i - 1, baseX, x, y - t->fontSize / 2 - 2);
                i = j;
                continue;
            }
            int adv = text_wide(cp) ? t->pitch : t->pitch / 2;
            text_glyph(t->screen, t->W, t->H, cp, x, y, t->fontSize, 0xffffffffu, 0xc0000000u);
            x += adv;
        }
    }
}

static void blit_masked(IP *t, int sx, int sy, int w, int h, int dx, int dy, int alpha) {
    const Image *im = win.sheet, *m = win.mask;
    if (!im) return;
    for (int y = 0; y < h; y++) {
        int ty = dy + y, yy = sy + y;
        if (ty < 0 || ty >= t->H || yy < 0 || yy >= im->h) continue;
        for (int x = 0; x < w; x++) {
            int tx = dx + x, xx = sx + x;
            if (tx < 0 || tx >= t->W || xx < 0 || xx >= im->w) continue;
            u32 s = im->px[(size_t)yy * im->w + xx];
            u32 a = s >> 24;
            if (!a) continue;
            if (m && xx < m->w && yy < m->h && (m->px[(size_t)yy * m->w + xx] & 0xff) > 0x80) a = a * (u32)alpha / 255;
            u32 *d = &t->screen[(size_t)ty * t->W + tx], ia = 255 - a;
            u32 r = ((s & 0xff) * a + (*d & 0xff) * ia) / 255;
            u32 g = (((s >> 8) & 0xff) * a + ((*d >> 8) & 0xff) * ia) / 255;
            u32 b = (((s >> 16) & 0xff) * a + ((*d >> 16) & 0xff) * ia) / 255;
            *d = r | g << 8 | b << 16 | 0xff000000u;
        }
    }
}

static void outlined(IP *t, const u32 *cps, int n, int x, int y, int size, int pitch) {
    static const int off[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, 1}, {-1, 1}, {1, -1}};
    for (int k = 0; k < 8; k++) text_line(t->screen, t->W, t->H, cps, n, x + off[k][0], y + off[k][1], size, pitch, 0xff000000u, 0);
    text_line(t->screen, t->W, t->H, cps, n, x, y, size, pitch, 0xffffffffu, 0);
}

void msg_draw(IP *t) {
    if (!t->winOpen || t->winHidden || !t->msgText) return;
    win_load(t);
    blit_masked(t, 0, 0, win.sheet ? win.sheet->w : 0, WIN_H, WIN_X, WIN_Y, t->winAlpha);
    if (t->msgName) {
        int px = WIN_X + 15, py = WIN_Y;
        blit_masked(t, win.plateX, win.plateY, win.plateW, win.plateH, px, py, t->winAlpha);
        int n;
        u32 *cps = text_decode(t->msgName, &n);
        int size = t->fontSize + 4, w = text_width(cps, n, t->pitch) - (t->pitch - size);
        outlined(t, cps, n, px + (win.plateW - w) / 2, py + (win.plateH - size) / 2 - 2, size, t->pitch);
        free(cps);
    }
    for (int i = 0; i < win.count; i++) {
        WinButton *b = &win.buttons[i];
        int bx = WIN_X + b->x, by = WIN_Y + b->y;
        if (t->mx >= bx && t->mx < bx + b->w && t->my >= by && t->my < by + b->h && t->uiMode == UI_NONE)
            blit_part(t->screen, t->W, t->H, win.sheet, b->sx, b->sy, b->w, b->h, bx, by, 255);
    }
    draw_text(t, WIN_X + win.textX + 1, WIN_Y + win.textY + 6);
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
    image_free(sel);
    sel = NULL;
    image_free(win.sheet);
    image_free(win.map);
    image_free(win.click);
    image_free(win.mask);
    memset(&win, 0, sizeof win);
}

#define SEL_X 136
#define SEL_Y 156
#define SEL_ROW 28
#define SEL_TOP 12
#define SEL_BOTTOM 6


static int choice_at(IP *t, int x, int y) {
    if (x < SEL_X + 14 || x >= SEL_X + 486) return -1;
    int r = (y - SEL_Y - SEL_TOP);
    if (r < 0) return -1;
    r /= SEL_ROW;
    return r < t->choiceCount ? r : -1;
}

void choice_update(IP *t) {
    int h = choice_at(t, t->mx, t->my);
    if (h != t->choiceHover) {
        t->choiceHover = h;
        t->dirty = true;
    }
}

void choice_draw(IP *t) {
    if (t->wait != W_CHOICE || !t->choiceCount) return;
    if (!sel) sel = image_load(t, PK_PARTS, "SelWindow");
    int h = SEL_TOP + SEL_ROW * t->choiceCount + SEL_BOTTOM;
    if (sel) {
        int top = h - SEL_BOTTOM > sel->h - SEL_BOTTOM ? sel->h - SEL_BOTTOM : h - SEL_BOTTOM;
        blit_part(t->screen, t->W, t->H, sel, 0, 0, sel->w, top, SEL_X, SEL_Y, 255);
        for (int y = top; y < h - SEL_BOTTOM; y += 8)
            blit_part(t->screen, t->W, t->H, sel, 0, 40, sel->w, (h - SEL_BOTTOM - y) < 8 ? h - SEL_BOTTOM - y : 8, SEL_X, SEL_Y + y, 255);
        blit_part(t->screen, t->W, t->H, sel, 0, 136, sel->w, SEL_BOTTOM, SEL_X, SEL_Y + h - SEL_BOTTOM, 255);
    }
    for (int i = 0; i < t->choiceCount; i++) {
        int ry = SEL_Y + SEL_TOP + i * SEL_ROW;
        if (i == t->choiceHover) fill_rect(t->screen, t->W, t->H, SEL_X + 14, ry, 472, SEL_ROW, 0xff5a49c7u);
        int n;
        u32 *cps = text_decode(t->choices[i].text, &n);
        text_line(t->screen, t->W, t->H, cps, n, SEL_X + 15, ry + 6, t->fontSize, t->pitch, 0xffffffffu, 0xc0000000u);
        free(cps);
    }
}

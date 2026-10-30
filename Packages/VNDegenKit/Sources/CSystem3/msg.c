// Message windows and choice menus.
#include "s3.h"

// Half-width katakana as the System 3.5 compiler stores hiragana, from U+FF66 (ｦ) to U+FF9D (ﾝ).
static const u16 kanaToHira[] = {
    0x3092, 0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x3083, 0x3085, 0x3087, 0x3063, // ｦｧｨｩｪｫｬｭｮｯ
    0x30fc, 0x3042, 0x3044, 0x3046, 0x3048, 0x304a, 0x304b, 0x304d, 0x304f, 0x3051, // ｰｱｲｳｴｵｶｷｸｹ
    0x3053, 0x3055, 0x3057, 0x3059, 0x305b, 0x305d, 0x305f, 0x3061, 0x3064, 0x3066, // ｺｻｼｽｾｿﾀﾁﾂﾃ
    0x3068, 0x306a, 0x306b, 0x306c, 0x306d, 0x306e, 0x306f, 0x3072, 0x3075, 0x3078, // ﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍ
    0x307b, 0x307e, 0x307f, 0x3080, 0x3081, 0x3082, 0x3084, 0x3086, 0x3088, 0x3089, // ﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗ
    0x308a, 0x308b, 0x308c, 0x308d, 0x308f, 0x3093,                                 // ﾘﾙﾚﾛﾜﾝ
};
static const u16 halfPunct[] = { 0x3002, 0x300c, 0x300d, 0x3001, 0x30fb }; // ｡｢｣､･

static S3Win *msgwin(S3 *s) { return &s->msgWin[s->curMsg > 0 && s->curMsg <= 100 ? s->curMsg : 1]; }

static int line_height(S3 *s) { return s->fontSize + s->lineSpace; }

void s3_msg_newline(S3 *s) {
    S3Win *w = msgwin(s);
    s->tx = w->x;
    s->ty += line_height(s);
}

void s3_msg_clear(S3 *s) {
    S3Win *w = msgwin(s);
    if (s->msgBgAlpha == 0) s3_fill(s, w->x, w->y, w->w, w->h, s->msgBg);
    else if (w->save && s->menuSave) {} // a transparent window keeps what is behind it
    s->tx = w->x;
    s->ty = w->y;
}

// Appends text to the menu item being built.
static void menu_append(S3 *s, const char *sjis) {
    S3MenuItem *m = &s->menu[s->menuBuild];
    m->zh = s->zh;
    size_t have = strlen(m->text), add = strlen(sjis);
    if (have + add >= sizeof m->text) add = sizeof m->text - 1 - have;
    memcpy(m->text + have, sjis, add);
    m->text[have + add] = 0;
}

// Converts one character for display under the current ZH mode. Returns the code point and
// whether it takes a half-width cell.
static int next_char(S3 *s, const u8 *p, u32 *cp, bool *half) {
    int n = s3_sjis_decode(p, cp);
    *half = n == 1;
    if (s->zh == 0 && n == 1) {
        u32 c = *cp;
        if (c >= 0xff66 && c <= 0xff9d) {
            u32 h = kanaToHira[c - 0xff66];
            // A following voiced or semi-voiced mark joins the kana.
            u32 next = 0;
            if (p[1]) s3_sjis_decode(p + 1, &next);
            if (next == 0xff9e && ((h >= 0x304b && h <= 0x3062) || (h >= 0x3064 && h <= 0x3069) || (h >= 0x306f && h <= 0x307d))) { h += 1; n++; }
            else if (next == 0xff9e && h == 0x3046) { h = 0x3094; n++; }
            else if (next == 0xff9f && h >= 0x306f && h <= 0x307d) { h += 2; n++; }
            *cp = h;
        } else if (c >= 0xff61 && c <= 0xff65) {
            *cp = halfPunct[c - 0xff61];
        } else if (c == 0xff9e) { *cp = 0x309b; } else if (c == 0xff9f) { *cp = 0x309c; }
        else if (c == ' ') *cp = 0x3000;
        else if (c > ' ' && c < 0x7f) *cp = c + 0xfee0;
        *half = false;
    }
    return n;
}

void s3_msg_text(S3 *s, const char *sjis) {
    if (s->menuBuild >= 0) { menu_append(s, sjis); return; }
    S3Win *w = msgwin(s);
    const u8 *p = (const u8 *)sjis;
    while (*p) {
        // A half-width word that would not fit starts a new line, so translated text breaks between words.
        if (*p > ' ' && *p < 0x7f && s->zh != 0 && (p == (const u8 *)sjis || p[-1] == ' ')) {
            int word = 0;
            for (const u8 *q = p; *q > ' ' && *q < 0x7f; q++) word += s->fontSize / 2;
            if (s->tx + word > w->x + w->w && s->tx > w->x && word <= w->w) s3_msg_newline(s);
        }
        u32 cp; bool half;
        p += next_char(s, p, &cp, &half);
        int adv = half ? s->fontSize / 2 : s->fontSize;
        if (s->tx + adv > w->x + w->w && s->tx > w->x) s3_msg_newline(s);
        s3_draw_char(s, cp, half, s->tx, s->ty, s->fontSize, s->msgColor);
        s->tx += adv;
    }
}

void s3_msg_select_window(S3 *s, int n, int frame, int noClear, int restore) {
    if (n < 1 || n > 100) return;
    s->curMsg = n;
    s->msgFrameOn = frame;
    S3Win *w = msgwin(s);
    (void)restore;
    if (!noClear) s3_msg_clear(s);
    s->tx = w->x;
    s->ty = w->y;
}

// ---- menus ----

static int menu_row_height(S3 *s) { return s->menuFontSize + 4; }

static int text_width(S3 *s, const char *sjis, int size) {
    int wpx = 0;
    const u8 *p = (const u8 *)sjis;
    while (*p) {
        u32 cp; bool half;
        p += next_char(s, p, &cp, &half);
        wpx += half ? size / 2 : size;
    }
    return wpx;
}

static void menu_rect(S3 *s, int *x, int *y, int *w, int *h) {
    S3Win *win = &s->selWin[s->curSel > 0 && s->curSel <= 100 ? s->curSel : 1];
    *x = win->x; *y = win->y; *w = win->w; *h = win->h;
    int need = 0, zh = s->zh;
    for (int i = 0; i < s->menuCount; i++) {
        s->zh = s->menu[i].zh;
        int tw = text_width(s, s->menu[i].text, s->menuFontSize);
        if (tw > need) need = tw;
    }
    s->zh = zh;
    if (*w < need + 16) *w = need + 16;
    *h = s->menuCount * menu_row_height(s) + 8;
    // A box that grew past the right of the screen, as translated text can make it, moves left.
    int right = s->viewX + (s->viewW > 0 ? s->viewW : s->dib.w);
    if (*x + *w > right) *x = right - *w > 0 ? right - *w : 0;
}

static void menu_draw(S3 *s) {
    int x, y, w, h;
    menu_rect(s, &x, &y, &w, &h);
    if (s->menuBgAlpha == 0) s3_fill(s, x, y, w, h, s->menuBg);
    if (s->selFrameOn) {
        s3_fill(s, x, y, w, 1, s->menuFrame); s3_fill(s, x, y + h - 1, w, 1, s->menuFrame);
        s3_fill(s, x, y, 1, h, s->menuFrame); s3_fill(s, x + w - 1, y, 1, h, s->menuFrame);
    }
    for (int i = 0; i < s->menuCount; i++) {
        int ry = y + 4 + i * menu_row_height(s);
        bool sel = i == s->menuCursorPos;
        int color = sel && s->menuCursor == 2 ? s->menuBg : s->menuColor;
        if (sel && s->menuCursor == 2) s3_fill(s, x + 4, ry, w - 8, menu_row_height(s) - 2, s->menuColor);
        int tx = x + 8, zh = s->zh;
        s->zh = s->menu[i].zh;
        const u8 *p = (const u8 *)s->menu[i].text;
        while (*p) {
            u32 cp; bool half;
            p += next_char(s, p, &cp, &half);
            tx += s3_draw_char(s, cp, half, tx, ry + 1, s->menuFontSize, color);
        }
        s->zh = zh;
        if (sel && s->menuCursor != 2) {
            int t = s->menuCursor == 1 ? 1 : 2;
            int rw = w - 8, rh = menu_row_height(s) - 1;
            s3_fill(s, x + 4, ry - 1, rw, t, s->menuColor); s3_fill(s, x + 4, ry - 1 + rh - t, rw, t, s->menuColor);
            s3_fill(s, x + 4, ry - 1, t, rh, s->menuColor); s3_fill(s, x + 4 + rw - t, ry - 1, t, rh, s->menuColor);
        }
    }
}

void s3_menu_open(S3 *s) {
    int x, y, w, h;
    menu_rect(s, &x, &y, &w, &h);
    if (!s3_clip(s, &x, &y, &w, &h)) { w = h = 0; }
    free(s->menuSave);
    s->menuSave = NULL;
    s->menuSaveX = x; s->menuSaveY = y; s->menuSaveW = w; s->menuSaveH = h;
    if (w > 0 && h > 0) {
        size_t px = s->dib.depth == 8 ? 1 : 4;
        s->menuSave = s3_alloc((size_t)w * h * px);
        for (int j = 0; j < h; j++) {
            size_t row = (size_t)(y + j) * s->dib.w + x;
            if (px == 1) memcpy(s->menuSave + (size_t)j * w, s->dib.p8 + row, (size_t)w);
            else memcpy(s->menuSave + (size_t)j * w * 4, s->dib.p32 + row, (size_t)w * 4);
        }
    }
    int d = s->menuDefault;
    if (d >= 1 && d <= s->menuCount) s->menuCursorPos = d - 1;
    else if (d > s->menuCount && d < 1000) s->menuCursorPos = s->menuCount - 1;
    else s->menuCursorPos = 0;
    menu_draw(s);
}

static void menu_restore(S3 *s) {
    if (!s->menuSave) return;
    int x = s->menuSaveX, y = s->menuSaveY, w = s->menuSaveW, h = s->menuSaveH;
    size_t px = s->dib.depth == 8 ? 1 : 4;
    for (int j = 0; j < h; j++) {
        size_t row = (size_t)(y + j) * s->dib.w + x;
        if (px == 1) memcpy(s->dib.p8 + row, s->menuSave + (size_t)j * w, (size_t)w);
        else memcpy(s->dib.p32 + row, s->menuSave + (size_t)j * w * 4, (size_t)w * 4);
    }
    free(s->menuSave);
    s->menuSave = NULL;
    s3_mark_dirty(s);
}

// Returns the item under the mouse, or -1.
static int menu_hit(S3 *s) {
    int x, y, w, h;
    menu_rect(s, &x, &y, &w, &h);
    int mx = s->mouseX + s->viewX, my = s->mouseY + s->viewY;
    if (mx < x || mx >= x + w || my < y + 4) return -1;
    int i = (my - y - 4) / menu_row_height(s);
    return i < s->menuCount ? i : -1;
}

// Polls input for the open menu. Returns true once the menu is done; waitArg[0] then holds the
// chosen item or -1 for cancel.
bool s3_menu_update(S3 *s) {
    u32 pressed = s->pressedMask;
    s->pressedMask = 0;
    int old = s->menuCursorPos;
    static int lastMx = -1, lastMy = -1;
    if (s->mouseX != lastMx || s->mouseY != lastMy) {
        lastMx = s->mouseX; lastMy = s->mouseY;
        int hit = menu_hit(s);
        if (hit >= 0) s->menuCursorPos = hit;
    }
    if (pressed & 1 && s->menuCursorPos > 0) s->menuCursorPos--;          // up
    if (pressed & 2 && s->menuCursorPos < s->menuCount - 1) s->menuCursorPos++; // down
    if (old != s->menuCursorPos) menu_draw(s);
    if (pressed & 16) {                                                      // return or left click
        bool click = s->waitArg[1];
        int hit = menu_hit(s);
        (void)click;
        if (hit >= 0 || !(pressed & 0x10000)) { s->waitArg[0] = hit >= 0 && (pressed & 0x10000) ? hit : s->menuCursorPos; return true; }
    }
    if (pressed & 32) { s->waitArg[0] = -1; return true; }                  // escape or right click
    return false;
}

void s3_menu_close(S3 *s, int choice) {
    (void)choice;
    menu_restore(s);
}

char *s3_display_utf8(S3 *s, const char *sjis) {
    size_t n = strlen(sjis);
    char *out = s3_alloc(n * 3 + 1), *o = out;
    const u8 *p = (const u8 *)sjis;
    while (*p) {
        u32 cp;
        bool half;
        p += next_char(s, p, &cp, &half);
        if (cp < 0x80) *o++ = (char)cp;
        else if (cp < 0x800) { *o++ = (char)(0xc0 | cp >> 6); *o++ = (char)(0x80 | (cp & 0x3f)); }
        else { *o++ = (char)(0xe0 | cp >> 12); *o++ = (char)(0x80 | ((cp >> 6) & 0x3f)); *o++ = (char)(0x80 | (cp & 0x3f)); }
    }
    *o = 0;
    return out;
}

// Command handlers. Arguments are decoded with the signatures learned from the SDK compilers:
// e = expression, b = raw byte, s = NUL-terminated string, f = function reference, m = u32.
// Behaviour follows AliceSoft's System 3.9 SDK manual, checked against the original engine.
#include "vm.h"
#include <strings.h>
#include <time.h>

typedef struct Arg { u16 v; S3LV lv; bool isVar; const char *s; u32 u; u16 fpage; } Arg;

// Reads one expression argument, keeping the variable when it is a lone variable reference.
static void earg(S3 *s, Arg *a) {
    u32 at = s->pc;
    int c = s->pc < s->codeSize ? s->code[s->pc] : 0x7f;
    a->isVar = false;
    if (c >= 0x80) {
        s3_lvarg(s, &a->lv);
        if (!s->vmError) { a->isVar = true; a->v = s3_lv_get(s, &a->lv, 0); return; }
        s->vmError = false;
        s->pc = at;
    }
    a->v = s3_expr(s);
}

static void args(S3 *s, const char *sig, Arg *a) {
    for (int i = 0; sig[i]; i++) {
        Arg *x = &a[i];
        memset(x, 0, sizeof *x);
        switch (sig[i]) {
        case 'e': earg(s, x); break;
        case 'b': x->v = (u16)s3_byte(s); break;
        case 's': x->s = s3_strarg(s); break;
        case 'm': x->u = s3_u32(s); break;
        case 'f': x->fpage = s3_u16(s); x->u = s3_u32(s); break;
        case ':': { // old-style inline text ending with ':'
            x->s = (const char *)s->code + s->pc;
            while (s->pc < s->codeSize && s->code[s->pc] != ':') {
                s->pc += s3_sjis_lead(s->code[s->pc]) ? 2 : 1;
            }
            s->pc++;
            break;
        }
        }
    }
}

// Writes to a variable argument and the ones after it.
static void put(S3 *s, const Arg *a, int k, u16 v) {
    if (a->isVar) s3_lv_set(s, &a->lv, k, v);
}

#define RND 0
static void rnd(S3 *s, u16 v) { s3_var_set(s, RND, v); }

static void unimpl(S3 *s, const char *name) {
    static char seen[64][16];
    static int n;
    for (int i = 0; i < n; i++) if (!strcmp(seen[i], name)) return;
    if (n < 64) snprintf(seen[n++], 16, "%s", name);
    s3_log(s, "not implemented: %s (page %d at 0x%x)", name, s->page, s->cmdStart);
}

// ---- strings ----

static char *str_slot(S3 *s, int n) {
    if (n < 1 || n >= S3_MAX_STRINGS) return NULL;
    if (!s->str[n]) s->str[n] = s3_alloc((size_t)s->strLen + 1);
    return s->str[n];
}

static void str_set(S3 *s, int n, const char *v) {
    char *d = str_slot(s, n);
    if (!d) return;
    snprintf(d, (size_t)s->strLen + 1, "%s", v);
}

static int sjis_chars(const char *p) {
    int n = 0;
    for (const u8 *q = (const u8 *)p; *q; n++) q += s3_sjis_lead(*q) && q[1] ? 2 : 1;
    return n;
}

// Number to text: full-width digits unless half is set, padded to fig digits when fig > 0.
static void num_text(char *out, size_t size, int fig, unsigned v, bool half) {
    char d[16];
    snprintf(d, sizeof d, "%u", v);
    if (fig > 0) {
        size_t n = strlen(d);
        if ((int)n > fig) memmove(d, d + n - (size_t)fig, (size_t)fig + 1);
        else { char t[16]; snprintf(t, sizeof t, "%*s", fig, d); memcpy(d, t, sizeof d); }
    }
    size_t o = 0;
    for (const char *p = d; *p && o + 3 < size; p++) {
        if (half) out[o++] = *p;
        else if (*p == ' ') { out[o++] = (char)0x81; out[o++] = 0x40; }
        else { out[o++] = (char)0x82; out[o++] = (char)(0x4f + (*p - '0')); }
    }
    out[o] = 0;
}

// ---- waits ----

static u32 keymask_now(S3 *s) {
    u32 m = 0;
    if (s->keyDown[0x26] || s->keyDown[0x68]) m |= 1;
    if (s->keyDown[0x28] || s->keyDown[0x62]) m |= 2;
    if (s->keyDown[0x25] || s->keyDown[0x64]) m |= 4;
    if (s->keyDown[0x27] || s->keyDown[0x66]) m |= 8;
    if (s->keyDown[0x0d] || s->mouseL) m |= 16;
    if (s->keyDown[0x20] || s->mouseR) m |= 32;
    if (s->keyDown[0x1b]) m |= 64;
    if (s->keyDown[0x09]) m |= 128;
    return m;
}

void s3_cmd_wait_key(S3 *s) {
    s->pressedMask = 0;
    s->wait = S3_WAIT_KEY;
}

void s3_cmd_open_menu(S3 *s) {
    s->menuBuild = -1;
    if (s->menuCount == 0) return;
    s3_menu_open(s);
    s->pressedMask = 0;
    s->wait = S3_WAIT_MENU;
}

static void wait_ms(S3 *s, u32 ms, bool keyCancels) {
    s->wait = S3_WAIT_TIME;
    s->waitUntil = s->clockMs + ms;
    s->waitArg[0] = keyCancels;
    s->waitArg[1] = 0;
    s->pressedMask = 0;
}

static void start_fade(S3 *s, bool in, bool white, int frames16, bool keyCancels) {
    memcpy(s->shownPal, s->pal, sizeof s->pal); // a fade shows the palette being drawn with
    s->fadeWhite = white;
    s->wait = S3_WAIT_FADE;
    s->waitArg[0] = in;
    s->waitArg[1] = keyCancels;
    s->waitArg[2] = (int)s->clockMs;
    s->waitArg[3] = frames16 * 16 * 1000 / 60; // total duration in ms
    s->pressedMask = 0;
    if (s->waitArg[3] <= 0) { s->fadeLevel = in ? 0 : 255; s->wait = S3_WAIT_NONE; s3_mark_dirty(s); }
}

// Called every frame while a wait is active.
void s3_service_wait(S3 *s) {
    switch (s->wait) {
    case S3_WAIT_KEY: {
        u32 p = s->pressedMask;
        s->pressedMask = 0;
        if (p & (16 | 32)) {
            s->wait = S3_WAIT_NONE;
            s3_msg_clear(s);
        }
        break;
    }
    case S3_WAIT_MENU:
        if (s3_menu_update(s)) {
            int choice = s->waitArg[0];
            if (s->trace) s3_log(s, "menu choice %d of %d", choice, s->menuCount);
            s3_menu_close(s, choice);
            s->wait = S3_WAIT_NONE;
            if (choice >= 0) {
                S3MenuItem *m = &s->menu[choice];
                rnd(s, (u16)(choice + 1));
                if (s->msgClearOnSelect) s3_msg_clear(s);
                if (m->page == s->page) s->pc = m->addr;
                else s3_goto_page(s, m->page, m->addr);
            }
            s->menuCount = 0;
        }
        break;
    case S3_WAIT_TIME:
        // Keys pressed during the wait reach RND whether or not they may end it (Y3, ZT20, ZT21).
        s->waitArg[1] |= (int)(s->pressedMask & 0xff);
        s->pressedMask = 0;
        if ((s->waitArg[0] && s->waitArg[1]) || s->clockMs >= s->waitUntil) {
            rnd(s, (u16)s->waitArg[1]);
            s->wait = S3_WAIT_NONE;
        }
        break;
    case S3_WAIT_FADE: {
        int elapsed = (int)(s->clockMs - (u64)(u32)s->waitArg[2]);
        int total = s->waitArg[3];
        bool done = elapsed >= total || (s->waitArg[1] && s->pressedMask);
        int level = done ? 255 : elapsed * 255 / total;
        // Fades move in 16 palette steps on the original engine.
        level = done ? 255 : (level / 16) * 16;
        s->fadeLevel = s->waitArg[0] ? 255 - level : level;
        s3_mark_dirty(s);
        if (done) {
            if (s->waitArg[1]) rnd(s, (u16)s->pressedMask);
            s->wait = S3_WAIT_NONE;
        }
        break;
    }
    case S3_WAIT_INPUT_POLL: {
        u32 p = s->pressedMask | keymask_now(s);
        if (p) { rnd(s, (u16)p); s->pressedMask = 0; s->wait = S3_WAIT_NONE; }
        break;
    }
    case S3_WAIT_EFFECT:
        if (s->clockMs >= s->waitUntil || (s->waitArg[0] && s->pressedMask)) {
            if (s->waitArg[0]) rnd(s, (u16)s->pressedMask);
            s->wait = S3_WAIT_NONE;
        }
        break;
    default:
        break;
    }
}

// ---- CG loading ----

void s3_load_cg(S3 *s, int n, int key, int yOffset) {
    S3Blob b;
    bool found = s->sys30 ? s3_dat_get(&s->cg30, (u32)n, &b) : s3_archive_get(&s->ga, (u32)n, &b, NULL);
    if (!found) { s3_log(s, "CG %d missing", n); s->cgPos = false; return; }
    S3Image img;
    if (!s3_cg_decode(b.data, b.size, &img)) { s3_log(s, "CG %d undecodable", n); s->cgPos = false; return; }
    // A 16-colour VSP whose header names bank 0 goes to the bank ZC 0 chose (1 at start), so it leaves the colours
    // the game set in bank 0 alone.
    if (img.vsp && img.palBank == 0 && s->palBank > 0 && s->palBank < 16) {
        int shift = s->palBank * 16;
        for (int i = 0; i < img.w * img.h; i++) img.pixels8[i] = (u8)(img.pixels8[i] + shift);
        memcpy(img.pal[shift], img.pal[0], 48);
        img.palMask = (u16)(1u << s->palBank);
        if (key >= 0 && key < 16) key += shift;
    }
    // A 16-colour VSP's transparent colour counts within its own bank.
    if (img.vsp && img.palBank > 0 && key >= 0 && key < 16) key += (img.palBank & 15) * 16;
    int x = img.x, y = img.y;
    if (s->cgPos) {
        if (s->cgPosMode == 0 || s->cgPosMode == 2 || s->cgPosMode == 30) { x = s->cgX; y = s->cgY; }
        else { x += s->cgX; y += s->cgY; }
        s->cgPos = false;
    }
    y += yOffset;
    if (s->trace) s3_log(s, "CG %d at %d,%d %dx%d bpp %d mask %04x pc %d", n, x, y, img.w, img.h, img.bpp, img.palMask, s->pcMode);
    if (img.hasPal && (s->pcMode & 4) && n != s->noPalCg) {
        for (int bank = 0; bank < 16; bank++) {
            if (!(img.palMask & (1u << bank))) continue;
            memcpy(s->pal[bank * 16], img.pal[bank * 16], 48);
        }
    }
    // Palette expansion copies the whole palette to the screen, including entries PS set.
    if (s->pcMode & 2) memcpy(s->shownPal, s->pal, sizeof s->pal);
    if (s->pcMode & 1) s3_draw_image(s, &img, x, y, key);
    s3_mark_dirty(s);
    s->lastCg = n;
    if (s->cgCountVar.on) {
        S3LV lv = { .var = s->cgCountVar.pointer, .offset = n, .element = true };
        u16 *c = s3_lv_at(s, &lv, 0);
        if (*c < 65535) (*c)++;
    }
    s3_image_free(&img);
}

// ---- effects ----

static void effect_copy(S3 *s, const Arg *a) {
    int sx = a[0].v, sy = a[1].v, w = a[2].v, h = a[3].v, dx = a[4].v, dy = a[5].v;
    int effect = a[6].v, opt = a[7].v, keyFlag = a[8].v;
    s3_copy(s, sx, sy, w, h, dx, dy);
    // Transitions run about a second at default speed on the original; until each pattern is
    // drawn, show the result after the same delay.
    int ms = effect >= 2000 ? 0 : opt ? opt * 16 : 500;
    if (ms > 0) {
        s->wait = S3_WAIT_EFFECT;
        s->waitUntil = s->clockMs + (u64)ms;
        s->waitArg[0] = keyFlag;
        s->pressedMask = 0;
    }
}

// ---- the dispatcher ----

static void cmd_B(S3 *s) {
    int sub = s3_byte(s);
    Arg a[8];
    switch (sub) {
    case 0: args(s, "e", a); if (a[0].v == 0) s3_msg_clear(s); else { s->tx = s->msgWin[s->curMsg].x; s->ty = s->msgWin[s->curMsg].y; } return;
    case 1: case 3: {
        args(s, "eeeeee", a);
        int n = a[0].v;
        if (n < 1 || n > 100) return;
        S3Win *w = sub == 1 ? &s->selWin[n] : &s->msgWin[n];
        w->x = a[1].v; w->y = a[2].v; w->w = a[3].v; w->h = a[4].v; w->save = a[5].v;
        return;
    }
    case 2: args(s, "eeeeee", a); if (a[0].v >= 1 && a[0].v <= 100) { s->curSel = a[0].v; s->selFrameOn = a[1].v; } return;
    case 4: args(s, "eeeeee", a); s3_msg_select_window(s, a[0].v, a[1].v, a[4].v, a[5].v); return;
    case 10: args(s, "ee", a); put(s, &a[0], 0, (u16)s->tx); put(s, &a[1], 0, (u16)s->ty); return;
    case 11: args(s, "ee", a); put(s, &a[0], 0, (u16)s->curSel); put(s, &a[1], 0, (u16)s->curMsg); return;
    case 12: args(s, "e", a); put(s, &a[0], 0, (u16)s->menuCount); return;
    case 13: case 14: {
        args(s, "e", a);
        int m = 0;
        for (int i = 0; i < s->menuCount; i++) {
            int n = sub == 13 ? sjis_chars(s->menu[i].text) : (int)strlen(s->menu[i].text);
            if (n > m) m = n;
        }
        put(s, &a[0], 0, (u16)m);
        return;
    }
    case 21: case 22: case 23: case 24: case 31: case 32: case 33: case 34: {
        args(s, "eee", a);
        int n = a[0].v;
        if (n < 1 || n > 100) n = 1;
        S3Win *w = (sub == 21 || sub == 22 || sub == 31 || sub == 32) ? &s->selWin[sub < 30 ? s->curSel : n] : &s->msgWin[sub < 30 ? s->curMsg : n];
        bool size = sub % 2 == 0;
        put(s, &a[1], 0, (u16)(size ? w->w : w->x));
        put(s, &a[2], 0, (u16)(size ? w->h : w->y));
        return;
    }
    default: s->vmError = true;
    }
}

static void cmd_C(S3 *s) {
    int k = s3_byte(s);
    Arg a[10];
    switch (k) {
    case 'B': args(s, "eeeee", a);
        s3_fill(s, a[0].v, a[1].v, a[2].v, 1, a[4].v); s3_fill(s, a[0].v, a[1].v + a[3].v - 1, a[2].v, 1, a[4].v);
        s3_fill(s, a[0].v, a[1].v, 1, a[3].v, a[4].v); s3_fill(s, a[0].v + a[2].v - 1, a[1].v, 1, a[3].v, a[4].v);
        return;
    case 'C': args(s, "eeeeee", a); s3_copy(s, a[0].v, a[1].v, a[2].v, a[3].v, a[4].v, a[5].v); return;
    case 'D': args(s, "eeeeeeeeee", a); unimpl(s, "CD"); return;
    case 'E': args(s, "eeeeeeeee", a); effect_copy(s, a); return;
    case 'F': args(s, "eeeee", a); s3_fill(s, a[0].v, a[1].v, a[2].v, a[3].v, a[4].v); return;
    case 'K': {
        args(s, "beeeeeeee", a);
        int x = a[1].v, y = a[2].v, w = a[3].v, h = a[4].v;
        if (a[0].v == 3 && s->dib.depth == 8 && s3_clip(s, &x, &y, &w, &h)) {
            int dst = a[5].v, src = a[6].v, cnt = a[7].v;
            for (int j = 0; j < h; j++) {
                u8 *row = s->dib.p8 + (size_t)(y + j) * s->dib.w + x;
                for (int i = 0; i < w; i++) if (row[i] >= src && row[i] < src + cnt) row[i] = (u8)(row[i] - src + dst);
            }
            s3_mark_dirty(s);
        } else if (a[0].v == 1 && s->dib.depth != 8 && s3_clip(s, &x, &y, &w, &h)) {
            u32 c = s3_rgb(s, a[5].v);
            int rate = a[6].v;
            for (int j = 0; j < h; j++) {
                u32 *row = s->dib.p32 + (size_t)(y + j) * s->dib.w + x;
                for (int i = 0; i < w; i++) {
                    u32 d = row[i];
                    int r = (d >> 16) & 255, g = (d >> 8) & 255, b = d & 255;
                    r += (((c >> 16) & 255) - r) * rate / 255; g += (((c >> 8) & 255) - g) * rate / 255; b += ((c & 255) - b) * rate / 255;
                    row[i] = (u32)r << 16 | (u32)g << 8 | (u32)b;
                }
            }
            s3_mark_dirty(s);
        } else unimpl(s, "CK");
        return;
    }
    case 'L': args(s, "eeeee", a); {
        int x0 = a[0].v, y0 = a[1].v, x1 = a[2].v, y1 = a[3].v;
        int dx = abs(x1 - x0), dy = -abs(y1 - y0), sxs = x0 < x1 ? 1 : -1, sys = y0 < y1 ? 1 : -1, err = dx + dy;
        for (;;) {
            s3_fill(s, x0, y0, 1, 1, a[4].v);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sxs; }
            if (e2 <= dx) { err += dx; y0 += sys; }
        }
    } return;
    case 'M': {
        args(s, "eeeeeeeee", a);
        int sx = a[0].v, sy = a[1].v, sw = a[2].v, sh = a[3].v, dx = a[4].v, dy = a[5].v, dw = a[6].v, dh = a[7].v, m = a[8].v;
        if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
        size_t px = s->dib.depth == 8 ? 1 : 4;
        u8 *tmp = s3_alloc((size_t)dw * dh * px);
        for (int j = 0; j < dh; j++) for (int i = 0; i < dw; i++) {
            int si = sx + (m & 2 ? dw - 1 - i : i) * sw / dw, sj = sy + (m & 1 ? dh - 1 - j : j) * sh / dh;
            if (si < 0 || sj < 0 || si >= s->dib.w || sj >= s->dib.h) continue;
            if (px == 1) tmp[j * dw + i] = s->dib.p8[(size_t)sj * s->dib.w + si];
            else ((u32 *)tmp)[j * dw + i] = s->dib.p32[(size_t)sj * s->dib.w + si];
        }
        for (int j = 0; j < dh; j++) for (int i = 0; i < dw; i++) {
            int x = dx + i, y = dy + j;
            if (x < 0 || y < 0 || x >= s->dib.w || y >= s->dib.h) continue;
            if (px == 1) s->dib.p8[(size_t)y * s->dib.w + x] = tmp[j * dw + i];
            else s->dib.p32[(size_t)y * s->dib.w + x] = ((u32 *)tmp)[j * dw + i];
        }
        free(tmp);
        s3_mark_dirty(s);
        return;
    }
    case 'P': args(s, "eee", a); unimpl(s, "CP"); return;
    case 'S': args(s, "eeeeeee", a); s3_copy_key(s, a[0].v, a[1].v, a[2].v, a[3].v, a[4].v, a[5].v, a[6].v); return;
    case 'T': args(s, "eee", a); unimpl(s, "CT"); return;
    case 'U': case 'V': args(s, "eeeeee", a); unimpl(s, "CU/CV"); return;
    case 'X': args(s, "eeeeeeee", a);
        if (a[0].v == 1) s3_copy_key(s, a[1].v, a[2].v, a[3].v, a[4].v, a[5].v, a[6].v, a[7].v);
        else { s3_copy(s, a[1].v, a[2].v, a[3].v, a[4].v, a[5].v, a[6].v); unimpl(s, "CX shadow"); }
        return;
    case 'Y': args(s, "eeeee", a); unimpl(s, "CY"); return;
    case 'Z': args(s, "eeeeeee", a); unimpl(s, "CZ"); return;
    default: s->vmError = true;
    }
}

static void cmd_D(S3 *s) {
    int k = s3_byte(s);
    Arg a[4];
    switch (k) {
    case 'C': {
        args(s, "eee", a);
        int p = a[0].v, n = a[1].v;
        if (p == 0) return; // page 0 is the normal variables, always 65536 here
        S3ArrayPage *ap = &s->arr[p & 255];
        u16 *v = s3_alloc(((size_t)n + 1) * sizeof(u16));
        if (ap->v) memcpy(v, ap->v, (ap->n < (u32)n + 1 ? ap->n : (u32)n + 1) * sizeof(u16));
        free(ap->v);
        ap->v = v; ap->n = (u32)n + 1; ap->save = a[2].v != 0;
        return;
    }
    case 'F': {
        args(s, "eee", a);
        if (!a[0].isVar) return;
        for (int i = 0; i < a[1].v; i++) s3_lv_set(s, &a[0].lv, i, a[2].v);
        return;
    }
    case 'I': {
        args(s, "eee", a);
        int p = a[0].v & 255;
        put(s, &a[1], 0, s->arr[p].v != NULL);
        put(s, &a[2], 0, (u16)s->arr[p].n);
        return;
    }
    case 'R': args(s, "e", a); if (a[0].isVar) s->bind[a[0].lv.var & 0x3fff].on = false; return;
    case 'S': {
        args(s, "eeee", a);
        if (!a[0].isVar || !a[1].isVar) return;
        S3Bind *b = &s->bind[a[1].lv.var & 0x3fff];
        b->on = true; b->pointer = a[0].lv.var; b->base = a[2].v; b->page = (u8)a[3].v;
        return;
    }
    default: s->vmError = true;
    }
}

static void cmd_E(S3 *s) {
    int k = s3_byte(s);
    Arg a[6];
    switch (k) {
    case 'C': {
        args(s, "e", a);
        int n = a[0].v;
        if (n == 0) s3_fill(s, s->viewX, s->viewY, s->viewW, s->viewH, 0);
        else if (n <= 100) s3_fill(s, s->clr[n].x, s->clr[n].y, s->clr[n].w, s->clr[n].h, s->clrColor[n]);
        return;
    }
    case 'S': args(s, "eeeeee", a); if (a[0].v >= 1 && a[0].v <= 100) { int n = a[0].v; s->clrColor[n] = a[1].v; s->clr[n] = (S3Win){ a[2].v, a[3].v, a[4].v, a[5].v, 0 }; } return;
    case 'G': args(s, "eeeee", a); if (a[0].v <= 100) { int n = a[0].v; put(s, &a[1], 0, (u16)s->clr[n].x); put(s, &a[2], 0, (u16)s->clr[n].y); put(s, &a[3], 0, (u16)s->clr[n].w); put(s, &a[4], 0, (u16)s->clr[n].h); } return;
    case 'M': {
        args(s, "eeee", a);
        int n = a[0].v, x = a[2].v, y = a[3].v;
        S3Win w = n <= 100 ? s->clr[n] : (S3Win){0};
        put(s, &a[1], 0, x >= w.x && x < w.x + w.w && y >= w.y && y < w.y + w.h);
        return;
    }
    case 'N': {
        args(s, "eeeee", a);
        int found = 0;
        for (int n = a[1].v; n <= a[2].v && n <= 100; n++) {
            S3Win w = s->clr[n];
            if (a[3].v >= w.x && a[3].v < w.x + w.w && a[4].v >= w.y && a[4].v < w.y + w.h) { found = n; break; }
        }
        put(s, &a[0], 0, (u16)found);
        return;
    }
    default: s->vmError = true;
    }
}

// LE and QE: variables or strings to and from a file in the save folder. Numbers are little-endian u16, strings
// NUL-terminated, as System39.exe writes them. A file too short for the count still loads what it has (RND 0).
static void var_file(S3 *s, bool write, int type, const char *name, size_t nameLen, const Arg *start, int count) {
    char file[256];
    snprintf(file, sizeof file, "%.*s", (int)nameLen, name);
    bool ascii = true;
    for (const char *c = file; *c; c++) if ((u8)*c >= 0x80) ascii = false;
    size_t len = strlen(file);
    if (!ascii && len >= 6 && !strcasecmp(file + len - 4, ".ALD") && s->prefix[0]) {
        // A Japanese name ending in <two letters>.ALD names a file beside the game's archives. English releases rename
        // those archives (TG108's 闘神ⅡSA.ALD is tt2SA.ALD, and its PC copy keeps this data as tt2FS.ALD), so use the
        // archives' prefix.
        char tail[8];
        snprintf(tail, sizeof tail, "%s", file + len - 6);
        snprintf(file, sizeof file, "%s%s", s->prefix, tail);
    } else if (!ascii) {
        // File systems want UTF-8 names.
        char *utf8 = s3_sjis_to_utf8(file);
        snprintf(file, sizeof file, "%s", utf8);
        free(utf8);
    }
    // Files the game writes live in the save folder; ones it only reads, such as shipped maps, may be in the game's.
    char *path = s3_find_file(s->saveDir, file);
    if (!path && !write) path = s3_find_file(s->gameDir, file);
    if (!path) path = s3_path(s->saveDir, file);
    if (write) {
        FILE *f = fopen(path, "wb");
        if (!f) { free(path); rnd(s, 255); return; }
        for (int i = 0; i < count; i++) {
            if (type == 0) { u16 v = start->isVar ? s3_lv_get(s, &start->lv, i) : 0; u8 b[2] = { (u8)v, (u8)(v >> 8) }; fwrite(b, 1, 2, f); }
            else { const char *x = str_slot(s, start->v + i); fwrite(x ? x : "", 1, x ? strlen(x) + 1 : 1, f); }
        }
        rnd(s, fclose(f) == 0 ? 0 : 201);
    } else {
        size_t n;
        u8 *d = s3_read_file(path, &n);
        if (!d) { free(path); rnd(s, 255); return; }
        size_t p = 0;
        for (int i = 0; i < count; i++) {
            if (type == 0) { if (p + 2 > n) break; if (start->isVar) s3_lv_set(s, &start->lv, i, s3_le16(d + p)); p += 2; }
            else { if (p >= n) break; str_set(s, start->v + i, (const char *)d + p); p += strlen((const char *)d + p) + 1; }
        }
        free(d);
        rnd(s, 0);
    }
    free(path);
}

static void table_read(S3 *s, int sub, Arg *a) {
    // Data tables hold u16 words (from [n] data) and NUL-terminated strings (from "str").
    S3Blob b;
    if (!s3_archive_get(&s->sa, (u32)s->dataPage + 1, &b, NULL)) return;
    const u8 *d = b.data;
    u32 n = (u32)b.size;
    switch (sub) {
    case 1: { // F1 string, skip
        u32 p = s->dataPtr;
        for (int i = 0; i < a[1].v && p < n; i++) { while (p < n && d[p]) p++; p++; }
        if (p < n) { str_set(s, a[0].v, (const char *)d + p); while (p < n && d[p]) p++; p++; }
        s->dataPtr = p;
        return;
    }
    case 2: case 3: case 4: case 5: {
        u32 p = s->dataPtr + (u32)a[1].v * 2;
        u16 v = p + 2 <= n ? s3_le16(d + p) : 0;
        put(s, &a[0], 0, v);
        if (sub == 2 || sub == 4) s->dataPtr = p + 2;
        return;
    }
    default: unimpl(s, "F table");
    }
}

static void cmd_F(S3 *s, int sub) {
    Arg a[2];
    if (sub < 1 || sub > 11) { s->vmError = true; return; }
    args(s, sub == 11 ? "es" : "ee", a);
    table_read(s, sub, a);
}

static void cmd_I(S3 *s) {
    int k = s3_byte(s);
    Arg a[4];
    switch (k) {
    case 'C': args(s, "ee", a); return;
    case 'E': args(s, "ee", a); return;
    case 'G': {
        args(s, "eeee", a);
        for (int i = 0; i < a[2].v; i++) { int code = a[1].v + i; put(s, &a[0], i, code < 256 && s->keyDown[code]); }
        s->polls++;
        return;
    }
    case 'K': {
        int n = s3_byte(s);
        s->polls++;
        if (n == 0 || n == 1) {
            u32 m = keymask_now(s);
            if (m) { rnd(s, (u16)m); return; }
            s->pressedMask = 0;
            s->wait = S3_WAIT_INPUT_POLL;
            return;
        }
        u32 m = keymask_now(s) | s->pressedMask;
        s->pressedMask = 0;
        if (n == 2) m &= 48;
        rnd(s, (u16)m);
        return;
    }
    case 'M': args(s, "ee", a); put(s, &a[0], 0, (u16)(s->mouseX + s->viewX)); put(s, &a[1], 0, (u16)(s->mouseY + s->viewY));
        rnd(s, (u16)((s->mouseL ? 16 : 0) | (s->mouseR ? 32 : 0) | s->clickLatch)); s->clickLatch = 0; s->polls++; return;
    case 'X': args(s, "e", a); put(s, &a[0], 0, 0); return;
    case 'Y': args(s, "e", a); return;
    case 'Z': args(s, "ee", a); s->mouseX = a[0].v - s->viewX; s->mouseY = a[1].v - s->viewY; return;
    default: s->vmError = true;
    }
}

static void cmd_L(S3 *s) {
    int k = s3_byte(s);
    Arg a[4];
    switch (k) {
    case 'D': args(s, "e", a); if (!s3_save_load(s, a[0].v)) rnd(s, 255); return; // success resumes after the QD
    case 'P': args(s, "eee", a); rnd(s, (u16)(s3_save_load_part(s, a[0].v, &a[1].lv, a[2].v) ? 0 : 255)); return;
    case 'T': {
        args(s, "ee", a);
        u16 t[7];
        bool ok = s3_save_time(s, a[0].v, t);
        if (ok) for (int i = 0; i < 7; i++) put(s, &a[1], i, t[i]);
        rnd(s, ok ? 0 : 255);
        return;
    }
    case 'L': { // LL 0, link, start, count: count words from data link `link` (the DA archive) into variables
        args(s, "beee", a);
        S3Blob b;
        if (!s3_archive_get(&s->da, a[1].v, &b, NULL)) { rnd(s, 255); return; }
        int n = 0;
        for (; n < a[3].v && (size_t)n * 2 + 2 <= b.size; n++) if (a[2].isVar) s3_lv_set(s, &a[2].lv, n, s3_le16(b.data + n * 2));
        rnd(s, n < a[3].v ? 254 : 0);
        return;
    }
    case 'E': args(s, "b:ee", a); var_file(s, false, a[0].v, a[1].s, strcspn(a[1].s, ":"), &a[2], a[3].v); return; // old form: the name ends with ':'
    case 'H': { int k2 = s3_byte(s); (void)k2; args(s, "be", a); rnd(s, 0); return; }
    case 'X': { int k2 = s3_byte(s); (void)k2; args(s, "eee", a); unimpl(s, "LX"); return; }
    default: s->vmError = true;
    }
}

static void cmd_M(S3 *s) {
    int k = s3_byte(s);
    Arg a[5];
    char buf[1024];
    switch (k) {
    case 'A': args(s, "ee", a); { char *d = str_slot(s, a[0].v); const char *src = str_slot(s, a[1].v); if (d && src) { snprintf(buf, sizeof buf, "%s%s", d, src); str_set(s, a[0].v, buf); } } return;
    case 'C': args(s, "ee", a); { const char *x = str_slot(s, a[0].v), *y = str_slot(s, a[1].v); rnd(s, (u16)(x && y && !strcmp(x, y))); } return;
    case 'D': args(s, "eee", a); { const char *src = str_slot(s, a[1].v); if (src) { snprintf(buf, sizeof buf, "%s", src);
        int chars = 0; size_t i = 0; while (buf[i] && chars < a[2].v) { i += s3_sjis_lead((u8)buf[i]) ? 2 : 1; chars++; } buf[i] = 0; str_set(s, a[0].v, buf); } } return;
    case 'E': args(s, "eeeee", a); unimpl(s, "ME"); return;
    case 'F': args(s, "eeee", a); unimpl(s, "MF"); return;
    case 'G': args(s, "be", a); unimpl(s, "MG"); return;
    case 'H': args(s, "eee", a); num_text(buf, sizeof buf, a[1].v, a[2].v, false); str_set(s, a[0].v, buf); return;
    case 'I': args(s, "ees", a); unimpl(s, "MI"); return;
    case 'J': args(s, "eeeee", a); unimpl(s, "MJ"); return;
    case 'L': args(s, "ee", a); { const char *x = str_slot(s, a[1].v); put(s, &a[0], 0, (u16)(x ? sjis_chars(x) : 0)); } return;
    case 'M': args(s, "ee", a); { const char *src = str_slot(s, a[1].v); if (src) { snprintf(buf, sizeof buf, "%s", src); str_set(s, a[0].v, buf); } } return;
    case 'N': args(s, "bee", a); unimpl(s, "MN"); return;
    case 'P': { // MP str, n: n full-width cells of the string, padded with spaces. A half-width character takes half a
        // cell, except under ZH 0, which shows it full-width.
        args(s, "ee", a);
        const char *x = str_slot(s, a[0].v);
        int room = a[1].v * 2;
        size_t o = 0;
        for (const u8 *q = (const u8 *)(x ? x : ""); *q && o + 2 < sizeof buf;) {
            int n = s3_sjis_lead(*q) && q[1] ? 2 : 1, cells = n == 1 && s->zh != 0 ? 1 : 2;
            if (cells > room) break;
            memcpy(buf + o, q, (size_t)n);
            o += (size_t)n; q += n; room -= cells;
        }
        while (room > 0 && o + 1 < sizeof buf) { buf[o++] = ' '; room -= s->zh != 0 ? 1 : 2; }
        buf[o] = 0;
        s3_msg_text(s, buf);
        return;
    }
    case 'S': args(s, "e:", a); { size_t n = 0; while (a[1].s[n] && a[1].s[n] != ':') n += s3_sjis_lead((u8)a[1].s[n]) ? 2 : 1;
        snprintf(buf, sizeof buf, "%.*s", (int)n, a[1].s); str_set(s, a[0].v, buf); } return;
    case 'T': args(s, ":", a); { size_t n = 0; while (a[0].s[n] && a[0].s[n] != ':') n += s3_sjis_lead((u8)a[0].s[n]) ? 2 : 1;
        snprintf(buf, sizeof buf, "%.*s", (int)n, a[0].s); char *u = s3_sjis_to_utf8(buf); snprintf(s->title, sizeof s->title, "%s", u); free(u); } return;
    case 'V': args(s, "e", a); return;
    case 'Z': args(s, "beee", a); if (a[0].v == 0 && a[2].v > 0 && a[2].v < S3_MAX_STRINGS) s->strCount = a[2].v; return; // MZ0,reserved,count,reserved
    default: s->vmError = true;
    }
}

static void cmd_N(S3 *s) {
    int k = s3_byte(s);
    Arg a[4];
    switch (k) {
    case '+': case '-': case '*': case '/': {
        args(s, "eee", a);
        for (int i = 0; i < a[2].v; i++) {
            long x = s3_lv_get(s, &a[0].lv, i), y = a[1].v, r;
            r = k == '+' ? x + y : k == '-' ? x - y : k == '*' ? x * y : (y ? x / y : 0);
            put(s, &a[0], i, (u16)(r < 0 ? 0 : r > 65535 ? 65535 : r));
        }
        return;
    }
    case '>': case '<': case '=': {
        args(s, "eeee", a);
        for (int i = 0; i < a[2].v; i++) {
            u16 x = s3_lv_get(s, &a[0].lv, i), y = a[1].v;
            put(s, &a[3], i, (u16)(k == '>' ? x > y : k == '<' ? x < y : x == y));
        }
        return;
    }
    case '\\': args(s, "ee", a); for (int i = 0; i < a[1].v; i++) put(s, &a[0], i, !s3_lv_get(s, &a[0].lv, i)); return;
    case '~': args(s, "ee", a); for (int i = 0; i < a[1].v; i++) put(s, &a[0], i, (u16)~s3_lv_get(s, &a[0].lv, i)); return;
    case '&': case '|': case '^': {
        args(s, "eee", a);
        for (int i = 0; i < a[1].v; i++) {
            u16 x = s3_lv_get(s, &a[0].lv, i), y = s3_lv_get(s, &a[2].lv, i);
            put(s, &a[0], i, k == '&' ? x & y : k == '|' ? x | y : x ^ y);
        }
        return;
    }
    case 'B': {
        // NB dest, src, count. Copies through a buffer, so overlapping ranges copy as if separate.
        args(s, "eee", a);
        u16 tmp[4096];
        int n = a[2].v < 4096 ? a[2].v : 4096;
        for (int i = 0; i < n; i++) tmp[i] = s3_lv_get(s, &a[1].lv, i);
        for (int i = 0; i < n; i++) put(s, &a[0], i, tmp[i]);
        return;
    }
    case 'C': args(s, "ee", a); for (int i = 0; i < a[1].v; i++) put(s, &a[0], i, 0); return;
    case 'I': args(s, "eeee", a); unimpl(s, "NI"); return;
    case 'P': {
        args(s, "eeee", a);
        bool same = true;
        for (int i = 0; i < a[2].v; i++) if (s3_lv_get(s, &a[0].lv, i) != s3_lv_get(s, &a[1].lv, i)) { same = false; break; }
        put(s, &a[3], 0, same);
        return;
    }
    case 'R': args(s, "ee", a); { unsigned v = a[1].v, r = 0; while ((r + 1) * (r + 1) <= v) r++; put(s, &a[0], 0, (u16)r); } return;
    case 'O': {
        args(s, "beee", a);
        // NO 0: unpack bits of var2 words into var1 flags; NO 1: pack flags of var1 into var2 bits.
        int n = a[3].v;
        if (a[0].v == 0) for (int i = 0; i < n; i++) put(s, &a[1], i, (s3_lv_get(s, &a[2].lv, i / 16) >> (15 - i % 16)) & 1);
        else {
            for (int i = 0; i < (n + 15) / 16; i++) put(s, &a[2], i, 0);
            for (int i = 0; i < n; i++) if (s3_lv_get(s, &a[1].lv, i)) put(s, &a[2], i / 16, (u16)(s3_lv_get(s, &a[2].lv, i / 16) | (0x8000 >> (i % 16))));
        }
        return;
    }
    case 'D': {
        // Double-precision (32-bit) numbers kept in variable pairs, numbered by the first arg.
        int op = s3_byte(s);
        Arg b[3];
        if (op == '+' || op == '-' || op == '*' || op == '/') args(s, "eee", b); else args(s, "ee", b);
        unimpl(s, "ND");
        return;
    }
    default: s->vmError = true;
    }
}

static void cmd_P(S3 *s) {
    int k = s3_byte(s);
    Arg a[4];
    switch (k) {
    case 'C': args(s, "e", a); s->pcMode = a[0].v; return;
    case 'D': args(s, "e", a); s->pdLevel = a[0].v; return;
    case 'F': case 'W': {
        int sub = s3_byte(s);
        args(s, sub < 2 ? "e" : "ee", a);
        bool in = sub == 0 || sub == 2;
        start_fade(s, in, k == 'W', a[0].v, sub >= 2 && a[1].v);
        return;
    }
    case 'G': args(s, "eee", a); for (int i = 0; i < a[2].v; i++) for (int c = 0; c < 3; c++) put(s, &a[0], i * 3 + c, s->pal[(a[1].v + i) & 255][c]); return;
    case 'N': args(s, "e", a); s->noPalCg = a[0].v; return;
    case 'P': args(s, "eee", a); for (int i = 0; i < a[2].v; i++) for (int c = 0; c < 3; c++) s->pal[(a[1].v + i) & 255][c] = (u8)s3_lv_get(s, &a[0].lv, i * 3 + c); return;
    case 'S': args(s, "eeee", a); s->pal[a[0].v & 255][0] = (u8)a[1].v; s->pal[a[0].v & 255][1] = (u8)a[2].v; s->pal[a[0].v & 255][2] = (u8)a[3].v; return;
    case 'T': {
        int sub = s3_byte(s);
        if (sub == 0) { args(s, "eee", a); int x = a[1].v, y = a[2].v;
            put(s, &a[0], 0, x < s->dib.w && y < s->dib.h && s->dib.p8 ? s->dib.p8[(size_t)y * s->dib.w + x] : 0); }
        else if (sub == 1) { Arg b[5]; args(s, "eeeee", b); u32 c = 0; int x = b[3].v, y = b[4].v;
            if (s->dib.p32 && x < s->dib.w && y < s->dib.h) c = s->dib.p32[(size_t)y * s->dib.w + x];
            put(s, &b[0], 0, (c >> 16) & 255); put(s, &b[1], 0, (c >> 8) & 255); put(s, &b[2], 0, c & 255); }
        else { Arg b[4]; args(s, "eeee", b); }
        return;
    }
    default: s->vmError = true;
    }
}

static void cmd_Q(S3 *s) {
    int k = s3_byte(s);
    Arg a[4];
    switch (k) {
    case 'C': args(s, "ee", a); rnd(s, (u16)(s3_save_copy(s, a[0].v, a[1].v) ? 1 : 255)); return;
    case 'D': args(s, "e", a); rnd(s, (u16)(s3_save_write(s, a[0].v) ? 1 : 255)); return;
    case 'P': args(s, "eee", a); rnd(s, (u16)(s3_save_write_part(s, a[0].v, &a[1].lv, a[2].v) ? 1 : 255)); return;
    case 'E': args(s, "b:ee", a); var_file(s, true, a[0].v, a[1].s, strcspn(a[1].s, ":"), &a[2], a[3].v); return; // old form: the name ends with ':'
    default: s->vmError = true;
    }
}

static void cmd_S(S3 *s) {
    int k = s3_byte(s);
    Arg a[5];
    switch (k) {
    case 'S': args(s, "e", a); if (a[0].v) s3_music_play(s, a[0].v, s->cdLoops); else s3_music_stop(s); s->cdLoops = 0; return;
    case 'L': args(s, "e", a); s->cdLoops = a[0].v; return;
    case 'C': args(s, "e", a); { int t = s3_music_position(s);
        if (t < 0) { for (int i = 0; i < 4; i++) put(s, &a[0], i, 999); }
        else { put(s, &a[0], 0, (u16)s->cdTrack); put(s, &a[0], 1, (u16)(t / 60000)); put(s, &a[0], 2, (u16)(t / 1000 % 60)); put(s, &a[0], 3, (u16)(t % 1000 * 75 / 1000)); } }
        return;
    case 'R': { int sub = s3_byte(s); args(s, "e", a);
        put(s, &a[0], 0, (u16)(sub == 0 ? (s3_music_position(s) >= 0 ? s->cdTrack : 0) : s->midiPlaying));
        for (int i = 1; i < 4; i++) put(s, &a[0], i, 0);
        return; }
    case 'I': { s3_byte(s); args(s, "e", a); put(s, &a[0], 0, 1); return; }
    case 'P': args(s, "ee", a); { S3Blob b; if (s3_archive_get(&s->wa, a[0].v, &b, NULL)) s3_wav_play(s, 0, b.data, b.size, a[1].v); } return;
    case 'Q': args(s, "eee", a); { S3Blob b; if (s3_archive_get(&s->wa, a[0].v, &b, NULL)) s3_wav_play(s, 0, b.data, b.size, a[2].v); } return;
    case 'T': args(s, "e", a); s3_wav_stop(s, 0); return;
    case 'M': args(s, "e", a); return;
    case 'O': args(s, "e", a); put(s, &a[0], 0, 1); return;
    case 'U': args(s, "ee", a); put(s, &a[0], 0, s3_wav_playing(s, 0)); put(s, &a[1], 0, 0); return;
    case 'W': args(s, "eeee", a); put(s, &a[0], 0, 2); return;
    case 'G': {
        int sub = s3_byte(s);
        if (sub >= 5) { args(s, "ee", a); if (sub >= 7) put(s, &a[1], 0, 0); return; }
        args(s, "e", a);
        if (sub == 0) s3_midi_stop(s);
        else if (sub == 1) { s3_midi_play(s, a[0].v, s->midiLoops); s->midiLoops = 0; }
        else if (sub == 2) put(s, &a[0], 0, 0);
        else if (sub == 4) s->midiLoops = a[0].v;
        return;
    }
    case 'X': {
        int dev = s3_byte(s), sub = s3_byte(s);
        (void)dev;
        if (sub == 1) { args(s, "eee", a); if (a[2].v) { if (dev == 1 || dev == 0) s3_music_stop(s); if (dev == 2 || dev == 0) s3_midi_stop(s); if (dev == 3 || dev == 0) s3_wav_stop(s, 0); } }
        else if (sub == 2) { args(s, "e", a); put(s, &a[0], 0, 1); }
        else if (sub == 4) { args(s, "e", a); put(s, &a[0], 0, 100); }
        return;
    }
    default: s->vmError = true;
    }
}

static void cmd_U(S3 *s) {
    int k = s3_byte(s);
    Arg a[2];
    switch (k) {
    case 'S': args(s, "ee", a); if (a[0].isVar) for (int i = 0; i < (a[1].v ? a[1].v : 1); i++) s3_push_var(s, (u16)(a[0].lv.var + i)); return;
    case 'G': args(s, "ee", a); {
        // Pops pushes in reverse; values return to the variables they came from.
        for (int i = 0; i < (a[1].v ? a[1].v : 1) && s->sp > 0; i++) {
            S3Frame f = s->stack[s->sp - 1];
            if (f.kind != S3_FRAME_PUSH) break;
            s->sp--;
            s3_var_set(s, f.var, f.value);
        }
    } return;
    case 'C': args(s, "be", a); {
        int mode = a[0].v;
        if (mode == 0 || mode == 3) s->sp = 0;
        else {
            int want = mode == 1 ? S3_FRAME_LABEL : S3_FRAME_PAGE, n = a[1].v;
            while (n > 0 && s->sp > 0) { S3Frame f = s->stack[--s->sp]; if (f.kind == S3_FRAME_PUSH) s3_var_set(s, f.var, f.value); if (f.kind == want) n--; }
        }
    } return;
    case 'D': args(s, "e", a); if (a[0].v == 0) { s->sp = 0; s3_goto_page(s, 0, 0); } else s3_return(s, true); return;
    case 'R': args(s, "e", a); { int pages = 0, labels = 0, pushes = 0; for (int i = 0; i < s->sp; i++) { if (s->stack[i].kind == S3_FRAME_PAGE) pages++; else if (s->stack[i].kind == S3_FRAME_LABEL) labels++; else pushes++; }
        put(s, &a[0], 0, s->sp ? s->stack[s->sp - 1].kind : 0); put(s, &a[0], 1, (u16)pages); put(s, &a[0], 2, (u16)labels); put(s, &a[0], 3, (u16)pushes); put(s, &a[0], 4, 0); put(s, &a[0], 5, 0); } return;
    default: s->vmError = true;
    }
}

static void cmd_W(S3 *s) {
    int k = s3_byte(s);
    Arg a[4];
    switch (k) {
    case 'V': args(s, "eeee", a); s->viewX = a[0].v; s->viewY = a[1].v; s->viewW = a[2].v; s->viewH = a[3].v; s3_mark_dirty(s); return;
    case 'W': args(s, "eee", a); s3_gfx_init(s, a[0].v, a[1].v, a[2].v); return;
    case 'X': args(s, "eeee", a); s->flip = true; s3_mark_dirty(s); return;
    case 'Z': { int sub = s3_byte(s); args(s, "e", a); s->flip = a[0].v != 0; if (sub == 0) s3_mark_dirty(s); return; }
    default: s->vmError = true;
    }
}

static void cmd_Z(S3 *s) {
    int k = s3_byte(s);
    Arg a[4];
    switch (k) {
    case 'A': args(s, "be", a); if (a[0].v == 0) s->decoration = a[1].v; else if (a[0].v == 1) s->decorationColor = a[1].v; return;
    case 'B': args(s, "e", a); s->fontWeight = a[0].v; return;
    case 'C': args(s, "ee", a); {
        int n = a[1].v;
        switch (a[0].v) {
        case 0: s->palBank = n; break;
        case 1: s->msgColor = n; break;
        case 2: s->menuColor = n; break;
        case 3: s->menuFrame = n; break;
        case 4: s->menuBg = n; break;
        case 5: s->msgFrame = n; break;
        case 6: s->msgBg = n; break;
        case 7: s->hitColor = n; break;
        case 10: s->menuCursor = n; break;
        case 11: s->menuSelColor = n; break;
        case 13: s->msgBgAlpha = n; break;
        case 14: s->menuBgAlpha = n; break;
        case 15: s->menuDefault = n; break;
        }
    } return;
    case 'D': args(s, "be", a); return;
    case 'E': args(s, "e", a); s->msgClearOnSelect = a[0].v != 0; return;
    case 'F': args(s, "e", a); return;
    case 'G': args(s, "e", a); if (a[0].isVar) { s->cgCountVar.on = true; s->cgCountVar.pointer = a[0].lv.var; } return;
    case 'H': args(s, "e", a); s->zh = a[0].v; return;
    case 'I': args(s, "ee", a); if (a[0].v < 8) s->keyMode[a[0].v] = a[1].v; return;
    case 'L': args(s, "e", a); s->lineSpace = a[0].v; return;
    case 'M': args(s, "e", a); s->fontSize = a[0].v; return;
    case 'R': args(s, "ee", a); s->rand = s->rand * 1103515245u + 12345u; put(s, &a[1], 0, a[0].v ? (u16)(1 + (s->rand >> 16) % a[0].v) : 0); return;
    case 'S': args(s, "e", a); s->menuFontSize = a[0].v; return;
    case 'T': {
        int sub = s3_byte(s);
        if (sub == 10) { args(s, "eee", a); return; }
        if (sub == 11) { args(s, "ee", a); put(s, &a[1], 0, 0); return; }
        args(s, "e", a);
        u64 t = s->clockMs - s->timerBase[0];
        switch (sub) {
        case 0: { time_t now = (time_t)(s->startTime + (long long)(s->clockMs / 1000)); struct tm tm; localtime_r(&now, &tm);
            int v[7] = { tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday };
            for (int i = 0; i < 7; i++) put(s, &a[0], i, (u16)v[i]); } break;
        case 1: s->timerBase[0] = s->clockMs - (u64)a[0].v * 10; break;
        case 2: put(s, &a[0], 0, (u16)(t / 100)); s->polls++; break;
        case 3: put(s, &a[0], 0, (u16)(t * 30 / 1000)); s->polls++; break;
        case 4: put(s, &a[0], 0, (u16)(t * 60 / 1000)); s->polls++; break;
        case 5: put(s, &a[0], 0, (u16)(t / 10)); s->polls++; break;
        case 20: wait_ms(s, a[0].v, false); break;
        case 21: wait_ms(s, a[0].v, true); break;
        }
        return;
    }
    case 'W': args(s, "e", a); return;
    case 'Z': {
        int sub = s3_byte(s);
        args(s, "e", a);
        switch (sub) {
        case 0: s->quit = true; break;
        case 1: put(s, &a[0], 0, 1); break;
        case 2: str_set(s, a[0].v, "\x82\x76\x82\x89\x82\x8e\x82\x84\x82\x8f\x82\x97\x82\x93"); break; // Ｗｉｎｄｏｗｓ
        case 3: put(s, &a[0], 0, 1920); put(s, &a[0], 1, 1080); put(s, &a[0], 2, 24); break;
        case 4: put(s, &a[0], 0, (u16)s->dib.w); put(s, &a[0], 1, (u16)s->dib.h); put(s, &a[0], 2, (u16)s->dib.depth); break;
        case 5: put(s, &a[0], 0, (u16)s->viewW); put(s, &a[0], 1, (u16)s->viewH); break;
        case 7: put(s, &a[0], 0, 65535); break;
        case 8: put(s, &a[0], 0, 65535); break;
        case 9: put(s, &a[0], 0, 1920); put(s, &a[0], 1, 1080); put(s, &a[0], 2, 24); break;
        case 10: put(s, &a[0], 0, 0); break;
        case 13: break;
        case 14: str_set(s, a[0].v, "Player"); break;
        }
        return;
    }
    default: s->vmError = true;
    }
}

static void cmd_V(S3 *s) {
    int k = s3_byte(s);
    Arg a[10];
    u16 v[10] = {0};
    const char *sig;
    switch (k) {
    case 'A': { // VA sub,n,a,b: sprites (sprite.c)
        int sub = s3_byte(s);
        args(s, "eee", a);
        s3_sprite_command(s, sub, a[0].v, a[1].v, a[2].v, a[1].isVar ? &a[1].lv : NULL, a[2].isVar ? &a[2].lv : NULL);
        return;
    }
    case 'B': case 'C': sig = "eeeeeee"; break;
    case 'E': case 'H': case 'P': sig = "eeeeee"; break;
    case 'F': sig = ""; break;
    case 'G': case 'J': sig = "eeee"; break;
    case 'I': v[4] = s3_byte(s) == 'P'; sig = "eeee"; break; // VIC or VIP
    case 'R': case 'W': sig = "eee"; break;
    case 'S': sig = "eeeee"; break;
    case 'T': sig = "eeeeeeeeee"; break;
    case 'V': sig = "ee"; break;
    case 'X': sig = "eeee"; break;
    case 'Z': sig = "bee"; break;
    default: s->vmError = true; return;
    }
    args(s, sig, a);
    for (size_t i = 0; sig[i]; i++) if (k != 'I' || i < 4) v[i] = a[i].v;
    if (k == 'J') { unimpl(s, "VJ"); return; }
    s3_unitmap_command(s, k, v, (k == 'R' || k == 'W') && a[2].isVar ? &a[2].lv : NULL);
}

static void cmd_Y(S3 *s) {
    Arg a[2];
    args(s, "ee", a);
    switch (a[0].v) {
    case 1: if (a[1].v == 0) s3_msg_clear(s); else { s->tx = s->msgWin[s->curMsg].x; s->ty = s->msgWin[s->curMsg].y; } return;
    case 2: for (int i = 1; i <= 20; i++) s3_var_set(s, (u16)i, 0); return;
    case 3:
        if (a[1].v == 10000) s->y3Cancel = false;
        else if (a[1].v == 10001) s->y3Cancel = true;
        else wait_ms(s, (u32)a[1].v * 1000 / 60, s->y3Cancel);
        return;
    case 4: s->rand = s->rand * 1103515245u + 12345u; rnd(s, a[1].v <= 1 ? a[1].v : (u16)(1 + (s->rand >> 16) % a[1].v)); return;
    default: unimpl(s, "Y"); return;
    }
}

static void cmd_slash(S3 *s) {
    int sub = s3_byte(s);
    Arg a[10];
    char buf[64];
    switch (sub) {
    case 0x7c: args(s, "m", a); s3_message(s, a[0].u); return;
    case 0x7d: case 0x7e: args(s, "mbe", a); num_text(buf, sizeof buf, a[1].v, a[2].v, sub == 0x7e); s3_msg_text(s, buf); return;
    case 0x7f: args(s, "me", a); { const char *x = str_slot(s, a[1].v); if (x) s3_msg_text(s, x); } return;
    case 0x27: args(s, "es", a); str_set(s, a[0].v, a[1].s); return;
    case 0x28: args(s, "s", a); { char *u = s3_sjis_to_utf8(a[0].s); snprintf(s->title, sizeof s->title, "%s", u); free(u); } return;
    case 0x29: args(s, "s", a); return;
    case 0x2c: cmd_F(s, s3_byte(s)); return;
    case 0x23: args(s, "ees", a); { // LC x,y,file
        char *p = s3_find_file(s->gameDir, a[2].s);
        size_t n; u8 *d = p ? s3_read_file(p, &n) : NULL;
        S3Image img;
        if (d && s3_cg_decode(d, n, &img)) { s3_draw_image(s, &img, a[0].v, a[1].v, -1); s3_image_free(&img); }
        free(d); free(p);
    } return;
    case 0x24: args(s, "bsee", a); var_file(s, false, a[0].v, a[1].s, strlen(a[1].s), &a[2], a[3].v); return; // LE
    case 0x26: args(s, "ees", a); unimpl(s, "MI"); return;
    case 0x2a: args(s, "bsee", a); var_file(s, true, a[0].v, a[1].s, strlen(a[1].s), &a[2], a[3].v); return; // QE
    case 0x47: return;  // emitted before ]; no visible effect found
    case 0x00: case 0x01: case 0x04: // TOC TOS TOP
        if (s->textSp > 0) { s->textSp--; int *t = s->textStack[s->textSp];
            if (sub == 0x00) s->msgColor = t[0]; else if (sub == 0x01) s->fontSize = t[0]; else { s->tx = t[0]; s->ty = t[1]; } }
        return;
    case 0x02: case 0x03: case 0x05: // TPC TPS TPP
        if (sub != 0x05) args(s, "e", a);
        if (s->textSp < 16) { int *t = s->textStack[s->textSp++];
            if (sub == 0x02) { t[0] = s->msgColor; s->msgColor = a[0].v; } else if (sub == 0x03) { t[0] = s->fontSize; s->fontSize = a[0].v; } else { t[0] = s->tx; t[1] = s->ty; } }
        return;
    case 0x06: args(s, "e", a); put(s, &a[0], 0, (u16)(a[0].v + 1)); return; // inc
    case 0x07: args(s, "e", a); put(s, &a[0], 0, (u16)(a[0].v ? a[0].v - 1 : 0)); return; // dec
    default: break;
    }
    // Everything else: parse by signature and skip.
    static const struct { u8 sub; const char *sig; } tab[] = {
        {0x08,"e"},{0x09,"e"},{0x0a,"ee"},{0x0b,"ee"},{0x0c,"e"},{0x0d,"e"},{0x0e,"ee"},{0x0f,"eeee"},{0x10,"ee"},{0x11,"e"},{0x12,"s"},
        {0x13,"eeee"},{0x14,""},{0x15,"eeee"},{0x16,"eee"},{0x17,"eee"},{0x18,"eeeee"},{0x19,"ee"},{0x1a,"e"},{0x1b,"eeee"},{0x1d,""},
        {0x1e,"ee"},{0x1f,""},{0x20,"e"},{0x25,"ess"},{0x2b,"bss"},{0x2d,"ee"},{0x2e,"ee"},{0x2f,"e"},{0x30,"ee"},{0x31,"f"},{0x32,"f"},
        {0x33,""},{0x34,""},{0x35,"ee"},{0x36,"eeeeeeeee"},{0x37,"eeeee"},{0x38,""},{0x39,"ee"},{0x3a,"e"},{0x3b,"e"},{0x3c,"eee"},{0x3d,"ee"},
        {0x3e,""},{0x3f,""},{0x42,"eeeeeeeee"},{0x43,"eeee"},{0x44,"eee"},{0x45,"f"},{0x46,""},{0x48,"s"},{0x49,"ss"},{0x4a,"e"},{0x4b,"e"},
        {0x4c,"eeeeee"},{0x4d,"es"},{0x4e,"ee"},{0x4f,"ee"},{0x50,"ebe"},{0x51,"ebe"},{0x52,"e"},{0x53,"e"},{0x54,"eeee"},{0x55,"eeeeee"},
        {0x56,"ess"},{0x57,"seee"},{0x58,"ee"},{0x59,"ee"},{0x5a,"s"},{0x5c,"eeeeeee"},{0x5d,"ee"},{0x5e,"eeee"},{0x5f,"e"},{0x61,"ee"},
        {0x62,""},{0x63,"e"},{0x64,"eee"},{0x65,"eee"},{0x66,"ef"},{0x67,"eee"},{0x68,"e"},{0x69,"e"},{0x6a,"e"},{0x6b,"e"},{0x6c,"e"},
        {0x6d,"ee"},{0x6e,"e"},{0x6f,"eeee"},{0x70,"e"},{0x71,"e"},{0x72,"e"},{0x73,"e"},{0x74,"se"},{0x75,"e"},{0x76,"e"},{0x77,"ee"},
        {0x78,"ee"},{0x79,"ee"},{0x7a,""},{0x7b,""},{0x80,"f"},{0x81,"ee"},{0x82,"ee"},{0x83,"e"},{0x84,"e"},{0x85,"e"},{0x86,"e"},
        {0x87,"eeee"},{0x88,"ee"},
    };
    for (size_t i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        if (tab[i].sub == sub) {
            args(s, tab[i].sig, a);
            snprintf(buf, sizeof buf, "/%02x", sub);
            unimpl(s, buf);
            return;
        }
    }
    s->vmError = true;
}

void s3_command(S3 *s) {
    int c = s3_byte(s);
    Arg a[4];
    switch (c) {
    case 'B': cmd_B(s); return;
    case 'C': cmd_C(s); return;
    case 'D': cmd_D(s); return;
    case 'E': cmd_E(s); return;
    case 'F': cmd_F(s, s3_byte(s)); return;
    case 'G': {
        int k = s3_byte(s);
        if (k == 0) { args(s, "e", a); s3_load_cg(s, a[0].v, -1, 0); }
        else if (k == 1) { args(s, "ee", a); s3_load_cg(s, a[0].v, a[1].v, 0); }
        else if (k == 'S') {
            args(s, "ee", a);
            S3Blob b; S3Image img;
            if (s3_archive_get(&s->ga, a[0].v, &b, NULL) && s3_cg_decode(b.data, b.size, &img)) {
                int x = img.x, y = img.y;
                if (s->cgPos) { if (s->cgPosMode == 0 || s->cgPosMode == 2) { x = s->cgX; y = s->cgY; } else { x += s->cgX; y += s->cgY; } }
                put(s, &a[1], 0, (u16)x); put(s, &a[1], 1, (u16)y); put(s, &a[1], 2, (u16)img.w); put(s, &a[1], 3, (u16)img.h);
                s3_image_free(&img);
            }
        } else if (k == 'X') { args(s, "ee", a); s3_load_cg(s, a[0].v, -1, 0); }
        else s->vmError = true;
        return;
    }
    case 'H': { args(s, "be", a); char buf[64]; num_text(buf, sizeof buf, a[0].v, a[1].v, false); s3_msg_text(s, buf); return; }
    case 'I': cmd_I(s); return;
    case 'J': {
        int k = s3_byte(s);
        if (k == 4) { s->cgPos = false; return; }
        args(s, "ee", a);
        s->cgX = a[0].v; s->cgY = a[1].v; s->cgPos = true; s->cgPosMode = k;
        return;
    }
    case 'K': { s3_byte(s); unimpl(s, "network"); s->vmError = true; return; }
    case 'L': cmd_L(s); return;
    case 'M': cmd_M(s); return;
    case 'N': cmd_N(s); return;
    case 'P': cmd_P(s); return;
    case 'Q': cmd_Q(s); return;
    case 'S': cmd_S(s); return;
    case 'T': args(s, "ee", a); s->tx = a[0].v; s->ty = a[1].v; return;
    case 'U': cmd_U(s); return;
    case 'V': cmd_V(s); return;
    case 'W': cmd_W(s); return;
    case 'X': { args(s, "e", a); const char *x = str_slot(s, a[0].v); if (x) s3_msg_text(s, x); return; }
    case 'Y': cmd_Y(s); return;
    case 'Z': cmd_Z(s); return;
    case '/': cmd_slash(s); return;
    default: s->vmError = true;
    }
}

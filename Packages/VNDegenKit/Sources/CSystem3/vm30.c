// System 3.0 (SYSTEM3.EXE) scenarios: the command set of the System 1-3.0 command manual, over the same expression
// encoding as System 3.5 (read from the games' ADISK.DAT). Labels are 16-bit page offsets and pages start with a
// 16-bit field. Saves and the V stacks use one flat state image laid out as SYSTEM3.EXE writes ASLEEP_A.DAT, read
// from saves it made under Wine for hand-assembled test pages (docs/system3-cleanroom.md).
#include "vm.h"
#include <sys/stat.h>
#include <time.h>

// ---- the state image ----

#define IMG_SIZE 9510
#define IMG_PAGE 0x70
#define IMG_PC 0x7c
#define IMG_VARS 0x80          // 512 variables
#define IMG_FONTS 0x480        // u16 choice font size, message font size
#define IMG_COLOURS 0x484      // u16 Z 0 to 6
#define IMG_MENUWIN 0x492      // B1 boxes 1 to 10, 16 bytes each: u16 x1, y1, x2, y2, push
#define IMG_MSGWIN 0x532       // B3 boxes 1 to 10
#define IMG_STACKS 0x5aa       // V stacks, 260 bytes each: U01-U20, then 10 text variables of 22 bytes
#define IMG_TEXT 0x5d2         // the 10 text variables, which stack 0's text part shares
#define STACK_SIZE 260
#define TEXT_LEN 22

static const char banner[] = "This is save data for System3 Union [PC9801:FMTOWNS:X68000] For NACT/ADV system          (C)1994 ALICE-SOFT    ";

typedef struct S3State30 {
    u8 img[IMG_SIZE];
    int textIndex;             // Y8
    int colours[8];            // Z 0 to 7
    int menuFont, msgFont;
    bool columns;              // column mode: some x coordinates count 8-pixel columns
    int srcScreen, dstScreen;  // Y30, Y31
    int cdTrack[100];          // Z 101-199: S n plays CD track cdTrack[n], 0 for none
    bool clearOnMenu;
    bool menuSet[11], msgSet[11]; // boxes B1 and B3 set, which the image keeps; others stay zero there
    char **jp, **en;           // replacement.txt pairs, sorted by jp
    int pairs;
} S3State30;

static void put16(u8 *p, int v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }

static int col(S3 *s, int x) { return s->s30->columns ? x * 8 : x; }

static char *text_var(S3 *s, int n) {
    if (n < 1 || n > 10) return NULL;
    if (!s->str[n]) s->str[n] = s3_alloc(TEXT_LEN + 1);
    return s->str[n];
}

// Writes the runtime's state into the image.
static void state_out(S3 *s) {
    S3State30 *t = s->s30;
    u8 *m = t->img;
    for (int i = 0; i < 512; i++) put16(m + IMG_VARS + i * 2, s3_var_get(s, (u16)i));
    put16(m + IMG_FONTS, t->menuFont);
    put16(m + IMG_FONTS + 2, t->msgFont);
    for (int k = 0; k < 7; k++) put16(m + IMG_COLOURS + k * 2, t->colours[k]);
    for (int k = 1; k <= 10; k++) {
        u8 *w = m + IMG_MENUWIN + (k - 1) * 16;
        S3Win b = s->selWin[k];
        if (t->menuSet[k]) { put16(w, b.x); put16(w + 2, b.y); put16(w + 4, b.x + b.w - 1); put16(w + 6, b.y + b.h - 1); put16(w + 8, b.save); }
    }
    for (int k = 1; k <= 10; k++) {
        u8 *w = m + IMG_MSGWIN + (k - 1) * 16;
        S3Win b = s->msgWin[k];
        if (t->msgSet[k]) { put16(w, b.x); put16(w + 2, b.y); put16(w + 4, b.x + b.w - 1); put16(w + 6, b.y + b.h - 1); }
    }
    for (int k = 1; k <= 10; k++) {
        u8 *p = m + IMG_TEXT + (k - 1) * TEXT_LEN;
        memset(p, 0, TEXT_LEN);
        const char *v = s->str[k];
        if (v) memcpy(p, v, strnlen(v, TEXT_LEN - 1));
    }
}

// Reads the runtime's state back from the image.
static void state_in(S3 *s) {
    S3State30 *t = s->s30;
    const u8 *m = t->img;
    for (int i = 0; i < 512; i++) s3_var_set(s, (u16)i, s3_le16(m + IMG_VARS + i * 2));
    t->menuFont = s3_le16(m + IMG_FONTS);
    t->msgFont = s3_le16(m + IMG_FONTS + 2);
    for (int k = 0; k < 7; k++) t->colours[k] = s3_le16(m + IMG_COLOURS + k * 2);
    for (int k = 1; k <= 10; k++) {
        const u8 *w = m + IMG_MENUWIN + (k - 1) * 16;
        int x1 = s3_le16(w), y1 = s3_le16(w + 2), x2 = s3_le16(w + 4), y2 = s3_le16(w + 6);
        if ((t->menuSet[k] = x2 > x1 || y2 > y1)) s->selWin[k] = (S3Win){ x1, y1, x2 - x1 + 1, y2 - y1 + 1, s3_le16(w + 8) };
        w = m + IMG_MSGWIN + (k - 1) * 16;
        x1 = s3_le16(w); y1 = s3_le16(w + 2); x2 = s3_le16(w + 4); y2 = s3_le16(w + 6);
        if ((t->msgSet[k] = x2 > x1 || y2 > y1)) s->msgWin[k] = (S3Win){ x1, y1, x2 - x1 + 1, y2 - y1 + 1, 0 };
    }
    for (int k = 1; k <= 10; k++) {
        char *v = text_var(s, k);
        memcpy(v, m + IMG_TEXT + (k - 1) * TEXT_LEN, TEXT_LEN);
        v[TEXT_LEN] = 0;
    }
    s->fontSize = t->msgFont;
    s->menuFontSize = t->menuFont;
    s->msgColor = t->colours[1];
    s->menuColor = t->colours[2];
    s->menuFrame = t->colours[3];
    s->menuBg = t->colours[4];
    s->msgFrame = t->colours[5];
    s->msgBg = t->colours[6];
}

static char *save_path(S3 *s, int slot) {
    char name[32];
    snprintf(name, sizeof name, "ASLEEP_%c.DAT", 'A' + slot - 1);
    char *found = s3_find_file(s->saveDir, name);
    return found ? found : s3_path(s->saveDir, name);
}

static bool save30(S3 *s, int slot) {
    if (slot < 1 || slot > 26) return false;
    S3State30 *t = s->s30;
    state_out(s);
    memset(t->img, 0, 0x80);
    memcpy(t->img, banner, sizeof banner);
    put16(t->img + IMG_PAGE, s->page + 1);
    put16(t->img + IMG_PC, (int)s->pc);
    char *path = save_path(s, slot);
    bool ok = s3_write_file(path, t->img, IMG_SIZE);
    s3_log(s, "saved slot %d: %s", slot, ok ? path : "failed");
    free(path);
    return ok;
}

static bool load30(S3 *s, int slot) {
    if (slot < 1 || slot > 26) return false;
    char *path = save_path(s, slot);
    size_t n;
    u8 *d = s3_read_file(path, &n);
    free(path);
    if (!d || n < IMG_SIZE || memcmp(d, "This is save data", 17) != 0) { free(d); return false; }
    memcpy(s->s30->img, d, IMG_SIZE);
    free(d);
    state_in(s);
    s->sp = 0;
    s->menuCount = 0;
    s->menuBuild = -1;
    int page = s3_le16(s->s30->img + IMG_PAGE), pc = s3_le16(s->s30->img + IMG_PC);
    if (page < 1 || !s3_goto_page(s, page - 1, (u32)pc)) return false;
    s3_var_set(s, 0, 0);
    return true;
}

// ---- replacement.txt: j: lines as the game shows them, e: lines to show instead ----

static void load_replacements(S3 *s) {
    char *path = s3_find_file(s->gameDir, "replacement.txt");
    size_t n;
    char *d = path ? (char *)s3_read_file(path, &n) : NULL;
    free(path);
    if (!d) return;
    S3State30 *t = s->s30;
    int cap = 0, count = 0;
    char **jp = NULL, **en = NULL;
    char *pending = NULL;
    for (char *save = NULL, *line = strtok_r(d, "\r\n", &save); line; line = strtok_r(NULL, "\r\n", &save)) {
        if (!strncmp(line, "j:", 2)) { pending = line + 2; continue; }
        if (strncmp(line, "e:", 2) || !pending) continue;
        if (count == cap) { cap = cap ? cap * 2 : 1024; jp = realloc(jp, sizeof(char *) * (size_t)cap); en = realloc(en, sizeof(char *) * (size_t)cap); }
        jp[count] = s3_sjis_to_utf8(pending);
        en[count] = s3_strdup(line + 2);
        count++;
        pending = NULL;
    }
    free(d);
    // Sort by the Japanese line, keeping the first translation of a line that appears twice.
    char **sj = s3_alloc(sizeof(char *) * (size_t)(count + 1)), **se = s3_alloc(sizeof(char *) * (size_t)(count + 1));
    int kept = 0;
    for (int i = 0; i < count; i++) {
        int lo = 0, hi = kept;
        while (lo < hi) { int mid = (lo + hi) / 2; if (strcmp(sj[mid], jp[i]) < 0) lo = mid + 1; else hi = mid; }
        if (lo < kept && !strcmp(sj[lo], jp[i])) { free(jp[i]); free(en[i]); continue; }
        memmove(sj + lo + 1, sj + lo, sizeof(char *) * (size_t)(kept - lo));
        memmove(se + lo + 1, se + lo, sizeof(char *) * (size_t)(kept - lo));
        sj[lo] = jp[i];
        se[lo] = en[i];
        kept++;
    }
    free(jp);
    free(en);
    t->jp = sj;
    t->en = se;
    t->pairs = kept;
    s3_log(s, "replacement.txt: %d lines", kept);
}

static const char *replacement(S3 *s, const char *displayed) {
    S3State30 *t = s->s30;
    int lo = 0, hi = t->pairs;
    while (lo < hi) {
        int mid = (lo + hi) / 2, c = strcmp(t->jp[mid], displayed);
        if (!c) return t->en[mid];
        if (c < 0) lo = mid + 1; else hi = mid;
    }
    return NULL;
}

// Shows a run of game text, through replacement.txt when the game has one.
static void text30(S3 *s, const char *sjis) {
    S3State30 *t = s->s30;
    if (t->pairs) {
        char *shown = s3_display_utf8(s, sjis);
        const char *en = replacement(s, shown);
        free(shown);
        if (en) {
            int zh = s->zh;
            s->zh = 2;
            s3_msg_text(s, en);
            s->zh = zh;
            return;
        }
    }
    s3_msg_text(s, sjis);
}

// ---- opening ----

bool s3_open30(S3 *s) {
    static const char *const scenario[] = { "ADISK.DAT" };
    static const char *const cgs[] = { "ACG.DAT", "BCG.DAT", "CCG.DAT", "DCG.DAT" };
    if (!s3_dat_open(&s->adisk, s->gameDir, scenario, 1)) return false;
    s3_dat_open(&s->cg30, s->gameDir, cgs, 4);
    s3_gaiji_load(s);
    s->s30 = s3_alloc(sizeof(S3State30));
    S3State30 *t = s->s30;
    // The defaults a fresh SYSTEM3.EXE saves.
    t->menuFont = t->msgFont = 16;
    int colours[8] = { 0, 15, 15, 15, 0, 15, 0, 15 };
    memcpy(t->colours, colours, sizeof colours);
    t->columns = true;
    t->clearOnMenu = true;
    for (int i = 0; i < 128; i++) { s->msgWin[i] = (S3Win){0}; s->selWin[i] = (S3Win){0}; }
    s->msgWin[1] = (S3Win){ 8, 304, 624, 88, 0 };
    s->selWin[1] = (S3Win){ 464, 80, 160, 200, 1 };
    s->strCount = 10;
    state_out(s);
    S3Win msgDefault = s->msgWin[1], selDefault = s->selWin[1];
    state_in(s);
    s->msgWin[1] = msgDefault;
    s->selWin[1] = selDefault;
    s->zh = 0;
    s->palBank = 0;
    load_replacements(s);
    snprintf(s->title, sizeof s->title, "%s", "System 3");
    return true;
}

void s3_close30(S3 *s) {
    S3State30 *t = s->s30;
    if (t) {
        for (int i = 0; i < t->pairs; i++) { free(t->jp[i]); free(t->en[i]); }
        free(t->jp);
        free(t->en);
        free(t);
        s->s30 = NULL;
    }
    // The archives are closed whatever state the open reached; closing an unopened one does nothing.
    s3_dat_close(&s->adisk);
    s3_dat_close(&s->cg30);
    free(s->gaiji);
    s->gaiji = NULL;
    s->gaijiCount = 0;
}

// ---- commands ----

static void e_args(S3 *s, u16 *v, S3LV *lv, int n) {
    for (int i = 0; i < n; i++) {
        u32 at = s->pc;
        int c = s->pc < s->codeSize ? s->code[s->pc] : 0x7f;
        if (lv) lv[i].var = 0xffff;
        if (c >= 0x80 && lv) {
            s3_lvarg(s, &lv[i]);
            if (!s->vmError) { v[i] = s3_lv_get(s, &lv[i], 0); continue; }
            s->vmError = false;
            s->pc = at;
            lv[i].var = 0xffff;
        }
        v[i] = s3_expr(s);
    }
}

static void fade(S3 *s, bool in, bool white, int speed) {
    memcpy(s->shownPal, s->pal, sizeof s->pal);
    s->fadeWhite = white;
    s->wait = S3_WAIT_FADE;
    s->waitArg[0] = in;
    s->waitArg[1] = 0;
    s->waitArg[2] = (int)s->clockMs;
    s->waitArg[3] = (speed ? speed : 1) * 16 * 1000 / 60;
}

static void cmd_y30(S3 *s, int a, int b) {
    S3State30 *t = s->s30;
    switch (a) {
    case 1:
        if (b == 0) s3_msg_clear(s);
        else { s->tx = s->msgWin[s->curMsg].x; s->ty = s->msgWin[s->curMsg].y; }
        return;
    case 2: { int from = b == 0 ? 1 : b == 1 ? 21 : 41, to = b == 0 ? 20 : b == 1 ? 40 : 56; for (int i = from; i <= to; i++) s3_var_set(s, (u16)i, 0); return; }
    case 3:
        if (b == 10000) s->y3Cancel = false;
        else if (b == 10001) s->y3Cancel = true;
        else { s->wait = S3_WAIT_TIME; s->waitUntil = s->clockMs + (u64)b * 1000 / 60; s->waitArg[0] = s->y3Cancel; s->waitArg[1] = 0; s->pressedMask = 0; }
        return;
    case 4: s->rand = s->rand * 1103515245u + 12345u; s3_var_set(s, 0, b <= 1 ? 0 : (u16)(1 + (s->rand >> 16) % b)); return;
    case 7: { S3Win w = s->msgWin[b > 0 && b < 128 ? b : 1]; s3_fill(s, w.x, w.y, w.w, w.h, s->msgBg); return; }
    case 8: t->textIndex = b; return;
    case 10: s->noPalCg = b; return;
    case 14: s3_var_set(s, 0, b == 2); return; // only the CD drive: SYSTEM3.EXE says no to FM (0) and the others
    case 16: s3_music_stop(s); return;
    case 17: s3_var_set(s, 0, (u16)(b == 0 && s3_music_position(s) >= 0)); return;
    case 18: s3_var_set(s, 1, 0); s3_var_set(s, 2, 0); return;
    case 25: case 26: {
        static const int sizes[] = { 16, 16, 24, 32, 48, 64 };
        int size = b >= 1 && b <= 5 ? sizes[b] : 16;
        if (a == 25) { t->menuFont = size; s->menuFontSize = size; } else { t->msgFont = size; s->fontSize = size; }
        return;
    }
    case 30: t->srcScreen = b == 1; return;
    case 31: t->dstScreen = b == 1; return;
    case 40: case 42: fade(s, true, false, b); return;
    case 41: fade(s, false, false, b); return;
    case 43: fade(s, false, true, b); return;
    case 46: s->pcMode = b & 7; return;
    case 230: t->clearOnMenu = b != 0; return;
    case 231: s3_var_set(s, 1, 640); s3_var_set(s, 2, b == 1 ? 480 : 400); s3_var_set(s, 3, 256); return;
    case 238: case 239: {
        u16 v[7] = {0};
        if (a == 238) {
            time_t now = (time_t)(s->startTime + (long long)(s->clockMs / 1000));
            struct tm tm;
            localtime_r(&now, &tm);
            u16 w[7] = { (u16)(tm.tm_year + 1900), (u16)(tm.tm_mon + 1), (u16)tm.tm_mday, (u16)tm.tm_hour, (u16)tm.tm_min, (u16)tm.tm_sec, (u16)(tm.tm_wday + 1) };
            memcpy(v, w, sizeof w);
        } else {
            char *path = save_path(s, b);
            struct stat st;
            if (stat(path, &st) == 0) {
                struct tm tm;
                time_t mt = st.st_mtime;
                localtime_r(&mt, &tm);
                u16 w[7] = { (u16)(tm.tm_year + 1900), (u16)(tm.tm_mon + 1), (u16)tm.tm_mday, (u16)tm.tm_hour, (u16)tm.tm_min, (u16)tm.tm_sec, (u16)(tm.tm_wday + 1) };
                memcpy(v, w, sizeof w);
            }
            free(path);
        }
        for (int i = 0; i < 7; i++) s3_var_set(s, (u16)(1 + i), v[i]);
        return;
    }
    case 251: t->columns = b == 0; return;
    case 252: s3_var_set(s, 0, 8); return;
    case 254: s3_var_set(s, 0, 1); return;   // Windows 3.1
    case 255: s->quit = true; return;
    default: return; // 5, 6, 13 (text speed, which SYSTEM3.EXE ignores too), 15, 21-24, 60 and the rest change nothing VNDegen shows
    }
}

static u32 keys_now(S3 *s) {
    u32 m = 0;
    if (s->keyDown[0x26]) m |= 1;
    if (s->keyDown[0x28]) m |= 2;
    if (s->keyDown[0x25]) m |= 4;
    if (s->keyDown[0x27]) m |= 8;
    if (s->keyDown[0x0d] || s->mouseL) m |= 16;
    if (s->keyDown[0x20] || s->mouseR) m |= 32;
    if (s->keyDown[0x1b]) m |= 64;
    if (s->keyDown[0x09]) m |= 128;
    return m;
}

static void command30(S3 *s) {
    S3State30 *t = s->s30;
    int c = s3_byte(s);
    u16 v[8];
    S3LV lv[8];
    char buf[64];
    switch (c) {
    case 'B': {
        int sub = s3_byte(s);
        e_args(s, v, NULL, 6);
        int k = v[0];
        if (k < 1 || k > 10) return;
        if (sub == 1 || sub == 3) {
            int x1 = col(s, v[1]), y1 = v[2], x2 = t->columns ? v[3] * 8 - 1 : v[3], y2 = v[4];
            S3Win w = { x1, y1, x2 - x1 + 1, y2 - y1 + 1, v[5] };
            if (sub == 1) { s->selWin[k] = w; t->menuSet[k] = true; } else { s->msgWin[k] = w; t->msgSet[k] = true; }
        } else if (sub == 2) {
            s->curSel = k;
            s->selFrameOn = v[1];
        } else if (sub == 4) {
            s3_msg_select_window(s, k, v[1], v[4], v[5]);
        }
        return;
    }
    case 'E': e_args(s, v, NULL, 6); if (v[0] <= 100) { s->clrColor[v[0]] = v[1]; s->clr[v[0]] = (S3Win){ col(s, v[2]), v[3], col(s, v[4]) - col(s, v[2]) + (t->columns ? 8 : 1), v[5] - v[3] + 1, 0 }; } return;
    case 'G': e_args(s, v, NULL, 1); s3_load_cg(s, v[0], -1, t->dstScreen * 480); return;
    case 'U': e_args(s, v, NULL, 2); s3_load_cg(s, v[0], v[1], t->dstScreen * 480); return;
    case 'J': e_args(s, v, NULL, 2); s->cgX = col(s, v[0]); s->cgY = v[1]; s->cgPos = true; s->cgPosMode = 30; return;
    case 'H': {
        int len = s3_byte(s);
        e_args(s, v, NULL, 1);
        if (len >= 9) return;
        char d[16];
        snprintf(d, sizeof d, "%u", v[0]);
        size_t n = strlen(d), o = 0;
        if (len > 0 && (int)n > len) memmove(d, d + n - (size_t)len, (size_t)len + 1);
        else if (len > 0) { char p[16]; snprintf(p, sizeof p, "%0*u", len, v[0]); memcpy(d, p, sizeof d); }
        for (const char *q = d; *q && o + 3 < sizeof buf; q++) { buf[o++] = (char)0x82; buf[o++] = (char)(0x4f + (*q - '0')); }
        buf[o] = 0;
        s3_msg_text(s, buf);
        return;
    }
    case 'I': {
        e_args(s, v, NULL, 6);
        int sx = col(s, v[0]), sy = v[1], w = (t->columns ? v[2] * 8 + 8 : v[2] + 1) - sx, h = v[3] - sy + 1;
        s3_copy(s, sx, sy + t->srcScreen * 480, w, h, col(s, v[4]), v[5] + t->dstScreen * 480);
        return;
    }
    case 'K': { // input: waits for a key or click (modes 0 and 6) and returns it in RND; mode 2 reads at once
        int mode = s3_byte(s);
        s->polls++;
        u32 m = keys_now(s) | (s->pressedMask & 0xff);
        if (mode == 2 || m) { s->pressedMask = 0; s3_var_set(s, 0, (u16)m); s3_var_set(s, 57, (u16)s->mouseX); s3_var_set(s, 58, (u16)s->mouseY); return; }
        s->pressedMask = 0;
        s->wait = S3_WAIT_INPUT_POLL;
        return;
    }
    case 'L': e_args(s, v, NULL, 1); if (!load30(s, v[0])) s3_var_set(s, 0, 255); return;
    case 'Q': e_args(s, v, NULL, 1); s3_var_set(s, 0, save30(s, v[0]) ? 1 : 255); return;
    case 'M': {
        const char *p = (const char *)s->code + s->pc;
        size_t n = 0;
        while (s->pc + n < s->codeSize && p[n] != ':') n += s3_sjis_lead((u8)p[n]) ? 2 : 1;
        s->pc += (u32)n + 1;
        char *dst = text_var(s, t->textIndex);
        if (dst) snprintf(dst, TEXT_LEN, "%.*s", (int)n, p);
        return;
    }
    case 'N': s3_byte(s); e_args(s, v, NULL, 2); return;
    case 'O': {
        e_args(s, v, NULL, 2);
        if (v[0] == 0) for (int i = 0; i < 16; i++) s3_var_set(s, (u16)(41 + i), (v[1] >> i) & 1);
        else { u16 r = 0; for (int i = 0; i < 16; i++) if (s3_var_get(s, (u16)(41 + i))) r |= (u16)(1 << i); s3_var_set(s, v[1], r); }
        return;
    }
    case 'P': e_args(s, v, NULL, 4); { int i = v[0] & 255; s->pal[i][0] = (u8)(v[1] * 17); s->pal[i][1] = (u8)(v[2] * 17); s->pal[i][2] = (u8)(v[3] * 17); memcpy(s->shownPal[i], s->pal[i], 3); s3_mark_dirty(s); } return;
    case 'S': {
        int n = s3_byte(s);
        if (n == 0) s3_music_stop(s);
        else if (n < 100 && t->cdTrack[n]) s3_music_play(s, t->cdTrack[n], 0);
        return;
    }
    case 'T': e_args(s, v, NULL, 2); s->tx = col(s, v[0]); s->ty = v[1]; return;
    case 'V': {
        e_args(s, v, NULL, 2);
        int k = v[1];
        u8 *slot = t->img + IMG_STACKS + (size_t)k * STACK_SIZE;
        if (k > 28) return;
        state_out(s);
        // U01-U20 and the text variables, as the original copies them; stack 0 shares memory with other state.
        if (v[0] == 0) {
            // SYSTEM3.EXE adds each value's low byte as a signed byte, so 0x01ff comes back as 0x00ff. Games may
            // depend on it, so the runtime does the same.
            for (int i = 0; i < 20; i++) put16(t->img + IMG_VARS + (21 + i) * 2, (slot[i * 2 + 1] << 8) + (int8_t)slot[i * 2]);
            memcpy(t->img + IMG_TEXT, slot + 40, 10 * TEXT_LEN);
        } else {
            u8 tmp[STACK_SIZE];
            memcpy(tmp, t->img + IMG_VARS + 21 * 2, 40);
            memcpy(tmp + 40, t->img + IMG_TEXT, 10 * TEXT_LEN);
            memcpy(slot, tmp, STACK_SIZE);
        }
        state_in(s);
        return;
    }
    case 'W': e_args(s, v, NULL, 3); return;
    case 'X': { int n = s3_byte(s); const char *x = text_var(s, n); if (x) text30(s, x); return; }
    case 'Y': e_args(s, v, NULL, 2); cmd_y30(s, v[0], v[1]); return;
    case 'Z': {
        e_args(s, v, NULL, 2);
        if (v[0] <= 7) {
            t->colours[v[0]] = v[1];
            switch (v[0]) {
            case 0: s->palBank = v[1]; break;
            case 1: s->msgColor = v[1]; break;
            case 2: s->menuColor = v[1]; break;
            case 3: s->menuFrame = v[1]; break;
            case 4: s->menuBg = v[1]; break;
            case 5: s->msgFrame = v[1]; break;
            case 6: s->msgBg = v[1]; break;
            }
        } else if (v[0] > 100 && v[0] < 200) t->cdTrack[v[0] - 100] = v[1] ? v[1] + 1 : 0;
        return;
    }
    default: s->vmError = true;
    }
    (void)lv;
}

static void inline_text30(S3 *s) {
    char buf[512];
    int n = 0;
    while (s->pc < s->codeSize && n < (int)sizeof buf - 3) {
        u8 c = s->code[s->pc];
        if (s3_sjis_lead(c)) { buf[n++] = (char)c; buf[n++] = (char)s->code[s->pc + 1]; s->pc += 2; }
        else if ((c >= 0xa0 && c <= 0xdf) || c == ' ') { buf[n++] = (char)c; s->pc++; }
        else break;
    }
    buf[n] = 0;
    text30(s, buf);
}

void s3_exec30(S3 *s) {
    s->cmdStart = s->pc;
    int c = s3_byte(s);
    S3LV lv;
    switch (c) {
    case 0x1a: s->vmError = true; return; // end of page data
    case '!': s3_var_ref(s, s3_byte(s), &lv); s3_lv_set(s, &lv, 0, s3_expr(s)); return;
    case '{': { u16 cond = s3_expr(s); u16 to = s3_u16(s); if (!cond) s->pc = to; return; }
    case '@': s->pc = s3_u16(s); return;
    case '\\': { u16 to = s3_u16(s); if (to) s3_label_call(s, to); else s3_return(s, false); return; }
    case '&': s3_goto_page(s, s3_expr(s), 0); return;
    case '%': { int p = s3_expr(s); if (p) s3_page_call(s, p, 0); else s3_return(s, true); return; }
    case '$':
        if (s->menuBuild >= 0) { s->menuBuild = -1; return; }
        if (s->menuCount < S3_MAX_MENU) {
            s->menuBuild = s->menuCount++;
            s->menu[s->menuBuild].page = s->page;
            s->menu[s->menuBuild].addr = s3_u16(s);
            s->menu[s->menuBuild].text[0] = 0;
        } else { s3_u16(s); s->menuBuild = S3_MAX_MENU; }
        return;
    case ']': s3_cmd_open_menu(s); return;
    case 'A': s3_cmd_wait_key(s); return;
    case 'R': s3_msg_newline(s); return;
    case 'F': s->pc = 2; return;
    default: break;
    }
    if (s3_sjis_lead((u8)c) || (c >= 0xa0 && c <= 0xdf) || c == ' ') { s->pc--; inline_text30(s); return; }
    s->pc--;
    command30(s);
}

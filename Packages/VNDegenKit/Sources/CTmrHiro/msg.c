#include "th.h"

static bool voice_like(const char *s) {
    if (strlen(s) < 6) return false;
    for (const char *p = s; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_')) return false;
    return true;
}

static char *expand_newlines(const char *s) {
    char *out = th_alloc(strlen(s) + 1), *o = out;
    for (const u8 *p = (const u8 *)s; *p;) {
        if ((*p >= 0x81 && *p <= 0x9f) || (*p >= 0xe0 && *p <= 0xfc)) {
            *o++ = (char)*p++;
            if (*p) *o++ = (char)*p++;
            continue;
        }
        if (p[0] == '\\' && p[1] == 'n') {
            *o++ = '\n';
            p += 2;
            continue;
        }
        *o++ = (char)*p++;
    }
    *o = 0;
    return out;
}

static int text_speed_ms(TH *t) { return t->textSpeed >= 9 ? 0 : (9 - t->textSpeed) * 8; }

void msg_show(TH *t, int kind, const char *body) {
    free(t->msgName);
    free(t->msgText);
    t->msgName = NULL;
    t->msgText = NULL;
    t->msgVoice[0] = 0;
    char *copy = th_strdup(body);
    char *text = copy;
    if (kind != 0) {
        char *comma = strchr(copy, ',');
        if (comma) {
            *comma = 0;
            t->msgName = th_strdup(copy);
            text = comma + 1;
            char *last = strrchr(text, ',');
            if (last && voice_like(last + 1)) {
                snprintf(t->msgVoice, sizeof t->msgVoice, "%s", last + 1);
                *last = 0;
            }
        }
    }
    char *expanded = expand_newlines(text);
    if (t->msgName) {
        size_t n = strlen(expanded);
        char *q = th_alloc(n + 5);
        memcpy(q, "\x81\x75", 2);
        memcpy(q + 2, expanded, n);
        memcpy(q + 2 + n, "\x81\x76", 3);
        free(expanded);
        expanded = q;
    }
    t->msgText = expanded;
    free(copy);
    t->msgKind = kind;
    t->msgRead = read_flag(t, t->sceneId, t->savePc, true);
    t->msgStart = t->now;
    t->msgChars = text_count(t->msgText);
    t->msgShown = text_speed_ms(t) == 0 || t->skipMode ? t->msgChars : 0;
    t->winVisible = true;
    t->winHidden = false;
    t->wait = W_TEXT;
    t->autoAt = 0;
    if (t->msgVoice[0] && !t->skipMode) snd_play(t, CH_VOICE, PAC_VOICE, t->msgVoice, false, 0);
    t->backlog = th_realloc(t->backlog, sizeof(BacklogEntry) * (size_t)(t->backlogCount + 1));
    BacklogEntry *e = &t->backlog[t->backlogCount++];
    e->name = t->msgName ? th_strdup(t->msgName) : NULL;
    e->text = th_strdup(t->msgText);
    snprintf(e->voice, sizeof e->voice, "%s", t->msgVoice);
    if (t->backlogCount > 200) {
        free(t->backlog[0].name);
        free(t->backlog[0].text);
        memmove(t->backlog, t->backlog + 1, sizeof(BacklogEntry) * (size_t)(t->backlogCount - 1));
        t->backlogCount--;
    }
    t->dirty = true;
}

bool msg_complete(TH *t) { return t->msgShown >= t->msgChars; }

void msg_advance(TH *t) {
    if (!msg_complete(t)) {
        t->msgShown = t->msgChars;
        t->dirty = true;
        return;
    }
    t->wait = W_NONE;
    t->dirty = true;
}

static u32 parse_rgb(const char *v, u32 def) {
    if (!v) return def;
    u32 x = (u32)strtoul(v, NULL, 16);
    return (x >> 16 & 0xff) | (x & 0xff00) | (x & 0xff) << 16;
}

void msg_draw(TH *t) {
    if (!t->winVisible || t->winHidden || !t->msgText) return;
    if (t->wait != W_TEXT && t->wait != W_CHOICE) return;
    const Ini *s = &t->sys;
    int wx = ini_int(s, "MESSAGEWINDOW", "MW_DRAW_POS_X", 0), wy = ini_int(s, "MESSAGEWINDOW", "MW_DRAW_POS_Y", t->H - 200);
    Image *base = image_get(t, "sys_window_base");
    if (base && t->winBaseDraw) {
        u32 stat = t->winStat;
        u32 rgba = (stat >> 16 & 0xff) | (stat & 0xff00) | (stat & 0xff) << 16 | (stat >> 24) << 24;
        fill_rect(t->screen, t->W, t->H, wx + base->x, wy + base->y, base->w, base->h, rgba);
    }
    int size = ini_int(s, "MESSAGEWINDOW", "TEXT_FONT", 24);
    int tx = ini_int(s, "MESSAGEWINDOW", "TEXT_DRAW_POS_X", 100), ty = ini_int(s, "MESSAGEWINDOW", "TEXT_DRAW_POS_Y", 20);
    int next = ini_int(s, "MESSAGEWINDOW", "TEXT_DRAW_NEXT", size + 8);
    int front = ini_int(s, "MESSAGEWINDOW", "TEXT_FRONT_H", 0);
    u32 color = parse_rgb(ini_get(s, "MESSAGEWINDOW", t->msgRead && t->readColor ? "TEXT_READ_COLOR" : "TEXT_COLOR"), 0xffffff) | 0xff000000u;
    int npx = ini_int(s, "MESSAGEWINDOW", "NP_DRAW_POS_X", tx);
    int lineTop = wy + ty + next - 16;
    if (t->msgName) text_draw(t, t->screen, t->W, t->H, t->msgName, wx + npx, lineTop - next, size, color, true, -1);
    int brackets = ini_int(s, "MESSAGEWINDOW", "TEXT_BRACKETS_H", 1);
    int x0 = wx + tx + (t->msgName ? 0 : front * size);
    int xc = t->msgName ? wx + tx + brackets * size : x0;
    int x = x0, y = lineTop, shown = 0;
    int limit = t->msgShown;
    int right = t->W - tx;
    const u8 *p = (const u8 *)t->msgText;
    while (*p && shown < limit) {
        if (*p == '\n') {
            p++;
            shown++;
            x = xc;
            y += next;
            continue;
        }
        u32 cp;
        int len = th_sjis_decode(p, &cp);
        char ch[3] = {0};
        memcpy(ch, p, (size_t)len);
        int w = text_width(t, ch, size);
        if (x + w > right) {
            x = xc;
            y += next;
        }
        x = text_draw(t, t->screen, t->W, t->H, ch, x, y, size, color, true, -1);
        p += len;
        shown++;
    }
    if (msg_complete(t) && t->wait == W_TEXT) {
        Image *cw = image_get(t, "sys_click_wait");
        int cx = ini_int(s, "MESSAGEWINDOW", "CW_DRAW_POS_X", 0), cy = ini_int(s, "MESSAGEWINDOW", "CW_DRAW_POS_Y", 0);
        int cwW = ini_int(s, "MESSAGEWINDOW", "CW_DRAW_WIDTH", 36), cwH = ini_int(s, "MESSAGEWINDOW", "CW_DRAW_HEIGHT", 36);
        int pat = ini_int(s, "MESSAGEWINDOW", "CW_DRAW_PAT", 1), per = ini_int(s, "MESSAGEWINDOW", "CW_DRAW_TIME", 50);
        if (cw && pat > 0 && per > 0) {
            int frame = (int)((t->now / (u32)per) % (u32)pat);
            int cols = cw->cw / cwW > 0 ? cw->cw / cwW : 1;
            blit_part(t->screen, t->W, t->H, cw, (frame % cols) * cwW, (frame / cols) * cwH, cwW, cwH, wx + cx, wy + cy, 255);
        }
    }
}

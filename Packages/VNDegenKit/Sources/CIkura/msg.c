#include "ikura.h"

#define CURSOR_MS 200
#define NAME_ALPHA 0xc0

static int val(IK *t, const u8 *p) { return (int)ik_value(t, rd32(p)); }

static u32 abgr(IK *t, const u8 *p) {
    u32 a = (u32)clampi(val(t, p), 0, 255), b = (u32)clampi(val(t, p + 4), 0, 255), g = (u32)clampi(val(t, p + 8), 0, 255),
        r = (u32)clampi(val(t, p + 12), 0, 255);
    return a << 24 | r << 16 | g << 8 | b;
}

static void clear(MsgWin *m) {
    m->count = m->revealed = 0;
    m->rawLen = 0;
    if (m->raw) m->raw[0] = 0;
    m->penX = m->x;
    m->penY = m->y;
    m->name = -1;
}

static void push_char(MsgWin *m, u32 cp, int x, int y) {
    if (m->count == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 256;
        m->chars = ik_realloc(m->chars, sizeof(MsgChar) * (size_t)m->cap);
    }
    m->chars[m->count++] = (MsgChar){cp, x, y};
}

static void newline(MsgWin *m) {
    m->penX = m->x;
    m->penY += m->pitch;
}

static void layout(IK *t, const u32 *cps, int n) {
    MsgWin *m = &t->msg;
    int i = 0;
    while (i < n) {
        if (cps[i] == '\n') {
            newline(m);
            i++;
            continue;
        }
        int j = i;
        if (cps[j] == ' ') j++;
        while (j < n && cps[j] != ' ' && cps[j] != '\n' && cps[j] < 0x80) j++;
        if (j == i) j++;
        int w = 0;
        for (int k = i; k < j; k++) w += text_advance(t, m->font, cps[k]);
        if (m->penX + w > m->right && m->penX > m->x && w <= m->right - m->x) newline(m);
        for (int k = i; k < j; k++) {
            int a = text_advance(t, m->font, cps[k]);
            if (m->penX + a > m->right && m->penX > m->x) newline(m);
            push_char(m, cps[k], m->penX, m->penY);
            m->penX += a;
        }
        i = j;
    }
}

static void add_raw(MsgWin *m, const u8 *s, int len) {
    if (m->rawLen + len + 1 > m->rawCap) {
        m->rawCap = (m->rawLen + len + 1) * 2;
        m->raw = ik_realloc(m->raw, (size_t)m->rawCap);
    }
    memcpy(m->raw + m->rawLen, s, (size_t)len);
    m->rawLen += len;
    m->raw[m->rawLen] = 0;
}

static void add_text(IK *t, const u8 *s, int len) {
    add_raw(&t->msg, s, len);
    u32 *cps = ik_alloc(sizeof(u32) * (size_t)(len + 1));
    int n = 0;
    for (int i = 0; i < len;) {
        u32 cp;
        int k = ik_sjis_decode(s + i, &cp);
        if (k <= 0) k = 1;
        i += k;
        cps[n++] = cp;
    }
    if (t->msg.revealed == t->msg.count) t->msg.revealAt = t->now + (u32)t->msg.msPerChar;
    layout(t, cps, n);
    free(cps);
}

static void finish(IK *t) {
    t->msg.tags = NULL;
    if (t->wait == W_MSG) t->wait = W_NONE;
}

static void run_tags(IK *t) {
    MsgWin *m = &t->msg;
    while (m->tags && m->tagPos < m->tagLen) {
        if (m->revealed < m->count) return;
        const u8 *p = m->tags + m->tagPos;
        int left = m->tagLen - m->tagPos;
        switch (p[0]) {
        case 0x00: finish(t); return;
        case 0x02:
            m->cursor = true;
            m->cursorAt = t->now;
            t->dirty = true;
            return;
        case 0x03:
            clear(m);
            t->dirty = true;
            m->tagPos++;
            break;
        case 0x04:
            if (left < 2) goto bad;
            m->name = p[1] == 0xff ? -1 : p[1];
            t->dirty = true;
            m->tagPos += 2;
            break;
        case 0x06: m->tagPos++; break;
        case 0x0a: {
            if (left < 5) goto bad;
            char num[16];
            int k = snprintf(num, sizeof num, "%d", (int)ik_value(t, 0x80000000u | rd32(p + 1)));
            add_text(t, (const u8 *)num, k);
            m->tagPos += 5;
            break;
        }
        case 0x13:
            if (left < 13) goto bad;
            if (!t->skipMode) {
                char name[13];
                memcpy(name, p + 1, 12);
                name[12] = 0;
                snd_voice(t, name);
            }
            m->tagPos += 13;
            break;
        case 0xff: {
            if (left < 3) goto bad;
            int len = rd16(p + 1);
            if (3 + len > left) goto bad;
            add_text(t, p + 3, len);
            m->tagPos += 3 + len;
            break;
        }
        default: goto bad;
        }
    }
    finish(t);
    return;
bad:
    ik_log(t, "%s: message tag %02x not handled", t->sc ? t->sc->name : "-", m->tags[m->tagPos]);
    finish(t);
}

static void stop_mode(IK *t, int which) {
    if (which) t->skipMode = 0;
    else t->autoMode = 0;
    int b = t->modeButton[which];
    if (b >= 0 && b < IK_BUTTONS && t->buttons[b].made && t->buttons[b].mode == 2) t->buttons[b].mode = 1;
    t->dirty = true;
}

void msg_stop_modes(IK *t) {
    if (t->skipMode) stop_mode(t, 1);
    if (t->autoMode) stop_mode(t, 0);
}

void msg_tick(IK *t) {
    MsgWin *m = &t->msg;
    if (t->wait != W_MSG || !m->tags) return;
    if ((t->skipMode || t->autoMode) && (t->click || t->rclick)) {
        msg_stop_modes(t);
        t->click = false;
    }
    if (t->skipMode && m->revealed < m->count) {
        m->revealed = m->count;
        t->dirty = true;
    }
    if (m->revealed < m->count) {
        if (t->click) {
            t->click = false;
            m->revealed = m->count;
            t->dirty = true;
        } else {
            while (m->revealed < m->count && (i32)(t->now - m->revealAt) >= 0) {
                m->revealed++;
                m->revealAt += (u32)m->msPerChar;
                t->dirty = true;
            }
        }
        if (m->revealed < m->count) return;
    }
    if (m->cursor) {
        if ((t->now - m->cursorAt) / CURSOR_MS != (t->now - 17 - m->cursorAt) / CURSOR_MS) t->dirty = true;
        bool go = t->click;
        if (t->skipMode) go = true;
        else if (t->autoMode && !snd_voice_playing(t)) {
            if (!t->autoAt) t->autoAt = t->now + (u32)clampi(t->settings[3], 0, 1000) * 2u;
            go = (i32)(t->now - t->autoAt) >= 0;
        }
        if (!go) return;
        t->autoAt = 0;
        t->click = false;
        m->cursor = false;
        m->tagPos++;
        snd_voice_stop(t);
        t->dirty = true;
    }
    run_tags(t);
}

bool msg_exec(IK *t, u16 op, const u8 *a, int n) {
    MsgWin *m = &t->msg;
    switch (op) {
    case 0x0900:
        if (n < 24) break;
        m->layer = val(t, a);
        m->x = val(t, a + 4);
        m->y = val(t, a + 8);
        m->right = val(t, a + 12);
        m->bottom = val(t, a + 16);
        m->pitch = val(t, a + 20);
        clear(m);
        break;
    case 0x0901:
        if (n < 28) break;
        m->nameLayer = val(t, a);
        m->nameX = val(t, a + 4);
        m->nameY = val(t, a + 8);
        m->nameW = val(t, a + 12);
        m->nameH = val(t, a + 16);
        m->nameImg = val(t, a + 20);
        m->nameCount = clampi(val(t, a + 24), 0, 32);
        break;
    case 0x0902: {
        int i = val(t, a);
        if (i >= 0 && i < 32 && n >= 12) {
            m->nameSrc[i][0] = val(t, a + 4);
            m->nameSrc[i][1] = val(t, a + 8);
        }
        break;
    }
    case 0x0903:
        if (n < 28) break;
        m->curLayer = val(t, a);
        m->curX = val(t, a + 4);
        m->curY = val(t, a + 8);
        m->curW = val(t, a + 12) - m->curX;
        m->curH = val(t, a + 16) - m->curY;
        m->curImg = val(t, a + 20);
        m->curFrames = clampi(val(t, a + 24), 0, 16);
        break;
    case 0x0904: {
        int i = val(t, a);
        if (i >= 0 && i < 16 && n >= 12) {
            m->curSrc[i][0] = val(t, a + 4);
            m->curSrc[i][1] = val(t, a + 8);
        }
        break;
    }
    case 0x090f:
        if (n < 36) break;
        m->font = val(t, a) & (IK_FONTS - 1);
        m->colour = abgr(t, a + 4);
        m->shadow = abgr(t, a + 20);
        break;
    case 0x0908:
        if (n < 4) break;
        m->tags = a + 4;
        m->tagLen = n - 4;
        m->tagPos = 0;
        m->cursor = false;
        t->wait = W_MSG;
        run_tags(t);
        break;
    case 0x0910: t->msgMode = val(t, a); break;
    case 0x0911:
    case 0x090d:
    case 0x090e:
    case 0x0905:
    case 0x0906:
    case 0x0907:
    case 0x0909:
    case 0x090a:
    case 0x090b:
    case 0x090c: break;
    default: return false;
    }
    return true;
}

void msg_draw(IK *t, int layer) {
    MsgWin *m = &t->msg;
    if (layer != m->layer || layer < 0 || layer >= t->layerCount) return;
    Layer *l = &t->layers[layer];
    int alpha = clampi(l->argb[0], 0, 255);
    if (!alpha) return;
    if (m->name >= 0 && m->name < m->nameCount)
        gfx_blit_min(t, l->argb, m->nameImg, m->nameSrc[m->name][0], m->nameSrc[m->name][1], m->nameW, m->nameH,
                     l->x + m->nameX, l->y + m->nameY, NAME_ALPHA);
    int clip[4] = {l->x, l->y, l->x + l->w, l->y + l->h};
    u32 colour = (m->colour >> 24) * (u32)alpha / 255 << 24 | (m->colour & 0xffffff);
    u32 shadow = (m->shadow >> 24) * (u32)alpha / 255 << 24 | (m->shadow & 0xffffff);
    for (int i = 0; i < m->revealed; i++)
        text_draw(t, m->font, m->chars[i].cp, l->x + m->chars[i].x, l->y + m->chars[i].y, colour, shadow, clip);
    if (m->cursor && m->curFrames > 0) {
        int f = (int)((t->now - m->cursorAt) / CURSOR_MS % (u32)m->curFrames);
        gfx_blit(t, l->argb, m->curImg, m->curSrc[f][0], m->curSrc[f][1], m->curW, m->curH, l->x + m->curX, l->y + m->curY);
    }
}

void msg_close(IK *t) {
    free(t->msg.chars);
    t->msg.chars = NULL;
    free(t->msg.raw);
    t->msg.raw = NULL;
}

#include "bsx.h"

static void four(BX *b, const char *sec, const char *key, int *out, int d0, int d1, int d2, int d3) {
    int v[4] = {d0, d1, d2, d3};
    bx_cfg_ints(&b->cfg, sec, key, v, 4);
    memcpy(out, v, sizeof v);
}

void bx_msg_init(BX *b) {
    const Cfg *c = &b->cfg;
    const char *base = bx_cfg_get(c, "message window", "base window");
    b->msgBase = base ? bx_image_load(b, base) : NULL;
    if (b->msgBase) {
        Image *copy = bx_image_new(b->msgBase->w, b->msgBase->h);
        memcpy(copy->px, b->msgBase->px, sizeof(u32) * (size_t)copy->w * (size_t)copy->h);
        b->msgBase = copy;
    }
    int pos[2] = {0, b->H - 136};
    bx_cfg_ints(c, "message window", "base pos", pos, 2);
    b->msgX = pos[0];
    b->msgY = pos[1];
    four(b, "message window", "name", b->nameRect, 80, 6, 460, 24);
    four(b, "message window", "text", b->textRect, 128, 40, 480, 72);
    b->nameColor = bx_cfg_color(c, "message window", "name color", 0xffffffffu);
    b->textColor = 0xffffffffu;
    b->bgColor = bx_cfg_color(c, "message window", "bg color", 0);
    b->fontH = bx_cfg_int(c, "default font", "height", 20);
    b->fontSize = b->fontH;
    b->lines = bx_cfg_int(c, "default font", "number of lines", 3);
    b->edge = bx_cfg_int(c, "default font", "edge", 1);
    b->shadow = bx_cfg_int(c, "default font", "shadow", 0);
    b->msgSpeed = 30;
    const char *una = bx_cfg_get(c, "message window", "unagashi icon");
    if (una) {
        Image *u = bx_image_load(b, una);
        if (u) {
            b->unagashi = bx_image_new(u->w, u->h);
            memcpy(b->unagashi->px, u->px, sizeof(u32) * (size_t)u->w * (size_t)u->h);
        }
    }
    int up[2] = {0, 0}, ui[4] = {32, 32, 1, 1};
    bx_cfg_ints(c, "message window", "unagashi pos", up, 2);
    bx_cfg_ints(c, "message window", "unagashi info", ui, 4);
    b->unaX = up[0];
    b->unaY = up[1];
    b->unaW = ui[0] > 0 ? ui[0] : 32;
    b->unaH = ui[1] > 0 ? ui[1] : 32;
    b->unaFrames = ui[3] > 0 ? ui[3] : 1;
    b->unaSpeed = bx_cfg_int(c, "message window", "unagashi speed", 30);
    if (b->unaSpeed < 16) b->unaSpeed = 16;
    const char *item = bx_cfg_get(c, "select window", "design0 item");
    if (item) {
        Image *im = bx_image_load(b, item);
        if (im) {
            b->choiceItem[0] = bx_image_new(im->w, im->h);
            memcpy(b->choiceItem[0]->px, im->px, sizeof(u32) * (size_t)im->w * (size_t)im->h);
        }
        char hit[80];
        snprintf(hit, sizeof hit, "%s_hit", item);
        im = bx_image_load(b, hit);
        if (im) {
            b->choiceItem[1] = bx_image_new(im->w, im->h);
            memcpy(b->choiceItem[1]->px, im->px, sizeof(u32) * (size_t)im->w * (size_t)im->h);
        }
    }
    b->choiceDatum = bx_cfg_int(c, "select window", "vertical datum line", b->H / 2);
}

void bx_msg_clear(BX *b) {
    free(b->speaker);
    free(b->text);
    free(b->voice);
    b->speaker = b->text = b->voice = NULL;
    b->textLen = b->textShown = 0;
    b->textDone = true;
}

static int glyph_count(const char *s) {
    int n = 0;
    while (*s) {
        u32 cp;
        s += bx_utf8_decode(s, &cp);
        n++;
    }
    return n;
}

static char *wrap(BX *b, const char *text) {
    int maxW = b->textRect[2];
    size_t n = strlen(text);
    char *out = bx_alloc(n * 2 + 8);
    size_t o = 0;
    int lineW = 0;
    const char *p = text;
    while (*p) {
        if (*p == '\n') {
            out[o++] = '\n';
            lineW = 0;
            p++;
            continue;
        }
        const char *w = p;
        u32 cp;
        int len = bx_utf8_decode(w, &cp);
        if (cp == ' ' || cp >= 0x2e80) w += len;
        else
            while (*w && *w != ' ' && *w != '\n') {
                w += bx_utf8_decode(w, &cp);
                if (cp >= 0x2e80) break;
            }
        char word[512];
        size_t wl = (size_t)(w - p) < sizeof word - 1 ? (size_t)(w - p) : sizeof word - 1;
        memcpy(word, p, wl);
        word[wl] = 0;
        int ww = bx_text_width(word, b->fontSize);
        if (lineW + ww > maxW && lineW > 0 && word[0] != ' ') {
            while (o && out[o - 1] == ' ') o--;
            out[o++] = '\n';
            lineW = 0;
        }
        if (!(lineW == 0 && word[0] == ' ')) {
            memcpy(out + o, word, wl);
            o += wl;
            lineW += ww;
        }
        p += wl ? wl : 1;
    }
    out[o] = 0;
    return out;
}

void bx_msg_show(BX *b, const char *name, const char *text, const char *voice) {
    bx_msg_clear(b);
    b->speaker = name ? bx_strdup(name) : NULL;
    b->text = wrap(b, text);
    b->voice = voice ? bx_strdup(voice) : NULL;
    b->textLen = glyph_count(b->text);
    b->textShown = 0;
    b->textStart = b->now;
    b->textDone = false;
    b->winShown = true;
    b->winHidden = false;
    b->autoAt = 0;
    if (voice) bx_snd_play(b, CH_VOICE, RX_VOICE, voice, false, 0);
    if (b->logCount == 512) {
        free(b->logs[0].name);
        free(b->logs[0].text);
        free(b->logs[0].voice);
        memmove(b->logs, b->logs + 1, sizeof(LogEntry) * 511);
        b->logCount--;
    }
    if (!b->logs) b->logs = bx_alloc(sizeof(LogEntry) * 512);
    LogEntry *l = &b->logs[b->logCount++];
    l->name = name ? bx_strdup(name) : NULL;
    l->text = bx_strdup(text);
    l->voice = voice ? bx_strdup(voice) : NULL;
    if (b->skipMode) bx_msg_reveal(b);
}

void bx_msg_update(BX *b) {
    if (b->textDone || !b->text) return;
    int shown = b->msgSpeed <= 0 ? b->textLen : (int)((b->now - b->textStart) / (u32)b->msgSpeed);
    if (shown >= b->textLen) {
        shown = b->textLen;
        b->textDone = true;
    }
    b->textShown = shown;
}

bool bx_msg_complete(BX *b) { return b->textDone; }

void bx_msg_reveal(BX *b) {
    b->textShown = b->textLen;
    b->textDone = true;
}

void bx_msg_draw(BX *b, u32 *dst) {
    if (!b->winShown || b->winHidden || b->transActive) return;
    if (b->msgBase) bx_blit(dst, b->W, b->H, b->msgBase, b->msgX, b->msgY, 255);
    if (!b->text) return;
    if (b->speaker && *b->speaker)
        bx_text_draw(b, dst, b->W, b->H, b->speaker, b->msgX + b->nameRect[0], b->msgY + b->nameRect[1], b->fontSize, b->nameColor, b->edge, b->shadow);
    int lineH = b->lines > 0 ? b->textRect[3] / b->lines : b->fontH;
    if (lineH < b->fontH) lineH = b->fontH;
    int x = b->msgX + b->textRect[0], y = b->msgY + b->textRect[1];
    char line[1024];
    size_t ln = 0;
    int count = 0;
    for (const char *p = b->text; count <= b->textShown;) {
        u32 cp;
        int len = *p ? bx_utf8_decode(p, &cp) : 0;
        if (!*p || cp == '\n' || count == b->textShown) {
            line[ln] = 0;
            bx_text_draw(b, dst, b->W, b->H, line, x, y, b->fontSize, b->textColor, b->edge, b->shadow);
            ln = 0;
            y += lineH;
            if (!*p || count == b->textShown) break;
            p += len;
            count++;
            continue;
        }
        if (ln + (size_t)len < sizeof line - 1) {
            memcpy(line + ln, p, (size_t)len);
            ln += (size_t)len;
        }
        p += len;
        count++;
    }
    if (b->textDone && b->unagashi && b->th.wait == W_TEXT) {
        int frame = (int)(b->now / (u32)b->unaSpeed) % b->unaFrames;
        int cols = b->unagashi->w / b->unaW;
        if (cols < 1) cols = 1;
        bx_blit_part(dst, b->W, b->H, b->unagashi, (frame % cols) * b->unaW, (frame / cols) * b->unaH, b->unaW, b->unaH,
                     b->msgX + b->unaX, b->msgY + b->unaY, 255);
    }
}

void bx_choice_begin(BX *b) {
    if (!b->choiceCount) {
        bx_log(b, "choice with no options");
        return;
    }
    Image *item = b->choiceItem[0];
    int w = item ? item->w : 520, h = item ? item->h : 36;
    int gap = bx_cfg_int(&b->cfg, "select window", "design0 interval", 0);
    int total = b->choiceCount * h + (b->choiceCount - 1) * gap;
    int y = b->choiceDatum - total / 2;
    for (int i = 0; i < b->choiceCount; i++) {
        Choice *c = &b->choice[i];
        c->w = w;
        c->h = h;
        c->x = (b->W - w) / 2;
        c->y = y + i * (h + gap);
    }
    b->choiceHover = -1;
    b->th.wait = W_CHOICE;
}

void bx_choice_update(BX *b) {
    b->choiceHover = -1;
    for (int i = 0; i < b->choiceCount; i++) {
        Choice *c = &b->choice[i];
        if (b->mx >= c->x && b->mx < c->x + c->w && b->my >= c->y && b->my < c->y + c->h) b->choiceHover = i;
    }
    if (b->keyPress[0x26] || b->keyPress[0x28]) {
        int d = b->keyPress[0x28] ? 1 : -1;
        int h = b->choiceHover < 0 ? (d > 0 ? 0 : b->choiceCount - 1) : (b->choiceHover + d + b->choiceCount) % b->choiceCount;
        b->mx = b->choice[h].x + b->choice[h].w / 2;
        b->my = b->choice[h].y + b->choice[h].h / 2;
        b->choiceHover = h;
    }
    if (b->click && b->choiceHover >= 0) {
        b->click = false;
        int label = b->choice[b->choiceHover].label;
        b->choiceCount = 0;
        b->th.wait = W_NONE;
        bx_vm_goto(b, label);
    }
}

void bx_choice_draw(BX *b, u32 *dst) {
    if (b->th.wait != W_CHOICE) return;
    int tp[2] = {b->choiceItem[0] ? b->choiceItem[0]->w / 2 : 260, 6};
    bx_cfg_ints(&b->cfg, "select window", "design0 text pos", tp, 2);
    u32 normal = bx_cfg_color(&b->cfg, "select window", "design0 default color", 0xffffffffu);
    u32 hover = bx_cfg_color(&b->cfg, "select window", "design0 select color", 0xff8844ffu);
    for (int i = 0; i < b->choiceCount; i++) {
        Choice *c = &b->choice[i];
        Image *im = b->choiceItem[i == b->choiceHover && b->choiceItem[1] ? 1 : 0];
        if (im) bx_blit(dst, b->W, b->H, im, c->x, c->y, 255);
        else bx_fill(dst, b->W, b->H, c->x, c->y, c->w, c->h, 0xa0000000u);
        const char *t = c->msg >= 0 && c->msg < b->sc->msgCount ? b->sc->msgs[c->msg] : "";
        int tw = bx_text_width(t, b->fontSize);
        bx_text_draw(b, dst, b->W, b->H, t, c->x + tp[0] - tw / 2, c->y + tp[1], b->fontSize, i == b->choiceHover ? hover : normal, b->edge, b->shadow);
    }
}

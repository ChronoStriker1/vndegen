#include "sg.h"

enum { ST_RUN, ST_MESSAGE, ST_SCROLL, ST_WAIT, ST_CHOICE, ST_BUSY };

static const int kTextPx = 24;
static const int kLinePx = 36;
static const int kPageLines = 3;
static const float kScrollPxPerSec = 30.0f;
static const int kWinX = 120, kWinW = 720, kWinH = 160;
static const int kTextPadX = 18, kTextPadY = 19;
static const int kWrapW = 684;
static const float kRevealPerMs = 0.12f;

static void clear_pages(Interp *ip) {
    for (int i = 0; i < ip->pageCount; i++) free(ip->pages[i]);
    free(ip->pages);
    ip->pages = NULL;
    ip->pageCount = ip->page = 0;
}

static void clear_choices(SgaState *a) {
    for (int i = 0; i < a->choiceCount; i++) free(a->choices[i]);
    free(a->choices);
    a->choices = NULL;
    a->choiceCount = 0;
}

static void push_page(Interp *ip, const char *s, size_t n) {
    char *page = sg_alloc(n + 1);
    memcpy(page, s, n);
    ip->pages = sg_realloc(ip->pages, sizeof(char *) * (size_t)(ip->pageCount + 1));
    ip->pages[ip->pageCount++] = page;
}

static bool no_line_start(const char *ch) {
    static const char *set[] = {"、", "。", "，", "．", "・", "：", "；", "？", "！", "ー", "）", "］", "｝", "」", "』", "〕", "〉", "》", "】", "ぁ",
                                "ぃ", "ぅ", "ぇ", "ぉ", "っ", "ゃ", "ゅ", "ょ", "ゎ", "ァ", "ィ", "ゥ", "ェ", "ォ", "ッ", "ャ", "ュ", "ョ", "ヮ",
                                "ヵ", "ヶ", "…", "‥", "･", ".", ",", "!", "?", ")", "]", "}"};
    for (size_t i = 0; i < sizeof set / sizeof set[0]; i++)
        if (strcmp(ch, set[i]) == 0) return true;
    return false;
}

static char *wrap_text(SG *g, const char *text, int width) {
    size_t n = strlen(text);
    char *out = sg_alloc(n * 2 + 1);
    size_t o = 0, lastChar = 0;
    int lineW = 0, lastW = 0;
    const char *p = text;
    while (*p) {
        if (*p == '\r') {
            p++;
            continue;
        }
        if (*p == '\n') {
            out[o++] = '\n';
            lineW = 0;
            p++;
            continue;
        }
        u32 cp;
        int len = utf8_decode(p, &cp);
        char ch[8] = {0};
        memcpy(ch, p, (size_t)len);
        int cw = font_width(g->font, kTextPx, ch);
        if (lineW + cw > width && lineW > 0) {
            if (no_line_start(ch) && lineW > lastW) {
                memmove(out + lastChar + 1, out + lastChar, o - lastChar);
                out[lastChar] = '\n';
                o++;
                out[o] = 0;
                lineW = font_width(g->font, kTextPx, out + lastChar + 1);
            } else {
                out[o++] = '\n';
                lineW = 0;
            }
        }
        lastChar = o;
        lastW = lineW;
        memcpy(out + o, p, (size_t)len);
        o += (size_t)len;
        lineW += cw;
        p += len;
    }
    out[o] = 0;
    return out;
}

static void split_pages(SG *g, Interp *ip, const char *text) {
    clear_pages(ip);
    char *wrapped = wrap_text(g, text, kWrapW);
    const char *p = wrapped;
    while (*p) {
        const char *brk = strstr(p, "\n\n");
        const char *end = brk ? brk : p + strlen(p);
        while (p < end) {
            const char *q = p;
            for (int lines = 0; q < end && lines < kPageLines; lines++) {
                const char *nl = memchr(q, '\n', (size_t)(end - q));
                q = nl ? nl + 1 : end;
            }
            size_t n = (size_t)(q - p);
            while (n > 0 && p[n - 1] == '\n') n--;
            if (n > 0) push_page(ip, p, n);
            p = q;
        }
        while (*p == '\n') p++;
    }
    if (!ip->pageCount) push_page(ip, "", 0);
    free(wrapped);
}

static int page_chars(const char *s) {
    int n = 0;
    while (*s) {
        u32 cp;
        s += utf8_decode(s, &cp);
        n++;
    }
    return n;
}

static void runner_reset(Interp *ip) {
    script_free(&ip->script);
    clear_pages(ip);
    clear_choices(&ip->a);
    ip->active = false;
    ip->pc = 0;
    ip->waitMs = 0;
    ip->state = ST_RUN;
    ip->scrollText = NULL;
    ip->scroll = 0;
    ip->shownMs = 0;
    ip->a.windowOpen = false;
    ip->a.msgCmd = 0;
    free(ip->a.lastPage);
    ip->a.lastPage = NULL;
}

void interp_free(Interp *ip) {
    runner_reset(ip);
    SgaState *a = &ip->a;
    free(a->items);
    free(a->floor);
    for (int i = 0; i < a->evCount; i++) {
        for (int k = 0; k < a->ev[i].sheetCount; k++) free(a->ev[i].sheets[k].motion);
        free(a->ev[i].sheets);
        free(a->ev[i].motion);
    }
    free(a->ev);
    for (int i = 0; i < a->faceCount; i++) {
        free(a->faces[i].key);
        img_free(a->faces[i].im);
    }
    free(a->faces);
    memset(ip, 0, sizeof *ip);
}

static Script script_copy(const Script *s) {
    Script c = *s;
    c.cmds = sg_alloc(sizeof(Cmd) * (size_t)(s->count ? s->count : 1));
    for (int i = 0; i < s->count; i++) {
        c.cmds[i] = s->cmds[i];
        c.cmds[i].args = sg_alloc(sizeof(CmdArg) * (size_t)(s->cmds[i].argCount ? s->cmds[i].argCount : 1));
        for (int k = 0; k < s->cmds[i].argCount; k++) {
            c.cmds[i].args[k] = s->cmds[i].args[k];
            if (s->cmds[i].args[k].s) c.cmds[i].args[k].s = sg_strdup(s->cmds[i].args[k].s);
        }
    }
    return c;
}

void interp_start(SG *g, const Script *s) {
    Interp *ip = &g->interp;
    runner_reset(ip);
    ip->script = script_copy(s);
    ip->active = true;
    ip->state = ST_RUN;
    ip->a.eventIndex = -2;
    ip->a.trigger = s->trigger;
}

static int skip_to_end(const Script *s, int from, int nest) {
    for (int i = from; i < s->count; i++) {
        if (s->cmds[i].nest < nest) return i;
        if (s->cmds[i].nest == nest && s->cmds[i].type == CMD_END) return i + 1;
    }
    return s->count;
}

static int branch_false(const Script *s, int header) {
    int nest = s->cmds[header].nest;
    for (int i = header + 1; i < s->count; i++) {
        const Cmd *c = &s->cmds[i];
        if (c->nest < nest) return i;
        if (c->nest == nest) {
            if (c->type == CMD_ELSE) return i + 1;
            if (c->type == CMD_END) return i + 1;
            return i;
        }
    }
    return s->count;
}

static int choice_case(const Script *s, int header, int k) {
    if (k == 0) return header + 1;
    int nest = s->cmds[header].nest;
    for (int i = header + 1; i < s->count; i++) {
        const Cmd *c = &s->cmds[i];
        if (c->nest < nest) return i;
        if (c->nest == nest) {
            if (c->type == CMD_CASE && cmd_int(c, 0) == k) return i + 1;
            if (c->type == CMD_END) return i + 1;
        }
    }
    return s->count;
}

static bool switch_on(SG *g, i32 index) {
    if (index >= 65536) return sg_local_switch(g, g->interp.a.eventIndex >= 0 ? g->field.npcs[g->interp.a.eventIndex].eventId : (Guid){{0}}, index - 65536);
    return index >= 0 && index < 1024 && g->switches[index];
}

static bool compare(i32 a, i32 b, int op) {
    switch (op) {
    case 0: return a == b;
    case 1: return a >= b;
    case 2: return a <= b;
    case 3: return a > b;
    case 4: return a < b;
    case 5: return a != b;
    default: return a == b;
    }
}

static bool eval_branch(SG *g, const Cmd *c) {
    switch (c->type) {
    case CMD_IF_SWITCH: return switch_on(g, cmd_int(c, 0)) == (cmd_int(c, 1) == 0);
    case CMD_IF_VARIABLE: {
        i32 i = cmd_int(c, 0);
        return compare(i >= 0 && i < 1024 ? g->vars[i] : 0, cmd_int(c, 1), cmd_int(c, 2));
    }
    case CMD_IF_ITEM: {
        i32 have = sga_item_count(g, cmd_guid(c, 0));
        return cmd_int(c, 3) == 1 ? have >= cmd_int(c, 1) : have == cmd_int(c, 1);
    }
    case CMD_IF_PARTY: return sga_in_party(g, cmd_guid(c, 0)) == (cmd_int(c, 1) == 0);
    case CMD_BATTLE: sg_log(g, "battle (not run): taken as won"); return true;
    case CMD_INN: sg_log(g, "inn %d (not run): taken as declined", cmd_int(c, 0)); return false;
    case CMD_SHOP: sg_log(g, "shop (not run)"); return false;
    default: return false;
    }
}

static bool is_branch(int type) {
    return type == CMD_IF_SWITCH || type == CMD_IF_VARIABLE || type == CMD_IF_ITEM || type == CMD_IF_PARTY || type == CMD_BATTLE ||
           type == CMD_INN || type == CMD_SHOP;
}

static void start_message(SG *g, const Cmd *c) {
    Interp *ip = &g->interp;
    SgaState *a = &ip->a;
    split_pages(g, ip, cmd_str(c, 0));
    a->msgCmd = c->type;
    a->msgPos = cmd_int(c, 1);
    a->faceL = a->faceR = (Guid){{0}};
    a->exprL = a->exprR = 0;
    a->speaker = 0;
    if (c->type == CMD_FACE_MESSAGE) {
        a->faceL = cmd_guid(c, 3);
        a->exprL = cmd_int(c, 4);
        a->faceR = cmd_guid(c, 5);
        a->exprR = cmd_int(c, 6);
        a->speaker = cmd_int(c, 7);
    }
    a->windowOpen = true;
    a->textMs = 0;
    ip->shownMs = 0;
    ip->state = ST_MESSAGE;
    sg_log(g, "message: %s", ip->pages[0]);
}

static void start_choices(SG *g, const Cmd *c, int header) {
    Interp *ip = &g->interp;
    SgaState *a = &ip->a;
    clear_choices(a);
    int n = cmd_int(c, 0);
    for (int i = 0; i < n; i++) {
        a->choices = sg_realloc(a->choices, sizeof(char *) * (size_t)(a->choiceCount + 1));
        a->choices[a->choiceCount++] = sg_strdup(cmd_str(c, 1 + i));
    }
    a->choiceCancel = c->argCount > n + 1 ? cmd_int(c, n + 1) : -1;
    a->choiceCursor = 0;
    a->choiceHeader = header;
    ip->state = ST_CHOICE;
    sg_log(g, "choices: %d", n);
}

static void step(SG *g) {
    Interp *ip = &g->interp;
    SgaState *a = &ip->a;
    int guard = 0;
    while (ip->active && ip->state == ST_RUN) {
        if (++guard > 10000) {
            sg_log(g, "event loop guard hit");
            ip->active = false;
            return;
        }
        if (ip->pc >= ip->script.count) {
            ip->active = false;
            a->carryWindow = a->windowOpen ? a->msgCmd : 0;
            a->windowOpen = false;
            sga_event_ended(g);
            sg_log(g, "event end");
            return;
        }
        int here = ip->pc;
        const Cmd *c = &ip->script.cmds[ip->pc++];
        if (c->type != CMD_CHOICES) a->carryWindow = 0;
        if (c->type != CMD_MESSAGE && c->type != CMD_FACE_MESSAGE && c->type != CMD_CHOICES && c->type != CMD_CASE &&
            c->type != CMD_ELSE && c->type != CMD_END)
            a->windowOpen = false;
        if (is_branch(c->type)) {
            if (!eval_branch(g, c)) ip->pc = branch_false(&ip->script, here);
            continue;
        }
        switch (c->type) {
        case CMD_ELSE:
        case CMD_CASE: ip->pc = skip_to_end(&ip->script, ip->pc, c->nest); break;
        case CMD_END: break;
        case CMD_MESSAGE:
        case CMD_FACE_MESSAGE: start_message(g, c); break;
        case CMD_CHOICES:
            start_choices(g, c, here);
            break;
        case CMD_SCROLL_TEXT: {
            bool image = c->argCount > 2 && c->args[2].kind == ARG_GUID;
            ip->scrollText = cmd_str(c, 0);
            ip->scroll = 0;
            ip->shownMs = 0;
            a->scrollSpeed = kScrollPxPerSec;
            a->scrollImage = image ? sg_image(g, cmd_guid(c, 2)) : NULL;
            a->msgPos = cmd_int(c, 1);
            a->scrollStatic = cmd_int(c, image ? 3 : 2) == 0;
            ip->state = ST_SCROLL;
            sg_log(g, "screen text: %s, %d bytes", a->scrollStatic ? "static" : "scrolling", (int)strlen(ip->scrollText));
            break;
        }
        case CMD_WAIT:
            ip->waitMs = (u32)cmd_int(c, 0);
            ip->state = ST_WAIT;
            break;
        default:
            if (sga_exec(g, c) && ip->state == ST_RUN) ip->state = ST_BUSY;
            break;
        }
    }
}

static void finish_choice(SG *g, int k) {
    Interp *ip = &g->interp;
    SgaState *a = &ip->a;
    sg_log(g, "choice %d", k);
    ip->pc = choice_case(&ip->script, a->choiceHeader, k);
    clear_choices(a);
    a->windowOpen = false;
    ip->state = ST_RUN;
}

void interp_update(SG *g, u32 ms) {
    Interp *ip = &g->interp;
    SgaState *a = &ip->a;
    sga_frame(g, ms);
    if (!ip->active) {
        if (sga_field_input(g)) step(g);
        return;
    }
    switch (ip->state) {
    case ST_MESSAGE: {
        ip->shownMs += ms;
        a->textMs += ms;
        int total = ip->page < ip->pageCount ? page_chars(ip->pages[ip->page]) : 0;
        bool revealed = (float)a->textMs * kRevealPerMs >= (float)total;
        if (sg_pressed(g, KEY_OK)) {
            if (!revealed) {
                a->textMs = (u32)((float)total / kRevealPerMs) + 1;
            } else if (++ip->page >= ip->pageCount) {
                free(a->lastPage);
                a->lastPage = sg_strdup(ip->pages[ip->pageCount - 1]);
                clear_pages(ip);
                ip->state = ST_RUN;
            } else {
                a->textMs = 0;
                sg_log(g, "message: %s", ip->pages[ip->page]);
            }
        }
        break;
    }
    case ST_CHOICE:
        if (sg_pressed(g, KEY_DOWN)) a->choiceCursor = (a->choiceCursor + 1) % a->choiceCount;
        if (sg_pressed(g, KEY_UP)) a->choiceCursor = (a->choiceCursor + a->choiceCount - 1) % a->choiceCount;
        if (sg_pressed(g, KEY_OK)) finish_choice(g, a->choiceCursor);
        else if (sg_pressed(g, KEY_CANCEL) && a->choiceCancel >= 0 && a->choiceCancel < a->choiceCount) finish_choice(g, a->choiceCancel);
        break;
    case ST_SCROLL: {
        ip->shownMs += ms;
        if (a->scrollStatic) {
            if (sg_pressed(g, KEY_OK)) {
                ip->state = ST_RUN;
                a->scrollImage = NULL;
            }
            break;
        }
        float speed = (g->keys & KEY_OK) ? a->scrollSpeed * 4.0f : a->scrollSpeed;
        ip->scroll += speed * (float)ms / 1000.0f;
        int lines = 1;
        for (const char *p = ip->scrollText; *p; p++)
            if (*p == '\n') lines++;
        int height = a->scrollImage ? a->scrollImage->h : lines * kLinePx;
        if (ip->scroll > (float)(g->h + height)) {
            ip->state = ST_RUN;
            a->scrollImage = NULL;
        }
        break;
    }
    case ST_WAIT:
        if (ip->waitMs <= ms) {
            ip->waitMs = 0;
            ip->state = ST_RUN;
        } else {
            ip->waitMs -= ms;
        }
        break;
    case ST_BUSY:
        if (!sga_busy(g)) ip->state = ST_RUN;
        break;
    default: break;
    }
    step(g);
}

void sga_window(SG *g, Image *im, const Res *r, int x, int y, int w, int h) {
    if (!im || !r) {
        fill_rect(g->fb, g->w, g->h, x, y, w, h, 0xa0000000u);
        return;
    }
    int l = r->border[0], t = r->border[1], rr = r->border[2], b = r->border[3];
    int iw = im->w, ih = im->h;
    blit(g->fb, g->w, g->h, im, 0, 0, l, t, x, y, l, t, 255);
    blit(g->fb, g->w, g->h, im, l, 0, iw - l - rr, t, x + l, y, w - l - rr, t, 255);
    blit(g->fb, g->w, g->h, im, iw - rr, 0, rr, t, x + w - rr, y, rr, t, 255);
    blit(g->fb, g->w, g->h, im, 0, t, l, ih - t - b, x, y + t, l, h - t - b, 255);
    blit(g->fb, g->w, g->h, im, l, t, iw - l - rr, ih - t - b, x + l, y + t, w - l - rr, h - t - b, 255);
    blit(g->fb, g->w, g->h, im, iw - rr, t, rr, ih - t - b, x + w - rr, y + t, rr, h - t - b, 255);
    blit(g->fb, g->w, g->h, im, 0, ih - b, l, b, x, y + h - b, l, b, 255);
    blit(g->fb, g->w, g->h, im, l, ih - b, iw - l - rr, b, x + l, y + h - b, w - l - rr, b, 255);
    blit(g->fb, g->w, g->h, im, iw - rr, ih - b, rr, b, x + w - rr, y + h - b, rr, b, 255);
}

static void draw_lines(SG *g, const char *text, int x, int y, bool center, u32 color, bool shadow, int maxChars) {
    const char *p = text;
    int line = 0, shown = 0;
    while (*p && (maxChars < 0 || shown < maxChars)) {
        const char *e = p;
        char buf[1024];
        size_t n = 0;
        while (*e && *e != '\n' && *e != '\r') {
            u32 cp;
            int len = utf8_decode(e, &cp);
            if (maxChars >= 0 && shown >= maxChars) break;
            if (n + (size_t)len < sizeof buf) {
                memcpy(buf + n, e, (size_t)len);
                n += (size_t)len;
            }
            shown++;
            e += len;
        }
        buf[n] = 0;
        int lx = center ? (g->w - font_width(g->font, kTextPx, buf)) / 2 : x;
        int ly = y + line * kLinePx;
        if (ly > -kLinePx && ly < g->h) {
            if (shadow) font_draw(g->font, g->fb, g->w, g->h, lx + 2, ly + 2, kTextPx, buf, 0xc0000000u);
            font_draw(g->font, g->fb, g->w, g->h, lx, ly, kTextPx, buf, color);
        }
        line++;
        while (*e && *e != '\n' && *e != '\r') e++;
        p = e;
        if (*p == '\r') p++;
        if (*p == '\n') p++;
    }
}

static void darken(Image *src, u32 *fb, int fw, int fh, int dx, int dy, int dw, int dh, bool dim) {
    if (!src) return;
    for (int y = 0; y < dh; y++) {
        int ty = dy + y;
        if (ty < 0 || ty >= fh) continue;
        int sy = y * src->h / dh;
        for (int x = 0; x < dw; x++) {
            int tx = dx + x;
            if (tx < 0 || tx >= fw) continue;
            u32 s = src->px[(size_t)sy * (size_t)src->w + (size_t)(x * src->w / dw)];
            u32 al = s >> 24;
            if (!al) continue;
            if (dim) s = (s & 0xff000000u) | (((s >> 16 & 255) / 2) << 16) | (((s >> 8 & 255) / 2) << 8) | ((s & 255) / 2);
            u32 *d = &fb[(size_t)ty * (size_t)fw + (size_t)tx];
            u32 ia = 255 - al;
            u32 r = ((s & 255) * al + (*d & 255) * ia) / 255;
            u32 gg = ((s >> 8 & 255) * al + (*d >> 8 & 255) * ia) / 255;
            u32 b = ((s >> 16 & 255) * al + (*d >> 16 & 255) * ia) / 255;
            *d = 0xff000000u | b << 16 | gg << 8 | r;
        }
    }
}

static int window_y(SG *g, int pos) {
    if (pos == 0) return g->h - 351 - kWinH;
    if (pos == 1) return (g->h - kWinH) / 2;
    return 351 * g->h / 544;
}

static void draw_message(SG *g, const char *text, int shown) {
    Interp *ip = &g->interp;
    SgaState *a = &ip->a;
    if (a->msgCmd == CMD_FACE_MESSAGE) {
        Image *l = sga_face(g, a->faceL, a->exprL), *r = sga_face(g, a->faceR, a->exprR);
        if (l) darken(l, g->fb, g->w, g->h, 0, g->h - l->h, l->w, l->h, r && a->speaker == 1);
        if (r) darken(r, g->fb, g->w, g->h, g->w - r->w, g->h - r->h, r->w, r->h, l && a->speaker == 0);
    }
    int wy = window_y(g, a->msgPos);
    const Res *res = NULL;
    Image *im;
    u32 color;
    if (a->msgCmd == CMD_FACE_MESSAGE) {
        res = res_find(&g->res, g->data.settings.windowImage);
        im = sg_image(g, g->data.settings.windowImage);
        color = 0xff000000u;
    } else {
        im = sga_default_window(g, &res);
        color = 0xffffffffu;
    }
    sga_window(g, im, res, kWinX * g->w / 960, wy, kWinW * g->w / 960, kWinH);
    if (text) {
        int total = page_chars(text);
        draw_lines(g, text, kWinX * g->w / 960 + kTextPadX, wy + kTextPadY, false, color, false, shown < total ? shown : -1);
        if (ip->state == ST_MESSAGE && shown >= total && (ip->shownMs / 400) % 3 != 2) {
            int cx = g->w / 2, cy = wy + kWinH - 14;
            for (int k = 0; k < 7; k++) fill_rect(g->fb, g->w, g->h, cx - 6 + k, cy - 5 + k, 13 - k * 2, 1, color);
        }
    }
}

static void draw_choices(SG *g) {
    SgaState *a = &g->interp.a;
    int maxW = 0;
    for (int i = 0; i < a->choiceCount; i++) {
        int w = font_width(g->font, kTextPx, a->choices[i]);
        if (w > maxW) maxW = w;
    }
    int itemH = 50, w = maxW + 36, h = a->choiceCount * itemH + 12;
    int x = (g->w - w) / 2, y = (g->h - h) / 2;
    const Res *res = NULL;
    Image *im = sga_default_window(g, &res);
    sga_window(g, im, res, x, y, w, h);
    const Res *sr = NULL;
    Image *sel = sga_selected_window(g, &sr);
    for (int i = 0; i < a->choiceCount; i++) {
        int iy = y + 6 + i * itemH;
        if (i == a->choiceCursor) sga_window(g, sel, sr, x + 6, iy + 4, w - 12, itemH - 8);
        font_draw(g->font, g->fb, g->w, g->h, x + 18, iy + 12, kTextPx, a->choices[i], 0xffffffffu);
    }
}

void interp_draw(SG *g) {
    Interp *ip = &g->interp;
    sga_draw_over(g);
    if (!ip->active) return;
    if (ip->state == ST_SCROLL && ip->scrollText && ip->a.scrollStatic) {
        Image *im = ip->a.scrollImage;
        if (im) {
            blit(g->fb, g->w, g->h, im, 0, 0, im->w, im->h, (g->w - im->w) / 2, (g->h - im->h) / 2, im->w, im->h, 255);
        } else {
            int lines = 1;
            for (const char *p = ip->scrollText; *p; p++)
                if (*p == '\n') lines++;
            int y = ip->a.msgPos == 0 ? 40 : ip->a.msgPos == 1 ? g->h - 40 - lines * kLinePx : (g->h - lines * kLinePx) / 2;
            draw_lines(g, ip->scrollText, 0, y, true, 0xffffffffu, true, -1);
        }
    } else if (ip->state == ST_SCROLL && ip->scrollText) {
        if (ip->a.scrollImage) {
            Image *im = ip->a.scrollImage;
            blit(g->fb, g->w, g->h, im, 0, 0, im->w, im->h, (g->w - im->w) / 2, g->h - (int)ip->scroll, im->w, im->h, 255);
        } else {
            draw_lines(g, ip->scrollText, 0, g->h - (int)ip->scroll, true, 0xffffffffu, true, -1);
        }
    } else if (ip->state == ST_MESSAGE && ip->page < ip->pageCount) {
        draw_message(g, ip->pages[ip->page], (int)((float)ip->a.textMs * kRevealPerMs));
    } else if (ip->state == ST_CHOICE) {
        if (ip->a.windowOpen && ip->a.lastPage) {
            draw_message(g, ip->a.lastPage, 1 << 30);
        } else if (ip->a.carryWindow) {
            int keep = ip->a.msgCmd;
            ip->a.msgCmd = ip->a.carryWindow;
            ip->a.faceL = ip->a.faceR = (Guid){{0}};
            draw_message(g, NULL, 0);
            ip->a.msgCmd = keep;
        }
        draw_choices(g);
    }
}

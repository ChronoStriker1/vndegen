#include "sg.h"

enum { CMD_TRANSFER = 20, CMD_BGM = 21, CMD_SE = 22, CMD_MESSAGE = 29, CMD_VARIABLE = 31, CMD_FACE_MESSAGE = 43, CMD_SCROLL_TEXT = 44,
       CMD_SCREEN = 51, CMD_FADE = 60 };

enum { ST_RUN, ST_MESSAGE, ST_SCROLL, ST_WAIT };

static const int kTextPx = 22;
static const int kLinePx = 36;

static void clear_pages(Interp *ip) {
    for (int i = 0; i < ip->pageCount; i++) free(ip->pages[i]);
    free(ip->pages);
    ip->pages = NULL;
    ip->pageCount = ip->page = 0;
}

static void split_pages(Interp *ip, const char *text) {
    clear_pages(ip);
    const char *p = text;
    while (*p) {
        const char *brk = strstr(p, "\r\n\r\n");
        size_t n = brk ? (size_t)(brk - p) : strlen(p);
        char *page = sg_alloc(n + 1);
        memcpy(page, p, n);
        ip->pages = sg_realloc(ip->pages, sizeof(char *) * (size_t)(ip->pageCount + 1));
        ip->pages[ip->pageCount++] = page;
        p += n;
        while (*p == '\r' || *p == '\n') p++;
    }
}

void interp_free(Interp *ip) {
    script_free(&ip->script);
    clear_pages(ip);
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
    interp_free(&g->interp);
    g->interp.script = script_copy(s);
    g->interp.active = true;
    g->interp.state = ST_RUN;
}

static void step(SG *g) {
    Interp *ip = &g->interp;
    while (ip->active && ip->state == ST_RUN) {
        if (ip->pc >= ip->script.count) {
            ip->active = false;
            sg_log(g, "event end");
            return;
        }
        const Cmd *c = &ip->script.cmds[ip->pc++];
        switch (c->type) {
        case CMD_MESSAGE:
        case CMD_FACE_MESSAGE:
            split_pages(ip, cmd_str(c, 0));
            if (ip->pageCount) {
                ip->state = ST_MESSAGE;
                ip->shownMs = 0;
                sg_log(g, "message: %s", ip->pages[0]);
            }
            break;
        case CMD_SCROLL_TEXT:
            ip->scrollText = cmd_str(c, 0);
            ip->scroll = 0;
            ip->state = ST_SCROLL;
            sg_log(g, "scroll text: %.60s", ip->scrollText);
            break;
        case CMD_SCREEN:
            g->fadeLevel = g->fadeTo = cmd_int(c, 0) ? 1.0f : 0.0f;
            g->fadeTotal = 0;
            break;
        case CMD_FADE:
            g->fadeFrom = g->fadeLevel;
            g->fadeTo = cmd_int(c, 1) ? 1.0f : 0.0f;
            g->fadeTotal = (u32)cmd_int(c, 0);
            g->fadeMs = 0;
            if (!g->fadeTotal) g->fadeLevel = g->fadeTo;
            if (cmd_int(c, 2)) {
                ip->waitMs = g->fadeTotal;
                ip->state = ST_WAIT;
            }
            break;
        case CMD_VARIABLE: {
            i32 index = cmd_int(c, 0), value = cmd_int(c, 3);
            if (index >= 0 && index < 1024 && cmd_int(c, 1) == 0) g->vars[index] = value;
            sg_log(g, "variable %d = %d", index, value);
            break;
        }
        case CMD_TRANSFER:
            g->pendingMap = cmd_guid(c, 0);
            g->pendingX = cmd_int(c, 1);
            g->pendingZ = cmd_int(c, 2);
            g->pendingDir = cmd_int(c, 3);
            g->pendingTransfer = true;
            sg_log(g, "transfer %d,%d dir %d", g->pendingX, g->pendingZ, g->pendingDir);
            break;
        case CMD_BGM:
        case CMD_SE:
            break;
        default:
            sg_log(g, "command %d not handled", c->type);
            break;
        }
    }
}

void interp_update(SG *g, u32 ms) {
    Interp *ip = &g->interp;
    if (!ip->active) return;
    switch (ip->state) {
    case ST_MESSAGE:
        ip->shownMs += ms;
        if (sg_pressed(g, KEY_OK)) {
            if (++ip->page >= ip->pageCount) {
                clear_pages(ip);
                ip->state = ST_RUN;
            } else {
                sg_log(g, "message: %s", ip->pages[ip->page]);
            }
        }
        break;
    case ST_SCROLL: {
        float speed = (g->keys & KEY_OK) ? 160.0f : 40.0f;
        ip->scroll += speed * (float)ms / 1000.0f;
        int lines = 1;
        for (const char *p = ip->scrollText; *p; p++)
            if (*p == '\n') lines++;
        if (ip->scroll > (float)(g->h + lines * kLinePx)) ip->state = ST_RUN;
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
    default:
        break;
    }
    step(g);
}

static void draw_window(SG *g, Guid id, int x, int y, int w, int h) {
    const Res *r = res_find(&g->res, id);
    Image *im = sg_image(g, id);
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

static void draw_lines(SG *g, const char *text, int x, int y, bool center, u32 color) {
    const char *p = text;
    int line = 0;
    while (*p) {
        const char *e = p;
        while (*e && *e != '\n' && *e != '\r') e++;
        char buf[1024];
        size_t n = (size_t)(e - p) < sizeof buf - 1 ? (size_t)(e - p) : sizeof buf - 1;
        memcpy(buf, p, n);
        buf[n] = 0;
        int lx = center ? (g->w - font_width(g->font, kTextPx, buf)) / 2 : x;
        int ly = y + line * kLinePx;
        if (ly > -kLinePx && ly < g->h) {
            font_draw(g->font, g->fb, g->w, g->h, lx + 2, ly + 2, kTextPx, buf, 0xc0000000u);
            font_draw(g->font, g->fb, g->w, g->h, lx, ly, kTextPx, buf, color);
        }
        line++;
        p = e;
        if (*p == '\r') p++;
        if (*p == '\n') p++;
    }
}

void interp_draw(SG *g) {
    Interp *ip = &g->interp;
    if (!ip->active) return;
    if (ip->state == ST_SCROLL && ip->scrollText) {
        draw_lines(g, ip->scrollText, 0, g->h - (int)ip->scroll, true, 0xffffffffu);
    } else if (ip->state == ST_MESSAGE && ip->page < ip->pageCount) {
        int wx = g->w / 8, wy = g->h * 352 / 544, ww = g->w * 3 / 4, wh = 160;
        draw_window(g, g->data.settings.windowImage, wx, wy, ww, wh);
        draw_lines(g, ip->pages[ip->page], wx + 18, wy + 22, false, 0xffffffffu);
    }
}

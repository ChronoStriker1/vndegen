#include "br.h"

static int kv(Node *entry, const char *key, int dflt) {
    Node *v = sx_cdr_value(sx_find(entry, key));
    return v && v->t == N_INT ? v->i : dflt;
}

static MsgCfg *msg_cfg(BR *e, int id) {
    if (id < 0 || id >= BR_MSGWIN) id = 0;
    MsgCfg *c = &e->msgCfg[id];
    if (c->loaded) return c;
    c->loaded = true;
    Node *map = sx_find(sx_find(e->cfg.root, ":message-window"), "entryMap");
    Node *entry = sx_assoc(map, id);
    if (!entry && id) {
        c->loaded = false;
        return msg_cfg(e, 0);
    }
    c->x = kv(entry, "window-position-x", 40);
    c->y = kv(entry, "window-position-y", 400);
    c->w = kv(entry, "window-width", 720);
    c->h = kv(entry, "window-height", 200);
    c->bg = sx_cdr_value(sx_find(entry, "window-bg-imageUrl"));
    c->tx = kv(entry, "ta-offset-x", 10);
    c->ty = kv(entry, "ta-offset-y", 32);
    c->tw = kv(entry, "ta-width", 700);
    c->th = kv(entry, "ta-height", 176);
    c->size = kv(entry, "ta-font-size", 22);
    c->cpl = kv(entry, "ta-chars-per-line", 0);
    c->pitch = kv(entry, "ta-line-pitch", 0);
    c->ruby = kv(entry, "ta-ruby-font-size", 12);
    c->edge = (u32)kv(entry, "ta-font-edge-color.r", 0) | (u32)kv(entry, "ta-font-edge-color.g", 0) << 8 | (u32)kv(entry, "ta-font-edge-color.b", 0) << 16;
    c->edgeOn = kv(entry, "ta-font-edge-color.a", 255) > 0;
    c->kwx = kv(entry, "ta-keywait-icon-posx", c->tw - c->size);
    c->kwy = kv(entry, "ta-keywait-icon-posy", c->th - c->size);
    c->kwIcon = kv(entry, "ta-keywait-icon", 200);
    c->kwIconAuto = kv(entry, "ta-keywait-icon-1", 201);
    c->speed[0] = kv(entry, "ta-display-speed-low", 100);
    c->speed[1] = kv(entry, "ta-display-speed-middle", 30);
    c->speed[2] = kv(entry, "ta-display-speed-high", 10);
    return c;
}

u32 color_code(BR *e, int idx) {
    Node *t = sx_find(e->cfg.root, ":color-table");
    Node *c = sx_cdr_value(sx_assoc(t, idx));
    if (!c || c->t != N_LIST || c->n < 3) return 0xffffffu;
    return (u32)sx_int(c->k[0], 255) | (u32)sx_int(c->k[1], 255) << 8 | (u32)sx_int(c->k[2], 255) << 16;
}

static void page_reset(BR *e) {
    Msg *m = &e->msg;
    m->count = m->shown = 0;
    m->cx = 0;
    m->line = 0;
    m->color = color_code(e, 20);
    if (m->layer) memset(m->layer->px, 0, sizeof(u32) * (size_t)m->layer->w * m->layer->h);
    m->drawn = 0;
    m->unread = false;
    free(m->raw);
    m->raw = NULL;
}

void msg_select(BR *e, int id) {
    Msg *m = &e->msg;
    m->win = id;
    MsgCfg *c = msg_cfg(e, id);
    int lw = c->tw + 8, lh = c->th + 8;
    if (!m->layer || m->layer->w != lw || m->layer->h != lh) {
        image_free(m->layer);
        m->layer = image_new(lw, lh);
    }
    page_reset(e);
}

void msg_show(BR *e, bool on) { e->msg.visible = on; }

static int line_height(MsgCfg *c) { return c->size + c->ruby + c->pitch; }

static void newline(BR *e) {
    Msg *m = &e->msg;
    m->cx = 0;
    m->line++;
}

static void put_glyph(BR *e, const u8 *p) {
    Msg *m = &e->msg;
    MsgCfg *c = msg_cfg(e, m->win);
    int w = font_char_width(p, c->size);
    int limit = c->cpl > 0 ? c->cpl * c->size / 2 : c->tw + 1;
    if (m->cx + w >= limit) newline(e);
    if (m->count >= BR_MSG_GLYPHS) return;
    Glyph *g = &m->g[m->count++];
    g->c[0] = p[0];
    g->c[1] = p[0] >= 0x80 ? p[1] : 0;
    g->x = m->cx;
    g->y = m->line * line_height(c) + c->ruby;
    g->color = m->color;
    g->size = 0;
    m->cx += w;
}

static void put_ruby(BR *e, const u8 *r, size_t n) {
    Msg *m = &e->msg;
    MsgCfg *c = msg_cfg(e, m->win);
    int rs = c->ruby > 0 ? c->ruby : 12;
    int width = 0;
    for (size_t i = 0; i < n;) {
        width += font_char_width(r + i, rs);
        i += (size_t)br_sjis_len(r + i);
    }
    int baseW = m->cx - m->rubyX;
    if (m->line != m->rubyLine) baseW = width;
    int x = m->rubyX + (baseW - width) / 2;
    int y = m->rubyLine * line_height(c);
    for (size_t i = 0; i < n && m->count < BR_MSG_GLYPHS;) {
        Glyph *g = &m->g[m->count++];
        g->c[0] = r[i];
        g->c[1] = r[i] >= 0x80 ? r[i + 1] : 0;
        g->x = x;
        g->y = y;
        g->color = 0xffffffu;
        g->size = rs;
        x += font_char_width(r + i, rs);
        i += (size_t)br_sjis_len(r + i);
    }
}

void msg_append(BR *e, const char *s) {
    Msg *m = &e->msg;
    char *raw = br_fmt("%s%s", m->raw ? m->raw : "", s);
    free(m->raw);
    m->raw = raw;
    if (!m->layer) msg_select(e, m->win);
    for (const u8 *p = (const u8 *)s; *p;) {
        if ((p[0] == '$' || p[0] == '#') && p[1]) {
            char k = (char)p[1];
            if (p[0] == '$' && k >= 'A' && k <= 'K') {
                m->color = color_code(e, 10 + (k - 'A'));
                p += 2;
                continue;
            }
            if (p[0] == '$' && k == 'N') {
                m->color = color_code(e, 20);
                p += 2;
                continue;
            }
            if (p[0] == '#' && k == 'R') {
                newline(e);
                p += 2;
                continue;
            }
            if (p[0] == '#' && (k == 'W' || k == 'E' || k == 'T' || k == 't')) {
                p += 2;
                continue;
            }
        }
        if (p[0] == '&' && p[1] == 'R' && p[2] == 'A') {
            m->rubyX = m->cx;
            m->rubyLine = m->line;
            p += 3;
            continue;
        }
        if (p[0] == '&' && p[1] == 'R' && p[2] == 'S') {
            const u8 *r = p + 3, *end = (const u8 *)strstr((const char *)r, "&RT");
            if (!end) end = r + strlen((const char *)r);
            put_ruby(e, r, (size_t)(end - r));
            p = *end ? end + 3 : end;
            continue;
        }
        if (p[0] < 0x20) {
            p++;
            continue;
        }
        put_glyph(e, p);
        p += br_sjis_len(p);
    }
    if (e->skipping || m->instant) m->shown = m->count;
    m->lastReveal = e->now;
}

void msg_send(BR *e, Node *call) {
    Node *a = call->n > 1 ? call->k[1] : NULL;
    if (!a) return;
    if (a->t == N_STR) {
        msg_append(e, a->s);
        if (!e->systemMode) history_add(e, a->s);
        if (e->sp && call->n > 2 && !e->systemMode) {
            int sc = e->stack[e->sp - 1].sc->num, line = sx_int(call->k[2], 0);
            if (!seen_text_has(e, sc, line)) e->msg.unread = true;
            seen_text(e, sc, line);
        }
        return;
    }
    if (sx_sym(a, "clear")) {
        if (!e->systemMode) history_page(e);
        page_reset(e);
        return;
    }
    if (sx_sym(a, "wait-w")) {
        e->wait = W_TEXT;
        e->msg.waitStart = e->now;
        return;
    }
    br_log(e, "unknown send-string %s", a->s ? a->s : "?");
}

static int speed_ms(BR *e) {
    MsgCfg *c = msg_cfg(e, e->msg.win);
    int s = e->flag[256];
    if (s <= 0 || s > 3) return 0;
    return c->speed[s - 1];
}

static void reveal(BR *e) {
    Msg *m = &e->msg;
    if (m->shown >= m->count) return;
    int ms = speed_ms(e);
    if (ms <= 0 || e->skipping) {
        m->shown = m->count;
        return;
    }
    while (m->shown < m->count && e->now - m->lastReveal >= (u32)ms) {
        m->shown++;
        m->lastReveal += (u32)ms;
    }
}

bool msg_typing(BR *e) { return e->msg.shown < e->msg.count; }

void msg_tick(BR *e) {
    Msg *m = &e->msg;
    reveal(e);
    bool advance = e->lPress || e->wheelDown;
    if (e->skipping) {
        m->shown = m->count;
        e->wait = W_NONE;
        return;
    }
    if (msg_typing(e)) {
        if (advance) m->shown = m->count;
        return;
    }
    if (e->autoMode && e->now - m->lastReveal >= (u32)e->autoMs) advance = true;
    if (advance) e->wait = W_NONE;
}

void msg_update(BR *e) {
    if (e->wait != W_TEXT) reveal(e);
}

static void draw_glyph_layer(BR *e, Image *L, Glyph *g, MsgCfg *c) {
    u8 code[3] = {g->c[0], g->c[1], 0};
    int edgeW = c->edgeOn ? 1 : 0;
    if (g->size) font_draw_layer(e, L, code, g->x + 4, g->y + 4, g->size, g->color, 0x5a5a5au, 1);
    else font_draw_layer(e, L, code, g->x + 4, g->y + 4, c->size, g->color, c->edge, edgeW);
}

void msg_draw(BR *e, u32 *dst) {
    Msg *m = &e->msg;
    if (!m->visible || e->msgHidden) return;
    MsgCfg *c = msg_cfg(e, m->win);
    Image *bg = image_url(e, c->bg);
    if (bg) {
        blit(dst, BR_W, BR_H, bg, 0, 0, -1, -1, c->x, c->y, 255);
        image_free(bg);
    }
    if (!m->layer) return;
    if (m->drawn > m->shown) {
        memset(m->layer->px, 0, sizeof(u32) * (size_t)m->layer->w * m->layer->h);
        m->drawn = 0;
    }
    while (m->drawn < m->shown) draw_glyph_layer(e, m->layer, &m->g[m->drawn++], c);
    blit(dst, BR_W, BR_H, m->layer, 0, 0, -1, -1, c->x + c->tx - 4, c->y + c->ty - 4, 255);
    if (e->wait == W_TEXT && !msg_typing(e)) {
        Parts *p = parts_get(e, e->autoMode ? c->kwIconAuto : c->kwIcon);
        Image *icon = parts_image(e, p, 0, e->now - m->waitStart);
        if (icon) {
            blit(dst, BR_W, BR_H, icon, 0, 0, -1, -1, c->x + c->tx + c->kwx, c->y + c->ty + c->kwy, 255);
            image_free(icon);
        }
    }
}

void msg_free(BR *e) {
    free(e->msg.raw);
    e->msg.raw = NULL;
    image_free(e->msg.layer);
    e->msg.layer = NULL;
}

#include "ex.h"

#define TEXT_PAD 10
#define NAME_PAD 12
#define WAIT_FRAME_MS 150

static void style_parse(TextStyle *t, char **f, int pitch, int size, int color, int shadow) {
    t->pitch = field_int(f[pitch]);
    t->size = field_int(f[size]);
    t->color = colorref((u32)field_int(f[color]));
    t->shadow = colorref((u32)field_int(f[shadow]));
}

static void shadow_parse(MsgFrame *m, char *spec) {
    char *p[6] = {0};
    int n = 0;
    for (char *s = spec; s && n < 6;) {
        p[n++] = s;
        char *c = strchr(s, ';');
        if (!c) break;
        *c = 0;
        s = c + 1;
    }
    if (n >= 3) {
        m->text.shadow = colorref((u32)strtoul(p[0], NULL, 0));
        m->text.shx = field_int(p[1]);
        m->text.shy = field_int(p[2]);
        m->text.shadowOn = (strtoul(p[0], NULL, 0) >> 24) == 0;
    }
    if (n >= 6) {
        m->name.shadow = colorref((u32)strtoul(p[3], NULL, 0));
        m->name.shx = field_int(p[4]);
        m->name.shy = field_int(p[5]);
        m->name.shadowOn = (strtoul(p[3], NULL, 0) >> 24) == 0;
    }
}

void msg_define(EX *e, char *args) {
    char *f[72];
    int n = split_csv(args, f, 72);
    if (n < 59) {
        ex_log(e, "short message area definition (%d fields)", n);
        return;
    }
    int id = field_int(f[0]);
    if (id < 0 || id >= EX_FRAMES) return;
    MsgFrame *m = &e->msg.frames[id];
    image_free(m->img);
    memset(m, 0, sizeof *m);
    m->used = true;
    m->img = image_load(e, f[4]);
    m->x = field_int(f[7]);
    m->y = field_int(f[8]);
    m->tx = field_int(f[11]);
    m->ty = field_int(f[12]);
    m->tw = field_int(f[13]);
    m->th = field_int(f[14]);
    m->nx = field_int(f[15]);
    m->ny = field_int(f[16]);
    m->nw = field_int(f[17]);
    m->nh = field_int(f[18]);
    style_parse(&m->text, f, 44, 45, 48, 49);
    style_parse(&m->name, f, 53, 54, 57, 58);
    shadow_parse(m, f[38]);
}

void msg_wait_mark(EX *e, char *args) {
    char *f[8];
    int n = split_csv(args, f, 8);
    if (n < 5) return;
    image_free(e->msg.waitImg);
    e->msg.waitImg = image_load(e, f[4]);
    e->msg.waitX = field_int(f[1]);
    e->msg.waitY = field_int(f[2]);
    e->msg.waitFrames = field_int(f[3]) > 0 ? field_int(f[3]) : 1;
}

void msg_ctrl_define(EX *e, char *args) {
    char *f[16];
    int n = split_csv(args, f, 16);
    if (n < 7) return;
    if (e->msg.ctrlCount >= EX_CTRLS) return;
    Ctrl *c = &e->msg.ctrls[e->msg.ctrlCount++];
    memset(c, 0, sizeof *c);
    c->id = field_int(f[0]);
    c->x = field_int(f[4]);
    c->y = field_int(f[5]);
    if (strchr(f[6], ';')) {
        char *p = f[6];
        for (int k = 0; p && k < 22; k++) {
            char *s = strchr(p, ';');
            if (s) *s = 0;
            if (k == 17) c->knob = image_load(e, p);
            p = s ? s + 1 : NULL;
        }
        c->img = image_load(e, f[3]);
        c->slider = true;
        c->value = 128;
        c->states = 1;
    } else {
        c->img = image_load(e, f[6]);
        c->states = c->img && c->img->h > 0 && c->img->w >= c->img->h * 3 ? 3 : 1;
        if (!c->img) {
            c->img = image_load(e, f[3]);
            c->states = 1;
        }
    }
}

void msg_free(EX *e) {
    Msg *m = &e->msg;
    for (int i = 0; i < EX_FRAMES; i++) image_free(m->frames[i].img);
    for (int i = 0; i < m->ctrlCount; i++) {
        image_free(m->ctrls[i].img);
        image_free(m->ctrls[i].knob);
    }
    image_free(m->waitImg);
    free(m->chars);
    free(m->name);
    free(m->emb);
    memset(m, 0, sizeof *m);
}

static MsgFrame *frame(EX *e) {
    MsgFrame *f = &e->msg.frames[e->msg.frame];
    return f->used ? f : &e->msg.frames[0];
}

void msg_set_frame(EX *e, int id) {
    if (id >= 0 && id < EX_FRAMES && e->msg.frames[id].used) e->msg.frame = id;
}

void msg_show(EX *e, bool on, int ms) {
    Msg *m = &e->msg;
    float to = on ? 255.0f : 0.0f;
    m->shown = on;
    if (ms > 0) anim_start(e, &m->fade, m->a, to, ms, 0);
    else {
        m->fade.active = false;
        m->a = to;
    }
}

void msg_temp_hide(EX *e, bool hide, int ms) {
    Msg *m = &e->msg;
    float to = hide ? 0.0f : 255.0f;
    m->tempHidden = hide;
    if (ms > 0) anim_start(e, &m->tempFade, m->tempA, to, ms, 0);
    else {
        m->tempFade.active = false;
        m->tempA = to;
    }
}

void msg_clear(EX *e) {
    Msg *m = &e->msg;
    m->count = m->visible = 0;
    m->nameLen = 0;
    m->active = false;
    m->embCount = m->embNext = 0;
}

static u32 *decode(const char *sjis, int *len, bool **italic) {
    size_t n = strlen(sjis);
    u32 *out = ex_alloc(sizeof(u32) * (n + 1));
    bool *it = italic ? ex_alloc(n + 1) : NULL;
    int k = 0;
    bool on = false;
    for (const u8 *p = (const u8 *)sjis; *p;) {
        if (p[0] == '{' && p[1] == 'I' && p[2] == ':' && p[4] == '}') {
            on = p[3] == '1';
            p += 5;
            continue;
        }
        if (p[0] == '\r') {
            p++;
            continue;
        }
        u32 cp;
        p += ex_sjis_decode(p, &cp);
        if (it) it[k] = on;
        out[k++] = cp;
    }
    *len = k;
    if (italic) *italic = it;
    return out;
}

static void layout(EX *e, const u32 *cps, const bool *it, int n) {
    Msg *m = &e->msg;
    MsgFrame *f = frame(e);
    m->chars = ex_realloc(m->chars, sizeof(PlacedChar) * (size_t)(n + 1));
    int size = f->text.size > 0 ? f->text.size : 32;
    int left = f->x + f->tx + TEXT_PAD, right = f->x + f->tx + f->tw + TEXT_PAD;
    int x = left, line = 0;
    int k = 0;
    for (int i = 0; i < n;) {
        if (cps[i] == '\n') {
            x = left;
            line++;
            i++;
            continue;
        }
        int j = i, w = 0;
        while (j < n && cps[j] != ' ' && cps[j] != '\n') w += text_advance(e, cps[j++], size);
        if (x > left && x + w > right) {
            x = left;
            line++;
        }
        for (; i < j; i++) {
            m->chars[k++] = (PlacedChar){cps[i], (i16)x, (i16)line, it && it[i]};
            x += text_advance(e, cps[i], size);
        }
        if (i < n && cps[i] == ' ') {
            if (x > left) {
                m->chars[k++] = (PlacedChar){' ', (i16)x, (i16)line, false};
                x += text_advance(e, ' ', size);
            }
            i++;
        }
    }
    m->count = k;
}

static void parse_embedded(EX *e, char **s, int ns) {
    Msg *m = &e->msg;
    m->embCount = m->embNext = 0;
    for (int si = 0; si < ns; si++) {
        char *copy = ex_strdup(s[si]);
        char *f[256];
        int n = split_csv(copy, f, 256);
        int i = 0;
        if (n > 0 && field_int(f[0]) == 258) {
            free(copy);
            continue;
        }
        if (si == 0 && n > 1 && field_int(f[0]) == 1 && field_int(f[1]) == 2) i = 1;
        while (i + 8 <= n && field_int(f[i]) == 2) {
            int op = field_int(f[i + 1]), nv = field_int(f[i + 2]), nstr = field_int(f[i + 3]);
            int at = field_int(f[i + 5]);
            i += 8;
            if (nv < 0 || i + nv > n || m->embCount >= EX_EMBEDDED) break;
            Embedded *b = &m->emb[m->embCount++];
            memset(b, 0, sizeof *b);
            b->op = op;
            b->at = at;
            b->nv = nv > 16 ? 16 : nv;
            for (int k = 0; k < nv; k++)
                if (k < 16) b->v[k] = field_int(f[i + k]);
            i += nv;
            i += nstr;
        }
        free(copy);
    }
}

void msg_start(EX *e, Op *op) {
    Msg *m = &e->msg;
    if (!m->emb) m->emb = ex_alloc(sizeof(Embedded) * EX_EMBEDDED);
    int who = op->nv ? op->v[0] : 1;
    const char *name = op->ns > 0 ? op->s[0] : "*";
    const char *text = op->ns > 1 ? op->s[1] : "";
    free(m->name);
    m->name = NULL;
    m->nameLen = 0;
    if (strcmp(name, "*")) m->name = decode(name, &m->nameLen, NULL);
    else {
        const char *def = chara_name(e, who);
        if (def && *def) m->name = decode(def, &m->nameLen, NULL);
    }
    bool *it = NULL;
    int n;
    u32 *cps = decode(text, &n, &it);
    layout(e, cps, it, n);
    free(cps);
    free(it);
    parse_embedded(e, op->s + 2, op->ns > 2 ? op->ns - 2 : 0);
    m->visible = 0;
    m->start = e->now;
    m->active = true;
    m->textId = op->nv > 1 ? op->v[1] : -1;
    m->speaker = who;
    snd_stop(e, EX_CH_VOICE, 0);
    if (m->textId >= 0 && chara_voiced(e, who)) snd_play_voice(e, m->textId);
}

bool msg_typing(EX *e) {
    Msg *m = &e->msg;
    return m->active && m->visible < m->count;
}

void msg_finish(EX *e) {
    Msg *m = &e->msg;
    m->visible = m->count;
}

void msg_tick(EX *e) {
    Msg *m = &e->msg;
    if (!m->active) return;
    u32 t = e->now - m->start;
    if (m->visible < m->count) {
        int speed = e->textMs > 0 ? e->textMs : 0;
        int v = speed ? (int)(t / (u32)speed) : m->count;
        m->visible = v > m->count ? m->count : v;
    }
    while (m->embNext < m->embCount && (u32)m->emb[m->embNext].at <= t) vm_exec_embedded(e, &m->emb[m->embNext++]);
}

void msg_flush_embedded(EX *e) {
    Msg *m = &e->msg;
    while (m->embNext < m->embCount) vm_exec_embedded(e, &m->emb[m->embNext++]);
}

static void draw_string(EX *e, u32 *dst, const TextStyle *st, const PlacedChar *c, int baseline, int alpha) {
    if (c->cp == ' ') return;
    if (st->shadowOn) text_glyph(e, dst, EX_W, EX_H, c->cp, c->x + st->shx, baseline + st->shy, st->size, st->shadow, alpha, c->italic);
    text_glyph(e, dst, EX_W, EX_H, c->cp, c->x, baseline, st->size, st->color, alpha, c->italic);
}

static void draw_ctrls(EX *e, u32 *dst, int alpha) {
    Msg *m = &e->msg;
    for (int i = 0; i < m->ctrlCount; i++) {
        Ctrl *c = &m->ctrls[i];
        if (!c->img) continue;
        if (c->slider) {
            blit(dst, EX_W, EX_H, c->img, c->x, c->y, alpha);
            if (c->knob) blit(dst, EX_W, EX_H, c->knob, c->x + (c->img->w - c->knob->w) * c->value / 255, c->y, alpha);
            continue;
        }
        int w = c->img->w / c->states;
        int state = c->states > 1 && m->ctrlHover == c->id ? 1 : 0;
        blit_part(dst, EX_W, EX_H, c->img, state * w, 0, w, c->img->h, c->x, c->y, alpha);
    }
}

void msg_draw(EX *e, u32 *dst) {
    Msg *m = &e->msg;
    if (m->fade.active) m->a = anim_value(e, &m->fade, m->a);
    if (m->tempFade.active) m->tempA = anim_value(e, &m->tempFade, m->tempA);
    int alpha = (int)(m->a * m->tempA / 255.0f);
    if (m->userHidden) alpha = 0;
    if (alpha <= 0) return;
    MsgFrame *f = frame(e);
    if (f->img) blit(dst, EX_W, EX_H, f->img, f->x, f->y, alpha);
    if (m->ctrlShown) draw_ctrls(e, dst, alpha);
    if (!m->active) return;
    if (m->nameLen) {
        TextStyle *st = &f->name;
        int size = st->size > 0 ? st->size : 32;
        int asc = text_ascent(e, size), desc = text_descent(e, size);
        int base = f->y + f->ny + (f->nh - asc - desc) / 2 + asc + 2;
        int x = f->x + f->nx + NAME_PAD;
        for (int i = 0; i < m->nameLen; i++) {
            PlacedChar c = {m->name[i], (i16)x, 0, false};
            draw_string(e, dst, st, &c, base, alpha);
            x += text_advance(e, m->name[i], size);
        }
    }
    TextStyle *st = &f->text;
    int size = st->size > 0 ? st->size : 32;
    int pitch = st->pitch > 0 ? st->pitch : size + 16;
    int top = f->y + f->ty + pitch - text_descent(e, size);
    for (int i = 0; i < m->visible && i < m->count; i++) draw_string(e, dst, st, &m->chars[i], top + m->chars[i].line * pitch, alpha);
    if (m->visible >= m->count && m->waitImg && e->wait == W_TEXT) {
        int fw = m->waitImg->w / m->waitFrames;
        int k = (int)((e->now / WAIT_FRAME_MS) % (u32)m->waitFrames);
        blit_part(dst, EX_W, EX_H, m->waitImg, k * fw, 0, fw, m->waitImg->h, m->waitX, m->waitY, alpha);
    }
}

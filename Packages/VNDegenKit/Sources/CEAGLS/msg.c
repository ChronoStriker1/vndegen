#include "eagls.h"

typedef struct Cell {
    char ch[3];
    int x, y, size, spacing;
    u32 color;
} Cell;

static Cell *cells;
static int cellCount, cellCap;

static void add_cell(const u8 *p, int len, int x, int y, int size, int spacing, u32 color) {
    if (cellCount == cellCap) {
        cellCap = cellCap ? cellCap * 2 : 256;
        cells = eg_realloc(cells, sizeof(Cell) * (size_t)cellCap);
    }
    Cell *c = &cells[cellCount++];
    memset(c, 0, sizeof *c);
    memcpy(c->ch, p, (size_t)len);
    c->x = x;
    c->y = y;
    c->size = size;
    c->spacing = spacing;
    c->color = color;
}

const Font *msg_font(EG *e, int i) {
    static const Font def = {21, false, 2, 6};
    if (i < 0 || i >= 16 || !e->fonts[i].size) return &def;
    return &e->fonts[i];
}

#define TEXT_COLOR 0xffc0c0c0u

int msg_tag(EG *e, const u8 *p, const Font **f, u32 *color) {
    if (*p != '(' || p[2] != '=') return 0;
    const u8 *q = p + 3;
    if (p[1] == 'F') {
        int k = 0;
        while (*q >= '0' && *q <= '9') k = k * 10 + (*q++ - '0');
        if (*q != ')') return 0;
        *f = msg_font(e, k);
        return (int)(q + 1 - p);
    }
    if (p[1] == 'C') {
        int r = 0, g = 0, b = 0;
        if (sscanf((const char *)q, "%d,%d,%d", &r, &g, &b) != 3) return 0;
        while (*q && *q != ')') q++;
        if (*q != ')') return 0;
        *color = (u32)clampi(r, 0, 255) | (u32)clampi(g, 0, 255) << 8 | (u32)clampi(b, 0, 255) << 16 | 0xff000000u;
        return (int)(q + 1 - p);
    }
    return 0;
}

static void layout(EG *e) {
    cellCount = 0;
    if (!e->text) return;
    int x0 = e->winRect[4], y0 = e->winRect[5], x1 = e->winRect[6];
    if (x1 <= x0) x1 = x0 + 600;
    const Font *f = msg_font(e, 0);
    int size = f->size, spacing = f->spacing, line = f->size + f->lineGap;
    u32 color = e->textWhite ? 0xffffffffu : TEXT_COLOR;
    int fx = x0 * 8, y = y0;
    const u8 *p = (const u8 *)e->text;
    while (*p) {
        if (*p == '(' && (p[1] == 'e' || p[1] == 'E') && p[2] == ')') {
            fx = x0 * 8;
            y += line;
            p += 3;
            continue;
        }
        int tag = msg_tag(e, p, &f, &color);
        if (tag) {
            size = f->size;
            spacing = f->spacing;
            p += tag;
            continue;
        }
        int len = eg_sjis_len(p);
        u32 cp;
        eg_sjis_decode(p, &cp);
        int adv = text_advance(cp, size, spacing);
        if (fx + adv > x1 * 8 && fx > x0 * 8) {
            fx = x0 * 8;
            y += line;
        }
        add_cell(p, len, fx / 8, y, size, spacing, color);
        fx += adv;
        p += len;
    }
}

void msg_window_parts(EG *e, int index, const char *name) {
    if (index < 0 || index >= 8) return;
    if (!strcmp(e->mesName[index], name) && e->mesImg[index]) return;
    image_free(e->mesImg[index]);
    e->mesImg[index] = eg_image_load(e, name);
    snprintf(e->mesName[index], sizeof e->mesName[index], "%s", name);
}

static void add_log(EG *e) {
    e->logs = eg_realloc(e->logs, sizeof(LogEntry) * (size_t)(e->logCount + 1));
    LogEntry *l = &e->logs[e->logCount++];
    l->name = e->speaker ? eg_strdup(e->speaker) : NULL;
    l->text = eg_strdup(e->text ? e->text : "");
    l->voice = e->voice ? eg_strdup(e->voice) : NULL;
    if (e->logCount > 400) {
        free(e->logs[0].name);
        free(e->logs[0].text);
        free(e->logs[0].voice);
        memmove(e->logs, e->logs + 1, sizeof(LogEntry) * (size_t)(e->logCount - 1));
        e->logCount--;
    }
}

void msg_show(EG *e, int id, const char *text) {
    free(e->text);
    e->text = eg_strdup(text);
    e->textId = id;
    e->textShown = 0;
    e->textStart = e->now;
    e->textDone = false;
    e->autoAt = 0;
    layout(e);
    add_log(e);
    if (e->voice) snd_play(e, e->voice, 0, 6, 0);
    else snd_play(e, "0", 3, 6, 0);
}

void msg_name(EG *e, const char *name, const char *voice) {
    free(e->speaker);
    free(e->voice);
    e->speaker = name && *name ? eg_strdup(name) : NULL;
    e->voice = voice && *voice ? eg_strdup(voice) : NULL;
}

void msg_clear(EG *e) {
    free(e->text);
    e->text = NULL;
    free(e->speaker);
    e->speaker = NULL;
    free(e->voice);
    e->voice = NULL;
    cellCount = 0;
}

bool msg_complete(EG *e) { return e->textShown >= cellCount; }

void msg_reveal(EG *e) { e->textShown = cellCount; }

void msg_update(EG *e) {
    if (!e->text) return;
    int speed = e->textSpeed;
    if (speed <= 0) e->textShown = cellCount;
    else {
        int n = (int)((e->now - e->textStart) / (u32)speed);
        e->textShown = n > cellCount ? cellCount : n;
    }
}

static bool window_visible(EG *e) { return e->mesMode && !e->winHidden && e->uiMode != UI_LOG; }

void msg_draw(EG *e, u32 *dst) {
    if (!window_visible(e)) return;
    int alpha = e->mesAlpha;
    Image *win = e->mesImg[0];
    if (win) blit(dst, e->W, e->H, win, e->winRect[0], e->winRect[1], alpha);
    if (e->speaker && e->th[0].wait != W_CHOICE) {
        const Font *f = msg_font(e, 1);
        int x0 = e->nameRect[0], y0 = e->nameRect[1], x1 = e->nameRect[2], y1 = e->nameRect[3];
        int tw = text_width(e->speaker, f->size, f->spacing);
        text_draw(e, dst, e->W, e->H, e->speaker, (x0 + x1 - tw) / 2, (y0 + y1 - f->size) / 2, f->size, f->spacing, 0xffffffffu, false);
    }
    int shown = e->textShown < cellCount ? e->textShown : cellCount;
    for (int i = 0; i < shown; i++) {
        Cell *c = &cells[i];
        text_draw(e, dst, e->W, e->H, c->ch, c->x, c->y, c->size, c->spacing, c->color, true);
    }
    if (e->text && shown >= cellCount && e->th[0].wait == W_TEXT && e->mesImg[3]) {
        int r[9] = {0};
        int n = ini_ints(&e->ini, "MesWaitCursor", r, 9);
        if (n >= 7 && r[6] > 0) {
            int per = n > 7 && r[7] > 0 ? r[7] : 100;
            int frame = (int)((e->now / (u32)per) % (u32)r[6]);
            blit_part(dst, e->W, e->H, e->mesImg[3], r[4] + frame * r[2], r[5], r[2], r[3], r[0], r[1], 255);
        }
    }
}

void choice_begin(EG *e) {
    e->choiceCount = 0;
    for (int i = 0; i < 8; i++)
        if (e->choice[i].used && e->choice[i].text) e->choiceCount = i + 1;
    e->choiceHover = -1;
    e->click = false;
}

static int choice_at(EG *e, int x, int y) {
    for (int i = 0; i < e->choiceCount; i++) {
        ChoiceBox *c = &e->choice[i];
        if (!c->used || !c->text) continue;
        if (x >= c->x && x < c->x + c->w && y >= c->y && y < c->y + c->h) return i;
    }
    return -1;
}

void choice_update(EG *e) {
    int h = choice_at(e, e->mx, e->my);
    if (h != e->choiceHover) {
        e->choiceHover = h;
        if (h >= 0) snd_play(e, "SYS_OV", 0, 2, 0);
    }
    if (e->click && h >= 0) {
        e->click = false;
        snd_play(e, "SYS_OK", 0, 2, 0);
        Var *v = var_get(&e->vars, e->choiceVar, true);
        Val *x = var_at(v, e->choiceVarIndex);
        x->i = h;
        free(x->s);
        x->s = NULL;
        e->choiceCount = 0;
        e->th[0].wait = W_NONE;
    }
}

void choice_draw(EG *e, u32 *dst) {
    if (e->th[0].wait != W_CHOICE) return;
    for (int i = 0; i < e->choiceCount; i++) {
        ChoiceBox *c = &e->choice[i];
        if (!c->used || !c->text) continue;
        bool hov = i == e->choiceHover;
        int pl = hov ? c->hplane : c->plane;
        Image *im = pl >= 0 && pl < EG_PLANES ? e->plane[pl] : NULL;
        int x = hov ? c->hx : c->x, y = hov ? c->hy : c->y;
        if (im) blit_part(dst, e->W, e->H, im, hov ? c->hsx : c->sx, hov ? c->hsy : c->sy, c->w, c->h, x, y, 255);
        const Font *f = msg_font(e, 0);
        text_draw(e, dst, e->W, e->H, c->text, x + (hov ? c->htx : c->tx), y + (hov ? c->hty : c->ty), f->size, f->spacing, e->textWhite ? 0xffffffffu : TEXT_COLOR, true);
    }
}

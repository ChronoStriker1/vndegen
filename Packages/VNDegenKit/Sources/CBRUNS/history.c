#include "br.h"

#define HIST_MAX 300
#define LINE_H 36
#define SEP_H 44
#define WRAP_W 860
#define FONT 28

typedef struct Entry {
    char **lines;
    int count;
    int voiceCh, voiceId;
} Entry;

typedef struct History {
    Entry e[HIST_MAX];
    int head, size;
    char *page;
    int voiceCh, voiceId;
} History;

static History *hist(BR *e) {
    if (!e->hist) {
        e->hist = br_alloc(sizeof(History));
        e->hist->voiceCh = -1;
    }
    return e->hist;
}

static void entry_free(Entry *en) {
    for (int i = 0; i < en->count; i++) free(en->lines[i]);
    free(en->lines);
    memset(en, 0, sizeof *en);
}

void history_free(BR *e) {
    if (!e->hist) return;
    for (int i = 0; i < HIST_MAX; i++) entry_free(&e->hist->e[i]);
    free(e->hist->page);
    free(e->hist);
    e->hist = NULL;
}

void history_add(BR *e, const char *s) {
    History *h = hist(e);
    char *p = br_fmt("%s%s", h->page ? h->page : "", s);
    free(h->page);
    h->page = p;
}

void history_voice_mark(BR *e, int ch, int num) {
    History *h = hist(e);
    h->voiceCh = ch;
    h->voiceId = num;
}

static void push_line(Entry *en, const char *s, size_t n, const char *color) {
    en->lines = br_realloc(en->lines, sizeof(char *) * (size_t)(en->count + 1));
    size_t cl = color ? strlen(color) : 0;
    char *l = br_alloc(cl + n + 1);
    if (cl) memcpy(l, color, cl);
    memcpy(l + cl, s, n);
    en->lines[en->count++] = l;
}

static void split(Entry *en, const char *text) {
    const u8 *p = (const u8 *)text, *start = p;
    int w = 0;
    char color[3] = "", lineColor[3] = "";
    while (*p) {
        if (p[0] == '#' && p[1] == 'R') {
            push_line(en, (const char *)start, (size_t)(p - start), lineColor);
            p += 2;
            start = p;
            w = 0;
            memcpy(lineColor, color, sizeof color);
            continue;
        }
        if ((p[0] == '$' || p[0] == '#' || p[0] == '&') && p[1] && p[1] < 0x80) {
            if (p[0] == '$') {
                color[0] = '$';
                color[1] = (char)p[1];
                color[2] = 0;
            }
            p += 2;
            continue;
        }
        int cw = font_char_width(p, FONT);
        if (w + cw > WRAP_W) {
            push_line(en, (const char *)start, (size_t)(p - start), lineColor);
            start = p;
            w = 0;
            memcpy(lineColor, color, sizeof color);
        }
        w += cw;
        p += br_sjis_len(p);
    }
    if (p > start) push_line(en, (const char *)start, (size_t)(p - start), lineColor);
}

void history_page(BR *e) {
    History *h = hist(e);
    if (h->page && *h->page) {
        Entry *en = &h->e[(h->head + h->size) % HIST_MAX];
        if (h->size == HIST_MAX) {
            en = &h->e[h->head];
            h->head = (h->head + 1) % HIST_MAX;
        } else h->size++;
        entry_free(en);
        split(en, h->page);
        en->voiceCh = h->voiceCh;
        en->voiceId = h->voiceId;
    }
    free(h->page);
    h->page = NULL;
    h->voiceCh = -1;
}

static Entry *at(History *h, int i) { return &h->e[(h->head + i) % HIST_MAX]; }

static int entry_h(Entry *en) { return en->count * LINE_H + SEP_H; }

int history_height(BR *e) {
    History *h = hist(e);
    int total = 0;
    for (int i = 0; i < h->size; i++) total += entry_h(at(h, i));
    return total;
}

int history_layout(BR *e, int scroll, int view, int textBase, int voiceBase, int count, int textX, int topY, int voiceX) {
    History *h = hist(e);
    int y = 0, line = 0, first = -1, vb = 0;
    for (int i = 0; i < count; i++) {
        Obj *o = obj_get(e, OBJ_BTN, textBase + i);
        if (o) {
            free(o->text);
            o->text = br_strdup("");
        }
        Obj *v = obj_get(e, OBJ_BTN, voiceBase + i);
        if (v) v->layer = 0;
    }
    for (int i = 0; i < h->size; i++) {
        Entry *en = at(h, i);
        int eh = entry_h(en);
        if (y + eh > scroll && y < scroll + view) {
            if (first < 0) first = i;
            if (en->voiceCh >= 0) {
                Obj *v = obj_get(e, OBJ_BTN, voiceBase + (i - first));
                int ly = y - scroll;
                if (v && ly >= 0 && ly < view) {
                    v->layer = 1;
                    v->x = voiceX;
                    v->y = topY + ly;
                }
            }
            for (int k = 0; k < en->count && line < count; k++) {
                int ly = y + k * LINE_H - scroll;
                if (ly < 0 || ly + LINE_H > view) continue;
                Obj *o = obj_get(e, OBJ_BTN, textBase + line++);
                if (!o) continue;
                free(o->text);
                o->text = br_strdup(en->lines[k]);
                o->x = textX;
                o->y = topY + ly;
                o->layer = 1;
            }
        }
        y += eh;
    }
    (void)vb;
    return first < 0 ? 0 : first;
}

int history_voice(BR *e, int idx, int *ch) {
    History *h = hist(e);
    if (idx < 0 || idx >= h->size) {
        *ch = -1;
        return 0;
    }
    *ch = at(h, idx)->voiceCh;
    return at(h, idx)->voiceId;
}

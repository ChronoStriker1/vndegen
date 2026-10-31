// The backlog: text the player has read, drawn by a CreateBacklog object and scrolled by the scrollbar
// SetScrollbar links to it. The layout comes from the $SYSTEM_backlog_* variables the backlog script sets.
#include "n2.h"

#include <stdlib.h>
#include <string.h>

typedef struct Entry {
    char *text;
    char *voice;
} Entry;

typedef struct Row {
    char *text;
    char *voice;  // on an entry's first row
} Row;

struct Backlog {
    Entry *entries;
    int count;
    Obj *view, *bar;
    Row *rows;
    int rowCount;
    int top;  // first row shown
    bool closed;
};

#define MAX_ENTRIES 400

static struct Backlog *bl(N2 *n) {
    if (!n->backlog) n->backlog = n2_alloc(sizeof(struct Backlog));
    return n->backlog;
}

static double num(N2 *n, const char *name, double def) {
    Value *v = var_get(n, name);
    return v ? v_tonum(v) : def;
}

static void free_rows(struct Backlog *b) {
    for (int i = 0; i < b->rowCount; i++) {
        free(b->rows[i].text);
        free(b->rows[i].voice);
    }
    free(b->rows);
    b->rows = NULL;
    b->rowCount = 0;
}

static void add_row(struct Backlog *b, const char *s, size_t len, const char *voice) {
    b->rows = n2_grow(b->rows, sizeof(Row) * (size_t)(b->rowCount + 1));
    b->rows[b->rowCount].text = n2_strndup(s, len);
    b->rows[b->rowCount].voice = voice ? n2_strdup(voice) : NULL;
    b->rowCount++;
}

// Splits entries at their line breaks and wraps long lines at the backlog's width.
static void layout(N2 *n, struct Backlog *b) {
    free_rows(b);
    int width = (int)(num(n, "$SYSTEM_backlog_character_width", 10) * 60);
    if (width < 200) width = 580;
    for (int i = 0; i < b->count; i++) {
        const char *s = b->entries[i].text;
        const char *voice = b->entries[i].voice;
        while (*s) {
            const char *e = strchr(s, '\n');
            size_t len = e ? (size_t)(e - s) : strlen(s);
            char *line = n2_strndup(s, len);
            // Wrap by measured width, at spaces where possible.
            char *p = line;
            while (*p) {
                size_t fit = strlen(p), lastSpace = 0;
                for (size_t k = 1; k <= strlen(p); k++) {
                    char saved = p[k];
                    p[k] = 0;
                    int w = text_width(n, p);
                    p[k] = saved;
                    if (p[k - 1] == ' ') lastSpace = k;
                    if (w > width) {
                        fit = lastSpace ? lastSpace : k - 1;
                        break;
                    }
                }
                if (!fit) fit = 1;
                add_row(b, p, fit, voice);
                voice = NULL;
                p += fit;
                while (*p == ' ') p++;
            }
            free(line);
            s += len + (e ? 1 : 0);
        }
    }
}

void backlog_add(N2 *n, const char *text, const char *voice) {
    struct Backlog *b = bl(n);
    if (b->count == MAX_ENTRIES) {
        free(b->entries[0].text);
        free(b->entries[0].voice);
        memmove(b->entries, b->entries + 1, sizeof(Entry) * (MAX_ENTRIES - 1));
        b->count--;
    }
    b->entries = n2_grow(b->entries, sizeof(Entry) * (size_t)(b->count + 1));
    // Leading indentation and trailing blank lines don't belong in the log.
    while (*text == ' ') text++;
    size_t len = strlen(text);
    while (len && (text[len - 1] == '\n' || text[len - 1] == ' ')) len--;
    b->entries[b->count].text = n2_strndup(text, len);
    b->entries[b->count].voice = voice ? n2_strdup(voice) : NULL;
    b->count++;
}

void backlog_clear(N2 *n) {
    struct Backlog *b = bl(n);
    for (int i = 0; i < b->count; i++) {
        free(b->entries[i].text);
        free(b->entries[i].voice);
    }
    free(b->entries);
    b->entries = NULL;
    b->count = 0;
    free_rows(b);
}

int backlog_count(N2 *n) {
    struct Backlog *b = bl(n);
    if (b->view && b->closed) return 0;
    return b->count;
}

static int visible_rows(N2 *n) {
    int r = (int)num(n, "$SYSTEM_backlog_row_max", 17);
    return r > 0 ? r : 17;
}

static void sync_bar(N2 *n, struct Backlog *b) {
    int maxTop = b->rowCount - visible_rows(n);
    if (maxTop < 0) maxTop = 0;
    if (b->top > maxTop) b->top = maxTop;
    if (b->top < 0) b->top = 0;
    // The bar runs from the newest text (value 0, at its start point) to the oldest (1).
    if (b->bar) b->bar->sbValue = maxTop ? 1.0 - (double)b->top / maxTop : 0;
    n->dirty = true;
}

void backlog_attach(N2 *n, Obj *o) {
    struct Backlog *b = bl(n);
    b->view = o;
    b->closed = false;
    layout(n, b);
    b->top = b->rowCount;
    sync_bar(n, b);
}

void backlog_set_scrollbar(N2 *n, Obj *bar, Obj *log) {
    struct Backlog *b = bl(n);
    b->bar = bar;
    sync_bar(n, b);
}

void backlog_scroll(N2 *n, int rows) {
    struct Backlog *b = bl(n);
    if (!b->view || !n2_obj_alive(n, b->view)) return;
    int maxTop = b->rowCount - visible_rows(n);
    if (maxTop < 0) maxTop = 0;
    if (rows > 0 && b->top >= maxTop) {
        // Scrolling down past the newest text closes the backlog, as on the PC.
        b->closed = true;
        return;
    }
    b->top += rows;
    sync_bar(n, b);
}

void backlog_bar_request(N2 *n, Obj *bar, const char *act) {
    int page = visible_rows(n);
    if (n2_streq_ci(act, "Prev")) backlog_scroll(n, -page);
    else if (n2_streq_ci(act, "Next")) backlog_scroll(n, page);
    else if (n2_streq_ci(act, "Decrement")) backlog_scroll(n, -1);
    else if (n2_streq_ci(act, "Increment")) backlog_scroll(n, 1);
}

// Each frame while the backlog shows: the wheel scrolls, a click on a speaker replays the voice.
void backlog_input(N2 *n) {
    struct Backlog *b = n->backlog;
    if (!b || !b->view || !n2_obj_alive(n, b->view)) {
        if (b) b->view = NULL;
        return;
    }
    if (n->wheel) {
        backlog_scroll(n, -n->wheel * 3);
        n->wheel = 0;
    }
    if (b->bar && b->bar->sbDragging) {
        int maxTop = b->rowCount - visible_rows(n);
        if (maxTop > 0) b->top = (int)((1.0 - b->bar->sbValue) * maxTop + 0.5);
        n->dirty = true;
    }
    if (n->mousePressed[VNN2_MOUSE_LEFT] && !n->clickOnButton) {
        int x0 = (int)num(n, "$SYSTEM_backlog_voice_icon_x", 20);
        int y0 = (int)num(n, "$SYSTEM_backlog_position_y", 50), dy = (int)num(n, "$SYSTEM_backlog_row_interval", 30);
        int row = dy > 0 ? (n->mouseY - y0) / dy : -1;
        if (n->mouseX >= x0 && n->mouseX < x0 + 28 && row >= 0 && row < visible_rows(n) && b->top + row < b->rowCount) {
            const char *v = b->rows[b->top + row].voice;
            if (v) voice_play(n, v);
        }
    }
}

void backlog_draw(N2 *n, Obj *o) {
    struct Backlog *b = bl(n);
    int x = (int)num(n, "$SYSTEM_backlog_position_x", 50), y = (int)num(n, "$SYSTEM_backlog_position_y", 50);
    int dy = (int)num(n, "$SYSTEM_backlog_row_interval", 30), ix = (int)num(n, "$SYSTEM_backlog_voice_icon_x", 20);
    int rows = visible_rows(n);
    Value *icon = var_get(n, "$SYSTEM_backlog_icon_voice");
    char iconPath[256] = "cg/sys/backlog/speaker-000.png";
    const char *ini = n2_ini(n, "バックログ", "icon_voice");
    if (icon && icon->t == V_STR) ini = icon->s->s;
    if (ini) {
        snprintf(iconPath, sizeof iconPath, "%s", ini);
        char *h = strrchr(iconPath, '#');
        if (h) *h = 0;
        char tmp[256];
        snprintf(tmp, sizeof tmp, iconPath, 0);
        snprintf(iconPath, sizeof iconPath, "%s", tmp);
    }
    Image *speaker = n2_load_image(n, iconPath);
    for (int i = 0; i < rows && b->top + i < b->rowCount; i++) {
        Row *r = &b->rows[b->top + i];
        Obj tmp = {.kind = OBJ_TEXT, .opacity = o->opacity, .zoomX = 1000, .zoomY = 1000, .transition = -1};
        TextObj *t = text_new(n, &tmp, r->text, 0, 0, true);
        tmp.text = t;
        text_draw(n, &tmp, n->screen, x, y + i * dy, o->opacity);
        text_free(t);
        if (r->voice && speaker) {
            Obj io = {.opacity = o->opacity, .zoomX = 1000, .zoomY = 1000, .transition = -1};
            n2_blit(n, &io, speaker, ix, y + i * dy + (dy - speaker->h) / 2.0, (u32)(o->opacity * 256 / 1000));
        }
    }
    img_unref(speaker);
}

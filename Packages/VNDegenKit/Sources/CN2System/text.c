// Text objects: markup parsing, layout, glyphs from stb_truetype, the typewriter, key waits, and the key-wait icons.
//
// A text block from <PRE> runs as its own thread: each markup part is typed out, `{code}` parts run when the
// typewriter reaches them, and WaitText in the main script returns after the last click. Source line breaks are the
// display line breaks; a line that holds only tags adds none. "&" before punctuation shows the punctuation alone,
// as the PC engine does.
#include "n2.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "vnfont.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"

// ---- fonts ----

typedef struct Glyph {
    u32 key;
    int w, h, x0, y0;  // coverage bitmap and its offset from the pen at the baseline
    int adv;
    u8 *cov;
    int ow, oh, ox0, oy0;  // the outline, two pixels wider each way
    u8 *outl;
} Glyph;

struct Font {
    u8 *data[2];
    stbtt_fontinfo info[2];
    int count;
    Glyph *cache;
    int cap, used;
};

static u8 *read_all(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *d = n2_alloc((size_t)n + 1);
    if (fread(d, 1, (size_t)n, f) != (size_t)n) {
        free(d);
        d = NULL;
    }
    fclose(f);
    if (d) *size = (size_t)n;
    return d;
}

static bool font_add(struct Font *f, const char *path, int index) {
    if (f->count >= 2) return false;
    size_t size;
    u8 *d = read_all(path, &size);
    if (!d) return false;
    int off = stbtt_GetFontOffsetForIndex(d, index);
    if (off < 0 || !stbtt_InitFont(&f->info[f->count], d, off)) {
        free(d);
        return false;
    }
    f->data[f->count++] = d;
    return true;
}

struct Font *font_open(const char *path, int index) {
    struct Font *f = n2_alloc(sizeof *f);
    if (path && !font_add(f, path, index)) {
        free(f);
        return NULL;
    }
    return f;
}

// The game's font through the shared font pack, and MS Gothic's substitute for characters it lacks.
struct Font *font_open_named(const char *face) {
    struct Font *f = n2_alloc(sizeof *f);
    vnfont_match m;
    if (vnfont_resolve(face, 0, 0, &m)) font_add(f, m.path, m.face);
    if (vnfont_resolve("MS Gothic", 0, 0, &m)) font_add(f, m.path, m.face);
    if (!f->count) {
        free(f);
        return NULL;
    }
    return f;
}

void font_close(struct Font *f) {
    if (!f) return;
    for (int i = 0; i < f->cap; i++) {
        free(f->cache[i].cov);
        free(f->cache[i].outl);
    }
    free(f->cache);
    for (int i = 0; i < f->count; i++) free(f->data[i]);
    free(f);
}

static void cache_grow(struct Font *f);

static Glyph *glyph(struct Font *f, u32 cp, int size) {
    u32 key = cp << 8 | (u32)(size & 0xff);
    if (f->cap) {
        for (int i = (int)(key * 2654435761u % (u32)f->cap);; i = (i + 1) % f->cap) {
            if (!f->cache[i].key) break;
            if (f->cache[i].key == key) return &f->cache[i];
        }
    }
    if ((f->used + 1) * 2 > f->cap) cache_grow(f);
    int i = (int)(key * 2654435761u % (u32)f->cap);
    while (f->cache[i].key) i = (i + 1) % f->cap;
    Glyph *g = &f->cache[i];
    f->used++;
    g->key = key;
    int fi = 0;
    for (int k = 0; k < f->count; k++) {
        if (stbtt_FindGlyphIndex(&f->info[k], (int)cp)) {
            fi = k;
            break;
        }
    }
    stbtt_fontinfo *info = &f->info[fi];
    float scale = stbtt_ScaleForPixelHeight(info, (float)size);
    int adv, lsb;
    stbtt_GetCodepointHMetrics(info, (int)cp, &adv, &lsb);
    g->adv = (int)lroundf(adv * scale);
    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(info, (int)cp, scale, scale, &x0, &y0, &x1, &y1);
    // Center the font's ascent and descent in the em box so mixed fonts share a baseline.
    int asc, desc, gap;
    stbtt_GetFontVMetrics(info, &asc, &desc, &gap);
    int base = (int)lroundf(asc * scale);
    g->w = x1 - x0;
    g->h = y1 - y0;
    g->x0 = x0;
    g->y0 = y0 + base;
    if (g->w > 0 && g->h > 0) {
        g->cov = n2_alloc((size_t)g->w * (size_t)g->h);
        stbtt_MakeCodepointBitmap(info, g->cov, g->w, g->h, g->w, scale, scale, (int)cp);
        // The outline: coverage dilated by two pixels.
        const int r = 2;
        g->ow = g->w + 2 * r;
        g->oh = g->h + 2 * r;
        g->ox0 = g->x0 - r;
        g->oy0 = g->y0 - r;
        g->outl = n2_alloc((size_t)g->ow * (size_t)g->oh);
        for (int y = 0; y < g->oh; y++) {
            for (int x = 0; x < g->ow; x++) {
                int best = 0;
                for (int dy = -r; dy <= r; dy++) {
                    int sy = y - r + dy;
                    if (sy < 0 || sy >= g->h) continue;
                    for (int dx = -r; dx <= r; dx++) {
                        int sx = x - r + dx;
                        if (sx < 0 || sx >= g->w || dx * dx + dy * dy > r * r + 1) continue;
                        int v = g->cov[sy * g->w + sx];
                        if (v > best) best = v;
                    }
                }
                g->outl[y * g->ow + x] = (u8)best;
            }
        }
    }
    return g;
}

static void cache_grow(struct Font *f) {
    Glyph *old = f->cache;
    int oldCap = f->cap;
    f->cap = f->cap ? f->cap * 2 : 512;
    f->cache = n2_alloc(sizeof(Glyph) * (size_t)f->cap);
    for (int i = 0; i < oldCap; i++) {
        if (!old[i].key) continue;
        int k = (int)(old[i].key * 2654435761u % (u32)f->cap);
        while (f->cache[k].key) k = (k + 1) % f->cap;
        f->cache[k] = old[i];
    }
    free(old);
}

// ---- text objects ----

enum { IT_CHAR, IT_WAIT, IT_VOICE, IT_SE, IT_BREAK };

typedef struct Item {
    int kind;
    u32 cp;
    int x, y;  // pen position, relative to the object, at the line top
    int size;
    u32 color, outline;
    bool outlined;
    char *arg;
} Item;

enum { TS_IDLE, TS_TYPING, TS_WAIT_MID, TS_WAIT_END, TS_DONE };

struct TextObj {
    Obj *obj;
    Item *items;
    int count, cap;
    int visible;
    double acc;
    int state;
    bool block;
    int w, h, lineSpacing, letterSpacing;
    bool autoW;
    int penX, penY;
    bool lineHasGlyph;
    int maxX;
    u32 lastTick;
    u32 waitStart;
    Buf log;
    char *voice;  // the block's first voice, for the backlog
    bool logged;
};

static u32 parse_color(const char *s) {
    while (*s == ' ' || *s == '"' || *s == '=') s++;
    if (*s == '#') return (u32)strtoul(s + 1, NULL, 16) & 0xffffff;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) return (u32)strtoul(s + 2, NULL, 16) & 0xffffff;
    return (u32)strtoul(s, NULL, 10) & 0xffffff;
}

// Reads attribute `name` from a tag's text ("FONT incolor=0x7e7e7e size=20").
static bool tag_attr(const char *tag, const char *name, char *out, size_t size) {
    size_t nl = strlen(name);
    for (const char *p = tag; *p; p++) {
        if ((p == tag || p[-1] == ' ' || p[-1] == '\t') && strncasecmp(p, name, nl) == 0) {
            const char *q = p + nl;
            while (*q == ' ') q++;
            if (*q != '=') continue;
            q++;
            while (*q == ' ') q++;
            bool quoted = *q == '"';
            if (quoted) q++;
            size_t k = 0;
            while (*q && (quoted ? *q != '"' : (*q != ' ' && *q != '>')) && k + 1 < size) out[k++] = *q++;
            out[k] = 0;
            return true;
        }
    }
    return false;
}

static Item *add_item(TextObj *t, int kind) {
    if (t->count == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 256;
        t->items = n2_grow(t->items, sizeof(Item) * (size_t)t->cap);
    }
    Item *it = &t->items[t->count++];
    memset(it, 0, sizeof *it);
    it->kind = kind;
    return it;
}

typedef struct Style {
    int size;
    u32 color, outline;
    bool outlined;
} Style;

static int font_size(N2 *n) { return n->fontSize > 0 ? n->fontSize : 24; }

static void new_line(TextObj *t) {
    t->penX = 0;
    t->penY += t->lineSpacing > 0 ? t->lineSpacing : 0;
    t->lineHasGlyph = false;
}

static void put_char(N2 *n, TextObj *t, u32 cp, Style *st) {
    Glyph *g = glyph(n->font, cp, st->size);
    int adv = g->adv + t->letterSpacing;
    // Wrap at the box width. The English scripts are wrapped already; this catches CreateText and long lines.
    if (!t->autoW && t->w > 0 && t->penX + adv > t->w && t->penX > 0) {
        // Move the word in progress to the next line when it started on this one.
        int start = t->count;
        while (start > 0 && t->items[start - 1].kind == IT_CHAR && t->items[start - 1].cp != ' ' &&
               t->items[start - 1].y == t->penY && cp != ' ')
            start--;
        if (cp == ' ') {
            new_line(t);
            return;
        }
        if (start > 0 && start < t->count && t->items[start].x > 0) {
            int shift = t->items[start].x;
            new_line(t);
            for (int i = start; i < t->count; i++) {
                if (t->items[i].kind != IT_CHAR) continue;
                t->items[i].x -= shift;
                t->items[i].y = t->penY;
            }
            Item *last = &t->items[t->count - 1];
            t->penX = last->x + glyph(n->font, last->cp, last->size)->adv + t->letterSpacing;
        } else {
            new_line(t);
        }
    }
    Item *it = add_item(t, IT_CHAR);
    it->cp = cp;
    it->x = t->penX;
    it->y = t->penY;
    it->size = st->size;
    it->color = st->color;
    it->outline = st->outline;
    it->outlined = st->outlined;
    t->penX += adv;
    if (t->penX > t->maxX) t->maxX = t->penX;
    if (cp != ' ') t->lineHasGlyph = true;
    char u[8];
    int k = 0;
    if (cp < 0x80) u[k++] = (char)cp;
    else if (cp < 0x800) { u[k++] = (char)(0xc0 | cp >> 6); u[k++] = (char)(0x80 | (cp & 0x3f)); }
    else { u[k++] = (char)(0xe0 | cp >> 12); u[k++] = (char)(0x80 | ((cp >> 6) & 0x3f)); u[k++] = (char)(0x80 | (cp & 0x3f)); }
    buf_put(&t->log, u, (size_t)k);
}

static void put_text(N2 *n, TextObj *t, const char *s, Style *st) {
    const char *p = s;
    u32 cp;
    while ((cp = n2_utf8_next(&p))) {
        if (cp == '\r') continue;
        if (cp == '\n') {
            new_line(t);
            buf_u8(&t->log, '\n');
            continue;
        }
        if (cp == '\t') cp = ' ';
        put_char(n, t, cp, st);
    }
}

void text_append(N2 *n, TextObj *t, const char *markup) {
    bool outlined = !n->outlineMode || !n2_streq_ci(n->outlineMode, "NONE");
    Style stack[8] = {{font_size(n), n->fontColor, n->fontOutline, outlined}};
    int depth = 0;
    const char *p = markup;
    while (*p) {
        if (*p == '<') {
            const char *e = strchr(p, '>');
            if (!e) break;
            char *tag = n2_strndup(p + 1, (size_t)(e - p - 1));
            p = e + 1;
            char name[32] = {0};
            sscanf(tag, "%31[^ \t>]", name);
            char val[512];
            if (n2_streq_ci(name, "K") || n2_streq_ci(name, "k")) {
                add_item(t, IT_WAIT);
            } else if (n2_streq_ci(name, "voice")) {
                if (tag_attr(tag, "src", val, sizeof val)) {
                    add_item(t, IT_VOICE)->arg = n2_strdup(val);
                    if (!t->voice) t->voice = n2_strdup(val);
                }
            } else if (n2_streq_ci(name, "se")) {
                if (tag_attr(tag, "src", val, sizeof val)) add_item(t, IT_SE)->arg = n2_strdup(val);
            } else if (n2_streq_ci(name, "FONT")) {
                if (depth < 7) {
                    stack[depth + 1] = stack[depth];
                    depth++;
                }
                if (tag_attr(tag, "incolor", val, sizeof val)) stack[depth].color = parse_color(val);
                if (tag_attr(tag, "outcolor", val, sizeof val)) {
                    stack[depth].outline = parse_color(val);
                    stack[depth].outlined = true;
                }
                if (tag_attr(tag, "size", val, sizeof val)) stack[depth].size = atoi(val);
            } else if (n2_streq_ci(name, "/FONT")) {
                if (depth > 0) depth--;
            } else if (n2_streq_ci(name, "BR")) {
                new_line(t);
                buf_u8(&t->log, '\n');
            } else if (n2_streq_ci(name, "VALUE")) {
                if (tag_attr(tag, "name", val, sizeof val)) {
                    Value *v = var_get(n, val);
                    char tmp[64];
                    if (v) put_text(n, t, v_tostr(v, tmp, sizeof tmp), &stack[depth]);
                }
            } else if (n2_streq_ci(name, "PRE")) {
                // Verbatim up to </PRE>.
                const char *end = strstr(p, "</PRE>");
                if (!end) end = strstr(p, "</pre>");
                if (!end) end = p + strlen(p);
                char *verb = n2_strndup(p, (size_t)(end - p));
                put_text(n, t, verb, &stack[depth]);
                free(verb);
                p = *end ? end + 6 : end;
            }
            free(tag);
            continue;
        }
        if (*p == '\n' || (*p == '\r' && p[1] == '\n')) {
            // A line with no characters on it (only tags) doesn't break.
            if (t->lineHasGlyph) {
                new_line(t);
                buf_u8(&t->log, '\n');
            }
            p += *p == '\r' ? 2 : 1;
            continue;
        }
        if (*p == '&' && p[1] && strchr(".,&!?;:'\"-", p[1])) {
            p++;
        }
        const char *q = p;
        u32 cp = n2_utf8_next(&q);
        p = q;
        if (cp == '\r') continue;
        if (cp == '\t') cp = ' ';
        put_char(n, t, cp, &stack[depth]);
    }
}

TextObj *text_new(N2 *n, Obj *o, const char *markup, int w, int h, bool autoW) {
    TextObj *t = n2_alloc(sizeof *t);
    t->obj = o;
    t->w = w;
    t->h = h;
    t->autoW = autoW || w <= 0;
    t->lineSpacing = font_size(n) + 6;
    text_append(n, t, markup);
    t->visible = t->count;
    t->state = TS_DONE;
    o->w = t->maxX;
    o->h = t->penY + t->lineSpacing;
    // CreateText with "inherit" or Center x centers the text on the screen.
    return t;
}

void text_free(TextObj *t) {
    if (!t) return;
    for (int i = 0; i < t->count; i++) free(t->items[i].arg);
    free(t->items);
    buf_free(&t->log);
    free(t->voice);
    free(t);
}

void text_start_block(N2 *n, Obj *o, TextBlock *b, Script *s, int w, int h, int letterSpacing, int lineSpacing) {
    TextObj *t = n2_alloc(sizeof *t);
    t->obj = o;
    t->block = true;
    t->w = w;
    t->h = h;
    t->letterSpacing = letterSpacing;
    t->lineSpacing = lineSpacing > 0 ? lineSpacing : font_size(n) + 12;
    t->state = TS_IDLE;
    o->text = t;
    o->w = w;
    o->h = h;
    o->erased = true;
    o->thread = thread_new(n, b->name);
    o->thread->owner = o;
    o->thread->textOwner = t;
    o->thread->wait = WAIT_DONE;
    thread_call(n, o->thread, b->chunk, NULL, 0);
    o->thread->wait = WAIT_DONE;  // held until Request(Enter)
    conquest_mark(n, s, b);
}

void text_enter(N2 *n, Obj *o) {
    TextObj *t = o->text;
    if (!t || !t->block || t->state != TS_IDLE || !o->thread) return;
    t->state = TS_TYPING;
    t->lastTick = n->now;
    o->thread->wait = WAIT_NONE;
    if (o->pushText) t->visible = t->count;
}

bool text_segment_done(TextObj *t) {
    return t->visible >= t->count && t->state == TS_TYPING;
}

bool text_finished(TextObj *t) {
    return t->state == TS_DONE;
}

static bool flag(N2 *n, const char *name) {
    Value *v = var_get(n, name);
    return v && v_truthy(v);
}

static double num(N2 *n, const char *name, double def) {
    Value *v = var_get(n, name);
    return v ? v_tonum(v) : def;
}

static void log_block(N2 *n, TextObj *t) {
    if (t->logged || t->obj->noLog) return;
    t->logged = true;
    buf_u8(&t->log, 0);
    backlog_add(n, (char *)t->log.data, t->voice);
    t->log.len--;
}

// Reveals items up to `limit`, playing the last voice and any sound effects passed.
static void reveal_to(N2 *n, TextObj *t, int limit, bool clicking) {
    const char *voice = NULL;
    while (t->visible < limit) {
        Item *it = &t->items[t->visible];
        if (it->kind == IT_WAIT) break;
        if (it->kind == IT_VOICE) voice = it->arg;
        else if (it->kind == IT_SE && !n2_skip_active(n)) {
            char *path = n2_fmt("sound/se/%s", it->arg);
            voice_se(n, path);
            free(path);
        }
        t->visible++;
    }
    if (voice && !n2_skip_active(n)) voice_play(n, voice);
    (void)clicking;
}

static int next_stop(TextObj *t) {
    for (int i = t->visible; i < t->count; i++)
        if (t->items[i].kind == IT_WAIT) return i;
    return t->count;
}

void text_update(N2 *n, Obj *o) {
    TextObj *t = o->text;
    if (!t->block || t->state == TS_IDLE || t->state == TS_DONE) return;
    bool skip = n2_skip_active(n);
    bool threadDone = !o->thread || o->thread->wait == WAIT_DONE;
    if (t->state == TS_TYPING) {
        int stop = next_stop(t);
        double speed = num(n, "#SYSTEM_text_speed", 500);
        double msPerChar = speed / 20.0;
        if (skip || o->pushText || msPerChar <= 0.5) {
            reveal_to(n, t, stop, false);
        } else {
            t->acc += (double)(n->now - t->lastTick) / msPerChar;
            int k = (int)t->acc;
            t->acc -= k;
            int limit = t->visible + k;
            if (limit > stop) limit = stop;
            reveal_to(n, t, limit, false);
        }
        t->lastTick = n->now;
        n->dirty = true;
        if (t->visible < t->count && t->items[t->visible].kind == IT_WAIT && t->visible == stop) {
            t->state = TS_WAIT_MID;
            t->waitStart = n->now;
        } else if (t->visible >= t->count && threadDone) {
            log_block(n, t);
            t->state = flag(n, "$SYSTEM_text_waitkey") || !var_get(n, "$SYSTEM_text_waitkey") ? TS_WAIT_END : TS_DONE;
            t->waitStart = n->now;
        }
    }
    if (t->state == TS_WAIT_MID || t->state == TS_WAIT_END) {
        n->dirty = true;  // the icon animates
        bool go = skip;
        if (!go && flag(n, "$SYSTEM_text_auto")) {
            // Auto mode: the voice has finished and a delay per character has passed.
            int chars = 0;
            for (int i = 0; i < t->visible; i++) chars += t->items[i].kind == IT_CHAR;
            double per = num(n, "#SYSTEM_auto_wait_per_character", 50);
            go = !voice_playing(n) && n->now - t->waitStart >= (u32)(500 + per * (chars > 60 ? 60 : chars));
        }
        if (go) {
            if (t->state == TS_WAIT_MID) {
                t->visible++;
                t->state = TS_TYPING;
                t->lastTick = n->now;
            } else {
                t->state = TS_DONE;
            }
        }
    }
}

static void find_active(N2 *n, Obj *o, TextObj **out) {
    for (int i = 0; i < o->childCount && !*out; i++) {
        Obj *c = o->children[i];
        if (c->text && c->text->block && (c->text->state == TS_TYPING || c->text->state == TS_WAIT_MID ||
                                          c->text->state == TS_WAIT_END))
            *out = c->text;
        find_active(n, c, out);
    }
}

void text_deliver_input(N2 *n, bool advance) {
    if (!advance || n->clickOnButton) return;
    if (n->system && n->system->wait != WAIT_DONE) return;
    TextObj *t = NULL;
    find_active(n, &n->root, &t);
    if (!t) find_active(n, &n->sysRoot, &t);
    if (!t) return;
    switch (t->state) {
    case TS_TYPING: reveal_to(n, t, next_stop(t), true); break;
    case TS_WAIT_MID:
        if (flag(n, "#SYSTEM_click_break_voice")) voice_stop(n);
        t->visible++;
        t->state = TS_TYPING;
        t->lastTick = n->now;
        break;
    case TS_WAIT_END:
        if (flag(n, "#SYSTEM_click_break_voice")) voice_stop(n);
        t->state = TS_DONE;
        break;
    }
    n->anyPress = false;
    n->dirty = true;
}

int text_width(N2 *n, const char *markup) {
    TextObj t = {0};
    t.autoW = true;
    t.lineSpacing = font_size(n);
    text_append(n, &t, markup);
    for (int i = 0; i < t.count; i++) free(t.items[i].arg);
    free(t.items);
    buf_free(&t.log);
    free(t.voice);
    return t.maxX;
}

// ---- drawing ----

static inline void put_cov(u32 *dst, int x, int y, u8 cov, u32 rgb, u32 alpha) {
    if (x < 0 || y < 0 || x >= N2_W || y >= N2_H || !cov) return;
    u32 a = cov * alpha >> 8;
    if (!a) return;
    u32 *d = &dst[(size_t)y * N2_W + x];
    u32 r = rgb >> 16 & 0xff, g = rgb >> 8 & 0xff, b = rgb & 0xff;
    u32 dr = *d & 0xff, dg = *d >> 8 & 0xff, db = *d >> 16 & 0xff;
    dr = (r * a + dr * (255 - a)) / 255;
    dg = (g * a + dg * (255 - a)) / 255;
    db = (b * a + db * (255 - a)) / 255;
    *d = dr | dg << 8 | db << 16 | 0xff000000u;
}

static void draw_glyph(N2 *n, u32 *dst, Item *it, int ox, int oy, u32 alpha) {
    Glyph *g = glyph(n->font, it->cp, it->size);
    // Line tops leave room above the glyphs: center the em box in the line.
    int baseY = oy + it->y;
    int baseX = ox + it->x;
    if (it->outlined && g->outl) {
        for (int y = 0; y < g->oh; y++)
            for (int x = 0; x < g->ow; x++)
                put_cov(dst, baseX + g->ox0 + x, baseY + g->oy0 + y, g->outl[y * g->ow + x], it->outline, alpha);
    }
    if (g->cov) {
        for (int y = 0; y < g->h; y++)
            for (int x = 0; x < g->w; x++)
                put_cov(dst, baseX + g->x0 + x, baseY + g->y0 + y, g->cov[y * g->w + x], it->color, alpha);
    }
}

static Image *icon_frame(N2 *n, const char *var, int *frames) {
    // "$SYSTEM_text_icon_line" is "cg/sys/icon/line/cur00_00_%02d.png#7": a pattern and a frame count.
    Value *v = var_get(n, var);
    if (!v || v->t != V_STR) return NULL;
    char pat[256];
    snprintf(pat, sizeof pat, "%s", v->s->s);
    char *hash = strrchr(pat, '#');
    int count = 1;
    if (hash) {
        count = atoi(hash + 1);
        *hash = 0;
    }
    if (count < 1) count = 1;
    *frames = count;
    int frame = (int)(n->now / 100) % count;
    char path[300];
    snprintf(path, sizeof path, pat, frame);
    return n2_load_image(n, path);
}

void text_draw(N2 *n, Obj *o, u32 *dst, int dx, int dy, double opacity) {
    TextObj *t = o->text;
    if (!t) return;
    u32 alpha = (u32)(fmax(0, fmin(1000, opacity)) * 256 / 1000);
    if (!alpha) return;
    if (o->centerX) dx = (N2_W - t->maxX) / 2;
    int lineH = t->lineSpacing;
    int ox = dx, oy = dy + (t->block ? (lineH - font_size(n)) / 2 : 0);
    int lastX = 0, lastY = 0;
    for (int i = 0; i < t->visible && i < t->count; i++) {
        Item *it = &t->items[i];
        if (it->kind != IT_CHAR) continue;
        draw_glyph(n, dst, it, ox, oy, alpha);
        lastX = it->x + glyph(n->font, it->cp, it->size)->adv;
        lastY = it->y;
    }
    if (t->block && (t->state == TS_WAIT_MID || t->state == TS_WAIT_END) && !flag(n, "$SYSTEM_text_icon_disable")) {
        int frames;
        Image *icon = icon_frame(n, t->state == TS_WAIT_END ? "$SYSTEM_text_icon_page" : "$SYSTEM_text_icon_line", &frames);
        if (flag(n, "$SYSTEM_text_auto")) {
            Image *a = icon_frame(n, "$SYSTEM_text_icon_auto", &frames);
            if (a) {
                img_unref(icon);
                icon = a;
            }
        }
        if (icon) {
            Obj tmp = {.opacity = opacity, .zoomX = 1000, .zoomY = 1000, .transition = -1};
            n2_blit(n, &tmp, icon, ox + lastX + 2, oy + lastY + (font_size(n) - icon->h) / 2.0, alpha);
            img_unref(icon);
        }
    }
}

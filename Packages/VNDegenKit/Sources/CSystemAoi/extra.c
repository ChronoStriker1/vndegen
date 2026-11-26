#include "sa_private.h"

#define NO_EDGE 0xffffffffu

enum { PAGE_MENU, PAGE_CG, PAGE_VIEW, PAGE_SCENES, PAGE_SCENARIOS, PAGE_MUSIC, PAGE_HINT, PAGE_UNITS, PAGE_ITEMS, PAGE_MAZES };
enum { ITEM_CG, ITEM_SCENES, ITEM_MUSIC, ITEM_SCENARIOS, ITEM_UNITS, ITEM_ITEMS, ITEM_MAZES, ITEM_QUIZ, ITEM_HINT, ITEMS };

enum {
    MENU_X = 320,
    MENU_Y = 111,
    MENU_W = 656,
    MENU_H = 64,
    BACK_X = 1125,
    BACK_Y = 671,
    BACK_W = 152,
    BACK_H = 44,
    CG_COLS = 6,
    CG_ROWS = 5,
    CG_X = 2,
    CG_Y = 62,
    CG_W = 208,
    CG_H = 120,
    CG_THUMB_X = 4,
    CG_THUMB_Y = 3,
    CG_BAR_X = 1249,
    CG_UP_Y = 63,
    CG_TRACK_Y = 88,
    CG_TRACK_H = 549,
    CG_DOWN_Y = 637,
    CG_ARROW_W = 28,
    CG_ARROW_H = 25,
    LIST_X = 30,
    LIST_Y = 37,
    LIST_W = 912,
    LIST_H = 50,
    LIST_ROWS = 13,
    LIST_TEXT_X = 83,
    LIST_TEXT_Y = 15,
    LIST_TEXT = 20,
    LIST_SPACING = 3,
    LIST_NEW_X = 886,
    LIST_BAR_X = 940,
    LIST_UP_Y = 6,
    LIST_TRACK_Y = 41,
    LIST_TRACK_H = 640,
    LIST_DOWN_Y = 681,
    LIST_ARROW_W = 36,
    LIST_ARROW_H = 35,
    PREVIEW_X = 1035,
    PREVIEW_Y = 118,
    COUNT_Y = 22,
    COUNT_SEEN_RIGHT = 1122,
    COUNT_SLASH_X = 1126,
    COUNT_TOTAL_X = 1143,
    COUNT_PERCENT_RIGHT = 1241,
    COUNT_SIGN_X = 1247,
    DIGIT_W = 12,
    DIGIT_H = 19,
    TRACK_X = 13,
    TRACK_Y = 64,
    TRACK_DX = 205,
    TRACK_DY = 40,
    TRACK_W = 204,
    TRACK_WIDE = 512,
    TRACK_H = 38,
    TRACK_COLS = 5,
    TRACK_NARROW = 61,
    TRACKS = 64,
    LABEL_SIZE = 12,
    PLAYER_Y = 89,
    PLAYER_STOP_Y = 133,
    PLAYER_H = 37,
    VOL_X = 1093,
    VOL_Y = 179,
    VOL_W = 70,
    VOL_H = 50,
    PLUS_X = 1168,
    MINUS_X = 1218,
    PM_W = 52,
};

enum {
    ART_MENU_BG,
    ART_MENU_ITEM,
    ART_BACK = ART_MENU_ITEM + ITEMS,
    ART_CG_BG,
    ART_CG_LOCKED,
    ART_CG_BACK,
    ART_CG_UP,
    ART_CG_DOWN,
    ART_CG_TRACK,
    ART_CG_THUMB,
    ART_CG_DIGITS,
    ART_SCENE_BG,
    ART_SCENE_ROW,
    ART_SCENE_UP,
    ART_SCENE_DOWN,
    ART_SCENE_TRACK,
    ART_SCENE_THUMB,
    ART_SCENE_DIGITS,
    ART_SCENARIO_BG,
    ART_SCENARIO_ROW,
    ART_SCENARIO_UP,
    ART_SCENARIO_DOWN,
    ART_SCENARIO_TRACK,
    ART_SCENARIO_THUMB,
    ART_MUSIC_BG,
    ART_MUSIC_PREV,
    ART_MUSIC_PLAY,
    ART_MUSIC_NEXT,
    ART_MUSIC_STOP,
    ART_MUSIC_BACK,
    ART_MUSIC_PLUS,
    ART_MUSIC_MINUS,
    ART_MUSIC_DIGITS,
    ART_TRACK,
    ART_TOTAL = ART_TRACK + TRACKS
};

enum {
    HOVER_BACK = 1,
    HOVER_UP,
    HOVER_DOWN,
    HOVER_TRACK_BAR,
    HOVER_PREV,
    HOVER_PLAY,
    HOVER_NEXT,
    HOVER_STOP,
    HOVER_PLUS,
    HOVER_MINUS,
    HOVER_ITEM = 100,
    HOVER_CELL = 200,
    HOVER_ROW = 300,
    HOVER_SONG = 400
};

static const char *const menuArt[ITEMS] = {"exm_b00", "exm_b01", "exm_b02", "exm_b03", "exm_b06", "exm_b07", "exm_b08", "exm_b05", "exm_b04"};

static const char *const artNames[ART_TRACK] = {
    [ART_MENU_BG] = "exm_a00",       [ART_BACK] = "exm_a01",         [ART_CG_BG] = "exc_a00",          [ART_CG_LOCKED] = "exc_a01",
    [ART_CG_BACK] = "exc_a02",       [ART_CG_UP] = "exc_a03",        [ART_CG_DOWN] = "exc_a04",        [ART_CG_TRACK] = "exc_a05",
    [ART_CG_THUMB] = "exc_a06",      [ART_CG_DIGITS] = "exc_1219",   [ART_SCENE_BG] = "ext_a00",       [ART_SCENE_ROW] = "ext_a01",
    [ART_SCENE_UP] = "ext_a04",      [ART_SCENE_DOWN] = "ext_a05",   [ART_SCENE_TRACK] = "ext_a06",    [ART_SCENE_THUMB] = "ext_a07",
    [ART_SCENE_DIGITS] = "ext_1219", [ART_SCENARIO_BG] = "ext_b00",  [ART_SCENARIO_ROW] = "ext_b01",   [ART_SCENARIO_UP] = "ext_b04",
    [ART_SCENARIO_DOWN] = "ext_b05", [ART_SCENARIO_TRACK] = "ext_b06", [ART_SCENARIO_THUMB] = "ext_b07", [ART_MUSIC_BG] = "exs_a00",
    [ART_MUSIC_PREV] = "exs_a01",    [ART_MUSIC_PLAY] = "exs_a02",   [ART_MUSIC_NEXT] = "exs_a03",     [ART_MUSIC_STOP] = "exs_a04",
    [ART_MUSIC_BACK] = "exs_a05",    [ART_MUSIC_PLUS] = "exs_c00",   [ART_MUSIC_MINUS] = "exs_c01",    [ART_MUSIC_DIGITS] = "exs_c02",
};

static void load_samples(SA *s) {
    ExtraScreen *e = &s->extra;
    char *p = sa_fmt("%s/sam.vfs", s->gameDir);
    char real[2048];
    FILE *f = sa_path_resolve(p, real, sizeof real) == 1 ? fopen(real, "rb") : NULL;
    free(p);
    if (!f) return;
    u8 h[16];
    if (fread(h, 1, 16, f) != 16) {
        fclose(f);
        return;
    }
    int count = rd16(h + 4), size = rd16(h + 6);
    u8 *index = sa_alloc((size_t)count * size + 8);
    if (fread(index, 1, (size_t)count * size + 8, f) != (size_t)count * size + 8) {
        free(index);
        fclose(f);
        return;
    }
    u32 chars = rd32(index + (size_t)count * size);
    u8 *names = sa_alloc((size_t)chars * 2 + 2);
    if (fread(names, 1, (size_t)chars * 2, f) != (size_t)chars * 2) chars = 0;
    e->samples = sa_alloc(sizeof *e->samples * (size_t)count);
    for (int i = 0; i < count; i++) {
        const u8 *r = index + (size_t)i * size;
        u32 at = rd32(r);
        if (at >= chars) continue;
        int len = 0;
        while (at + len < chars && (names[(at + len) * 2] || names[(at + len) * 2 + 1])) len++;
        char *n = sa_utf16_to_utf8(names + at * 2, len);
        SampleEntry *se = &e->samples[e->sampleCount++];
        snprintf(se->name, sizeof se->name, "%s", n);
        se->offset = rd32(r + 0xa);
        se->size = rd32(r + 0xe);
        free(n);
    }
    free(index);
    free(names);
    e->sampleFile = f;
}

static Image *sample(SA *s, const char *name) {
    ExtraScreen *e = &s->extra;
    for (int i = 0; i < e->sampleCount; i++) {
        if (!sa_ieq(e->samples[i].name, name)) continue;
        u8 *d = sa_alloc(e->samples[i].size);
        Image *im = NULL;
        if (!fseek(e->sampleFile, (long)e->samples[i].offset, SEEK_SET) && fread(d, 1, e->samples[i].size, e->sampleFile) == e->samples[i].size)
            im = sa_agf_decode(d, e->samples[i].size);
        free(d);
        return im;
    }
    return NULL;
}

void sa_extra_init(SA *s) {
    ExtraScreen *e = &s->extra;
    char n[24];
    for (int i = 0; i < ART_TRACK; i++) {
        if (!artNames[i]) continue;
        snprintf(n, sizeof n, "%s.agf", artNames[i]);
        e->art[i] = sa_load_image(&s->vfs, n);
    }
    for (int i = 0; i < ITEMS; i++) {
        snprintf(n, sizeof n, "%s.agf", menuArt[i]);
        e->art[ART_MENU_ITEM + i] = sa_load_image(&s->vfs, n);
    }
    for (int i = 0; i < TRACKS; i++) {
        snprintf(n, sizeof n, "exs_b%02d.agf", i + 1);
        e->art[ART_TRACK + i] = sa_load_image(&s->vfs, n);
    }
    sa_table_load(&e->cg, &s->vfs, "ExCg.obj");
    sa_table_load(&e->scenes, &s->vfs, "ExEvntA.obj");
    sa_table_load(&e->scenarios, &s->vfs, "ExEvntB.obj");
    sa_table_load(&e->hints, &s->vfs, "ExHint.obj");
    for (int i = 0; i < 30; i++) e->thumbFor[i] = -1;
    e->playing = -1;
    e->hover = -1;
    e->book.pick = -1;
    load_samples(s);
    sa_records_load(s);
}

static void drop_thumbs(ExtraScreen *e) {
    for (int i = 0; i < 30; i++) {
        sa_image_free(e->thumb[i]);
        e->thumb[i] = NULL;
        e->thumbFor[i] = -1;
    }
}

static void book_free_art(BookScreen *b);

void sa_extra_free(SA *s) {
    ExtraScreen *e = &s->extra;
    for (int i = 0; i < EXTRA_ART; i++) sa_image_free(e->art[i]);
    drop_thumbs(e);
    sa_image_free(e->view);
    sa_table_free(&e->cg);
    sa_table_free(&e->scenes);
    sa_table_free(&e->scenarios);
    sa_table_free(&e->hints);
    sa_records_flush(s);
    book_free_art(&e->book);
    for (int i = 0; i < 100; i++) {
        sa_image_free(e->book.icon[i]);
        sa_image_free(e->book.bigIcon[i]);
    }
    sa_table_free(&e->book.items);
    free(e->samples);
    if (e->sampleFile) fclose(e->sampleFile);
    memset(e, 0, sizeof *e);
}

static int cg_count(const ExtraScreen *e) { return e->cg.rows < EXTRA_CG ? e->cg.rows : EXTRA_CG; }

static bool flag(const SA *s, int id) { return id >= 0 && id < SA_G_VARS && s->vm.g[id]; }

static bool cg_open(const SA *s, int i) { return flag(s, sa_table_int(&s->extra.cg, i, 1)); }

static const Table *list_table(const SA *s) { return s->extra.page == PAGE_SCENES ? &s->extra.scenes : &s->extra.scenarios; }

static u8 *list_viewed(SA *s) { return s->extra.page == PAGE_SCENES ? s->extra.viewedScene : s->extra.viewedScenario; }

static int list_count(const SA *s) {
    int n = list_table(s)->rows, cap = s->extra.page == PAGE_SCENES ? EXTRA_SCENES : EXTRA_SCENARIOS;
    return n < cap ? n : cap;
}

static bool row_open(const SA *s, int i) { return flag(s, sa_table_int(list_table(s), i, 1)); }

static int book_scroll_max(const SA *s);
static bool book_page(int page);
static void book_sort(SA *s);
static void book_open(SA *s, int page);
static void book_close(SA *s);
static int book_hit(SA *s, int x, int y);

static int scroll_max(const SA *s) {
    if (s->extra.page >= PAGE_UNITS) return book_scroll_max(s);
    if (s->extra.page == PAGE_CG) {
        int rows = (cg_count(&s->extra) + CG_COLS - 1) / CG_COLS;
        return rows > CG_ROWS ? rows - CG_ROWS : 0;
    }
    int n = list_count(s);
    return n > LIST_ROWS ? n - LIST_ROWS : 0;
}

void sa_extra_seen_picture(SA *s, const char *name) {
    const Table *t = &s->extra.cg;
    for (int i = 0; i < t->rows; i++) {
        if (!sa_ieq(sa_table_text(t, i, 0), name)) continue;
        int id = sa_table_int(t, i, 1);
        if (id >= 0 && id < SA_G_VARS && !s->vm.g[id]) {
            s->vm.g[id] = 1;
            s->vm.gDirty = true;
        }
        return;
    }
}

static void stop_music(SA *s) {
    if (s->extra.playing >= 0) sa_stop(s->mix, CH_BGM, 0, 300);
    s->extra.playing = -1;
}

static void show_page(SA *s, int page) {
    ExtraScreen *e = &s->extra;
    if (e->page == PAGE_MUSIC && page != PAGE_MUSIC) stop_music(s);
    if (book_page(e->page)) book_close(s);
    e->page = page;
    if (book_page(page)) {
        book_open(s, page);
        book_sort(s);
    }
    e->hover = -1;
    e->scroll = 0;
    drop_thumbs(e);
    s->mode = MODE_EXTRA;
    s->dirty = true;
}

void sa_extra_open(SA *s) {
    sa_reset_scene(s);
    show_page(s, PAGE_MENU);
}

bool sa_extra_script_done(SA *s) {
    ExtraScreen *e = &s->extra;
    if (!e->resume) return false;
    e->resume = false;
    sa_reset_scene(s);
    sa_stop(s->mix, CH_VOICE, 0, 0);
    sa_stop(s->mix, CH_BGM, 0, 500);
    int page = e->page, scroll = e->scroll;
    show_page(s, page);
    e->scroll = scroll;
    return true;
}

static bool in(int x, int y, int rx, int ry, int w, int h) { return x >= rx && x < rx + w && y >= ry && y < ry + h; }

static int track_x(int i) { return i < TRACK_NARROW ? TRACK_X + i % TRACK_COLS * TRACK_DX : TRACK_X; }

static int track_y(int i) { return i < TRACK_NARROW ? TRACK_Y + i / TRACK_COLS * TRACK_DY : TRACK_Y + (TRACK_NARROW / TRACK_COLS + 1 + i - TRACK_NARROW) * TRACK_DY; }

static int track_w(int i) { return i < TRACK_NARROW ? TRACK_W : TRACK_WIDE; }

static int hit(SA *s, int x, int y) {
    ExtraScreen *e = &s->extra;
    switch (e->page) {
    case PAGE_MENU:
        for (int i = 0; i < ITEMS; i++)
            if (in(x, y, MENU_X, MENU_Y + i * MENU_H, MENU_W, MENU_H)) return HOVER_ITEM + i;
        break;
    case PAGE_CG:
        if (in(x, y, CG_BAR_X, CG_UP_Y, CG_ARROW_W, CG_ARROW_H)) return HOVER_UP;
        if (in(x, y, CG_BAR_X, CG_DOWN_Y, CG_ARROW_W, CG_ARROW_H)) return HOVER_DOWN;
        if (in(x, y, CG_BAR_X, CG_TRACK_Y, CG_ARROW_W, CG_TRACK_H)) return HOVER_TRACK_BAR;
        for (int r = 0; r < CG_ROWS; r++)
            for (int c = 0; c < CG_COLS; c++)
                if (in(x, y, CG_X + c * CG_W, CG_Y + r * CG_H, CG_W, CG_H)) {
                    int i = (e->scroll + r) * CG_COLS + c;
                    return i < cg_count(e) ? HOVER_CELL + r * CG_COLS + c : -1;
                }
        break;
    case PAGE_SCENES:
    case PAGE_SCENARIOS:
        if (in(x, y, LIST_BAR_X, LIST_UP_Y, LIST_ARROW_W, LIST_ARROW_H)) return HOVER_UP;
        if (in(x, y, LIST_BAR_X, LIST_DOWN_Y, LIST_ARROW_W, LIST_ARROW_H)) return HOVER_DOWN;
        if (in(x, y, LIST_BAR_X, LIST_TRACK_Y, LIST_ARROW_W, LIST_TRACK_H)) return HOVER_TRACK_BAR;
        for (int r = 0; r < LIST_ROWS; r++)
            if (in(x, y, LIST_X, LIST_Y + r * LIST_H, LIST_W, LIST_H)) return e->scroll + r < list_count(s) ? HOVER_ROW + r : -1;
        break;
    case PAGE_MUSIC:
        for (int i = 0; i < TRACKS; i++)
            if (in(x, y, track_x(i), track_y(i), track_w(i), TRACK_H)) return HOVER_SONG + i;
        if (in(x, y, 1050, PLAYER_Y, 68, PLAYER_H)) return HOVER_PREV;
        if (in(x, y, 1115, PLAYER_Y, 92, PLAYER_H)) return HOVER_PLAY;
        if (in(x, y, 1204, PLAYER_Y, 68, PLAYER_H)) return HOVER_NEXT;
        if (in(x, y, 1115, PLAYER_STOP_Y, 92, PLAYER_H)) return HOVER_STOP;
        if (in(x, y, PLUS_X, VOL_Y, PM_W, VOL_H)) return HOVER_PLUS;
        if (in(x, y, MINUS_X, VOL_Y, PM_W, VOL_H)) return HOVER_MINUS;
        break;
    case PAGE_UNITS:
    case PAGE_ITEMS:
    case PAGE_MAZES:
        return book_hit(s, x, y);
    default:
        return -1;
    }
    if (in(x, y, BACK_X, BACK_Y, BACK_W, BACK_H)) return HOVER_BACK;
    return -1;
}

static void digits(const Image *sheet, u32 *fb, const char *t, int x, int y) {
    static const char chars[] = "0123456789+-/%?";
    for (; *t; t++, x += DIGIT_W) {
        const char *c = strchr(chars, *t);
        if (c) sa_blit_rect(fb, sheet, (int)(c - chars) * DIGIT_W, 0, DIGIT_W, DIGIT_H, x, y);
    }
}

static void counter(const Image *sheet, u32 *fb, int seen, int total) {
    char t[16];
    snprintf(t, sizeof t, "%d", seen);
    digits(sheet, fb, t, COUNT_SEEN_RIGHT - (int)strlen(t) * DIGIT_W, COUNT_Y);
    snprintf(t, sizeof t, "%d", total);
    digits(sheet, fb, t, COUNT_TOTAL_X, COUNT_Y);
    snprintf(t, sizeof t, "%d", total ? seen * 100 / total : 0);
    digits(sheet, fb, t, COUNT_PERCENT_RIGHT - (int)strlen(t) * DIGIT_W, COUNT_Y);
}

static void bar(const ExtraScreen *e, u32 *fb, int up, int down, int track, int thumb, int x, int upY, int trackY, int trackH, int downY,
                int w, int ah, int scroll, int max, int visible, int total) {
    sa_blit_rect(fb, e->art[up], 0, e->hover == HOVER_UP ? ah : 0, w, ah, x, upY);
    sa_blit_rect(fb, e->art[down], 0, e->hover == HOVER_DOWN ? ah : 0, w, ah, x, downY);
    sa_blit(fb, e->art[track], x, trackY);
    const Image *t = e->art[thumb];
    if (!t || total <= 0) return;
    int len = trackH * visible / (total > visible ? total : visible);
    if (len < 12) len = 12;
    int at = trackY + (max > 0 ? (trackH - len) * scroll / max : 0);
    int tx = x + (w - t->w) / 2;
    sa_blit_rect(fb, t, 0, 0, t->w, len - 6, tx, at);
    sa_blit_rect(fb, t, 0, t->h - 6, t->w, 6, tx, at + len - 6);
}

static void darken_box(u32 *fb, int x, int y, int w, int h) {
    for (int j = y; j < y + h && j < SA_H; j++)
        for (int i = x; i < x + w && i < SA_W; i++) {
            u32 p = fb[(size_t)j * SA_W + i];
            fb[(size_t)j * SA_W + i] = 0xff000000u | (p >> 1 & 0x7f7f7f);
        }
}

static void tint_blue(u32 *fb, int x, int y, int w, int h) {
    for (int j = y; j < y + h && j < SA_H; j++)
        for (int i = x; i < x + w && i < SA_W; i++) {
            u32 p = fb[(size_t)j * SA_W + i];
            fb[(size_t)j * SA_W + i] = (p & 0xffff0000u) | (p >> 1 & 0x7f7f);
        }
}

static void draw_menu(SA *s, u32 *fb) {
    ExtraScreen *e = &s->extra;
    sa_blit(fb, e->art[ART_MENU_BG], 0, 0);
    for (int i = 0; i < ITEMS; i++)
        sa_blit_rect(fb, e->art[ART_MENU_ITEM + i], 0, e->hover == HOVER_ITEM + i ? MENU_H : 0, MENU_W, MENU_H, MENU_X, MENU_Y + i * MENU_H);
    sa_blit_rect(fb, e->art[ART_BACK], 0, e->hover == HOVER_BACK ? BACK_H : 0, BACK_W, BACK_H, BACK_X, BACK_Y);
}

static Image *thumb(SA *s, int slot, int index) {
    ExtraScreen *e = &s->extra;
    if (e->thumbFor[slot] != index) {
        sa_image_free(e->thumb[slot]);
        e->thumb[slot] = sample(s, sa_table_text(&e->cg, index, 0));
        e->thumbFor[slot] = index;
    }
    return e->thumb[slot];
}

static void label(u32 *fb, const char *t, int x, int y, int w, u32 color, bool gray) {
    if (gray) sa_fill(fb, x, y, w, 12, 0xff7d7e7eu);
    else darken_box(fb, x, y, w, 12);
    sa_font_set_plain(true);
    sa_font_set_spacing(0);
    sa_text_draw(fb, SA_W, SA_H, t, -1, x + 1, y, LABEL_SIZE, color, 0xffffffffu);
    sa_font_set_spacing(1);
    sa_font_set_plain(false);
}

static void draw_cg(SA *s, u32 *fb) {
    ExtraScreen *e = &s->extra;
    sa_blit(fb, e->art[ART_CG_BG], 0, 0);
    int n = cg_count(e), seen = 0;
    for (int i = 0; i < n; i++) seen += cg_open(s, i);
    for (int r = 0; r < CG_ROWS; r++)
        for (int c = 0; c < CG_COLS; c++) {
            int i = (e->scroll + r) * CG_COLS + c, x = CG_X + c * CG_W, y = CG_Y + r * CG_H;
            if (i >= n) continue;
            bool open = cg_open(s, i);
            sa_blit(fb, e->art[ART_CG_LOCKED], x, y);
            if (open) {
                Image *t = thumb(s, r * CG_COLS + c, i);
                sa_blit(fb, t, x + CG_THUMB_X, y + CG_THUMB_Y);
            }
            char t[16];
            snprintf(t, sizeof t, "No.%03d", i + 1);
            label(fb, t, x + 2, y + 3, 36, 0xffffff, false);
            if (open && !e->viewedCg[i]) label(fb, "NEW", x + 186, y + 3, 18, 0x00ffff, true);
            if (open && e->hover == HOVER_CELL + r * CG_COLS + c) tint_blue(fb, x, y, CG_W, CG_H - 1);
        }
    bar(e, fb, ART_CG_UP, ART_CG_DOWN, ART_CG_TRACK, ART_CG_THUMB, CG_BAR_X, CG_UP_Y, CG_TRACK_Y, CG_TRACK_H, CG_DOWN_Y, CG_ARROW_W, CG_ARROW_H,
        e->scroll, scroll_max(s), CG_ROWS, (n + CG_COLS - 1) / CG_COLS);
    counter(e->art[ART_CG_DIGITS], fb, seen, n);
    sa_blit_rect(fb, e->art[ART_CG_BACK], 0, e->hover == HOVER_BACK ? BACK_H : 0, BACK_W, BACK_H, BACK_X, BACK_Y);
}

static void draw_list(SA *s, u32 *fb) {
    ExtraScreen *e = &s->extra;
    bool scenes = e->page == PAGE_SCENES;
    const Table *t = list_table(s);
    const u8 *viewed = list_viewed(s);
    sa_blit(fb, e->art[scenes ? ART_SCENE_BG : ART_SCENARIO_BG], 0, 0);
    int n = list_count(s), seen = 0;
    for (int i = 0; i < n; i++) seen += row_open(s, i);
    const Image *row = e->art[scenes ? ART_SCENE_ROW : ART_SCENARIO_ROW];
    sa_font_set_spacing(LIST_SPACING);
    for (int r = 0; r < LIST_ROWS; r++) {
        int i = e->scroll + r, y = LIST_Y + r * LIST_H;
        if (i >= n) break;
        sa_blit_rect(fb, row, 0, e->hover == HOVER_ROW + r ? 52 : 0, LIST_W, 52, LIST_X, y);
        bool open = row_open(s, i);
        sa_text_draw(fb, SA_W, SA_H, open ? sa_table_text(t, i, 0) : "？？？？？？？？？？？？", -1, LIST_TEXT_X, y + LIST_TEXT_Y, LIST_TEXT, 0xffffff, 0x000000);
        if (open && !viewed[i]) sa_text_draw(fb, SA_W, SA_H, "NEW", -1, LIST_NEW_X, y + LIST_TEXT_Y, LIST_TEXT, 0x00ffff, 0x000000);
    }
    sa_font_set_spacing(1);
    if (e->hover >= HOVER_ROW && e->hover < HOVER_SONG) {
        int i = e->scroll + e->hover - HOVER_ROW;
        if (i < n && row_open(s, i)) {
            if (e->thumbFor[0] != 1000 + i) {
                sa_image_free(e->thumb[0]);
                e->thumb[0] = sample(s, sa_table_text(t, i, 4));
                e->thumbFor[0] = 1000 + i;
            }
            sa_blit(fb, e->thumb[0], PREVIEW_X, PREVIEW_Y);
        }
    }
    if (scenes)
        bar(e, fb, ART_SCENE_UP, ART_SCENE_DOWN, ART_SCENE_TRACK, ART_SCENE_THUMB, LIST_BAR_X, LIST_UP_Y, LIST_TRACK_Y, LIST_TRACK_H, LIST_DOWN_Y,
            LIST_ARROW_W, LIST_ARROW_H, e->scroll, scroll_max(s), LIST_ROWS, n);
    else
        bar(e, fb, ART_SCENARIO_UP, ART_SCENARIO_DOWN, ART_SCENARIO_TRACK, ART_SCENARIO_THUMB, LIST_BAR_X, LIST_UP_Y, LIST_TRACK_Y, LIST_TRACK_H,
            LIST_DOWN_Y, LIST_ARROW_W, LIST_ARROW_H, e->scroll, scroll_max(s), LIST_ROWS, n);
    counter(e->art[ART_SCENE_DIGITS], fb, seen, n);
    sa_blit_rect(fb, e->art[ART_BACK], 0, e->hover == HOVER_BACK ? BACK_H : 0, BACK_W, BACK_H, BACK_X, BACK_Y);
}

static void draw_music(SA *s, u32 *fb) {
    ExtraScreen *e = &s->extra;
    sa_blit(fb, e->art[ART_MUSIC_BG], 0, 0);
    for (int i = 0; i < TRACKS; i++) {
        int frame = e->playing == i ? 2 : e->hover == HOVER_SONG + i ? 1 : 0;
        sa_blit_rect(fb, e->art[ART_TRACK + i], 0, frame * TRACK_H, track_w(i), TRACK_H, track_x(i), track_y(i));
    }
    sa_blit_rect(fb, e->art[ART_MUSIC_PREV], 0, e->hover == HOVER_PREV ? PLAYER_H : 0, 68, PLAYER_H, 1050, PLAYER_Y);
    sa_blit_rect(fb, e->art[ART_MUSIC_PLAY], 0, e->playing >= 0 ? 2 * PLAYER_H : e->hover == HOVER_PLAY ? PLAYER_H : 0, 92, PLAYER_H, 1115, PLAYER_Y);
    sa_blit_rect(fb, e->art[ART_MUSIC_NEXT], 0, e->hover == HOVER_NEXT ? PLAYER_H : 0, 68, PLAYER_H, 1204, PLAYER_Y);
    sa_blit_rect(fb, e->art[ART_MUSIC_STOP], 0, e->hover == HOVER_STOP ? PLAYER_H : 0, 92, PLAYER_H, 1115, PLAYER_STOP_Y);
    int v = sa_option(s, OPT_BGM);
    sa_blit_rect(fb, e->art[ART_MUSIC_DIGITS], v * VOL_W, 0, VOL_W, VOL_H, VOL_X, VOL_Y);
    sa_blit_rect(fb, e->art[ART_MUSIC_PLUS], 0, v == 15 ? 2 * VOL_H : e->hover == HOVER_PLUS ? VOL_H : 0, PM_W, VOL_H, PLUS_X, VOL_Y);
    sa_blit_rect(fb, e->art[ART_MUSIC_MINUS], 0, v == 0 ? 2 * VOL_H : e->hover == HOVER_MINUS ? VOL_H : 0, PM_W, VOL_H, MINUS_X, VOL_Y);
    sa_blit_rect(fb, e->art[ART_MUSIC_BACK], 0, e->hover == HOVER_BACK ? BACK_H : 0, BACK_W, BACK_H, BACK_X, BACK_Y);
}

enum {
    BOOK_ROW_X = 188,
    BOOK_ROW_Y = 65,
    ITEM_ROW_Y = 64,
    BOOK_ROW_W = 584,
    BOOK_ROW_H = 32,
    BOOK_SHOWN = 20,
    BOOK_SORT_Y = 17,
    BOOK_SORT_W = 60,
    BOOK_SORT_H = 20,
    BOOK_BAR_X = 772,
    BOOK_UP_Y = 30,
    BOOK_TRACK_Y = 65,
    BOOK_TRACK_H = 610,
    BOOK_DOWN_Y = 675,
    BOOK_ARROW_W = 36,
    BOOK_ARROW_H = 35,
    BOOK_TEXT = 14,
    BOOK_NAME_X = 222,
    BOOK_NAME_SPACING = 8,
    BOOK_TEXT_DY = 9,
    UNIT_RACE_RIGHT = 584,
    UNIT_RACE = 9,
    UNIT_RACE_DY = 14,
    UNIT_LV_RIGHT = 640,
    UNIT_KILLS_RIGHT = 744,
    UNIT_NONE_X = 677,
    UNIT_CARD_X = 814,
    UNIT_CARD_Y = 70,
    SPRITE_ANCHOR_X = 75,
    SPRITE_ANCHOR_Y = 162,
    MOTION_X = 1094,
    MOTION_Y = 58,
    MOTION_BTN_X = 1100,
    MOTION_FRONT_Y = 101,
    MOTION_BACK_Y = 249,
    MOTION_DY = 23,
    MOTION_W = 92,
    MOTION_H = 17,
    MOTIONS = 6,
    MOTION_MS = 100,
    ITEM_ICON_X = 194,
    ITEM_ICON_DY = 4,
    ITEM_ICON = 24,
    ITEM_NAME_X = 232,
    ITEM_BUY_RIGHT = 636,
    ITEM_SELL_RIGHT = 740,
    ITEM_CARD_X = 882,
    ITEM_CARD_Y = 150,
    MAZE_ROW_X = 10,
    MAZE_ROW_Y = 45,
    MAZE_ROW_W = 616,
    MAZE_ROW_H = 51,
    MAZE_SHOWN = 13,
    MAZE_BAR_X = 625,
    MAZE_UP_Y = 11,
    MAZE_TRACK_Y = 46,
    MAZE_TRACK_H = 634,
    MAZE_DOWN_Y = 680,
    MAZE_NAME_X = 90,
    MAZE_NAME = 24,
    MAZE_NAME_DY = 13,
    MAZE_LOCK_X = 92,
    MAZE_LOCK = 24,
    MAZE_LOCK_DY = 12,
    MAZE_PCT_RIGHT = 570,
    MAZE_PCT = 16,
    MAZE_PCT_DY = 17,
    MAZE_PANEL_X = 670,
    MAZE_PANEL_Y = 58,
    MAZE_PANEL_W = 600,
    MAZE_PANEL_H = 550,
    MAZE_NOTE_X = 833,
    MAZE_NOTE_Y = 305,
    MAZE_NOTE = 16,
    MAZE_NOTE_DY = 18,
    MAP_CELL = 32,
    MAP_DX = 4,
    MAP_DY = -3,
    HOVER_SORT = 500,
    HOVER_MOTION = 600,
    HOVER_TOGGLE = 20,
};

enum {
    BOOK_BG,
    BOOK_ROW,
    BOOK_BACK,
    BOOK_SORT0,
    BOOK_SORT1,
    BOOK_SORT2,
    BOOK_UP,
    BOOK_DOWN,
    BOOK_TRACK,
    BOOK_THUMB,
    BOOK_DIGITS,
    BOOK_MOTION_PANEL,
    BOOK_MOTION,
    BOOK_ITEM_CARD = BOOK_MOTION + 12,
    BOOK_ITEM_STAR,
    BOOK_ARTS
};

static const char *const unitArt[] = {"exu_a00", "exu_a01", "exu_a02", "exu_a10", "exu_a12", "exu_a13", "exu_a20", "exu_a21", "exu_a22", "exu_a23", "exu_1219", "exu_c99"};
static const char *const itemArt[] = {"exi_a00", "exi_a01", "exi_a02", "exi_a11", "exi_a12", "exi_a13", "exi_a20", "exi_a21", "exi_a22", "exi_a23", "exi_1219", NULL};
static const char *const mazeArt[] = {"xdn_a00", "xdn_c00", "xdn_f03", NULL, NULL, NULL, "xdn_d00", "xdn_d01", "xdn_d02", "xdn_d03", "xdn_1219", "xdn_e00"};
static const char motionLetter[MOTIONS] = {'a', 'b', 'c', 'd', 'e', 'f'};
static const int mapObjects[][2] = {{100, 3}, {60, 4}, {101, 201}, {102, 202}, {103, 203}, {104, 204}, {42, 205}, {39, 107}, {41, 200}};
enum { MAP_OBJECTS = sizeof mapObjects / sizeof mapObjects[0], MAP_FLOOR_TILE = MAP_OBJECTS, MAP_WALL_TILE = MAP_FLOOR_TILE + 1 };

static bool book_page(int page) { return page == PAGE_UNITS || page == PAGE_ITEMS || page == PAGE_MAZES; }

static const Table *book_table(SA *s) {
    switch (s->extra.page) {
    case PAGE_UNITS:
        return &s->party.units;
    case PAGE_ITEMS:
        return &s->extra.book.items;
    default:
        return &s->exp.floors;
    }
}

static int book_shown(const SA *s) { return s->extra.page == PAGE_MAZES ? MAZE_SHOWN : BOOK_SHOWN; }

static int book_count(SA *s) {
    int n = book_table(s)->rows;
    return n < BOOK_ROWS ? n : BOOK_ROWS;
}

static const UnitRecord *unit_record(const SA *s, int row) { return sa_records_unit(s, sa_table_int(&s->party.units, row, 0)); }

static bool book_known(SA *s, int row) {
    switch (s->extra.page) {
    case PAGE_UNITS:
        return unit_record(s, row) != NULL;
    case PAGE_ITEMS:
        return sa_records_has_item(s, sa_table_int(&s->extra.book.items, row, 0));
    default:
        return sa_records_floor(s, sa_table_int(&s->exp.floors, row, 0), sa_table_int(&s->exp.floors, row, 1)) >= 0;
    }
}

static bool is_enemy(const SA *s, int row) { return sa_table_int(&s->party.units, row, 0) >= 1000; }

static int floor_pct(const SA *s, int row) { return sa_records_floor(s, sa_table_int(&s->exp.floors, row, 0), sa_table_int(&s->exp.floors, row, 1)); }

static int item_price(const SA *s, int row) { return sa_table_int(&s->extra.book.items, row, 9); }

static bool item_on_sale(const SA *s, int row) {
    int stock = sa_table_int(&s->extra.book.items, row, 7);
    return stock == 0 || (stock > 0 && stock < SA_F_VARS && s->vm.f[stock]);
}

static u32 sjis_rank(u32 cp) {
    if (cp == 0xff1f || cp == '?') return 0x8148;
    if (cp == 0x30fc) return 0x815b;
    if (cp >= 0x3041 && cp <= 0x3093) return 0x829f + cp - 0x3041;
    if (cp >= 0x30a1 && cp <= 0x30f6) {
        u32 v = 0x8340 + cp - 0x30a1;
        return v >= 0x837f ? v + 1 : v;
    }
    if (cp < 0x80) return cp;
    return 0x8800 + cp;
}

static const char *book_name(SA *s, int row) {
    if (!book_known(s, row)) return "？？？？？";
    return sa_table_text(book_table(s), row, s->extra.page == PAGE_UNITS ? 2 : 1);
}

static int name_compare(const char *a, const char *b) {
    while (*a && *b) {
        u32 ca, cb;
        a += sa_utf8_decode(a, &ca);
        b += sa_utf8_decode(b, &cb);
        u32 ra = sjis_rank(ca), rb = sjis_rank(cb);
        if (ra != rb) return ra < rb ? -1 : 1;
    }
    return *a ? 1 : *b ? -1 : 0;
}

static bool sort_keyed(SA *s, int row) {
    BookScreen *b = &s->extra.book;
    if (b->sortKey == 0) return true;
    if (!book_known(s, row)) return false;
    if (s->extra.page == PAGE_UNITS) return b->sortKey == 1 ? !is_enemy(s, row) : is_enemy(s, row);
    return true;
}

static int sort_value(SA *s, int row) {
    if (s->extra.page == PAGE_UNITS) {
        const UnitRecord *u = unit_record(s, row);
        return s->extra.book.sortKey == 1 ? u->level : u->kills;
    }
    return s->extra.book.sortKey == 1 ? item_price(s, row) / 4 : item_on_sale(s, row) ? item_price(s, row) : -1;
}

static void book_sort(SA *s) {
    BookScreen *b = &s->extra.book;
    int n = book_count(s);
    b->count = n;
    for (int i = 0; i < n; i++) b->order[i] = i;
    if (s->extra.page == PAGE_MAZES || b->sortDir == 0) return;
    int keyed[BOOK_ROWS], rest[BOOK_ROWS], nk = 0, nr = 0;
    for (int i = 0; i < n; i++) {
        if (sort_keyed(s, i)) keyed[nk++] = i;
        else rest[nr++] = i;
    }
    for (int i = 1; i < nk; i++) {
        int v = keyed[i], j = i;
        while (j > 0) {
            int c = b->sortKey == 0 ? name_compare(book_name(s, keyed[j - 1]), book_name(s, v)) : sort_value(s, keyed[j - 1]) - sort_value(s, v);
            if (c <= 0) break;
            keyed[j] = keyed[j - 1];
            j--;
        }
        keyed[j] = v;
    }
    int k = 0;
    for (int i = 0; i < nk; i++) b->order[k++] = keyed[b->sortDir == 2 ? nk - 1 - i : i];
    for (int i = 0; i < nr; i++) b->order[k++] = rest[b->sortDir == 2 ? nr - 1 - i : i];
}

static Image *item_icon(SA *s, int row, bool big) {
    int n = sa_table_int(&s->extra.book.items, row, 3);
    if (n < 0 || n >= 100) return NULL;
    Image **slot = big ? &s->extra.book.bigIcon[n] : &s->extra.book.icon[n];
    if (!*slot) {
        char name[16];
        snprintf(name, sizeof name, "item%02d.agf", n);
        Image *im = sa_load_image(&s->vfs, name);
        *slot = im && !big ? sa_image_scale(im, ITEM_ICON, ITEM_ICON) : im;
        if (im && !big) sa_image_free(im);
    }
    return *slot;
}

static void book_free_art(BookScreen *b) {
    for (int i = 0; i < BOOK_ARTS; i++) {
        sa_image_free(b->art[i]);
        b->art[i] = NULL;
    }
    sa_image_free(b->sprite);
    b->sprite = NULL;
    b->spriteUnit = 0;
    for (int i = 0; i < 24; i++) {
        sa_image_free(b->mapIcon[i]);
        b->mapIcon[i] = NULL;
    }
}

static Image *scale2(Image *im) {
    if (!im) return NULL;
    Image *out = sa_image_scale(im, im->w * 2, im->h * 2);
    sa_image_free(im);
    return out;
}

static void book_load_art(SA *s, int page) {
    BookScreen *b = &s->extra.book;
    book_free_art(b);
    const char *const *names = page == PAGE_UNITS ? unitArt : page == PAGE_ITEMS ? itemArt : mazeArt;
    char n[24];
    for (int i = 0; i <= BOOK_MOTION_PANEL; i++) {
        if (!names[i]) continue;
        snprintf(n, sizeof n, "%s.agf", names[i]);
        b->art[i] = sa_load_image(&s->vfs, n);
    }
    if (page == PAGE_UNITS)
        for (int i = 0; i < 12; i++) {
            snprintf(n, sizeof n, "exu_c%02d.agf", i);
            b->art[BOOK_MOTION + i] = sa_load_image(&s->vfs, n);
        }
    if (page == PAGE_ITEMS) {
        b->art[BOOK_ITEM_CARD] = sa_load_image(&s->vfs, "pit_a00.agf");
        b->art[BOOK_ITEM_STAR] = sa_load_image(&s->vfs, "pit_a01.agf");
    }
    if (page == PAGE_MAZES) {
        for (int i = 0; i < MAP_OBJECTS; i++) {
            snprintf(n, sizeof n, "mmf%04d.agf", mapObjects[i][1]);
            b->mapIcon[i] = scale2(sa_load_image(&s->vfs, n));
        }
        b->mapIcon[MAP_FLOOR_TILE] = scale2(sa_load_image(&s->vfs, "mmf0000.agf"));
        for (int k = 0; k < 2; k++)
            for (int side = 0; side < 4; side++) {
                snprintf(n, sizeof n, "mmw00%d%d.agf", k ? 4 : 0, side);
                b->mapIcon[MAP_WALL_TILE + k * 4 + side] = scale2(sa_load_image(&s->vfs, n));
            }
    }
}

static void book_open(SA *s, int page) {
    BookScreen *b = &s->extra.book;
    if (!b->items.rows) sa_table_load(&b->items, &s->vfs, "item.obj");
    sa_records_sync(s);
    book_load_art(s, page);
    b->sortKey = b->sortDir = 0;
    b->pick = b->pickPos = -1;
    b->shown = false;
    b->motion = 0;
    b->ms = 0;
}

static void book_close(SA *s) { book_free_art(&s->extra.book); }

static int book_hit(SA *s, int x, int y) {
    ExtraScreen *e = &s->extra;
    BookScreen *b = &e->book;
    bool maze = e->page == PAGE_MAZES;
    int bx = maze ? MAZE_BAR_X : BOOK_BAR_X, upY = maze ? MAZE_UP_Y : BOOK_UP_Y, downY = maze ? MAZE_DOWN_Y : BOOK_DOWN_Y;
    int trackY = maze ? MAZE_TRACK_Y : BOOK_TRACK_Y, trackH = maze ? MAZE_TRACK_H : BOOK_TRACK_H;
    if (in(x, y, bx, upY, BOOK_ARROW_W, BOOK_ARROW_H)) return HOVER_UP;
    if (in(x, y, bx, downY, BOOK_ARROW_W, BOOK_ARROW_H)) return HOVER_DOWN;
    if (in(x, y, bx, trackY, BOOK_ARROW_W, trackH)) return HOVER_TRACK_BAR;
    if (!maze)
        for (int k = 0; k < 3; k++)
            if (in(x, y, 580 + k * 62, BOOK_SORT_Y, BOOK_SORT_W, BOOK_SORT_H)) return HOVER_SORT + k;
    if (e->page == PAGE_UNITS && b->shown && b->pick >= 0) {
        if (in(x, y, UNIT_CARD_X + 194, UNIT_CARD_Y - 19, 72, 18)) return HOVER_TOGGLE;
        bool enemy = is_enemy(s, b->pick);
        for (int k = 0; k < MOTIONS; k++) {
            int my = enemy ? MOTION_BACK_Y + k * MOTION_DY : MOTION_FRONT_Y + k * MOTION_DY;
            if (in(x, y, MOTION_BTN_X, my, MOTION_W, MOTION_H)) return HOVER_MOTION + k + (enemy ? MOTIONS : 0);
        }
        if (!enemy && in(x, y, MOTION_BTN_X, MOTION_BACK_Y, MOTION_W, MOTION_H)) return HOVER_MOTION + MOTIONS;
    }
    int rx = maze ? MAZE_ROW_X : BOOK_ROW_X, ry = maze ? MAZE_ROW_Y : e->page == PAGE_ITEMS ? ITEM_ROW_Y : BOOK_ROW_Y, rw = maze ? MAZE_ROW_W : BOOK_ROW_W, rh = maze ? MAZE_ROW_H : BOOK_ROW_H;
    for (int r = 0; r < book_shown(s); r++)
        if (in(x, y, rx, ry + r * rh, rw, rh)) return e->scroll + r < b->count ? HOVER_ROW + r : -1;
    if (in(x, y, BACK_X, BACK_Y, BACK_W, BACK_H)) return HOVER_BACK;
    return -1;
}

static void book_text(u32 *fb, const char *t, int x, int y, int size, int spacing, u32 color) {
    sa_font_set_spacing(spacing);
    sa_text_draw(fb, SA_W, SA_H, t, -1, x, y, size, color, NO_EDGE);
    sa_font_set_spacing(0);
}

static void book_right(u32 *fb, const char *t, int right, int y, int size, u32 color) {
    sa_text_draw(fb, SA_W, SA_H, t, -1, right - sa_text_width(t, -1, size), y, size, color, NO_EDGE);
}

static void book_counter(const Image *sheet, u32 *fb, int seen, int total) {
    char t[16];
    snprintf(t, sizeof t, "%d", seen);
    digits(sheet, fb, t, COUNT_SEEN_RIGHT - (int)strlen(t) * DIGIT_W, COUNT_Y);
    snprintf(t, sizeof t, "%d", total);
    digits(sheet, fb, t, COUNT_TOTAL_X, COUNT_Y);
    snprintf(t, sizeof t, "%d", total ? seen * 100 / total : 0);
    digits(sheet, fb, t, COUNT_PERCENT_RIGHT - (int)strlen(t) * DIGIT_W, COUNT_Y);
}

static void book_bar(SA *s, u32 *fb, int x, int upY, int trackY, int trackH, int downY) {
    ExtraScreen *e = &s->extra;
    BookScreen *b = &e->book;
    sa_blit_rect(fb, b->art[BOOK_UP], 0, e->hover == HOVER_UP ? BOOK_ARROW_H : 0, BOOK_ARROW_W, BOOK_ARROW_H, x, upY);
    sa_blit_rect(fb, b->art[BOOK_DOWN], 0, e->hover == HOVER_DOWN ? BOOK_ARROW_H : 0, BOOK_ARROW_W, BOOK_ARROW_H, x, downY);
    sa_blit_rect(fb, b->art[BOOK_TRACK], 0, 0, BOOK_ARROW_W, trackH, x, trackY);
    const Image *t = b->art[BOOK_THUMB];
    int shown = book_shown(s), n = b->count;
    if (!t || n <= 0) return;
    int len = trackH * shown / (n > shown ? n : shown), max = scroll_max(s);
    if (len < 12) len = 12;
    int at = trackY + (max > 0 ? (trackH - len) * e->scroll / max : 0);
    sa_blit_rect(fb, t, 0, 0, t->w, len - 6, x, at);
    sa_blit_rect(fb, t, 0, t->h - 6, t->w, 6, x, at + len - 6);
}

static int unit_sp(const SA *s, int row, int level) {
    int sp = sa_table_int(&s->party.units, row, 37);
    return sa_table_int(&s->party.units, row, 0) < 1000 ? sp + 3 * (level - 1) : sp;
}

static void card_value(const Card *c, u32 *fb, int v, int right, int y) {
    char t[16];
    snprintf(t, sizeof t, "%d", v);
    sa_draw_number(fb, c->digits, t, right, y, 0, 10, 13);
}

static void card_pair(const Card *c, u32 *fb, int a, int b, int right, int y) {
    char t[24];
    snprintf(t, sizeof t, "%d/%3d", a, b);
    for (char *p = t; *p; p++)
        if (*p == ' ') *p = '?';
    sa_draw_number(fb, c->digits, t, right, y, 0, 10, 13);
}

static int card_icon(const char *attribute) {
    static const char *const order[] = {"斬", "打", "射", "無", "光", "闇", "火", "水", "土", "風", "雷", "回", "異"};
    for (size_t i = 0; i < sizeof order / sizeof order[0]; i++)
        if (!strncmp(attribute, order[i], strlen(order[i]))) return (int)i;
    return 3;
}

static void card_wrap(u32 *fb, const char *d, int x, int y, int width, int size, int pitch, int lines) {
    int line = 0;
    while (*d && line < lines) {
        int n = 0, w = 0;
        const char *q = d;
        while (*q && *q != '|') {
            u32 cp;
            int len = sa_utf8_decode(q, &cp);
            int cw = sa_char_width(cp, size);
            if (n && w + cw > width) break;
            w += cw;
            q += len;
            n++;
        }
        sa_text_draw(fb, SA_W, SA_H, d, n, x, y + line * pitch, size, 0x000000, NO_EDGE);
        line++;
        d = *q == '|' ? q + 1 : q;
    }
}

static const char *ability_text(const SA *s, const char *name) {
    const Table *a = &s->party.abilities;
    for (int r = 0; r < a->rows; r++)
        if (!strcmp(sa_table_text(a, r, 2), name)) return sa_table_text(a, r, 6);
    return "";
}

static Image *motion_sprite(SA *s, int row, int motion) {
    BookScreen *b = &s->extra.book;
    int key = row * 16 + motion + 1;
    if (b->spriteUnit != key) {
        sa_image_free(b->sprite);
        char n[24];
        snprintf(n, sizeof n, "u%04d%c%d.agf", sa_table_int(&s->party.units, row, 11), motionLetter[motion % MOTIONS], motion >= MOTIONS ? 2 : 0);
        SpriteSheet sh;
        Image *sheet = sa_sheet_load(&s->vfs, n, &sh);
        b->frames = 0;
        b->sprite = sheet && sh.frames > 0 ? sa_sheet_strip(sheet, &sh, 100, 1, &b->frames) : NULL;
        sa_image_free(sheet);
        b->spriteUnit = key;
    }
    return b->sprite;
}

static void draw_unit_card(SA *s, u32 *fb, int row, int motion) {
    const Card *c = &s->card;
    BookScreen *b = &s->extra.book;
    const Table *u = &s->party.units;
    int x = UNIT_CARD_X, y = UNIT_CARD_Y;
    sa_blit_rect(fb, c->toggle, c->showDetail ? 0 : 72, s->extra.hover == HOVER_TOGGLE ? 18 : 0, 72, 18, x + 194, y - 19);
    sa_blit(fb, c->showDetail ? c->detail : c->frame, x, y);
    sa_blit(fb, c->desc, x, y + 500);
    if (row < 0) return;
    const UnitRecord *rec = unit_record(s, row);
    bool enemy = is_enemy(s, row);
    int level = enemy ? sa_table_int(u, row, 17) : rec && rec->level > 0 ? rec->level : 1;
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    if (c->showDetail) {
        sa_text_draw(fb, SA_W, SA_H, sa_table_text(u, row, 2), -1, x + 28, y + 10, 16, 0x000000, NO_EDGE);
        card_value(c, fb, level, x + 260, y + 11);
        for (int k = 0; k < 3; k++) {
            const char *name = sa_table_text(u, row, 34 + k);
            int sy = y + 37 + k * 140;
            sa_text_draw(fb, SA_W, SA_H, name, -1, x + 81, sy, 15, 0x000000, NO_EDGE);
            card_wrap(fb, ability_text(s, name), x + 12, sy + 24, 248, 14, 16, 6);
        }
        book_right(fb, "なし", x + 253, y + 477, 15, 0x000000);
    } else {
        int v[10];
        sa_party_unit_stats(&s->party, row, level, v);
        sa_text_draw(fb, SA_W, SA_H, sa_table_text(u, row, 2), -1, x + 28, y + 8, 16, 0x000000, NO_EDGE);
        sa_text_draw(fb, SA_W, SA_H, sa_table_text(u, row, 4), -1, x + 27, y + 31, 15, 0x000000, NO_EDGE);
        card_value(c, fb, level, x + 260, y + 9);
        card_value(c, fb, s->battle.lvup.rows ? sa_battle_exp_needed(&s->battle, row, level) : 0, x + 262, y + 31);
        const Image *attr = c->icon[card_icon(sa_table_text(u, row, 14))];
        if (attr) sa_blit(fb, attr, x + 242, y + 208);
        card_value(c, fb, sa_table_int(u, row, 15), x + 262, y + 72);
        card_pair(c, fb, v[0], v[0], x + 262, y + 92);
        card_pair(c, fb, v[1], v[1], x + 262, y + 112);
        card_value(c, fb, unit_sp(s, row, level), x + 262, y + 132);
        card_value(c, fb, v[2 + STAT_SPEED], x + 232, y + 152);
        card_value(c, fb, v[2 + STAT_EVASION], x + 232, y + 172);
        card_value(c, fb, v[2 + STAT_GOVERN], x + 102, y + 192);
        card_value(c, fb, v[2 + STAT_COMMAND], x + 232, y + 192);
        card_value(c, fb, v[2 + STAT_ATTACK], x + 102, y + 232);
        card_value(c, fb, v[2 + STAT_MAGIC], x + 232, y + 232);
        card_value(c, fb, v[2 + STAT_DEFENCE], x + 102, y + 252);
        card_value(c, fb, v[2 + STAT_RESIST], x + 232, y + 252);
        static const char fixedPrefix[] = "固定／";
        bool fixed = !strncmp(sa_table_text(u, row, 41), fixedPrefix, strlen(fixedPrefix));
        int known = 0;
        for (int k = 0; k < 8 && known < 6 && (!fixed || known < 1); k++) {
            const char *name = sa_table_text(u, row, 41 + k * 3);
            if (!*name || !strcmp(name, "*")) continue;
            int sr = -1;
            for (int i = 0; i < s->battle.skills.rows && sr < 0; i++)
                if (!strcmp(sa_table_text(&s->battle.skills, i, 1), name)) sr = i;
            int ry = y + 276 + known * 20;
            if (sr >= 0) {
                const Image *ic = c->icon[card_icon(sa_table_text(&s->battle.skills, sr, 14))];
                if (ic) sa_blit(fb, ic, x + 61, ry - 3);
                card_value(c, fb, sa_table_int(&s->battle.skills, sr, 13), x + 256, ry + 1);
            }
            bool isFixed = !strncmp(name, fixedPrefix, strlen(fixedPrefix));
            if (isFixed) sa_text_draw(fb, SA_W, SA_H, "★", -1, x + 48, ry, 15, 0x000000, NO_EDGE);
            sa_text_draw(fb, SA_W, SA_H, isFixed ? name + strlen(fixedPrefix) : name, -1, x + 91, ry, 15, 0x000000, NO_EDGE);
            known++;
        }
        char slots[16];
        snprintf(slots, sizeof slots, "%d/%d", known, sa_table_int(u, row, 38) + (fixed ? 1 : 0));
        sa_draw_number(fb, c->digits, slots, x + 8 + (int)strlen(slots) * 10, y + 395, 0, 10, 13);
        for (int k = 0; k < 3; k++) book_right(fb, sa_table_text(u, row, 34 + k), x + 253, y + 417 + k * 20, 15, 0x000000);
        book_right(fb, "なし", x + 253, y + 477, 15, 0x000000);
        const Image *im = motion_sprite(s, row, motion);
        if (im && b->frames > 0) {
            int i = (int)(b->ms / MOTION_MS % (u32)b->frames), cw = im->w / b->frames;
            sa_blit_rect(fb, im, i * cw, 0, cw, im->h, x + SPRITE_ANCHOR_X - cw / 2, y + SPRITE_ANCHOR_Y - im->h / 2);
        }
    }
    card_wrap(fb, sa_table_text(u, row, 76), x + 8, y + 524, 250, 16, 17, 8);
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
}

static void draw_motions(SA *s, u32 *fb, int row) {
    BookScreen *b = &s->extra.book;
    sa_blit(fb, b->art[BOOK_MOTION_PANEL], MOTION_X, MOTION_Y);
    bool enemy = is_enemy(s, row);
    for (int k = 0; k < MOTIONS * 2; k++) {
        if (enemy ? k < MOTIONS : k > MOTIONS) continue;
        int y = k < MOTIONS ? MOTION_FRONT_Y + k * MOTION_DY : MOTION_BACK_Y + (k - MOTIONS) * MOTION_DY;
        int frame = s->extra.hover == HOVER_MOTION + k ? 1 : 0;
        sa_blit_rect(fb, b->art[BOOK_MOTION + k], 0, frame * MOTION_H, MOTION_W, MOTION_H, MOTION_BTN_X, y);
    }
}

static void draw_units(SA *s, u32 *fb) {
    ExtraScreen *e = &s->extra;
    BookScreen *b = &e->book;
    const Table *u = &s->party.units;
    sa_blit(fb, b->art[BOOK_BG], 0, 0);
    int seen = 0;
    for (int i = 0; i < b->count; i++) seen += book_known(s, i);
    for (int k = 0; k < 3; k++)
        sa_blit_rect(fb, b->art[BOOK_SORT0 + k], b->sortKey == k ? b->sortDir * BOOK_SORT_W : 0, e->hover == HOVER_SORT + k ? BOOK_SORT_H : 0,
                     BOOK_SORT_W, BOOK_SORT_H, 580 + k * 62, BOOK_SORT_Y);
    sa_font_set_plain(true);
    for (int r = 0; r < BOOK_SHOWN && e->scroll + r < b->count; r++) {
        int row = b->order[e->scroll + r], y = BOOK_ROW_Y + r * BOOK_ROW_H;
        int frame = b->pickPos == e->scroll + r && unit_record(s, row) ? 2 : e->hover == HOVER_ROW + r ? 1 : 0;
        sa_blit_rect(fb, b->art[BOOK_ROW], 0, frame * BOOK_ROW_H, BOOK_ROW_W, BOOK_ROW_H, BOOK_ROW_X, y);
        const UnitRecord *rec = unit_record(s, row);
        if (!rec) {
            book_text(fb, "?????", BOOK_NAME_X + 4, y + BOOK_TEXT_DY, BOOK_TEXT, 10, 0x202020);
            sa_font_set_spacing(3);
            book_right(fb, "????", UNIT_RACE_RIGHT, y + UNIT_RACE_DY, UNIT_RACE, 0x202020);
            sa_font_set_spacing(0);
            continue;
        }
        bool enemy = is_enemy(s, row);
        u32 color = enemy ? 0xff0000 : 0x0000ff;
        book_text(fb, sa_table_text(u, row, 2), BOOK_NAME_X, y + BOOK_TEXT_DY, BOOK_TEXT, BOOK_NAME_SPACING, color);
        sa_font_set_spacing(3);
        book_right(fb, sa_table_text(u, row, 4), UNIT_RACE_RIGHT, y + UNIT_RACE_DY, UNIT_RACE, color);
        sa_font_set_spacing(0);
        char t[16];
        if (enemy) {
            book_text(fb, "---", 612, y + BOOK_TEXT_DY, BOOK_TEXT, 0, 0x808080);
            snprintf(t, sizeof t, "%d", rec->kills);
            book_right(fb, t, UNIT_KILLS_RIGHT, y + BOOK_TEXT_DY, BOOK_TEXT, 0x000000);
        } else {
            snprintf(t, sizeof t, "%d", rec->level > 0 ? rec->level : 1);
            book_right(fb, t, UNIT_LV_RIGHT, y + BOOK_TEXT_DY, BOOK_TEXT, 0x000000);
            book_text(fb, "------", UNIT_NONE_X, y + BOOK_TEXT_DY, BOOK_TEXT, 0, 0x808080);
        }
    }
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    book_bar(s, fb, BOOK_BAR_X, BOOK_UP_Y, BOOK_TRACK_Y, BOOK_TRACK_H, BOOK_DOWN_Y);
    book_counter(b->art[BOOK_DIGITS], fb, seen, b->count);
    int hovered = e->hover >= HOVER_ROW && e->hover < HOVER_ROW + BOOK_SHOWN && e->scroll + e->hover - HOVER_ROW < b->count ? b->order[e->scroll + e->hover - HOVER_ROW] : -1;
    if (hovered >= 0 && unit_record(s, hovered) && hovered != b->pick) draw_unit_card(s, fb, hovered, MOTIONS);
    else draw_unit_card(s, fb, b->shown ? b->pick : -1, b->motion);
    if (b->shown && b->pick >= 0) draw_motions(s, fb, b->pick);
    sa_blit_rect(fb, b->art[BOOK_BACK], 0, e->hover == HOVER_BACK ? BACK_H : 0, BACK_W, BACK_H, BACK_X, BACK_Y);
}

static void draw_item_card(SA *s, u32 *fb, int row) {
    BookScreen *b = &s->extra.book;
    const Table *t = &b->items;
    int x = ITEM_CARD_X - 968, y = ITEM_CARD_Y - 86;
    sa_blit(fb, b->art[BOOK_ITEM_CARD], ITEM_CARD_X, ITEM_CARD_Y);
    if (row < 0) return;
    sa_font_set_plain(true);
    sa_blit_rect(fb, b->art[BOOK_ITEM_STAR], 0, clampi(sa_table_int(t, row, 34), 0, 3) * 32, 32, 32, ITEM_CARD_X + 14, ITEM_CARD_Y + 36);
    sa_font_set_spacing(8);
    sa_text_draw(fb, SA_W, SA_H, sa_table_text(t, row, 1), -1, ITEM_CARD_X + 58, y + 131, 14, 0x000000, NO_EDGE);
    sa_font_set_spacing(0);
    Image *ic = item_icon(s, row, true);
    if (ic) sa_blit(fb, ic, x + 977, y + 159);
    char v[16];
    snprintf(v, sizeof v, "%d", item_price(s, row));
    book_right(fb, item_on_sale(s, row) ? v : "--", x + 1268, y + 199, 14, 0x000000);
    snprintf(v, sizeof v, "%d", item_price(s, row) / 4);
    book_right(fb, v, x + 1268, y + 219, 14, 0x000000);
    card_wrap(fb, sa_table_text(t, row, 32), x + 978, y + 267, 290, 12, 16, 5);
    card_wrap(fb, sa_table_text(t, row, 33), x + 978, y + 372, 290, 12, 16, 5);
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
}

static void draw_items(SA *s, u32 *fb) {
    ExtraScreen *e = &s->extra;
    BookScreen *b = &e->book;
    const Table *t = &b->items;
    sa_blit(fb, b->art[BOOK_BG], 0, 0);
    int seen = 0, shown = -1;
    for (int i = 0; i < b->count; i++) seen += book_known(s, i);
    for (int k = 0; k < 3; k++)
        sa_blit_rect(fb, b->art[BOOK_SORT0 + k], b->sortKey == k ? b->sortDir * BOOK_SORT_W : 0, e->hover == HOVER_SORT + k ? BOOK_SORT_H : 0,
                     BOOK_SORT_W, BOOK_SORT_H, 580 + k * 62, BOOK_SORT_Y);
    for (int r = 0; r < BOOK_SHOWN && e->scroll + r < b->count; r++) {
        int row = b->order[e->scroll + r], y = ITEM_ROW_Y + r * BOOK_ROW_H;
        bool known = book_known(s, row), hover = e->hover == HOVER_ROW + r;
        if (known && hover) shown = row;
        int frame = !known ? 3 : hover ? 1 : 0;
        sa_blit_rect(fb, b->art[BOOK_ROW], 0, frame * BOOK_ROW_H, BOOK_ROW_W, BOOK_ROW_H, BOOK_ROW_X, y);
        if (!known) {
            book_text(fb, "?????", ITEM_NAME_X + 2, y + BOOK_TEXT_DY, BOOK_TEXT, 10, 0x000000);
            continue;
        }
        Image *ic = item_icon(s, row, false);
        if (ic) sa_blit(fb, ic, ITEM_ICON_X, y + ITEM_ICON_DY);
        book_text(fb, sa_table_text(t, row, 1), ITEM_NAME_X, y + BOOK_TEXT_DY, BOOK_TEXT, BOOK_NAME_SPACING, 0x000000);
        char v[16];
        if (item_on_sale(s, row)) {
            snprintf(v, sizeof v, "%d", item_price(s, row));
            book_right(fb, v, ITEM_BUY_RIGHT, y + BOOK_TEXT_DY, BOOK_TEXT, 0x000000);
        } else
            book_right(fb, "--------", ITEM_BUY_RIGHT, y + BOOK_TEXT_DY, BOOK_TEXT, 0x808080);
        snprintf(v, sizeof v, "%d", item_price(s, row) / 4);
        book_right(fb, v, ITEM_SELL_RIGHT, y + BOOK_TEXT_DY, BOOK_TEXT, 0x000000);
    }
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    book_bar(s, fb, BOOK_BAR_X, BOOK_UP_Y, BOOK_TRACK_Y, BOOK_TRACK_H, BOOK_DOWN_Y);
    book_counter(b->art[BOOK_DIGITS], fb, seen, b->count);
    draw_item_card(s, fb, shown);
    sa_blit_rect(fb, b->art[BOOK_BACK], 0, e->hover == HOVER_BACK ? BACK_H : 0, BACK_W, BACK_H, BACK_X, BACK_Y);
}

static int map_icon(int object) {
    for (int i = 0; i < MAP_OBJECTS; i++)
        if (mapObjects[i][0] == object) return i;
    return -1;
}

static void draw_full_map(SA *s, u32 *fb, int row) {
    BookScreen *b = &s->extra.book;
    size_t n;
    u8 *m = sa_vfs_read(&s->vfs, sa_table_text(&s->exp.floors, row, 5), &n);
    int w = m && n >= 48 ? (int)rd32(m + 4) : 0, h = m && n >= 48 ? (int)rd32(m + 8) : 0;
    if (w <= 0 || h <= 0 || w > DUN_SIDE || h > DUN_SIDE || n < 48 + (size_t)(w * h) * 104) {
        free(m);
        return;
    }
    int cs = MAP_CELL;
    while (cs > 8 && (w * cs > MAZE_PANEL_W - 40 || h * cs > MAZE_PANEL_H - 40)) cs -= 2;
    int ox = MAZE_PANEL_X + (MAZE_PANEL_W - w * cs) / 2 + MAP_DX, oy = MAZE_PANEL_Y + (MAZE_PANEL_H - h * cs) / 2 + MAP_DY;
    sa_fill(fb, ox, oy, w * cs, h * cs, 0xff000000u);
    static const int wallField[4] = {9, 12, 15, 18};
    for (int pass = 0; pass < 2; pass++)
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                const u8 *c = m + 48 + (size_t)(y * w + x) * 104;
                bool floor = (int)rd32(c + 20) >= 0;
                int px = ox + x * cs, py = oy + y * cs;
                if (pass == 0) {
                    const Image *tile = b->mapIcon[MAP_FLOOR_TILE];
                    if (tile && floor) sa_blit_rect(fb, tile, 0, 0, cs, cs, px, py);
                    for (int side = 0; side < 4; side++) {
                        int wall = (int)rd32(c + (size_t)wallField[side] * 4);
                        int nx = x + (side == 1) - (side == 3), ny = y + (side == 2) - (side == 0);
                        bool beside = nx >= 0 && ny >= 0 && nx < w && ny < h && (int)rd32(m + 48 + (size_t)(ny * w + nx) * 104 + 20) >= 0;
                        if (wall < 0 || wall == 10 || (!floor && !beside)) continue;
                        const Image *im = b->mapIcon[MAP_WALL_TILE + (wall == 20 ? 4 : 0) + side];
                        if (im) sa_blit_rect(fb, im, 0, 0, cs, cs, px, py);
                    }
                    continue;
                }
                if (!floor) continue;
                int icon = map_icon((int)rd32(c + 24));
                if (icon >= 0 && b->mapIcon[icon]) sa_blit_rect(fb, b->mapIcon[icon], 0, 0, cs, cs, px, py);
            }
    free(m);
}

static void draw_mazes(SA *s, u32 *fb) {
    ExtraScreen *e = &s->extra;
    BookScreen *b = &e->book;
    const Table *t = &s->exp.floors;
    sa_blit(fb, b->art[BOOK_BG], 0, 0);
    sa_font_set_plain(true);
    int done = 0;
    for (int i = 0; i < b->count; i++) done += floor_pct(s, i) >= 100;
    for (int r = 0; r < MAZE_SHOWN && e->scroll + r < b->count; r++) {
        int row = b->order[e->scroll + r], y = MAZE_ROW_Y + r * MAZE_ROW_H, pct = floor_pct(s, row);
        int frame = pct < 0 ? 3 : b->pickPos == e->scroll + r ? 2 : e->hover == HOVER_ROW + r ? 1 : 0;
        sa_blit_rect(fb, b->art[BOOK_ROW], 0, frame * MAZE_ROW_H, MAZE_ROW_W, MAZE_ROW_H, MAZE_ROW_X, y);
        if (pct < 0) {
            sa_text_draw(fb, SA_W, SA_H, "？？？？", -1, MAZE_LOCK_X, y + MAZE_LOCK_DY, MAZE_LOCK, 0x000000, NO_EDGE);
            continue;
        }
        const char *dungeon = "";
        for (int d = 0; d < s->exp.dungeons.rows; d++)
            if (sa_table_int(&s->exp.dungeons, d, 0) == sa_table_int(t, row, 0)) dungeon = sa_table_text(&s->exp.dungeons, d, 1);
        char name[160], v[16];
        snprintf(name, sizeof name, "%s　%s", dungeon, sa_table_text(t, row, 4));
        sa_font_set_spacing(0);
        sa_text_draw(fb, SA_W, SA_H, name, -1, MAZE_NAME_X, y + MAZE_NAME_DY, MAZE_NAME, 0x000000, NO_EDGE);
        sa_font_set_spacing(1);
        snprintf(v, sizeof v, "%d%%", pct);
        book_right(fb, v, MAZE_PCT_RIGHT, y + MAZE_PCT_DY, MAZE_PCT, 0x000000);
    }
    sa_font_set_plain(false);
    book_bar(s, fb, MAZE_BAR_X, MAZE_UP_Y, MAZE_TRACK_Y, MAZE_TRACK_H, MAZE_DOWN_Y);
    book_counter(b->art[BOOK_DIGITS], fb, done, b->count);
    sa_blit(fb, b->art[BOOK_MOTION_PANEL], MAZE_PANEL_X, MAZE_PANEL_Y);
    if (b->shown && b->pick >= 0 && floor_pct(s, b->pick) >= 100) draw_full_map(s, fb, b->pick);
    else {
        sa_text_draw(fb, SA_W, SA_H, "踏破率１００%のダンジョンは", -1, MAZE_NOTE_X, MAZE_NOTE_Y, MAZE_NOTE, 0xffffff, NO_EDGE);
        sa_text_draw(fb, SA_W, SA_H, "ここにミニマップが表示されます。", -1, MAZE_NOTE_X, MAZE_NOTE_Y + MAZE_NOTE_DY, MAZE_NOTE, 0xffffff, NO_EDGE);
    }
    sa_blit_rect(fb, b->art[BOOK_BACK], 0, e->hover == HOVER_BACK ? BACK_H : 0, BACK_W, BACK_H, BACK_X, BACK_Y);
}

static void book_click(SA *s, int h) {
    ExtraScreen *e = &s->extra;
    BookScreen *b = &e->book;
    if (h >= HOVER_MOTION && h < HOVER_MOTION + 2 * MOTIONS) {
        b->motion = h - HOVER_MOTION;
        b->ms = 0;
        return;
    }
    if (h == HOVER_TOGGLE) {
        s->card.showDetail = !s->card.showDetail;
        return;
    }
    if (h >= HOVER_SORT && h < HOVER_SORT + 3) {
        int k = h - HOVER_SORT;
        b->sortDir = b->sortKey == k && b->sortDir == 1 ? 2 : 1;
        b->sortKey = k;
        b->shown = false;
        book_sort(s);
        return;
    }
    if (h >= HOVER_ROW && h < HOVER_ROW + BOOK_SHOWN) {
        int row = b->order[e->scroll + h - HOVER_ROW];
        if (e->page == PAGE_ITEMS || !book_known(s, row)) return;
        b->pick = row;
        b->pickPos = e->scroll + h - HOVER_ROW;
        b->shown = true;
        b->motion = MOTIONS;
        b->ms = 0;
    }
}

static int book_scroll_max(const SA *s) {
    int n = s->extra.book.count, shown = book_shown(s);
    return n > shown ? n - shown : 0;
}

static void draw(SA *s) {
    ExtraScreen *e = &s->extra;
    u32 *fb = s->fb;
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u;
    switch (e->page) {
    case PAGE_MENU:
        draw_menu(s, fb);
        break;
    case PAGE_HINT: {
        draw_menu(s, fb);
        sa_dim(fb);
        char text[256];
        snprintf(text, sizeof text, "%s", e->hints.rows ? sa_table_text(&e->hints, e->hint, 2) : "");
        const char *lines[4] = {text};
        int count = 1;
        for (char *p = text; (p = strchr(p, '|')) && count < 4; p++) {
            *p = 0;
            lines[count++] = p + 1;
        }
        sa_popup_draw(s->battle.pop, fb, lines, count);
        break;
    }
    case PAGE_CG:
        draw_cg(s, fb);
        break;
    case PAGE_VIEW:
        sa_blit(fb, e->view, 0, 0);
        break;
    case PAGE_SCENES:
    case PAGE_SCENARIOS:
        draw_list(s, fb);
        break;
    case PAGE_MUSIC:
        draw_music(s, fb);
        break;
    case PAGE_UNITS:
        draw_units(s, fb);
        break;
    case PAGE_ITEMS:
        draw_items(s, fb);
        break;
    case PAGE_MAZES:
        draw_mazes(s, fb);
        break;
    }
}

static const int trackBgm[TRACKS - TRACK_NARROW] = {63, 65, 64};

static void play_track(SA *s, int i) {
    ExtraScreen *e = &s->extra;
    if (i < 0) i = TRACKS - 1;
    if (i >= TRACKS) i = 0;
    int id = i < TRACK_NARROW ? i + 2 : trackBgm[i - TRACK_NARROW];
    for (int k = 0; k < s->bgmCount; k++)
        if (s->bgm[k].id == id) sa_play_loop(s->mix, CH_BGM, 0, s->bgm[k].file, s->bgm[k].loopTo, s->bgm[k].loopFrom);
    e->playing = e->track = i;
}

static void set_volume(SA *s, int delta) {
    int v = clampi(sa_option(s, OPT_BGM) + delta, 0, 15);
    s->opt.value[OPT_BGM] = (u8)v;
    sa_option_apply(s);
    s->vm.gDirty = true;
}

static void scroll_by(SA *s, int d) {
    ExtraScreen *e = &s->extra;
    int v = clampi(e->scroll + d, 0, scroll_max(s));
    if (v != e->scroll) {
        e->scroll = v;
        s->dirty = true;
    }
}

static void scroll_to(SA *s, int y, int top, int h) {
    s->extra.scroll = clampi((y - top) * (scroll_max(s) + 1) / h, 0, scroll_max(s));
}

static void play_script(SA *s, const Table *t, int row) {
    char box[32];
    snprintf(box, sizeof box, "%s", sa_table_text(t, row, 2));
    s->extra.resume = true;
    sa_reset_scene(s);
    s->nextBox[0] = 0;
    s->afterScript = 0;
    if (!sa_start_script(s, box, sa_table_int(t, row, 3))) sa_extra_script_done(s);
}

static void pick_item(SA *s, int item) {
    ExtraScreen *e = &s->extra;
    switch (item) {
    case ITEM_CG:
        show_page(s, PAGE_CG);
        break;
    case ITEM_SCENES:
        show_page(s, PAGE_SCENES);
        break;
    case ITEM_SCENARIOS:
        show_page(s, PAGE_SCENARIOS);
        break;
    case ITEM_MUSIC:
        show_page(s, PAGE_MUSIC);
        break;
    case ITEM_QUIZ:
        e->resume = true;
        sa_reset_scene(s);
        s->nextBox[0] = 0;
        s->afterScript = 0;
        if (!sa_start_script(s, "quiz.box", 0)) sa_extra_script_done(s);
        break;
    case ITEM_UNITS:
        show_page(s, PAGE_UNITS);
        break;
    case ITEM_ITEMS:
        show_page(s, PAGE_ITEMS);
        break;
    case ITEM_MAZES:
        show_page(s, PAGE_MAZES);
        break;
    case ITEM_HINT:
        e->hint = e->hints.rows ? (int)(sa_vm_random(&s->vm) % (u32)e->hints.rows) : 0;
        e->page = PAGE_HINT;
        break;
    }
}

static void click(SA *s, int h) {
    ExtraScreen *e = &s->extra;
    if (e->page == PAGE_HINT) {
        e->page = PAGE_MENU;
        return;
    }
    if (e->page == PAGE_VIEW) {
        sa_image_free(e->view);
        e->view = NULL;
        e->page = PAGE_CG;
        return;
    }
    if (h == HOVER_BACK) {
        if (e->page == PAGE_MENU) sa_go_title(s);
        else show_page(s, PAGE_MENU);
        return;
    }
    if (book_page(e->page) && (h >= HOVER_ROW || h == HOVER_TOGGLE)) {
        book_click(s, h);
        return;
    }
    if (h == HOVER_UP) scroll_by(s, -1);
    else if (h == HOVER_DOWN) scroll_by(s, 1);
    else if (h == HOVER_TRACK_BAR) {
        if (e->page == PAGE_CG) scroll_to(s, s->mouseY, CG_TRACK_Y, CG_TRACK_H);
        else if (e->page == PAGE_MAZES) scroll_to(s, s->mouseY, MAZE_TRACK_Y, MAZE_TRACK_H);
        else if (book_page(e->page)) scroll_to(s, s->mouseY, BOOK_TRACK_Y, BOOK_TRACK_H);
        else scroll_to(s, s->mouseY, LIST_TRACK_Y, LIST_TRACK_H);
    } else if (h >= HOVER_SONG) play_track(s, h - HOVER_SONG);
    else if (h >= HOVER_ROW) {
        int i = e->scroll + h - HOVER_ROW;
        if (!row_open(s, i)) return;
        list_viewed(s)[i] = 1;
        s->vm.gDirty = true;
        play_script(s, list_table(s), i);
    } else if (h >= HOVER_CELL) {
        int i = e->scroll * CG_COLS + h - HOVER_CELL;
        if (!cg_open(s, i)) return;
        e->viewedCg[i] = 1;
        s->vm.gDirty = true;
        sa_image_free(e->view);
        e->view = sa_load_image(&s->vfs, sa_table_text(&e->cg, i, 0));
        e->page = PAGE_VIEW;
    } else if (h >= HOVER_ITEM)
        pick_item(s, h - HOVER_ITEM);
    else if (h == HOVER_PLAY) play_track(s, e->playing >= 0 ? e->playing : e->track);
    else if (h == HOVER_PREV) play_track(s, e->track - 1);
    else if (h == HOVER_NEXT) play_track(s, e->track + 1);
    else if (h == HOVER_STOP) stop_music(s);
    else if (h == HOVER_PLUS) set_volume(s, 1);
    else if (h == HOVER_MINUS) set_volume(s, -1);
}

void sa_extra_wheel(SA *s, int notches) {
    if (s->mode != MODE_EXTRA) return;
    scroll_by(s, notches > 0 ? -1 : 1);
}

void sa_extra_tick(SA *s, u32 elapsedMs) {
    ExtraScreen *e = &s->extra;
    if (s->mode != MODE_EXTRA || e->page != PAGE_UNITS) return;
    u32 before = e->book.ms / MOTION_MS;
    e->book.ms += elapsedMs;
    if (e->book.ms / MOTION_MS != before) s->dirty = true;
}

bool sa_extra_frame(SA *s) {
    if (s->mode != MODE_EXTRA) return false;
    ExtraScreen *e = &s->extra;
    int h = e->page == PAGE_HINT || e->page == PAGE_VIEW ? -1 : hit(s, s->mouseX, s->mouseY);
    if (h != e->hover) {
        e->hover = h;
        s->dirty = true;
    }
    if (s->rightClicked) {
        s->rightClicked = false;
        if (e->page == PAGE_MENU) sa_go_title(s);
        else click(s, e->page == PAGE_HINT || e->page == PAGE_VIEW ? -1 : HOVER_BACK);
        s->dirty = true;
    }
    if (s->clicked) {
        s->clicked = false;
        click(s, h);
        s->dirty = true;
    }
    if (s->mode == MODE_EXTRA && s->dirty) {
        draw(s);
        s->serial++;
        s->dirty = false;
    }
    return true;
}

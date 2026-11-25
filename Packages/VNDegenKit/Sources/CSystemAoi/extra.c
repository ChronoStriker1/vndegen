#include "sa_private.h"

enum { PAGE_MENU, PAGE_CG, PAGE_VIEW, PAGE_SCENES, PAGE_SCENARIOS, PAGE_MUSIC, PAGE_HINT };
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
    load_samples(s);
}

static void drop_thumbs(ExtraScreen *e) {
    for (int i = 0; i < 30; i++) {
        sa_image_free(e->thumb[i]);
        e->thumb[i] = NULL;
        e->thumbFor[i] = -1;
    }
}

void sa_extra_free(SA *s) {
    ExtraScreen *e = &s->extra;
    for (int i = 0; i < EXTRA_ART; i++) sa_image_free(e->art[i]);
    drop_thumbs(e);
    sa_image_free(e->view);
    sa_table_free(&e->cg);
    sa_table_free(&e->scenes);
    sa_table_free(&e->scenarios);
    sa_table_free(&e->hints);
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

static int scroll_max(const SA *s) {
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
    e->page = page;
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
    case ITEM_HINT:
        e->hint = e->hints.rows ? (int)(sa_vm_random(&s->vm) % (u32)e->hints.rows) : 0;
        e->page = PAGE_HINT;
        break;
    default:
        sa_log(s, "extra item %d not built yet", item);
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
    if (h == HOVER_UP) scroll_by(s, -1);
    else if (h == HOVER_DOWN) scroll_by(s, 1);
    else if (h == HOVER_TRACK_BAR) {
        if (e->page == PAGE_CG) scroll_to(s, s->mouseY, CG_TRACK_Y, CG_TRACK_H);
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

#include "anim.h"

enum { EX_CG, EX_ANIME, EX_SCENES, EX_SPRITES, EX_MUSIC, EX_TABS };
enum { GRID_COLS = 4, CG_ROWS = 3, MEMORY_ROWS = 2, THUMB_X = 97, THUMB_Y = 158, THUMB_DX = 277, THUMB_DY = 165, THUMB_W = 256, THUMB_H = 144 };
enum { TAB_X = 262, TAB_Y = 5, TAB_DX = 194, TAB_W = 182, TAB_H = 40 };
enum { PAGE_X = 90, PAGE_Y = 61, PAGE_DX = 82, PAGE_W = 78, PAGE_H = 38 };
enum { BACK_X = 62, BACK_Y = 670, BACK_W = 278, BACK_H = 40 };
enum { BIG_Y = 412, BIG_H = 50, SMALL_H = 25, GROUP_PREFIX = 5 };

typedef struct Glyph {
    int x, w;
} Glyph;

typedef struct ExtraLayout {
    int frameX, frameY, frameW, frameH, frameDX, frameDY, hoverX, hoverY, hoverW, hoverH, hoverDX, hoverDY;
    int smallY, countRight, countY, percentRight, percentY, memHaveRight, memTotalLeft, memCountY;
    Glyph small[11], big[10];
    int textX, textY, textSize, textPitch, textW;
    u32 textColor, textShadow;
    int musicTabX, musicTabY, musicBackY, stopX, stopY, stopW, stopH, stopScreenX, repeatX, repeatScreenX, buttonsY;
    int trackColumns, trackAX, trackAY, trackADX, trackADY, trackStateDY, trackW, trackH;
    int trackSplit, trackLeftX, trackRightX, trackY, trackDY;
    const char *const *trackFiles;
    int trackCount;
} ExtraLayout;

static const char *const MGSW_TRACKS[] = {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10",
                                          "11", "12", "13", "14", "15", "16", "30", "31", "40", "41"};
static const char *const DDD_TRACKS[] = {"01", "02", "03", "04", "05", "06", "07", "20", "21", "30", "31"};

static const ExtraLayout EXTRA[GAME_COUNT] = {
    [GAME_MGSW] = {3, 495, 270, 158, -8, -7, 283, 498, 264, 152, -4, -4,
                   468, 348, 277, 2260, 75, 2125, 2259, 75,
                   {{4, 12}, {22, 12}, {40, 12}, {58, 12}, {75, 13}, {93, 13}, {111, 13}, {129, 14}, {148, 12}, {165, 14}, {183, 14}},
                   {{42, 26}, {80, 25}, {115, 26}, {150, 28}, {185, 28}, {223, 27}, {257, 28}, {295, 26}, {330, 28}, {366, 28}},
                   122, 553, 23, 35, 1020, 0xffffffffu, 0,
                   3, 86, 3, 3, 215, 234, 48, 756, 247, 1018, 72,
                   3, 5, 360, 538, 138, 46, 518, 38,
                   11, 94, 656, 136, 46, MGSW_TRACKS, 20},
    [GAME_DDD] = {1, 492, 276, 164, -10, -10, 277, 492, 276, 164, -10, -10,
                  466, 348, 275, 2259, 75, 2124, 2259, 75,
                  {{3, 14}, {21, 14}, {41, 10}, {57, 14}, {75, 14}, {93, 15}, {111, 14}, {129, 14}, {147, 14}, {165, 14}, {183, 14}},
                  {{40, 30}, {82, 18}, {112, 30}, {147, 32}, {183, 32}, {221, 29}, {256, 30}, {292, 31}, {328, 31}, {364, 30}},
                  123, 553, 27, 35, 990, 0xff3a3a3au, 0,
                  3, 87, 3, 3, 213, 236, 40, 756, 247, 1018, 70,
                  4, 13, 360, 514, 258, 86, 490, 76,
                  6, 101, 681, 136, 85, DDD_TRACKS, 11},
};

static bool in_rect(AN *a, int x, int y, int w, int h) { return a->mx >= x && a->my >= y && a->mx < x + w && a->my < y + h; }

static int group_count(AN *a) {
    int n = 0;
    for (int i = 0; i < a->def.cgCount; i++)
        if (!i || strncmp(a->def.cg[i], a->def.cg[i - 1], GROUP_PREFIX)) n++;
    return n;
}

static int group_start(AN *a, int group, int *size) {
    int n = -1, start = 0;
    for (int i = 0; i < a->def.cgCount; i++) {
        if (!i || strncmp(a->def.cg[i], a->def.cg[i - 1], GROUP_PREFIX)) {
            if (n == group) break;
            n++;
            start = i;
        }
    }
    int end = start;
    while (end < a->def.cgCount && !strncmp(a->def.cg[end], a->def.cg[start], GROUP_PREFIX)) end++;
    *size = end - start;
    return start;
}

static int unlocked_in(AN *a, int start, int size) {
    int n = 0;
    for (int i = start; i < start + size; i++) n += an_cg_unlocked(a, i);
    return n;
}

static int per_page(int tab) { return GRID_COLS * (tab == EX_SCENES ? MEMORY_ROWS : CG_ROWS); }

static int page_count(AN *a, int tab) {
    int items = tab == EX_SCENES ? a->def.memoryCount : group_count(a);
    return (items + per_page(tab) - 1) / per_page(tab);
}

static const char *atlas_name(int tab) { return tab == EX_SCENES ? "pMemoryButton" : tab == EX_MUSIC ? "pMusicButton" : "pCgButton"; }
static const char *screen_name(int tab) { return tab == EX_SCENES ? "pMemory" : tab == EX_MUSIC ? "pMusic" : "pCg"; }

static Image *thumbs(AN *a, int tab, int page) {
    char name[32];
    snprintf(name, sizeof name, tab == EX_SCENES ? "p_metn%02d" : "p_cgtn%02d", page + 1);
    return an_image_load(a, name);
}

enum { SMALL_GAP = 4 };

static int digits_of(int value, int *digits) {
    char s[16];
    int n = snprintf(s, sizeof s, "%d", value);
    for (int i = 0; i < n; i++) digits[i] = s[i] - '0';
    return n;
}

enum { BIG_CELL = 69 };

static void draw_big_cells(AN *a, Image *im, const ExtraLayout *L, int value, int lastCentre2, int y) {
    int digits[12], n = digits_of(value, digits);
    for (int i = 0; i < n; i++) {
        const Glyph *g = &L->big[digits[i]];
        int centre2 = lastCentre2 - BIG_CELL * (n - 1 - i);
        an_gfx_blit(a->fb, a->W, a->H, im, g->x, BIG_Y, g->w, BIG_H, (centre2 - g->w + 1) / 2, y, 255);
    }
}

static void draw_count(AN *a, Image *im, const ExtraLayout *L, int have, int total, int right, int y) {
    int all[24], n = 0, d[12];
    int nh = digits_of(have, d);
    for (int i = 0; i < nh; i++) all[n++] = d[i] + 1;
    all[n++] = 0;
    int nt = digits_of(total, d);
    for (int i = 0; i < nt; i++) all[n++] = d[i] + 1;
    int w = 0;
    for (int i = 0; i < n; i++) w += L->small[all[i]].w + (i ? SMALL_GAP : 0);
    int x = right - w;
    for (int i = 0; i < n; i++) {
        an_gfx_blit(a->fb, a->W, a->H, im, L->small[all[i]].x, L->smallY, L->small[all[i]].w, SMALL_H, x, y, 255);
        x += L->small[all[i]].w + SMALL_GAP;
    }
}

void an_extra_open(AN *a) {
    a->screen = SCR_EXTRA;
    a->extraTab = EX_CG;
    a->extraPage = 0;
    a->extraViewer = -1;
    a->extraTrack = -1;
    snprintf(a->extraBgm, sizeof a->extraBgm, "%s", a->bgm);
}

static void draw_tabs(AN *a, Image *b, int tab) {
    const ExtraLayout *L = &EXTRA[a->game];
    for (int k = 0; k < EX_TABS; k++) {
        bool live = k != EX_ANIME && k != EX_SPRITES;
        int state = k == a->extraTab ? 2 : live && in_rect(a, TAB_X + TAB_DX * k, TAB_Y, TAB_W, TAB_H) ? 1 : 0;
        int ax = tab == EX_MUSIC ? L->musicTabX + 186 * k : 3 + 186 * k, ay = tab == EX_MUSIC ? L->musicTabY + 42 * state : 284 + 42 * state;
        an_gfx_blit(a->fb, a->W, a->H, b, ax, ay, TAB_W, TAB_H, TAB_X + TAB_DX * k, TAB_Y + (tab == EX_MUSIC && a->game == GAME_DDD), 255);
    }
    int by = tab == EX_MUSIC ? L->musicBackY : 200;
    an_gfx_blit(a->fb, a->W, a->H, b, 3, by + (in_rect(a, BACK_X, BACK_Y, BACK_W, BACK_H) ? 42 : 0), BACK_W, BACK_H, BACK_X,
                BACK_Y + (tab == EX_MUSIC && a->game == GAME_DDD ? 1 : 0), 255);
}

static void draw_pages(AN *a, Image *b) {
    int pages = page_count(a, a->extraTab);
    for (int i = 0; i < pages; i++) {
        int x = PAGE_X + PAGE_DX * i;
        int state = i == a->extraPage ? 2 : in_rect(a, x, PAGE_Y, PAGE_W, PAGE_H) ? 1 : 0;
        an_gfx_blit(a->fb, a->W, a->H, b, 3 + 82 * i, 3 + 42 * state, PAGE_W, PAGE_H, x, PAGE_Y, 255);
    }
}

static void cell_xy(int k, int *x, int *y) {
    *x = THUMB_X + THUMB_DX * (k % GRID_COLS);
    *y = THUMB_Y + THUMB_DY * (k / GRID_COLS);
}

static int cell_hit(AN *a, int count) {
    for (int k = 0; k < count; k++) {
        int x, y;
        cell_xy(k, &x, &y);
        if (in_rect(a, x, y, THUMB_W, THUMB_H)) return k;
    }
    return -1;
}

static void draw_cell(AN *a, Image *b, Image *t, int k, bool open, bool hover) {
    const ExtraLayout *L = &EXTRA[a->game];
    int x, y;
    cell_xy(k, &x, &y);
    an_gfx_blit(a->fb, a->W, a->H, b, L->frameX, L->frameY, L->frameW, L->frameH, x + L->frameDX, y + L->frameDY, 255);
    if (!open) return;
    int r = k / GRID_COLS, c = k % GRID_COLS;
    if (hover) an_gfx_blit(a->fb, a->W, a->H, b, L->hoverX, L->hoverY, L->hoverW, L->hoverH, x + L->hoverDX, y + L->hoverDY, 255);
    if (t) an_gfx_blit(a->fb, a->W, a->H, t, 2 + 258 * c, (hover ? 148 : 2) + 292 * r, THUMB_W, THUMB_H, x, y, 255);
}

static void draw_cg(AN *a, Image *b) {
    const ExtraLayout *L = &EXTRA[a->game];
    Image *t = thumbs(a, EX_CG, a->extraPage);
    int groups = group_count(a), first = a->extraPage * per_page(EX_CG);
    int shown = groups - first < per_page(EX_CG) ? groups - first : per_page(EX_CG);
    int hover = cell_hit(a, shown);
    for (int k = 0; k < shown; k++) {
        int size, start = group_start(a, first + k, &size), have = unlocked_in(a, start, size);
        draw_cell(a, b, t, k, have > 0, have > 0 && k == hover);
        int x, y;
        cell_xy(k, &x, &y);
        draw_count(a, b, L, have, size, x + L->countRight - THUMB_X, y + L->countY - THUMB_Y);
    }
    int have = unlocked_in(a, 0, a->def.cgCount);
    draw_big_cells(a, b, L, a->def.cgCount ? have * 100 / a->def.cgCount : 0, L->percentRight, L->percentY);
}

static void wrap_chars(AN *a, const char *in, int size, int width, char *out, size_t cap) {
    size_t o = 0;
    int line = 0;
    while (*in && o + 8 < cap) {
        if (in[0] == '@' && in[1] == 'n') {
            out[o++] = *in++;
            out[o++] = *in++;
            line = 0;
            continue;
        }
        u32 cp;
        int len = an_utf8_decode(in, &cp);
        char one[8];
        snprintf(one, sizeof one, "%.*s", len, in);
        int w = an_text_width(a, one, size);
        if (line && line + w > width) {
            out[o++] = '@';
            out[o++] = 'n';
            line = 0;
        }
        memcpy(out + o, in, (size_t)len);
        o += (size_t)len;
        in += len;
        line += w;
    }
    out[o] = 0;
}

static void draw_scenes(AN *a, Image *b) {
    const ExtraLayout *L = &EXTRA[a->game];
    Image *t = thumbs(a, EX_SCENES, a->extraPage);
    int first = a->extraPage * per_page(EX_SCENES);
    int shown = a->def.memoryCount - first < per_page(EX_SCENES) ? a->def.memoryCount - first : per_page(EX_SCENES);
    int hover = cell_hit(a, shown), have = 0;
    for (int k = 0; k < shown; k++) {
        bool open = an_memory_unlocked(a, first + k + 1);
        draw_cell(a, b, t, k, open, open && k == hover);
    }
    for (int n = 1; n <= a->def.memoryCount; n++) have += an_memory_unlocked(a, n);
    int nt = 0, total = a->def.memoryCount;
    for (int v = total; v; v /= 10) nt++;
    draw_big_cells(a, b, L, have, L->memHaveRight, L->memCountY);
    draw_big_cells(a, b, L, total, L->memTotalLeft + BIG_CELL * (nt - 1), L->memCountY);
    if (hover >= 0 && an_memory_unlocked(a, first + hover + 1)) {
        char text[2048];
        if (a->game == GAME_DDD) snprintf(text, sizeof text, "%s", a->def.memory[first + hover]);
        else wrap_chars(a, a->def.memory[first + hover], L->textSize, L->textW, text, sizeof text);
        a->fractionalText = a->game == GAME_DDD;
        an_text_draw(a, L->textX, L->textY, text, L->textSize, L->textColor, L->textShadow, a->game == GAME_DDD ? L->textW : 0, L->textPitch);
        a->fractionalText = false;
    }
}

static const int MGSW_TRACK_Y[11] = {136, 182, 228, 274, 320, 366, 410, 456, 502, 548, 594};

static void track_screen(AN *a, const ExtraLayout *L, int n, int *x, int *y) {
    int row = n < L->trackSplit ? n : n - L->trackSplit;
    *x = n < L->trackSplit ? L->trackLeftX : L->trackRightX;
    *y = a->game == GAME_MGSW ? MGSW_TRACK_Y[row] : L->trackY + L->trackDY * row;
}

static void draw_music(AN *a, Image *b) {
    const ExtraLayout *L = &EXTRA[a->game];
    for (int n = 0; n < L->trackCount; n++) {
        int x, y;
        track_screen(a, L, n, &x, &y);
        int state = n == a->extraTrack ? 2 : in_rect(a, x, y, L->trackW, L->trackH) ? 1 : 0;
        int ax = L->trackAX + L->trackADX * (n % L->trackColumns), ay = L->trackAY + L->trackADY * (n / L->trackColumns) + L->trackStateDY * state;
        an_gfx_blit(a->fb, a->W, a->H, b, ax, ay, L->trackW, L->trackH, x, y, 255);
    }
    bool stopHover = in_rect(a, L->stopScreenX, L->buttonsY, L->stopW, L->stopH);
    an_gfx_blit(a->fb, a->W, a->H, b, L->stopX, L->stopY + (stopHover ? L->stopH + 8 : 0), L->stopW, L->stopH, L->stopScreenX, L->buttonsY, 255);
    int rs = a->extraRepeat ? 2 : in_rect(a, L->repeatScreenX, L->buttonsY, L->stopW, L->stopH) ? 1 : 0;
    an_gfx_blit(a->fb, a->W, a->H, b, L->repeatX, L->stopY + (L->stopH + 8) * rs, L->stopW, L->stopH, L->repeatScreenX, L->buttonsY, 255);
}

static void draw_viewer(AN *a) {
    Image *im = an_image_load(a, a->def.cg[a->extraViewer]);
    an_gfx_fill(a->fb, a->W, a->H, 0xff000000u);
    if (im) an_gfx_blit(a->fb, a->W, a->H, im, 0, 0, a->W, a->H, 0, 0, 255);
}

void an_extra_render(AN *a) {
    if (a->screen != SCR_EXTRA) return;
    if (a->extraViewer >= 0) {
        draw_viewer(a);
        return;
    }
    Image *bg = an_image_load(a, screen_name(a->extraTab));
    if (bg) an_gfx_blit(a->fb, a->W, a->H, bg, 0, 0, a->W, a->H, 0, 0, 255);
    Image *b = an_image_load(a, atlas_name(a->extraTab));
    if (!b) return;
    if (a->extraTab != EX_MUSIC) draw_pages(a, b);
    if (a->extraTab == EX_CG) draw_cg(a, b);
    else if (a->extraTab == EX_SCENES) draw_scenes(a, b);
    else draw_music(a, b);
    Image *chrome = a->extraTab == EX_MUSIC ? b : an_image_load(a, "pCgButton");
    if (chrome) draw_tabs(a, chrome, a->extraTab);
}

static void viewer_next(AN *a) {
    for (int i = a->extraViewer + 1; i < a->def.cgCount; i++)
        if (an_cg_unlocked(a, i)) {
            a->extraViewer = i;
            return;
        }
    a->extraViewer = -1;
}

static void start_replay(AN *a, int n) {
    int label = an_script_label(a->sc, "CheckMemory");
    if (label < 0) return;
    memset(&a->scene, 0, sizeof a->scene);
    memset(a->L, 0, sizeof a->L);
    a->L[4] = n;
    a->logCount = 0;
    a->replay = n;
    a->screen = SCR_NONE;
    an_vm_start(a, (u32)label);
}

void an_replay_end(AN *a) {
    a->replay = 0;
    an_reset_scene(a);
    an_system_save(a);
    a->th.wait = W_END;
    a->phase = PH_TITLE;
    a->screen = SCR_EXTRA;
    a->extraTab = EX_SCENES;
    a->extraViewer = -1;
    a->extraTrack = -1;
    for (int c = 0; c < CH_COUNT; c++) an_snd_stop(a, c);
    if (a->extraBgm[0]) an_snd_play(a, CH_BGM, a->extraBgm, true, false);
}

static void play_track(AN *a, int n) {
    const ExtraLayout *L = &EXTRA[a->game];
    a->extraTrack = n;
    an_snd_play(a, CH_BGM, L->trackFiles[n], a->extraRepeat, false);
}

static void leave(AN *a) {
    a->screen = SCR_NONE;
    if (a->extraTrack >= 0 && a->extraBgm[0]) an_snd_play(a, CH_BGM, a->extraBgm, true, false);
    a->extraTrack = -1;
    an_system_save(a);
}

bool an_extra_input(AN *a) {
    if (a->screen != SCR_EXTRA) return false;
    if (a->extraViewer >= 0) {
        if (a->rclicked) a->extraViewer = -1;
        else if (a->clicked) viewer_next(a);
        a->clicked = a->rclicked = false;
        return true;
    }
    const ExtraLayout *L = &EXTRA[a->game];
    if (a->extraTab == EX_MUSIC && a->extraTrack >= 0 && !a->extraRepeat && !an_snd_playing(a, CH_BGM)) {
        int next = a->extraTrack + 1 < L->trackCount ? a->extraTrack + 1 : -1;
        if (next >= 0) play_track(a, next);
        else a->extraTrack = -1;
    }
    if (a->rclicked) {
        a->rclicked = false;
        leave(a);
        return true;
    }
    if (!a->clicked) return true;
    a->clicked = false;
    if (in_rect(a, BACK_X, BACK_Y, BACK_W, BACK_H)) {
        leave(a);
        return true;
    }
    for (int k = 0; k < EX_TABS; k++)
        if (k != EX_ANIME && k != EX_SPRITES && in_rect(a, TAB_X + TAB_DX * k, TAB_Y, TAB_W, TAB_H) && k != a->extraTab) {
            a->extraTab = k;
            a->extraPage = 0;
            return true;
        }
    if (a->extraTab == EX_MUSIC) {
        for (int n = 0; n < L->trackCount; n++) {
            int x, y;
            track_screen(a, L, n, &x, &y);
            if (in_rect(a, x, y, L->trackW, L->trackH)) play_track(a, n);
        }
        if (in_rect(a, L->stopScreenX, L->buttonsY, L->stopW, L->stopH)) {
            an_snd_stop(a, CH_BGM);
            a->extraTrack = -1;
        }
        if (in_rect(a, L->repeatScreenX, L->buttonsY, L->stopW, L->stopH)) {
            a->extraRepeat = !a->extraRepeat;
            if (a->extraTrack >= 0) play_track(a, a->extraTrack);
        }
        return true;
    }
    int pages = page_count(a, a->extraTab);
    for (int i = 0; i < pages; i++)
        if (in_rect(a, PAGE_X + PAGE_DX * i, PAGE_Y, PAGE_W, PAGE_H)) {
            a->extraPage = i;
            return true;
        }
    int first = a->extraPage * per_page(a->extraTab);
    if (a->extraTab == EX_CG) {
        int groups = group_count(a), shown = groups - first < per_page(EX_CG) ? groups - first : per_page(EX_CG);
        int k = cell_hit(a, shown);
        if (k < 0) return true;
        int size, start = group_start(a, first + k, &size);
        for (int i = start; i < start + size; i++)
            if (an_cg_unlocked(a, i)) {
                a->extraViewer = i;
                break;
            }
        return true;
    }
    int shown = a->def.memoryCount - first < per_page(EX_SCENES) ? a->def.memoryCount - first : per_page(EX_SCENES);
    int k = cell_hit(a, shown);
    if (k >= 0 && an_memory_unlocked(a, first + k + 1)) start_replay(a, first + k + 1);
    return true;
}

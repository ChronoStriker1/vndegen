#include "anim.h"

enum { TA_NEW, TA_LOAD, TA_SETTINGS, TA_VOLUME, TA_EXTRA, TA_FLOWCHART, TA_HELP, TA_EXIT };

typedef struct TitleButton {
    int action, ax, ay, sx, sy, hx, hy, hw, hh;
} TitleButton;

typedef struct TitleLayout {
    int count, w, h, hoverDx, hoverDy;
    TitleButton button[8];
} TitleLayout;

static const TitleLayout TITLE[GAME_COUNT] = {
    [GAME_DDD] = {7, 366, 54, 0, 60,
                  {{TA_NEW, 3, 4, 115, 38, 115, 38, 366, 54},
                   {TA_LOAD, 373, 4, 115, 98, 115, 98, 366, 54},
                   {TA_SETTINGS, 3, 124, 115, 158, 115, 158, 366, 54},
                   {TA_VOLUME, 373, 124, 115, 218, 115, 218, 366, 54},
                   {TA_EXTRA, 3, 244, 115, 278, 115, 278, 366, 54},
                   {TA_FLOWCHART, 373, 244, 115, 338, 115, 338, 366, 54},
                   {TA_EXIT, 373, 364, 115, 458, 115, 458, 366, 54}}},
    [GAME_MGSW] = {7, 458, 68, 0, 68,
                   {{TA_NEW, 0, 0, 50, 235, 61, 242, 387, 56},
                    {TA_LOAD, 458, 0, 50, 302, 61, 309, 404, 56},
                    {TA_SETTINGS, 0, 136, 50, 369, 61, 376, 329, 56},
                    {TA_VOLUME, 458, 136, 50, 436, 61, 443, 344, 56},
                    {TA_EXTRA, 0, 272, 50, 503, 61, 510, 309, 56},
                    {TA_HELP, 458, 272, 50, 570, 61, 577, 404, 56},
                    {TA_EXIT, 0, 408, 50, 637, 61, 644, 278, 56}}},
};

static int title_hit(AN *a) {
    const TitleLayout *t = &TITLE[a->game];
    for (int i = 0; i < t->count; i++) {
        const TitleButton *b = &t->button[i];
        if (a->mx >= b->hx && a->mx < b->hx + b->hw && a->my >= b->hy && a->my < b->hy + b->hh) return i;
    }
    return -1;
}

void an_title_render(AN *a) {
    an_gfx_fill(a->fb, a->W, a->H, 0xff000000u);
    Image *bg = an_image_load(a, "pMenu");
    if (bg) an_gfx_blit(a->fb, a->W, a->H, bg, 0, 0, a->W, a->H, 0, 0, 255);
    Image *img = an_image_load(a, "pMenuButton");
    const TitleLayout *t = &TITLE[a->game];
    int hover = title_hit(a);
    for (int i = 0; i < t->count && img; i++) {
        const TitleButton *b = &t->button[i];
        bool lit = i == hover;
        an_gfx_blit(a->fb, a->W, a->H, img, b->ax + (lit ? t->hoverDx : 0), b->ay + (lit ? t->hoverDy : 0), t->w, t->h, b->sx, b->sy, 255);
    }
}

static void open_help(AN *a) {
    char *p = an_fmt("%s/help.html", a->dir);
    if (an_path_resolve(p, a->page, sizeof a->page) < 0 || !an_file_exists(a->page)) {
        an_log(a, "no help.html in the game folder");
        a->page[0] = 0;
    }
    free(p);
}

void an_title_input(AN *a) {
    if (!a->clicked) return;
    a->clicked = false;
    int hit = title_hit(a);
    if (hit < 0) return;
    switch (TITLE[a->game].button[hit].action) {
    case TA_NEW:
        memset(a->L, 0, sizeof a->L);
        memset(&a->scene, 0, sizeof a->scene);
        a->G[2] = 1;
        a->logCount = 0;
        an_vm_start(a, 8);
        break;
    case TA_LOAD: an_slots_open(a, false); break;
    case TA_FLOWCHART: an_flow_open(a, false); break;
    case TA_VOLUME: an_volume_open(a); break;
    case TA_SETTINGS: an_settings_open(a); break;
    case TA_HELP: open_help(a); break;
    case TA_EXIT: an_confirm(a, MSG_QUIT, ACT_QUIT, 0); break;
    default: break;
    }
}

enum { WIN_Y = 534, WIN_H = 186, TEXT_SIZE = 24, LINE_H = 46 };
enum { CHOICE_W = 719, CHOICE_H = 79, CHOICE_GAP = 16 };



static size_t text_shown(AN *a) {
    if (!a->text) return 0;
    size_t n = strlen(a->text);
    if (a->textComplete || a->skipMode || a->ctrl) return n;
    size_t cps = (size_t)(10 + a->textSpeed * a->textSpeed / 80);
    if (a->textSpeed >= 100) return n;
    size_t want = (size_t)(a->now - a->textStart) * cps / 1000;
    if (want >= n) return n;
    while (want > 0 && ((u8)a->text[want] & 0xc0) == 0x80) want--;
    return want;
}

static bool text_done(AN *a) { return !a->text || text_shown(a) >= strlen(a->text); }

static int win_alpha(AN *a) { return clampi(a->windowOpacity * 224 / 100, 0, 255); }

typedef struct GaugeLabel {
    int ax, ay, w, h, x;
} GaugeLabel;

static const GaugeLabel GAUGE_LABEL[GAME_COUNT][2] = {
    [GAME_DDD] = {{4, 585, 55, 13, 22}, {68, 585, 53, 14, 94}},
    [GAME_MGSW] = {{8, 585, 46, 13, 26}, {73, 585, 41, 13, 99}},
};

enum { GAUGE_LABEL_Y = 682, GAUGE_Y = 697, GAUGE_W = 60, GAUGE_FILL = 58 };

static u32 auto_wait(AN *a) { return (u32)(2500 - a->autoSpeed * 20) + (u32)(100 - a->autoSpeed / 2) / 2 * (u32)strlen(a->text); }

static void draw_gauge(AN *a, Image *w, int k, float left) {
    const GaugeLabel *g = &GAUGE_LABEL[a->game][k];
    int x = 20 + 70 * k;
    an_gfx_blit(a->fb, a->W, a->H, w, g->ax, g->ay, g->w, g->h, g->x, GAUGE_LABEL_Y, 255);
    an_gfx_blit(a->fb, a->W, a->H, w, 2 + 62 * k, 600, GAUGE_W, 7, x, GAUGE_Y, 255);
    int fill = (int)(left * GAUGE_FILL + 0.5f);
    if (fill > 0) an_gfx_blit(a->fb, a->W, a->H, w, 3 + 62 * k, 610, fill, 5, x + 1, GAUGE_Y + 1, 255);
}

static void draw_gauges(AN *a, Image *w) {
    if (a->option[OPT_VOICE_BAR] == 0) {
        float p = an_snd_progress(a, CH_VOICE);
        draw_gauge(a, w, 0, p < 0 ? 0 : 1 - p);
    }
    if (a->option[OPT_AUTO_BAR] == 0) {
        float left = 0;
        if (a->autoMode && a->text && a->th.wait == W_TEXT) {
            u32 wait = auto_wait(a), spent = a->now - a->textStart;
            left = spent >= wait ? 0 : 1 - (float)spent / (float)wait;
        }
        draw_gauge(a, w, 1, left);
    }
}

static void draw_window(AN *a) {
    const UILayout *u = an_ui(a);
    Image *w = an_image_load(a, "spMessageWindow");
    if (w) {
        an_gfx_blit(a->fb, a->W, a->H, w, 2, 220, 130, WIN_H, 0, WIN_Y, win_alpha(a));
        an_gfx_blit(a->fb, a->W, a->H, w, 2, 17, 1020, WIN_H, 130, WIN_Y, win_alpha(a));
        an_gfx_blit(a->fb, a->W, a->H, w, 134, 220, 130, WIN_H, 1150, WIN_Y, win_alpha(a));
        draw_gauges(a, w);
    }
    Image *ui = an_image_load(a, "spMessageWindowButton");
    if (a->name) {
        if (a->voiceName && ui) an_gfx_blit(a->fb, a->W, a->H, ui, 4, 53, 76, 28, 92, 563, 255);
        an_text_draw(a, u->nameX, u->nameRow, a->name, u->textSize, 0xffffffffu, 0x90000000u, 0, u->linePitch);
    }
    if (a->text) {
        size_t n = strlen(a->text), shown = text_shown(a);
        char *part = an_alloc(n + 1);
        memcpy(part, a->text, shown < n ? shown : n);
        an_text_draw(a, u->textX, a->name ? u->textRow : u->textRowNoName, part, u->textSize, 0xffffffffu, 0x90000000u, u->textW, u->linePitch);
        free(part);
    }
    if (ui) {
        an_bar_draw(a, ui);
        an_gfx_blit(a->fb, a->W, a->H, ui, 168, 53, 28, 28, 1243, 563, 255);
    }
    Image *cur = an_image_load(a, "pAnimeCursor");
    if (cur && a->th.wait == W_TEXT && text_done(a)) {
        int f = (int)(a->now / 80 % 9);
        int fx = 4 + (f % 6) * 42, fy = 4 + (f / 6) * 42;
        an_gfx_blit(a->fb, a->W, a->H, cur, fx, fy, 36, 36, 1216, 649, 255);
    }
}

static void layout_choices(AN *a) {
    int total = a->choiceCount * CHOICE_H + (a->choiceCount - 1) * CHOICE_GAP;
    int y = (WIN_Y - total) / 2;
    for (int i = 0; i < a->choiceCount; i++) {
        Choice *c = &a->choice[i];
        c->x = (a->W - CHOICE_W) / 2;
        c->y = y + i * (CHOICE_H + CHOICE_GAP);
        c->w = CHOICE_W;
        c->h = CHOICE_H;
    }
}

static int choice_hit(AN *a) {
    for (int i = 0; i < a->choiceCount; i++) {
        Choice *c = &a->choice[i];
        if (a->mx >= c->x && a->mx < c->x + c->w && a->my >= c->y && a->my < c->y + c->h) return i;
    }
    return -1;
}

static void draw_choices(AN *a) {
    layout_choices(a);
    Image *sel = an_image_load(a, "spSelect");
    int hover = choice_hit(a);
    for (int i = 0; i < a->choiceCount; i++) {
        Choice *c = &a->choice[i];
        if (sel) an_gfx_blit(a->fb, a->W, a->H, sel, 152, i == hover ? 99 : 7, CHOICE_W, CHOICE_H, c->x, c->y, 255);
        int w = an_text_width(a, c->text, TEXT_SIZE);
        an_text_draw(a, c->x + (c->w - w) / 2, c->y + c->h / 2 + 8, c->text, TEXT_SIZE, 0xffffffffu, 0x90000000u, 0, LINE_H);
    }
}

void an_window_render(AN *a) {
    if (a->th.wait == W_CHOICE) {
        draw_choices(a);
        return;
    }
    if (a->scene.window && a->text && a->screen == SCR_NONE) draw_window(a);
}

void an_game_input(AN *a) {
    if (a->th.wait == W_CHOICE) {
        layout_choices(a);
        if (!a->clicked) return;
        a->clicked = false;
        int hit = choice_hit(a);
        if (hit < 0) return;
        a->choiceResult = hit + 1;
        a->th.wait = W_NONE;
        return;
    }
    if (a->th.wait == W_TEXT) {
        u32 autoWait = auto_wait(a);
        bool autoNext = a->autoMode && text_done(a) && !an_snd_playing(a, CH_VOICE) && a->now - a->textStart > autoWait;
        if (a->clicked && !text_done(a)) {
            a->clicked = false;
            a->textComplete = true;
            return;
        }
        bool skipOk = (a->skipMode || a->ctrl) && (a->option[OPT_SKIP_UNREAD] == 0 || a->textRead);
        if (a->skipMode && !skipOk) a->skipMode = false;
        if (a->clicked || skipOk || autoNext) {
            a->clicked = false;
            a->th.wait = W_NONE;
            if (!skipOk && a->option[OPT_SKIP_VOICE] == 0) an_snd_stop(a, CH_VOICE);
        }
        return;
    }
    if (a->th.wait == W_MOVIE && (a->clicked || a->skipMode)) {
        a->clicked = false;
        an_movie_stop(a);
        a->th.wait = W_NONE;
        return;
    }
    if (a->th.wait == W_TRANS && a->clicked) {
        a->clicked = false;
        a->transTime = 0;
    }
}

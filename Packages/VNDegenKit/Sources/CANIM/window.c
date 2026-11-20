#include "anim.h"

typedef struct TitleButton {
    int fx, fy, y;
} TitleButton;

enum { TB_NEW, TB_CONTINUE, TB_SETTINGS, TB_VOLUME, TB_EXTRA, TB_FLOWCHART, TB_EXIT, TB_COUNT };

static const TitleButton TITLE[TB_COUNT] = {
    {3, 4, 38}, {373, 4, 98}, {3, 124, 158}, {373, 124, 218}, {3, 244, 278}, {373, 244, 338}, {373, 364, 458},
};

enum { TITLE_X = 115, TITLE_W = 366, TITLE_H = 54, TITLE_HOVER = 60 };

static int title_hit(AN *a) {
    for (int i = 0; i < TB_COUNT; i++)
        if (a->mx >= TITLE_X && a->mx < TITLE_X + TITLE_W && a->my >= TITLE[i].y && a->my < TITLE[i].y + TITLE_H) return i;
    return -1;
}

void an_title_render(AN *a) {
    an_gfx_fill(a->fb, a->W, a->H, 0xff000000u);
    Image *bg = an_image_load(a, "pMenu");
    if (bg) an_gfx_blit(a->fb, a->W, a->H, bg, 0, 0, a->W, a->H, 0, 0, 255);
    Image *b = an_image_load(a, "pMenuButton");
    int hover = title_hit(a);
    for (int i = 0; i < TB_COUNT && b; i++) {
        int fy = TITLE[i].fy + (i == hover ? TITLE_HOVER : 0);
        an_gfx_blit(a->fb, a->W, a->H, b, TITLE[i].fx, fy, TITLE_W, TITLE_H, TITLE_X, TITLE[i].y, 255);
    }
}

void an_title_input(AN *a) {
    if (!a->clicked) return;
    a->clicked = false;
    switch (title_hit(a)) {
    case TB_NEW:
        memset(a->L, 0, sizeof a->L);
        memset(&a->scene, 0, sizeof a->scene);
        a->G[2] = 1;
        a->logCount = 0;
        an_vm_start(a, 8);
        break;
    case TB_EXIT: a->quit = true; break;
    default: break;
    }
}

enum { WIN_Y = 534, WIN_H = 186, TEXT_X = 180, ROW1 = 583, TEXT_W = 1020, TEXT_SIZE = 24, LINE_H = 46, NAME_X = 194, WIN_ALPHA = 130 };
enum { BAR_X = 568, BAR_Y = 694, BAR_W = 712, BAR_H = 26, CHOICE_W = 719, CHOICE_H = 79, CHOICE_GAP = 16 };

static void draw_window(AN *a) {
    Image *w = an_image_load(a, "spMessageWindow");
    if (w) {
        an_gfx_blit(a->fb, a->W, a->H, w, 2, 220, 130, WIN_H, 0, WIN_Y, WIN_ALPHA);
        an_gfx_blit(a->fb, a->W, a->H, w, 2, 17, 1020, WIN_H, 130, WIN_Y, WIN_ALPHA);
        an_gfx_blit(a->fb, a->W, a->H, w, 134, 220, 130, WIN_H, 1150, WIN_Y, WIN_ALPHA);
    }
    Image *ui = an_image_load(a, "spMessageWindowButton");
    if (a->name) {
        if (a->voiceName && ui) an_gfx_blit(a->fb, a->W, a->H, ui, 4, 53, 76, 28, 92, 563, 255);
        an_text_draw(a, NAME_X, ROW1, a->name, TEXT_SIZE, 0xffffffffu, 0x90000000u, 0, LINE_H);
    }
    if (a->text) an_text_draw(a, TEXT_X, a->name ? ROW1 + LINE_H : ROW1, a->text, TEXT_SIZE, 0xffffffffu, 0x90000000u, TEXT_W, LINE_H);
    if (ui) {
        an_gfx_blit(a->fb, a->W, a->H, ui, 2, 118, BAR_W, BAR_H, BAR_X, BAR_Y, 255);
        for (int i = 0; i < 11; i++) an_gfx_blit(a->fb, a->W, a->H, ui, 2 + 58 * i, 2, 56, 22, 606 + 58 * i + (i >= 2) * 10 + (i >= 7) * 10 + (i >= 10) * 10, 696, 255);
        an_gfx_blit(a->fb, a->W, a->H, ui, 168, 53, 28, 28, 1243, 563, 255);
    }
    Image *cur = an_image_load(a, "pAnimeCursor");
    if (cur && a->th.wait == W_TEXT) {
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
    if (a->scene.window && a->text) draw_window(a);
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
        if (a->clicked || a->skipMode) {
            a->clicked = false;
            a->th.wait = W_NONE;
            if (!a->skipMode) an_snd_stop(a, CH_VOICE);
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

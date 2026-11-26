#include "sa_private.h"

enum {
    ROW_H = 27,
    DIGIT_X = 921,
    DIGIT_W = 35,
    DIGIT_H = 25,
    DOWN_X = 961,
    UP_X = 1006,
    ARROW_W = 44,
    VOLUME_MAX = 15,
    BUTTON_W = 252,
    BUTTON_H = 35,
    BUTTON_Y = 679,
    WIDE_Y = 644,
    WIDE_H = 25,
    YES_X = 440,
    NO_X = 640,
    YES_Y = 382,
    YES_W = 200,
    YES_H = 60,
};

enum { HOVER_CHOICE = 0, HOVER_ARROW = 200, HOVER_BUTTON = 300, HOVER_YES = 400, HOVER_NO = 401 };
enum { BTN_KEYS, BTN_GAME, BTN_TITLE, BTN_END, BTN_WINDOWS, BTN_RESET, BUTTONS };
enum { ASK_NONE, ASK_TITLE, ASK_END, ASK_RESET, ASK_WINDOWS };

typedef struct OptChoice {
    u8 setting, index;
    const char *art;
    short x, y, w;
} OptChoice;

static const OptChoice choices[] = {
    {OPT_SCREEN, 0, "opt_b00", 306, 55, 160},      {OPT_SCREEN, 1, "opt_b01", 466, 55, 160},
    {OPT_ALPHA, 0, "opt_c00", 301, 97, 64},        {OPT_ALPHA, 1, "opt_c01", 366, 97, 64},
    {OPT_ALPHA, 2, "opt_c02", 431, 97, 64},        {OPT_ALPHA, 3, "opt_c03", 496, 97, 64},
    {OPT_ALPHA, 4, "opt_c04", 561, 97, 64},        {OPT_EFFECT, 0, "opt_d00", 496, 139, 64},
    {OPT_EFFECT, 1, "opt_d01", 561, 139, 64},      {OPT_FRAME, 0, "opt_e00", 496, 181, 64},
    {OPT_FRAME, 1, "opt_e01", 561, 181, 64},       {OPT_CONFIRM, 0, "opt_f00", 329, 223, 64},
    {OPT_CONFIRM, 1, "opt_f01", 394, 223, 112},    {OPT_CONFIRM, 2, "opt_f02", 505, 223, 120},
    {OPT_POPUP, 0, "opt_g00", 366, 265, 64},       {OPT_POPUP, 1, "opt_g01", 431, 265, 64},
    {OPT_POPUP, 2, "opt_g02", 496, 265, 64},       {OPT_POPUP, 3, "opt_g03", 561, 265, 64},
    {OPT_CLICK, 0, "opt_h00", 404, 307, 112},      {OPT_CLICK, 1, "opt_h01", 515, 307, 112},
    {OPT_RIGHT, 0, "opt_i00", 384, 349, 172},      {OPT_RIGHT, 1, "opt_i01", 555, 349, 72},
    {OPT_CURSOR, 0, "opt_j00", 326, 391, 60},      {OPT_CURSOR, 1, "opt_j01", 386, 391, 60},
    {OPT_CURSOR, 2, "opt_j02", 446, 391, 60},      {OPT_CURSOR, 3, "opt_j03", 506, 391, 60},
    {OPT_CURSOR, 4, "opt_j04", 566, 391, 60},      {OPT_AUTOMOUSE, 0, "opt_k00", 496, 433, 64},
    {OPT_AUTOMOUSE, 1, "opt_k01", 561, 433, 64},   {OPT_TEXT_SPEED, 0, "opt_l00", 301, 475, 64},
    {OPT_TEXT_SPEED, 1, "opt_l01", 366, 475, 64},  {OPT_TEXT_SPEED, 2, "opt_l02", 431, 475, 64},
    {OPT_TEXT_SPEED, 3, "opt_l03", 496, 475, 64},  {OPT_TEXT_SPEED, 4, "opt_l04", 561, 475, 64},
    {OPT_AUTO_SPEED, 0, "opt_m00", 301, 517, 64},  {OPT_AUTO_SPEED, 1, "opt_m01", 366, 517, 64},
    {OPT_AUTO_SPEED, 2, "opt_m02", 431, 517, 64},  {OPT_AUTO_SPEED, 3, "opt_m03", 496, 517, 64},
    {OPT_AUTO_SPEED, 4, "opt_m04", 561, 517, 64},  {OPT_AUTO_VOICE, 0, "opt_n00", 344, 559, 180},
    {OPT_AUTO_VOICE, 1, "opt_n01", 525, 559, 100}, {OPT_SKIP, 0, "opt_o00", 454, 601, 72},
    {OPT_SKIP, 1, "opt_o01", 525, 601, 100},       {OPT_3D, 0, "opt_s00", 1043, 55, 84},
    {OPT_3D, 1, "opt_s01", 1128, 55, 84},          {OPT_BRIGHTNESS, 0, "opt_t00", 888, 97, 64},
    {OPT_BRIGHTNESS, 1, "opt_t01", 953, 97, 64},   {OPT_BRIGHTNESS, 2, "opt_t02", 1018, 97, 64},
    {OPT_BRIGHTNESS, 3, "opt_t03", 1083, 97, 64},  {OPT_BRIGHTNESS, 4, "opt_t04", 1148, 97, 64},
    {OPT_CAMERA, 0, "opt_u00", 888, 139, 64},      {OPT_CAMERA, 1, "opt_u01", 953, 139, 64},
    {OPT_CAMERA, 2, "opt_u02", 1018, 139, 64},     {OPT_CAMERA, 3, "opt_u03", 1083, 139, 64},
    {OPT_CAMERA, 4, "opt_u04", 1148, 139, 64},     {OPT_GAME_SPEED, 0, "opt_p00", 1018, 181, 64},
    {OPT_GAME_SPEED, 1, "opt_p01", 1083, 181, 64}, {OPT_GAME_SPEED, 2, "opt_p02", 1148, 181, 64},
    {OPT_BATTLE_FX, 0, "opt_q00", 1043, 223, 84},  {OPT_BATTLE_FX, 1, "opt_q01", 1128, 223, 84},
    {OPT_RANKS, 0, "opt_r00", 973, 265, 144},      {OPT_RANKS, 1, "opt_r01", 1118, 265, 96},
    {OPT_AUTOSAVE, 0, "opt_v00", 1033, 307, 44},   {OPT_AUTOSAVE, 1, "opt_v01", 1078, 307, 44},
    {OPT_AUTOSAVE, 2, "opt_v02", 1123, 307, 44},   {OPT_AUTOSAVE, 3, "opt_v03", 1168, 307, 44},
    {OPT_BGM_ON, 0, "opt_w00", 1083, 391, 64},     {OPT_BGM_ON, 1, "opt_w01", 1148, 391, 64},
    {OPT_SE_ON, 0, "opt_x00", 1083, 433, 64},      {OPT_SE_ON, 1, "opt_x01", 1148, 433, 64},
    {OPT_VE_ON, 0, "opt_y00", 1083, 475, 64},      {OPT_VE_ON, 1, "opt_y01", 1148, 475, 64},
    {OPT_TALK_ON, 0, "opt_z00", 1083, 517, 64},    {OPT_TALK_ON, 1, "opt_z01", 1148, 517, 64},
    {OPT_INACTIVE, 0, "opt_000", 978, 559, 96},    {OPT_INACTIVE, 1, "opt_001", 1073, 559, 76},
    {OPT_INACTIVE, 2, "opt_002", 1148, 559, 64},   {OPT_PAGE_VOICE, 0, "opt_100", 1083, 601, 64},
    {OPT_PAGE_VOICE, 1, "opt_101", 1148, 601, 64},
};

#define CHOICES ((int)(sizeof choices / sizeof choices[0]))

typedef struct OptVolume {
    u8 setting;
    const char *digits, *down, *up;
    short y;
} OptVolume;

static const OptVolume volumes[] = {
    {OPT_BGM, "opt_w03", "opt_w02", "opt_w04", 391},
    {OPT_SE, "opt_x03", "opt_x02", "opt_x04", 433},
    {OPT_VE, "opt_y03", "opt_y02", "opt_y04", 475},
    {OPT_TALK, "opt_z03", "opt_z02", "opt_z04", 517},
};

typedef struct OptButton {
    const char *art;
    short x, y, w, h;
} OptButton;

static const OptButton buttons[BUTTONS] = {
    {"opt_a04", 138, BUTTON_Y, BUTTON_W, BUTTON_H}, {"opt_a01", 389, BUTTON_Y, BUTTON_W, BUTTON_H},
    {"opt_a02", 640, BUTTON_Y, BUTTON_W, BUTTON_H}, {"opt_a03", 891, BUTTON_Y, BUTTON_W, BUTTON_H},
    {"opt_a06", 654, WIDE_Y, 360, WIDE_H},          {"opt_a05", 1028, WIDE_Y, 184, WIDE_H},
};

enum { ART_VOLUME = CHOICES, ART_BUTTON = ART_VOLUME + 12, ART_YES = ART_BUTTON + BUTTONS, ART_NO, ART_COUNT };

static const u8 defaults[OPT_COUNT] = {
    [OPT_ALPHA] = 2,  [OPT_POPUP] = 2,   [OPT_RIGHT] = 1,      [OPT_CURSOR] = 4,  [OPT_AUTOMOUSE] = 1, [OPT_TEXT_SPEED] = 2,
    [OPT_AUTO_SPEED] = 2, [OPT_SKIP] = 1, [OPT_BRIGHTNESS] = 2, [OPT_CAMERA] = 2, [OPT_AUTOSAVE] = 2,  [OPT_PAGE_VOICE] = 1,
    [OPT_BGM] = 8,    [OPT_SE] = 8,      [OPT_VE] = 8,         [OPT_TALK] = 10,
};

static const u32 textMs[5] = {0, 25, 50, 75, 100};
static const u32 autoBaseMs[5] = {230, 250, 260, 290, 320}, autoCharMs[5] = {25, 25, 50, 75, 100}, autoLineMs[5] = {700, 700, 975, 1225, 1475};
static const u32 windowAlpha[5] = {64, 112, 160, 192, 255};

void sa_option_defaults(OptionScreen *o) {
    memset(o->value, 0, sizeof o->value);
    memcpy(o->value, defaults, OPT_COUNT);
}

void sa_option_init(OptionScreen *o, Vfs *vfs) {
    o->bg = sa_load_image(vfs, "opt_a00.agf");
    char name[24];
    for (int i = 0; i < CHOICES; i++) {
        snprintf(name, sizeof name, "%s.agf", choices[i].art);
        o->art[i] = sa_load_image(vfs, name);
    }
    for (int i = 0; i < 4; i++) {
        const char *n[3] = {volumes[i].digits, volumes[i].down, volumes[i].up};
        for (int k = 0; k < 3; k++) {
            snprintf(name, sizeof name, "%s.agf", n[k]);
            o->art[ART_VOLUME + i * 3 + k] = sa_load_image(vfs, name);
        }
    }
    for (int i = 0; i < BUTTONS; i++) {
        snprintf(name, sizeof name, "%s.agf", buttons[i].art);
        o->art[ART_BUTTON + i] = sa_load_image(vfs, name);
    }
    o->art[ART_YES] = sa_load_image(vfs, "pop_b00.agf");
    o->art[ART_NO] = sa_load_image(vfs, "pop_b01.agf");
    o->hover = -1;
    sa_option_defaults(o);
}

void sa_option_free(OptionScreen *o) {
    sa_image_free(o->bg);
    for (int i = 0; i < OPTION_ART; i++) sa_image_free(o->art[i]);
    memset(o, 0, sizeof *o);
}

int sa_option(const SA *s, int which) { return s->opt.value[which]; }

u32 sa_option_text_ms(const SA *s) { return textMs[s->opt.value[OPT_TEXT_SPEED] % 5]; }

u32 sa_option_auto_ms(const SA *s, int chars, int lines) {
    int k = s->opt.value[OPT_AUTO_SPEED] % 5;
    return autoBaseMs[k] + autoCharMs[k] * (u32)chars + autoLineMs[k] * (u32)lines;
}

u32 sa_option_window_alpha(const SA *s) { return windowAlpha[s->opt.value[OPT_ALPHA] % 5]; }

static float gain(const SA *s, int on, int level) { return s->opt.value[on] ? 0.0f : (float)s->opt.value[level] / VOLUME_MAX; }

void sa_option_apply(SA *s) {
    sa_set_volume(s->mix, CH_BGM, gain(s, OPT_BGM_ON, OPT_BGM));
    sa_set_volume(s->mix, CH_SE, gain(s, OPT_SE_ON, OPT_SE));
    sa_set_volume(s->mix, CH_VOICE, gain(s, OPT_TALK_ON, OPT_TALK));
    sa_dungeon_options(&s->dungeon, s->opt.value[OPT_BRIGHTNESS], s->opt.value[OPT_CAMERA], s->opt.value[OPT_3D] == 0);
}

enum { GLOBAL_OPTIONS = SA_G_VARS, GLOBAL_CG = GLOBAL_OPTIONS + OPTION_BYTES, GLOBAL_SCENES = GLOBAL_CG + EXTRA_CG,
       GLOBAL_SCENARIOS = GLOBAL_SCENES + EXTRA_SCENES, GLOBAL_SIZE = GLOBAL_SCENARIOS + EXTRA_SCENARIOS };

void sa_global_load(SA *s) {
    char *path = sa_fmt("%s/vndegen/global.dat", s->saveDir);
    size_t n;
    u8 *g = sa_read_file(path, &n);
    free(path);
    if (g && n >= SA_G_VARS) memcpy(s->vm.g, g, SA_G_VARS);
    if (g && n >= GLOBAL_SIZE) {
        memcpy(s->opt.value, g + GLOBAL_OPTIONS, OPTION_BYTES);
        memcpy(s->extra.viewedCg, g + GLOBAL_CG, EXTRA_CG);
        memcpy(s->extra.viewedScene, g + GLOBAL_SCENES, EXTRA_SCENES);
        memcpy(s->extra.viewedScenario, g + GLOBAL_SCENARIOS, EXTRA_SCENARIOS);
    }
    free(g);
}

void sa_global_save(SA *s) {
    u8 *g = sa_alloc(GLOBAL_SIZE);
    memcpy(g, s->vm.g, SA_G_VARS);
    memcpy(g + GLOBAL_OPTIONS, s->opt.value, OPTION_BYTES);
    memcpy(g + GLOBAL_CG, s->extra.viewedCg, EXTRA_CG);
    memcpy(g + GLOBAL_SCENES, s->extra.viewedScene, EXTRA_SCENES);
    memcpy(g + GLOBAL_SCENARIOS, s->extra.viewedScenario, EXTRA_SCENARIOS);
    char *dir = sa_fmt("%s/vndegen", s->saveDir), *path = sa_fmt("%s/global.dat", dir);
    sa_mkdirs(dir);
    sa_write_file(path, g, GLOBAL_SIZE);
    free(dir);
    free(path);
    free(g);
}

void sa_option_open(SA *s) {
    OptionScreen *o = &s->opt;
    o->back = s->mode;
    o->hover = -1;
    o->ask = ASK_NONE;
    s->mode = MODE_OPTION;
    s->dirty = true;
}

static bool in(int x, int y, int rx, int ry, int w, int h) { return x >= rx && x < rx + w && y >= ry && y < ry + h; }

static bool in_game(const SA *s) { return s->opt.back != MODE_TITLE && s->opt.back != MODE_EXTRA; }

static bool button_shown(const SA *s, int b) { return b != BTN_GAME || in_game(s); }

static int hit(SA *s, int x, int y) {
    OptionScreen *o = &s->opt;
    if (o->ask != ASK_NONE) {
        if (in(x, y, YES_X, YES_Y, YES_W, YES_H)) return HOVER_YES;
        if (in(x, y, NO_X, YES_Y, YES_W, YES_H)) return HOVER_NO;
        return -1;
    }
    for (int i = 0; i < CHOICES; i++)
        if (in(x, y, choices[i].x, choices[i].y, choices[i].w, ROW_H)) return HOVER_CHOICE + i;
    for (int i = 0; i < 4; i++) {
        if (in(x, y, DOWN_X, volumes[i].y, ARROW_W, ROW_H)) return HOVER_ARROW + i * 2;
        if (in(x, y, UP_X, volumes[i].y, ARROW_W, ROW_H)) return HOVER_ARROW + i * 2 + 1;
    }
    for (int i = 0; i < BUTTONS; i++)
        if (button_shown(s, i) && in(x, y, buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h)) return HOVER_BUTTON + i;
    return -1;
}

static void draw_ask(SA *s, u32 *fb, const char *text) {
    sa_dim(fb);
    char buf[160];
    snprintf(buf, sizeof buf, "%s", text);
    const char *lines[2] = {buf, NULL};
    int count = 1;
    char *bar = strchr(buf, '|');
    if (bar) {
        *bar = 0;
        lines[1] = bar + 1;
        count = 2;
    }
    sa_popup_draw(s->battle.pop, fb, lines, count);
    OptionScreen *o = &s->opt;
    sa_blit_rect(fb, o->art[ART_YES], 0, o->hover == HOVER_YES ? YES_H : 0, YES_W, YES_H, YES_X, YES_Y);
    sa_blit_rect(fb, o->art[ART_NO], 0, o->hover == HOVER_NO ? YES_H : 0, YES_W, YES_H, NO_X, YES_Y);
}

static void draw(SA *s) {
    OptionScreen *o = &s->opt;
    u32 *fb = s->fb;
    sa_blit(fb, o->bg, 0, 0);
    for (int i = 0; i < CHOICES; i++) {
        const OptChoice *c = &choices[i];
        int frame = o->value[c->setting] == c->index ? 2 : o->hover == HOVER_CHOICE + i ? 1 : 0;
        sa_blit_rect(fb, o->art[i], 0, frame * ROW_H, c->w, ROW_H, c->x, c->y);
    }
    for (int i = 0; i < 4; i++) {
        int v = o->value[volumes[i].setting], y = volumes[i].y;
        sa_blit_rect(fb, o->art[ART_VOLUME + i * 3], v * DIGIT_W, 0, DIGIT_W, DIGIT_H, DIGIT_X, y);
        int down = v == 0 ? 2 : o->hover == HOVER_ARROW + i * 2 ? 1 : 0;
        int up = v == VOLUME_MAX ? 2 : o->hover == HOVER_ARROW + i * 2 + 1 ? 1 : 0;
        sa_blit_rect(fb, o->art[ART_VOLUME + i * 3 + 1], 0, down * ROW_H, ARROW_W, ROW_H, DOWN_X, y);
        sa_blit_rect(fb, o->art[ART_VOLUME + i * 3 + 2], 0, up * ROW_H, ARROW_W, ROW_H, UP_X, y);
    }
    for (int i = 0; i < BUTTONS; i++) {
        if (!button_shown(s, i)) continue;
        const OptButton *b = &buttons[i];
        sa_blit_rect(fb, o->art[ART_BUTTON + i], 0, o->hover == HOVER_BUTTON + i ? b->h : 0, b->w, b->h, b->x, b->y);
    }
    static const char *const asks[] = {"", "タイトルに戻りますか？", "ゲームを終了しますか？", "全て初期設定に戻しますか？",
                                       "全てウインドウの位置を|初期位置に戻しますか？"};
    if (o->ask != ASK_NONE) draw_ask(s, fb, asks[o->ask]);
}

static void changed(SA *s) {
    sa_option_apply(s);
    s->vm.gDirty = true;
}

static void close_screen(SA *s) {
    OptionScreen *o = &s->opt;
    s->mode = o->back;
    if (s->mode == MODE_CAMP) sa_camp_return(s);
    s->town.hover = -1;
    s->dirty = true;
}

static void answer(SA *s) {
    OptionScreen *o = &s->opt;
    int ask = o->ask;
    o->ask = ASK_NONE;
    if (ask == ASK_TITLE) sa_go_title(s);
    else if (ask == ASK_END) s->quit = true;
    else if (ask == ASK_RESET) {
        u8 page = o->value[OPT_SAVE_PAGE];
        sa_option_defaults(o);
        o->value[OPT_SAVE_PAGE] = page;
        changed(s);
    }
}

static void click(SA *s, int h) {
    OptionScreen *o = &s->opt;
    if (o->ask != ASK_NONE) {
        if (h == HOVER_YES) answer(s);
        else if (h == HOVER_NO) o->ask = ASK_NONE;
        return;
    }
    if (h < 0) return;
    if (h < HOVER_ARROW) {
        const OptChoice *c = &choices[h - HOVER_CHOICE];
        o->value[c->setting] = c->index;
        changed(s);
    } else if (h < HOVER_BUTTON) {
        int i = (h - HOVER_ARROW) / 2, up = (h - HOVER_ARROW) % 2;
        u8 *v = &o->value[volumes[i].setting];
        if (up && *v < VOLUME_MAX) (*v)++;
        if (!up && *v > 0) (*v)--;
        changed(s);
    } else {
        switch (h - HOVER_BUTTON) {
        case BTN_KEYS:
            sa_log(s, "key config left out: the app maps keys");
            break;
        case BTN_GAME:
            close_screen(s);
            break;
        case BTN_TITLE:
            if (in_game(s)) o->ask = ASK_TITLE;
            else close_screen(s);
            break;
        case BTN_END:
            o->ask = ASK_END;
            break;
        case BTN_WINDOWS:
            o->ask = ASK_WINDOWS;
            break;
        case BTN_RESET:
            o->ask = ASK_RESET;
            break;
        }
    }
}

bool sa_option_frame(SA *s) {
    if (s->mode != MODE_OPTION) return false;
    OptionScreen *o = &s->opt;
    int h = hit(s, s->mouseX, s->mouseY);
    if (h != o->hover) {
        o->hover = h;
        s->dirty = true;
    }
    if (s->rightClicked) {
        s->rightClicked = false;
        if (o->ask != ASK_NONE) o->ask = ASK_NONE;
        else close_screen(s);
        s->dirty = true;
    }
    if (s->clicked) {
        s->clicked = false;
        click(s, h);
        s->dirty = true;
    }
    if (s->mode == MODE_OPTION && s->dirty) {
        draw(s);
        s->serial++;
        s->dirty = false;
    }
    return true;
}

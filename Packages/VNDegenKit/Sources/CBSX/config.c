#include "bsx.h"

enum { BAR_SPEED, BAR_AUTO, BAR_R, BAR_G, BAR_B, BAR_A, BAR_MASTER, BAR_BGM, BAR_SE, BAR_BGS, BAR_VOICE, BAR_ATTEN, BAR_COUNT };

typedef struct CBtn {
    Image *img;
    int frames, x, y;
} CBtn;

typedef struct Bar {
    bool used;
    Image *img;
    int x, y;
    CBtn minus, plus;
} Bar;

struct ConfigPhase {
    Image *base, *check, *radio;
    Bar bars[BAR_COUNT];
    CBtn chars[8];
    int charCount;
    CBtn reset, back, title, quit;
    int checks[3][2];
    int autoSel[3][2];
    int voiceMode[4][2];
    int drag;
};

static Image *own(BX *b, const char *name) {
    Image *im = name && *name ? bx_image_load(b, name) : NULL;
    if (!im) return NULL;
    Image *c = bx_image_new(im->w, im->h);
    c->x = im->x;
    c->y = im->y;
    memcpy(c->px, im->px, sizeof(u32) * (size_t)im->w * (size_t)im->h);
    return c;
}

static bool cbtn(BX *b, const char *key, CBtn *bt) {
    memset(bt, 0, sizeof *bt);
    const char *v = bx_cfg_get(&b->cfg, "config screen", key);
    if (!v) return false;
    char name[64], f[64];
    bx_cfg_field(v, 0, name, sizeof name);
    bt->img = own(b, name);
    bt->frames = bx_cfg_field(v, 1, f, sizeof f) > 0 ? bx_eval(f, NULL) : 1;
    if (bt->frames < 1) bt->frames = 1;
    bt->x = bx_cfg_field(v, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    bt->y = bx_cfg_field(v, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    return bt->img != NULL;
}

static bool on(const CBtn *bt, int x, int y) {
    if (!bt->img) return false;
    int h = bt->img->h / bt->frames;
    return x >= bt->x && y >= bt->y && x < bt->x + bt->img->w && y < bt->y + h;
}

static void draw(BX *b, u32 *dst, const CBtn *bt, int frame) {
    if (!bt->img) return;
    int h = bt->img->h / bt->frames;
    bx_blit_part(dst, b->W, b->H, bt->img, 0, clampi(frame, 0, bt->frames - 1) * h, bt->img->w, h, bt->x, bt->y, 255);
}

static void xy(BX *b, const char *key, int *out, int dx, int dy) {
    int v[2] = {dx, dy};
    bx_cfg_ints(&b->cfg, "config screen", key, v, 2);
    out[0] = v[0];
    out[1] = v[1];
}

static int *bar_value(BX *b, int i) {
    switch (i) {
    case BAR_SPEED: return &b->set.textSpeed;
    case BAR_AUTO: return &b->set.autoSpeed;
    case BAR_R: return &b->set.color[0];
    case BAR_G: return &b->set.color[1];
    case BAR_B: return &b->set.color[2];
    case BAR_A: return &b->set.color[3];
    case BAR_MASTER: return &b->volMaster;
    case BAR_BGM: return &b->volBgm;
    case BAR_SE: return &b->volSe;
    case BAR_BGS: return &b->volBgs;
    case BAR_VOICE: return &b->volVoice;
    default: return &b->set.attenuation;
    }
}

static int bar_max(int i) { return i >= BAR_R && i <= BAR_A ? 255 : 100; }

void bx_settings_apply(BX *b) {
    b->msgSpeed = (100 - clampi(b->set.textSpeed, 0, 100)) * 60 / 100;
    b->bgColor = (u32)clampi(b->set.color[0], 0, 255) | (u32)clampi(b->set.color[1], 0, 255) << 8 | (u32)clampi(b->set.color[2], 0, 255) << 16 |
                 (u32)clampi(b->set.color[3], 0, 255) << 24;
    bx_snd_volumes(b);
}

void bx_settings_defaults(BX *b) {
    u32 c = bx_cfg_color(&b->cfg, "message window", "bg color", 0x80000000u);
    b->set.color[0] = (int)(c & 0xff);
    b->set.color[1] = (int)(c >> 8 & 0xff);
    b->set.color[2] = (int)(c >> 16 & 0xff);
    b->set.color[3] = (int)(c >> 24);
    b->set.textSpeed = 50;
    b->set.autoSpeed = 50;
    b->set.attenuation = 60;
    b->set.skipUnread = 0;
    b->set.ceaseSkip = 1;
    b->set.voiceMask = 0xff;
    b->volMaster = b->volBgm = b->volSe = b->volBgs = b->volVoice = 100;
}

static char *settings_path(BX *b) { return bx_fmt("%s/save/vndegen.cfg", b->saveDir); }

void bx_settings_load(BX *b) {
    bx_settings_defaults(b);
    char *path = settings_path(b);
    size_t n;
    u8 *d = bx_read_file(path, &n);
    free(path);
    if (d) {
        Cfg c;
        char *text = bx_fmt("[settings]\n%s", (char *)d);
        bx_cfg_parse(&c, (const u8 *)text, strlen(text));
        free(text);
        free(d);
        struct {
            const char *key;
            int *v;
        } keys[] = {{"text speed", &b->set.textSpeed}, {"auto speed", &b->set.autoSpeed}, {"red", &b->set.color[0]},
                    {"green", &b->set.color[1]}, {"blue", &b->set.color[2]}, {"alpha", &b->set.color[3]},
                    {"master", &b->volMaster}, {"bgm", &b->volBgm}, {"se", &b->volSe}, {"bgs", &b->volBgs},
                    {"voice", &b->volVoice}, {"attenuation", &b->set.attenuation}, {"skip unread", &b->set.skipUnread},
                    {"cease skip", &b->set.ceaseSkip}, {"voice mask", &b->set.voiceMask}, {"voice mode", &b->set.voiceMode},
                    {"auto select", &b->set.autoSelect}, {"effects", &b->set.noEffects}};
        for (size_t i = 0; i < sizeof keys / sizeof *keys; i++) *keys[i].v = bx_cfg_int(&c, "settings", keys[i].key, *keys[i].v);
        bx_cfg_free(&c);
    }
    bx_settings_apply(b);
}

void bx_settings_save(BX *b) {
    char *text = bx_fmt("text speed = %d\nauto speed = %d\nred = %d\ngreen = %d\nblue = %d\nalpha = %d\nmaster = %d\nbgm = %d\nse = %d\n"
                        "bgs = %d\nvoice = %d\nattenuation = %d\nskip unread = %d\ncease skip = %d\nvoice mask = %d\nvoice mode = %d\n"
                        "auto select = %d\neffects = %d\n",
                        b->set.textSpeed, b->set.autoSpeed, b->set.color[0], b->set.color[1], b->set.color[2], b->set.color[3],
                        b->volMaster, b->volBgm, b->volSe, b->volBgs, b->volVoice, b->set.attenuation, b->set.skipUnread,
                        b->set.ceaseSkip, b->set.voiceMask, b->set.voiceMode, b->set.autoSelect, b->set.noEffects);
    char dir[2048];
    snprintf(dir, sizeof dir, "%s/save", b->saveDir);
    bx_mkdirs(dir);
    char *path = settings_path(b);
    bx_write_file(path, text, strlen(text));
    free(path);
    free(text);
}

void bx_config_free(BX *b) {
    ConfigPhase *c = b->config;
    if (!c) return;
    bx_image_free(c->base);
    bx_image_free(c->check);
    bx_image_free(c->radio);
    for (int i = 0; i < BAR_COUNT; i++) {
        bx_image_free(c->bars[i].img);
        bx_image_free(c->bars[i].minus.img);
        bx_image_free(c->bars[i].plus.img);
    }
    for (int i = 0; i < c->charCount; i++) bx_image_free(c->chars[i].img);
    CBtn *bs[] = {&c->reset, &c->back, &c->title, &c->quit};
    for (int i = 0; i < 4; i++) bx_image_free(bs[i]->img);
    free(c);
    b->config = NULL;
}

static void bar_spec(BX *b, ConfigPhase *c, int index, const char *prefix) {
    Bar *bar = &c->bars[index];
    char key[64];
    snprintf(key, sizeof key, "%s bar", prefix);
    CBtn t;
    if (!cbtn(b, key, &t)) return;
    bar->used = true;
    bar->img = t.img;
    bar->x = t.x;
    bar->y = t.y;
    snprintf(key, sizeof key, "%s min", prefix);
    cbtn(b, key, &bar->minus);
    snprintf(key, sizeof key, "%s max", prefix);
    cbtn(b, key, &bar->plus);
}

bool bx_config_open(BX *b) {
    bx_config_free(b);
    ConfigPhase *c = bx_alloc(sizeof *c);
    b->config = c;
    const char *sec = "config screen";
    const char *base = bx_cfg_get(&b->cfg, sec, "config base");
    c->base = own(b, base);
    c->check = own(b, bx_cfg_get(&b->cfg, sec, "check mark"));
    c->radio = own(b, bx_cfg_get(&b->cfg, sec, "radio button"));
    static const char *prefixes[BAR_COUNT] = {"speed0", "speed1", "color0", "color1", "color2", "color3", "volume0", "volume1", "volume2", "volume3", "volume4", "volume6"};
    for (int i = 0; i < BAR_COUNT; i++) bar_spec(b, c, i, prefixes[i]);
    c->charCount = clampi(bx_cfg_int(&b->cfg, sec, "characters", 0), 0, 8);
    for (int i = 0; i < c->charCount; i++) {
        char key[32];
        snprintf(key, sizeof key, "character%d", i);
        cbtn(b, key, &c->chars[i]);
    }
    cbtn(b, "reset button", &c->reset);
    cbtn(b, "back button", &c->back);
    cbtn(b, "title button", &c->title);
    cbtn(b, "exit button", &c->quit);
    xy(b, "cease skip", c->checks[0], 414, 122);
    xy(b, "enable skip unread", c->checks[1], 590, 122);
    xy(b, "all effect", c->checks[2], 186, 122);
    for (int i = 0; i < 3; i++) {
        char key[32];
        snprintf(key, sizeof key, "auto select pos%d", i);
        xy(b, key, c->autoSel[i], 37 + 100 * i, 177);
    }
    for (int i = 0; i < 4; i++) {
        char key[32];
        snprintf(key, sizeof key, "voice mode pos%d", i);
        xy(b, key, c->voiceMode[i], 37, 265 + 28 * i);
    }
    c->drag = -1;
    b->phaseBack = b->phase;
    b->phase = PH_CONFIG;
    return true;
}

static void close_config(BX *b) {
    bx_settings_save(b);
    bx_config_free(b);
    b->phase = b->phaseBack;
    b->phaseBack = PH_NONE;
}

static bool near(int x, int y, const int *p) { return x >= p[0] - 4 && x < p[0] + 150 && y >= p[1] - 4 && y < p[1] + 16; }

static void set_bar(BX *b, ConfigPhase *c, int i, int mx) {
    Bar *bar = &c->bars[i];
    int w = bar->img ? bar->img->w : 200;
    *bar_value(b, i) = clampi((mx - bar->x) * bar_max(i) / (w > 0 ? w : 1), 0, bar_max(i));
    bx_settings_apply(b);
}

void bx_config_update(BX *b) {
    ConfigPhase *c = b->config;
    if (!c) return;
    if (c->drag >= 0) {
        if (b->lDown) set_bar(b, c, c->drag, b->mx);
        else c->drag = -1;
    }
    if (b->rclick) {
        b->rclick = false;
        close_config(b);
        return;
    }
    if (b->lPress)
        for (int i = 0; i < BAR_COUNT; i++) {
            Bar *bar = &c->bars[i];
            if (bar->used && bar->img && b->mx >= bar->x && b->mx < bar->x + bar->img->w && b->my >= bar->y && b->my < bar->y + bar->img->h) {
                c->drag = i;
                set_bar(b, c, i, b->mx);
            }
        }
    if (!b->click) return;
    b->click = false;
    for (int i = 0; i < BAR_COUNT; i++) {
        Bar *bar = &c->bars[i];
        int step = bar_max(i) / 10;
        if (on(&bar->minus, b->mx, b->my)) *bar_value(b, i) = clampi(*bar_value(b, i) - step, 0, bar_max(i));
        else if (on(&bar->plus, b->mx, b->my)) *bar_value(b, i) = clampi(*bar_value(b, i) + step, 0, bar_max(i));
        else continue;
        bx_settings_apply(b);
        return;
    }
    for (int i = 0; i < c->charCount; i++)
        if (on(&c->chars[i], b->mx, b->my)) {
            b->set.voiceMask ^= 1 << i;
            return;
        }
    if (near(b->mx, b->my, c->checks[0])) b->set.ceaseSkip = !b->set.ceaseSkip;
    else if (near(b->mx, b->my, c->checks[1])) b->set.skipUnread = !b->set.skipUnread;
    else if (near(b->mx, b->my, c->checks[2])) b->set.noEffects = !b->set.noEffects;
    for (int i = 0; i < 3; i++)
        if (near(b->mx, b->my, c->autoSel[i])) b->set.autoSelect = i;
    for (int i = 0; i < 4; i++)
        if (near(b->mx, b->my, c->voiceMode[i])) b->set.voiceMode = i;
    if (on(&c->reset, b->mx, b->my)) {
        bx_settings_defaults(b);
        bx_settings_apply(b);
    } else if (on(&c->back, b->mx, b->my)) close_config(b);
    else if (on(&c->title, b->mx, b->my)) {
        bool inGame = b->phaseBack == PH_NONE;
        close_config(b);
        if (inGame) bx_system(b, "@title");
    } else if (on(&c->quit, b->mx, b->my)) {
        bx_settings_save(b);
        b->quit = true;
    }
}

void bx_config_draw(BX *b, u32 *dst) {
    ConfigPhase *c = b->config;
    if (!c) return;
    if (c->base) bx_blit(dst, b->W, b->H, c->base, c->base->x, c->base->y, 255);
    for (int i = 0; i < BAR_COUNT; i++) {
        Bar *bar = &c->bars[i];
        if (!bar->used || !bar->img) continue;
        int w = bar->img->w * *bar_value(b, i) / bar_max(i);
        bx_blit_part(dst, b->W, b->H, bar->img, 0, 0, w, bar->img->h, bar->x, bar->y, 255);
        draw(b, dst, &bar->minus, on(&bar->minus, b->mx, b->my));
        draw(b, dst, &bar->plus, on(&bar->plus, b->mx, b->my));
    }
    for (int i = 0; i < c->charCount; i++) draw(b, dst, &c->chars[i], b->set.voiceMask >> i & 1 ? 0 : 1);
    if (c->check) {
        int flags[3] = {b->set.ceaseSkip, b->set.skipUnread, !b->set.noEffects};
        for (int i = 0; i < 3; i++)
            if (flags[i]) bx_blit(dst, b->W, b->H, c->check, c->checks[i][0] + 1, c->checks[i][1] + 1, 255);
    }
    if (c->radio) {
        int a = clampi(b->set.autoSelect, 0, 2), v = clampi(b->set.voiceMode, 0, 3);
        bx_blit(dst, b->W, b->H, c->radio, c->autoSel[a][0], c->autoSel[a][1], 255);
        bx_blit(dst, b->W, b->H, c->radio, c->voiceMode[v][0], c->voiceMode[v][1], 255);
    }
    draw(b, dst, &c->reset, on(&c->reset, b->mx, b->my));
    draw(b, dst, &c->back, on(&c->back, b->mx, b->my));
    draw(b, dst, &c->title, on(&c->title, b->mx, b->my));
    draw(b, dst, &c->quit, on(&c->quit, b->mx, b->my));
}

#include "bsx.h"
#include <math.h>

enum { BAR_SPEED, BAR_AUTO, BAR_R, BAR_G, BAR_B, BAR_A, BAR_MASTER, BAR_BGM, BAR_SE, BAR_BGS, BAR_VOICE, BAR_BGV, BAR_ATTEN, BAR_COUNT };

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
    CBtn chars[8], details[8];
    int charCount;
    CBtn reset, back, title, quit;
    int checks[3][2];
    int autoSel[3][2];
    int voiceMode[4][2];
    int bgmSel[2][2];
    bool hasBgmSel;
    int drag;
    int detail;
    Image *dssBase, *dssCheck;
    int dssPlay[2];
    Bar dssBars[2];
    CBtn dssBack;
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

static bool cbtn_in(BX *b, const char *sec, const char *key, CBtn *bt) {
    memset(bt, 0, sizeof *bt);
    const char *v = bx_cfg_get(&b->cfg, sec, key);
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

static bool cbtn(BX *b, const char *key, CBtn *bt) { return cbtn_in(b, "config screen", key, bt); }

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
    case BAR_BGV: return &b->volBgv;
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

static int cfg_percent(BX *b, const char *sec, const char *key, float def) {
    const char *v = bx_cfg_get(&b->cfg, sec, key);
    return (int)lroundf((v ? (float)atof(v) : def) * 100.0f);
}

void bx_settings_defaults(BX *b) {
    u32 c = bx_cfg_color(&b->cfg, "message window", "bg color", 0x80000000u);
    b->set.color[0] = (int)(c & 0xff);
    b->set.color[1] = (int)(c >> 8 & 0xff);
    b->set.color[2] = (int)(c >> 16 & 0xff);
    b->set.color[3] = (int)(c >> 24);
    b->set.textSpeed = 50;
    b->set.autoSpeed = 50;
    b->set.skipUnread = 0;
    b->set.ceaseSkip = 0;
    b->set.noEffects = 0;
    b->set.voiceMask = -1;
    b->set.voiceMode = 0;
    b->set.autoSelect = 0;
    b->set.bgmSelect = 0;
    b->set.xCount = 1;
    const char *dv = "default volume";
    b->volMaster = cfg_percent(b, dv, "master", 1.0f);
    b->volBgm = cfg_percent(b, dv, "bgm", 0.5f);
    b->set.attenuation = cfg_percent(b, dv, "attenuated bgm", 0.6f);
    b->volSe = cfg_percent(b, dv, "se", 0.75f);
    b->volBgs = cfg_percent(b, dv, "bgs", 0.5f);
    b->volVoice = cfg_percent(b, dv, "voice", 0.75f);
    b->volBgv = cfg_percent(b, dv, "bgv", 0.5f);
    b->voiceCount = 0;
    for (int i = 1; i <= BX_VOICES; i++) {
        char key[32];
        snprintf(key, sizeof key, "voice%02d", i);
        const char *sig = bx_cfg_get(&b->cfg, "config screen", key);
        if (!sig) break;
        VoiceLevel *v = &b->voices[b->voiceCount++];
        snprintf(v->sig, sizeof v->sig, "%.2s", sig);
        snprintf(key, sizeof key, "voice%02d voice", i);
        v->voice = cfg_percent(b, "config screen", key, 1.0f);
        snprintf(key, sizeof key, "voice%02d bgv", i);
        v->bgv = cfg_percent(b, "config screen", key, 1.0f);
        snprintf(key, sizeof key, "voice%02d mask", i);
        v->mask = bx_cfg_int(&b->cfg, "config screen", key, 0);
    }
    b->otherVoices = (VoiceLevel){"", bx_cfg_int(&b->cfg, "config screen", "voice ex mask", 0x40), 100, 100};
}

typedef struct Layout {
    size_t effects, cease, unread, voiceMode, bgmSelect, autoSelect;
    u8 effectsBit;
} Layout;

static Layout layout_of(BX *b) {
    if (b->sc->minor == 0) return (Layout){0x4c, 0x4d, 0x4e, 0x4f, 0, 0x51, 0};
    return (Layout){0x48, 0x4a, 0x4b, 0x4c, bx_cfg_get(&b->cfg, "config screen", "bgm selector pos0") ? 0x4d : 0, 0x52, 0x80};
}

static size_t common_tail(BX *b) {
    if (!b->common) return 0;
    size_t p = b->readFlags + (size_t)b->sc->msgCount, n = b->commonSize;
    for (int k = 0; k < (b->sc->minor >= 3 ? 4 : 3); k++) {
        if (p + 4 > n) return 0;
        u32 c = rd32(b->common + p);
        size_t len = k == 0 ? 4 * (size_t)c : c;
        if (c > n || p + 4 + len > n) return 0;
        p += 4 + len;
    }
    return p + 40 + 12 * (size_t)b->voiceCount <= n ? p : 0;
}

static float getf(const u8 *p) {
    float f;
    memcpy(&f, p, 4);
    return f;
}

static void setf(u8 *p, float f) { memcpy(p, &f, 4); }

static int percent(const u8 *p) { return clampi((int)lroundf(getf(p) * 100.0f), 0, 100); }

static void settings_from_common(BX *b) {
    size_t tail = common_tail(b);
    if (!tail || b->commonSize < 0x53) return;
    const u8 *d = b->common;
    Layout l = layout_of(b);
    b->set.textSpeed = clampi(100 - percent(d + 0x35), 0, 100);
    b->set.autoSpeed = clampi(100 - percent(d + 0x39), 0, 100);
    b->set.color[0] = d[0x3f];
    b->set.color[1] = d[0x3e];
    b->set.color[2] = d[0x3d];
    b->set.color[3] = d[0x40];
    b->set.noEffects = l.effectsBit ? !(d[l.effects] & l.effectsBit) : !d[l.effects];
    b->set.ceaseSkip = d[l.cease] != 0;
    b->set.skipUnread = d[l.unread] != 0;
    b->set.voiceMode = clampi(d[l.voiceMode], 0, 3);
    b->set.bgmSelect = l.bgmSelect ? d[l.bgmSelect] != 0 : 0;
    b->set.autoSelect = clampi(d[l.autoSelect], 0, 2);
    int *vols[] = {&b->volMaster, &b->volBgm, &b->set.attenuation, &b->volSe, &b->volBgs, &b->volVoice, &b->volBgv};
    for (int i = 0; i < 7; i++) *vols[i] = percent(d + tail + 4 * (size_t)i);
    b->set.voiceMask = (int)rd32(d + tail + 28);
    for (int i = 0; i < b->voiceCount; i++) {
        b->voices[i].voice = percent(d + tail + 32 + 12 * (size_t)i + 4);
        b->voices[i].bgv = percent(d + tail + 32 + 12 * (size_t)i + 8);
    }
    size_t others = tail + 32 + 12 * (size_t)b->voiceCount;
    b->otherVoices.voice = percent(d + others);
    b->otherVoices.bgv = percent(d + others + 4);
}

static void settings_to_common(BX *b) {
    size_t tail = common_tail(b);
    if (!tail || b->commonSize < 0x53) return;
    u8 *d = b->common;
    Layout l = layout_of(b);
    setf(d + 0x35, (float)(100 - clampi(b->set.textSpeed, 0, 100)) / 100.0f);
    setf(d + 0x39, (float)(100 - clampi(b->set.autoSpeed, 0, 100)) / 100.0f);
    d[0x3d] = (u8)clampi(b->set.color[2], 0, 255);
    d[0x3e] = (u8)clampi(b->set.color[1], 0, 255);
    d[0x3f] = (u8)clampi(b->set.color[0], 0, 255);
    d[0x40] = (u8)clampi(b->set.color[3], 0, 255);
    if (l.effectsBit) d[l.effects] = (u8)(b->set.noEffects ? d[l.effects] & ~l.effectsBit : d[l.effects] | l.effectsBit);
    else d[l.effects] = !b->set.noEffects;
    d[l.cease] = b->set.ceaseSkip != 0;
    d[l.unread] = b->set.skipUnread != 0;
    d[l.voiceMode] = (u8)clampi(b->set.voiceMode, 0, 3);
    if (l.bgmSelect) d[l.bgmSelect] = b->set.bgmSelect != 0;
    d[l.autoSelect] = (u8)clampi(b->set.autoSelect, 0, 2);
    int vols[] = {b->volMaster, b->volBgm, b->set.attenuation, b->volSe, b->volBgs, b->volVoice, b->volBgv};
    for (int i = 0; i < 7; i++) setf(d + tail + 4 * (size_t)i, (float)clampi(vols[i], 0, 100) / 100.0f);
    wr32(d + tail + 28, (u32)b->set.voiceMask);
    for (int i = 0; i < b->voiceCount; i++) {
        setf(d + tail + 32 + 12 * (size_t)i + 4, (float)clampi(b->voices[i].voice, 0, 100) / 100.0f);
        setf(d + tail + 32 + 12 * (size_t)i + 8, (float)clampi(b->voices[i].bgv, 0, 100) / 100.0f);
    }
    size_t others = tail + 32 + 12 * (size_t)b->voiceCount;
    setf(d + others, (float)clampi(b->otherVoices.voice, 0, 100) / 100.0f);
    setf(d + others + 4, (float)clampi(b->otherVoices.bgv, 0, 100) / 100.0f);
    b->commonDirty = true;
}

static char *settings_path(BX *b) { return bx_fmt("%s/save/vndegen.cfg", b->saveDir); }

static void settings_from_vndegen_cfg(BX *b, bool all) {
    char *path = settings_path(b);
    size_t n;
    u8 *d = bx_read_file(path, &n);
    free(path);
    if (!d) return;
    Cfg c;
    char *text = bx_fmt("[settings]\n%s", (char *)d);
    bx_cfg_parse(&c, (const u8 *)text, strlen(text));
    free(text);
    free(d);
    b->set.xCount = bx_cfg_int(&c, "settings", "x count", b->set.xCount);
    if (all) {
        struct {
            const char *key;
            int *v;
        } keys[] = {{"text speed", &b->set.textSpeed}, {"auto speed", &b->set.autoSpeed}, {"red", &b->set.color[0]},
                    {"green", &b->set.color[1]}, {"blue", &b->set.color[2]}, {"alpha", &b->set.color[3]},
                    {"skip unread", &b->set.skipUnread}, {"cease skip", &b->set.ceaseSkip}, {"voice mask", &b->set.voiceMask},
                    {"voice mode", &b->set.voiceMode}, {"auto select", &b->set.autoSelect}, {"effects", &b->set.noEffects}};
        for (size_t i = 0; i < sizeof keys / sizeof *keys; i++) *keys[i].v = bx_cfg_int(&c, "settings", keys[i].key, *keys[i].v);
        struct {
            const char *key, *base;
            int *v;
        } vols[] = {{"bgm", "bgm", &b->volBgm}, {"se", "se", &b->volSe}, {"bgs", "bgs", &b->volBgs}, {"bgv", "bgv", &b->volBgv},
                    {"voice", "voice", &b->volVoice}};
        for (size_t i = 0; i < sizeof vols / sizeof *vols; i++) {
            const char *v = bx_cfg_get(&c, "settings", vols[i].key);
            if (v) *vols[i].v = cfg_percent(b, "default volume", vols[i].base, 0.5f) * clampi(atoi(v), 0, 100) / 100;
        }
        b->volMaster = clampi(bx_cfg_int(&c, "settings", "master", b->volMaster), 0, 100);
        if (b->set.voiceMask == 0xff) b->set.voiceMask = -1;
    }
    bx_cfg_free(&c);
}

void bx_settings_load(BX *b) {
    bx_settings_defaults(b);
    settings_from_common(b);
    settings_from_vndegen_cfg(b, b->commonMade);
    if (b->commonMade) settings_to_common(b);
    bx_settings_apply(b);
}

void bx_settings_save(BX *b) {
    settings_to_common(b);
    bx_common_save(b);
    char *text = bx_fmt("x count = %d\n", b->set.xCount);
    char dir[2048];
    snprintf(dir, sizeof dir, "%s/save", b->saveDir);
    bx_mkdirs(dir);
    char *path = settings_path(b);
    bx_write_file(path, text, strlen(text));
    free(path);
    free(text);
}

int bx_voice_level(BX *b, const char *name, bool bgv) {
    const VoiceLevel *v = &b->otherVoices;
    for (int i = 0; name && i < b->voiceCount; i++)
        if (b->voices[i].sig[0] && !strncmp(name, b->voices[i].sig, strlen(b->voices[i].sig))) {
            v = &b->voices[i];
            break;
        }
    return bgv ? v->bgv : v->voice;
}

const char *bx_bgm_track(BX *b, const char *name) {
    static char out[64];
    const char *db = bx_cfg_get(&b->cfg, "main window", "bgmselect db");
    if (!b->set.bgmSelect || !db || !name) return name;
    size_t n;
    u8 *d = bx_res_read(&b->arc, RX_DEF, db, &n);
    if (!d) return name;
    char *text = bx_alloc(n + 1);
    memcpy(text, d, n);
    free(d);
    const char *found = name;
    for (char *p = strchr(text, '['); p; p = strchr(p + 1, '[')) {
        char a[64], c[64];
        if (sscanf(p, "[ \"%63[^\"]\" , \"%63[^\"]\" ]", a, c) == 2 && bx_ieq(a, name)) {
            snprintf(out, sizeof out, "%s", c);
            found = out;
            break;
        }
    }
    free(text);
    return found;
}

VoiceLevel *bx_voice_of(BX *b, int mask) {
    for (int i = 0; i < b->voiceCount; i++)
        if (b->voices[i].mask == mask) return &b->voices[i];
    return &b->otherVoices;
}

static void free_bar(Bar *bar) {
    bx_image_free(bar->img);
    bx_image_free(bar->minus.img);
    bx_image_free(bar->plus.img);
}

void bx_config_free(BX *b) {
    ConfigPhase *c = b->config;
    if (!c) return;
    bx_image_free(c->base);
    bx_image_free(c->check);
    bx_image_free(c->radio);
    for (int i = 0; i < BAR_COUNT; i++) free_bar(&c->bars[i]);
    for (int i = 0; i < 2; i++) free_bar(&c->dssBars[i]);
    for (int i = 0; i < c->charCount; i++) {
        bx_image_free(c->chars[i].img);
        bx_image_free(c->details[i].img);
    }
    CBtn *bs[] = {&c->reset, &c->back, &c->title, &c->quit, &c->dssBack};
    for (int i = 0; i < 5; i++) bx_image_free(bs[i]->img);
    bx_image_free(c->dssBase);
    bx_image_free(c->dssCheck);
    free(c);
    b->config = NULL;
}

static void bar_in(BX *b, const char *sec, Bar *bar, const char *prefix) {
    char key[64];
    snprintf(key, sizeof key, "%s bar", prefix);
    CBtn t;
    if (!cbtn_in(b, sec, key, &t)) return;
    bar->used = true;
    bar->img = t.img;
    bar->x = t.x;
    bar->y = t.y;
    snprintf(key, sizeof key, "%s min", prefix);
    cbtn_in(b, sec, key, &bar->minus);
    snprintf(key, sizeof key, "%s max", prefix);
    cbtn_in(b, sec, key, &bar->plus);
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
    static const char *prefixes[BAR_COUNT] = {"speed0", "speed1", "color0", "color1", "color2", "color3", "volume0", "volume1", "volume2", "volume3", "volume4", "volume5", "volume6"};
    for (int i = 0; i < BAR_COUNT; i++) bar_in(b, sec, &c->bars[i], prefixes[i]);
    c->charCount = clampi(bx_cfg_int(&b->cfg, sec, "characters", 0), 0, 8);
    for (int i = 0; i < c->charCount; i++) {
        char key[32];
        snprintf(key, sizeof key, "character%d", i);
        cbtn(b, key, &c->chars[i]);
        snprintf(key, sizeof key, "character%d detail", i);
        cbtn(b, key, &c->details[i]);
    }
    c->hasBgmSel = bx_cfg_get(&b->cfg, sec, "bgm selector pos0") != NULL;
    xy(b, "bgm selector pos0", c->bgmSel[0], 0, 0);
    xy(b, "bgm selector pos1", c->bgmSel[1], 0, 0);
    const char *dss = "detailed sound setting";
    c->dssBase = own(b, bx_cfg_get(&b->cfg, dss, "base"));
    c->dssCheck = own(b, bx_cfg_get(&b->cfg, dss, "check mark"));
    int play[2] = {0, 0};
    bx_cfg_ints(&b->cfg, dss, "voice play", play, 2);
    c->dssPlay[0] = play[0];
    c->dssPlay[1] = play[1];
    bar_in(b, dss, &c->dssBars[0], "volume0");
    bar_in(b, dss, &c->dssBars[1], "volume1");
    cbtn_in(b, dss, "back button", &c->dssBack);
    c->detail = -1;
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

static int *detail_value(BX *b, ConfigPhase *c, int i) {
    VoiceLevel *v = bx_voice_of(b, 1 << c->detail);
    return i ? &v->bgv : &v->voice;
}

static void set_detail_bar(BX *b, ConfigPhase *c, int i, int mx) {
    Bar *bar = &c->dssBars[i];
    int w = bar->img && bar->img->w > 0 ? bar->img->w : 200;
    *detail_value(b, c, i) = clampi((mx - bar->x) * 100 / w, 0, 100);
}

static bool on_bar(const BX *b, const Bar *bar) {
    return bar->used && bar->img && b->mx >= bar->x && b->mx < bar->x + bar->img->w && b->my >= bar->y && b->my < bar->y + bar->img->h;
}

static void detail_update(BX *b, ConfigPhase *c) {
    if (c->drag >= BAR_COUNT) {
        if (b->lDown) set_detail_bar(b, c, c->drag - BAR_COUNT, b->mx);
        else c->drag = -1;
    }
    if (b->rclick) {
        b->rclick = false;
        c->detail = -1;
        return;
    }
    if (b->lPress)
        for (int i = 0; i < 2; i++)
            if (on_bar(b, &c->dssBars[i])) {
                c->drag = BAR_COUNT + i;
                set_detail_bar(b, c, i, b->mx);
            }
    if (!b->click) return;
    b->click = false;
    for (int i = 0; i < 2; i++) {
        Bar *bar = &c->dssBars[i];
        if (on(&bar->minus, b->mx, b->my)) *detail_value(b, c, i) = clampi(*detail_value(b, c, i) - 10, 0, 100);
        else if (on(&bar->plus, b->mx, b->my)) *detail_value(b, c, i) = clampi(*detail_value(b, c, i) + 10, 0, 100);
    }
    if (near(b->mx, b->my, c->dssPlay)) b->set.voiceMask ^= 1 << c->detail;
    else if (on(&c->dssBack, b->mx, b->my)) c->detail = -1;
}

void bx_config_update(BX *b) {
    ConfigPhase *c = b->config;
    if (!c) return;
    if (c->detail >= 0) {
        detail_update(b, c);
        return;
    }
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
    for (int i = 0; i < c->charCount; i++) {
        if (on(&c->chars[i], b->mx, b->my)) {
            b->set.voiceMask ^= 1 << i;
            return;
        }
        if (on(&c->details[i], b->mx, b->my) && c->dssBase) {
            c->detail = i;
            return;
        }
    }
    for (int i = 0; c->hasBgmSel && i < 2; i++)
        if (near(b->mx, b->my, c->bgmSel[i])) b->set.bgmSelect = i;
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
    for (int i = 0; i < c->charCount; i++) {
        draw(b, dst, &c->chars[i], b->set.voiceMask >> i & 1 ? 0 : 1);
        draw(b, dst, &c->details[i], on(&c->details[i], b->mx, b->my));
    }
    if (c->check) {
        int flags[3] = {b->set.ceaseSkip, b->set.skipUnread, !b->set.noEffects};
        for (int i = 0; i < 3; i++)
            if (flags[i]) bx_blit(dst, b->W, b->H, c->check, c->checks[i][0] + 1, c->checks[i][1] + 1, 255);
    }
    if (c->radio) {
        int a = clampi(b->set.autoSelect, 0, 2), v = clampi(b->set.voiceMode, 0, 3);
        bx_blit(dst, b->W, b->H, c->radio, c->autoSel[a][0], c->autoSel[a][1], 255);
        bx_blit(dst, b->W, b->H, c->radio, c->voiceMode[v][0], c->voiceMode[v][1], 255);
        if (c->hasBgmSel) bx_blit(dst, b->W, b->H, c->radio, c->bgmSel[b->set.bgmSelect != 0][0], c->bgmSel[b->set.bgmSelect != 0][1], 255);
    }
    draw(b, dst, &c->reset, on(&c->reset, b->mx, b->my));
    draw(b, dst, &c->back, on(&c->back, b->mx, b->my));
    draw(b, dst, &c->title, on(&c->title, b->mx, b->my));
    draw(b, dst, &c->quit, on(&c->quit, b->mx, b->my));
    if (c->detail < 0) return;
    bx_blit(dst, b->W, b->H, c->dssBase, c->dssBase->x, c->dssBase->y, 255);
    if (c->dssCheck && b->set.voiceMask >> c->detail & 1) bx_blit(dst, b->W, b->H, c->dssCheck, c->dssPlay[0] + 1, c->dssPlay[1] + 1, 255);
    for (int i = 0; i < 2; i++) {
        Bar *bar = &c->dssBars[i];
        if (!bar->used) continue;
        bx_blit_part(dst, b->W, b->H, bar->img, 0, 0, bar->img->w * *detail_value(b, c, i) / 100, bar->img->h, bar->x, bar->y, 255);
        draw(b, dst, &bar->minus, on(&bar->minus, b->mx, b->my));
        draw(b, dst, &bar->plus, on(&bar->plus, b->mx, b->my));
    }
    draw(b, dst, &c->dssBack, on(&c->dssBack, b->mx, b->my));
}

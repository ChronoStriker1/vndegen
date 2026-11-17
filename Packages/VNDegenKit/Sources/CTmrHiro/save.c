#include "th.h"

#include <time.h>

enum {
    SLOT_SIZE = 43121,
    OFF_TIME = 0,
    OFF_TITLE = 16,
    TITLE_LEN = 128,
    OFF_FLAGS = 656,
    FLAGS_LEN = 100,
    OFF_FRAMES = 756,
    FRAME_SIZE = 144,
    FRAME_NAME = 128,
    FRAME_MAX = 8,
    OFF_LAYERS = 3700,
    LAYER_SIZE = 1230,
    LAYER_COUNT = 15,
    OFF_XFORM = 22150,
    XFORM_SIZE = 128,
    OFF_CHANNELS = 24050,
    CHANNEL_SIZE = 212,
    THUMB_W = 128,
    THUMB_H = 72,
};

static void put_f32(u8 *p, float f) {
    u32 v;
    memcpy(&v, &f, 4);
    wr32(p, v);
}

static float get_f32(const u8 *p) {
    u32 v = rd32(p);
    float f;
    memcpy(&f, &v, 4);
    return f;
}

static void put_str(u8 *p, size_t cap, const char *s) {
    size_t n = strlen(s);
    if (n >= cap) n = cap - 1;
    memcpy(p, s, n);
    p[n] = 0;
}

static void get_str(const u8 *p, size_t cap, char *out, size_t outSize) {
    size_t n = 0;
    while (n < cap && p[n]) n++;
    if (n >= outSize) n = outSize - 1;
    memcpy(out, p, n);
    out[n] = 0;
}

static char *slot_path(TH *t, int slot) { return th_fmt("%s/savedata.%03d", t->saveDir, slot); }

static void put_ones(u8 *d, int off, int count) {
    for (int i = 0; i < count; i++) put_f32(d + off + i * 4, 1.0f);
}

static void slot_defaults(u8 *d) {
    static const struct { int off, count, step, n; } ones[] = {
        {25374, 8, 128, 4}, {25898, 8, 0, 1}, {26026, 8, 128, 19}, {29294, 3, 0, 1}, {29310, 4, 0, 1},
        {29422, 8, 0, 1}, {29550, 3, 0, 1}, {29566, 4, 0, 1}, {29818, 3, 0, 1}, {29834, 4, 0, 1},
        {29946, 8, 128, 19}, {34855, 8, 128, 4}, {35379, 8, 128, 20}, {38707, 8, 128, 4}, {39231, 8, 128, 20},
    };
    for (size_t i = 0; i < sizeof ones / sizeof ones[0]; i++)
        for (int k = 0; k < ones[i].n; k++) put_ones(d, ones[i].off + k * ones[i].step, ones[i].count);
    static const int gauges[] = {29270, 29398, 29526, 29794};
    for (size_t i = 0; i < sizeof gauges / sizeof gauges[0]; i++) {
        put_f32(d + gauges[i], 1024.0f);
        put_f32(d + gauges[i] + 4, 546.0f);
    }
    d[29628] = 0x10;
    d[29896] = 0x10;
    wr16(d + 29650, 500);
    wr16(d + 29918, 500);
    put_f32(d + 29690, -1.0f);
    wr32(d + 34695, 0x14);
    wr32(d + 34699, 0xffffffffu);
    wr32(d + 34703, 0xffffffffu);
    wr32(d + 34711, 0xffffffffu);
    static const u32 order[10] = {4, 3, 2, 1, 0, 5, 6, 7, 8, 9};
    for (int i = 0; i < 10; i++) wr32(d + 34779 + i * 4, order[i]);
    wr16(d + 42587, 0x0810);
    static const int chans[6][3] = {{1, 4, -2251}, {1, 1, -501}, {1, 0, -1001}, {1, 0, -1001}, {1, 0, -1001}, {1, 1, -1001}};
    for (int c = 0; c < 6; c++)
        for (int k = 0; k < 3; k++) wr32(d + 24262 + c * 212 + k * 4, (u32)chans[c][k]);
}

bool save_exists(TH *t, int slot) {
    char *p = slot_path(t, slot);
    char real[2048];
    bool ok = th_path_resolve(p, real, sizeof real) == 1;
    free(p);
    return ok;
}

static int scene_id_of(TH *t, const char *name) {
    Scene *s = scene_load(t, name);
    if (!s) return 0;
    for (int i = 0; i < s->count && i < 8; i++)
        if (s->recs[i].cls == 0x50 && s->recs[i].op == 0x14 && s->recs[i].len >= 2) return rd16(s->recs[i].body);
    return 0;
}

static void write_thumb(TH *t, int slot) {
    size_t stride = (THUMB_W * 3 + 3) & ~(size_t)3;
    size_t size = 54 + stride * THUMB_H;
    u8 *b = th_alloc(size);
    b[0] = 'B';
    b[1] = 'M';
    wr32(b + 2, (u32)size);
    wr32(b + 10, 54);
    wr32(b + 14, 40);
    wr32(b + 18, THUMB_W);
    wr32(b + 22, THUMB_H);
    wr16(b + 26, 1);
    wr16(b + 28, 24);
    wr32(b + 34, (u32)(stride * THUMB_H));
    for (int y = 0; y < THUMB_H; y++) {
        u8 *row = b + 54 + stride * (size_t)(THUMB_H - 1 - y);
        for (int x = 0; x < THUMB_W; x++) {
            u32 r = 0, g = 0, bl = 0, n = 0;
            int x0 = x * t->W / THUMB_W, x1 = (x + 1) * t->W / THUMB_W, y0 = y * t->H / THUMB_H, y1 = (y + 1) * t->H / THUMB_H;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) {
                    u32 p = t->sceneBuf[(size_t)yy * t->W + xx];
                    r += p & 0xff;
                    g += p >> 8 & 0xff;
                    bl += p >> 16 & 0xff;
                    n++;
                }
            if (!n) n = 1;
            row[x * 3] = (u8)(bl / n);
            row[x * 3 + 1] = (u8)(g / n);
            row[x * 3 + 2] = (u8)(r / n);
        }
    }
    char *path = th_fmt("%s/savedata%03d.bmp", t->saveDir, slot);
    th_write_file(path, b, size);
    free(path);
    free(b);
}

bool save_slot(TH *t, int slot) {
    if (!t->sc) return false;
    u8 *d = th_alloc(SLOT_SIZE);
    slot_defaults(d);
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    u16 st[8] = {(u16)(tm.tm_year + 1900), (u16)(tm.tm_mon + 1), (u16)tm.tm_wday, (u16)tm.tm_mday, (u16)tm.tm_hour, (u16)tm.tm_min, (u16)tm.tm_sec, 0};
    for (int i = 0; i < 8; i++) wr16(d + OFF_TIME + i * 2, st[i]);
    put_str(d + OFF_TITLE, TITLE_LEN, t->sceneTitle);
    memset(d + OFF_FLAGS, 1, FLAGS_LEN);
    d[OFF_FLAGS + 1] = 0;
    int resume = t->savePc >= 0 ? t->savePc : t->pc;
    int depth = t->sp < FRAME_MAX - 1 ? t->sp : FRAME_MAX - 1;
    u8 *f = d + OFF_FRAMES;
    put_str(f, FRAME_NAME, t->sc->name);
    wr32(f + FRAME_NAME, (u32)(resume + 1));
    wr32(f + FRAME_NAME + 4, (u32)resume);
    wr32(f + FRAME_NAME + 8, (u32)scene_id_of(t, t->sc->name));
    wr32(f + FRAME_NAME + 12, (u32)depth);
    for (int k = 1; k <= depth; k++) {
        Frame *fr = &t->stack[t->sp - k];
        u8 *g = d + OFF_FRAMES + FRAME_SIZE * k;
        put_str(g, FRAME_NAME, fr->scene);
        wr32(g + FRAME_NAME, (u32)fr->pc);
        wr32(g + FRAME_NAME + 4, (u32)(fr->pc > 0 ? fr->pc - 1 : 0));
        wr32(g + FRAME_NAME + 8, (u32)scene_id_of(t, fr->scene));
        wr32(g + FRAME_NAME + 12, (u32)(depth - k));
    }
    if (t->bgName[0]) {
        put_str(d + OFF_LAYERS, 32, t->bgName);
        u8 *x = d + OFF_XFORM;
        for (int i = 0; i < 2; i++) put_f32(x + 24 + i * 4, 1.0f);
        for (int i = 0; i < 6; i++) put_f32(x + 32 + i * 4, 1.0f);
        Image *bg = t->bg;
        wr16(x + 76, (u16)(bg ? bg->cw : t->W));
        wr16(x + 80, (u16)(bg ? bg->ch : t->H));
    }
    for (int i = 0; i < t->sprCount; i++) {
        Sprite *s = &t->spr[i];
        int layer = s->layer > 0 && s->layer < LAYER_COUNT ? s->layer : TH_MAX_SPRITES - t->sprCount + i;
        u8 *l = d + OFF_LAYERS + LAYER_SIZE * layer;
        put_str(l, 32, s->name);
        if (s->img) wr32(l + 33, (u32)((t->W - s->img->cw) / 2));
        u8 *x = d + OFF_XFORM + XFORM_SIZE * layer;
        put_f32(x, s->x);
        put_f32(x + 4, s->y);
        put_f32(x + 24, s->sx);
        put_f32(x + 28, s->sy);
        for (int k = 0; k < 6; k++) put_f32(x + 32 + k * 4, 1.0f);
        if (layer > 0) wr16(x + 124, 300);
        if (s->img) {
            wr16(x + 76, (u16)s->img->cw);
            wr16(x + 80, (u16)s->img->ch);
        }
    }
    put_str(d + OFF_CHANNELS + 20, 64, t->bgmName);
    if (t->msgVoice[0]) put_str(d + OFF_CHANNELS + CHANNEL_SIZE + 20, 64, t->msgVoice);
    char *path = slot_path(t, slot);
    bool ok = th_write_file(path, d, SLOT_SIZE);
    free(path);
    free(d);
    if (ok) write_thumb(t, slot);
    th_log(t, "save %03d %s at %s:%d", slot, ok ? "written" : "failed", t->sc->name, resume);
    return ok;
}

bool load_slot(TH *t, int slot) {
    char *path = slot_path(t, slot);
    size_t size;
    u8 *d = th_read_file(path, &size);
    free(path);
    if (!d || size < SLOT_SIZE) {
        free(d);
        th_log(t, "load %03d: no save", slot);
        return false;
    }
    char name[FRAME_NAME + 1];
    const u8 *f = d + OFF_FRAMES;
    get_str(f, FRAME_NAME, name, sizeof name);
    int pcCur = (int)rd32(f + FRAME_NAME + 4);
    int depth = (int)rd32(f + FRAME_NAME + 12);
    if (!name[0] || !scene_load(t, name)) {
        th_log(t, "load %03d: unknown scene %s", slot, name);
        free(d);
        return false;
    }
    for (int c = 0; c < CH_COUNT; c++) snd_stop(t, c, 0);
    t->sp = 0;
    for (int k = depth < FRAME_MAX - 1 ? depth : FRAME_MAX - 1; k >= 1; k--) {
        const u8 *g = d + OFF_FRAMES + FRAME_SIZE * k;
        get_str(g, FRAME_NAME, t->stack[t->sp].scene, sizeof t->stack[t->sp].scene);
        t->stack[t->sp].pc = (int)rd32(g + FRAME_NAME);
        t->sp++;
    }
    t->sc = scene_load(t, name);
    snprintf(t->scene, sizeof t->scene, "%s", name);
    t->pc = pcCur;
    get_str(d + OFF_TITLE, TITLE_LEN, t->sceneTitle, sizeof t->sceneTitle);
    char bg[64];
    get_str(d + OFF_LAYERS, 32, bg, sizeof bg);
    gfx_set_bg(t, bg[0] ? bg : "bl", 0);
    gfx_finish_transition(t);
    t->sprCount = 0;
    for (int layer = 1; layer < LAYER_COUNT && t->sprCount < TH_MAX_SPRITES; layer++) {
        char nm[64];
        get_str(d + OFF_LAYERS + LAYER_SIZE * layer, 32, nm, sizeof nm);
        if (!nm[0]) continue;
        const u8 *x = d + OFF_XFORM + XFORM_SIZE * layer;
        Sprite *s = &t->spr[t->sprCount++];
        snprintf(s->name, sizeof s->name, "%s", nm);
        s->layer = layer;
        s->x = get_f32(x);
        s->y = get_f32(x + 4);
        s->sx = get_f32(x + 24);
        s->sy = get_f32(x + 28);
        if (!(s->sx > 0)) s->sx = 1;
        if (!(s->sy > 0)) s->sy = 1;
        s->img = image_get(t, nm);
    }
    t->navi = NULL;
    char bgm[64];
    get_str(d + OFF_CHANNELS + 20, 64, bgm, sizeof bgm);
    t->bgmName[0] = 0;
    if (bgm[0]) {
        snprintf(t->bgmName, sizeof t->bgmName, "%s", bgm);
        snd_play(t, CH_BGM, PAC_BGM, bgm, true, 0);
    }
    t->wait = W_NONE;
    t->winVisible = false;
    t->buttonCount = t->choiceCount = t->mapCount = 0;
    t->sceneDirty = t->dirty = true;
    free(d);
    th_log(t, "load %03d: %s:%d depth %d", slot, name, pcCur, depth);
    return true;
}

void save_system(TH *t) { (void)t; }

void load_system(TH *t) { (void)t; }

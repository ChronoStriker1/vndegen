#include "anim.h"

#include <time.h>

enum {
    SLOT_SIZE = 248216,
    SLOT_COUNT = 180,
    FILE_HEAD = 20,
    OFF_COMMENT = 0x09,
    COMMENT_END = 0x74,
    COMMENT_MAX = 101,
    OFF_PC = 0x74,
    OFF_SCENE_KIND = 0x78,
    OFF_RETURN = 0x7c,
    OFF_BG = 0xa4,
    BG_NAME = 0x40,
    OFF_LAYER_X = 0x1b0,
    OFF_LAYER_NAME = 0x2a0,
    LAYER_NAME = 260,
    SAVED_LAYERS = 10,
    OFF_LAYER_OFFSET = 0xcc8,
    OFF_LAYER_Y = 0xcf0,
    OFF_LAYER_ZOOM = 0xd18,
    OFF_LAYER_ONE = 0x250,
    OFF_LAYER_HANDLES = 0xd68,
    LAYER_HANDLES_END = 0x1218,
    OFF_LAYER_WAIT = 0x1240,
    LAYER_WAIT = 0x64,
    OFF_LAYER_TIMES = 0x1650,
    OFF_LAYER_VARS = 0x1780,
    LAYER_VARS_SIZE = 0x1450,
    VAR_SIZE = 0x68,
    VAR_NAME = 0x64,
    OFF_LOCALS = 0x2fdf0,
    OFF_CAMERA_ON = 0x30f58,
    OFF_CAMERA = 0x30f60,
    OFF_SLOT_X = 0x30f78,
    OFF_SLOT_X_NOW = 0x31018,
    POS_SLOTS = 5,
    OFF_SLOT_ORDER = 0x310b8,
    OFF_SLOT_ORDER2 = 0x310e0,
    OFF_LAYER_SHOWN = 0x31108,
    OFF_LAYER_SCALE3 = 0x32dd8,
    LAYER_SCALE3 = 0x38,
    OFF_EMOTE_WAIT = 0x3b744,
    OFF_EMOTE_WAIT_TIME = 0x3b7a8,
    OFF_EMOTE_SCALE = 0x3b7c8,
    OFF_EMOTE_ON = 0x3b7d0,
    SYS_LAST_SLOT = 0x8,
    SYS_LAST_AUTO = 0xc,
    AUTO_FIRST = 160,
    AUTO_SLOTS = 8,
    SYS_READ = 0x3ec8,
    SYS_READ_MAX = 0x7a000,
};

static char *save_path(AN *a) { return an_fmt("%s/%s_save.dat", a->saveDir, a->base); }
static char *system_path(AN *a) { return an_fmt("%s/%s_system.dat", a->saveDir, a->base); }

static void random_key(u8 key[16]) {
    u32 s = (u32)time(NULL) * 2654435761u;
    for (int i = 0; i < 16; i++) {
        s = s * 1103515245u + 12345u;
        key[i] = (u8)(s >> 16);
    }
}

static u8 *read_plain(const char *path, size_t *size, u8 key[16]) {
    size_t n;
    u8 *raw = an_read_file(path, &n);
    if (!raw) return NULL;
    if (n >= 20 && key) memcpy(key, raw + 4, 16);
    u8 *plain = an_gax_open(raw, n, size);
    free(raw);
    return plain;
}

static bool write_plain(const char *path, const u8 *plain, size_t size, const u8 key[16]) {
    u8 *out = an_alloc(size + 20);
    wr32(out, 0x01000000u);
    memcpy(out + 4, key, 16);
    memcpy(out + 20, plain, size);
    u8 k[16];
    memcpy(k, key, 16);
    an_gax_crypt(k, out + 20, size, true);
    bool ok = an_write_file(path, out, size + 20);
    free(out);
    return ok;
}

static bool saves_load(AN *a) {
    if (a->saves) return true;
    char *p = save_path(a);
    size_t n = 0;
    a->saves = read_plain(p, &n, a->saveKey);
    free(p);
    if (!a->saves || n < (size_t)SLOT_SIZE * SLOT_COUNT) {
        free(a->saves);
        a->saves = an_alloc((size_t)SLOT_SIZE * SLOT_COUNT);
        random_key(a->saveKey);
    }
    return true;
}

static u8 *slot_data(AN *a, int slot) {
    if (slot < 0 || slot >= SLOT_COUNT || !saves_load(a)) return NULL;
    return a->saves + (size_t)slot * SLOT_SIZE;
}

bool an_slot_info(AN *a, int slot, SlotInfo *info) {
    memset(info, 0, sizeof *info);
    const u8 *s = slot_data(a, slot);
    if (!s || rd16(s) != 1) return false;
    info->year = rd16(s + 2);
    info->month = s[4];
    info->day = s[5];
    info->hour = s[6];
    info->minute = s[7];
    info->second = s[8];
    size_t n = 0;
    while (OFF_COMMENT + n < COMMENT_END && s[OFF_COMMENT + n]) n++;
    if (n >= sizeof info->comment) n = sizeof info->comment - 1;
    memcpy(info->comment, s + OFF_COMMENT, n);
    info->comment[n] = 0;
    return true;
}

static void put_name(u8 *dst, size_t cap, const char *s) {
    size_t n = strlen(s);
    if (n >= cap) n = cap - 1;
    memset(dst, 0, cap);
    memcpy(dst, s, n);
}

static void get_name(char *dst, size_t cap, const u8 *src, size_t max) {
    size_t n = 0;
    while (n < max && n + 1 < cap && src[n]) n++;
    memcpy(dst, src, n);
    dst[n] = 0;
}

static void write_thumbnail(AN *a, int slot) {
    enum { TW = 256, TH = 256, IH = 144 };
    u32 *px = an_alloc(sizeof(u32) * TW * TH);
    for (int y = 0; y < IH; y++)
        for (int x = 0; x < TW; x++) {
            u32 r = 0, g = 0, b = 0;
            int sx0 = x * a->W / TW, sx1 = (x + 1) * a->W / TW, sy0 = y * a->H / IH, sy1 = (y + 1) * a->H / IH;
            int count = 0;
            for (int sy = sy0; sy < sy1; sy++)
                for (int sx = sx0; sx < sx1; sx++) {
                    u32 c = a->shot[(size_t)sy * (size_t)a->W + (size_t)sx];
                    r += c & 0xff;
                    g += c >> 8 & 0xff;
                    b += c >> 16 & 0xff;
                    count++;
                }
            if (!count) count = 1;
            px[y * TW + x] = (r / count) | (g / count) << 8 | (b / count) << 16 | 0xff000000u;
        }
    char *dir = an_fmt("%s/cg", a->saveDir);
    an_mkdirs(dir);
    free(dir);
    char *p = an_fmt("%s/cg/pSaveThumb%03d.png", a->saveDir, slot);
    if (!an_png_write(p, px, TW, TH)) an_log(a, "can't write %s", p);
    free(p);
    free(px);
}

void an_capture_shot(AN *a) { memcpy(a->shot, a->fb, sizeof(u32) * (size_t)a->W * (size_t)a->H); }

static const int SLOT_DEFAULT_X[POS_SLOTS] = {220, 430, 640, 850, 1060};
static const u32 SLOT_ORDER[] = {2, 0, 4, 3, 1, 5}, SLOT_ORDER2[] = {1, 4, 0, 3, 2};

static void wrf64(u8 *p, double v) { memcpy(p, &v, 8); }

static double rdf64(const u8 *p) {
    double v;
    memcpy(&v, p, 8);
    return v;
}

static int slot_x(AN *a, int i) { return a->slotX[i] ? a->slotX[i] : i < POS_SLOTS ? SLOT_DEFAULT_X[i] : 640; }

static int layer_drop(AN *a, const Layer *l) { return (int)(an_define_char_scale(&a->def, l->name) * 100.0f + 0.5f); }

static void pc_defaults(AN *a, u8 *s) {
    for (int i = 0; i < SAVED_LAYERS; i++) {
        wrf64(s + OFF_LAYER_ONE + 8 * i, 1.0);
        put_name(s + OFF_LAYER_WAIT + LAYER_WAIT * i, LAYER_WAIT, "sPauseWait01");
        wr32(s + OFF_LAYER_TIMES + 4 * i, i < SAVED_LAYERS / 2 ? 1800 : 2000);
        wr32(s + OFF_LAYER_SHOWN + 4 * i, 1);
    }
    memset(s + OFF_LAYER_HANDLES, 0xff, LAYER_HANDLES_END - OFF_LAYER_HANDLES);
    for (int i = 0; i < POS_SLOTS; i++) {
        wrf64(s + OFF_SLOT_X + 8 * i, slot_x(a, i));
        wrf64(s + OFF_SLOT_X_NOW + 8 * i, slot_x(a, i));
    }
    for (size_t i = 0; i < sizeof SLOT_ORDER / sizeof SLOT_ORDER[0]; i++) wr32(s + OFF_SLOT_ORDER + 4 * i, SLOT_ORDER[i]);
    for (size_t i = 0; i < sizeof SLOT_ORDER2 / sizeof SLOT_ORDER2[0]; i++) wr32(s + OFF_SLOT_ORDER2 + 4 * i, SLOT_ORDER2[i]);
    put_name(s + OFF_EMOTE_WAIT, LAYER_WAIT, "sPauseWait01");
    wr32(s + OFF_EMOTE_WAIT_TIME, 2000);
    wrf64(s + OFF_EMOTE_SCALE, a->emoteScale > 0 ? a->emoteScale : 1.0f);
    wr32(s + OFF_EMOTE_ON, 1);
}

static void fill_slot(AN *a, u8 *s) {
    memset(s, 0, SLOT_SIZE);
    pc_defaults(a, s);
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    s[0] = 1;
    s[2] = (u8)((tm.tm_year + 1900) & 0xff);
    s[3] = (u8)((tm.tm_year + 1900) >> 8);
    s[4] = (u8)(tm.tm_mon + 1);
    s[5] = (u8)tm.tm_mday;
    s[6] = (u8)tm.tm_hour;
    s[7] = (u8)tm.tm_min;
    s[8] = (u8)tm.tm_sec;
    char *comment = a->name ? an_fmt(a->game == GAME_MGSW ? "%s%s" : "%s@n%s", a->name, a->text ? a->text : "") : an_strdup(a->text ? a->text : "");
    char *sjis = an_utf8_to_sjis(comment);
    put_name(s + OFF_COMMENT, COMMENT_MAX, sjis);
    free(sjis);
    free(comment);
    wr32(s + OFF_PC, a->textPc);
    wr32(s + OFF_SCENE_KIND, (u32)a->sceneKind);
    wr32(s + OFF_RETURN, a->th.sp > 0 ? a->th.stack[a->th.sp - 1] : 0);
    put_name(s + OFF_BG, BG_NAME, a->scene.bg);
    if (a->scene.camera) {
        wr32(s + OFF_CAMERA_ON, 1);
        wrf64(s + OFF_CAMERA, a->scene.bgX);
        wrf64(s + OFF_CAMERA + 8, a->scene.bgY);
        wr32(s + OFF_CAMERA + 16, (u32)a->scene.bgScale);
        wr32(s + OFF_CAMERA + 20, a->scene.bgMoveTime);
    }
    for (int i = 0; i < SAVED_LAYERS; i++) {
        Layer *l = &a->scene.layer[i];
        if (l->kind == LK_NONE) continue;
        put_name(s + OFF_LAYER_NAME + LAYER_NAME * i, LAYER_NAME, l->name);
        if (l->idle[0]) put_name(s + OFF_LAYER_WAIT + LAYER_WAIT * i, LAYER_WAIT, l->idle);
        wr32(s + OFF_LAYER_X + 4 * i, 640);
        wr32(s + OFF_LAYER_OFFSET + 4 * i, (u32)((a->slotX[i] ? a->slotX[i] : 640) - 640 + l->toX));
        for (int k = 0; k < 3; k++) wrf64(s + OFF_LAYER_SCALE3 + LAYER_SCALE3 * i + 8 * k, 1.0);
        if (l->scale != 1000) wr32(s + OFF_LAYER_ZOOM + 4 * i, (u32)l->scale);
        if (l->kind != LK_EMOTE) continue;
        wr32(s + OFF_LAYER_Y + 4 * i, (u32)(l->toY + layer_drop(a, l)));
        for (int k = 0; k < l->varCount && (k + 1) * VAR_SIZE <= LAYER_VARS_SIZE; k++) {
            u8 *v = s + OFF_LAYER_VARS + LAYER_VARS_SIZE * i + VAR_SIZE * k;
            put_name(v, VAR_NAME, l->vars[k].name);
            memcpy(v + VAR_NAME, &l->vars[k].value, 4);
        }
    }
    for (int i = 0; i < AN_VARS; i++) wr32(s + OFF_LOCALS + 4 * i, (u32)a->L[i]);
}

static bool write_saves(AN *a) {
    char *p = save_path(a);
    bool ok = write_plain(p, a->saves, (size_t)SLOT_SIZE * SLOT_COUNT, a->saveKey);
    free(p);
    return ok;
}

static void clear_slot(AN *a, int slot) { memset(slot_data(a, slot), 0, SLOT_SIZE); }

bool an_delete_slot(AN *a, int slot) {
    if (!slot_data(a, slot)) return false;
    clear_slot(a, slot);
    bool ok = write_saves(a);
    an_log(a, "deleted slot %d: %s", slot, ok ? "ok" : "failed");
    return ok;
}

bool an_reset_saves(AN *a) {
    if (!saves_load(a)) return false;
    for (int slot = 0; slot < SLOT_COUNT; slot++) clear_slot(a, slot);
    bool ok = write_saves(a);
    an_log(a, "reset saves: %s", ok ? "ok" : "failed");
    return ok;
}

int an_last_auto(const AN *a) { return a->sys && a->sysSize >= SYS_LAST_AUTO + 4 ? (int)(i32)rd32(a->sys + SYS_LAST_AUTO) : -1; }

void an_auto_save(AN *a, u32 pc) {
    if (!a->sys || a->sysSize < SYS_LAST_AUTO + 4) return;
    i32 last = (i32)rd32(a->sys + SYS_LAST_AUTO);
    int slot = last >= AUTO_FIRST && last < AUTO_FIRST + AUTO_SLOTS - 1 ? last + 1 : AUTO_FIRST;
    u8 *s = slot_data(a, slot);
    if (!s || a->textPc == 0) return;
    an_capture_shot(a);
    u32 textPc = a->textPc;
    fill_slot(a, s);
    wr32(s + OFF_PC, pc);
    a->textPc = textPc;
    if (!write_saves(a)) return;
    write_thumbnail(a, slot);
    wr32(a->sys + SYS_LAST_AUTO, (u32)slot);
    an_system_save(a);
    an_log(a, "auto saved slot %d at %x", slot, pc);
}

bool an_save_slot(AN *a, int slot) {
    u8 *s = slot_data(a, slot);
    if (!s || a->textPc == 0) return false;
    fill_slot(a, s);
    bool ok = write_saves(a);
    if (ok) {
        write_thumbnail(a, slot);
        a->lastSlot = slot;
        an_system_save(a);
    }
    an_log(a, "saved slot %d at %x: %s", slot, a->textPc, ok ? "ok" : "failed");
    return ok;
}

static bool restore_slot(AN *a, const u8 *s) {
    if (!s || rd16(s) != 1) return false;
    u32 pc = rd32(s + OFF_PC);
    if (pc < 8 || pc >= a->sc->codeEnd) return false;
    an_reset_scene(a);
    get_name(a->scene.bg, sizeof a->scene.bg, s + OFF_BG, BG_NAME);
    a->scene.bgScale = 1000;
    if (rd32(s + OFF_CAMERA_ON) == 1) {
        i32 scale = (i32)rd32(s + OFF_CAMERA + 16);
        a->scene.camera = true;
        a->scene.bgX = a->scene.bgFromX = (int)rdf64(s + OFF_CAMERA);
        a->scene.bgY = a->scene.bgFromY = (int)rdf64(s + OFF_CAMERA + 8);
        a->scene.bgScale = scale > 0 ? scale : 1000;
    }
    for (int i = 0; i < SAVED_LAYERS; i++) {
        Layer *l = &a->scene.layer[i];
        get_name(l->name, sizeof l->name, s + OFF_LAYER_NAME + LAYER_NAME * i, LAYER_NAME);
        get_name(l->idle, sizeof l->idle, s + OFF_LAYER_WAIT + LAYER_WAIT * i, LAYER_WAIT);
        if (!l->name[0]) continue;
        size_t n = strlen(l->name);
        l->kind = n > 4 && an_ieq(l->name + n - 4, ".psb") ? LK_EMOTE : LK_IMAGE;
        l->alpha = l->fromAlpha = l->toAlpha = 255;
        i32 zoom = (i32)rd32(s + OFF_LAYER_ZOOM + 4 * i);
        l->scale = zoom > 0 ? zoom : 1000;
        double base = i < POS_SLOTS ? rdf64(s + OFF_SLOT_X + 8 * i) : 0;
        a->slotX[i] = base > 0 && base < 4096 ? (int)base : 640;
        l->toX = l->fromX = (i32)rd32(s + OFF_LAYER_OFFSET + 4 * i) - (a->slotX[i] - 640);
        if (l->kind == LK_EMOTE) l->toY = l->fromY = (i32)rd32(s + OFF_LAYER_Y + 4 * i) - layer_drop(a, l);
        for (int k = 0; k * VAR_SIZE + VAR_SIZE <= LAYER_VARS_SIZE; k++) {
            const u8 *v = s + OFF_LAYER_VARS + LAYER_VARS_SIZE * i + VAR_SIZE * k;
            if (!v[0]) break;
            char name[32];
            get_name(name, sizeof name, v, VAR_NAME);
            float value;
            memcpy(&value, v + VAR_NAME, 4);
            an_layer_set_var(l, name, value);
        }
        l->varSerial++;
        l->fresh = true;
    }
    for (int i = 0; i < AN_VARS; i++) a->L[i] = (i32)rd32(s + OFF_LOCALS + 4 * i);
    a->sceneKind = (int)rd32(s + OFF_SCENE_KIND);
    double scale = rdf64(s + OFF_EMOTE_SCALE);
    if (scale > 0.05 && scale < 10) a->emoteScale = (float)scale;
    a->phase = PH_GAME;
    a->screen = SCR_NONE;
    memset(&a->th, 0, sizeof a->th);
    a->th.pc = pc;
    a->scene.window = true;
    return true;
}

bool an_load_slot(AN *a, int slot) {
    bool ok = restore_slot(a, slot_data(a, slot));
    an_log(a, "loaded slot %d: %s", slot, ok ? "ok" : "failed");
    return ok;
}

static char *quick_path(AN *a) { return an_fmt("%s/qSavedata.dat", a->saveDir); }

bool an_quick_save(AN *a) {
    if (a->textPc == 0) return false;
    u8 *s = an_alloc(SLOT_SIZE);
    fill_slot(a, s);
    u8 key[16];
    random_key(key);
    char *p = quick_path(a);
    bool ok = write_plain(p, s, SLOT_SIZE, key);
    free(p);
    free(s);
    an_log(a, "quick save at %x: %s", a->textPc, ok ? "ok" : "failed");
    return ok;
}

bool an_quick_load(AN *a) {
    char *p = quick_path(a);
    size_t n = 0;
    u8 *s = read_plain(p, &n, NULL);
    free(p);
    bool ok = s && n >= SLOT_SIZE && restore_slot(a, s);
    free(s);
    an_log(a, "quick load: %s", ok ? "ok" : "failed");
    return ok;
}

enum { SYS_CG = 0x25c8, SYS_CG_MAX = 1200, SYS_MEMORY = 0x3888, SYS_MEMORY_MAX = 200 };

static u8 *sys_flag(const AN *a, u32 base, int i, int max) {
    if (!a->sys || i < 0 || i >= max || a->sysSize < base + 4u * (u32)max) return NULL;
    return a->sys + base + 4u * (u32)i;
}

bool an_cg_unlocked(const AN *a, int id) {
    const u8 *p = sys_flag(a, SYS_CG, id, SYS_CG_MAX);
    return p && rd32(p) != 0;
}

void an_cg_unlock(AN *a, const char *name) {
    if (!name || !*name) return;
    for (int i = 0; i < a->def.cgCount; i++)
        if (an_ieq(a->def.cg[i], name)) {
            u8 *p = sys_flag(a, SYS_CG, i, SYS_CG_MAX);
            if (p) wr32(p, 1);
            return;
        }
}

bool an_memory_unlocked(const AN *a, int n) {
    const u8 *p = sys_flag(a, SYS_MEMORY, n, SYS_MEMORY_MAX);
    return p && rd32(p) != 0;
}

void an_memory_unlock(AN *a, int n) {
    u8 *p = sys_flag(a, SYS_MEMORY, n, SYS_MEMORY_MAX);
    if (p) wr32(p, 1);
}

typedef struct OptionByte {
    u32 at;
    u8 first, second;
} OptionByte;

static const OptionByte OPTION_BYTES[GAME_COUNT][11] = {
    [GAME_DDD] =
        {
            [OPT_SCREEN] = {0, 0, 0},           [OPT_EFFECTS] = {0x1, 0, 2},       [OPT_SKIP_UNREAD] = {0x2, 1, 0},
            [OPT_KEEP_SKIP] = {0x3, 1, 0},      [OPT_EVENT_MOVIES] = {0x6, 1, 0},  [OPT_HAIR] = {0x1668, 1, 0},
            [OPT_SKIP_VOICE] = {0x7e524, 0, 1}, [OPT_HISTORY] = {0x7e520, 0, 1},  [OPT_CONFIRM] = {0x5, 1, 0},
            [OPT_VOICE_BAR] = {0x7e528, 1, 0},  [OPT_AUTO_BAR] = {0x7e52c, 1, 0},
        },
    [GAME_MGSW] =
        {
            [OPT_SCREEN] = {0, 0, 0},           [OPT_EFFECTS] = {0x1, 0, 2},       [OPT_SKIP_UNREAD] = {0x2, 1, 0},
            [OPT_KEEP_SKIP] = {0x3, 1, 0},      [OPT_EVENT_MOVIES] = {0x6, 1, 0},  [OPT_HAIR] = {0, 0, 0},
            [OPT_SKIP_VOICE] = {0x7e524, 0, 1}, [OPT_HISTORY] = {0x7e520, 0, 1},  [OPT_CONFIRM] = {0x5, 1, 0},
            [OPT_VOICE_BAR] = {0x7e528, 1, 0},  [OPT_AUTO_BAR] = {0x7e52c, 1, 0},
        },
};

enum { SYS_VOLUME = 0x18, SYS_TEXT_SPEED = 0x27c, SYS_AUTO_SPEED = 0x280, SYS_OPACITY = 0x298, SYS_SETTINGS_END = 0x7e530 };
static const u32 VOLUME_BYTES[5] = {0x18, 0x19, 0x1b, 0x1e, 0x1f};

static void settings_read(AN *a) {
    if (!a->sys || a->sysSize < SYS_SETTINGS_END) return;
    const u8 *s = a->sys;
    for (int i = 0; i < 11; i++) {
        const OptionByte *o = &OPTION_BYTES[a->game][i];
        if (o->first == o->second) continue;
        if (s[o->at] == o->first) a->option[i] = 0;
        else if (s[o->at] == o->second) a->option[i] = 1;
    }
    for (int i = 0; i < 5; i++) a->volume[i] = clampi(s[VOLUME_BYTES[i]], 0, 100);
    const VoiceTabs *t = an_voice_tabs(a);
    for (int c = 0; c < t->count; c++) {
        a->voiceVolume[c] = clampi(s[t->voiceAt + (u32)c], 0, 100);
        if (c == t->count - 1) continue;
        a->bgvVolume[c] = clampi(s[t->bgvAt + (u32)c], 0, 100);
        a->talkVolume[c] = clampi(s[t->talkAt + (u32)c], 0, 100);
    }
    a->windowOpacity = clampi((int)rd32(s + SYS_OPACITY) * 100 / 217, 0, 100);
    a->textSpeed = clampi((200 - (int)rd32(s + SYS_TEXT_SPEED)) * 100 / 172, 0, 100);
    a->autoSpeed = clampi((50 - (int)rd32(s + SYS_AUTO_SPEED)) * 100 / 43, 0, 100);
}

static void settings_write(AN *a) {
    if (!a->sys || a->sysSize < SYS_SETTINGS_END) return;
    u8 *s = a->sys;
    for (int i = 0; i < 11; i++) {
        const OptionByte *o = &OPTION_BYTES[a->game][i];
        if (o->first != o->second) s[o->at] = a->option[i] ? o->second : o->first;
    }
    for (int i = 0; i < 5; i++) s[VOLUME_BYTES[i]] = (u8)clampi(a->volume[i], 0, 100);
    const VoiceTabs *t = an_voice_tabs(a);
    for (int c = 0; c < t->count; c++) {
        s[t->voiceAt + (u32)c] = (u8)clampi(a->voiceVolume[c], 0, 100);
        if (c == t->count - 1) continue;
        s[t->bgvAt + (u32)c] = (u8)clampi(a->bgvVolume[c], 0, 100);
        s[t->talkAt + (u32)c] = (u8)clampi(a->talkVolume[c], 0, 100);
    }
    wr32(s + SYS_OPACITY, (u32)(a->windowOpacity * 217 / 100));
    wr32(s + SYS_TEXT_SPEED, (u32)(200 - a->textSpeed * 172 / 100));
    wr32(s + SYS_AUTO_SPEED, (u32)(50 - a->autoSpeed * 43 / 100));
}

void an_system_load(AN *a) {
    char *p = system_path(a);
    size_t n = 0;
    free(a->sys);
    a->sys = read_plain(p, &n, a->sysKey);
    free(p);
    a->sysSize = n;
    if (!a->sys || n < SYS_READ_MAX) {
        free(a->sys);
        a->sys = NULL;
        a->sysSize = 0;
        return;
    }
    a->lastSlot = (int)rd32(a->sys + SYS_LAST_SLOT);
    settings_read(a);
}

void an_system_save(AN *a) {
    if (!a->sys) return;
    wr32(a->sys + SYS_LAST_SLOT, (u32)a->lastSlot);
    settings_write(a);
    char *p = system_path(a);
    write_plain(p, a->sys, a->sysSize, a->sysKey);
    free(p);
}

void an_mark_read(AN *a, int id) {
    if (a->sys && id >= 0 && SYS_READ + (size_t)id < SYS_READ_MAX) a->sys[SYS_READ + id] = 1;
}

bool an_is_read(AN *a, int id) { return a->sys && id >= 0 && SYS_READ + (size_t)id < SYS_READ_MAX && a->sys[SYS_READ + id]; }

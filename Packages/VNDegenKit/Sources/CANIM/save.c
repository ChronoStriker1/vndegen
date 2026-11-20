#include "anim.h"

#include <time.h>

enum {
    SLOT_SIZE = 248216,
    SLOT_COUNT = 180,
    FILE_HEAD = 20,
    OFF_COMMENT = 0x09,
    COMMENT_END = 0x74,
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
    OFF_LAYER_SCALE = 0xcf0,
    OFF_LOCALS = 0x2fdf0,
    SYS_LAST_SLOT = 0x8,
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
    char *p = an_fmt("%s/cg/pSaveThumb%03d.png", a->saveDir, slot);
    an_png_write(p, px, TW, TH);
    free(p);
    free(px);
}

void an_capture_shot(AN *a) { memcpy(a->shot, a->fb, sizeof(u32) * (size_t)a->W * (size_t)a->H); }

static void fill_slot(AN *a, u8 *s) {
    memset(s, 0, SLOT_SIZE);
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
    char *comment = a->name ? an_fmt("%s@n%s", a->name, a->text ? a->text : "") : an_strdup(a->text ? a->text : "");
    put_name(s + OFF_COMMENT, COMMENT_END - OFF_COMMENT, comment);
    free(comment);
    wr32(s + OFF_PC, a->textPc);
    wr32(s + OFF_SCENE_KIND, (u32)a->sceneKind);
    wr32(s + OFF_RETURN, a->th.sp > 0 ? a->th.stack[a->th.sp - 1] : 0);
    put_name(s + OFF_BG, BG_NAME, a->scene.bg);
    for (int i = 0; i < SAVED_LAYERS; i++) {
        Layer *l = &a->scene.layer[i];
        if (l->kind == LK_NONE) continue;
        put_name(s + OFF_LAYER_NAME + LAYER_NAME * i, LAYER_NAME, l->name);
        wr32(s + OFF_LAYER_X + 4 * i, 640);
        int base = a->slotX[i] ? a->slotX[i] : 640;
        wr32(s + OFF_LAYER_OFFSET + 4 * i, (u32)(base - 640 + l->toX));
        if (l->kind == LK_EMOTE) wr32(s + OFF_LAYER_SCALE + 4 * i, (u32)(an_define_char_scale(&a->def, l->name) * 100.0f + 0.5f));
    }
    for (int i = 0; i < AN_VARS; i++) wr32(s + OFF_LOCALS + 4 * i, (u32)a->L[i]);
}

bool an_save_slot(AN *a, int slot) {
    u8 *s = slot_data(a, slot);
    if (!s || a->textPc == 0) return false;
    fill_slot(a, s);
    char *p = save_path(a);
    bool ok = write_plain(p, a->saves, (size_t)SLOT_SIZE * SLOT_COUNT, a->saveKey);
    free(p);
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
    for (int i = 0; i < SAVED_LAYERS; i++) {
        Layer *l = &a->scene.layer[i];
        get_name(l->name, sizeof l->name, s + OFF_LAYER_NAME + LAYER_NAME * i, LAYER_NAME);
        if (!l->name[0]) continue;
        size_t n = strlen(l->name);
        l->kind = n > 4 && an_ieq(l->name + n - 4, ".psb") ? LK_EMOTE : LK_IMAGE;
        l->alpha = l->fromAlpha = l->toAlpha = 255;
        l->scale = 1000;
        a->slotX[i] = 640;
        l->toX = l->fromX = (i32)rd32(s + OFF_LAYER_OFFSET + 4 * i);
    }
    for (int i = 0; i < AN_VARS; i++) a->L[i] = (i32)rd32(s + OFF_LOCALS + 4 * i);
    a->sceneKind = (int)rd32(s + OFF_SCENE_KIND);
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
}

void an_system_save(AN *a) {
    if (!a->sys) return;
    wr32(a->sys + SYS_LAST_SLOT, (u32)a->lastSlot);
    char *p = system_path(a);
    write_plain(p, a->sys, a->sysSize, a->sysKey);
    free(p);
}

void an_mark_read(AN *a, int id) {
    if (a->sys && id >= 0 && SYS_READ + (size_t)id < SYS_READ_MAX) a->sys[SYS_READ + id] = 1;
}

bool an_is_read(AN *a, int id) { return a->sys && id >= 0 && SYS_READ + (size_t)id < SYS_READ_MAX && a->sys[SYS_READ + id]; }

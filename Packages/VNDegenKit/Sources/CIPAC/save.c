#include "ipac.h"

#include <sys/stat.h>
#include <time.h>

#define RAW_SIZE 6952204
#define SYS_SIZE 5510796
#define O_SIZE 0x30
#define O_NAME 0x31
#define O_TEXT 0x46
#define O_SCENE 0x146
#define O_TITLE 0x15f
#define O_SCRIPT 0x19f
#define O_POS 0x1c0
#define O_BG 0x1c5
#define O_RULE 0x1e5
#define O_SCALE 0x230
#define O_SPEED 0x23e
#define O_SPRITE 0x24a
#define O_SLOTX 0x2aa
#define O_BGMFLAG 0x598
#define O_BGM 0x599
#define O_VOICE 0x59b
#define O_FLAGS 0x5bb
#define O_LOG 0xa44
#define O_LINE 0x541648
#define O_OBJ 0x54168c
#define OBJ_STRIDE 0x58
#define LOG_MAX (O_LINE - O_LOG - 4)

static const int spriteOrder[SP_COUNT] = {SP_C, SP_R, SP_L};

static char *slot_path(IP *t, int slot) { return ip_fmt("%s/_DAT%04d.DAT", t->saveDir, slot); }

bool save_exists(IP *t, int slot) {
    char *p = slot_path(t, slot);
    bool ok = ip_file_exists(p);
    free(p);
    return ok;
}

static u8 *read_packed(const char *path, size_t want) {
    size_t n;
    u8 *d = ip_read_file(path, &n);
    if (!d) return NULL;
    u8 *raw = NULL;
    if (n >= 8 && !memcmp(d, "IEL1", 4)) {
        size_t full = rd32(d + 4);
        raw = lzss_decode(d + 8, n - 8, want && want < full ? want : full);
    } else if (n >= want) {
        raw = d;
        d = NULL;
    }
    free(d);
    return raw;
}

bool save_info(IP *t, int slot, SlotInfo *info) {
    char *p = slot_path(t, slot);
    struct stat st;
    u8 *raw = stat(p, &st) == 0 ? read_packed(p, 0x200) : NULL;
    free(p);
    if (!raw) return false;
    memset(info, 0, sizeof *info);
    snprintf(info->scene, sizeof info->scene, "%.*s", O_TITLE - O_SCENE, (const char *)raw + O_SCENE);
    free(raw);
    time_t mt = st.st_mtime;
    struct tm tm;
    localtime_r(&mt, &tm);
    info->month = tm.tm_mon + 1;
    info->day = tm.tm_mday;
    info->hour = tm.tm_hour;
    info->minute = tm.tm_min;
    return true;
}

static void put_str(u8 *raw, size_t off, size_t max, const char *s) {
    size_t n = s ? strlen(s) : 0;
    if (n >= max) n = max - 1;
    if (n) memcpy(raw + off, s, n);
    raw[off + n] = 0;
}

static void get_str(const u8 *raw, size_t off, size_t max, char *out, size_t outSize) {
    size_t n = 0;
    while (n < max && raw[off + n]) n++;
    if (n >= outSize) n = outSize - 1;
    memcpy(out, raw + off, n);
    out[n] = 0;
}

static void put_f32(u8 *p, float v) {
    u32 u;
    memcpy(&u, &v, 4);
    wr32(p, u);
}

static float get_f32(const u8 *p) {
    u32 u = rd32(p);
    float v;
    memcpy(&v, &u, 4);
    return v;
}

static size_t put_log(IP *t, u8 *raw) {
    size_t o = O_LOG + 4;
    int first = 0;
    for (;;) {
        size_t need = 0;
        for (int i = first; i < t->logCount; i++) {
            LogEntry *e = &t->logs[i];
            need += 1 + (e->voice[0] ? strlen(e->voice) + 2 : 0) + (e->name ? strlen(e->name) : 0) + 1 + strlen(e->text) + 1;
        }
        if (need <= LOG_MAX || first >= t->logCount) break;
        first++;
    }
    for (int i = first; i < t->logCount; i++) {
        LogEntry *e = &t->logs[i];
        raw[o++] = 0xff;
        if (e->voice[0]) {
            raw[o++] = '#';
            size_t n = strlen(e->voice);
            memcpy(raw + o, e->voice, n);
            o += n;
            raw[o++] = 0;
        }
        size_t n = e->name ? strlen(e->name) : 0;
        if (n) memcpy(raw + o, e->name, n);
        o += n;
        raw[o++] = 0;
        n = strlen(e->text);
        memcpy(raw + o, e->text, n);
        o += n;
        raw[o++] = 0;
    }
    wr32(raw + O_LOG, (u32)(o - O_LOG - 4));
    return o;
}

static void get_log(IP *t, const u8 *raw) {
    for (int i = 0; i < t->logCount; i++) {
        free(t->logs[i].name);
        free(t->logs[i].text);
    }
    t->logCount = 0;
    size_t len = rd32(raw + O_LOG), o = O_LOG + 4, end = O_LOG + 4 + (len < LOG_MAX ? len : LOG_MAX);
    while (o < end && raw[o] == 0xff) {
        o++;
        char voice[32] = "";
        if (raw[o] == '#') {
            o++;
            get_str(raw, o, 31, voice, sizeof voice);
            o += strlen((const char *)raw + o) + 1;
        }
        const char *name = (const char *)raw + o;
        o += strlen(name) + 1;
        const char *text = (const char *)raw + o;
        o += strlen(text) + 1;
        t->logs = ip_realloc(t->logs, sizeof(LogEntry) * (size_t)(t->logCount + 1));
        LogEntry *e = &t->logs[t->logCount++];
        e->name = *name ? ip_strdup(name) : NULL;
        e->text = ip_strdup(text);
        snprintf(e->voice, sizeof e->voice, "%s", voice);
    }
}

static bool write_packed(IP *t, const char *path, const u8 *raw, size_t n) {
    size_t zn;
    u8 *z = lzss_encode(raw, n, &zn);
    u8 *file = ip_alloc(zn + 8);
    memcpy(file, "IEL1", 4);
    wr32(file + 4, (u32)n);
    memcpy(file + 8, z, zn);
    free(z);
    ip_mkdirs(t->saveDir);
    bool ok = ip_write_file(path, file, zn + 8);
    free(file);
    return ok;
}

bool save_slot(IP *t, int slot) {
    if (!t->sc || !t->msgText) return false;
    u8 *raw = ip_alloc(RAW_SIZE);
    raw[O_SIZE] = (u8)t->pitch;
    put_str(raw, O_NAME, O_TEXT - O_NAME, t->msgName);
    put_str(raw, O_TEXT, O_SCENE - O_TEXT, t->msgRaw);
    put_str(raw, O_SCENE, O_TITLE - O_SCENE, t->sceneName);
    put_str(raw, O_TITLE, O_SCRIPT - O_TITLE, t->title);
    put_str(raw, O_SCRIPT, O_POS - O_SCRIPT, t->sc->name);
    wr32(raw + O_POS, (u32)t->msgOff);
    put_str(raw, O_BG, O_RULE - O_BG, t->bgName);
    put_str(raw, O_RULE, 0x20, t->ruleName[0] ? t->ruleName : "!");
    put_f32(raw + O_SCALE, 800);
    put_f32(raw + O_SCALE + 4, 600);
    wr16(raw + O_SPEED, (u16)t->speed);
    raw[O_SPEED + 3] = 1;
    for (int k = 0; k < SP_COUNT; k++) {
        Sprite *s = &t->spr[spriteOrder[k]];
        put_str(raw, O_SPRITE + (size_t)k * 0x20, 0x20, s->img && s->name[0] ? s->name : "!");
    }
    static const u16 slotX[6] = {400, 600, 200, 600, 600, 600};
    for (int k = 0; k < 6; k++) wr16(raw + O_SLOTX + k * 2, slotX[k]);
    raw[O_BGMFLAG] = 1;
    raw[O_BGM] = (u8)(t->bgmNumber ? t->bgmNumber + 100 : 255);
    put_str(raw, O_VOICE, 0x20, t->msgVoice);
    for (int i = 0; i < IP_VARS; i++) raw[O_FLAGS + i] = (u8)t->vf[i];
    put_log(t, raw);
    wr32(raw + O_LINE, (u32)t->msgLine);
    for (int i = 0; i < IP_OBJS; i++) {
        Obj *o = &t->obj[i];
        if (!o->img) continue;
        u8 *r = raw + O_OBJ + (size_t)i * OBJ_STRIDE;
        r[0] = 1;
        r[1] = o->vis ? 1 : 0;
        put_f32(r + 4, (float)o->x);
        put_f32(r + 8, (float)o->y);
        put_f32(r + 12, (float)o->alpha);
        put_f32(r + 16, (float)o->mx);
        put_f32(r + 20, (float)o->my);
        put_f32(r + 24, (float)o->malpha);
        put_str(r, 0x38, OBJ_STRIDE - 0x38, o->name);
    }
    char *p = slot_path(t, slot);
    bool ok = write_packed(t, p, raw, RAW_SIZE);
    if (!ok) ip_log(t, "can't write %s", p);
    free(p);
    free(raw);
    if (ok) save_system(t);
    return ok;
}

bool load_slot(IP *t, int slot) {
    char *p = slot_path(t, slot);
    u8 *raw = read_packed(p, RAW_SIZE);
    free(p);
    if (!raw) return false;
    char script[64], name[64];
    get_str(raw, O_SCRIPT, O_POS - O_SCRIPT, script, sizeof script);
    Script *sc = script_load(t, script);
    if (!sc) {
        ip_log(t, "save %d names a missing script", slot);
        free(raw);
        return false;
    }
    game_to_title(t);
    t->sc = sc;
    int pos = (int)rd32(raw + O_POS);
    t->pc = sc->count - 1;
    for (int i = 0; i < sc->count; i++)
        if (sc->st[i].off >= pos) {
            t->pc = i;
            break;
        }
    t->inGame = true;
    t->menuEnabled = true;
    for (int i = 0; i < IP_VARS; i++) t->vf[i] = raw[O_FLAGS + i];
    get_str(raw, O_SCENE, O_TITLE - O_SCENE, t->sceneName, sizeof t->sceneName);
    get_str(raw, O_BG, O_RULE - O_BG, name, sizeof name);
    snprintf(t->bgName, sizeof t->bgName, "%s", name[0] != '!' ? name : "");
    t->bg = t->bgName[0] ? image_get(t, PK_BG, t->bgName) : NULL;
    for (int k = 0; k < SP_COUNT; k++) {
        get_str(raw, O_SPRITE + (size_t)k * 0x20, 0x20, name, sizeof name);
        gfx_set_sprite(t, spriteOrder[k], name[0] && name[0] != '!' ? name : NULL);
    }
    get_str(raw, O_RULE, 0x20, name, sizeof name);
    snprintf(t->ruleName, sizeof t->ruleName, "%s", name[0] != '!' ? name : "");
    int sp = rd16(raw + O_SPEED);
    t->speed = sp > 0 ? sp : 4;
    for (int i = 0; i < IP_OBJS; i++) {
        const u8 *r = raw + O_OBJ + (size_t)i * OBJ_STRIDE;
        if (!r[0]) continue;
        Obj *o = &t->obj[i];
        get_str(r, 0x38, OBJ_STRIDE - 0x38, o->name, sizeof o->name);
        o->img = image_get(t, PK_PARTS, o->name);
        o->vis = r[1] != 0;
        o->x = (int)get_f32(r + 4);
        o->y = (int)get_f32(r + 8);
        o->alpha = clampi((int)get_f32(r + 12), 0, 255);
        o->mx = (int)get_f32(r + 16);
        o->my = (int)get_f32(r + 20);
        o->malpha = (int)get_f32(r + 24);
    }
    get_log(t, raw);
    get_str(raw, O_VOICE, 0x20, name, sizeof name);
    if (name[0] && sc->st[t->pc].kind != S_VOICE) snprintf(t->voice, sizeof t->voice, "%s", name);
    int bgm = raw[O_BGM];
    if (bgm != 255 && bgm) snd_bgm(t, bgm, 0);
    gfx_apply_now(t);
    t->wait = W_NONE;
    free(raw);
    return true;
}

static char *sys_path(IP *t) { return ip_fmt("%s/_SYSDAT.DAT", t->saveDir); }

void load_system(IP *t) {
    char *p = sys_path(t);
    size_t n;
    u8 *d = ip_read_file(p, &n);
    free(p);
    if (!d || n < O_FLAGS + IP_VARS) {
        free(d);
        return;
    }
    for (int i = 0; i < IP_VARS; i++) t->vw[i] = d[O_FLAGS + i];
    free(d);
}

void save_system(IP *t) {
    char *p = sys_path(t);
    size_t n;
    u8 *d = ip_read_file(p, &n);
    if (!d || n != SYS_SIZE) {
        free(d);
        d = ip_alloc(SYS_SIZE);
        static const u8 head[8] = {0, 1, 1, 1, 0, 0x80, 0x80, 0x80};
        memcpy(d, head, 8);
        memset(d + 8, 0xff, 16);
        memset(d + 24, 1, 16);
        n = SYS_SIZE;
    }
    for (int i = 0; i < IP_VARS; i++) d[O_FLAGS + i] = (u8)t->vw[i];
    ip_mkdirs(t->saveDir);
    if (!ip_write_file(p, d, n)) ip_log(t, "can't write %s", p);
    free(d);
    free(p);
}

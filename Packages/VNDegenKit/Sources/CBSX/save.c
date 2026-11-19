#include "bsx.h"

#include <sys/stat.h>
#include <time.h>

#define SLOT_INFO 592
#define HIST 50
#define LUA_DATA "{[\"begin-time counters\"]={[1]={};[2]={};[3]={};};[\"begin-time parameters\"]={[1]={};[2]={};[3]={};};}"

typedef struct Buf {
    u8 *d;
    size_t n, cap;
} Buf;

static void put(Buf *b, const void *p, size_t n) {
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2 + 1024;
        b->d = bx_realloc(b->d, b->cap);
    }
    memcpy(b->d + b->n, p, n);
    b->n += n;
}

static void put32(Buf *b, u32 v) {
    u8 t[4];
    wr32(t, v);
    put(b, t, 4);
}

static void put8(Buf *b, u8 v) { put(b, &v, 1); }

static void zeros(Buf *b, size_t n) {
    u8 z[64] = {0};
    while (n) {
        size_t k = n < sizeof z ? n : sizeof z;
        put(b, z, k);
        n -= k;
    }
}

static size_t utf8_to_utf16(const char *s, u8 *out, size_t maxUnits) {
    size_t n = 0;
    while (*s && n < maxUnits) {
        u32 cp;
        s += bx_utf8_decode(s, &cp);
        if (cp == '\n') {
            if (n + 2 > maxUnits) break;
            out[2 * n] = '\\';
            out[2 * n + 1] = 0;
            n++;
            cp = 'n';
        }
        if (cp >= 0x10000) {
            if (n + 2 > maxUnits) break;
            cp -= 0x10000;
            u16 hi = (u16)(0xd800 + (cp >> 10)), lo = (u16)(0xdc00 + (cp & 0x3ff));
            out[2 * n] = (u8)hi;
            out[2 * n + 1] = (u8)(hi >> 8);
            n++;
            out[2 * n] = (u8)lo;
            out[2 * n + 1] = (u8)(lo >> 8);
            n++;
            continue;
        }
        out[2 * n] = (u8)cp;
        out[2 * n + 1] = (u8)(cp >> 8);
        n++;
    }
    return n;
}

static void put_wide_fixed(Buf *b, const char *s, size_t units) {
    u8 tmp[1024] = {0};
    if (units * 2 > sizeof tmp) units = sizeof tmp / 2;
    size_t n = utf8_to_utf16(s, tmp, units - 1);
    (void)n;
    put(b, tmp, units * 2);
}

static void put_wide_counted(Buf *b, const char *s) {
    u8 tmp[256];
    size_t n = utf8_to_utf16(s, tmp, 127);
    put32(b, (u32)n);
    put(b, tmp, n * 2);
}

char *bx_save_path(BX *b, int slot) {
    if (slot < 0) return bx_fmt("%s/save/no-%02d", b->saveDir, -slot);
    return bx_fmt("%s/save/no%03d", b->saveDir, slot);
}

bool bx_save_exists(BX *b, int slot) {
    char *p = bx_save_path(b, slot);
    bool r = bx_file_exists(p);
    free(p);
    return r;
}

void bx_thumb_size(BX *b, int *w, int *h) {
    const char *r = bx_cfg_get(&b->cfg, "main window", "thumbnail ratio");
    double ratio = r ? atof(r) : 0.15;
    if (ratio < 0.01 || ratio > 0.5) ratio = 0.15;
    *w = (int)(b->W * ratio);
    *h = (int)(b->H * ratio);
}

void bx_save_capture_thumb(BX *b) {
    int tw, th;
    bx_thumb_size(b, &tw, &th);
    if (b->thumb && (b->thumb->w != tw || b->thumb->h != th)) {
        bx_image_free(b->thumb);
        b->thumb = NULL;
    }
    if (!b->thumb) b->thumb = bx_image_new(tw, th);
    for (int y = 0; y < th; y++)
        for (int x = 0; x < tw; x++) {
            u32 r = 0, g = 0, bl = 0;
            int sx0 = x * b->W / tw, sx1 = (x + 1) * b->W / tw;
            int sy0 = y * b->H / th, sy1 = (y + 1) * b->H / th;
            int n = 0;
            for (int sy = sy0; sy < sy1; sy++)
                for (int sx = sx0; sx < sx1; sx++) {
                    u32 p = b->screen[(size_t)sy * b->W + sx];
                    r += p & 0xff;
                    g += p >> 8 & 0xff;
                    bl += p >> 16 & 0xff;
                    n++;
                }
            if (!n) n = 1;
            b->thumb->px[(size_t)y * tw + x] = (r / n) | (g / n) << 8 | (bl / n) << 16 | 0xff000000u;
        }
}


static int mode_of(BX *b) { return b->phase == PH_MAP ? 7 : b->phase == PH_CUSTOM_MAP ? 9 : 1; }

static void put_wears(BX *b, Buf *buf) {
    int n = 0;
    while (n < 32) {
        char key[32];
        snprintf(key, sizeof key, "ch%d max wear no", n + 1);
        if (!bx_cfg_get(&b->cfg, "bustup setting", key)) break;
        n++;
    }
    put32(buf, (u32)n);
    for (int i = 0; i < n; i++) {
        char key[32];
        snprintf(key, sizeof key, "ch%d max wear no", i + 1);
        int w = clampi(bx_cfg_int(&b->cfg, "bustup setting", key, 0), 0, 255) + 1;
        put32(buf, (u32)w);
        zeros(buf, (size_t)w);
    }
}

static void put_obj(BX *b, Buf *buf, const char *name, int kind, int x, int y, int parts, int alpha) {
    u8 head[0x1a];
    int w = b->W, h = b->H, frames = kind == OK_OBJ && bx_cfg_get(&b->cfg, "button object", name) ? 2 : 1;
    if (kind != OK_BG && kind != OK_CHAR && bx_res_head(&b->arc, RX_GFX, name, head, sizeof head)) {
        w = rd16(head + 0x16);
        h = rd16(head + 0x18) / frames;
    }
    put8(buf, (u8)kind);
    zeros(buf, 12);
    put_wide_counted(buf, name);
    put32(buf, (u32)x);
    put32(buf, (u32)y);
    put32(buf, (u32)x);
    put32(buf, (u32)y);
    zeros(buf, 16);
    put32(buf, (u32)w);
    put32(buf, (u32)h);
    put32(buf, 0);
    put32(buf, (u32)frames);
    put32(buf, (u32)parts);
    put32(buf, (u32)clampi(alpha, 0, 255));
    put32(buf, 0x3f800000u);
    zeros(buf, 20);
    if (kind == OK_OVERLAY) {
        zeros(buf, 17);
        put32(buf, 0x7fffffffu);
        put32(buf, 0xffffffffu);
        put32(buf, 0);
    }
}

static int obj_order(const void *pa, const void *pb) {
    const Obj *a = *(const Obj *const *)pa, *b = *(const Obj *const *)pb;
    int ka = a->kind == OK_BG ? 0 : 1, kb = b->kind == OK_BG ? 0 : 1;
    if (ka != kb) return ka - kb;
    return a->seq - b->seq;
}

static char *slot_text(BX *b) {
    if (!b->text) return bx_strdup("");
    if (b->speaker && *b->speaker) return bx_fmt("【%s】%s", b->speaker, b->text);
    return bx_strdup(b->text);
}

bool bx_save_slot(BX *b, int slot) {
    Script *s = b->sc;
    Buf buf = {0};
    char id[33];
    snprintf(id, sizeof id, "%s", b->saveId[0] ? b->saveId : "00000000000000000000000000000000");
    put(&buf, id, 32);
    if (!b->thumb) bx_save_capture_thumb(b);
    int tw = b->thumb->w, th = b->thumb->h;
    put32(&buf, (u32)(tw * th * 4 + SLOT_INFO));
    for (int y = th - 1; y >= 0; y--)
        for (int x = 0; x < tw; x++) {
            u32 p = b->thumb->px[(size_t)y * tw + x];
            u8 px[4] = {(u8)(p >> 16), (u8)(p >> 8), (u8)p, 0xff};
            put(&buf, px, 4);
        }
    time_t now = (time_t)b->startTime + (time_t)(b->now / 1000);
    struct tm tm;
    localtime_r(&now, &tm);
    char date[32], clock[32];
    snprintf(date, sizeof date, "%04d/%02d/%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    snprintf(clock, sizeof clock, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    put_wide_fixed(&buf, date, 16);
    put_wide_fixed(&buf, clock, 16);
    int day = bx_var_by_name(b, "#day"), tz = bx_var_by_name(b, "#time");
    put32(&buf, (u32)(day >= 0 ? b->globals[day].v : 0));
    put32(&buf, (u32)(tz >= 0 ? b->globals[tz].v : 0));
    put32(&buf, 0xffffffffu);
    put32(&buf, (u32)b->msgsShown);
    char *text = slot_text(b);
    put_wide_fixed(&buf, text, 256);
    free(text);
    zeros(&buf, 16);
    put32(&buf, b->now);
    put32(&buf, 0);
    put32(&buf, (u32)(b->startTime & 0xffffffff));
    zeros(&buf, 8);
    put32(&buf, (u32)mode_of(b));
    put32(&buf, 0);
    put32(&buf, 1);
    zeros(&buf, 9);
    put_wears(b, &buf);
    u32 cap = s->hdr[0] ? s->hdr[0] : 1024;
    put32(&buf, b->th.pc);
    put32(&buf, (u32)(b->th.sp * 2));
    put32(&buf, cap);
    for (u32 i = 0; i < cap; i++) {
        u32 v = 0;
        if ((int)(i / 2) < b->th.sp) v = i % 2 ? b->th.frameLabel[i / 2] : b->th.stack[i / 2];
        put32(&buf, v);
    }
    u32 nloc = s->hdr[1];
    put32(&buf, nloc);
    for (u32 i = 0; i < nloc; i++) put32(&buf, (u32)(i < BX_LOCALS ? b->locals[i].v : 0));
    put32(&buf, (u32)s->varCount);
    for (int i = 0; i < s->varCount; i++) put32(&buf, (u32)b->globals[i].v);
    put32(&buf, b->msgPc);
    put32(&buf, s->minor >= 1 ? (u32)(b->histCount % HIST) : 0);
    put32(&buf, (u32)b->histCount);
    for (int i = 0; i < HIST; i++) put32(&buf, i < b->histCount ? b->hist[i] : 0xffffffffu);
    put32(&buf, 0);
    put32(&buf, 0xffffffffu);
    put32(&buf, s->minor >= 3 ? (u32)b->msgsShown : 0);
    put8(&buf, 0);
    put8(&buf, 0);
    if (s->minor >= 3) {
        zeros(&buf, 3);
        put32(&buf, (u32)strlen(LUA_DATA));
        put(&buf, LUA_DATA, strlen(LUA_DATA) + 1);
    }
    put32(&buf, (u32)(b->bgmName[0] ? b->bgmIndex : 0));
    zeros(&buf, 12);
    put32(&buf, 1);
    zeros(&buf, 13);
    const Obj *list[BX_OBJS];
    int count = 0;
    for (int i = 0; i < BX_OBJS; i++)
        if (b->scene.obj[i].used) list[count++] = &b->scene.obj[i];
    qsort(list, (size_t)count, sizeof *list, obj_order);
    for (int i = 0; i < count; i++) {
        const Obj *o = list[i];
        int parts = o->kind == OK_BG ? (o->face[0] ? 2 : 0) : 2;
        put_obj(b, &buf, o->name, o->kind, o->x, o->y, parts, o->alpha);
        if (o->kind == OK_BG && o->face[0]) put_obj(b, &buf, o->face, OK_OVERLAY, 0, 0, 2, 255);
    }
    put8(&buf, 0xff);
    u8 tail[27] = {0};
    tail[3] = !b->winLayer;
    tail[7] = !b->menuLayer;
    tail[11] = !b->infoLayer;
    put(&buf, tail, sizeof tail);
    char *path = bx_save_path(b, slot);
    char dir[2048];
    snprintf(dir, sizeof dir, "%s/save", b->saveDir);
    bx_mkdirs(dir);
    bool ok = bx_write_file(path, buf.d, buf.n);
    bx_log(b, "%s %s (%zu bytes)", ok ? "saved" : "can't save", path, buf.n);
    bx_common_save(b);
    free(path);
    free(buf.d);
    return ok;
}

typedef struct Rd {
    const u8 *d;
    size_t n, p;
    bool bad;
} Rd;

static u32 get32(Rd *r) {
    if (r->p + 4 > r->n) {
        r->bad = true;
        r->p = r->n;
        return 0;
    }
    u32 v = rd32(r->d + r->p);
    r->p += 4;
    return v;
}

static u8 get8(Rd *r) {
    if (r->p + 1 > r->n) {
        r->bad = true;
        return 0xff;
    }
    return r->d[r->p++];
}

static void skip(Rd *r, size_t n) {
    if (r->p + n > r->n) {
        r->bad = true;
        r->p = r->n;
    } else r->p += n;
}

static char *get_wide(Rd *r) {
    u32 n = get32(r);
    if (n > 512 || r->p + 2 * (size_t)n > r->n) {
        r->bad = true;
        return bx_strdup("");
    }
    char *s = bx_utf16_to_utf8(r->d + r->p, n);
    r->p += 2 * (size_t)n;
    return s;
}

static void show_message_at(BX *b, u32 pc) {
    const u8 *p = b->sc->code + pc;
    if (pc + 6 > b->sc->codeSize || p[0] != 0x1d) return;
    int flag = p[1];
    u32 msg = rd32(p + 2);
    int name = flag >= 1 ? (int)rd32(p + 6) : -1;
    const char *text = msg < (u32)b->sc->msgCount ? b->sc->msgs[msg] : "";
    const char *speaker = name >= 0 && name < b->sc->charCount ? b->sc->chars[name] : NULL;
    bool skip = b->skipMode;
    b->skipMode = false;
    bx_msg_show(b, speaker, text, NULL);
    b->skipMode = skip;
    bx_msg_reveal(b);
}

bool bx_load_slot(BX *b, int slot) {
    char *path = bx_save_path(b, slot);
    size_t n;
    u8 *d = bx_read_file(path, &n);
    if (!d) {
        bx_log(b, "no save %s", path);
        free(path);
        return false;
    }
    Rd r = {d, n, 32, false};
    u32 info = get32(&r);
    skip(&r, info);
    if (r.bad || info < SLOT_INFO) {
        bx_log(b, "%s is too short", path);
        free(d);
        free(path);
        return false;
    }
    int shown = (int)rd32(d + 36 + info - SLOT_INFO + 76);
    skip(&r, 0x24);
    int mode = (int)get32(&r);
    skip(&r, 4 + 4 + 9);
    u32 chars = get32(&r);
    for (u32 i = 0; i < chars && i < 64 && !r.bad; i++) skip(&r, get32(&r));
    u32 pc = get32(&r);
    u32 depth = get32(&r);
    u32 cap = get32(&r);
    if (cap > 65536) r.bad = true;
    u32 *stack = bx_alloc(sizeof(u32) * (cap ? cap : 1));
    for (u32 i = 0; i < cap && !r.bad; i++) stack[i] = get32(&r);
    u32 nloc = get32(&r);
    i32 locals[BX_LOCALS] = {0};
    for (u32 i = 0; i < nloc && !r.bad; i++) {
        u32 v = get32(&r);
        if (i < BX_LOCALS) locals[i] = (i32)v;
    }
    u32 nglob = get32(&r);
    i32 *globals = bx_alloc(sizeof(i32) * (nglob ? nglob : 1));
    for (u32 i = 0; i < nglob && !r.bad; i++) globals[i] = (i32)get32(&r);
    u32 msgPc = get32(&r);
    u32 histPos = get32(&r) % HIST;
    u32 histCount = get32(&r);
    u32 ring[HIST], hist[HIST];
    for (int i = 0; i < HIST; i++) ring[i] = get32(&r);
    for (int i = 0; i < HIST; i++) hist[i] = histCount >= HIST ? ring[(histPos + (u32)i) % HIST] : ring[i];
    skip(&r, 14);
    if (b->sc->minor >= 3) {
        skip(&r, 3);
        skip(&r, (size_t)get32(&r) + 1);
    }
    u32 bgm = get32(&r);
    skip(&r, 12 + 4 + 13);
    if (r.bad || pc >= b->sc->codeSize || depth / 2 > BX_STACK || (int)nglob != b->sc->varCount) {
        bx_log(b, "%s doesn't fit this game", path);
        free(stack);
        free(globals);
        free(d);
        free(path);
        return false;
    }
    bx_phase_reset(b);
    memset(&b->scene, 0, sizeof b->scene);
    Obj *last = NULL;
    while (!r.bad) {
        u8 kind = get8(&r);
        if (kind == 0xff) break;
        skip(&r, 12);
        char *name = get_wide(&r);
        i32 x = (i32)get32(&r), y = (i32)get32(&r);
        skip(&r, 24 + 20);
        u8 alpha = get8(&r);
        skip(&r, 3 + 4 + 20);
        if (kind == OK_OVERLAY) skip(&r, 29);
        if (r.bad) break;
        if (!*name) {
            free(name);
            continue;
        }
        if (kind == OK_OVERLAY && last && last->kind == OK_BG && !strncmp(name, last->name, strlen(last->name))) bx_obj_face(b, last, name);
        else {
            last = bx_obj_put(b, kind, 0, name, x, y);
            if (last) last->alpha = alpha;
        }
        free(name);
    }
    u8 tail[27] = {0};
    if (!r.bad && r.p + sizeof tail <= r.n) memcpy(tail, r.d + r.p, sizeof tail);
    for (int i = 0; i < b->sc->varCount; i++) b->globals[i] = (Val){0, globals[i]};
    for (int i = 0; i < BX_LOCALS; i++) b->locals[i] = (Val){0, locals[i]};
    b->th.sp = 0;
    for (u32 i = 0; i + 1 < depth && i + 1 < cap; i += 2) {
        b->th.stack[b->th.sp] = stack[i];
        b->th.frameLabel[b->th.sp] = stack[i + 1];
        b->th.sp++;
    }
    b->msgsShown = shown;
    b->histCount = histCount > HIST ? HIST : (int)histCount;
    memcpy(b->hist, hist, sizeof hist);
    b->msgPc = msgPc;
    b->th.pc = pc;
    b->th.halted = false;
    b->th.skipLabel = -1;
    b->choiceCount = 0;
    b->skipMode = false;
    b->autoMode = false;
    b->transActive = false;
    bx_transition(b, 0, 0, NULL);
    b->shownDirty = true;
    b->stripLevel = b->viewLevel = 0;
    bool bust = false;
    for (int i = 0; i < BX_OBJS; i++) bust |= b->scene.obj[i].used && b->scene.obj[i].kind == OK_CHAR;
    b->dressEnabled = b->viewEnabled = bust;
    if ((i32)bgm > 0 && bgm < (u32)b->sc->strCount) {
        b->bgmIndex = (int)bgm;
        snprintf(b->bgmName, sizeof b->bgmName, "%s", b->sc->strs[bgm]);
        bx_snd_play(b, CH_BGM, RX_BGM, b->bgmName, true, 500);
    } else {
        b->bgmName[0] = 0;
        bx_snd_stop(b, CH_BGM, 300);
    }
    bx_snd_loop_stop(b, CH_BGS, NULL, 0);
    bx_snd_loop_stop(b, CH_VOICE2, NULL, 0);
    bx_snd_stop(b, CH_VOICE, 0);
    bx_snd_stop(b, CH_VOICE2, 0);
    bx_msg_clear(b);
    b->winShown = b->winLayer = !tail[3];
    b->menuLayer = !tail[7];
    b->infoLayer = !tail[11];
    b->winHidden = false;
    if (mode == 7 || mode == 9) {
        b->th.wait = W_NONE;
        bx_phase_start(b, mode == 7 ? PH_MAP : PH_CUSTOM_MAP);
    } else if (msgPc < b->sc->codeSize && b->sc->code[msgPc] == 0x1d) {
        show_message_at(b, msgPc);
        b->th.wait = W_TEXT;
    } else b->th.wait = W_NONE;
    bx_log(b, "loaded %s at %x", path, pc);
    free(stack);
    free(globals);
    free(d);
    free(path);
    return true;
}

void bx_autosave(BX *b) {
    int first = bx_cfg_int(&b->cfg, "data screen", "no begin", 15);
    if (first <= 0) return;
    int pick = -1;
    long long oldest = 0;
    for (int i = 0; i < first; i++) {
        char *path = bx_save_path(b, i);
        char real[2048];
        struct stat st;
        bool found = bx_path_resolve(path, real, sizeof real) > 0 && !stat(real, &st);
        free(path);
        if (!found) {
            pick = i;
            break;
        }
        if (pick < 0 || (long long)st.st_mtime < oldest) {
            pick = i;
            oldest = (long long)st.st_mtime;
        }
    }
    bx_save_capture_thumb(b);
    bx_save_slot(b, pick);
}

void bx_common_load(BX *b) {
    free(b->common);
    b->common = NULL;
    b->commonSize = 0;
    char path[2048];
    snprintf(path, sizeof path, "%s/save/common.dat", b->saveDir);
    size_t n;
    u8 *d = bx_read_file(path, &n);
    if (!d) return;
    u32 choices = b->sc->hdr[2], msgs = (u32)b->sc->msgCount;
    size_t p = 0x20, q = 0;
    for (; p + 8 + choices <= n && p < 0x1000; p++) {
        q = p + 4 + choices;
        if (rd32(d + p) == choices && q + 4 + msgs <= n && rd32(d + q) == msgs) break;
    }
    if (p + 8 + choices > n || p >= 0x1000) {
        bx_log(b, "common.dat doesn't match this script (%u choices, %u messages)", choices, msgs);
        free(d);
        return;
    }
    b->common = d;
    b->commonSize = n;
    b->choiceFlags = p + 4;
    b->readFlags = q + 4;
}

void bx_common_save(BX *b) {
    if (!b->common || !b->commonDirty) return;
    char path[2048];
    snprintf(path, sizeof path, "%s/save/common.dat", b->saveDir);
    if (bx_write_file(path, b->common, b->commonSize)) b->commonDirty = false;
}

bool bx_msg_read(BX *b, u32 msg) { return b->common && msg < (u32)b->sc->msgCount && b->common[b->readFlags + msg]; }

void bx_mark_read(BX *b, u32 msg) {
    if (!b->common || msg >= (u32)b->sc->msgCount || b->common[b->readFlags + msg]) return;
    b->common[b->readFlags + msg] = 1;
    b->commonDirty = true;
}

bool bx_choice_seen(BX *b, int id) { return b->common && id >= 0 && (u32)id < b->sc->hdr[2] && b->common[b->choiceFlags + (size_t)id]; }

void bx_mark_choice(BX *b, int id) {
    if (!b->common || id < 0 || (u32)id >= b->sc->hdr[2] || b->common[b->choiceFlags + (size_t)id]) return;
    b->common[b->choiceFlags + (size_t)id] = 1;
    b->commonDirty = true;
}

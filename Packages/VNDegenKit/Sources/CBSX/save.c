#include "bsx.h"

#include <time.h>

#define THUMB_W 120
#define THUMB_H 90
#define SLOT_INFO 592
#define HIST 50

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

void bx_save_capture_thumb(BX *b) {
    if (!b->thumb) b->thumb = bx_image_new(THUMB_W, THUMB_H);
    for (int y = 0; y < THUMB_H; y++)
        for (int x = 0; x < THUMB_W; x++) {
            u32 r = 0, g = 0, bl = 0;
            int sx0 = x * b->W / THUMB_W, sx1 = (x + 1) * b->W / THUMB_W;
            int sy0 = y * b->H / THUMB_H, sy1 = (y + 1) * b->H / THUMB_H;
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
            b->thumb->px[(size_t)y * THUMB_W + x] = (r / n) | (g / n) << 8 | (bl / n) << 16 | 0xff000000u;
        }
}

static const u8 headTail[] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,
};

static int mode_of(BX *b) { return b->phase == PH_MAP ? 7 : 1; }

static void put_obj(Buf *buf, const char *name, int kind, int x, int y, int parts, int alpha) {
    put8(buf, (u8)kind);
    zeros(buf, 12);
    put_wide_counted(buf, name);
    put32(buf, (u32)x);
    put32(buf, (u32)y);
    put32(buf, (u32)x);
    put32(buf, (u32)y);
    zeros(buf, 16);
    put32(buf, 800);
    put32(buf, 600);
    put32(buf, 0);
    put32(buf, 1);
    put32(buf, (u32)parts);
    put32(buf, (u32)clampi(alpha, 0, 255));
    put32(buf, 0x3f800000u);
    zeros(buf, 20);
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
    put32(&buf, THUMB_W * THUMB_H * 4 + SLOT_INFO);
    if (!b->thumb) bx_save_capture_thumb(b);
    for (int y = THUMB_H - 1; y >= 0; y--)
        for (int x = 0; x < THUMB_W; x++) {
            u32 p = b->thumb->px[(size_t)y * THUMB_W + x];
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
    put(&buf, headTail + 4, sizeof headTail - 4);
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
    put32(&buf, 0);
    put32(&buf, (u32)b->histCount);
    for (int i = 0; i < HIST; i++) put32(&buf, i < b->histCount ? b->hist[i] : 0xffffffffu);
    put32(&buf, 0);
    put32(&buf, 0xffffffffu);
    put32(&buf, 0);
    put8(&buf, 0);
    put8(&buf, 0);
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
        bool face = o->kind == OK_BG && o->face[0];
        put_obj(&buf, o->name, o->kind, o->x, o->y, o->kind == OK_CHAR || face ? 2 : 0, o->alpha);
        if (face) {
            put_obj(&buf, o->face, OK_OVERLAY, 0, 0, 2, 255);
            put8(&buf, 0);
            zeros(&buf, 12);
            put32(&buf, 0);
            put32(&buf, 0x7fffffffu);
            put32(&buf, 0xffffffffu);
            put32(&buf, 0);
        }
    }
    put8(&buf, 0xff);
    static const u8 tail[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    put(&buf, tail, sizeof tail);
    char *path = bx_save_path(b, slot);
    char dir[2048];
    snprintf(dir, sizeof dir, "%s/save", b->saveDir);
    bx_mkdirs(dir);
    bool ok = bx_write_file(path, buf.d, buf.n);
    bx_log(b, "%s %s (%zu bytes)", ok ? "saved" : "can't save", path, buf.n);
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
    size_t infoStart = r.p + THUMB_W * THUMB_H * 4;
    skip(&r, info);
    if (r.bad || infoStart + SLOT_INFO > n) {
        bx_log(b, "%s is too short", path);
        free(d);
        free(path);
        return false;
    }
    skip(&r, 0x24);
    int mode = (int)get32(&r);
    skip(&r, 0x7f - 0x28);
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
    get32(&r);
    u32 histCount = get32(&r);
    u32 hist[HIST];
    for (int i = 0; i < HIST; i++) hist[i] = get32(&r);
    skip(&r, 14);
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
    bx_phase_end(b);
    bx_map_free(b);
    memset(&b->scene, 0, sizeof b->scene);
    Obj *last = NULL;
    while (!r.bad) {
        u8 kind = get8(&r);
        if (kind == 0xff) break;
        skip(&r, 12);
        char *name = get_wide(&r);
        if (!*name) {
            skip(&r, 12);
            free(name);
            continue;
        }
        i32 x = (i32)get32(&r), y = (i32)get32(&r);
        skip(&r, 24 + 20);
        u8 alpha = get8(&r);
        skip(&r, 3 + 4 + 20);
        if (kind == OK_OVERLAY && last && last->kind == OK_BG && !strncmp(name, last->name, strlen(last->name))) bx_obj_face(b, last, name);
        else {
            last = bx_obj_put(b, kind, 0, name, x, y);
            if (last) last->alpha = alpha;
        }
        free(name);
    }
    for (int i = 0; i < b->sc->varCount; i++) b->globals[i] = (Val){0, globals[i]};
    for (int i = 0; i < BX_LOCALS; i++) b->locals[i] = (Val){0, locals[i]};
    b->th.sp = 0;
    for (u32 i = 0; i + 1 < depth && i + 1 < cap; i += 2) {
        b->th.stack[b->th.sp] = stack[i];
        b->th.frameLabel[b->th.sp] = stack[i + 1];
        b->th.sp++;
    }
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
    if ((i32)bgm > 0 && bgm < (u32)b->sc->strCount) {
        b->bgmIndex = (int)bgm;
        snprintf(b->bgmName, sizeof b->bgmName, "%s", b->sc->strs[bgm]);
        bx_snd_play(b, CH_BGM, RX_BGM, b->bgmName, true, 500);
    } else {
        b->bgmName[0] = 0;
        bx_snd_stop(b, CH_BGM, 300);
    }
    bx_snd_stop(b, CH_BGS, 0);
    bx_snd_stop(b, CH_VOICE, 0);
    bx_msg_clear(b);
    b->winShown = true;
    b->winHidden = false;
    if (mode == 7) {
        b->th.wait = W_NONE;
        bx_phase_start(b, PH_MAP);
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

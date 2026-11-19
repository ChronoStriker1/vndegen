#include "ex.h"

#include <time.h>

#define MARK 0x7fffffffu
#define PC_ENGINE_REGS 148

static const char *SOUND_REST =
    ";1,-1,2.0.0,257,0,4.0.0.0.0;1,-1,2.-1.-1,512,0,4.0.0.0.0;1,-1,2.0.0,769,0,4.0.0.0.0;1,-1,2.214.214,0,0,4.0.0.0.0;1,-1,2.0.0,769,0,4.0.0.0.0;1,-1,2.0.0,0,0,4.0.0.0.0;1,-1,2.0.0,769,0,"
    "4.0.0.0.0;1,-1,2.0.0,769,0,4.0.0.0.0";
static const char *PORTRAITS =
    "8;0,144,145,-1,-1,-1,-1,16842752,-98,760,0,0;0,146,147,-1,-1,-1,-1,16842752,1537,730,0,0;0,148,149,-1,-1,-1,-1,16842752,0,0,0,0;0,-2,-2,-1,-1,-1,-1,16842752,0,0,0,0;0,-2,-2,-1,-1,-1,-1,16842752,"
    "0,0,0,0;0,-2,-2,-1,-1,-1,-1,16842752,0,0,0,0;0,-2,-2,-1,-1,-1,-1,16842752,0,0,0,0;0,-2,-2,-1,-1,-1,-1,16842752,0,0,0,0";
static const char *WAITMARK = "0;1,1500,960,4,res\\g\\sy\\01\\1920_300.gyu,a..................b..................c..................d..................";
static const char *EXITS = "8,0.exit,1.title,2.exit,3.exit,4.exit,5.exit,6.exit,7.exit";

typedef struct Out {
    u8 *d;
    size_t n, cap;
} Out;

static void raw(Out *o, const void *p, size_t n) {
    if (o->n + n > o->cap) {
        o->cap = (o->n + n) * 2 + 1024;
        o->d = ex_realloc(o->d, o->cap);
    }
    memcpy(o->d + o->n, p, n);
    o->n += n;
}

static void u32le(Out *o, u32 v) {
    u8 b[4] = {(u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24)};
    raw(o, b, 4);
}

static void str(Out *o, const char *s) {
    size_t n = strlen(s) + 1;
    u32le(o, MARK);
    u32le(o, 0);
    u32le(o, (u32)n);
    raw(o, s, n);
}

static void empties(Out *o, int k) {
    for (int i = 0; i < k; i++) str(o, "");
}

static const char *kind_letter(int slot) { return slot >= 99 ? "q" : slot >= 90 ? "a" : "u"; }

static int kind_index(int slot) { return slot >= 99 ? slot - 99 : slot >= 90 ? slot - 90 : slot; }

static char *pc_path(EX *e, int slot) { return ex_fmt("%s/%s%d", e->saveDir, kind_letter(slot), kind_index(slot)); }

static char *pc_thumb_path(EX *e, int slot) {
    const char *k = slot >= 99 ? "Q" : slot >= 90 ? "A" : "U";
    return ex_fmt("%s/thumbnail/%s%05d.gyu", e->saveDir, k, kind_index(slot));
}

static char *db_path(EX *e, int slot) { return ex_fmt("%s/%sdb", e->saveDir, kind_letter(slot)); }

static int db_entries(int slot) { return slot >= 90 ? 10 : 100; }

static u32 dos_time(long long t) {
    time_t tt = (time_t)t;
    struct tm tm;
    localtime_r(&tt, &tm);
    u32 date = (u32)((tm.tm_year - 80) << 9 | (tm.tm_mon + 1) << 5 | tm.tm_mday);
    u32 tod = (u32)(tm.tm_hour << 11 | tm.tm_min << 5 | tm.tm_sec / 2);
    return date << 16 | tod;
}

static long long from_dos(u32 v) {
    if (!v) return 0;
    struct tm tm = {0};
    tm.tm_year = (int)(v >> 25) + 80;
    tm.tm_mon = (int)(v >> 21 & 15) - 1;
    tm.tm_mday = (int)(v >> 16 & 31);
    tm.tm_hour = (int)(v >> 11 & 31);
    tm.tm_min = (int)(v >> 5 & 63);
    tm.tm_sec = (int)(v & 31) * 2;
    tm.tm_isdst = -1;
    return (long long)mktime(&tm);
}

static u8 *db_read(EX *e, int slot, size_t *n) {
    char *path = db_path(e, slot);
    u8 *d = ex_read_file(path, n);
    free(path);
    size_t want = (size_t)db_entries(slot) * 17;
    if (d && *n >= want) return d;
    free(d);
    Out o = {0};
    for (int i = 0; i < db_entries(slot); i++) {
        u32le(&o, 0);
        str(&o, "");
    }
    *n = o.n;
    return o.d;
}

static void db_set(EX *e, int slot, u32 stamp) {
    size_t n;
    u8 *d = db_read(e, slot, &n);
    size_t at = (size_t)kind_index(slot) * 17;
    d[at] = (u8)stamp;
    d[at + 1] = (u8)(stamp >> 8);
    d[at + 2] = (u8)(stamp >> 16);
    d[at + 3] = (u8)(stamp >> 24);
    char *path = db_path(e, slot);
    ex_write_file(path, d, n);
    free(path);
    free(d);
}

bool pc_save_write(EX *e, int slot, long long when, const u32 *shot) {
    Out o = {0};
    u8 head[28] = {0x00, 0x80};
    raw(&o, head, sizeof head);
    str(&o, "FCReg");
    u32le(&o, (u32)e->nR);
    const i32 *src = e->blockR ? e->blockR : e->R;
    int ordinal = 0;
    Script *sc = e->savePoint.sc;
    for (int i = 0; sc && i < e->savePoint.pc && i < sc->count; i++)
        if (sc->ops[i].op == 5) ordinal++;
    for (int i = 0; i < e->nR; i++) {
        i32 v = src[i];
        if (i < PC_ENGINE_REGS) {
            switch (i) {
            case 2: v = ordinal; break;
            case 3: v = -1; break;
            case 4: v = 515; break;
            case 5: v = e->savePoint.pc - 1; break;
            case 8: v = 125830200; break;
            case 9: v = 256; break;
            case 10: v = e->blockR ? e->blockCg : e->cgId; break;
            case 18:
            case 19: v = -1; break;
            case 21: v = 70780800; break;
            case 28: v = 16318208; break;
            case 61: v = 8585216; break;
            default: v = i >= 100 ? -1 : 0; break;
            }
        }
        u32le(&o, (u32)v);
    }
    str(&o, "FCStrReg");
    u32le(&o, 0x100);
    empties(&o, 2);
    char *path = ex_fmt("rld\\%s.rld", e->savePoint.sc ? e->savePoint.sc->name : (e->sc ? e->sc->name : "start"));
    str(&o, path);
    free(path);
    empties(&o, 2);
    str(&o, "-1;8;0,0,0,0;-1;;;;;;;;");
    str(&o, "0;0x40000000,-1,-1,0,0");
    str(&o, "0,0,0,0");
    str(&o, "3;0;0");
    empties(&o, 193);
    str(&o, WAITMARK);
    empties(&o, 1);
    char *sound = e->musicTrack >= 0 ? ex_fmt("9;1,%d,2.144.144,65,0,4.0.0.0.0%s", e->musicTrack, SOUND_REST) : ex_fmt("9;1,-1,2.0.0,64,0,4.0.0.0.0%s", SOUND_REST);
    str(&o, sound);
    free(sound);
    str(&o, PORTRAITS);
    empties(&o, 1);
    str(&o, EXITS);
    empties(&o, 2);
    str(&o, "0,,,");
    empties(&o, 45);
    str(&o, "sc00.dll");
    str(&o, "ExHIBIT");
    u8 zero[8] = {0};
    raw(&o, zero, sizeof zero);
    empties(&o, 1);
    path = pc_path(e, slot);
    bool ok = ex_write_file(path, o.d, o.n);
    free(path);
    free(o.d);
    if (!ok) return false;
    if (shot) {
        char *dir = ex_fmt("%s/thumbnail", e->saveDir);
        ex_mkdirs(dir);
        free(dir);
        size_t gl;
        u8 *g = gyu_encode(shot, 320, 180, &gl);
        path = pc_thumb_path(e, slot);
        ex_write_file(path, g, gl);
        free(path);
        free(g);
    }
    db_set(e, slot, dos_time(when));
    return true;
}

static const u8 *next_str(const u8 *p, const u8 *end, const u8 **s, u32 *len) {
    while (p + 12 <= end) {
        u32 m = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
        if (m == MARK && !p[4] && !p[5] && !p[6] && !p[7]) {
            u32 n = (u32)p[8] | (u32)p[9] << 8 | (u32)p[10] << 16 | (u32)p[11] << 24;
            if (p + 12 + n > end) return NULL;
            *s = p + 12;
            *len = n;
            return p + 12 + n;
        }
        p++;
    }
    return NULL;
}

bool pc_save_read(EX *e, int slot, char *script, size_t scriptSize, i32 **regs, int *count) {
    char *path = pc_path(e, slot);
    size_t n;
    u8 *d = ex_read_file(path, &n);
    free(path);
    if (!d) return false;
    const u8 *end = d + n, *p = d, *s;
    u32 len;
    bool ok = false;
    *regs = NULL;
    p = next_str(p, end, &s, &len);
    if (p && len >= 6 && !memcmp(s, "FCReg", 5) && p + 4 <= end) {
        u32 c = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
        if (c > 0 && c <= 65536 && p + 4 + (size_t)c * 4 <= end) {
            i32 *r = ex_alloc(sizeof(i32) * c);
            for (u32 i = 0; i < c; i++) {
                const u8 *q = p + 4 + i * 4;
                r[i] = (i32)((u32)q[0] | (u32)q[1] << 8 | (u32)q[2] << 16 | (u32)q[3] << 24);
            }
            *regs = r;
            *count = (int)c;
            p += 4 + (size_t)c * 4;
            for (int k = 0; k < 8 && p; k++) {
                p = next_str(p, end, &s, &len);
                if (!p) break;
                if (len > 8 && !strncmp((const char *)s, "rld\\", 4)) {
                    const char *name = (const char *)s + 4;
                    size_t nl = strnlen(name, len - 4);
                    if (nl > 4 && !strncmp(name + nl - 4, ".rld", 4)) nl -= 4;
                    if (nl < scriptSize) {
                        memcpy(script, name, nl);
                        script[nl] = 0;
                        ok = true;
                    }
                    break;
                }
            }
        }
    }
    free(d);
    if (!ok) {
        free(*regs);
        *regs = NULL;
    }
    return ok;
}

long long pc_slot_time(EX *e, int slot) {
    char *path = pc_path(e, slot);
    bool exists = ex_file_exists(path);
    free(path);
    if (!exists) return 0;
    size_t n;
    u8 *d = db_read(e, slot, &n);
    size_t at = (size_t)kind_index(slot) * 17;
    u32 v = at + 4 <= n ? (u32)d[at] | (u32)d[at + 1] << 8 | (u32)d[at + 2] << 16 | (u32)d[at + 3] << 24 : 0;
    free(d);
    long long t = from_dos(v);
    return t ? t : 1;
}

Image *pc_slot_thumb(EX *e, int slot) {
    char *path = pc_thumb_path(e, slot);
    size_t n;
    u8 *d = ex_read_file(path, &n);
    free(path);
    if (!d) return NULL;
    Image *im = gyu_decode(d, n);
    free(d);
    return im;
}

#include "eagls.h"

#include <ctype.h>
#include <time.h>

#define BLOCK 0x151F58
#define INT_TABLE 0x18
#define INT_SLOTS 997
#define INT_REC 468
#define INT_MAX 0x71EBC
#define STR_TABLE 0x71EC0
#define STR_SLOTS 97
#define STR_REC 320
#define STR_MAX 0x79800
#define PLANE_NAMES 0xA5884
#define LAYER_BG 0xA7184
#define THREAD_POS 0x12EC70
#define THREAD_POS2 0x12ED3C
#define THREAD_FRAME 0x12DC68
#define SOUND_NAMES 0x14654C
#define MES_PARTS 0x1467C5
#define TEXT_BUF 0x1468A5
#define THUMB 0x148E1C
#define THUMB_W 127
#define THUMB_H 95
#define THUMB_STRIDE 384

static const char banner[] = "[E]nhanced [A]dventure [G]ame [L]anguage [S]ystem E.A.G.L.S";
static const char xorKey[] = "1qaz2wsx3edc4rfv5tgb6yhn7ujm8ik,9ol.0p;/-@:^[]";

u8 *lzss_unpack(const u8 *src, size_t n, size_t outLen) {
    u8 *out = eg_alloc(outLen + 1);
    u8 win[4096] = {0};
    int wp = 0xfee;
    size_t o = 0, i = 0;
    while (i < n && o < outLen) {
        u8 flags = src[i++];
        for (int b = 0; b < 8 && i < n && o < outLen; b++, flags >>= 1) {
            if (flags & 1) {
                u8 c = src[i++];
                out[o++] = c;
                win[wp] = c;
                wp = (wp + 1) & 0xfff;
            } else {
                if (i + 1 >= n) break;
                int lo = src[i], hi = src[i + 1];
                i += 2;
                int pos = lo | (hi & 0xf0) << 4, len = (hi & 0xf) + 3;
                for (int k = 0; k < len && o < outLen; k++) {
                    u8 c = win[(pos + k) & 0xfff];
                    out[o++] = c;
                    win[wp] = c;
                    wp = (wp + 1) & 0xfff;
                }
            }
        }
    }
    return out;
}

u8 *lzss_pack(const u8 *src, size_t n, size_t *outLen) {
    size_t cap = n + n / 8 + 16;
    u8 *out = eg_alloc(cap);
    int *head = eg_alloc(sizeof(int) * 65536);
    int *prev = eg_alloc(sizeof(int) * n + sizeof(int));
    for (int i = 0; i < 65536; i++) head[i] = -1;
    size_t o = 0, i = 0;
    size_t flagAt = 0;
    int bit = 8;
    while (i < n) {
        if (bit == 8) {
            flagAt = o++;
            out[flagAt] = 0;
            bit = 0;
        }
        int bestLen = 0;
        size_t bestPos = 0;
        if (i + 2 < n) {
            u32 h = ((u32)src[i] << 8 ^ (u32)src[i + 1] << 4 ^ src[i + 2]) & 0xffff;
            int cand = head[h];
            int tries = 64;
            while (cand >= 0 && tries-- > 0 && i - (size_t)cand < 4096 - 18) {
                int len = 0;
                while (len < 18 && i + (size_t)len < n && src[cand + len] == src[i + len]) len++;
                if (len > bestLen) {
                    bestLen = len;
                    bestPos = (size_t)cand;
                    if (len == 18) break;
                }
                cand = prev[cand];
            }
        }
        int take = bestLen >= 3 ? bestLen : 1;
        if (bestLen >= 3) {
            int pos = (int)((bestPos + 0xfee) & 0xfff);
            out[o++] = (u8)(pos & 0xff);
            out[o++] = (u8)((pos >> 4 & 0xf0) | (bestLen - 3));
        } else {
            out[flagAt] |= (u8)(1 << bit);
            out[o++] = src[i];
        }
        bit++;
        for (int k = 0; k < take; k++, i++) {
            if (i + 2 < n) {
                u32 h = ((u32)src[i] << 8 ^ (u32)src[i + 1] << 4 ^ src[i + 2]) & 0xffff;
                prev[i] = head[h];
                head[h] = (int)i;
            }
        }
    }
    free(head);
    free(prev);
    *outLen = o;
    return out;
}

static void xor_stream(u8 *d, size_t n, u32 *seed) {
    for (size_t i = 0; i < n; i++) d[i] ^= (u8)xorKey[eg_rand(seed) % 46];
}

static int cmod(int a, int n) {
    int r = (a < 0 ? -a : a) % n;
    return a < 0 ? -r : r;
}

static int name_hash(const char *s, int n) {
    int h = 0;
    for (const u8 *p = (const u8 *)s; *p; p++) h = cmod(h * 96 + (int)(int8_t)*p - 32, n);
    return h;
}

static int probe_slot(const u8 *table, int rec, int n, const char *name, int *maxProbe, bool find) {
    int h = name_hash(name, n), j = 0;
    for (int i = 1; i <= n; i++) {
        int idx = cmod((h + j + i) * i, n);
        const u8 *r = table + (size_t)idx * rec;
        if (find) {
            if (r[0] && !strcmp((const char *)r + 1, name)) return idx;
            if (!r[0] && i > (maxProbe ? *maxProbe : n)) return -1;
        } else if (!r[0]) {
            if (maxProbe && *maxProbe < i) *maxProbe = i;
            return idx;
        }
        if (idx == 0) j = 1;
        else {
            j = 0;
            for (const u8 *p = (const u8 *)name; *p; p++) j = cmod(j * 0x76 - 0x13 + (int)(int8_t)*p, n);
            if (idx == j) j = cmod((int)(int8_t)name[0] + j, n);
        }
    }
    return -1;
}

static void lower_name(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; src[i] && i + 1 < n; i++) dst[i] = (char)tolower((u8)src[i]);
    dst[i] = 0;
}

static char *save_path(EG *e, int slot) {
    if (slot >= EG_SLOTS) return eg_fmt("%s/Save/VNDegenQuick.sav", e->saveDir);
    return eg_fmt("%s/Save/Save%d.sav", e->saveDir, slot);
}

static int msg_offset(Thread *t, const char **file) {
    *file = t->sc ? t->sc->name : "";
    return t->msgOff;
}

static void put_time(u8 *b) {
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    wr16(b, (u16)(tmv.tm_year + 1900));
    wr16(b + 2, (u16)(tmv.tm_mon + 1));
    wr16(b + 4, (u16)tmv.tm_wday);
    wr16(b + 6, (u16)tmv.tm_mday);
    wr16(b + 8, (u16)tmv.tm_hour);
    wr16(b + 10, (u16)tmv.tm_min);
    wr16(b + 12, (u16)tmv.tm_sec);
}

static void write_vars(EG *e, u8 *b) {
    int maxInt = 0, maxStr = 0;
    for (int i = 0; i < e->vars.count; i++) {
        Var *v = &e->vars.vars[i];
        if (v->name[0] != '_') continue;
        char name[68];
        lower_name(name, v->name, sizeof name);
        if (v->str) {
            int idx = probe_slot(b + STR_TABLE, STR_REC, STR_SLOTS, name, &maxStr, false);
            if (idx < 0) continue;
            u8 *r = b + STR_TABLE + (size_t)idx * STR_REC;
            r[0] = 1;
            snprintf((char *)r + 1, 63, "%s", name);
            const char *val = v->n > 0 && v->v[0].s ? v->v[0].s : "";
            snprintf((char *)r + 64, 256, "%s", val);
        } else {
            int idx = probe_slot(b + INT_TABLE, INT_REC, INT_SLOTS, name, &maxInt, false);
            if (idx < 0) continue;
            u8 *r = b + INT_TABLE + (size_t)idx * INT_REC;
            r[0] = 1;
            snprintf((char *)r + 1, 67, "%s", name);
            for (int k = 0; k < 100 && k < v->n; k++) wr32(r + 68 + k * 4, (u32)v->v[k].i);
        }
    }
    wr32(b + INT_MAX, (u32)maxInt);
    wr32(b + STR_MAX, (u32)maxStr);
}

static void write_thumb(EG *e, u8 *b) {
    Image *t = e->thumb;
    if (!t) return;
    for (int y = 0; y < THUMB_H && y < t->h; y++) {
        u8 *row = b + THUMB + (size_t)(THUMB_H - 1 - y) * THUMB_STRIDE;
        for (int x = 0; x < THUMB_W && x < t->w; x++) {
            u32 p = t->px[(size_t)y * t->w + x];
            row[x * 3] = (u8)(p >> 16);
            row[x * 3 + 1] = (u8)(p >> 8);
            row[x * 3 + 2] = (u8)p;
        }
    }
}

static void build_block(EG *e, u8 *b) {
    memset(b, 0, BLOCK);
    put_time(b);
    write_vars(e, b);
    for (int p = 0; p < EG_PLANES && p < 32; p++)
        if (e->planeName[p][0]) snprintf((char *)b + PLANE_NAMES + p * 64, 64, "%s", e->planeName[p]);
    Layer *bg = &e->layer[2];
    if (bg->vis) {
        i32 r[11] = {bg->plane, bg->dx, bg->dy, (i32)((u32)bg->dw | (u32)bg->dh << 16), bg->sx, bg->sy, (i32)((u32)bg->sw | (u32)bg->sh << 16), 0, 0, 0, bg->alpha};
        for (int i = 0; i < 11; i++) wr32(b + LAYER_BG + i * 4, (u32)r[i]);
    }
    const char *file;
    int off = msg_offset(&e->th[0], &file);
    wr32(b + THREAD_POS, (u32)off);
    snprintf((char *)b + THREAD_POS + 4, 32, "%s", file);
    wr32(b + THREAD_POS + 0x48, 1);
    wr32(b + THREAD_POS + 0x4C, 1);
    for (int i = 0; i < 30; i++) wr32(b + THREAD_POS + 0x50 + i * 4, 0xffffffffu);
    wr32(b + THREAD_POS2, (u32)(off > 4 ? off - 4 : 0));
    snprintf((char *)b + THREAD_POS2 + 4, 32, "%s", file);
    b[THREAD_FRAME] = 1;
    b[THREAD_FRAME + 1] = 1;
    wr32(b + THREAD_FRAME + 4, (u32)off);
    for (int c = 0; c < 2; c++) snprintf((char *)b + SOUND_NAMES + c * 64, 64, "%s", e->bgmName[c][0] ? e->bgmName[c] : "0");
    for (int i = 0; i < 7; i++)
        if (e->mesName[i][0]) {
            b[MES_PARTS - 1 + i * 32] = 1;
            snprintf((char *)b + MES_PARTS + i * 32, 31, "%s", e->mesName[i]);
        }
    for (int i = 0; i < 6; i++) b[0x14674C + i] = 4;
    wr32(b + 0x146770, 200);
    wr32(b + 0x146774, 200);
    for (int i = 0; i < 12; i++) wr32(b + 0x146794 + i * 4, (u32)e->winRect[i]);
    wr32(b + 0x146AA4, (u32)e->mesAlpha);
    for (int i = 0; i < 16; i++) wr32(b + 0x146AAC + i * 4, (u32)e->nameRect[i]);
    wr32(b + 0x146AEC, (u32)(e->mesMode != 0));
    if (e->speaker) {
        char nm[160];
        snprintf(nm, sizeof nm, e->nameFmt[0] ? e->nameFmt[0] : "%s", e->speaker);
        snprintf((char *)b + 0x146AF0, 96, "%s", nm);
    }
    b[TEXT_BUF] = 1;
    snprintf((char *)b + TEXT_BUF + 1, 256, "%s", e->text ? e->text : "");
    write_thumb(e, b);
}

static void trailer_str(u8 **p, size_t *n, size_t *cap, const char *s) {
    size_t l = strlen(s);
    if (*n + l + 2 > *cap) {
        *cap = (*cap + l + 2) * 2;
        *p = eg_realloc(*p, *cap);
    }
    wr16(*p + *n, (u16)l);
    memcpy(*p + *n + 2, s, l);
    *n += l + 2;
}

static void trailer_i32(u8 **p, size_t *n, size_t *cap, i32 v) {
    if (*n + 4 > *cap) {
        *cap = (*cap + 4) * 2;
        *p = eg_realloc(*p, *cap);
    }
    wr32(*p + *n, (u32)v);
    *n += 4;
}

static u8 *build_trailer(EG *e, size_t *len) {
    size_t n = 0, cap = 4096;
    u8 *p = eg_alloc(cap);
    memcpy(p, "VNDG", 4);
    n = 4;
    trailer_i32(&p, &n, &cap, 1);
    Thread *t = &e->th[0];
    trailer_i32(&p, &n, &cap, t->sp);
    for (int i = 0; i < t->sp; i++) {
        trailer_str(&p, &n, &cap, t->stack[i].sc ? t->stack[i].sc->name : "");
        trailer_i32(&p, &n, &cap, t->stack[i].sc && t->stack[i].pc < t->stack[i].sc->count ? t->stack[i].sc->ins[t->stack[i].pc].off : 0);
    }
    for (int l = 0; l < EG_PLANES; l++) {
        Layer *ly = &e->layer[l];
        i32 v[12] = {ly->vis, ly->plane, ly->dx, ly->dy, ly->dw, ly->dh, ly->sx, ly->sy, ly->sw, ly->sh, ly->alpha, ly->movie};
        for (int k = 0; k < 12; k++) trailer_i32(&p, &n, &cap, v[k]);
        trailer_str(&p, &n, &cap, e->planeName[l]);
    }
    for (int i = 0; i < 8; i++) trailer_str(&p, &n, &cap, e->mesName[i]);
    for (int i = 0; i < 12; i++) trailer_i32(&p, &n, &cap, e->winRect[i]);
    for (int i = 0; i < 16; i++) trailer_i32(&p, &n, &cap, e->nameRect[i]);
    trailer_i32(&p, &n, &cap, e->mesMode);
    trailer_i32(&p, &n, &cap, e->mesMode2);
    trailer_i32(&p, &n, &cap, e->mesAlpha);
    for (int c = 0; c < 2; c++) {
        trailer_str(&p, &n, &cap, e->bgmName[c]);
        trailer_i32(&p, &n, &cap, e->bgmLoop[c]);
    }
    *len = n;
    return p;
}

bool save_slot(EG *e, int slot) {
    if (e->th[0].wait != W_TEXT && e->th[0].wait != W_CHOICE) {
        eg_log(e, "can't save outside a message");
        return false;
    }
    u8 *block = eg_alloc(BLOCK);
    build_block(e, block);
    size_t na, nb;
    u8 *pa = lzss_pack(block, BLOCK, &na);
    u8 *pb = lzss_pack(block, BLOCK, &nb);
    size_t tl;
    u8 *tr = build_trailer(e, &tl);
    size_t total = 128 + 8 + na + nb + 1 + tl;
    u8 *f = eg_alloc(total);
    memcpy(f, banner, sizeof banner);
    wr32(f + 128, (u32)na);
    wr32(f + 132, (u32)nb);
    memcpy(f + 136, pa, na);
    memcpy(f + 136 + na, pb, nb);
    i8 seedByte = (i8)(time(NULL) % 255);
    u32 seed;
    eg_rand_seed(&seed, seedByte);
    xor_stream(f + 136, na + nb, &seed);
    f[136 + na + nb] = (u8)seedByte;
    memcpy(f + 137 + na + nb, tr, tl);
    char dir[2048];
    snprintf(dir, sizeof dir, "%s/Save", e->saveDir);
    eg_mkdirs(dir);
    char *path = save_path(e, slot);
    bool ok = eg_write_file(path, f, total);
    eg_log(e, "%s slot %d to %s", ok ? "saved" : "failed to save", slot, path);
    free(path);
    free(f);
    free(pa);
    free(pb);
    free(tr);
    free(block);
    ui_slots_changed(e);
    return ok;
}

static u8 *read_save(EG *e, int slot, u8 **block2, u8 **trailer, size_t *trailerLen) {
    char *path = save_path(e, slot);
    size_t n;
    u8 *d = eg_read_file(path, &n);
    free(path);
    if (!d || n < 137 || memcmp(d, banner, 20)) {
        free(d);
        return NULL;
    }
    u32 na = rd32(d + 128), nb = rd32(d + 132);
    if ((size_t)136 + na + nb + 1 > n) {
        free(d);
        return NULL;
    }
    u32 seed;
    eg_rand_seed(&seed, (i32)(int8_t)d[136 + na + nb]);
    xor_stream(d + 136, (size_t)na + nb, &seed);
    u8 *b1 = lzss_unpack(d + 136, na, BLOCK);
    if (block2) *block2 = lzss_unpack(d + 136 + na, nb, BLOCK);
    if (trailer) {
        size_t at = 137 + (size_t)na + nb;
        *trailerLen = n > at ? n - at : 0;
        *trailer = NULL;
        if (*trailerLen >= 8 && !memcmp(d + at, "VNDG", 4)) {
            *trailer = eg_alloc(*trailerLen);
            memcpy(*trailer, d + at, *trailerLen);
        } else *trailerLen = 0;
    }
    free(d);
    return b1;
}

bool save_exists(EG *e, int slot) {
    char *path = save_path(e, slot);
    bool ok = eg_file_exists(path);
    free(path);
    return ok;
}

bool save_info(EG *e, int slot, SlotInfo *info, Image **thumb) {
    memset(info, 0, sizeof *info);
    if (!save_exists(e, slot)) return false;
    u8 *b = read_save(e, slot, NULL, NULL, NULL);
    if (!b) return false;
    info->used = true;
    info->year = rd16(b);
    info->month = rd16(b + 2);
    info->day = rd16(b + 6);
    info->hour = rd16(b + 8);
    info->minute = rd16(b + 10);
    info->second = rd16(b + 12);
    info->stamp = ((((long long)info->year * 13 + info->month) * 32 + info->day) * 24 + info->hour) * 3600 + info->minute * 60 + info->second;
    snprintf(info->text, sizeof info->text, "%s", (const char *)b + TEXT_BUF + 1);
    if (thumb) {
        Image *t = image_new(THUMB_W, THUMB_H);
        for (int y = 0; y < THUMB_H; y++) {
            const u8 *row = b + THUMB + (size_t)(THUMB_H - 1 - y) * THUMB_STRIDE;
            for (int x = 0; x < THUMB_W; x++) t->px[(size_t)y * THUMB_W + x] = (u32)row[x * 3 + 2] | (u32)row[x * 3 + 1] << 8 | (u32)row[x * 3] << 16 | 0xff000000u;
        }
        *thumb = t;
    }
    free(b);
    return true;
}

void save_delete(EG *e, int slot) {
    char *path = save_path(e, slot);
    char real[2048];
    if (eg_path_resolve(path, real, sizeof real) >= 0) remove(real);
    free(path);
    ui_slots_changed(e);
}

static void read_vars(EG *e, const u8 *b) {
    for (int i = e->vars.count - 1; i >= 0; i--)
        if (e->vars.vars[i].name[0] == '_') var_remove(&e->vars, e->vars.vars[i].name);
    for (int s = 0; s < INT_SLOTS; s++) {
        const u8 *r = b + INT_TABLE + (size_t)s * INT_REC;
        if (!r[0]) continue;
        char name[68];
        snprintf(name, sizeof name, "%s", (const char *)r + 1);
        Var *v = var_get(&e->vars, name, true);
        v->str = false;
        for (int k = 0; k < 100; k++) {
            i32 x = (i32)rd32(r + 68 + k * 4);
            if (x || k < v->n) var_at(v, k)->i = x;
        }
    }
    for (int s = 0; s < STR_SLOTS; s++) {
        const u8 *r = b + STR_TABLE + (size_t)s * STR_REC;
        if (!r[0]) continue;
        char name[64], val[257];
        snprintf(name, sizeof name, "%s", (const char *)r + 1);
        snprintf(val, sizeof val, "%s", (const char *)r + 64);
        vm_set_str_name(e, name, val);
    }
}

static int rdi(const u8 *t, size_t n, size_t *at) {
    if (*at + 4 > n) return 0;
    int v = (int)(i32)rd32(t + *at);
    *at += 4;
    return v;
}

static void rds(const u8 *t, size_t n, size_t *at, char *out, size_t cap) {
    out[0] = 0;
    if (*at + 2 > n) return;
    size_t l = rd16(t + *at);
    *at += 2;
    if (*at + l > n) return;
    size_t c = l < cap - 1 ? l : cap - 1;
    memcpy(out, t + *at, c);
    out[c] = 0;
    *at += l;
}

static void restore_scene(EG *e, const u8 *b, const u8 *t, size_t tn) {
    for (int p = 0; p < EG_PLANES; p++) {
        gfx_free_plane(e, p);
        gfx_layer_reset(e, p);
    }
    movie_close_all(e);
    if (t) {
        size_t at = 8;
        int sp = rdi(t, tn, &at);
        for (int i = 0; i < sp && i < EG_STACK; i++) {
            char file[128];
            rds(t, tn, &at, file, sizeof file);
            int off = rdi(t, tn, &at);
            Script *s = vm_script(e, file);
            e->th[0].stack[i].sc = s;
            e->th[0].stack[i].pc = s ? script_at_offset(s, off) : 0;
        }
        e->th[0].sp = sp < EG_STACK ? sp : EG_STACK;
        for (int l = 0; l < EG_PLANES; l++) {
            Layer *ly = &e->layer[l];
            int v[12];
            for (int k = 0; k < 12; k++) v[k] = rdi(t, tn, &at);
            ly->vis = v[0];
            ly->plane = v[1];
            ly->dx = v[2];
            ly->dy = v[3];
            ly->dw = v[4];
            ly->dh = v[5];
            ly->sx = v[6];
            ly->sy = v[7];
            ly->sw = v[8];
            ly->sh = v[9];
            ly->alpha = v[10];
            ly->movie = 0;
            char name[64];
            rds(t, tn, &at, name, sizeof name);
            if (name[0] && !strstr(name, ".mpg")) gfx_load(e, name, l);
        }
        for (int i = 0; i < 8; i++) {
            char name[64];
            rds(t, tn, &at, name, sizeof name);
            if (name[0]) msg_window_parts(e, i, name);
        }
        for (int i = 0; i < 12; i++) e->winRect[i] = rdi(t, tn, &at);
        for (int i = 0; i < 16; i++) e->nameRect[i] = rdi(t, tn, &at);
        e->mesMode = rdi(t, tn, &at);
        e->mesMode2 = rdi(t, tn, &at);
        e->mesAlpha = rdi(t, tn, &at);
        for (int c = 0; c < 2; c++) {
            char name[64];
            rds(t, tn, &at, name, sizeof name);
            int loop = rdi(t, tn, &at);
            if (name[0] && strcmp(name, "0")) snd_play(e, name, loop ? 1 : 0, c, 0);
        }
        return;
    }
    e->th[0].sp = 0;
    for (int p = 0; p < EG_PLANES; p++) {
        const char *name = (const char *)b + PLANE_NAMES + p * 64;
        if (name[0] && !strstr(name, ".mpg") && p != 20 && p != 21) gfx_load(e, name, p);
    }
    const u8 *r = b + LAYER_BG;
    int bgr[8] = {(int)(i32)rd32(r + 4), (int)(i32)rd32(r + 8), (int)(rd32(r + 12) & 0xffff), (int)(rd32(r + 12) >> 16),
                  (int)(i32)rd32(r + 16), (int)(i32)rd32(r + 20), (int)(rd32(r + 24) & 0xffff), (int)(rd32(r + 24) >> 16)};
    if (bgr[2] > 0) gfx_set_layer(e, 2, (int)(i32)rd32(r), bgr, (int)(i32)rd32(r + 40));
    for (int l = 3; l < EG_PLANES; l++) {
        const u8 *q = b + LAYER_BG + (size_t)(l - 2) * 0x5E00;
        if (q + 40 > b + BLOCK || rd32(q) != 1) continue;
        int lr[8] = {(int)(i32)rd32(q + 8), (int)(i32)rd32(q + 12), (int)(rd32(q + 16) & 0xffff), (int)(rd32(q + 16) >> 16),
                     (int)(i32)rd32(q + 20), (int)(i32)rd32(q + 24), (int)(rd32(q + 28) & 0xffff), (int)(rd32(q + 28) >> 16)};
        if (lr[2] > 0) gfx_set_layer(e, l, (int)(i32)rd32(q + 4), lr, (int)(i32)rd32(q + 32));
    }
    for (int i = 0; i < 7; i++) {
        char name[32];
        snprintf(name, sizeof name, "%s", (const char *)b + MES_PARTS + i * 32);
        if (name[0]) msg_window_parts(e, i, name);
    }
    e->mesMode = 1;
    e->mesAlpha = 255;
    for (int c = 0; c < 2; c++) {
        const char *name = (const char *)b + SOUND_NAMES + c * 64;
        if (name[0] && strcmp(name, "0")) snd_play(e, name, 1, c, 0);
    }
}

bool load_slot(EG *e, int slot) {
    u8 *trailer = NULL;
    size_t tn = 0;
    u8 *b = read_save(e, slot, NULL, &trailer, &tn);
    if (!b) return false;
    char file[64];
    snprintf(file, sizeof file, "%s", (const char *)b + THREAD_POS + 4);
    int off = (int)rd32(b + THREAD_POS);
    Script *s = vm_script(e, file);
    if (!s) {
        free(b);
        free(trailer);
        return false;
    }
    snd_stop_all(e);
    msg_clear(e);
    for (int i = 1; i < EG_THREADS; i++) e->th[i].active = false;
    read_vars(e, b);
    restore_scene(e, b, trailer, tn);
    Thread *t = &e->th[0];
    t->active = true;
    t->sc = s;
    t->pc = script_at_offset(s, off);
    t->wait = W_NONE;
    e->choiceCount = 0;
    e->winHidden = false;
    gfx_present(e);
    eg_log(e, "loaded slot %d: %s at %d", slot, file, off);
    free(b);
    free(trailer);
    return true;
}

void save_capture_thumb(EG *e) {
    image_free(e->thumb);
    e->thumb = image_new(THUMB_W, THUMB_H);
    for (int y = 0; y < THUMB_H; y++)
        for (int x = 0; x < THUMB_W; x++) {
            int sx = x * e->W / THUMB_W, sy = y * e->H / THUMB_H;
            e->thumb->px[(size_t)y * THUMB_W + x] = e->screen[(size_t)sy * e->W + sx];
        }
}

static char *config_path(EG *e) { return eg_fmt("%s/Save/VNDegen.cfg", e->saveDir); }

void config_load(EG *e) {
    char *path = config_path(e);
    size_t n;
    u8 *d = eg_read_file(path, &n);
    free(path);
    if (!d) return;
    Ini ini;
    ini_parse(&ini, d, n);
    free(d);
    struct {
        const char *k;
        int *v;
    } items[] = {{"TextSpeed", &e->textSpeedSlider}, {"AutoSpeed", &e->autoSpeed}, {"TextWhite", &e->textWhite}, {"SkipUnread", &e->skipUnread},
                 {"AutoWaitVoice", &e->autoWaitVoice}, {"Mute", &e->mute}, {"Music", &e->volBgm}, {"Voice", &e->volVoice},
                 {"Sound", &e->volSe}, {"Movie", &e->volMovie}};
    for (size_t i = 0; i < sizeof items / sizeof *items; i++) {
        int v;
        if (ini_ints(&ini, items[i].k, &v, 1) == 1) *items[i].v = v;
    }
    for (int i = 0; i < ini.count; i++) {
        const char *k = ini.keys[i];
        if (k[0] != ':' && k[0] != '@') continue;
        char name[96];
        int idx = 0;
        snprintf(name, sizeof name, "%s", k);
        char *br = strchr(name, '[');
        if (br) {
            idx = atoi(br + 1);
            *br = 0;
        }
        const char *v = ini.values[i];
        if (v[0] == '"') {
            char s[300];
            snprintf(s, sizeof s, "%s", v + 1);
            size_t l = strlen(s);
            if (l && s[l - 1] == '"') s[l - 1] = 0;
            char *sj = eg_utf8_to_sjis(s);
            vm_set_str_name(e, name, sj);
            free(sj);
        } else var_at(var_get(&e->vars, name, true), idx)->i = atoi(v);
    }
    ini_free(&ini);
    e->textSpeed = 40 - e->textSpeedSlider * 40 / 100;
}

void config_save(EG *e) {
    size_t cap = 4096, n = 0;
    char *out = eg_alloc(cap);
#define PUT(...)                                                                    \
    do {                                                                            \
        int w = snprintf(NULL, 0, __VA_ARGS__);                                     \
        if (n + (size_t)w + 1 > cap) {                                              \
            cap = (cap + (size_t)w + 1) * 2;                                        \
            out = eg_realloc(out, cap);                                             \
        }                                                                           \
        n += (size_t)snprintf(out + n, cap - n, __VA_ARGS__);                       \
    } while (0)
    PUT("TextSpeed=%d\nAutoSpeed=%d\nTextWhite=%d\nSkipUnread=%d\nAutoWaitVoice=%d\nMute=%d\nMusic=%d\nVoice=%d\nSound=%d\nMovie=%d\n",
        e->textSpeedSlider, e->autoSpeed, e->textWhite, e->skipUnread, e->autoWaitVoice, e->mute, e->volBgm, e->volVoice, e->volSe, e->volMovie);
    for (int i = 0; i < e->vars.count; i++) {
        Var *v = &e->vars.vars[i];
        if (v->name[0] != ':' && v->name[0] != '@') continue;
        bool any = false;
        for (int k = 0; k < v->n; k++) {
            if (v->v[k].s) {
                char *u = eg_sjis_to_utf8(v->v[k].s);
                PUT("%s[%d]=\"%s\"\n", v->name, k, u);
                free(u);
                any = true;
            } else if (v->v[k].i) {
                PUT("%s[%d]=%d\n", v->name, k, v->v[k].i);
                any = true;
            }
        }
        if (!any) PUT("%s[0]=0\n", v->name);
    }
#undef PUT
    char dir[2048];
    snprintf(dir, sizeof dir, "%s/Save", e->saveDir);
    eg_mkdirs(dir);
    char *path = config_path(e);
    eg_write_file(path, out, n);
    free(path);
    free(out);
    e->globalDirty = false;
}

#define GLOBAL_BLOCK 0x10B0C8
#define GLOBAL_INTS 1228

static void import_pc_globals(EG *e) {
    char path[2048];
    snprintf(path, sizeof path, "%s/Save/GlobalSave.sav", e->saveDir);
    size_t n;
    u8 *d = eg_read_file(path, &n);
    if (!d || n < 134 || memcmp(d, banner, 20)) {
        free(d);
        return;
    }
    u32 na = rd32(d + 128);
    if ((size_t)132 + na + 1 > n) {
        free(d);
        return;
    }
    u32 seed;
    eg_rand_seed(&seed, (i32)(int8_t)d[132 + na]);
    xor_stream(d + 132, na, &seed);
    u8 *g = lzss_unpack(d + 132, na, GLOBAL_BLOCK);
    free(d);
    int count = 0;
    for (int s = 0; s < INT_SLOTS; s++) {
        const u8 *r = g + GLOBAL_INTS + (size_t)s * INT_REC;
        if (!r[0] || r[1] != '@') continue;
        char name[68];
        snprintf(name, sizeof name, "%s", (const char *)r + 1);
        Var *v = var_get(&e->vars, name, true);
        for (int k = 0; k < 100; k++) {
            i32 x = (i32)rd32(r + 68 + k * 4);
            if (x) var_at(v, k)->i = x;
        }
        var_at(v, 0);
        count++;
    }
    free(g);
    eg_log(e, "read %d globals from the PC's GlobalSave.sav", count);
}

void global_load(EG *e) {
    config_load(e);
    import_pc_globals(e);
}

void global_save(EG *e) { config_save(e); }

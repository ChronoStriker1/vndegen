// The PC engine's save files. Every .npf is an NPSF container: "NPSF", the unpacked size, an encrypted flag and a
// compressed flag, then a zlib stream whose first 4096 bytes went through the substitution table below after adding a
// key byte made from the file's name. The layout and the table come from saves made by saya.exe under Wine and the
// routine that writes them (docs/n2system-cleanroom.md).
//
// Inside: NPSG (common/val.npf, the '#' variables) and NPSL (each slot's val.npf, the '$' variables) hold 1024-byte
// records: a type byte (0 flag or unset, 1 string, 2 number), the name, a NUL and three zero bytes, the value as a
// double, then the value as a string. NPSI (thum.npf) is the thumbnail.
#include "n2.h"

#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#define NPS_TAG 0xf1f2b5c5u

static const u8 kTable[256] = {
    0x85,0xa5,0xd2,0x76,0x97,0x79,0x02,0x45,0x4f,0xa4,0x35,0xf7,0x74,0xa9,0x4d,0x42,
    0x43,0x40,0xbc,0xe3,0x07,0x38,0xf4,0xfa,0x34,0x01,0x72,0x5c,0x10,0x06,0xe7,0x54,
    0x1d,0x5e,0x71,0xd6,0x92,0x4b,0x8c,0x3a,0xa6,0xe9,0x1b,0xb5,0xe6,0x31,0x93,0x46,
    0x94,0x9e,0x29,0xd9,0x64,0x77,0x08,0x11,0xeb,0x5a,0x47,0xb0,0xee,0x51,0x73,0x69,
    0x22,0x75,0x78,0x53,0xc8,0xab,0x37,0x5d,0xad,0xac,0x87,0x20,0x62,0x9a,0x6b,0x9c,
    0x21,0xce,0xf6,0xfe,0xa2,0xd0,0x2d,0x8a,0x96,0x00,0x0a,0x9d,0xbb,0x6e,0xf0,0x99,
    0xd4,0xfb,0x58,0x4a,0x17,0x80,0x6f,0x3b,0xb1,0x05,0x2f,0x6a,0xbf,0xa1,0xc7,0x8e,
    0xff,0x55,0xd7,0xd5,0x60,0x57,0x2a,0xc6,0x3d,0xf2,0x19,0xcb,0xb7,0x0d,0xed,0x86,
    0xe0,0xc5,0x28,0x66,0xaf,0x7d,0xea,0x1c,0x9b,0xc1,0x3e,0x6c,0xe1,0xc0,0x65,0x84,
    0xe8,0x7b,0x7c,0xf8,0x82,0x52,0xcf,0x0c,0xf9,0x81,0x8d,0x4e,0x26,0x59,0x2b,0x5f,
    0x5b,0x7f,0x49,0xaa,0xb2,0x16,0xb8,0x18,0xa7,0x41,0x8f,0xc9,0x27,0x0e,0xa3,0xd8,
    0x68,0x04,0x25,0xbe,0x24,0xf3,0x89,0xf1,0xbd,0xec,0x03,0xa0,0x9f,0xd3,0xfc,0xc2,
    0x13,0xcd,0x2e,0x39,0xf5,0x15,0x33,0x1e,0xdd,0xe4,0x1f,0xdb,0x48,0xca,0xde,0xae,
    0xe5,0xda,0x12,0x1a,0x67,0xdc,0xb9,0x23,0x50,0x56,0x2c,0x95,0x61,0xcc,0x8b,0xef,
    0x83,0x09,0xba,0x14,0x30,0x32,0x0f,0x7a,0x63,0x88,0xfd,0x4c,0x98,0x36,0xb4,0x3f,
    0x0b,0xb3,0x70,0xa8,0xdf,0x91,0xd1,0x44,0xc4,0xc3,0xe2,0x7e,0x6d,0xb6,0x90,0x3c,
};

static u8 name_key(const char *path) {
    const char *base = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;
    u32 k = 0x19780131u;
    size_t len = strlen(base);
    for (size_t i = 0; i < len; i++) k -= (u8)base[i];
    return (u8)(k * (u32)len);
}

static void put32(u8 *p, u32 v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

static u32 get32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

bool npsf_write(const char *path, const u8 *data, size_t size) {
    uLongf cap = compressBound((uLong)size);
    u8 *out = n2_alloc(16 + cap);
    if (compress2(out + 16, &cap, data, (uLong)size, Z_DEFAULT_COMPRESSION) != Z_OK) {
        free(out);
        return false;
    }
    memcpy(out, "NPSF", 4);
    put32(out + 4, (u32)size);
    put32(out + 8, 1);
    put32(out + 12, 1);
    u8 key = name_key(path);
    for (uLongf i = 0; i < cap && i < 4096; i++) out[16 + i] = kTable[(u8)(out[16 + i] + key)];
    char *tmp = n2_fmt("%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    bool ok = f && fwrite(out, 1, 16 + cap, f) == 16 + cap;
    if (f) ok = fclose(f) == 0 && ok;
    if (ok) ok = rename(tmp, path) == 0;
    else unlink(tmp);
    free(tmp);
    free(out);
    return ok;
}

u8 *npsf_read(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 16) {
        fclose(f);
        return NULL;
    }
    u8 *raw = n2_alloc((size_t)n);
    bool ok = fread(raw, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    if (!ok || memcmp(raw, "NPSF", 4) != 0) {
        free(raw);
        return NULL;
    }
    u32 usize = get32(raw + 4), enc = get32(raw + 8), comp = get32(raw + 12);
    u8 *body = raw + 16;
    size_t blen = (size_t)n - 16;
    if (enc) {
        u8 inv[256];
        for (int i = 0; i < 256; i++) inv[kTable[i]] = (u8)i;
        u8 key = name_key(path);
        for (size_t i = 0; i < blen && i < 4096; i++) body[i] = (u8)(inv[body[i]] - key);
    }
    u8 *out = n2_alloc((size_t)usize + 1);
    if (comp) {
        uLongf ol = usize;
        if (uncompress(out, &ol, body, (uLong)blen) != Z_OK || ol != usize) {
            free(out);
            free(raw);
            return NULL;
        }
    } else {
        memcpy(out, body, blen < usize ? blen : usize);
    }
    free(raw);
    *size = usize;
    return out;
}

// ---- paths ----

// Script paths into the save folder: "#SYSTEM_save_path/0001/date.npf" or a relative "0001/date.npf".
static char *save_path(N2 *n, const char *path) {
    size_t sl = strlen(n->saveDir);
    char *p;
    if (strncmp(path, n->saveDir, sl) == 0) p = n2_strdup(path);
    else p = n2_fmt("%s/%s", n->saveDir, path);
    for (char *q = p; *q; q++)
        if (*q == '\\') *q = '/';
    return p;
}

static char *slot_dir(N2 *n, int slot) { return n2_fmt("%s/%04d", n->saveDir, slot); }

static void mkdir_p(const char *path) {
    char *p = n2_strdup(path);
    for (char *q = p + 1; *q; q++) {
        if (*q == '/') {
            *q = 0;
            mkdir(p, 0755);
            *q = '/';
        }
    }
    mkdir(p, 0755);
    free(p);
}

u8 *save_read_file(N2 *n, const char *path, size_t *size) {
    char *p = save_path(n, path);
    u8 *d = npsf_read(p, size);
    free(p);
    return d;
}

void save_write_file(N2 *n, const char *path, const u8 *data, size_t size) {
    char *p = save_path(n, path);
    char *slash = strrchr(p, '/');
    if (slash) {
        *slash = 0;
        mkdir_p(p);
        *slash = '/';
    }
    if (!npsf_write(p, data, size)) n2_log(n, "can't write %s", p);
    free(p);
}

// ---- variables ----

typedef struct VarRef {
    const char *name;
    Var *var;
} VarRef;

static int by_order(const void *a, const void *b) {
    const VarRef *x = a, *y = b;
    return x->var->order < y->var->order ? -1 : x->var->order > y->var->order;
}

static Buf write_vars(N2 *n, char sigil, const char *magic) {
    VarRef *refs = n2_alloc(sizeof(VarRef) * (size_t)(n->vars.count + 1));
    int count = 0;
    for (int i = 0; i < n->vars.cap; i++) {
        MapEntry *e = &n->vars.entries[i];
        if (!e->key || !e->value || e->key[0] != sigil) continue;
        // The save path is the PC engine's own; VNDegen sets it at start.
        refs[count++] = (VarRef){e->key, e->value};
    }
    qsort(refs, (size_t)count, sizeof(VarRef), by_order);
    Buf b = {0};
    buf_str(&b, magic);
    buf_u32(&b, NPS_TAG);
    buf_u32(&b, 16 + 1024u * (u32)count);
    buf_u32(&b, (u32)count);
    for (int i = 0; i < count; i++) {
        u8 rec[1024] = {0};
        Value *v = &refs[i].var->v;
        size_t nl;
        char *name = n2_utf8_to_sjis(refs[i].name, &nl);
        if (nl > 500) nl = 500;
        rec[0] = v->t == V_STR ? 1 : v->t == V_NUM ? 2 : 0;
        memcpy(rec + 1, name, nl);
        size_t at = 1 + nl + 1 + 3;
        double d = v->t == V_STR ? 0 : v->n;
        memcpy(rec + at, &d, 8);
        at += 8;
        if (v->t == V_STR) {
            size_t sl;
            char *s = n2_utf8_to_sjis(v->s->s, &sl);
            if (sl > 1024 - at - 1) sl = 1024 - at - 1;
            memcpy(rec + at, s, sl);
            free(s);
        }
        free(name);
        buf_put(&b, rec, 1024);
    }
    free(refs);
    return b;
}

static void read_vars(N2 *n, const u8 *d, size_t size) {
    if (size < 16) return;
    u32 count = get32(d + 12);
    for (u32 i = 0; i < count && 16 + (size_t)(i + 1) * 1024 <= size; i++) {
        const u8 *r = d + 16 + (size_t)i * 1024;
        const u8 *nul = memchr(r + 1, 0, 1023);
        if (!nul) continue;
        size_t nl = (size_t)(nul - r - 1);
        char *name = n2_sjis_to_utf8(r + 1, nl, NULL);
        size_t at = 1 + nl + 1 + 3;
        if (at + 8 > 1024) {
            free(name);
            continue;
        }
        double num;
        memcpy(&num, r + at, 8);
        Value v;
        if (r[0] == 1) {
            const u8 *s = r + at + 8;
            const u8 *e = memchr(s, 0, (size_t)(r + 1024 - s));
            char *u = n2_sjis_to_utf8(s, e ? (size_t)(e - s) : (size_t)(r + 1024 - s), NULL);
            v = v_str(u);
            free(u);
        } else if (r[0] == 2) {
            v = v_num(num);
        } else {
            v = num != 0 ? v_bool(true) : v_null();
        }
        var_set(n, name, &v);
        v_free(&v);
        free(name);
    }
}

void save_globals(N2 *n) {
    char *dir = n2_fmt("%s/common", n->saveDir);
    mkdir_p(dir);
    Buf b = write_vars(n, '#', "NPSG");
    char *path = n2_fmt("%s/val.npf", dir);
    if (!npsf_write(path, b.data, b.len)) n2_log(n, "can't write %s", path);
    free(path);
    buf_free(&b);
    free(dir);
}

void load_globals(N2 *n) {
    char *path = n2_fmt("%s/common/val.npf", n->saveDir);
    size_t size;
    u8 *d = npsf_read(path, &size);
    if (d && size >= 4 && memcmp(d, "NPSG", 4) == 0) read_vars(n, d, size);
    free(d);
    free(path);
}

// ---- thumbnails ----

// NPSI: "NPSI", the tag, a 16-bit format word (19), width twice, height twice, then BGRA pixels, rows top first.
Image *save_read_thumbnail(N2 *n, const char *path) {
    size_t size;
    u8 *d = save_read_file(n, path, &size);
    if (!d || size < 26 || memcmp(d, "NPSI", 4) != 0) {
        free(d);
        return NULL;
    }
    u32 w = get32(d + 10), h = get32(d + 18);
    if (w == 0 || h == 0 || w > 4096 || h > 4096 || 26 + (size_t)w * h * 4 > size) {
        free(d);
        return NULL;
    }
    Image *img = img_new((int)w, (int)h);
    const u8 *p = d + 26;
    for (size_t i = 0; i < (size_t)w * h; i++)
        img->px[i] = (u32)p[i * 4 + 2] | (u32)p[i * 4 + 1] << 8 | (u32)p[i * 4] << 16 | 0xff000000u;
    free(d);
    return img;
}

static void write_thumbnail(N2 *n, const char *path, int w, int h) {
    u32 *screen = n2_alloc(sizeof(u32) * N2_W * N2_H);
    n2_render_to(n, screen);
    Buf b = {0};
    buf_str(&b, "NPSI");
    buf_u32(&b, NPS_TAG);
    u8 fmt[2] = {19, 0};
    buf_put(&b, fmt, 2);
    buf_u32(&b, (u32)w);
    buf_u32(&b, (u32)w);
    buf_u32(&b, (u32)h);
    buf_u32(&b, (u32)h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // Box-average the screen pixels this thumbnail pixel covers.
            int x0 = x * N2_W / w, x1 = (x + 1) * N2_W / w, y0 = y * N2_H / h, y1 = (y + 1) * N2_H / h;
            u32 r = 0, g = 0, bl = 0, c = 0;
            for (int sy = y0; sy < y1; sy++)
                for (int sx = x0; sx < x1; sx++) {
                    u32 p = screen[sy * N2_W + sx];
                    r += p & 0xff;
                    g += p >> 8 & 0xff;
                    bl += p >> 16 & 0xff;
                    c++;
                }
            if (!c) c = 1;
            u8 px[4] = {(u8)(bl / c), (u8)(g / c), (u8)(r / c), 0xff};
            buf_put(&b, px, 4);
        }
    }
    save_write_file(n, path, b.data, b.len);
    buf_free(&b);
    free(screen);
}

// ---- slots ----

bool save_exists(N2 *n, int slot) {
    char *dir = slot_dir(n, slot);
    char *path = n2_fmt("%s/val.npf", dir);
    struct stat st;
    bool ok = stat(path, &st) == 0;
    free(path);
    free(dir);
    return ok;
}

void save_delete(N2 *n, int slot) {
    char *dir = slot_dir(n, slot);
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d))) {
            if (de->d_name[0] == '.') continue;
            char *p = n2_fmt("%s/%s", dir, de->d_name);
            unlink(p);
            free(p);
        }
        closedir(d);
        rmdir(dir);
    }
    free(dir);
}

bool save_slot(N2 *n, int slot) {
    char *dir = slot_dir(n, slot);
    mkdir_p(dir);
    Buf vars = write_vars(n, '$', "NPSL");
    char *path = n2_fmt("%s/val.npf", dir);
    bool ok = npsf_write(path, vars.data, vars.len);
    free(path);
    buf_free(&vars);
    Buf state = slot_state_write(n);
    path = n2_fmt("%s/vndegen.npf", dir);
    ok = npsf_write(path, state.data, state.len) && ok;
    free(path);
    buf_free(&state);
    if (slot != 0 && slot != 9999) {
        Value *tw = var_get(n, "#SYSTEM_save_thumbnail_width"), *th = var_get(n, "#SYSTEM_save_thumbnail_height");
        int w = tw ? (int)v_tonum(tw) : 100, h = th ? (int)v_tonum(th) : 75;
        if (w <= 0 || w > 800) w = 100;
        if (h <= 0 || h > 600) h = 75;
        char *rel = n2_fmt("%04d/thum.npf", slot);
        write_thumbnail(n, rel, w, h);
        free(rel);
    }
    save_globals(n);
    free(dir);
    n2_log(n, "saved slot %d: %s", slot, ok ? "ok" : "failed");
    return ok;
}

bool load_slot(N2 *n, int slot) {
    char *dir = slot_dir(n, slot);
    char *path = n2_fmt("%s/vndegen.npf", dir);
    size_t size;
    u8 *state = npsf_read(path, &size);
    free(path);
    path = n2_fmt("%s/val.npf", dir);
    size_t vsize;
    u8 *vars = npsf_read(path, &vsize);
    free(path);
    free(dir);
    bool ok = false;
    if (state && vars && vsize >= 4 && memcmp(vars, "NPSL", 4) == 0) {
        ok = slot_state_read(n, state, size, vars, vsize);
    } else {
        n2_log(n, "slot %d has no state VNDegen can load", slot);
    }
    free(state);
    free(vars);
    return ok;
}

void slot_read_vars(N2 *n, const u8 *d, size_t size) { read_vars(n, d, size); }

// ---- read text, for skip ----

void conquest_mark(N2 *n, Script *s, TextBlock *b) {
    char *key = n2_fmt("%s|%s", s->path, b->name);
    map_set(&n->conquest, key, (void *)1);
    free(key);
}

bool conquest_read(N2 *n, const char *file, const char *scene) {
    // Whether every text block of the file has been shown.
    Script *s = n2_load_script(n, file);
    if (!s || !s->blockCount) return false;
    for (int i = 0; i < s->blockCount; i++) {
        char *key = n2_fmt("%s|%s", s->path, s->blocks[i].name);
        bool seen = map_get(&n->conquest, key) != NULL;
        free(key);
        if (!seen) return false;
    }
    return true;
}

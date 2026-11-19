#include "ex.h"
#include "ex_casepath.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>

void *ex_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) abort();
    return p;
}

void *ex_realloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) abort();
    return q;
}

char *ex_strdup(const char *s) {
    size_t n = strlen(s);
    char *d = ex_alloc(n + 1);
    memcpy(d, s, n);
    return d;
}

char *ex_fmt(const char *fmt, ...) {
    va_list ap, aq;
    va_start(ap, fmt);
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *s = ex_alloc((size_t)n + 1);
    vsnprintf(s, (size_t)n + 1, fmt, aq);
    va_end(aq);
    return s;
}

int ex_path_resolve(const char *path, char *out, size_t size) { return ex_casepath_resolve(path, out, size); }

u8 *ex_read_file(const char *path, size_t *size) {
    char real[2048];
    if (ex_path_resolve(path, real, sizeof real) < 0) return NULL;
    FILE *f = fopen(real, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    u8 *d = ex_alloc((size_t)n + 1);
    if (fread(d, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(d); return NULL; }
    fclose(f);
    if (size) *size = (size_t)n;
    return d;
}

bool ex_mkdirs(const char *dir) {
    char buf[2048];
    int found = ex_path_resolve(dir, buf, sizeof buf);
    if (found < 0) return false;
    if (found > 0) return true;
    for (char *p = buf + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        mkdir(buf, 0755);
        *p = '/';
    }
    return mkdir(buf, 0755) == 0 || errno == EEXIST;
}

bool ex_write_file(const char *path, const void *data, size_t size) {
    char real[2048];
    if (ex_path_resolve(path, real, sizeof real) < 0) return false;
    char tmp[2100];
    snprintf(tmp, sizeof tmp, "%s.tmp", real);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    if (!ok || rename(tmp, real) != 0) { remove(tmp); return false; }
    ex_casepath_changed(real);
    return true;
}

bool ex_ieq(const char *a, const char *b) {
    for (; *a && *b; a++, b++)
        if (tolower((u8)*a) != tolower((u8)*b)) return false;
    return !*a && !*b;
}

extern const uint16_t ex_sjis_table[11340];

static bool sjis_lead(u8 c) { return (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc); }

int ex_sjis_decode(const u8 *p, u32 *cp) {
    u8 c = p[0];
    if (c < 0x80) { *cp = c; return 1; }
    if (c >= 0xa1 && c <= 0xdf) { *cp = 0xff61 + (c - 0xa1); return 1; }
    if (sjis_lead(c) && p[1] >= 0x40 && p[1] <= 0xfc) {
        int lead = c <= 0x9f ? c - 0x81 : c - 0xe0 + 31;
        u16 v = ex_sjis_table[lead * 189 + (p[1] - 0x40)];
        *cp = v ? v : 0xfffd;
        return 2;
    }
    *cp = 0xfffd;
    return 1;
}

static int utf8_put(char *o, u32 cp) {
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp < 0x800) { o[0] = (char)(0xc0 | cp >> 6); o[1] = (char)(0x80 | (cp & 0x3f)); return 2; }
    o[0] = (char)(0xe0 | cp >> 12);
    o[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
    o[2] = (char)(0x80 | (cp & 0x3f));
    return 3;
}

char *ex_sjis_to_utf8(const char *s) {
    size_t n = strlen(s);
    char *out = ex_alloc(n * 3 + 1), *o = out;
    for (const u8 *p = (const u8 *)s; *p;) {
        u32 cp;
        p += ex_sjis_decode(p, &cp);
        o += utf8_put(o, cp);
    }
    *o = 0;
    return out;
}

static u16 *sjis_reverse;

char *ex_utf8_to_sjis(const char *s) {
    if (!sjis_reverse) {
        u16 *r = ex_alloc(sizeof(u16) * 0x10000);
        for (int lead = 0; lead < 60; lead++)
            for (int t = 0; t < 189; t++) {
                u16 cp = ex_sjis_table[lead * 189 + t];
                if (!cp || r[cp]) continue;
                int b1 = lead < 31 ? 0x81 + lead : 0xe0 + lead - 31;
                r[cp] = (u16)(b1 << 8 | (0x40 + t));
            }
        sjis_reverse = r;
    }
    size_t n = strlen(s);
    char *out = ex_alloc(n * 2 + 1), *o = out;
    const u8 *p = (const u8 *)s;
    while (*p) {
        u32 cp;
        u8 c = *p;
        int k = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : 4;
        cp = k == 1 ? c : k == 2 ? (u32)(c & 0x1f) : k == 3 ? (u32)(c & 0x0f) : (u32)(c & 0x07);
        for (int i = 1; i < k && p[i]; i++) cp = cp << 6 | (p[i] & 0x3f);
        p += k;
        if (cp < 0x80) *o++ = (char)cp;
        else if (cp >= 0xff61 && cp <= 0xff9f) *o++ = (char)(0xa1 + cp - 0xff61);
        else if (cp < 0x10000 && sjis_reverse[cp]) { *o++ = (char)(sjis_reverse[cp] >> 8); *o++ = (char)(sjis_reverse[cp] & 0xff); }
        else *o++ = '?';
    }
    *o = 0;
    return out;
}

int ex_sjis_len(const u8 *p) {
    if (sjis_lead(p[0]) && p[1]) return 2;
    return 1;
}

bool ex_file_exists(const char *path) {
    char real[2048];
    return ex_path_resolve(path, real, sizeof real) > 0;
}

void ex_log(EX *e, const char *fmt, ...) {
    if (!e || !e->log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(e->log, fmt, ap);
    va_end(ap);
    fputc('\n', e->log);
    fflush(e->log);
}


char *ex_game_path(EX *e, const char *rel) {
    char *p = ex_fmt("%s/%s", e->gameDir, rel);
    for (char *c = p + strlen(e->gameDir); *c; c++)
        if (*c == '\\') *c = '/';
    return p;
}

void mt_seed(MT *m, u32 seed) {
    for (int i = 0; i < 624; i++) {
        u32 up = seed & 0xffff0000u;
        seed = seed * 69069u + 1;
        m->mt[i] = up | (seed & 0xffff0000u) >> 16;
        seed = seed * 69069u + 1;
    }
    m->i = 624;
}

u32 mt_next(MT *m) {
    if (m->i >= 624) {
        for (int k = 0; k < 624; k++) {
            u32 y = (m->mt[k] & 0x80000000u) | (m->mt[(k + 1) % 624] & 0x7fffffffu);
            m->mt[k] = m->mt[(k + 397) % 624] ^ (y >> 1) ^ ((y & 1) ? 0x9908b0dfu : 0);
        }
        m->i = 0;
    }
    u32 y = m->mt[m->i++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9d2c5680u;
    y ^= (y << 15) & 0xefc60000u;
    y ^= y >> 18;
    return y;
}

int split_csv(char *s, char **out, int max) {
    int n = 0;
    if (!s) return 0;
    for (char *p = s;;) {
        if (n < max) out[n++] = p;
        char *c = strchr(p, ',');
        if (!c) break;
        *c = 0;
        p = c + 1;
    }
    return n;
}

int field_int(const char *s) {
    if (!s) return 0;
    while (*s == ' ') s++;
    return (int)strtol(s, NULL, 0);
}

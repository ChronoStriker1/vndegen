#include "br.h"
#include "br_casepath.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <zlib.h>

void *br_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) abort();
    return p;
}

void *br_realloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) abort();
    return q;
}

char *br_strdup(const char *s) {
    size_t n = strlen(s);
    char *d = br_alloc(n + 1);
    memcpy(d, s, n);
    return d;
}

char *br_fmt(const char *fmt, ...) {
    va_list ap, aq;
    va_start(ap, fmt);
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *s = br_alloc((size_t)n + 1);
    vsnprintf(s, (size_t)n + 1, fmt, aq);
    va_end(aq);
    return s;
}

int br_path_resolve(const char *path, char *out, size_t size) { return br_casepath_resolve(path, out, size); }

u8 *br_read_file(const char *path, size_t *size) {
    char real[2048];
    if (br_path_resolve(path, real, sizeof real) < 0) return NULL;
    FILE *f = fopen(real, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    u8 *d = br_alloc((size_t)n + 1);
    if (fread(d, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(d); return NULL; }
    fclose(f);
    if (size) *size = (size_t)n;
    return d;
}

bool br_mkdirs(const char *dir) {
    char buf[2048];
    int found = br_path_resolve(dir, buf, sizeof buf);
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

bool br_write_file(const char *path, const void *data, size_t size) {
    char real[2048];
    if (br_path_resolve(path, real, sizeof real) < 0) return false;
    char tmp[2100];
    snprintf(tmp, sizeof tmp, "%s.tmp", real);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    if (!ok || rename(tmp, real) != 0) { remove(tmp); return false; }
    br_casepath_changed(real);
    return true;
}

bool br_ieq(const char *a, const char *b) {
    for (; *a && *b; a++, b++)
        if (tolower((u8)*a) != tolower((u8)*b)) return false;
    return !*a && !*b;
}

extern const uint16_t br_sjis_table[11340];

static bool sjis_lead(u8 c) { return (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc); }

int br_sjis_decode(const u8 *p, u32 *cp) {
    u8 c = p[0];
    if (c < 0x80) { *cp = c; return 1; }
    if (c >= 0xa1 && c <= 0xdf) { *cp = 0xff61 + (c - 0xa1); return 1; }
    if (sjis_lead(c) && p[1] >= 0x40 && p[1] <= 0xfc) {
        int lead = c <= 0x9f ? c - 0x81 : c - 0xe0 + 31;
        u16 v = br_sjis_table[lead * 189 + (p[1] - 0x40)];
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

char *br_sjis_to_utf8(const char *s) {
    size_t n = strlen(s);
    char *out = br_alloc(n * 3 + 1), *o = out;
    for (const u8 *p = (const u8 *)s; *p;) {
        u32 cp;
        p += br_sjis_decode(p, &cp);
        o += utf8_put(o, cp);
    }
    *o = 0;
    return out;
}

static u16 *sjis_reverse;

char *br_utf8_to_sjis(const char *s) {
    if (!sjis_reverse) {
        u16 *r = br_alloc(sizeof(u16) * 0x10000);
        for (int lead = 0; lead < 60; lead++)
            for (int t = 0; t < 189; t++) {
                u16 cp = br_sjis_table[lead * 189 + t];
                if (!cp || r[cp]) continue;
                int b1 = lead < 31 ? 0x81 + lead : 0xe0 + lead - 31;
                r[cp] = (u16)(b1 << 8 | (0x40 + t));
            }
        sjis_reverse = r;
    }
    size_t n = strlen(s);
    char *out = br_alloc(n * 2 + 1), *o = out;
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

int br_sjis_len(const u8 *p) {
    if (sjis_lead(p[0]) && p[1]) return 2;
    return 1;
}

bool br_file_exists(const char *path) {
    char real[2048];
    return br_path_resolve(path, real, sizeof real) > 0;
}

void br_log(BR *e, const char *fmt, ...) {
    if (!e || !e->log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(e->log, fmt, ap);
    va_end(ap);
    fputc('\n', e->log);
    fflush(e->log);
}


char *br_game_path(BR *e, const char *rel) {
    char *p = br_fmt("%s/%s", e->gameDir, rel);
    for (char *c = p + strlen(e->gameDir); *c; c++)
        if (*c == '\\') *c = '/';
    return p;
}

u8 *br_eenc_decode(const u8 *d, size_t n, size_t *outLen) {
    if (n < 8 || memcmp(d, "EEN", 3) || (d[3] != 'C' && d[3] != 'Z')) return NULL;
    u32 key = ((u32)d[4] | (u32)d[5] << 8 | (u32)d[6] << 16 | (u32)d[7] << 24) ^ 0xDEADBEEFu;
    size_t len = n - 8;
    u8 *body = br_alloc(len + 1);
    for (size_t i = 0; i < len; i++) body[i] = d[8 + i] ^ (u8)(key >> ((i & 3) * 8));
    if (d[3] == 'C') {
        *outLen = len;
        return body;
    }
    z_stream z = {0};
    if (inflateInit(&z) != Z_OK) {
        free(body);
        return NULL;
    }
    size_t cap = len * 4 + 1024, got = 0;
    u8 *out = br_alloc(cap + 1);
    z.next_in = body;
    z.avail_in = (uInt)len;
    int r;
    do {
        if (got == cap) {
            cap *= 2;
            out = br_realloc(out, cap + 1);
        }
        z.next_out = out + got;
        z.avail_out = (uInt)(cap - got);
        r = inflate(&z, Z_NO_FLUSH);
        got = cap - z.avail_out;
    } while (r == Z_OK);
    inflateEnd(&z);
    free(body);
    if (r != Z_STREAM_END) {
        free(out);
        return NULL;
    }
    out[got] = 0;
    *outLen = got;
    return out;
}

u8 *br_eenc_encode(const u8 *d, size_t n, bool compress, size_t *outLen) {
    u8 *body;
    size_t len;
    if (compress) {
        uLongf cap = compressBound((uLong)n);
        body = br_alloc(cap);
        compress2(body, &cap, d, (uLong)n, 9);
        len = cap;
    } else {
        body = br_alloc(n + 1);
        memcpy(body, d, n);
        len = n;
    }
    u32 key = (u32)n ^ 0xDEADBEEFu;
    u8 *out = br_alloc(len + 8);
    memcpy(out, compress ? "EENZ" : "EENC", 4);
    for (int i = 0; i < 4; i++) out[4 + i] = (u8)(n >> (i * 8));
    for (size_t i = 0; i < len; i++) out[8 + i] = body[i] ^ (u8)(key >> ((i & 3) * 8));
    free(body);
    *outLen = len + 8;
    return out;
}

u8 *br_read_game(BR *e, const char *rel, size_t *size) {
    char *p = br_game_path(e, rel);
    size_t n;
    u8 *d = br_read_file(p, &n);
    free(p);
    if (!d) return NULL;
    if (n >= 8 && !memcmp(d, "EEN", 3)) {
        size_t m;
        u8 *o = br_eenc_decode(d, n, &m);
        free(d);
        if (!o) return NULL;
        d = o;
        n = m;
    }
    if (size) *size = n;
    return d;
}

#include "anim.h"
#include "an_casepath.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>

void *an_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) abort();
    return p;
}

void *an_realloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) abort();
    return q;
}

char *an_strdup(const char *s) {
    size_t n = strlen(s);
    char *d = an_alloc(n + 1);
    memcpy(d, s, n);
    return d;
}

char *an_fmt(const char *fmt, ...) {
    va_list ap, aq;
    va_start(ap, fmt);
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *s = an_alloc((size_t)n + 1);
    vsnprintf(s, (size_t)n + 1, fmt, aq);
    va_end(aq);
    return s;
}

int an_path_resolve(const char *path, char *out, size_t size) { return an_casepath_resolve(path, out, size); }

u8 *an_read_file(const char *path, size_t *size) {
    char real[2048];
    if (an_path_resolve(path, real, sizeof real) < 0) return NULL;
    FILE *f = fopen(real, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    u8 *d = an_alloc((size_t)n + 1);
    if (fread(d, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(d);
        return NULL;
    }
    fclose(f);
    if (size) *size = (size_t)n;
    return d;
}

bool an_mkdirs(const char *dir) {
    char buf[2048];
    int found = an_path_resolve(dir, buf, sizeof buf);
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

bool an_write_file(const char *path, const void *data, size_t size) {
    char real[2048];
    if (an_path_resolve(path, real, sizeof real) < 0) return false;
    char tmp[2100];
    snprintf(tmp, sizeof tmp, "%s.tmp", real);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    if (!ok || rename(tmp, real) != 0) {
        remove(tmp);
        return false;
    }
    an_casepath_changed(real);
    return true;
}

bool an_file_exists(const char *path) {
    char real[2048];
    return an_path_resolve(path, real, sizeof real) > 0;
}

void an_log(AN *a, const char *fmt, ...) {
    if (!a || !a->log) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(a->log, "[anim %u] ", a->now);
    vfprintf(a->log, fmt, ap);
    fputc('\n', a->log);
    fflush(a->log);
    va_end(ap);
}

static int utf8_put(char *o, u32 cp) {
    if (cp < 0x80) {
        o[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        o[0] = (char)(0xc0 | cp >> 6);
        o[1] = (char)(0x80 | (cp & 0x3f));
        return 2;
    }
    if (cp < 0x10000) {
        o[0] = (char)(0xe0 | cp >> 12);
        o[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
        o[2] = (char)(0x80 | (cp & 0x3f));
        return 3;
    }
    o[0] = (char)(0xf0 | cp >> 18);
    o[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
    o[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
    o[3] = (char)(0x80 | (cp & 0x3f));
    return 4;
}

extern const uint16_t an_sjis_table[11340];

static bool sjis_lead(u8 c) { return (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc); }

char *an_sjis_to_utf8(const char *s) {
    size_t n = strlen(s);
    char *out = an_alloc(n * 3 + 1), *o = out;
    for (const u8 *p = (const u8 *)s; *p;) {
        u8 c = p[0];
        u32 cp = 0xfffd;
        int len = 1;
        if (c < 0x80) cp = c;
        else if (c >= 0xa1 && c <= 0xdf) cp = 0xff61 + (c - 0xa1);
        else if (sjis_lead(c) && p[1] >= 0x40 && p[1] <= 0xfc) {
            int lead = c <= 0x9f ? c - 0x81 : c - 0xe0 + 31;
            u16 v = an_sjis_table[lead * 189 + (p[1] - 0x40)];
            cp = v ? v : 0xfffd;
            len = 2;
        }
        o += utf8_put(o, cp);
        p += len;
    }
    *o = 0;
    return out;
}

int an_utf8_decode(const char *s, u32 *cp) {
    const u8 *p = (const u8 *)s;
    if (p[0] < 0x80) {
        *cp = p[0];
        return 1;
    }
    if ((p[0] & 0xe0) == 0xc0 && p[1]) {
        *cp = (u32)(p[0] & 0x1f) << 6 | (p[1] & 0x3f);
        return 2;
    }
    if ((p[0] & 0xf0) == 0xe0 && p[1] && p[2]) {
        *cp = (u32)(p[0] & 0x0f) << 12 | (u32)(p[1] & 0x3f) << 6 | (p[2] & 0x3f);
        return 3;
    }
    if ((p[0] & 0xf8) == 0xf0 && p[1] && p[2] && p[3]) {
        *cp = (u32)(p[0] & 0x07) << 18 | (u32)(p[1] & 0x3f) << 12 | (u32)(p[2] & 0x3f) << 6 | (p[3] & 0x3f);
        return 4;
    }
    *cp = 0xfffd;
    return 1;
}

bool an_ieq(const char *a, const char *b) {
    for (; *a && *b; a++, b++)
        if (tolower((u8)*a) != tolower((u8)*b)) return false;
    return *a == *b;
}

u32 an_hash(const char *s) {
    u32 h = 2166136261u;
    for (; *s; s++) h = (h ^ (u32)tolower((u8)*s)) * 16777619u;
    return h;
}

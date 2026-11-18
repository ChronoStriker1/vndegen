#include "eagls.h"
#include "eg_casepath.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>

void *eg_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) abort();
    return p;
}

void *eg_realloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) abort();
    return q;
}

char *eg_strdup(const char *s) {
    size_t n = strlen(s);
    char *d = eg_alloc(n + 1);
    memcpy(d, s, n);
    return d;
}

char *eg_fmt(const char *fmt, ...) {
    va_list ap, aq;
    va_start(ap, fmt);
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *s = eg_alloc((size_t)n + 1);
    vsnprintf(s, (size_t)n + 1, fmt, aq);
    va_end(aq);
    return s;
}

int eg_path_resolve(const char *path, char *out, size_t size) { return eg_casepath_resolve(path, out, size); }

u8 *eg_read_file(const char *path, size_t *size) {
    char real[2048];
    if (eg_path_resolve(path, real, sizeof real) < 0) return NULL;
    FILE *f = fopen(real, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    u8 *d = eg_alloc((size_t)n + 1);
    if (fread(d, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(d); return NULL; }
    fclose(f);
    if (size) *size = (size_t)n;
    return d;
}

bool eg_mkdirs(const char *dir) {
    char buf[2048];
    snprintf(buf, sizeof buf, "%s", dir);
    for (char *p = buf + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        mkdir(buf, 0755);
        *p = '/';
    }
    return mkdir(buf, 0755) == 0 || errno == EEXIST;
}

bool eg_write_file(const char *path, const void *data, size_t size) {
    char real[2048];
    if (eg_path_resolve(path, real, sizeof real) < 0) return false;
    char tmp[2100];
    snprintf(tmp, sizeof tmp, "%s.tmp", real);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    if (!ok || rename(tmp, real) != 0) { remove(tmp); return false; }
    eg_casepath_changed(real);
    return true;
}

bool eg_ieq(const char *a, const char *b) {
    for (; *a && *b; a++, b++)
        if (tolower((u8)*a) != tolower((u8)*b)) return false;
    return !*a && !*b;
}

extern const uint16_t eg_sjis_table[11340];

static bool sjis_lead(u8 c) { return (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc); }

int eg_sjis_decode(const u8 *p, u32 *cp) {
    u8 c = p[0];
    if (c < 0x80) { *cp = c; return 1; }
    if (c >= 0xa1 && c <= 0xdf) { *cp = 0xff61 + (c - 0xa1); return 1; }
    if (sjis_lead(c) && p[1] >= 0x40 && p[1] <= 0xfc) {
        int lead = c <= 0x9f ? c - 0x81 : c - 0xe0 + 31;
        u16 v = eg_sjis_table[lead * 189 + (p[1] - 0x40)];
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

char *eg_sjis_to_utf8(const char *s) {
    size_t n = strlen(s);
    char *out = eg_alloc(n * 3 + 1), *o = out;
    for (const u8 *p = (const u8 *)s; *p;) {
        u32 cp;
        p += eg_sjis_decode(p, &cp);
        o += utf8_put(o, cp);
    }
    *o = 0;
    return out;
}

static u16 *sjis_reverse;

char *eg_utf8_to_sjis(const char *s) {
    if (!sjis_reverse) {
        u16 *r = eg_alloc(sizeof(u16) * 0x10000);
        for (int lead = 0; lead < 60; lead++)
            for (int t = 0; t < 189; t++) {
                u16 cp = eg_sjis_table[lead * 189 + t];
                if (!cp || r[cp]) continue;
                int b1 = lead < 31 ? 0x81 + lead : 0xe0 + lead - 31;
                r[cp] = (u16)(b1 << 8 | (0x40 + t));
            }
        sjis_reverse = r;
    }
    size_t n = strlen(s);
    char *out = eg_alloc(n * 2 + 1), *o = out;
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

int eg_sjis_len(const u8 *p) {
    if (sjis_lead(p[0]) && p[1]) return 2;
    return 1;
}

bool eg_file_exists(const char *path) {
    char real[2048];
    return eg_path_resolve(path, real, sizeof real) >= 0;
}

void eg_log(EG *e, const char *fmt, ...) {
    if (!e || !e->log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(e->log, fmt, ap);
    va_end(ap);
    fputc('\n', e->log);
    fflush(e->log);
}

bool ini_parse(Ini *ini, const u8 *data, size_t size) {
    memset(ini, 0, sizeof *ini);
    if (!data) return false;
    size_t i = 0;
    while (i < size) {
        size_t s = i;
        while (i < size && data[i] != '\n') i++;
        size_t end = i;
        i++;
        while (end > s && (data[end - 1] == '\r' || data[end - 1] == ' ')) end--;
        if (end == s || data[s] == '[' || data[s] == ';') continue;
        size_t eq = s;
        while (eq < end && data[eq] != '=') eq++;
        if (eq == end) continue;
        size_t kl = eq - s;
        char *k = eg_alloc(kl + 1);
        memcpy(k, data + s, kl);
        char *v = eg_alloc(end - eq);
        memcpy(v, data + eq + 1, end - eq - 1);
        ini->keys = eg_realloc(ini->keys, sizeof(char *) * (size_t)(ini->count + 1));
        ini->values = eg_realloc(ini->values, sizeof(char *) * (size_t)(ini->count + 1));
        ini->keys[ini->count] = k;
        ini->values[ini->count] = v;
        ini->count++;
    }
    return true;
}

void ini_free(Ini *ini) {
    for (int i = 0; i < ini->count; i++) {
        free(ini->keys[i]);
        free(ini->values[i]);
    }
    free(ini->keys);
    free(ini->values);
    memset(ini, 0, sizeof *ini);
}

const char *ini_get(const Ini *ini, const char *key) {
    for (int i = 0; i < ini->count; i++)
        if (eg_ieq(ini->keys[i], key)) return ini->values[i];
    return NULL;
}

int ini_ints(const Ini *ini, const char *key, int *out, int max) {
    const char *v = ini_get(ini, key);
    if (!v) return 0;
    int n = 0;
    while (*v && n < max) {
        char *end;
        long x = strtol(v, &end, 10);
        if (end == v) {
            v++;
            continue;
        }
        out[n++] = (int)x;
        v = end;
    }
    return n;
}

void eg_rand_seed(u32 *s, i32 seed) { *s = (u32)seed; }

int eg_rand(u32 *s) {
    *s = *s * 214013u + 2531011u;
    return (int)((*s >> 16) & 0x7fff);
}

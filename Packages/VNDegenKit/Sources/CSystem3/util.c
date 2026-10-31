// Files, case-insensitive lookup, Shift-JIS, and logging.
#include "s3.h"
#include "s3_casepath.h"

extern const uint16_t s3_sjis_table[];

void *s3_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) abort();
    return p;
}

char *s3_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = s3_alloc(n);
    memcpy(d, s, n);
    return d;
}

u8 *s3_read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    u8 *d = malloc((size_t)n + 1);
    if (!d) { fclose(f); return NULL; }
    size_t got = fread(d, 1, (size_t)n, f);
    fclose(f);
    d[got] = 0;
    if (size) *size = got;
    return d;
}

char *s3_path(const char *dir, const char *name) {
    size_t a = strlen(dir), b = strlen(name);
    char *p = s3_alloc(a + b + 2);
    memcpy(p, dir, a);
    size_t o = a;
    if (a && dir[a - 1] != '/') p[o++] = '/';
    memcpy(p + o, name, b + 1);
    return p;
}

// Games ask for names in any case, and may use backslashes. The shared lookup matches each path component without
// regard to ASCII case, as Windows does.
char *s3_find_file(const char *dir, const char *name) {
    char *joined = s3_path(dir, name);
    for (char *c = joined; *c; c++) if (*c == '\\') *c = '/';
    char out[4096];
    int found = s3_casepath_resolve(joined, out, sizeof out);
    free(joined);
    return found == 1 ? s3_strdup(out) : NULL;
}

bool s3_sjis_lead(u8 c) { return (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc); }

int s3_sjis_decode(const u8 *p, u32 *cp) {
    u8 c = p[0];
    if (c < 0x80) { *cp = c; return 1; }
    if (c >= 0xa1 && c <= 0xdf) { *cp = 0xff61 + (c - 0xa1); return 1; } // half-width katakana
    if (s3_sjis_lead(c) && p[1] >= 0x40 && p[1] <= 0xfc) {
        int lead = c <= 0x9f ? c - 0x81 : c - 0xe0 + 31;
        u16 v = s3_sjis_table[lead * 189 + (p[1] - 0x40)];
        *cp = v ? v : 0xfffd;
        return 2;
    }
    *cp = 0xfffd;
    return 1;
}

char *s3_sjis_to_utf8(const char *s) {
    size_t n = strlen(s);
    char *out = s3_alloc(n * 3 + 1), *o = out;
    const u8 *p = (const u8 *)s;
    while (*p) {
        u32 cp;
        p += s3_sjis_decode(p, &cp);
        if (cp < 0x80) *o++ = (char)cp;
        else if (cp < 0x800) { *o++ = (char)(0xc0 | cp >> 6); *o++ = (char)(0x80 | (cp & 0x3f)); }
        else { *o++ = (char)(0xe0 | cp >> 12); *o++ = (char)(0x80 | ((cp >> 6) & 0x3f)); *o++ = (char)(0x80 | (cp & 0x3f)); }
    }
    *o = 0;
    return out;
}

bool s3_write_file(const char *path, const void *data, size_t size) {
    size_t n = strlen(path) + 8;
    char *tmp = s3_alloc(n);
    snprintf(tmp, n, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    bool ok = f && fwrite(data, 1, size, f) == size;
    if (f && fclose(f) != 0) ok = false;
    if (ok) ok = rename(tmp, path) == 0;
    if (!ok) remove(tmp);
    free(tmp);
    return ok;
}

u16 s3_le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
u32 s3_le32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

void s3_log(S3 *s, const char *fmt, ...) {
    if (!s->log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(s->log, fmt, ap);
    va_end(ap);
    fputc('\n', s->log);
    fflush(s->log);
}

void s3_fail(S3 *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->problem, sizeof s->problem, fmt, ap);
    va_end(ap);
    s3_log(s, "problem: %s", s->problem);
    s->quit = true;
}

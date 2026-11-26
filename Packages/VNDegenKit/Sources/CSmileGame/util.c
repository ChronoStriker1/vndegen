#include "sg.h"

#include <stdarg.h>

void *sg_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) abort();
    return p;
}

void *sg_realloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) abort();
    return q;
}

char *sg_strdup(const char *s) {
    size_t n = strlen(s);
    char *d = sg_alloc(n + 1);
    memcpy(d, s, n);
    return d;
}

char *sg_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *s = sg_alloc((size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

u8 *sg_read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *d = sg_alloc((size_t)n + 1);
    if (n > 0 && fread(d, 1, (size_t)n, f) != (size_t)n) {
        free(d);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return d;
}

void sg_log(SG *g, const char *fmt, ...) {
    if (!g || !g->log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g->log, fmt, ap);
    va_end(ap);
    fputc('\n', g->log);
    fflush(g->log);
}

bool guid_zero(Guid g) {
    for (int i = 0; i < 16; i++)
        if (g.b[i]) return false;
    return true;
}

bool guid_eq(Guid a, Guid b) { return memcmp(a.b, b.b, 16) == 0; }

void guid_str(Guid g, char out[37]) {
    const u8 *b = g.b;
    snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", b[3], b[2], b[1], b[0], b[5],
             b[4], b[7], b[6], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

static int hexv(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool guid_parse(const char *s, Guid *out) {
    static const int order[16] = {3, 2, 1, 0, 5, 4, 7, 6, 8, 9, 10, 11, 12, 13, 14, 15};
    u8 raw[16];
    int n = 0;
    for (; *s && n < 32; s++) {
        if (*s == '-') continue;
        int v = hexv((unsigned char)*s);
        if (v < 0) return false;
        if (n & 1)
            raw[n / 2] |= (u8)v;
        else
            raw[n / 2] = (u8)(v << 4);
        n++;
    }
    if (n != 32) return false;
    for (int i = 0; i < 16; i++) out->b[order[i]] = raw[i];
    return true;
}

int utf8_decode(const char *s, u32 *cp) {
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

Rd rd_make(const u8 *p, size_t n) { return (Rd){p, p + n, false}; }

bool rd_need(Rd *r, size_t n) {
    if (r->bad || (size_t)(r->end - r->p) < n) {
        r->bad = true;
        return false;
    }
    return true;
}

u8 rd_u8(Rd *r) { return rd_need(r, 1) ? *r->p++ : 0; }

i16 rd_i16(Rd *r) {
    if (!rd_need(r, 2)) return 0;
    i16 v = (i16)(r->p[0] | r->p[1] << 8);
    r->p += 2;
    return v;
}

i32 rd_i32(Rd *r) {
    if (!rd_need(r, 4)) return 0;
    i32 v = (i32)((u32)r->p[0] | (u32)r->p[1] << 8 | (u32)r->p[2] << 16 | (u32)r->p[3] << 24);
    r->p += 4;
    return v;
}

i64 rd_i64(Rd *r) {
    u64 lo = (u32)rd_i32(r), hi = (u32)rd_i32(r);
    return (i64)(lo | hi << 32);
}

float rd_f32(Rd *r) {
    i32 v = rd_i32(r);
    float f;
    memcpy(&f, &v, 4);
    return f;
}

Guid rd_guid(Rd *r) {
    Guid g = {{0}};
    if (rd_need(r, 16)) {
        memcpy(g.b, r->p, 16);
        r->p += 16;
    }
    return g;
}

char *rd_str(Rd *r) {
    u32 n = 0;
    for (int shift = 0; shift < 35; shift += 7) {
        u8 c = rd_u8(r);
        n |= (u32)(c & 0x7f) << shift;
        if (!(c & 0x80)) break;
    }
    if (!rd_need(r, n)) return sg_strdup("");
    char *s = sg_alloc(n + 1);
    memcpy(s, r->p, n);
    r->p += n;
    return s;
}

void rd_skip(Rd *r, size_t n) {
    if (rd_need(r, n)) r->p += n;
}

u32 color_parse(const char *hex) {
    if (!hex || hex[0] != '#') return 0xffffffffu;
    u32 v = (u32)strtoul(hex + 1, NULL, 16);
    u32 rr = v >> 16 & 255, gg = v >> 8 & 255, bb = v & 255;
    return 0xff000000u | bb << 16 | gg << 8 | rr;
}

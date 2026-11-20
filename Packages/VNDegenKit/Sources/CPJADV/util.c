#include "pj.h"
#include "pj_casepath.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>

void *pj_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) abort();
    return p;
}

void *pj_realloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) abort();
    return q;
}

char *pj_strdup(const char *s) {
    size_t n = strlen(s);
    char *d = pj_alloc(n + 1);
    memcpy(d, s, n);
    return d;
}

char *pj_fmt(const char *fmt, ...) {
    va_list ap, aq;
    va_start(ap, fmt);
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *s = pj_alloc((size_t)n + 1);
    vsnprintf(s, (size_t)n + 1, fmt, aq);
    va_end(aq);
    return s;
}

int pj_path_resolve(const char *path, char *out, size_t size) { return pj_casepath_resolve(path, out, size); }

u8 *pj_read_file(const char *path, size_t *size) {
    char real[2048];
    if (pj_path_resolve(path, real, sizeof real) < 0) return NULL;
    FILE *f = fopen(real, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    u8 *d = pj_alloc((size_t)n + 1);
    if (fread(d, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(d);
        return NULL;
    }
    fclose(f);
    if (size) *size = (size_t)n;
    return d;
}

bool pj_write_file(const char *path, const void *data, size_t size) {
    char real[2048];
    if (pj_path_resolve(path, real, sizeof real) < 0) return false;
    char *tmp = pj_fmt("%s.tmp", real);
    FILE *f = fopen(tmp, "wb");
    bool ok = f && fwrite(data, 1, size, f) == size;
    if (f && fclose(f)) ok = false;
    if (ok && rename(tmp, real)) ok = false;
    if (!ok) remove(tmp);
    else pj_casepath_changed(real);
    free(tmp);
    return ok;
}

bool pj_mkdirs(const char *dir) {
    char real[2048];
    if (pj_path_resolve(dir, real, sizeof real) < 0) return false;
    for (char *p = real + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        if (mkdir(real, 0755) && errno != EEXIST) return false;
        *p = '/';
    }
    return !mkdir(real, 0755) || errno == EEXIST;
}

int pj_utf8_decode(const char *s, u32 *cp) {
    const u8 *p = (const u8 *)s;
    if (p[0] < 0x80) {
        *cp = p[0];
        return 1;
    }
    if ((p[0] & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
        *cp = (u32)(p[0] & 0x1f) << 6 | (p[1] & 0x3f);
        return 2;
    }
    if ((p[0] & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80) {
        *cp = (u32)(p[0] & 0x0f) << 12 | (u32)(p[1] & 0x3f) << 6 | (p[2] & 0x3f);
        return 3;
    }
    if ((p[0] & 0xf8) == 0xf0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80 && (p[3] & 0xc0) == 0x80) {
        *cp = (u32)(p[0] & 0x07) << 18 | (u32)(p[1] & 0x3f) << 12 | (u32)(p[2] & 0x3f) << 6 | (p[3] & 0x3f);
        return 4;
    }
    *cp = 0xfffd;
    return 1;
}

bool pj_ieq(const char *a, const char *b) {
    for (; *a && *b; a++, b++)
        if (tolower((u8)*a) != tolower((u8)*b)) return false;
    return *a == *b;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) *--e = 0;
    return s;
}

void pj_ini_parse(Ini *c, const u8 *data, size_t size) {
    char *buf = pj_alloc(size + 1);
    memcpy(buf, data, size);
    char sec[64] = "";
    for (char *line = buf, *next; line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = 0;
        char *cm = strstr(line, "//");
        if (cm) *cm = 0;
        char *l = trim(line);
        if (!*l) continue;
        if (*l == '[') {
            char *e = strchr(l, ']');
            if (e) *e = 0;
            snprintf(sec, sizeof sec, "%s", l + 1);
            continue;
        }
        char *eq = strchr(l, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(l), *v = trim(eq + 1);
        size_t vn = strlen(v);
        if (vn >= 2 && v[0] == '"' && v[vn - 1] == '"') {
            v[vn - 1] = 0;
            v++;
        }
        if (c->count == c->cap) {
            c->cap = c->cap ? c->cap * 2 : 32;
            c->sec = pj_realloc(c->sec, sizeof(char *) * (size_t)c->cap);
            c->key = pj_realloc(c->key, sizeof(char *) * (size_t)c->cap);
            c->val = pj_realloc(c->val, sizeof(char *) * (size_t)c->cap);
        }
        c->sec[c->count] = pj_strdup(sec);
        c->key[c->count] = pj_strdup(k);
        c->val[c->count] = pj_strdup(v);
        c->count++;
    }
    free(buf);
}

void pj_ini_free(Ini *c) {
    for (int i = 0; i < c->count; i++) {
        free(c->sec[i]);
        free(c->key[i]);
        free(c->val[i]);
    }
    free(c->sec);
    free(c->key);
    free(c->val);
    memset(c, 0, sizeof *c);
}

const char *pj_ini_get(const Ini *c, const char *sec, const char *key) {
    for (int i = 0; i < c->count; i++)
        if (pj_ieq(c->sec[i], sec) && pj_ieq(c->key[i], key)) return c->val[i];
    return NULL;
}

int pj_ini_ints(const char *v, int *out, int max) {
    int n = 0;
    while (v && *v && n < max) {
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

Image *pj_image_new(int w, int h) {
    Image *im = pj_alloc(sizeof *im);
    im->w = w;
    im->h = h;
    im->px = pj_alloc((size_t)w * (size_t)h * 4);
    return im;
}

void pj_image_free(Image *im) {
    if (!im) return;
    free(im->px);
    free(im);
}

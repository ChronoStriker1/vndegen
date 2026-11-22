#include "sa.h"
#include "sa_casepath.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>

void *sa_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) abort();
    return p;
}

void *sa_realloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) abort();
    return q;
}

char *sa_strdup(const char *s) {
    size_t n = strlen(s);
    char *d = sa_alloc(n + 1);
    memcpy(d, s, n);
    return d;
}

char *sa_fmt(const char *fmt, ...) {
    va_list ap, aq;
    va_start(ap, fmt);
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *s = sa_alloc((size_t)n + 1);
    vsnprintf(s, (size_t)n + 1, fmt, aq);
    va_end(aq);
    return s;
}

int sa_path_resolve(const char *path, char *out, size_t size) { return sa_casepath_resolve(path, out, size); }

u8 *sa_read_file(const char *path, size_t *size) {
    char real[2048];
    if (sa_path_resolve(path, real, sizeof real) < 0) return NULL;
    FILE *f = fopen(real, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    u8 *d = sa_alloc((size_t)n + 1);
    if (fread(d, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(d);
        return NULL;
    }
    fclose(f);
    if (size) *size = (size_t)n;
    return d;
}

bool sa_write_file(const char *path, const void *data, size_t size) {
    char real[2048];
    if (sa_path_resolve(path, real, sizeof real) < 0) return false;
    char *tmp = sa_fmt("%s.tmp", real);
    FILE *f = fopen(tmp, "wb");
    bool ok = f && fwrite(data, 1, size, f) == size;
    if (f && fclose(f)) ok = false;
    if (ok && rename(tmp, real)) ok = false;
    if (!ok) remove(tmp);
    else sa_casepath_changed(real);
    free(tmp);
    return ok;
}

bool sa_mkdirs(const char *dir) {
    char real[2048];
    if (sa_path_resolve(dir, real, sizeof real) < 0) return false;
    for (char *p = real + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        if (mkdir(real, 0755) && errno != EEXIST) return false;
        *p = '/';
    }
    return !mkdir(real, 0755) || errno == EEXIST;
}


bool sa_ieq(const char *a, const char *b) {
    for (; *a && *b; a++, b++)
        if (tolower((u8)*a) != tolower((u8)*b)) return false;
    return *a == *b;
}

int sa_utf8_decode(const char *s, u32 *cp) {
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

int sa_utf8_put(char *out, u32 cp) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xc0 | cp >> 6);
        out[1] = (char)(0x80 | (cp & 0x3f));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xe0 | cp >> 12);
        out[1] = (char)(0x80 | (cp >> 6 & 0x3f));
        out[2] = (char)(0x80 | (cp & 0x3f));
        return 3;
    }
    out[0] = (char)(0xf0 | cp >> 18);
    out[1] = (char)(0x80 | (cp >> 12 & 0x3f));
    out[2] = (char)(0x80 | (cp >> 6 & 0x3f));
    out[3] = (char)(0x80 | (cp & 0x3f));
    return 4;
}

char *sa_utf16_to_utf8(const u8 *p, int chars) {
    char *s = sa_alloc((size_t)chars * 4 + 1);
    int n = 0;
    for (int i = 0; i < chars; i++) {
        u32 c = rd16(p + i * 2);
        if (!c) break;
        if (c >= 0xd800 && c < 0xdc00 && i + 1 < chars) {
            u32 lo = rd16(p + i * 2 + 2);
            if (lo >= 0xdc00 && lo < 0xe000) {
                c = 0x10000 + ((c - 0xd800) << 10) + (lo - 0xdc00);
                i++;
            }
        }
        n += sa_utf8_put(s + n, c);
    }
    s[n] = 0;
    return s;
}

Image *sa_image_new(int w, int h) {
    Image *im = sa_alloc(sizeof *im);
    im->w = w;
    im->h = h;
    im->px = sa_alloc((size_t)w * (size_t)h * 4);
    return im;
}

void sa_image_free(Image *im) {
    if (!im) return;
    free(im->px);
    free(im);
}

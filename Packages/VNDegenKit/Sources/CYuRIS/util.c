// Allocation, files, hashes and text encodings.
#include "yr.h"
#include "vm.h"
#include "yr_casepath.h"
#include <sys/stat.h>
#include <unistd.h>

void *yr_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) abort();
    return p;
}

void *yr_realloc(void *p, size_t n) {
    p = realloc(p, n ? n : 1);
    if (!p) abort();
    return p;
}

char *yr_strdup(const char *s) { return yr_strndup(s, strlen(s)); }

char *yr_strndup(const char *s, size_t n) {
    char *d = yr_alloc(n + 1);
    memcpy(d, s, n);
    return d;
}

char *yr_path(const char *dir, const char *name) {
    size_t n = strlen(dir) + strlen(name) + 2;
    char *p = yr_alloc(n);
    snprintf(p, n, "%s/%s", dir, name);
    return p;
}

u8 *yr_read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    u8 *d = yr_alloc((size_t)n + 1);
    size_t got = fread(d, 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) { free(d); return NULL; }
    if (size) *size = (size_t)n;
    return d;
}

bool yr_write_file(const char *path, const void *data, size_t size) {
    // Written to a fresh temporary beside the file and renamed over it, so a failed write leaves the old file.
    struct stat old;
    bool had = stat(path, &old) == 0;
    size_t n = strlen(path) + 16;
    char *tmp = yr_alloc(n);
    snprintf(tmp, n, "%s.XXXXXX", path);
    int fd = mkstemp(tmp);
    bool ok = fd >= 0;
    if (ok && had && fchmod(fd, old.st_mode & 0777) != 0) { close(fd); remove(tmp); ok = false; fd = -1; }
    if (ok) {
        FILE *f = fdopen(fd, "wb");
        if (!f) { close(fd); ok = false; }
        else {
            ok = fwrite(data, 1, size, f) == size;
            if (fclose(f) != 0) ok = false;
        }
        if (ok) ok = rename(tmp, path) == 0;
        if (!ok) remove(tmp);
    }
    free(tmp);
    return ok;
}

u16 yr_le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
u32 yr_le32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
u64 yr_le64(const u8 *p) { return (u64)yr_le32(p) | (u64)yr_le32(p + 4) << 32; }

// MurmurHash2 with seed 0, the hash of archive entry names.
u32 yr_murmur2(const u8 *data, size_t len) {
    const u32 m = 0x5bd1e995;
    u32 h = (u32)len;
    while (len >= 4) {
        u32 k = yr_le32(data);
        k *= m;
        k ^= k >> 24;
        k *= m;
        h *= m;
        h ^= k;
        data += 4;
        len -= 4;
    }
    switch (len) {
    case 3: h ^= (u32)data[2] << 16; /* fallthrough */
    case 2: h ^= (u32)data[1] << 8; /* fallthrough */
    case 1: h ^= data[0]; h *= m;
    }
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

u32 yr_crc32(const u8 *data, size_t len) {
    static u32 table[256];
    if (!table[1]) {
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    }
    u32 c = 0xffffffffu;
    for (size_t i = 0; i < len; i++) c = table[(c ^ data[i]) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

void yr_key(char *out, size_t size, const char *s) {
    size_t i = 0;
    for (; s[i] && i + 1 < size; i++) {
        char c = s[i];
        if (c == '\\') c = '/';
        else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out[i] = c;
    }
    out[i] = 0;
}

char *yr_find_file(const char *dir, const char *name) {
    char *joined = yr_path(dir, name);
    for (char *c = joined; *c; c++) if (*c == '\\') *c = '/';
    char out[4096];
    int found = yr_casepath_resolve(joined, out, sizeof out);
    free(joined);
    return found == 1 ? yr_strdup(out) : NULL;
}

void yr_log(YR *y, const char *fmt, ...) {
    if (!y->log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(y->log, fmt, ap);
    va_end(ap);
    fflush(y->log);
}

extern const uint16_t yr_sjis_table[11340];

static bool sjis_lead(u8 c) { return (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc); }

int yr_sjis_decode(const u8 *p, u32 *cp) {
    u8 c = p[0];
    if (c < 0x80) { *cp = c; return 1; }
    if (c >= 0xa1 && c <= 0xdf) { *cp = 0xff61 + (c - 0xa1); return 1; }
    if (sjis_lead(c) && p[1] >= 0x40 && p[1] <= 0xfc) {
        int lead = c <= 0x9f ? c - 0x81 : c - 0xe0 + 31;
        u16 v = yr_sjis_table[lead * 189 + (p[1] - 0x40)];
        *cp = v ? v : 0xfffd;
        return 2;
    }
    *cp = 0xfffd;
    return 1;
}

static int utf8_decode(const u8 *p, u32 *cp) {
    u8 c = p[0];
    int n = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 1;
    for (int i = 1; i < n; i++) if ((p[i] & 0xc0) != 0x80) { *cp = 0xfffd; return 1; }
    switch (n) {
    case 1: *cp = c < 0x80 ? c : 0xfffd; break;
    case 2: *cp = (u32)(c & 0x1f) << 6 | (p[1] & 0x3f); break;
    case 3: *cp = (u32)(c & 0x0f) << 12 | (u32)(p[1] & 0x3f) << 6 | (p[2] & 0x3f); break;
    default: *cp = (u32)(c & 0x07) << 18 | (u32)(p[1] & 0x3f) << 12 | (u32)(p[2] & 0x3f) << 6 | (p[3] & 0x3f);
    }
    return n;
}

int yr_char_decode(int enc, const u8 *p, u32 *cp) {
    if (!*p) { *cp = 0; return 1; }
    return enc == YR_UTF8 ? utf8_decode(p, cp) : yr_sjis_decode(p, cp);
}

int yr_char_len(int enc, const u8 *p) {
    u32 cp;
    return yr_char_decode(enc, p, &cp);
}

char *yr_to_utf8(int enc, const char *s) {
    if (enc == YR_UTF8) return yr_strdup(s);
    size_t n = strlen(s);
    char *out = yr_alloc(n * 3 + 1), *o = out;
    const u8 *p = (const u8 *)s;
    while (*p) {
        u32 cp;
        p += yr_sjis_decode(p, &cp);
        if (cp < 0x80) *o++ = (char)cp;
        else if (cp < 0x800) { *o++ = (char)(0xc0 | cp >> 6); *o++ = (char)(0x80 | (cp & 0x3f)); }
        else { *o++ = (char)(0xe0 | cp >> 12); *o++ = (char)(0x80 | ((cp >> 6) & 0x3f)); *o++ = (char)(0x80 | (cp & 0x3f)); }
    }
    *o = 0;
    return out;
}

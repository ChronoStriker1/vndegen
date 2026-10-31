// System39.ain: the compiler's tables of DLLs, functions, variable names and messages.
//
// Read from the files: "AINI", then every byte rotated right by two bits. Decoded, it is a u32
// version and tagged sections, each a 4-byte tag, a u32 zero and a u32 count:
//   HEL0  DLLs: name, u32 function count, each function name, u32 argument count, u32 types
//   FUNC  functions: name, u16 page, u32 address
//   VARI  variable names
//   MSGI  messages, NUL-terminated CP932, numbered from 0 for the /| command
#include "s3.h"

static bool take_str(const u8 *d, size_t n, size_t *p, const char **out) {
    if (*p >= n) return false;
    const u8 *e = memchr(d + *p, 0, n - *p);
    if (!e) return false;
    if (out) *out = (const char *)d + *p;
    *p = (size_t)(e - d) + 1;
    return true;
}

bool s3_ain_open(S3Ain *ain, const char *path) {
    memset(ain, 0, sizeof *ain);
    size_t n;
    u8 *d = s3_read_file(path, &n);
    if (!d) return false;
    if (n < 8 || memcmp(d, "AINI", 4) != 0) { free(d); return false; }
    for (size_t i = 4; i < n; i++) d[i] = (u8)((d[i] << 2) | (d[i] >> 6));
    ain->data = d;
    size_t p = 8;
    while (p + 12 <= n) {
        const u8 *tag = d + p;
        u32 count = s3_le32(d + p + 8);
        p += 12;
        if (count > n - p) return true; // every entry takes at least one byte
        if (!memcmp(tag, "HEL0", 4)) {
            for (u32 i = 0; i < count; i++) {
                if (!take_str(d, n, &p, NULL) || p + 4 > n) return true;
                u32 fns = s3_le32(d + p); p += 4;
                for (u32 j = 0; j < fns; j++) {
                    if (!take_str(d, n, &p, NULL) || p + 4 > n) return true;
                    u32 args = s3_le32(d + p);
                    if ((size_t)args > (n - p - 4) / 4) return true;
                    p += 4 + (size_t)args * 4;
                }
            }
        } else if (!memcmp(tag, "FUNC", 4)) {
            ain->funcCount = count;
            for (u32 i = 0; i < count; i++) {
                if (!take_str(d, n, &p, NULL)) return true;
                p += 6;
            }
        } else if (!memcmp(tag, "VARI", 4)) {
            ain->varCount = count;
            ain->varNames = s3_alloc(sizeof(char *) * (count + 1));
            for (u32 i = 0; i < count; i++)
                if (!take_str(d, n, &p, (const char **)&ain->varNames[i])) return true;
        } else if (!memcmp(tag, "MSGI", 4)) {
            ain->msgCount = count;
            ain->msgs = s3_alloc(sizeof(char *) * (count + 1));
            for (u32 i = 0; i < count; i++)
                if (!take_str(d, n, &p, &ain->msgs[i])) { ain->msgCount = i; return true; }
        } else {
            break;
        }
    }
    return true;
}

void s3_ain_close(S3Ain *ain) {
    free(ain->data);
    free(ain->varNames);
    free(ain->msgs);
    memset(ain, 0, sizeof *ain);
}

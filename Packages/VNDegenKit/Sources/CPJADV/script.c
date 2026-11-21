#include "pj.h"

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool pj_script_load(Script *s, Pac *a, char *err, size_t errSize) {
    memset(s, 0, sizeof *s);
    size_t n;
    s->code = pj_pac_read(a, "scenario.dat", &s->codeSize);
    if (!s->code || s->codeSize < 0x20 || memcmp(s->code, "PJADV_SF0001", 12)) {
        snprintf(err, errSize, "scenario.dat is missing or not a PJADV scenario");
        return false;
    }
    u8 *fl = pj_pac_read(a, "filename.dat", &n);
    if (fl && n >= 16 && !memcmp(fl, "PJADV_FL0001", 12)) {
        u32 count = rd32(fl + 12);
        if (16 + (size_t)count * 32 <= n) {
            s->nameCount = (int)count;
            s->names = pj_alloc(sizeof(char *) * (count ? count : 1));
            for (u32 i = 0; i < count; i++) {
                char name[33];
                memcpy(name, fl + 16 + (size_t)i * 32, 32);
                name[32] = 0;
                s->names[i] = pj_strdup(name);
            }
        }
    }
    free(fl);
    s->text = pj_pac_read(a, "textdata.bin", &s->textSize);
    if (!s->text || s->textSize < 16 || memcmp(s->text, "PJADV_TF0001", 12)) {
        snprintf(err, errSize, "textdata.bin is missing or not PJADV text");
        return false;
    }
    return true;
}

void pj_script_free(Script *s) {
    for (int i = 0; i < s->nameCount; i++) free(s->names[i]);
    free(s->names);
    free(s->code);
    free(s->text);
    memset(s, 0, sizeof *s);
}

const char *pj_script_name(const Script *s, int index) {
    return index >= 1 && index <= s->nameCount ? s->names[index - 1] : NULL;
}

char *pj_script_text(const Script *s, u32 offset) {
    if (offset < 16 || offset >= s->textSize) return pj_strdup("");
    const char *p = (const char *)s->text + offset;
    size_t max = s->textSize - offset, len = strnlen(p, max);
    char *out = pj_alloc(len + 1);
    size_t o = 0;
    for (size_t i = 0; i < len;) {
        if (p[i] == '\\' && i + 3 < len && p[i + 1] == 'U' && p[i + 2] >= '1' && p[i + 2] <= '4') {
            int bytes = p[i + 2] - '0';
            size_t end = i + 3 + (size_t)bytes * 2;
            bool ok = end < len && p[end] == 'h';
            for (int k = 0; ok && k < bytes * 2; k++)
                if (hexval(p[i + 3 + k]) < 0) ok = false;
            if (ok) {
                for (int k = 0; k < bytes; k++) out[o++] = (char)(hexval(p[i + 3 + 2 * k]) << 4 | hexval(p[i + 4 + 2 * k]));
                i = end + 1;
                continue;
            }
        }
        out[o++] = p[i++];
    }
    out[o] = 0;
    return out;
}

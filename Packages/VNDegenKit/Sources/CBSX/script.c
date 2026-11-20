#include "bsx.h"

static char *wstr_at(const u8 *d, size_t n, size_t off) {
    if (off >= n) return bx_strdup("");
    size_t e = off;
    while (e + 1 < n && (d[e] || d[e + 1])) e += 2;
    return bx_utf16_to_utf8(d + off, (e - off) / 2);
}

static char **table(const Script *s, const u32 sec[14][2], int ti, int si, int *count) {
    u32 to = sec[ti][0], tn = sec[ti][1];
    u32 so = sec[si][0];
    *count = 0;
    if (!tn || (u64)to + tn > s->size) return NULL;
    int n = (int)(tn / 4);
    char **out = bx_alloc(sizeof(char *) * (size_t)n);
    for (int i = 0; i < n; i++) out[i] = wstr_at(s->data, s->size, (size_t)so + 2 * (size_t)rd32(s->data + to + 4 * (size_t)i));
    *count = n;
    return out;
}

static char *unescape(char *t) {
    char *o = t;
    for (char *p = t; *p; p++) {
        if (p[0] == '\\' && p[1] == 'n') {
            *o++ = '\n';
            p++;
        } else *o++ = *p;
    }
    *o = 0;
    return t;
}

Script *bx_script_load(const char *path, char *err, size_t errSize) {
    size_t n;
    u8 *d = bx_read_file(path, &n);
    if (!d) {
        if (err) snprintf(err, errSize, "can't read bsxx.dat");
        return NULL;
    }
    if (n < 0xa8 || memcmp(d, "BSXScript 3.", 12)) {
        if (err) snprintf(err, errSize, "bsxx.dat isn't BSXScript 3");
        free(d);
        return NULL;
    }
    Script *s = bx_alloc(sizeof *s);
    s->data = d;
    s->size = n;
    memcpy(s->version, d, 15);
    s->minor = d[12] - '0';
    for (int i = 0; i < 6; i++) s->hdr[i] = rd32(d + 16 + 4 * i);
    u32 nsys = rd32(d + 40), codeOff = rd32(d + 44), codeSize = rd32(d + 48), nmsg = rd32(d + 52);
    u32 sec[14][2];
    for (int i = 0; i < 14; i++) {
        sec[i][0] = rd32(d + 56 + 8 * i);
        sec[i][1] = rd32(d + 60 + 8 * i);
    }
    if ((u64)codeOff + codeSize > n || (u64)168 + 8 * (u64)nsys > n) {
        if (err) snprintf(err, errSize, "bsxx.dat is damaged");
        bx_script_free(s);
        return NULL;
    }
    s->code = d + codeOff;
    s->codeSize = codeSize;
    s->sysvarCount = (int)nsys;
    s->sysvars = bx_alloc(sizeof(u32[2]) * (nsys ? nsys : 1));
    for (u32 i = 0; i < nsys; i++) {
        s->sysvars[i][0] = rd32(d + 168 + 8 * i);
        s->sysvars[i][1] = rd32(d + 172 + 8 * i);
    }
    s->labelCount = (int)(sec[0][1] / 8);
    s->labels = bx_alloc(sizeof(u32) * (size_t)(s->labelCount ? s->labelCount : 1));
    s->labelNames = bx_alloc(sizeof(char *) * (size_t)(s->labelCount ? s->labelCount : 1));
    for (int i = 0; i < s->labelCount; i++) {
        s->labels[i] = rd32(d + sec[0][0] + 8 * (size_t)i);
        s->labelNames[i] = wstr_at(d, n, (size_t)sec[1][0] + 2 * (size_t)rd32(d + sec[0][0] + 8 * (size_t)i + 4));
    }
    s->strs = table(s, (const u32(*)[2])sec, 2, 3, &s->strCount);
    s->vars = table(s, (const u32(*)[2])sec, 6, 7, &s->varCount);
    s->chars = table(s, (const u32(*)[2])sec, 10, 11, &s->charCount);
    u32 mo = sec[12][0], so = sec[13][0];
    if ((u64)mo + 4 * (u64)nmsg > n) nmsg = 0;
    s->msgCount = (int)nmsg;
    s->msgs = bx_alloc(sizeof(char *) * (nmsg ? nmsg : 1));
    for (u32 i = 0; i < nmsg; i++) s->msgs[i] = unescape(wstr_at(d, n, (size_t)so + 2 * (size_t)rd32(d + mo + 4 * i)));
    for (int i = 0; i < s->charCount; i++) unescape(s->chars[i]);
    return s;
}

static void free_list(char **l, int n) {
    for (int i = 0; i < n; i++) free(l[i]);
    free(l);
}

void bx_script_free(Script *s) {
    if (!s) return;
    free_list(s->labelNames, s->labelCount);
    free(s->labels);
    free_list(s->strs, s->strCount);
    free_list(s->vars, s->varCount);
    free_list(s->chars, s->charCount);
    free_list(s->msgs, s->msgCount);
    free(s->sysvars);
    free(s->data);
    free(s);
}

int bx_script_label(const Script *s, const char *name) {
    for (int i = 0; i < s->labelCount; i++)
        if (!strcmp(s->labelNames[i], name)) return i;
    for (int i = 0; i < s->labelCount; i++)
        if (bx_ieq(s->labelNames[i], name)) return i;
    return -1;
}

int bx_script_var(const Script *s, const char *name) {
    for (int i = 0; i < s->varCount; i++)
        if (!strcmp(s->vars[i], name)) return i;
    return -1;
}

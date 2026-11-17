#include "n2.h"

#include <stdlib.h>
#include <string.h>

typedef struct Scan {
    u8 *t;
    int slot;
    ScanRecord *recs;
    int count, cap;
} Scan;

typedef struct Marks {
    int *starts, *ends;
    int count;
} Marks;

static const struct {
    const char *word;
    int len, kind;
} keywords[] = {{"chapter", 7, SCAN_CHAPTER}, {"scene", 5, SCAN_SCENE}, {"cut", 3, SCAN_CUT}, {"function", 8, SCAN_FUNCTION}};

static bool ws(const u8 *t, int p) {
    u8 c = t[p];
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || (c == 0x81 && t[p + 1] == 0x40);
}

static int keyword(const u8 *t, int q, int *len) {
    for (int i = 0; i < 4; i++) {
        if (memcmp(t + q, keywords[i].word, (size_t)keywords[i].len) == 0 && ws(t, q + keywords[i].len)) {
            *len = keywords[i].len;
            return keywords[i].kind;
        }
    }
    return 0;
}

static bool dbcs(u8 c) { return (c < 0xa1 || c > 0xdf) && c > 0x80; }

static bool skipc(const u8 *t, int *pp) {
    int p = *pp;
    u8 c = t[p];
    if (c == 0) return false;
    if (t[p] == '/' && t[p + 1] == '/') {
        for (;;) {
            c = t[p];
            if (c == '\r' || c == '\n') break;
            p++;
            if (t[p] == 0) break;
        }
        p++;
    } else if (t[p] == '/' && t[p + 1] == '*') {
        p += 2;
        int depth = 1;
        while (t[p] != 0) {
            if (t[p] == '/' && t[p + 1] == '*') {
                depth++;
                p += 2;
            } else if (t[p] == '*' && t[p + 1] == '/') {
                if (--depth == 0) break;
                p += 2;
            } else {
                p++;
            }
        }
        p += 2;
        if (t[p] == '\r') p++;
    } else {
        if (c != '\r' && c != '\n') return false;
        int o = p;
        p = o + 1;
        if (t[p] == '\n') p = o + 2;
        if (t[p] != '.') {
            *pp = p;
            return true;
        }
        for (;;) {
            c = t[p];
            if (c == '\r' || c == '\n') break;
            p++;
            if (t[p] == 0) break;
        }
        *pp = p;
        return true;
    }
    if (t[p] == '\n') p++;
    *pp = p;
    return true;
}

static int match(const u8 *t, int p) {
    int start = p;
    if (t[p] == 0) return -1;
    bool inq = false;
    int depth = 0;
    for (;;) {
        bool ok = false;
        if (!inq) {
            int p2 = p;
            ok = skipc(t, &p2);
            if (ok) p = p2;
        }
        if (!ok) {
            u8 c = t[p];
            if (c == '}' && !inq) {
                depth--;
                if (depth == 0) return p;
                if (depth < 0) return -1;
            } else if (c == '{' && !inq) {
                depth++;
            } else if (c == '"') {
                bool esc = false;
                if (p != start) {
                    int q = p - 1;
                    if (t[q] > 0x3e && (t[q] < 0xa1 || t[q] > 0xdf)) q = p - 2 >= start ? p - 2 : -1;
                    esc = q >= 0 && t[q] == '\\';
                }
                if (!esc) inq = !inq;
            }
            p += dbcs(c) ? 2 : 1;
        }
        if (t[p] == 0) return -1;
    }
}

static void finalize(u8 *t) {
    for (int i = 0; t[i]; i++)
        if (t[i] == 0x10) t[i] = '\n';
}

static ScanRecord *new_record(Scan *s, int kind) {
    if (s->count == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 256;
        s->recs = n2_grow(s->recs, sizeof(ScanRecord) * (size_t)s->cap);
    }
    ScanRecord *r = &s->recs[s->count++];
    memset(r, 0, sizeof *r);
    r->kind = kind;
    r->slot = s->slot;
    return r;
}

static int declare(Scan *s, int ri, int p);

static bool top(Scan *s, int p) {
    u8 *t = s->t;
    if (t[p] == 0) return false;
    bool inq = false;
    for (;;) {
        u8 c = t[p];
        int np;
        if (c == '"') inq = !inq;
        if (ws(t, p)) {
            int q = p + 1, len = 0;
            int kind = keyword(t, q, &len);
            if (!kind || inq) {
                np = q;
            } else if (kind == SCAN_CHAPTER && s->slot != 0) {
                np = q + 1;
            } else {
                ScanRecord *r = new_record(s, kind);
                r->decl = q;
                np = declare(s, (int)(r - s->recs), q + len);
                if (np < 0) return false;
            }
        } else {
            np = p + 1;
            if (!inq) {
                int p2 = p;
                if (skipc(t, &p2)) np = p2 - 1;
            }
        }
        p = np;
        if (t[p] == 0) {
            finalize(t);
            return true;
        }
    }
}

static int params(Scan *s, ScanRecord *r, int p) {
    u8 *t = s->t;
    while (t[p] && t[p] != '(') p++;
    if (!t[p]) return p;
    p++;
    for (int k = 0; k < 16; k++) {
        while (t[p] && ws(t, p)) p++;
        if (t[p] == '/' && (t[p + 1] == '*' || t[p + 1] == '/')) {
            skipc(t, &p);
            while (t[p] && ws(t, p)) p++;
        }
        if (t[p] == ')') break;
        bool quoted = t[p] == '"';
        if (quoted) p++;
        int a = p, n = 0;
        while (t[p]) {
            p++;
            n++;
            if (n > 0x3f || t[p] == ')' || t[p] == ',' || (t[p] == '"' && quoted)) break;
        }
        if (r->paramCount < 16) {
            r->params[r->paramCount] = n2_strndup((const char *)t + a, (size_t)n);
            r->paramQuoted[r->paramCount] = quoted;
            r->paramCount++;
        }
        if (t[p] == '"') {
            p++;
            while (t[p] && ws(t, p)) p++;
        }
        if (t[p] == ')') break;
        if (t[p] == ',') p++;
    }
    return p;
}

static void mark(Marks *m, int start, int end) {
    m->starts = n2_grow(m->starts, sizeof(int) * (size_t)(m->count + 1));
    m->ends = n2_grow(m->ends, sizeof(int) * (size_t)(m->count + 1));
    m->starts[m->count] = start;
    m->ends[m->count] = end;
    m->count++;
}

static bool nested(Scan *s, int ri, Marks *m) {
    u8 *t = s->t;
    int p = s->recs[ri].base;
    bool inq = false;
    for (;;) {
        if (p >= s->recs[ri].end - 10) return true;
        u8 c = t[p];
        int np;
        if (c == '"') inq = !inq;
        if (ws(t, p)) {
            np = p + 1;
            int len;
            if (keyword(t, p + 1, &len) && !inq) {
                int e = match(t, p + 1);
                mark(m, p + 1, e);
                if (e < 0 || !top(s, p)) return false;
                np = e;
            }
        } else if (!inq) {
            int p2 = p;
            np = skipc(t, &p2) ? p2 - 1 : p + 1;
        } else {
            np = p + 1;
        }
        p = np;
    }
}

static bool is_pre(const u8 *t, int p) {
    return t[p] == '<' && (t[p + 1] == 'P' || t[p + 1] == 'p') && (t[p + 2] == 'R' || t[p + 2] == 'r') &&
           (t[p + 3] == 'E' || t[p + 3] == 'e');
}

static bool is_endpre(const u8 *t, int p) {
    return t[p] == '/' && (t[p + 1] == 'P' || t[p + 1] == 'p') && (t[p + 2] == 'R' || t[p + 2] == 'r') &&
           (t[p + 3] == 'E' || t[p + 3] == 'e') && t[p + 4] == '>';
}

static int inline_block(Scan *s, int p) {
    u8 *t = s->t;
    while (t[p] != '{') {
        if (t[p] == 0) return -1;
        p++;
    }
    int ri = (int)(new_record(s, SCAN_INLINE) - s->recs);
    s->recs[ri].base = p + 1;
    s->recs[ri].decl = p;
    int e;
    for (;;) {
        e = match(t, p);
        if (e < 0) return -1;
        p = e + 1;
        bool again = false;
        while (t[p] != 0) {
            if (t[p] != '\r' && t[p] != '\n') {
                if (t[p] == '{') again = true;
                break;
            }
            t[p] = 0x10;
            p++;
        }
        if (!again) break;
    }
    s->recs[ri].end = e - 1;
    return e;
}

static int find_mark(Marks *m, int p) {
    for (int i = 0; i < m->count; i++)
        if (m->starts[i] == p) return i;
    return -1;
}

static bool pre(Scan *s, int ri, Marks *m) {
    u8 *t = s->t;
    int p = s->recs[ri].base;
    bool inq = false;
    for (;;) {
        int end = s->recs[ri].end;
        if (t[p] == 0 || p >= end - 4) return true;
        int k = find_mark(m, p);
        if (k >= 0) p = m->ends[k];
        if (t[p] == '"') inq = !inq;
        if (is_pre(t, p)) {
            mark(m, p, 0);
            int n = m->count - 1;
            int q = p + 4;
            while (t[q] && ws(t, q)) q++;
            bool inpre = true;
            if (t[q] != '>') {
                for (;;) {
                    if (t[q] == 0 || ws(t, q)) break;
                    q++;
                    if (t[q] == '>') {
                        q++;
                        break;
                    }
                }
            } else {
                q++;
            }
            int w = q;
            while (t[w] && ws(t, w)) w++;
            memset(t + q, 0x10, (size_t)(w - q));
            p = w;
            for (;;) {
                u8 c = t[p];
                if (c == 0 || p >= s->recs[ri].end - 4) break;
                if (c == '[') {
                    q = p + 1;
                    while (t[q] && ws(t, q)) q++;
                    if (t[q] != ']') {
                        for (;;) {
                            if (t[q] == 0 || ws(t, q)) break;
                            q++;
                            if (t[q] == ']') {
                                q++;
                                break;
                            }
                        }
                    } else {
                        q++;
                    }
                    w = q;
                    while (t[w] == ' ' || t[w] == '\t' || t[w] == '\r' || t[w] == '\n') w++;
                    memset(t + q, 0x10, (size_t)(w - q));
                    p = w;
                    while (t[p] != '[' && t[p] != 0) {
                        if (is_endpre(t, p)) {
                            if (inpre) break;
                            inpre = true;
                            p += 5;
                        } else if (is_pre(t, p)) {
                            p += 4;
                            inpre = !inpre;
                            while (t[p] && ws(t, p)) p++;
                        } else if (t[p] == '{') {
                            p = inline_block(s, p);
                            if (p < 0) return false;
                        } else {
                            p += dbcs(t[p]) ? 2 : 1;
                        }
                    }
                } else {
                    if (is_endpre(t, p)) {
                        p += 5;
                        m->ends[n] = p;
                        break;
                    }
                    int p2 = p;
                    if (skipc(t, &p2)) p = p2;
                    else p += dbcs(c) ? 2 : 1;
                }
            }
            if (!m->ends[n]) return false;
        } else if (inq) {
            p++;
        } else {
            int p2 = p;
            p = skipc(t, &p2) ? p2 : p + 1;
        }
    }
}

static int declare(Scan *s, int ri, int p) {
    u8 *t = s->t;
    if (t[p] == 0) return -1;
    while (ws(t, p)) {
        p++;
        if (t[p] == 0) return -1;
    }
    int nameStart = p, nameEnd = p, n = 0, paramsAt = -1;
    for (;;) {
        if (ws(t, p) || t[p] == '{') break;
        int p1 = t[p] > 0x80 ? p + 2 : p + 1;
        nameEnd = p1;
        n++;
        if (n > 0xff) {
            p = p1;
            break;
        }
        u8 c = t[p1];
        p = p1;
        if (c == '(' && s->recs[ri].kind == SCAN_FUNCTION) {
            paramsAt = p;
            break;
        }
        if (c == 0) break;
    }
    s->recs[ri].name = n2_strndup((const char *)t + nameStart, (size_t)(nameEnd - nameStart));
    if (s->recs[ri].kind == SCAN_FUNCTION) p = params(s, &s->recs[ri], paramsAt >= 0 ? paramsAt : p);
    while (t[p] != '{') {
        if (t[p] == 0) return -1;
        p++;
    }
    s->recs[ri].base = p + 1;
    int e = match(t, p);
    if (e < 0) return -1;
    s->recs[ri].end = e;
    Marks m = {0};
    bool ok = nested(s, ri, &m) && pre(s, ri, &m);
    free(m.starts);
    free(m.ends);
    if (!ok) return -1;
    int end = s->recs[ri].end;
    s->recs[ri].end = end - 1;
    return end;
}

ScanRecord *scan_process(u8 **texts, const size_t *lens, int fileCount, int *count) {
    Scan s = {0};
    for (int i = 0; i < fileCount; i++) {
        u8 *t = n2_alloc(lens[i] + 16);
        memcpy(t, texts[i], lens[i]);
        s.t = t;
        s.slot = i;
        if (t[0]) top(&s, 0);
        memcpy(texts[i], t, lens[i]);
        free(t);
    }
    *count = s.count;
    return s.recs;
}

void scan_free(ScanRecord *recs, int count) {
    for (int i = 0; i < count; i++) {
        free(recs[i].name);
        for (int k = 0; k < recs[i].paramCount; k++) free(recs[i].params[k]);
    }
    free(recs);
}

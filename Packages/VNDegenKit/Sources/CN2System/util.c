// Memory, strings, Shift-JIS and the small containers the runtime uses.
#include "n2.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

extern const uint16_t n2_sjis_table[11340];

void *n2_alloc(size_t size) {
    void *p = calloc(1, size ? size : 1);
    if (!p) abort();
    return p;
}

void *n2_grow(void *p, size_t size) {
    p = realloc(p, size ? size : 1);
    if (!p) abort();
    return p;
}

char *n2_strdup(const char *s) {
    return n2_strndup(s, strlen(s));
}

char *n2_strndup(const char *s, size_t n) {
    char *d = n2_alloc(n + 1);
    memcpy(d, s, n);
    return d;
}

char *n2_fmt(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *s = n2_alloc((size_t)n + 1);
    vsnprintf(s, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return s;
}

bool n2_streq_ci(const char *a, const char *b) {
    for (; *a && *b; a++, b++)
        if (tolower((u8)*a) != tolower((u8)*b)) return false;
    return *a == *b;
}

bool n2_glob(const char *p, const char *s) {
    // Iterative match with one backtrack point, enough for '*' patterns.
    const char *star = NULL, *mark = NULL;
    while (*s) {
        if (*p == '*') {
            star = p++;
            mark = s;
        } else if (*p && tolower((u8)*p) == tolower((u8)*s)) {
            p++;
            s++;
        } else if (star) {
            p = star + 1;
            s = ++mark;
        } else {
            return false;
        }
    }
    while (*p == '*') p++;
    return *p == 0;
}

u32 n2_hash(const char *s) {
    u32 h = 2166136261u;
    for (; *s; s++) h = (h ^ (u32)tolower((u8)*s)) * 16777619u;
    return h;
}

static int sjis_index(u8 lead, u8 trail) {
    int slot;
    if (lead >= 0x81 && lead <= 0x9f) slot = lead - 0x81;
    else if (lead >= 0xe0 && lead <= 0xfc) slot = lead - 0xe0 + 31;
    else return -1;
    if (trail < 0x40 || trail > 0xfc) return -1;
    return slot * 189 + (trail - 0x40);
}

static void put_utf8(Buf *b, u32 cp) {
    u8 t[4];
    int n;
    if (cp < 0x80) { t[0] = (u8)cp; n = 1; }
    else if (cp < 0x800) { t[0] = 0xc0 | (u8)(cp >> 6); t[1] = 0x80 | (cp & 0x3f); n = 2; }
    else if (cp < 0x10000) { t[0] = 0xe0 | (u8)(cp >> 12); t[1] = 0x80 | ((cp >> 6) & 0x3f); t[2] = 0x80 | (cp & 0x3f); n = 3; }
    else { t[0] = 0xf0 | (u8)(cp >> 18); t[1] = 0x80 | ((cp >> 12) & 0x3f); t[2] = 0x80 | ((cp >> 6) & 0x3f); t[3] = 0x80 | (cp & 0x3f); n = 4; }
    buf_put(b, t, (size_t)n);
}

char *n2_sjis_to_utf8(const u8 *s, size_t n, size_t *outLen) {
    Buf b = {0};
    for (size_t i = 0; i < n;) {
        u8 c = s[i];
        if (c < 0x80) {
            buf_u8(&b, c);
            i++;
        } else if (c >= 0xa1 && c <= 0xdf) {
            put_utf8(&b, 0xff61 + (c - 0xa1));  // half-width katakana
            i++;
        } else if (i + 1 < n) {
            int idx = sjis_index(c, s[i + 1]);
            u32 cp = idx >= 0 ? n2_sjis_table[idx] : 0;
            put_utf8(&b, cp ? cp : 0x30fb);
            i += 2;
        } else {
            i++;
        }
    }
    buf_u8(&b, 0);
    if (outLen) *outLen = b.len - 1;
    return (char *)b.data;
}

// Reverse lookup, built on first use. Unicode points fit 16 bits for everything CP932 maps.
static u16 *g_reverse;

char *n2_utf8_to_sjis(const char *s, size_t *outLen) {
    if (!g_reverse) {
        u16 *r = n2_alloc(65536 * sizeof(u16));
        for (int i = 0; i < 11340; i++) {
            u16 cp = n2_sjis_table[i];
            if (!cp || r[cp]) continue;
            int slot = i / 189, trail = i % 189 + 0x40;
            int lead = slot < 31 ? slot + 0x81 : slot - 31 + 0xe0;
            r[cp] = (u16)(lead << 8 | trail);
        }
        g_reverse = r;
    }
    Buf b = {0};
    const char *p = s;
    u32 cp;
    while ((cp = n2_utf8_next(&p))) {
        if (cp < 0x80) buf_u8(&b, (u8)cp);
        else if (cp >= 0xff61 && cp <= 0xff9f) buf_u8(&b, (u8)(0xa1 + cp - 0xff61));
        else if (cp < 65536 && g_reverse[cp]) {
            buf_u8(&b, (u8)(g_reverse[cp] >> 8));
            buf_u8(&b, (u8)g_reverse[cp]);
        } else {
            buf_u8(&b, '?');
        }
    }
    buf_u8(&b, 0);
    if (outLen) *outLen = b.len - 1;
    return (char *)b.data;
}

u32 n2_utf8_next(const char **pp) {
    const u8 *p = (const u8 *)*pp;
    u32 c = *p;
    if (!c) return 0;
    int n = 0;
    if (c < 0x80) n = 0;
    else if ((c & 0xe0) == 0xc0) { c &= 0x1f; n = 1; }
    else if ((c & 0xf0) == 0xe0) { c &= 0x0f; n = 2; }
    else if ((c & 0xf8) == 0xf0) { c &= 0x07; n = 3; }
    else { *pp = (const char *)p + 1; return 0xfffd; }
    p++;
    for (int i = 0; i < n; i++) {
        if ((*p & 0xc0) != 0x80) { *pp = (const char *)p; return 0xfffd; }
        c = c << 6 | (*p++ & 0x3f);
    }
    *pp = (const char *)p;
    return c;
}

void buf_put(Buf *b, const void *p, size_t n) {
    if (b->len + n > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 256;
        while (cap < b->len + n) cap *= 2;
        b->data = n2_grow(b->data, cap);
        b->cap = cap;
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
}

void buf_u8(Buf *b, u8 v) { buf_put(b, &v, 1); }

void buf_u32(Buf *b, u32 v) {
    u8 t[4] = {(u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24)};
    buf_put(b, t, 4);
}

void buf_str(Buf *b, const char *s) { buf_put(b, s, strlen(s)); }

void buf_free(Buf *b) {
    free(b->data);
    memset(b, 0, sizeof *b);
}

static MapEntry *map_slot(const Map *m, const char *key, u32 h) {
    int mask = m->cap - 1;
    for (int i = (int)(h & (u32)mask);; i = (i + 1) & mask) {
        MapEntry *e = &m->entries[i];
        if (!e->key || (e->hash == h && n2_streq_ci(e->key, key))) return e;
    }
}

void *map_get(const Map *m, const char *key) {
    if (!m->cap) return NULL;
    MapEntry *e = map_slot(m, key, n2_hash(key));
    return e->key ? e->value : NULL;
}

static void map_rehash(Map *m, int cap) {
    MapEntry *old = m->entries;
    int oldCap = m->cap;
    m->entries = n2_alloc(sizeof(MapEntry) * (size_t)cap);
    m->cap = cap;
    m->count = 0;
    for (int i = 0; i < oldCap; i++) {
        if (!old[i].key) continue;
        if (!old[i].value) { free(old[i].key); continue; }
        MapEntry *e = map_slot(m, old[i].key, old[i].hash);
        *e = old[i];
        m->count++;
    }
    free(old);
}

void map_set(Map *m, const char *key, void *value) {
    if (!m->cap) map_rehash(m, 64);
    u32 h = n2_hash(key);
    MapEntry *e = map_slot(m, key, h);
    if (e->key) {
        // Removal keeps the key as a tombstone with a NULL value, so probe chains stay intact.
        e->value = value;
        return;
    }
    if (!value) return;
    if ((m->count + 1) * 4 > m->cap * 3) {
        map_rehash(m, m->cap * 2);
        e = map_slot(m, key, h);
    }
    e->key = n2_strdup(key);
    e->hash = h;
    e->value = value;
    m->count++;
}

void map_free(Map *m, void (*freeValue)(void *)) {
    for (int i = 0; i < m->cap; i++) {
        if (!m->entries[i].key) continue;
        if (freeValue && m->entries[i].value) freeValue(m->entries[i].value);
        free(m->entries[i].key);
    }
    free(m->entries);
    memset(m, 0, sizeof *m);
}

// ---- values ----

Str *str_new(const char *s, int len) {
    Str *r = n2_alloc(sizeof(Str) + (size_t)len + 1);
    r->rc = 1;
    r->len = len;
    memcpy(r->s, s, (size_t)len);
    return r;
}

Str *str_ref(Str *s) {
    if (s) s->rc++;
    return s;
}

void str_unref(Str *s) {
    if (s && --s->rc == 0) free(s);
}

Value v_null(void) { return (Value){.t = V_NULL}; }
Value v_num(double n) { return (Value){.t = V_NUM, .n = n}; }
Value v_bool(bool b) { return (Value){.t = V_BOOL, .n = b ? 1 : 0}; }
Value v_str(const char *s) { return v_strn(s, (int)strlen(s)); }
Value v_strn(const char *s, int len) { return (Value){.t = V_STR, .s = str_new(s, len)}; }

Value v_copy(const Value *v) {
    Value r = *v;
    if (r.t == V_STR) str_ref(r.s);
    return r;
}

void v_free(Value *v) {
    if (v->t == V_STR) str_unref(v->s);
    v->t = V_NULL;
    v->s = NULL;
}

bool v_truthy(const Value *v) {
    switch (v->t) {
    case V_NUM:
    case V_BOOL: return v->n != 0;
    case V_STR: return v->s->len > 0 && !n2_streq_ci(v->s->s, "false") && !n2_streq_ci(v->s->s, "null");
    default: return false;
    }
}

double v_tonum(const Value *v) {
    switch (v->t) {
    case V_NUM:
    case V_BOOL: return v->n;
    case V_STR: {
        const char *s = v->s->s;
        if (s[0] == '#' && strlen(s) == 7) return (double)strtol(s + 1, NULL, 16);
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) return (double)strtol(s + 2, NULL, 16);
        if (n2_streq_ci(s, "true")) return 1;
        return strtod(s, NULL);
    }
    default: return 0;
    }
}

const char *v_tostr(const Value *v, char *tmp, size_t tmpSize) {
    switch (v->t) {
    case V_STR: return v->s->s;
    case V_NUM:
        if (v->n == (double)(long long)v->n) snprintf(tmp, tmpSize, "%lld", (long long)v->n);
        else snprintf(tmp, tmpSize, "%g", v->n);
        return tmp;
    case V_BOOL: return v->n ? "true" : "false";
    default: return "";
    }
}

bool v_equal(const Value *a, const Value *b) {
    if (a->t == V_NULL || b->t == V_NULL) {
        // null equals false, 0 and the empty string, as unset variables compare in the scripts.
        const Value *o = a->t == V_NULL ? b : a;
        return o->t == V_NULL || !v_truthy(o);
    }
    if (a->t == V_STR && b->t == V_STR) return strcmp(a->s->s, b->s->s) == 0;
    if (a->t == V_STR || b->t == V_STR) {
        char t1[64], t2[64];
        const Value *s = a->t == V_STR ? a : b, *o = a->t == V_STR ? b : a;
        if (o->t == V_BOOL) return v_truthy(s) == (o->n != 0);
        return strcmp(v_tostr(s, t1, sizeof t1), v_tostr(o, t2, sizeof t2)) == 0;
    }
    return a->n == b->n;
}

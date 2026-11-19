#include "bsx.h"


static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) s[--n] = 0;
    return s;
}

static bool utf8_text(const u8 *s, size_t n) {
    for (size_t i = 0; i < n;) {
        u8 c = s[i];
        int len = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 0;
        if (!len || i + (size_t)len > n) return false;
        for (int k = 1; k < len; k++)
            if ((s[i + (size_t)k] & 0xc0) != 0x80) return false;
        i += (size_t)len;
    }
    return true;
}

bool bx_cfg_parse(Cfg *c, const u8 *data, size_t size) {
    memset(c, 0, sizeof *c);
    if (!data) return false;
    char *text = bx_alloc(size + 1);
    memcpy(text, data, size);
    char *utf = text;
    if (!utf8_text(data, size)) {
        utf = bx_sjis_to_utf8(text);
        free(text);
    }
    char section[128] = "";
    for (char *line = utf; line && *line;) {
        char *next = strchr(line, '\n');
        if (next) *next++ = 0;
        char *semi = strchr(line, ';');
        if (semi) *semi = 0;
        char *l = trim(line);
        if (l[0] == '[') {
            char *e = strchr(l, ']');
            if (e) *e = 0;
            snprintf(section, sizeof section, "%s", l + 1);
        } else {
            char *eq = strchr(l, '=');
            if (eq) {
                *eq = 0;
                if (c->count == c->cap) {
                    c->cap = c->cap ? c->cap * 2 : 256;
                    c->sec = bx_realloc(c->sec, sizeof(char *) * (size_t)c->cap);
                    c->key = bx_realloc(c->key, sizeof(char *) * (size_t)c->cap);
                    c->val = bx_realloc(c->val, sizeof(char *) * (size_t)c->cap);
                }
                c->sec[c->count] = bx_strdup(section);
                c->key[c->count] = bx_strdup(trim(l));
                c->val[c->count] = bx_strdup(trim(eq + 1));
                c->count++;
            }
        }
        line = next;
    }
    free(utf);
    return true;
}

void bx_cfg_free(Cfg *c) {
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

const char *bx_cfg_get(const Cfg *c, const char *sec, const char *key) {
    for (int i = 0; i < c->count; i++)
        if (!strcmp(c->sec[i], sec) && !strcmp(c->key[i], key)) return c->val[i];
    return NULL;
}

static int expr_add(const char **p);

static int expr_atom(const char **p) {
    while (**p == ' ' || **p == '\t') (*p)++;
    if (**p == '(') {
        (*p)++;
        int v = expr_add(p);
        while (**p == ' ') (*p)++;
        if (**p == ')') (*p)++;
        return v;
    }
    if (**p == '-') {
        (*p)++;
        return -expr_atom(p);
    }
    if (**p == '+') {
        (*p)++;
        return expr_atom(p);
    }
    char *end;
    double v;
    if ((*p)[0] == '0' && ((*p)[1] == 'x' || (*p)[1] == 'X')) v = (double)strtoul(*p, &end, 16);
    else v = strtod(*p, &end);
    *p = end;
    return (int)v;
}

static int expr_mul(const char **p) {
    int v = expr_atom(p);
    for (;;) {
        while (**p == ' ') (*p)++;
        if (**p == '*') {
            (*p)++;
            v *= expr_atom(p);
        } else if (**p == '/') {
            (*p)++;
            int d = expr_atom(p);
            v = d ? v / d : 0;
        } else return v;
    }
}

static int expr_add(const char **p) {
    int v = expr_mul(p);
    for (;;) {
        while (**p == ' ') (*p)++;
        if (**p == '+') {
            (*p)++;
            v += expr_mul(p);
        } else if (**p == '-') {
            (*p)++;
            v -= expr_mul(p);
        } else return v;
    }
}

int bx_eval(const char *expr, const char **end) {
    const char *p = expr;
    int v = expr_add(&p);
    if (end) *end = p;
    return v;
}

int bx_cfg_field(const char *v, int index, char *out, size_t size) {
    if (!v) return -1;
    for (int i = 0; i < index; i++) {
        v = strchr(v, ',');
        if (!v) return -1;
        v++;
    }
    while (*v == ' ' || *v == '\t') v++;
    size_t n = 0;
    while (v[n] && v[n] != ',') n++;
    while (n && (v[n - 1] == ' ' || v[n - 1] == '\t')) n--;
    if (n >= size) n = size - 1;
    memcpy(out, v, n);
    out[n] = 0;
    return (int)n;
}

int bx_cfg_ints(const Cfg *c, const char *sec, const char *key, int *out, int max) {
    const char *v = bx_cfg_get(c, sec, key);
    if (!v) return 0;
    int n = 0;
    while (n < max && *v) {
        const char *e;
        out[n++] = bx_eval(v, &e);
        while (*e && *e != ',') e++;
        if (*e != ',') break;
        v = e + 1;
    }
    return n;
}

int bx_cfg_int(const Cfg *c, const char *sec, const char *key, int def) {
    int v;
    return bx_cfg_ints(c, sec, key, &v, 1) ? v : def;
}

u32 bx_cfg_color(const Cfg *c, const char *sec, const char *key, u32 def) {
    const char *v = bx_cfg_get(c, sec, key);
    if (!v) return def;
    u32 argb = (u32)strtoul(v, NULL, 0);
    return (argb & 0xff00ff00u) | (argb >> 16 & 0xff) | (argb & 0xff) << 16;
}

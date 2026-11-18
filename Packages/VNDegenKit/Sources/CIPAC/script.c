#include "ipac.h"

#include <ctype.h>

static void add(Script *s, int kind, int line, const char *b, size_t n) {
    if (s->count == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 1024;
        s->st = ip_realloc(s->st, sizeof(Stmt) * (size_t)s->cap);
    }
    char *str = ip_alloc(n + 1);
    memcpy(str, b, n);
    s->st[s->count++] = (Stmt){(u8)kind, line, str};
}

static bool is_space(u8 c) { return c == ' ' || c == '\t'; }

static void trim(const char **b, const char **e) {
    while (*b < *e && is_space((u8)**b)) (*b)++;
    while (*e > *b && (is_space((u8)(*e)[-1]) || (*e)[-1] == '\r')) (*e)--;
}

static const char *comment_at(const char *b, const char *e) {
    for (const char *p = b; p + 1 < e; p += ip_sjis_len((const u8 *)p))
        if (p[0] == '/' && p[1] == '/' && (p == b || p[-1] != ':')) return p;
    return e;
}

static void split_braces(Script *s, int line, const char *b, const char *e) {
    while (b < e) {
        trim(&b, &e);
        if (b >= e) return;
        const char *p = b;
        while (p < e && *p != '{' && *p != '}') p += ip_sjis_len((const u8 *)p);
        const char *q = p;
        const char *bb = b;
        trim(&bb, &q);
        if (q > bb) {
            if (*bb == '#') add(s, S_CMD, line, bb + 1, (size_t)(q - bb - 1));
            else if (isdigit((u8)*bb)) add(s, S_CASE, line, bb, (size_t)(q - bb));
            else if (*bb == '<') {
                const char *z = memchr(bb, '>', (size_t)(q - bb));
                add(s, S_JUMP, line, bb + 1, (size_t)((z ? z : q) - bb - 1));
            } else add(s, S_CMD, line, bb, (size_t)(q - bb));
        }
        if (p >= e) return;
        add(s, *p == '{' ? S_OPEN : S_CLOSE, line, "", 0);
        b = p + 1;
    }
}

static bool sprite_line(const char *b, const char *e) {
    if (e - b < 3 || !isupper((u8)b[0]) || !is_space((u8)b[1])) return false;
    for (const char *p = b + 2; p < e; p++)
        if (is_space((u8)*p)) return false;
    return true;
}

Script *script_parse(const char *name, const u8 *data, size_t size) {
    Script *s = ip_alloc(sizeof *s);
    s->name = ip_strdup(name);
    const char *p = (const char *)data, *end = p + size;
    int line = 0;
    bool choosing = false;
    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        const char *b = p, *e = nl ? nl : end;
        p = nl ? nl + 1 : end;
        line++;
        trim(&b, &e);
        if (b >= e) {
            choosing = false;
            add(s, S_BLANK, line, "", 0);
            continue;
        }
        if (e - b >= 2 && b[0] == '/' && b[1] == '/') continue;
        if (choosing) {
            add(s, S_TEXT, line, b, (size_t)(e - b));
            continue;
        }
        u8 c = (u8)*b;
        if (c == '#' || c == '{' || c == '}' || (isdigit(c) && memchr(b, '{', (size_t)(e - b)))) {
            split_braces(s, line, b, comment_at(b, e));
        } else if (c == ':') {
            const char *z = comment_at(b, e);
            b++;
            trim(&b, &z);
            add(s, S_LABEL, line, b, (size_t)(z - b));
        } else if (c == '[') {
            const char *z = memchr(b, ']', (size_t)(e - b));
            add(s, S_BG, line, b + 1, (size_t)((z ? z : e) - b - 1));
        } else if (c == '<') {
            const char *z = memchr(b, '>', (size_t)(e - b));
            add(s, S_JUMP, line, b + 1, (size_t)((z ? z : e) - b - 1));
        } else if (c == '-' && e - b > 1 && isalnum((u8)b[1])) {
            add(s, S_VOICE, line, b + 1, (size_t)(e - b - 1));
        } else if (e - b >= 4 && (u8)b[0] == 0x81 && (u8)b[1] == 0x79) {
            const char *z = e;
            if (z - b >= 4 && (u8)z[-2] == 0x81 && (u8)z[-1] == 0x7a) z -= 2;
            add(s, S_NAME, line, b + 2, (size_t)(z - b - 2));
        } else if (c == '?' && e - b == 1) {
            add(s, S_CHOICE, line, "", 0);
            choosing = true;
        } else if (sprite_line(b, e)) {
            add(s, S_SPRITE, line, b, (size_t)(e - b));
        } else {
            add(s, S_TEXT, line, b, (size_t)(e - b));
        }
    }
    add(s, S_BLANK, line + 1, "", 0);
    return s;
}

Script *script_load(IP *t, const char *name) {
    for (int i = 0; i < t->scriptCount; i++)
        if (ip_ieq(t->scripts[i]->name, name)) return t->scripts[i];
    size_t n;
    u8 *d = store_load(&t->store, PK_SCR, name, ".ORG", &n);
    if (!d) return NULL;
    Script *s = script_parse(name, d, n);
    free(d);
    t->scripts = ip_realloc(t->scripts, sizeof(Script *) * (size_t)(t->scriptCount + 1));
    t->scripts[t->scriptCount++] = s;
    return s;
}

void script_free(Script *s) {
    if (!s) return;
    for (int i = 0; i < s->count; i++) free(s->st[i].s);
    free(s->st);
    free(s->name);
    free(s);
}

int script_label(const Script *s, const char *label) {
    for (int i = 0; i < s->count; i++)
        if (s->st[i].kind == S_LABEL && !strcmp(s->st[i].s, label)) return i;
    for (int i = 0; i < s->count; i++)
        if (s->st[i].kind == S_LABEL && ip_ieq(s->st[i].s, label)) return i;
    return -1;
}

int script_block_end(const Script *s, int open) {
    int depth = 0;
    for (int i = open; i < s->count; i++) {
        if (s->st[i].kind == S_OPEN) depth++;
        else if (s->st[i].kind == S_CLOSE && --depth == 0) return i;
    }
    return s->count;
}

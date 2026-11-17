#include "th.h"

static IniSection *add_section(Ini *ini, const char *name) {
    ini->sections = th_realloc(ini->sections, sizeof(IniSection) * (size_t)(ini->count + 1));
    IniSection *s = &ini->sections[ini->count++];
    memset(s, 0, sizeof *s);
    s->name = th_strdup(name);
    return s;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}

bool ini_load(Ini *ini, const u8 *data, size_t size) {
    memset(ini, 0, sizeof *ini);
    IniSection *cur = NULL;
    size_t pos = 0;
    while (pos + 2 <= size) {
        u16 len = rd16(data + pos);
        pos += 2;
        if (pos + len > size) break;
        char *line = th_alloc((size_t)len + 1);
        memcpy(line, data + pos, len);
        pos += len;
        char *s = trim(line);
        if (*s == '[') {
            char *e = strchr(s, ']');
            if (e) *e = 0;
            cur = add_section(ini, s + 1);
        } else if (*s && cur) {
            char *eq = strchr(s, '=');
            cur->lines = th_realloc(cur->lines, sizeof(IniLine) * (size_t)(cur->count + 1));
            IniLine *l = &cur->lines[cur->count++];
            if (eq) {
                *eq = 0;
                l->key = th_strdup(trim(s));
                l->value = th_strdup(trim(eq + 1));
            } else {
                l->key = th_strdup(s);
                l->value = th_strdup("");
            }
        }
        free(line);
    }
    return ini->count > 0;
}

void ini_free(Ini *ini) {
    for (int i = 0; i < ini->count; i++) {
        for (int j = 0; j < ini->sections[i].count; j++) {
            free(ini->sections[i].lines[j].key);
            free(ini->sections[i].lines[j].value);
        }
        free(ini->sections[i].lines);
        free(ini->sections[i].name);
    }
    free(ini->sections);
    memset(ini, 0, sizeof *ini);
}

const IniSection *ini_section(const Ini *ini, const char *section) {
    for (int i = 0; i < ini->count; i++)
        if (th_ieq(ini->sections[i].name, section)) return &ini->sections[i];
    return NULL;
}

const char *ini_get(const Ini *ini, const char *section, const char *key) {
    const IniSection *s = ini_section(ini, section);
    if (!s) return NULL;
    for (int j = 0; j < s->count; j++)
        if (th_ieq(s->lines[j].key, key)) return s->lines[j].value;
    return NULL;
}

int ini_int(const Ini *ini, const char *section, const char *key, int def) {
    const char *v = ini_get(ini, section, key);
    if (!v || !*v) return def;
    return (int)strtol(v, NULL, 0);
}

int ini_ints(const Ini *ini, const char *section, const char *key, int *out, int max) {
    const char *v = ini_get(ini, section, key);
    if (!v) return 0;
    int n = 0;
    const char *p = v;
    while (*p && n < max) {
        while (*p == ' ' || *p == '\t') p++;
        char *end;
        long x = strtol(p, &end, 0);
        out[n++] = end == p ? 0 : (int)x;
        p = end;
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
        else break;
    }
    return n;
}

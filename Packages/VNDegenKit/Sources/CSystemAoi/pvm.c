#include "sa.h"

typedef struct Lex {
    const char *p, *end;
} Lex;

static void skip_ws(Lex *l) {
    while (l->p < l->end && (*l->p == ' ' || *l->p == '\t' || *l->p == '\r' || *l->p == '\n')) l->p++;
}

static bool ident(Lex *l, char *out, size_t n) {
    skip_ws(l);
    size_t k = 0;
    while (l->p < l->end && ((*l->p >= 'A' && *l->p <= 'Z') || (*l->p >= 'a' && *l->p <= 'z'))) {
        if (k + 1 < n) out[k++] = *l->p;
        l->p++;
    }
    out[k] = 0;
    return k > 0;
}

static const char *args_end(Lex *l) {
    int depth = 0;
    bool quote = false;
    for (const char *q = l->p; q < l->end; q++) {
        if (*q == '"') quote = !quote;
        if (quote) continue;
        if (*q == '(' || *q == '[') depth++;
        if (*q == ')' || *q == ']') {
            if (--depth == 0 && *q == ')') return q;
        }
    }
    return NULL;
}

static int split_args(const char *s, const char *e, char parts[][64], int max) {
    int n = 0, k = 0, depth = 0;
    bool quote = false;
    for (const char *q = s; q < e && n < max; q++) {
        char c = *q;
        if (c == '"') {
            quote = !quote;
            continue;
        }
        if (!quote && (c == '[' || c == ']')) {
            depth += c == '[' ? 1 : -1;
            continue;
        }
        if (!quote && c == ',' && depth == 0) {
            parts[n][k] = 0;
            n++;
            k = 0;
            continue;
        }
        if (!quote && (c == ' ' || c == '\t')) continue;
        if (!quote && c == ',' ) c = ' ';
        if (k < 63) parts[n][k++] = c;
    }
    if (n < max) {
        parts[n][k] = 0;
        n++;
    }
    return n;
}

static u32 color(const char *s) { return *s == '#' ? (u32)strtoul(s + 1, NULL, 16) : 0; }

static void floats(const char *s, float *out, int n) {
    char *e;
    for (int i = 0; i < n; i++) {
        out[i] = strtof(s, &e);
        if (e == s) break;
        s = e;
        while (*s == ' ') s++;
    }
}

int sa_pvm_find(const Pvm *m, const char *name) {
    for (int i = 0; i < m->count; i++)
        if (!strcmp(m->layer[i].name, name)) return i;
    return -1;
}

bool sa_pvm_parse(Pvm *m, const u8 *data, size_t size) {
    memset(m, 0, sizeof *m);
    Lex l = {(const char *)data, (const char *)data + size};
    int stack[64], sp = 0, cap = 0, vcap = 0, tcap = 0;
    char word[32];
    while (l.p < l.end) {
        skip_ws(&l);
        if (l.p >= l.end) break;
        if (*l.p == '}') {
            if (sp) sp--;
            l.p++;
            continue;
        }
        if (!ident(&l, word, sizeof word)) {
            l.p++;
            continue;
        }
        skip_ws(&l);
        if (l.p >= l.end || *l.p != '(') continue;
        const char *e = args_end(&l);
        if (!e) break;
        char parts[12][64];
        int n = split_args(l.p + 1, e, parts, 12);
        l.p = e + 1;
        skip_ws(&l);
        PvmLayer *cur = sp ? &m->layer[stack[sp - 1]] : NULL;
        if (!strcmp(word, "Layer")) {
            if (m->count == cap) {
                cap = cap ? cap * 2 : 64;
                m->layer = sa_realloc(m->layer, sizeof(PvmLayer) * (size_t)cap);
            }
            PvmLayer *ly = &m->layer[m->count];
            memset(ly, 0, sizeof *ly);
            snprintf(ly->name, sizeof ly->name, "%s", parts[0]);
            ly->parent = sp ? stack[sp - 1] : -1;
            ly->diffuse = 0xffffffff;
            vcap = tcap = 0;
            if (l.p < l.end && *l.p == '{') {
                l.p++;
                if (sp < 64) stack[sp++] = m->count;
            }
            m->count++;
        } else if (!strcmp(word, "Attribute")) {
            if (!strcmp(parts[0], "BackFace")) m->backFace = true;
        } else if (!cur) {
            continue;
        } else if (!strcmp(word, "Switch")) {
            cur->sw = atoi(parts[0]);
        } else if (!strcmp(word, "Material") && n >= 5) {
            cur->diffuse = color(parts[0]);
            cur->ambient = color(parts[1]);
            cur->power = strtof(parts[2], NULL);
            cur->specular = color(parts[3]);
            cur->emissive = color(parts[4]);
        } else if (!strcmp(word, "Stage")) {
            for (int i = 1; i < n; i++)
                if (strchr(parts[i], '.')) snprintf(cur->texture, sizeof cur->texture, "%s", parts[i]);
        } else if (!strcmp(word, "Blend")) {
            cur->blend = atoi(parts[0]);
        } else if (!strcmp(word, "Translate") && n >= 3) {
            for (int i = 0; i < 3; i++) cur->translate[i] = strtof(parts[i], NULL);
            cur->hasTranslate = true;
        } else if (!strcmp(word, "Range") && n >= 3) {
            for (int i = 0; i < 3; i++) cur->range[i] = atoi(parts[i]);
            cur->hasRange = true;
        } else if (!strcmp(word, "Face") && n >= 3) {
            if (cur->tcount == tcap) {
                tcap = tcap ? tcap * 2 : 64;
                cur->tri = sa_realloc(cur->tri, sizeof(u16) * 3 * (size_t)tcap);
            }
            for (int i = 0; i < 3; i++) cur->tri[cur->tcount * 3 + i] = (u16)atoi(parts[i]);
            cur->tcount++;
        } else if (!strcmp(word, "Vertex") && n >= 3) {
            if (cur->vcount == vcap) {
                vcap = vcap ? vcap * 2 : 64;
                cur->v = sa_realloc(cur->v, sizeof(PvmVertex) * (size_t)vcap);
            }
            PvmVertex *v = &cur->v[cur->vcount++];
            memset(v, 0, sizeof *v);
            floats(parts[0], v->pos, 3);
            floats(parts[1], v->normal, 3);
            floats(parts[2], v->uv, 2);
        }
    }
    for (int i = 0; i < m->count; i++) {
        PvmLayer *ly = &m->layer[i];
        for (int t = 0; t < ly->tcount * 3; t++)
            if (ly->tri[t] >= ly->vcount) ly->tri[t] = 0;
    }
    return m->count > 0;
}

void sa_pvm_free(Pvm *m) {
    for (int i = 0; i < m->count; i++) {
        free(m->layer[i].v);
        free(m->layer[i].tri);
    }
    free(m->layer);
    memset(m, 0, sizeof *m);
}

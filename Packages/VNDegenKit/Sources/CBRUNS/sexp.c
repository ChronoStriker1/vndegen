#include "br.h"

#include <ctype.h>

static const struct {
    const char *name;
    u8 op;
} ops[] = {
    {"define-set", A_DEFINE_SET},
    {"define-sub", A_DEFINE_SUB},
    {"call-sub", A_CALL_SUB},
    {"call-far", A_CALL_FAR},
    {"jump", A_JUMP},
    {"return", A_RETURN},
    {"label", A_LABEL},
    {"goto", A_GOTO},
    {"if", A_IF},
    {"eval", A_EVAL},
    {"eval-noeffect", A_EVAL_NOEFFECT},
    {"switch", A_SWITCH},
    {"send-string", A_SEND_STRING},
    {"call-user-func", A_CALL_USER_FUNC},
    {"assign-word", A_ASSIGN_WORD},
    {"assign-local", A_ASSIGN_LOCAL},
    {"assign-acc", A_ASSIGN_ACC},
    {"assign-reflag", A_ASSIGN_REFLAG},
    {"idref-word", A_IDREF_WORD},
    {"idref-local", A_IDREF_LOCAL},
    {"idref-acc", A_IDREF_ACC},
    {"idref-reflag", A_IDREF_REFLAG},
    {"pre-incr-word", A_PRE_INCR_WORD},
    {"pre-incr-local", A_PRE_INCR_LOCAL},
    {"pre-incr-acc", A_PRE_INCR_ACC},
    {"pre-decr-word", A_PRE_DECR_WORD},
    {"pre-decr-local", A_PRE_DECR_LOCAL},
    {"pre-decr-acc", A_PRE_DECR_ACC},
    {"bop-add", A_ADD},
    {"bop-sub", A_SUB},
    {"bop-mul", A_MUL},
    {"bop-div", A_DIV},
    {"bop-mod", A_MOD},
    {"bop-equal?", A_EQ},
    {"bop-not-equal?", A_NE},
    {"bop-less?", A_LT},
    {"bop-less-equal?", A_LE},
    {"bop-great?", A_GT},
    {"bop-great-equal?", A_GE},
    {"bop-logical-and", A_LAND},
    {"bop-logical-or", A_LOR},
    {"bop-bit-and", A_BAND},
    {"bop-bit-or", A_BOR},
    {"bop-bit-xor", A_BXOR},
    {"bop-shift-left", A_SHL},
    {"bop-shift-right", A_SHR},
    {"boolean-not", A_NOT},
    {"negate", A_NEG},
    {"three-op", A_THREE_OP},
};

static u8 op_code(const char *s) {
    if (strncmp(s, "a3:", 3)) return A_NONE;
    s += 3;
    for (size_t i = 0; i < sizeof ops / sizeof *ops; i++)
        if (!strcmp(ops[i].name, s)) return ops[i].op;
    return A_NONE;
}

typedef struct Reader {
    const u8 *p, *end;
} Reader;

static void skip_space(Reader *r) {
    while (r->p < r->end) {
        if (*r->p == ';') {
            while (r->p < r->end && *r->p != '\n') r->p++;
        } else if (isspace(*r->p) || *r->p == '\'') r->p++;
        else break;
    }
}

static bool sjis_lead(u8 c) { return (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc); }

static Node *node(u8 t) {
    Node *n = br_alloc(sizeof *n);
    n->t = t;
    return n;
}

static void push(Node *list, Node *k, int *cap) {
    if (list->n == *cap) {
        *cap = *cap ? *cap * 2 : 4;
        list->k = br_realloc(list->k, sizeof(Node *) * (size_t)*cap);
    }
    list->k[list->n++] = k;
}

static Node *read_atom(Reader *r) {
    const u8 *s = r->p;
    while (r->p < r->end && !isspace(*r->p) && *r->p != '(' && *r->p != ')' && *r->p != ';') r->p += sjis_lead(*r->p) ? 2 : 1;
    if (r->p > r->end) r->p = r->end;
    size_t len = (size_t)(r->p - s);
    char *t = br_alloc(len + 1);
    memcpy(t, s, len);
    char *endp;
    long long v = strtoll(t, &endp, 0);
    if (len && *endp == 0 && (isdigit((u8)t[0]) || ((t[0] == '-' || t[0] == '+') && len > 1 && isdigit((u8)t[1])))) {
        Node *n = node(N_INT);
        n->i = (i32)v;
        free(t);
        return n;
    }
    Node *n = node(N_SYM);
    n->s = t;
    n->op = op_code(t);
    return n;
}

static Node *read_string(Reader *r) {
    r->p++;
    size_t cap = 64, len = 0;
    char *t = br_alloc(cap);
    while (r->p < r->end && *r->p != '"') {
        u8 c = *r->p++;
        if (c == '\\' && r->p < r->end) c = *r->p++;
        if (len + 3 >= cap) t = br_realloc(t, cap *= 2);
        t[len++] = (char)c;
        if (sjis_lead(c) && r->p < r->end) t[len++] = (char)*r->p++;
    }
    if (r->p < r->end) r->p++;
    t[len] = 0;
    Node *n = node(N_STR);
    n->s = t;
    return n;
}

static Node *read_list(Reader *r) {
    r->p++;
    Node *list = node(N_LIST);
    int cap = 0;
    for (;;) {
        skip_space(r);
        if (r->p >= r->end) break;
        if (*r->p == ')') {
            r->p++;
            break;
        }
        if (*r->p == '.' && r->p + 1 < r->end && (isdigit(r->p[1]) || r->p[1] == '-')) {
            Node *dot = node(N_SYM);
            dot->s = br_strdup(".");
            push(list, dot, &cap);
            r->p++;
            continue;
        }
        Node *k = *r->p == '(' ? read_list(r) : *r->p == '"' ? read_string(r) : read_atom(r);
        push(list, k, &cap);
    }
    if (list->n && list->k[0]->t == N_SYM) list->op = list->k[0]->op;
    return list;
}

Node *sx_parse(const char *text, size_t len) {
    Reader r = {(const u8 *)text, (const u8 *)text + len};
    Node *top = node(N_LIST);
    int cap = 0;
    for (;;) {
        skip_space(&r);
        if (r.p >= r.end) break;
        if (*r.p == ')') {
            r.p++;
            continue;
        }
        Node *k = *r.p == '(' ? read_list(&r) : *r.p == '"' ? read_string(&r) : read_atom(&r);
        push(top, k, &cap);
    }
    return top;
}

void sx_free(Node *n) {
    if (!n) return;
    for (int i = 0; i < n->n; i++) sx_free(n->k[i]);
    free(n->k);
    free(n->s);
    free(n);
}

bool sx_sym(const Node *n, const char *name) { return n && n->t == N_SYM && !strcmp(n->s, name); }

Node *sx_find(Node *list, const char *key) {
    if (!list || list->t != N_LIST) return NULL;
    for (int i = 0; i < list->n; i++) {
        Node *k = list->k[i];
        if (k->t == N_LIST && k->n && sx_sym(k->k[0], key)) return k;
    }
    return NULL;
}

Node *sx_assoc(Node *list, i32 key) {
    if (!list || list->t != N_LIST) return NULL;
    for (int i = 0; i < list->n; i++) {
        Node *k = list->k[i];
        if (k->t == N_LIST && k->n && k->k[0]->t == N_INT && k->k[0]->i == key) return k;
    }
    return NULL;
}

Node *sx_cdr_value(Node *pair) {
    if (!pair || pair->t != N_LIST || pair->n < 2) return NULL;
    if (pair->n >= 3 && sx_sym(pair->k[1], ".")) return pair->k[2];
    return pair->k[1];
}

int sx_int(const Node *n, int dflt) { return n && n->t == N_INT ? n->i : dflt; }

const char *sx_str(const Node *n) { return n && (n->t == N_STR || n->t == N_SYM) ? n->s : NULL; }

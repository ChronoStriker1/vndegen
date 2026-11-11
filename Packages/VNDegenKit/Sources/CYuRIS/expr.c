// Expressions. An argument is a postfix sequence of tokens, each an opcode byte, a 16-bit payload length and the
// payload. Numbers: B, W, I, L (8 to 64-bit) and F (double). M is a string literal in quotes. H is a variable (a
// sigil byte and a 16-bit id); V starts an array element whose subscripts follow, separated by ',' and closed by ')';
// v names a whole array. Operators: + - * / %, = ! < > S (<=) Z (>=), & | (logical), A O ^ (bitwise), R (negate),
// s and i (to string and to integer). The opcodes were read from scripts compiled with the SDK compiler.
#include "yr.h"
#include "vm.h"
#include <math.h>

enum { STACK = 128 };

typedef struct Stack {
    YVal v[STACK];
    int n;
    int marks[16];    // stack heights where open array references began
    u32 markVid[16];
    int nmarks;
} Stack;

static void push(Stack *s, YVal v) {
    if (s->n < STACK) s->v[s->n++] = v;
    else yv_free(&v);
}

static YVal pop(Stack *s) {
    if (s->n <= 0) return yv_int(0);
    return s->v[--s->n];
}

// String literals are quoted with ", ' or `.
static char *strip_quotes(const u8 *p, u32 n) {
    if (n >= 2 && (p[0] == '"' || p[0] == '\'' || p[0] == '`') && p[n - 1] == p[0])
        return yr_strndup((const char *)p + 1, n - 2);
    return yr_strndup((const char *)p, n);
}

static YVal var_value(YR *y, YTask *t, u32 vid, int n, const i64 *sub) {
    if (vid < 1000 && yr_sys_dynamic(&y->sys[vid])) return yr_sys_get(y, t, &y->sys[vid], n, sub);
    YVar *v = yr_var(y, vid);
    if (!v) return yv_int(0);
    return yr_var_get(v, yr_var_index(v, n, sub));
}

static YVal binary(u8 op, YVal a, YVal b) {
    YVal r;
    if (op == '+' && (a.t == YT_STR || b.t == YT_STR)) {
        char *x = yv_as_str(&a), *z = yv_as_str(&b);
        size_t nx = strlen(x), nz = strlen(z);
        char *c = yr_alloc(nx + nz + 1);
        memcpy(c, x, nx);
        memcpy(c + nx, z, nz);
        free(x);
        free(z);
        r = yv_str_own(c);
    } else if ((op == '=' || op == '!' || op == '<' || op == '>' || op == 'S' || op == 'Z') &&
               (a.t == YT_STR || b.t == YT_STR)) {
        char *x = yv_as_str(&a), *z = yv_as_str(&b);
        int c = strcmp(x, z);
        free(x);
        free(z);
        bool v = op == '=' ? c == 0 : op == '!' ? c != 0 : op == '<' ? c < 0 : op == '>' ? c > 0 : op == 'S' ? c <= 0 : c >= 0;
        r = yv_int(v);
    } else if (a.t == YT_FLT || b.t == YT_FLT) {
        double x = yv_as_flt(&a), z = yv_as_flt(&b);
        switch (op) {
        case '+': r = yv_flt(x + z); break;
        case '-': r = yv_flt(x - z); break;
        case '*': r = yv_flt(x * z); break;
        case '/': r = yv_flt(z != 0 ? x / z : 0); break;
        case '%': r = yv_flt(z != 0 ? fmod(x, z) : 0); break;
        case '=': r = yv_int(x == z); break;
        case '!': r = yv_int(x != z); break;
        case '<': r = yv_int(x < z); break;
        case '>': r = yv_int(x > z); break;
        case 'S': r = yv_int(x <= z); break;
        case 'Z': r = yv_int(x >= z); break;
        case '&': r = yv_int(x != 0 && z != 0); break;
        case '|': r = yv_int(x != 0 || z != 0); break;
        case 'A': r = yv_int((i64)x & (i64)z); break;
        case 'O': r = yv_int((i64)x | (i64)z); break;
        case '^': r = yv_int((i64)x ^ (i64)z); break;
        default: r = yv_int(0);
        }
    } else {
        i64 x = yv_as_int(&a), z = yv_as_int(&b);
        switch (op) {
        case '+': r = yv_int((i64)((u64)x + (u64)z)); break;
        case '-': r = yv_int((i64)((u64)x - (u64)z)); break;
        case '*': r = yv_int((i64)((u64)x * (u64)z)); break;
        case '/': r = yv_int(z && !(x == INT64_MIN && z == -1) ? x / z : 0); break;
        case '%': r = yv_int(z && !(x == INT64_MIN && z == -1) ? x % z : 0); break;
        case '=': r = yv_int(x == z); break;
        case '!': r = yv_int(x != z); break;
        case '<': r = yv_int(x < z); break;
        case '>': r = yv_int(x > z); break;
        case 'S': r = yv_int(x <= z); break;
        case 'Z': r = yv_int(x >= z); break;
        case '&': r = yv_int(x && z); break;
        case '|': r = yv_int(x || z); break;
        case 'A': r = yv_int(x & z); break;
        case 'O': r = yv_int(x | z); break;
        case '^': r = yv_int(x ^ z); break;
        default: r = yv_int(0);
        }
    }
    yv_free(&a);
    yv_free(&b);
    return r;
}

// Runs tokens, leaving their results on the stack.
static void run(YR *y, YTask *t, const u8 *p, u32 n, Stack *s) {
    u32 o = 0;
    while (o + 3 <= n) {
        u8 op = p[o];
        u32 len = yr_le16(p + o + 1);
        const u8 *d = p + o + 3;
        if (o + 3 + len > n) break;
        o += 3 + len;
        switch (op) {
        case 'B': push(s, yv_int(len >= 1 ? (int8_t)d[0] : 0)); break;
        case 'W': push(s, yv_int(len >= 2 ? (int16_t)yr_le16(d) : 0)); break;
        case 'I': push(s, yv_int(len >= 4 ? (int32_t)yr_le32(d) : 0)); break;
        case 'L': push(s, yv_int(len >= 8 ? (i64)yr_le64(d) : 0)); break;
        case 'F': {
            double f = 0;
            if (len >= 8) { u64 b = yr_le64(d); memcpy(&f, &b, 8); }
            push(s, yv_flt(f));
            break;
        }
        case 'M': push(s, yv_str_own(strip_quotes(d, len))); break;
        case 'H':
            if (len >= 3) push(s, var_value(y, t, yr_le16(d + 1), 0, NULL));
            break;
        case 'V':
        case 'v':
            if (len >= 3 && s->nmarks < 16) {
                s->marks[s->nmarks] = s->n;
                s->markVid[s->nmarks++] = yr_le16(d + 1);
            }
            break;
        case ',': break;
        case ')':
            if (s->nmarks) {
                int base = s->marks[--s->nmarks];
                u32 vid = s->markVid[s->nmarks];
                i64 sub[8];
                int k = 0;
                for (int i = base; i < s->n; i++) {
                    if (k < 8) sub[k++] = yv_as_int(&s->v[i]);
                    yv_free(&s->v[i]);
                }
                s->n = base;
                push(s, var_value(y, t, vid, k, sub));
            }
            break;
        case 'R': {
            YVal a = pop(s);
            push(s, a.t == YT_FLT ? yv_flt(-a.f) : yv_int(-yv_as_int(&a)));
            yv_free(&a);
            break;
        }
        case 's': {
            YVal a = pop(s);
            push(s, yv_str_own(yv_as_str(&a)));
            yv_free(&a);
            break;
        }
        case 'i': {
            YVal a = pop(s);
            push(s, a.t == YT_FLT ? yv_int((i64)a.f) : yv_int(yv_as_int(&a)));
            yv_free(&a);
            break;
        }
        case '+': case '-': case '*': case '/': case '%': case '=': case '!': case '<': case '>': case 'S': case 'Z':
        case '&': case '|': case 'A': case 'O': case '^': {
            YVal b = pop(s), a = pop(s);
            push(s, binary(op, a, b));
            break;
        }
        default:
            if (y->log) yr_log(y, "expr: unknown token %02x\n", op);
            break;
        }
    }
}

YVal yr_eval(YR *y, YTask *t, const u8 *p, u32 n) {
    Stack s;
    s.n = 0;
    s.nmarks = 0;
    run(y, t, p, n, &s);
    if (!s.n) return yv_int(0);
    YVal r = s.v[s.n - 1];
    for (int i = 0; i < s.n - 1; i++) yv_free(&s.v[i]);
    return r;
}

bool yr_lvalue(YR *y, YTask *t, const u8 *p, u32 n, YRef *out) {
    memset(out, 0, sizeof *out);
    out->index = 0;
    if (n < 6) return false;
    u8 op = p[0];
    u32 vid = yr_le16(p + 4);
    out->vid = vid;
    if (vid < 1000 && yr_sys_dynamic(&y->sys[vid])) out->sys = y->sys[vid];
    else if (!(out->var = yr_var(y, vid))) return false;
    if (op == 'H') return true;
    if (op != 'V' && op != 'v') return false;
    // Subscripts: everything between the reference and its closing ')'.
    if (n < 6 + 4 || p[n - 4] != ')') {
        out->index = -1;
        return true;
    }
    Stack s;
    s.n = 0;
    s.nmarks = 0;
    run(y, t, p + 6, n - 6 - 4, &s);
    out->nsub = s.n > 8 ? 8 : s.n;
    for (int i = 0; i < s.n; i++) {
        if (i < 8) out->sub[i] = yv_as_int(&s.v[i]);
        yv_free(&s.v[i]);
    }
    if (op == 'v' || out->nsub == 0) out->index = -1;
    else if (out->var) out->index = yr_var_index(out->var, out->nsub, out->sub);
    return true;
}

YVal yr_ref_get(YR *y, YTask *t, const YRef *r) {
    if (!r->var) return r->vid < 1000 ? yr_sys_get(y, t, &r->sys, r->nsub, r->sub) : yv_int(0);
    return yr_var_get(r->var, r->index < 0 ? 0 : r->index);
}

void yr_ref_set(YR *y, YTask *t, const YRef *r, const YVal *v) {
    if ((long)r->vid == y->watchVid) {
        char *str = yv_as_str(v);
        yr_log(y, "watch: var %u[%ld] = \"%s\" (task %s script %u ip %u)\n", r->vid, r->index, str, t ? t->id : "-",
               t ? t->script : 0, t ? t->ip : 0);
        free(str);
    }
    if (!r->var) { if (r->vid < 1000) yr_sys_set(y, t, &r->sys, r->nsub, r->sub, v); return; }
    if (!r->var->count) yr_var_declare(y, r->var, r->var->init.t ? r->var->init.t : v->t, 0, NULL, NULL);
    if (r->index < 0) {   // a whole array takes the value in every element
        for (u32 i = 0; i < r->var->count; i++) yr_var_set(r->var, (long)i, v);
        return;
    }
    yr_var_set(r->var, r->index, v);
}

u8 yr_ref_type(YR *y, const YRef *r) {
    if (r->vid < 1000) {
        YVar *v = yr_var(y, r->vid);
        return v && v->t ? v->t : YT_INT;
    }
    return r->var ? r->var->t : YT_INT;
}

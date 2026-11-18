#include "eagls.h"

#include <ctype.h>

#define HEADER 3600

static const char scKey[] = "EAGLS_SYSTEM";

u8 *script_decrypt(const u8 *data, size_t size, size_t *textSize) {
    if (size < HEADER + 2) return NULL;
    size_t len = size - HEADER - 2;
    u8 *t = eg_alloc(len + 1);
    memcpy(t, data + HEADER, len);
    u32 seed;
    eg_rand_seed(&seed, (i32)(int8_t)data[size - 1]);
    for (size_t i = 0; i < len; i += 2) t[i] ^= (u8)scKey[eg_rand(&seed) % 12];
    t[len] = 0;
    *textSize = len;
    return t;
}

typedef struct Parser {
    Script *sc;
    const u8 *p, *end;
    int line;
    struct Block {
        int kind;
        int ins;
        int top;
        int *breaks;
        int breakCount;
        int *chain;
        int chainCount;
    } blocks[64];
    int depth;
    int *chain;
    int chainCount;
    bool chainOpen;
} Parser;

enum { B_IF, B_LOOP };

static Ins *emit(Parser *ps, int op) {
    Script *s = ps->sc;
    if (s->count == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 256;
        s->ins = eg_realloc(s->ins, sizeof(Ins) * (size_t)s->cap);
    }
    Ins *in = &s->ins[s->count++];
    memset(in, 0, sizeof *in);
    in->op = (u8)op;
    in->line = ps->line;
    in->target = -1;
    return in;
}

static void skip_ws(Parser *ps) {
    while (ps->p < ps->end && (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\r' || *ps->p == '\n' || *ps->p == 0)) {
        if (*ps->p == '\n') ps->line++;
        ps->p++;
    }
}

static void skip_inline(Parser *ps) {
    while (ps->p < ps->end && (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\r')) ps->p++;
}

static bool name_char(u8 c) { return isalnum(c) || c == '_'; }

static char *take(const u8 *a, const u8 *b) {
    char *s = eg_alloc((size_t)(b - a) + 1);
    memcpy(s, a, (size_t)(b - a));
    return s;
}

static bool parse_arg(Parser *ps, Arg *a, const char *stops);

static bool parse_var(Parser *ps, Arg *a) {
    const u8 *s = ps->p;
    ps->p++;
    while (ps->p < ps->end && name_char(*ps->p)) ps->p++;
    a->kind = A_VAR;
    a->s = take(s, ps->p);
    if (ps->p < ps->end && *ps->p == '[') {
        ps->p++;
        a->index = eg_alloc(sizeof(Arg));
        parse_arg(ps, a->index, "]");
        if (ps->p < ps->end && *ps->p == ']') ps->p++;
    }
    return true;
}

static bool parse_arg(Parser *ps, Arg *a, const char *stops) {
    memset(a, 0, sizeof *a);
    skip_inline(ps);
    if (ps->p >= ps->end) return false;
    u8 c = *ps->p;
    if (c == '"') {
        const u8 *s = ++ps->p;
        while (ps->p < ps->end && *ps->p != '"') ps->p += eg_sjis_len(ps->p);
        a->kind = A_STR;
        a->s = take(s, ps->p);
        if (ps->p < ps->end) ps->p++;
        skip_inline(ps);
        return true;
    }
    if (c == '_' || c == ':' || c == '@') {
        parse_var(ps, a);
        skip_inline(ps);
        return true;
    }
    if (isdigit(c) || (c == '-' && ps->p + 1 < ps->end && isdigit(ps->p[1]))) {
        const u8 *s = ps->p;
        ps->p++;
        while (ps->p < ps->end && isdigit(*ps->p)) ps->p++;
        if (ps->p < ps->end && !strchr(stops, *ps->p) && *ps->p != ' ' && *ps->p != '\t') {
            while (ps->p < ps->end && !strchr(stops, *ps->p) && *ps->p != '\n') ps->p += eg_sjis_len(ps->p);
            a->kind = A_WORD;
            a->s = take(s, ps->p);
            return true;
        }
        char *t = take(s, ps->p);
        a->kind = A_NUM;
        a->num = (i32)strtol(t, NULL, 10);
        free(t);
        skip_inline(ps);
        return true;
    }
    const u8 *s = ps->p;
    while (ps->p < ps->end && !strchr(stops, *ps->p) && *ps->p != '\n') ps->p += eg_sjis_len(ps->p);
    const u8 *e = ps->p;
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) e--;
    a->kind = A_WORD;
    a->s = take(s, e);
    if (e - s == 1 && strchr("+-*/%=!<>&|", *s)) {
        a->kind = A_OP;
        a->op = (char)*s;
    } else if (!strcmp(a->s, "TRUE") || !strcmp(a->s, "FALSE")) {
        a->kind = A_NUM;
        a->num = a->s[0] == 'T';
    }
    return true;
}

static int parse_args(Parser *ps, Arg **out) {
    int n = 0, cap = 0;
    Arg *args = NULL;
    skip_inline(ps);
    if (ps->p < ps->end && *ps->p == ')') {
        ps->p++;
        *out = NULL;
        return 0;
    }
    while (ps->p < ps->end) {
        if (n == cap) {
            cap = cap ? cap * 2 : 8;
            args = eg_realloc(args, sizeof(Arg) * (size_t)cap);
        }
        parse_arg(ps, &args[n], ",)");
        n++;
        if (ps->p < ps->end && *ps->p == ',') {
            ps->p++;
            skip_inline(ps);
            if (ps->p < ps->end && *ps->p == ')') {
                ps->p++;
                break;
            }
            continue;
        }
        if (ps->p < ps->end && *ps->p == ')') ps->p++;
        break;
    }
    *out = args;
    return n;
}

static int parse_rpn(Parser *ps, Arg **out) {
    int n = 0, cap = 0;
    Arg *args = NULL;
    while (ps->p < ps->end) {
        skip_inline(ps);
        if (ps->p >= ps->end) break;
        u8 c = *ps->p;
        bool tok = c == '_' || c == ':' || c == '@' || c == '"' || isdigit(c) || strchr("+-*/%=!<>&|", c);
        if (!tok) break;
        if (isdigit(c)) {
            const u8 *q = ps->p;
            while (q < ps->end && isdigit(*q)) q++;
            if (q < ps->end && *q == '(') break;
        }
        const u8 *save = ps->p;
        Arg a;
        parse_arg(ps, &a, ",)\n");
        if (ps->p >= ps->end || *ps->p != ',') {
            ps->p = save;
            free(a.s);
            break;
        }
        ps->p++;
        if (n == cap) {
            cap = cap ? cap * 2 : 8;
            args = eg_realloc(args, sizeof(Arg) * (size_t)cap);
        }
        args[n++] = a;
    }
    *out = args;
    return n;
}

static void close_chain(Parser *ps) {
    if (!ps->chainOpen) return;
    for (int i = 0; i < ps->chainCount; i++) ps->sc->ins[ps->chain[i]].target = ps->sc->count;
    free(ps->chain);
    ps->chain = NULL;
    ps->chainCount = 0;
    ps->chainOpen = false;
}

static bool parse_block_head(Parser *ps, int kind, int cmd) {
    while (ps->p < ps->end && *ps->p != ',') ps->p++;
    if (ps->p < ps->end) ps->p++;
    while (ps->p < ps->end && isdigit(*ps->p)) ps->p++;
    Arg *cond = NULL;
    int cn = 0;
    if (cmd != 121) {
        if (ps->p < ps->end && *ps->p == '(') ps->p++;
        cn = parse_rpn(ps, &cond);
        skip_inline(ps);
        if (ps->p < ps->end && *ps->p == ')') ps->p++;
    }
    skip_inline(ps);
    if (ps->p < ps->end && *ps->p == '{') ps->p++;
    if (ps->depth >= 64) return false;
    struct Block *b = &ps->blocks[ps->depth++];
    memset(b, 0, sizeof *b);
    b->kind = kind;
    if (kind == B_LOOP) {
        b->top = ps->sc->count;
        Ins *in = emit(ps, I_LOOP);
        in->args = cond;
        in->argc = cn;
        b->ins = ps->sc->count - 1;
    } else if (cmd != 121) {
        Ins *in = emit(ps, I_IF);
        in->args = cond;
        in->argc = cn;
        b->ins = ps->sc->count - 1;
    } else {
        b->ins = -1;
    }
    return true;
}

static void parse_block_end(Parser *ps) {
    ps->p++;
    while (ps->p < ps->end && isdigit(*ps->p)) ps->p++;
    if (ps->p < ps->end && *ps->p == ';') ps->p++;
    if (!ps->depth) return;
    struct Block b = ps->blocks[--ps->depth];
    if (b.kind == B_LOOP) {
        Ins *j = emit(ps, I_JMP);
        j->target = b.top;
        ps->sc->ins[b.ins].target = ps->sc->count;
        for (int i = 0; i < b.breakCount; i++) ps->sc->ins[b.breaks[i]].target = ps->sc->count;
        free(b.breaks);
        return;
    }
    emit(ps, I_JMP);
    b.chain = eg_realloc(b.chain, sizeof(int) * (size_t)(b.chainCount + 1));
    b.chain[b.chainCount++] = ps->sc->count - 1;
    if (b.ins >= 0) ps->sc->ins[b.ins].target = ps->sc->count;
    ps->chain = b.chain;
    ps->chainCount = b.chainCount;
    ps->chainOpen = true;
}

static struct Block *innermost_loop(Parser *ps) {
    for (int i = ps->depth - 1; i >= 0; i--)
        if (ps->blocks[i].kind == B_LOOP) return &ps->blocks[i];
    return NULL;
}

static void parse_statement(Parser *ps) {
    u8 c = *ps->p;
    if (c == '}') {
        close_chain(ps);
        parse_block_end(ps);
        return;
    }
    if (isdigit(c)) {
        const u8 *q = ps->p;
        int n = 0;
        while (q < ps->end && isdigit(*q)) n = n * 10 + (*q++ - '0');
        if (q < ps->end && *q == ',' && n >= 120 && n <= 123) {
            if (n == 121 || n == 122) {
                int *chain = ps->chainOpen ? ps->chain : NULL;
                int count = ps->chainOpen ? ps->chainCount : 0;
                ps->chain = NULL;
                ps->chainCount = 0;
                ps->chainOpen = false;
                parse_block_head(ps, B_IF, n);
                ps->blocks[ps->depth - 1].chain = chain;
                ps->blocks[ps->depth - 1].chainCount = count;
                return;
            }
            close_chain(ps);
            parse_block_head(ps, n == 123 ? B_LOOP : B_IF, n);
            return;
        }
        close_chain(ps);
        if (q < ps->end && *q == '(') {
            ps->p = q + 1;
            Arg *args;
            int argc = parse_args(ps, &args);
            if (n == 124 || n == 125) {
                struct Block *lp = innermost_loop(ps);
                if (lp) {
                    Ins *j = emit(ps, I_JMP);
                    if (n == 125) j->target = lp->top;
                    else {
                        lp->breaks = eg_realloc(lp->breaks, sizeof(int) * (size_t)(lp->breakCount + 1));
                        lp->breaks[lp->breakCount++] = ps->sc->count - 1;
                    }
                    free(args);
                    return;
                }
            }
            Ins *in = emit(ps, I_CALL);
            in->n = n;
            in->args = args;
            in->argc = argc;
            return;
        }
        ps->p = q;
        return;
    }
    close_chain(ps);
    if (c == '$') {
        const u8 *s = ++ps->p;
        while (ps->p < ps->end && *ps->p != '\n' && *ps->p != '\r') ps->p++;
        Script *sc = ps->sc;
        sc->labels = eg_realloc(sc->labels, sizeof(Label) * (size_t)(sc->labelCount + 1));
        sc->labels[sc->labelCount].name = take(s, ps->p);
        sc->labels[sc->labelCount].ins = sc->count;
        sc->labelCount++;
        return;
    }
    if (c == '&') {
        ps->p++;
        int id = 0;
        while (ps->p < ps->end && isdigit(*ps->p)) id = id * 10 + (*ps->p++ - '0');
        skip_inline(ps);
        if (ps->p < ps->end && *ps->p == '"') ps->p++;
        const u8 *s = ps->p;
        while (ps->p < ps->end && *ps->p != '"' && *ps->p != '\n') ps->p += eg_sjis_len(ps->p);
        Ins *in = emit(ps, I_TEXT);
        in->n = id;
        in->s = take(s, ps->p);
        if (ps->p < ps->end && *ps->p == '"') ps->p++;
        return;
    }
    if (c == '#') {
        const u8 *s = ++ps->p;
        while (ps->p < ps->end && *ps->p != '\n' && *ps->p != '\r') ps->p++;
        const u8 *e = ps->p;
        const u8 *eq = s;
        while (eq < e && *eq != '=') eq += eg_sjis_len(eq);
        const u8 *ne = eq;
        for (;;) {
            if (ne > s && (ne[-1] == ' ' || ne[-1] == '\t')) ne--;
            else if (ne - s >= 2 && ne[-2] == 0x81 && ne[-1] == 0x40) ne -= 2;
            else break;
        }
        Ins *in = emit(ps, I_NAME);
        in->s = take(s, ne);
        if (eq < e) {
            const u8 *v = eq + 1, *ve = e;
            while (ve > v && (ve[-1] == ' ' || ve[-1] == '\t')) ve--;
            in->s2 = take(v, ve);
        }
        return;
    }
    if (c == '_' || c == ':' || c == '@') {
        Arg v;
        memset(&v, 0, sizeof v);
        parse_var(ps, &v);
        skip_inline(ps);
        if (ps->p < ps->end && *ps->p == '=') {
            ps->p++;
            Ins *in = emit(ps, I_ASSIGN);
            in->var = v;
            in->argc = parse_rpn(ps, &in->args);
            return;
        }
        free(v.s);
        return;
    }
    ps->p += eg_sjis_len(ps->p);
}

Script *script_parse(const char *name, const u8 *data, size_t size) {
    Script *s = eg_alloc(sizeof *s);
    s->name = eg_strdup(name);
    Parser ps;
    memset(&ps, 0, sizeof ps);
    ps.sc = s;
    ps.p = data;
    ps.end = data + size;
    ps.line = 1;
    for (;;) {
        skip_ws(&ps);
        if (ps.p >= ps.end) break;
        const u8 *before = ps.p;
        parse_statement(&ps);
        if (ps.p == before) ps.p++;
    }
    close_chain(&ps);
    emit(&ps, I_NOP);
    return s;
}

void script_free(Script *s) {
    if (!s) return;
    for (int i = 0; i < s->count; i++) {
        Ins *in = &s->ins[i];
        for (int k = 0; k < in->argc; k++) {
            free(in->args[k].s);
            if (in->args[k].index) {
                free(in->args[k].index->s);
                free(in->args[k].index);
            }
        }
        free(in->args);
        free(in->var.s);
        if (in->var.index) {
            free(in->var.index->s);
            free(in->var.index);
        }
        free(in->s);
        free(in->s2);
    }
    for (int i = 0; i < s->labelCount; i++) free(s->labels[i].name);
    free(s->labels);
    free(s->ins);
    free(s->name);
    free(s);
}

int script_label(const Script *s, const char *label) {
    for (int i = 0; i < s->labelCount; i++)
        if (eg_ieq(s->labels[i].name, label)) return s->labels[i].ins;
    return -1;
}

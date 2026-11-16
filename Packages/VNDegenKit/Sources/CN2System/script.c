// NSS source to bytecode. The lexer reads the Shift-JIS bytes as stored, so every instruction's source offset is a
// byte offset into the file the PC engine keeps in its saves. Names and strings become UTF-8 constants.
//
// The language, from Saya's scripts and NitroSharp's parser (MIT): `function`, `chapter` and `scene` declarations,
// `#include`, C-like statements, `select` with `case` labels naming buttons, `call_chapter` and `call_scene`, and
// `<PRE @box>[name] ... </PRE>` text blocks. Text outside declarations ("$Revision: $", ".//" lines) is skipped.
#include "n2.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum {
    T_EOF, T_IDENT, T_STRING, T_NUMBER, T_PUNCT, T_PRE,
};

typedef struct Tok {
    int kind;
    int pos, end;   // byte offsets
    char *text;     // UTF-8: identifier with its sigil, string contents, or punctuation
    double num;
    bool quoted;
    bool lineStart;
} Tok;

typedef struct Loop {
    int start;
    int *breaks;
    int breakCount;
    struct Loop *outer;
    bool isSelect;
} Loop;

typedef struct P {
    const u8 *src;
    int len;
    int pos;
    Tok tok;
    Script *script;
    Chunk *chunk;
    Loop *loop;
    char *err;
    size_t errSize;
    bool failed;
    int line;
} P;

static bool is_lead(u8 c) { return (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc); }

static int line_of(P *p, int pos) {
    int line = 1;
    for (int i = 0; i < pos && i < p->len; i++)
        if (p->src[i] == '\n') line++;
    return line;
}

static void fail(P *p, const char *msg) {
    if (p->failed) return;
    p->failed = true;
    snprintf(p->err, p->errSize, "%s line %d: %s", p->script->path, line_of(p, p->tok.pos), msg);
}

static char *utf8_of(P *p, int a, int b) {
    return n2_sjis_to_utf8(p->src + a, (size_t)(b - a), NULL);
}

static void skip_trivia(P *p) {
    for (;;) {
        while (p->pos < p->len && (p->src[p->pos] == ' ' || p->src[p->pos] == '\t' || p->src[p->pos] == '\r' ||
                                   p->src[p->pos] == '\n'))
            p->pos++;
        if (p->pos + 1 < p->len && p->src[p->pos] == '/' && p->src[p->pos + 1] == '/') {
            while (p->pos < p->len && p->src[p->pos] != '\n') p->pos += is_lead(p->src[p->pos]) ? 2 : 1;
            continue;
        }
        if (p->pos + 1 < p->len && p->src[p->pos] == '/' && p->src[p->pos + 1] == '*') {
            p->pos += 2;
            while (p->pos + 1 < p->len && !(p->src[p->pos] == '*' && p->src[p->pos + 1] == '/'))
                p->pos += is_lead(p->src[p->pos]) ? 2 : 1;
            p->pos = p->pos + 2 < p->len ? p->pos + 2 : p->len;
            continue;
        }
        break;
    }
}

static bool ident_char(const u8 *s, int i, int len) {
    u8 c = s[i];
    if (c >= 0x80) return true;
    return isalnum(c) || c == '_';
}

static bool at_line_start(P *p, int pos) {
    for (int i = pos - 1; i >= 0; i--) {
        u8 c = p->src[i];
        if (c == '\n') return true;
        if (c != ' ' && c != '\t' && c != '\r') return false;
    }
    return true;
}

static void next(P *p) {
    free(p->tok.text);
    memset(&p->tok, 0, sizeof p->tok);
    skip_trivia(p);
    Tok *t = &p->tok;
    t->pos = p->pos;
    t->lineStart = at_line_start(p, p->pos);
    if (p->pos >= p->len) {
        t->kind = T_EOF;
        t->end = p->pos;
        return;
    }
    const u8 *s = p->src;
    u8 c = s[p->pos];
    if (c == '"') {
        int a = ++p->pos;
        while (p->pos < p->len && s[p->pos] != '"' && s[p->pos] != '\n') p->pos += is_lead(s[p->pos]) ? 2 : 1;
        t->kind = T_STRING;
        t->quoted = true;
        t->text = utf8_of(p, a, p->pos);
        if (p->pos < p->len && s[p->pos] == '"') p->pos++;
    } else if (c == '<' && p->pos + 4 < p->len && strncasecmp((const char *)s + p->pos, "<PRE", 4) == 0 &&
               (s[p->pos + 4] == ' ' || s[p->pos + 4] == '\t' || s[p->pos + 4] == '>')) {
        t->kind = T_PRE;
        p->pos += 4;
    } else if (isdigit(c) || (c == '.' && p->pos + 1 < p->len && isdigit(s[p->pos + 1]))) {
        int a = p->pos;
        if (c == '0' && p->pos + 1 < p->len && (s[p->pos + 1] == 'x' || s[p->pos + 1] == 'X')) {
            p->pos += 2;
            while (p->pos < p->len && isxdigit(s[p->pos])) p->pos++;
            char *h = n2_strndup((const char *)s + a + 2, (size_t)(p->pos - a - 2));
            t->num = (double)strtoll(h, NULL, 16);
            free(h);
            t->kind = T_NUMBER;
        } else {
            while (p->pos < p->len && (isdigit(s[p->pos]) || s[p->pos] == '.')) p->pos++;
            if (p->pos < p->len && ident_char(s, p->pos, p->len)) {
                // An identifier that starts with digits.
                while (p->pos < p->len && ident_char(s, p->pos, p->len)) p->pos += is_lead(s[p->pos]) ? 2 : 1;
                t->kind = T_IDENT;
                t->text = utf8_of(p, a, p->pos);
            } else {
                char *n = n2_strndup((const char *)s + a, (size_t)(p->pos - a));
                t->num = strtod(n, NULL);
                free(n);
                t->kind = T_NUMBER;
            }
        }
    } else if (c == '$' || c == '#' || c >= 0x80 || isalpha(c) || c == '_') {
        int a = p->pos;
        if (c == '$' || c == '#') p->pos++;
        if (c == '#' && p->len - p->pos >= 7 && strncmp((const char *)s + p->pos, "include", 7) == 0) {
            p->pos += 7;
            t->kind = T_IDENT;
            t->text = n2_strdup("#include");
        } else {
            while (p->pos < p->len && ident_char(s, p->pos, p->len)) p->pos += is_lead(s[p->pos]) ? 2 : 1;
            if (p->pos > p->len) p->pos = p->len;
            t->kind = T_IDENT;
            t->text = utf8_of(p, a, p->pos);
            if (p->pos == a + 1 && (c == '$' || c == '#')) t->kind = T_PUNCT;
        }
    } else {
        static const char *two[] = {"==", "!=", "<=", ">=", "&&", "||", "++", "--", "+=", "-=", "*=", "/=", "%=",
                                    "->", NULL};
        t->kind = T_PUNCT;
        for (int i = 0; two[i]; i++) {
            if (p->pos + 1 < p->len && s[p->pos] == (u8)two[i][0] && s[p->pos + 1] == (u8)two[i][1]) {
                t->text = n2_strdup(two[i]);
                p->pos += 2;
                break;
            }
        }
        if (!t->text) {
            int n = is_lead(c) ? 2 : 1;
            t->text = n2_strndup((const char *)s + p->pos, (size_t)n);
            p->pos += n;
        }
    }
    t->end = p->pos;
}

static bool is(P *p, const char *punct) {
    return (p->tok.kind == T_PUNCT || (p->tok.kind == T_IDENT && !p->tok.quoted)) && p->tok.text &&
           strcmp(p->tok.text, punct) == 0;
}

static bool is_kw(P *p, const char *kw) {
    return p->tok.kind == T_IDENT && n2_streq_ci(p->tok.text, kw);
}

static bool accept(P *p, const char *punct) {
    if (!is(p, punct)) return false;
    next(p);
    return true;
}

static void expect(P *p, const char *punct) {
    if (!accept(p, punct)) {
        char m[96];
        snprintf(m, sizeof m, "expected '%s'", punct);
        fail(p, m);
    }
}

// ---- emitting ----

static int emit(P *p, int op, int arg) {
    Chunk *c = p->chunk;
    if (c->codeLen == c->codeCap) {
        c->codeCap = c->codeCap ? c->codeCap * 2 : 64;
        c->code = n2_grow(c->code, sizeof(u32) * (size_t)c->codeCap);
        c->srcpos = n2_grow(c->srcpos, sizeof(i32) * (size_t)c->codeCap);
    }
    c->code[c->codeLen] = (u32)op | (u32)arg << 8;
    c->srcpos[c->codeLen] = p->tok.pos;
    return c->codeLen++;
}

static void patch(P *p, int at, int target) {
    p->chunk->code[at] = (p->chunk->code[at] & 0xff) | (u32)target << 8;
}

static int add_const(Chunk *c, Value v) {
    for (int i = 0; i < c->constCount; i++) {
        Value *k = &c->consts[i];
        if (k->t == v.t && k->delta == v.delta &&
            ((v.t == V_STR && strcmp(k->s->s, v.s->s) == 0) || (v.t != V_STR && k->n == v.n))) {
            v_free(&v);
            return i;
        }
    }
    if (c->constCount == c->constCap) {
        c->constCap = c->constCap ? c->constCap * 2 : 16;
        c->consts = n2_grow(c->consts, sizeof(Value) * (size_t)c->constCap);
    }
    c->consts[c->constCount] = v;
    return c->constCount++;
}

static int str_const(P *p, const char *s) { return add_const(p->chunk, v_str(s)); }

static int param_index(P *p, const char *name) {
    Chunk *c = p->chunk;
    for (int i = 0; i < c->paramCount; i++)
        if (strcmp(c->params[i], name) == 0) return i;
    return -1;
}

static Chunk *new_chunk(P *p, int kind, const char *name) {
    Chunk *c = n2_alloc(sizeof *c);
    c->kind = kind;
    c->name = n2_strdup(name);
    c->script = p->script;
    Script *s = p->script;
    s->chunks = n2_grow(s->chunks, sizeof(Chunk *) * (size_t)(s->chunkCount + 1));
    s->chunks[s->chunkCount++] = c;
    return c;
}

// ---- expressions ----

static void expr(P *p);

// An lvalue the caller assigns to: a parameter or a variable, maybe indexed.
typedef struct LVal {
    int kind;  // 0 none, 1 param, 2 var, 3 indexed var
    int index;  // param index or name constant
} LVal;

static void load_lval(P *p, LVal lv) {
    if (lv.kind == 1) emit(p, OP_GETPARAM, lv.index);
    else if (lv.kind == 2) emit(p, OP_GETVAR, lv.index);
}

// Built-ins whose arguments name variables rather than pass values, as a bit mask of argument positions.
static u32 ref_args(const char *fn) {
    if (n2_streq_ci(fn, "Position")) return 6;
    if (n2_streq_ci(fn, "CursorPosition") || n2_streq_ci(fn, "DateTime")) return 0x3f;
    if (n2_streq_ci(fn, "Array") || n2_streq_ci(fn, "Count")) return 1;
    return 0;
}

static void primary(P *p) {
    Tok *t = &p->tok;
    if (t->kind == T_NUMBER) {
        emit(p, OP_CONST, add_const(p->chunk, v_num(t->num)));
        next(p);
        return;
    }
    if (accept(p, "(")) {
        expr(p);
        expect(p, ")");
        return;
    }
    if (t->kind == T_STRING) {
        const char *s = t->text;
        int pi = param_index(p, s);
        if (pi >= 0) emit(p, OP_GETPARAM, pi);
        else if ((s[0] == '$' || s[0] == '#') && s[1]) emit(p, OP_GETVAR, str_const(p, s));
        else if (n2_streq_ci(s, "null")) emit(p, OP_NULL, 0);
        else if (n2_streq_ci(s, "true")) emit(p, OP_TRUE, 0);
        else if (n2_streq_ci(s, "false")) emit(p, OP_FALSE, 0);
        else emit(p, OP_CONST, str_const(p, s));
        next(p);
        return;
    }
    if (t->kind == T_IDENT) {
        char *name = n2_strdup(t->text);
        next(p);
        if (is(p, "(")) {
            next(p);
            int argc = 0;
            u32 refs = ref_args(name);
            if (!is(p, ")")) {
                do {
                    if (is(p, ",") || is(p, ")")) {
                        emit(p, OP_NULL, 0);  // an empty argument
                    } else if ((refs >> argc & 1) && p->tok.kind == T_IDENT &&
                               (p->tok.text[0] == '$' || p->tok.text[0] == '#')) {
                        // An argument the function writes to or reads as an array: pass the variable's name.
                        emit(p, OP_CONST, str_const(p, p->tok.text));
                        next(p);
                    } else {
                        expr(p);
                    }
                    argc++;
                } while (accept(p, ",") && !p->failed);
            }
            expect(p, ")");
            emit(p, OP_CALL, str_const(p, name) | argc << 16);
            free(name);
            return;
        }
        int pi = param_index(p, name);
        if (pi >= 0) {
            emit(p, OP_GETPARAM, pi);
        } else if (name[0] == '$' || name[0] == '#') {
            if (accept(p, "[")) {
                expr(p);
                expect(p, "]");
                emit(p, OP_GETIDX, str_const(p, name));
            } else {
                emit(p, OP_GETVAR, str_const(p, name));
            }
        } else if (n2_streq_ci(name, "null")) {
            emit(p, OP_NULL, 0);
        } else if (n2_streq_ci(name, "true")) {
            emit(p, OP_TRUE, 0);
        } else if (n2_streq_ci(name, "false")) {
            emit(p, OP_FALSE, 0);
        } else {
            // A bare word is a constant such as Hideable, AxlDxl or a button name.
            emit(p, OP_CONST, str_const(p, name));
        }
        free(name);
        return;
    }
    if (t->kind == T_PUNCT && (!strcmp(t->text, "#") || !strcmp(t->text, "$"))) {
        // VariableValue(#, name) passes the sigil alone.
        emit(p, OP_CONST, str_const(p, t->text));
        next(p);
        return;
    }
    fail(p, "unexpected token in an expression");
    next(p);
}

static void unary(P *p) {
    if (accept(p, "!")) {
        unary(p);
        emit(p, OP_NOT, 0);
    } else if (accept(p, "-")) {
        unary(p);
        emit(p, OP_NEG, 0);
    } else if (accept(p, "+")) {
        unary(p);
    } else if (accept(p, "@")) {
        unary(p);
        emit(p, OP_DELTA, 0);
    } else {
        primary(p);
    }
}

static void binary(P *p, int level) {
    static const char *ops[][4] = {
        {"||"}, {"&&"}, {"==", "!="}, {"<", "<=", ">", ">="}, {"+", "-"}, {"*", "/", "%"},
    };
    static const int codes[][4] = {
        {0}, {0}, {OP_EQ, OP_NE}, {OP_LT, OP_LE, OP_GT, OP_GE}, {OP_ADD, OP_SUB}, {OP_MUL, OP_DIV, OP_MOD},
    };
    if (level == 6) {
        unary(p);
        return;
    }
    binary(p, level + 1);
    for (;;) {
        if (level == 0 && is(p, "||")) {
            next(p);
            // Short-circuit: a || b is "a ? true : bool(b)".
            emit(p, OP_DUP, 0);
            int j = emit(p, OP_JNZ, 0);
            emit(p, OP_POP, 0);
            binary(p, 1);
            emit(p, OP_NOT, 0);
            emit(p, OP_NOT, 0);
            patch(p, j, p->chunk->codeLen);
            continue;
        }
        if (level == 1 && is(p, "&&")) {
            next(p);
            emit(p, OP_DUP, 0);
            int j = emit(p, OP_JZ, 0);
            emit(p, OP_POP, 0);
            binary(p, 2);
            emit(p, OP_NOT, 0);
            emit(p, OP_NOT, 0);
            patch(p, j, p->chunk->codeLen);
            continue;
        }
        int found = -1;
        for (int i = 0; i < 4 && ops[level][i]; i++)
            if (level >= 2 && is(p, ops[level][i])) found = i;
        if (found < 0) return;
        next(p);
        binary(p, level + 1);
        emit(p, codes[level][found], 0);
    }
}

static void expr(P *p) { binary(p, 0); }

// ---- statements ----

static void statement(P *p);

static void block(P *p) {
    if (!accept(p, "[")) expect(p, "{");  // "}else[" in extra_cglibrary.nss opens a block too
    while (!p->failed && !is(p, "}") && p->tok.kind != T_EOF) statement(p);
    expect(p, "}");
}

static void push_loop(P *p, Loop *l, int start, bool isSelect) {
    memset(l, 0, sizeof *l);
    l->start = start;
    l->outer = p->loop;
    l->isSelect = isSelect;
    p->loop = l;
}

static void pop_loop(P *p, Loop *l, int end) {
    for (int i = 0; i < l->breakCount; i++) patch(p, l->breaks[i], end);
    free(l->breaks);
    p->loop = l->outer;
}

static void raw_name(P *p, char **out) {
    // A declaration or chapter name runs to whitespace, '{', '(' or ';' and may hold dots and slashes.
    skip_trivia(p);
    int a = p->pos;
    while (p->pos < p->len) {
        u8 c = p->src[p->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '{' || c == '(' || c == ';') break;
        p->pos += is_lead(c) ? 2 : 1;
    }
    if (p->pos > p->len) p->pos = p->len;
    *out = utf8_of(p, a, p->pos);
    next(p);
}

static void pre_block(P *p);

static void assignment_or_expr(P *p) {
    Tok *t = &p->tok;
    if (t->kind == T_IDENT && !t->quoted) {
        // Look ahead for an assignment without consuming the expression.
        int save = p->pos;
        Tok saved = *t;
        saved.text = n2_strdup(t->text);
        char *name = n2_strdup(t->text);
        next(p);
        LVal lv = {0};
        int pi = param_index(p, name);
        bool indexed = false;
        int idxStart = -1;
        if (pi >= 0) lv = (LVal){1, pi};
        else if (name[0] == '$' || name[0] == '#') lv = (LVal){2, str_const(p, name)};
        if (lv.kind == 2 && is(p, "[")) {
            // $arr[i] = v: compile the index first.
            next(p);
            idxStart = p->chunk->codeLen;
            expr(p);
            expect(p, "]");
            indexed = true;
            lv.kind = 3;
        }
        const char *op = p->tok.kind == T_PUNCT ? p->tok.text : NULL;
        if (lv.kind && op &&
            (!strcmp(op, "=") || !strcmp(op, "+=") || !strcmp(op, "-=") || !strcmp(op, "*=") || !strcmp(op, "/=") ||
             !strcmp(op, "%=") || !strcmp(op, "++") || !strcmp(op, "--"))) {
            char o = op[0];
            bool incdec = op[1] == o && (o == '+' || o == '-');
            next(p);
            if (indexed) {
                // Stack: index. Compound assignment re-reads the element through a second index evaluation.
                if (o != '=') {
                    int len = p->chunk->codeLen;
                    for (int i = idxStart; i < len; i++) emit(p, (int)(p->chunk->code[i] & 0xff), (int)(p->chunk->code[i] >> 8));
                    emit(p, OP_GETIDX, lv.index);
                }
            } else if (o != '=') {
                load_lval(p, lv);
            }
            if (incdec) emit(p, OP_CONST, add_const(p->chunk, v_num(1)));
            else expr(p);
            switch (o) {
            case '+': emit(p, OP_ADD, 0); break;
            case '-': emit(p, OP_SUB, 0); break;
            case '*': emit(p, OP_MUL, 0); break;
            case '/': emit(p, OP_DIV, 0); break;
            case '%': emit(p, OP_MOD, 0); break;
            }
            if (lv.kind == 1) emit(p, OP_SETPARAM, lv.index);
            else if (lv.kind == 2) emit(p, OP_SETVAR, lv.index);
            else emit(p, OP_SETIDX, lv.index);
            free(name);
            free(saved.text);
            accept(p, ";");
            return;
        }
        // Not an assignment: rewind and parse an expression statement.
        if (indexed) p->chunk->codeLen = idxStart;
        free(name);
        free(p->tok.text);
        p->tok = saved;
        p->pos = save;
    }
    expr(p);
    emit(p, OP_POP, 0);
    if (!accept(p, ";")) accept(p, ":");
}

static void statement(P *p) {
    if (p->failed) return;
    Tok *t = &p->tok;
    if (is(p, "{") || is(p, "[")) {
        block(p);
        return;
    }
    if (accept(p, ";") || accept(p, ":") || accept(p, ")")) return;  // stray terminators the PC engine tolerates
    if (is(p, "*") && p->pos < p->len && p->src[p->pos] == '/') {
        // An unmatched "*/".
        p->pos++;
        next(p);
        return;
    }
    if (t->kind == T_PRE) {
        pre_block(p);
        return;
    }
    if (t->kind == T_PUNCT && t->text[0] == '.' && t->lineStart) {
        // ".//" and "..//" marker lines.
        while (p->pos < p->len && p->src[p->pos] != '\n') p->pos++;
        next(p);
        return;
    }
    if (is_kw(p, "if")) {
        next(p);
        expect(p, "(");
        expr(p);
        expect(p, ")");
        int jz = emit(p, OP_JZ, 0);
        statement(p);
        if (is_kw(p, "else")) {
            next(p);
            int j = emit(p, OP_JMP, 0);
            patch(p, jz, p->chunk->codeLen);
            statement(p);
            patch(p, j, p->chunk->codeLen);
        } else {
            patch(p, jz, p->chunk->codeLen);
        }
        return;
    }
    if (is_kw(p, "while")) {
        next(p);
        int start = p->chunk->codeLen;
        expect(p, "(");
        expr(p);
        expect(p, ")");
        int jz = emit(p, OP_JZ, 0);
        Loop l;
        push_loop(p, &l, start, false);
        statement(p);
        emit(p, OP_JMP, start);
        patch(p, jz, p->chunk->codeLen);
        pop_loop(p, &l, p->chunk->codeLen);
        return;
    }
    if (is_kw(p, "select")) {
        next(p);
        int start = emit(p, OP_SELECT_BEGIN, 0);
        Loop l;
        push_loop(p, &l, start, true);
        block(p);
        emit(p, OP_SELECT_END, start);
        pop_loop(p, &l, p->chunk->codeLen);
        return;
    }
    if (is_kw(p, "case")) {
        next(p);
        char *label = NULL;
        bool at = accept(p, "@");
        if (p->tok.kind == T_STRING || p->tok.kind == T_IDENT) {
            label = at ? n2_fmt("@%s", p->tok.text) : n2_strdup(p->tok.text);
            next(p);
        } else {
            fail(p, "expected a case label");
            return;
        }
        accept(p, ":");
        emit(p, OP_ISPRESSED, str_const(p, label));
        free(label);
        int jz = emit(p, OP_JZ, 0);
        statement(p);
        patch(p, jz, p->chunk->codeLen);
        return;
    }
    if (is_kw(p, "return")) {
        next(p);
        if (is(p, ";") || is(p, "}")) emit(p, OP_NULL, 0);
        else expr(p);
        emit(p, OP_RET, 0);
        accept(p, ";");
        return;
    }
    if (is_kw(p, "break") || is_kw(p, "continue")) {
        bool isBreak = is_kw(p, "break");
        next(p);
        accept(p, ";");
        Loop *l = p->loop;
        if (!l) return;  // stray breaks do nothing
        if (isBreak) {
            l->breaks = n2_grow(l->breaks, sizeof(int) * (size_t)(l->breakCount + 1));
            l->breaks[l->breakCount++] = emit(p, OP_JMP, 0);
        } else {
            emit(p, OP_JMP, l->start);
        }
        return;
    }
    if (is_kw(p, "call_chapter")) {
        char *path;
        raw_name(p, &path);
        emit(p, OP_CONST, str_const(p, path));
        emit(p, OP_CALL_CHAPTER, 0);
        free(path);
        accept(p, ";");
        return;
    }
    if (is_kw(p, "call_scene")) {
        skip_trivia(p);
        if (p->pos < p->len && (p->src[p->pos] == '$' || p->src[p->pos] == '#')) {
            next(p);
            expr(p);
        } else {
            char *name;
            raw_name(p, &name);
            emit(p, OP_CONST, str_const(p, name));
            free(name);
        }
        emit(p, OP_CALL_SCENE, 0);
        accept(p, ";");
        return;
    }
    assignment_or_expr(p);
}

// <PRE @box09>[text0010] markup {code} markup </PRE>
static void pre_block(P *p) {
    const u8 *s = p->src;
    int startPos = p->tok.pos;
    while (p->pos < p->len && (s[p->pos] == ' ' || s[p->pos] == '\t')) p->pos++;
    int a = p->pos;
    while (p->pos < p->len && s[p->pos] != '>' && s[p->pos] != '\n') p->pos++;
    char *box = utf8_of(p, a, p->pos);
    if (p->pos < p->len && s[p->pos] == '>') p->pos++;
    // The end tag.
    // The end tag. "<PRE>...</PRE>" inside the block marks verbatim text and nests.
    int end = p->pos, depth = 0;
    while (end + 6 <= p->len) {
        if (strncasecmp((const char *)s + end, "<PRE>", 5) == 0) depth++;
        else if (strncasecmp((const char *)s + end, "</PRE>", 6) == 0 && depth-- == 0) break;
        end += is_lead(s[end]) ? 2 : 1;
    }
    if (end + 6 > p->len) end = p->len;
    // The block name in brackets.
    int q = p->pos;
    while (q < end && (s[q] == ' ' || s[q] == '\t' || s[q] == '\r' || s[q] == '\n')) q++;
    char *name;
    if (q < end && s[q] == '[') {
        int b = q + 1;
        while (q < end && s[q] != ']') q++;
        name = utf8_of(p, b, q);
        if (q < end) q++;
    } else {
        name = n2_fmt("text%d", p->script->blockCount);
    }
    // Compile the block body into a text chunk.
    Script *sc = p->script;
    Chunk *outer = p->chunk;
    Chunk *tc = new_chunk(p, CHUNK_TEXT, name);
    p->chunk = tc;
    Buf markup = {0};
    int i = q;
    while (i < end) {
        if (s[i] == '{') {
            if (markup.len) {
                buf_u8(&markup, 0);
                char *u = n2_sjis_to_utf8(markup.data, markup.len - 1, NULL);
                emit(p, OP_TEXT, str_const(p, u));
                free(u);
                markup.len = 0;
            }
            // Code up to the matching brace, compiled with the normal parser.
            int depth = 0, j = i;
            for (; j < end; j += is_lead(s[j]) ? 2 : 1) {
                if (s[j] == '"') {
                    j++;
                    while (j < end && s[j] != '"') j += is_lead(s[j]) ? 2 : 1;
                } else if (s[j] == '{') depth++;
                else if (s[j] == '}' && --depth == 0) break;
            }
            int savedLen = p->len;
            p->pos = i + 1;
            p->len = j;
            next(p);
            while (!p->failed && p->tok.kind != T_EOF) statement(p);
            p->len = savedLen;
            i = j + 1;
            continue;
        }
        if (s[i] == '/' && i + 1 < end && s[i + 1] == '/' && at_line_start(p, i)) {
            while (i < end && s[i] != '\n') i++;
            continue;
        }
        int n = is_lead(s[i]) ? 2 : 1;
        buf_put(&markup, s + i, (size_t)n);
        i += n;
    }
    if (markup.len) {
        buf_u8(&markup, 0);
        char *u = n2_sjis_to_utf8(markup.data, markup.len - 1, NULL);
        emit(p, OP_TEXT, str_const(p, u));
        free(u);
    }
    buf_free(&markup);
    emit(p, OP_TEXT_END, 0);
    p->chunk = outer;
    sc->blocks = n2_grow(sc->blocks, sizeof(TextBlock) * (size_t)(sc->blockCount + 1));
    TextBlock *tb = &sc->blocks[sc->blockCount];
    tb->name = name;
    tb->box = box;
    tb->chunk = tc;
    tb->srcpos = startPos;
    p->tok.pos = startPos;
    emit(p, OP_PRE, sc->blockCount);
    sc->blockCount++;
    p->pos = end + 6 <= p->len ? end + 6 : p->len;
    next(p);
}

// ---- declarations ----

static void declaration(P *p, int kind) {
    char *name;
    raw_name(p, &name);
    Chunk *c = new_chunk(p, kind, name);
    free(name);
    p->chunk = c;
    if (kind == CHUNK_FUNCTION && accept(p, "(")) {
        while (!p->failed && !is(p, ")") && p->tok.kind != T_EOF) {
            if (p->tok.kind == T_IDENT || p->tok.kind == T_STRING) {
                c->params = n2_grow(c->params, sizeof(char *) * (size_t)(c->paramCount + 1));
                c->params[c->paramCount++] = n2_strdup(p->tok.text);
            }
            next(p);
            accept(p, ",");
        }
        expect(p, ")");
    }
    block(p);
    emit(p, OP_NULL, 0);
    emit(p, OP_RET, 0);
    p->chunk = NULL;
}

Script *script_compile(const char *path, const u8 *raw, size_t rawLen, char *err, size_t errSize) {
    Script *s = n2_alloc(sizeof *s);
    s->path = n2_strdup(path);
    s->raw = n2_alloc(rawLen + 1);
    memcpy(s->raw, raw, rawLen);
    s->rawLen = rawLen;
    P p = {.src = s->raw, .len = (int)rawLen, .script = s, .err = err, .errSize = errSize};
    next(&p);
    while (!p.failed && p.tok.kind != T_EOF) {
        if (p.tok.kind == T_IDENT && p.tok.lineStart && strcmp(p.tok.text, "#include") == 0) {
            next(&p);
            if (p.tok.kind == T_STRING) {
                s->includes = n2_grow(s->includes, sizeof(char *) * (size_t)(s->includeCount + 1));
                s->includes[s->includeCount++] = n2_strdup(p.tok.text);
            }
            next(&p);
        } else if (is_kw(&p, "function") && p.tok.lineStart) {
            declaration(&p, CHUNK_FUNCTION);
        } else if (is_kw(&p, "chapter") && p.tok.lineStart) {
            declaration(&p, CHUNK_CHAPTER);
        } else if (is_kw(&p, "scene") && p.tok.lineStart) {
            declaration(&p, CHUNK_SCENE);
        } else {
            // Anything else outside a declaration is skipped a line at a time.
            while (p.pos < p.len && p.src[p.pos] != '\n') p.pos++;
            next(&p);
        }
    }
    free(p.tok.text);
    if (p.failed) {
        script_free(s);
        return NULL;
    }
    return s;
}

static void chunk_free(Chunk *c) {
    free(c->name);
    for (int i = 0; i < c->paramCount; i++) free(c->params[i]);
    free(c->params);
    free(c->code);
    free(c->srcpos);
    for (int i = 0; i < c->constCount; i++) v_free(&c->consts[i]);
    free(c->consts);
    free(c);
}

void script_free(Script *s) {
    if (!s) return;
    for (int i = 0; i < s->chunkCount; i++) chunk_free(s->chunks[i]);
    free(s->chunks);
    for (int i = 0; i < s->blockCount; i++) {
        free(s->blocks[i].name);
        free(s->blocks[i].box);
    }
    free(s->blocks);
    for (int i = 0; i < s->includeCount; i++) free(s->includes[i]);
    free(s->includes);
    free(s->path);
    free(s->source);
    free(s->raw);
    free(s);
}

Chunk *script_find(Script *s, int kind, const char *name) {
    for (int i = 0; i < s->chunkCount; i++)
        if (s->chunks[i]->kind == kind && n2_streq_ci(s->chunks[i]->name, name)) return s->chunks[i];
    return NULL;
}

TextBlock *script_block(Script *s, const char *name) {
    if (name[0] == '@') name++;
    for (int i = 0; i < s->blockCount; i++)
        if (n2_streq_ci(s->blocks[i].name, name)) return &s->blocks[i];
    return NULL;
}

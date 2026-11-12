#include "yr.h"
#include "vm.h"

typedef struct Chars {
    const char *s;
    size_t *at;
    u32 *code;
    int n;
} Chars;

static Chars split(YR *y, const char *s) {
    Chars c = { s, NULL, NULL, 0 };
    size_t len = strlen(s);
    c.at = yr_alloc(sizeof(size_t) * (len + 1));
    c.code = yr_alloc(sizeof(u32) * (len + 1));
    const u8 *p = (const u8 *)s;
    while (*p) {
        int k = yr_char_len(y->enc, p);
        u32 code = p[0];
        if (y->enc == YR_UTF8) yr_char_decode(y->enc, p, &code);
        else if (k == 2) code = (u32)p[0] << 8 | p[1];
        c.at[c.n] = (size_t)(p - (const u8 *)s);
        c.code[c.n++] = code;
        p += k;
    }
    c.at[c.n] = len;
    return c;
}

static char *piece(const Chars *c, int from, int count) {
    if (count < 0) count = 0;
    if (from + count > c->n) count = c->n - from;
    return yr_strndup(c->s + c->at[from], c->at[from + count] - c->at[from]);
}

static bool multibyte(YR *y, u32 code) {
    if (y->enc == YR_UTF8) return code >= 0x80;
    u32 hi = code >> 8;
    return (hi >= 129 && hi <= 159) || (hi >= 161 && hi <= 198) || (hi >= 201 && hi <= 252);
}

static bool word_char(YR *y, u32 code) {
    if (code >= 256) return multibyte(y, code);
    return (code >= 46 && code <= 57) || (code >= 65 && code <= 90) || code == 95 || (code >= 97 && code <= 122);
}

static YVar *target(YR *y, YCall *c, const char *key, long *base) {
    i64 ref = yc_int_or(c, key, 0);
    *base = (long)(u32)ref;
    return yr_var(y, (u32)((u64)ref >> 32));
}

typedef struct Tokens {
    YVar *text, *type;
    long textAt, typeAt;
    int n;
} Tokens;

static void emit(Tokens *t, int type, char *text) {
    YVal v = yv_str_own(text);
    if (t->text) yr_var_set(t->text, t->textAt + t->n, &v);
    yv_free(&v);
    YVal k = yv_int(type);
    if (t->type) yr_var_set(t->type, t->typeAt + t->n, &k);
    t->n++;
}

static int lex(YR *y, const char *line, Tokens *t) {
    Chars c = split(y, line);
    int state = y->lexComment ? 99 : 0, i = 0, st = 0;
    bool lineStart = false;
    while (i < c.n) {
        u32 ch = c.code[i], nx = i + 1 < c.n ? c.code[i + 1] : 0;
        if (state == 0) {
            int tok = 0;
            const char *text = NULL;
            int step = 1;
            if ((ch >= '0' && ch <= '9') || ch == '-') state = 91;
            else if (ch >= 'A' && ch <= 'Z') state = 92;
            else if (ch >= 'a' && ch <= 'z') state = 93;
            else if (ch == '_') state = 94;
            else if (ch == '.') state = 95;
            else if (ch >= 256 && multibyte(y, ch)) state = 96;
            else if (ch == '"') state = 22;
            else if (ch == ' ') state = 31;
            else if (ch == '\t') state = 32;
            else if (ch == '/' && nx == '/') { emit(t, 0, yr_strdup("\r")); break; }
            else if (ch == '/' && nx == '*') { y->lexComment = true; state = 99; }
            else if (ch == '\\') { tok = 51; text = "\\"; }
            else if (ch == '(') { tok = 52; text = "("; }
            else if (ch == ')') { tok = 53; text = ")"; }
            else if (ch == ',') { tok = 54; text = ","; }
            else if (ch == '@') { tok = 55; text = "@"; }
            else if (ch == '$') { tok = 56; text = "$"; }
            else if (ch == '#') { tok = 57; text = "#"; step = nx == '=' ? 2 : 1; }
            else if (ch == '+' && nx == '+') { tok = 11; text = "++"; step = 2; }
            else if (ch == '+' && nx == '=') { tok = 12; text = "+="; step = 2; }
            else if (ch == ';') { i++; st = i; continue; }
            else { emit(t, 16, piece(&c, st, i - st + 1)); i++; st = i; continue; }
            if (tok) {
                emit(t, tok, yr_strdup(text));
                i += step;
                st = i;
                continue;
            }
            if (state >= 91 && state <= 96 && i == 0) lineStart = true;
            i++;
            continue;
        }
        if (state == 22 && (ch == '"' || ch == '\r')) {
            state = 0;
            emit(t, 31, piece(&c, st, i - st + 1));
            i++;
            st = i;
            continue;
        }
        if ((state == 31 && ch != ' ') || (state == 32 && ch != '\t')) {
            state = 0;
            st = i;
            continue;
        }
        if (state >= 91 && state <= 96) {
            bool more = word_char(y, ch) ||
                        (lineStart && (ch == ' ' || ch == '(' || ch == ')' || ch == '*' || ch == '?' || ch == '!' || ch == ';'));
            if (!more) {
                state = 0;
                emit(t, 32, piece(&c, st, i - st));
                st = i;
                continue;
            }
        }
        if (state == 99) {
            if (ch == '*' && nx == '/') {
                state = 0;
                i += 2;
                st = i;
                y->lexComment = false;
                continue;
            }
            if (ch == '\r') emit(t, 0, yr_strdup("\r"));
        }
        i++;
    }
    free(c.at);
    free(c.code);
    return t->n;
}

int yr_product_lex(YCall *c) {
    YR *y = c->y;
    char *line = yc_str(c, "PSTR");
    Tokens t = { 0 };
    t.text = target(y, c, "PINT", &t.textAt);
    t.type = target(y, c, "PINT2", &t.typeAt);
    int n = lex(y, line ? line : "", &t);
    free(line);
    return n;
}

int yr_product_lines(YCall *c) {
    YR *y = c->y;
    long textAt, lenAt;
    YVar *text = target(y, c, "PINT", &textAt), *len = target(y, c, "PINT2", &lenAt);
    if (!text || text->t != YT_STR) return 0;
    int i = 0;
    for (; textAt + i < (long)text->count; i++) {
        const char *s = text->strs[textAt + i];
        int n = 0;
        if (s) for (const u8 *p = (const u8 *)s; *p; p += yr_char_len(y->enc, p)) n++;
        if (len) {
            YVal v = yv_int(n);
            yr_var_set(len, lenAt + i, &v);
        }
        if (!n) break;
    }
    return i;
}

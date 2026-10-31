// Variable commands: declarations, LET, VARACT, VARINFO, MATH and LABELINFO, from the manual's variable pages.
#include "yr.h"
#include "vm.h"
#include <math.h>

static u64 rng_state = 0x9e3779b97f4a7c15ull;

static u64 rng(void) {
    u64 x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return rng_state = x;
}

// INT[@A(3,4)=7]: the first SET is the variable with its dimensions, the second its initial value.
static int declare(YCall *c, u8 t) {
    const YArg *target = NULL, *value = NULL;
    for (int k = 0; k < c->in->argc; k++) {
        const YArg *a = &c->s->args[c->in->arg + k];
        if (!target) target = a;
        else if (!value) value = a;
    }
    if (!target || !target->size) return YR_NEXT;
    YRef r;
    if (!yr_lvalue(c->y, c->t, target->data, target->size, &r) || !r.var) return YR_NEXT;
    u32 dims[8];
    int nd = target->data[0] == 'V' ? r.nsub : 0;
    for (int i = 0; i < nd; i++) dims[i] = r.sub[i] > 0 ? (u32)r.sub[i] : 1;
    // Without a value the manual gives 0, or an empty string.
    YVal init = value && value->size ? yr_eval(c->y, c->t, value->data, value->size) : (YVal){ 0 };
    if (r.var->init.t) t = r.var->init.t;   // ysv knows the declared type
    if ((long)r.vid == c->y->watchVid)
        yr_log(c->y, "watch: var %u declared (script %u ip %u)\n", r.vid, c->t->script, c->t->ip);
    yr_var_declare(c->y, r.var, t, (u8)nd, dims, init.t ? &init : NULL);
    yv_free(&init);
    return YR_NEXT;
}

static YVal apply_op(int op, YVal cur, YVal v) {
    static const u8 ops[] = { 0, '+', '-', '*', '/', '%', 'A', 'O', '^' };
    if (op <= 0 || op >= (int)sizeof ops) { yv_free(&cur); return v; }
    YVal r;
    if (ops[op] == '+' && (cur.t == YT_STR || v.t == YT_STR)) {
        char *a = yv_as_str(&cur), *b = yv_as_str(&v);
        size_t na = strlen(a), nb = strlen(b);
        a = yr_realloc(a, na + nb + 1);
        memcpy(a + na, b, nb + 1);
        free(b);
        r = yv_str_own(a);
    } else if (cur.t == YT_FLT || v.t == YT_FLT) {
        double x = yv_as_flt(&cur), z = yv_as_flt(&v);
        switch (ops[op]) {
        case '+': r = yv_flt(x + z); break;
        case '-': r = yv_flt(x - z); break;
        case '*': r = yv_flt(x * z); break;
        case '/': r = yv_flt(z != 0 ? x / z : 0); break;
        case '%': r = yv_flt(z != 0 ? fmod(x, z) : 0); break;
        default: r = yv_flt(x);
        }
    } else {
        i64 x = yv_as_int(&cur), z = yv_as_int(&v);
        switch (ops[op]) {
        case '+': r = yv_int(x + z); break;
        case '-': r = yv_int(x - z); break;
        case '*': r = yv_int(x * z); break;
        case '/': r = yv_int(z ? x / z : 0); break;
        case '%': r = yv_int(z ? x % z : 0); break;
        case 'A': r = yv_int(x & z); break;
        case 'O': r = yv_int(x | z); break;
        default: r = yv_int(x ^ z); break;
        }
    }
    yv_free(&cur);
    yv_free(&v);
    return r;
}

static int let(YCall *c) {
    if (c->in->argc < 2) return YR_NEXT;
    const YArg *target = &c->s->args[c->in->arg], *value = &c->s->args[c->in->arg + 1];
    YRef r;
    if (!yr_lvalue(c->y, c->t, target->data, target->size, &r)) return YR_NEXT;
    YVal v = yr_eval(c->y, c->t, value->data, value->size);
    int op = target->type >> 8;
    if (op) v = apply_op(op, yr_ref_get(c->y, c->t, &r), v);
    yr_ref_set(c->y, c->t, &r, &v);
    yv_free(&v);
    return YR_NEXT;
}

// ---- strings by character ----

static int char_count(YR *y, const char *s) {
    int n = 0;
    for (const u8 *p = (const u8 *)s; *p; p += yr_char_len(y->enc, p)) n++;
    return n;
}

// Byte offset of character pos (0-based), clamped to the end.
static size_t char_offset(YR *y, const char *s, int pos) {
    const u8 *p = (const u8 *)s;
    for (int i = 0; i < pos && *p; i++) p += yr_char_len(y->enc, p);
    return (size_t)(p - (const u8 *)s);
}

static bool is_full_width(YR *y, const u8 *p) {
    u32 cp;
    int n = yr_char_decode(y->enc, p, &cp);
    if (y->enc == YR_SJIS) return n == 2;
    return cp >= 0x1100 && !(cp >= 0xff61 && cp <= 0xff9f);
}

static char *convert_width(YR *y, const char *s, bool toFull) {
    size_t n = strlen(s);
    char *out = yr_alloc(n * 3 + 1), *o = out;
    const u8 *p = (const u8 *)s;
    while (*p) {
        int len = yr_char_len(y->enc, p);
        u32 cp;
        yr_char_decode(y->enc, p, &cp);
        if (toFull && len == 1 && cp >= 0x21 && cp <= 0x7e) {
            u32 f = 0xff01 + (cp - 0x21);
            if (y->enc == YR_UTF8) { *o++ = (char)(0xe0 | f >> 12); *o++ = (char)(0x80 | ((f >> 6) & 0x3f)); *o++ = (char)(0x80 | (f & 0x3f)); }
            else {
                // Shift-JIS full-width ASCII: 0x8240-ish rows; digits and letters are 0x824f.., others vary, so keep
                // the half-width character where the table has no simple row.
                if (cp >= '0' && cp <= '9') { *o++ = (char)0x82; *o++ = (char)(0x4f + cp - '0'); }
                else if (cp >= 'A' && cp <= 'Z') { *o++ = (char)0x82; *o++ = (char)(0x60 + cp - 'A'); }
                else if (cp >= 'a' && cp <= 'z') { *o++ = (char)0x82; *o++ = (char)(0x81 + cp - 'a'); }
                else *o++ = (char)cp;
            }
        } else if (!toFull && cp >= 0xff01 && cp <= 0xff5e) {
            *o++ = (char)(0x21 + cp - 0xff01);
        } else if (!toFull && cp == 0x3000) {
            *o++ = ' ';
        } else {
            memcpy(o, p, (size_t)len);
            o += len;
        }
        p += len;
    }
    *o = 0;
    return out;
}

static void resize_1d(YR *y, YVar *v, i64 size) {
    if (size < 0) size = 0;
    YVar old = *v;
    v->ints = NULL;
    v->flts = NULL;
    v->strs = NULL;
    v->count = 0;
    u32 dims[1] = { (u32)size };
    yr_var_declare(y, v, old.t, 1, dims, NULL);
    for (u32 i = 0; i < v->count && i < old.count; i++) {
        if (v->t == YT_INT) v->ints[i] = old.ints[i];
        else if (v->t == YT_FLT) v->flts[i] = old.flts[i];
        else { v->strs[i] = old.strs[i]; old.strs[i] = NULL; }
    }
    if (old.strs) for (u32 i = 0; i < old.count; i++) free(old.strs[i]);
    free(old.ints);
    free(old.flts);
    free(old.strs);
}

static int varact(YCall *c) {
    YR *y = c->y;
    YRef set, out;
    bool hasSet = yc_ref(c, "SET", &set), hasLet = yc_ref(c, "LET", &out);
    if (!hasSet) return YR_NEXT;
    if (yc_has(c, "DIMSIZE") && set.var) {
        const YArg *a = yc_arg(c, "DIMSIZE");
        i64 n = yc_int_or(c, "DIMSIZE", 0);
        int op = a->type >> 8;
        if (op == 1) n = (i64)set.var->count + n;
        else if (op == 2) n = (i64)set.var->count - n;
        resize_1d(y, set.var, n);
        return YR_NEXT;
    }
    if ((yc_int_or(c, "PUSH", 0) || yc_int_or(c, "POP", 0)) && set.var) {
        bool push = yc_int_or(c, "PUSH", 0) != 0;
        YVar *v = set.var;
        i64 pos = yc_int_or(c, "POS", 0);
        i64 n = v->count;
        if (push) {
            resize_1d(y, v, n + 1);
            if (pos < 0 || pos > n) pos = n;
            for (i64 i = n; i > pos; i--) {
                if (v->t == YT_INT) v->ints[i] = v->ints[i - 1];
                else if (v->t == YT_FLT) v->flts[i] = v->flts[i - 1];
                else { v->strs[i] = v->strs[i - 1]; v->strs[i - 1] = NULL; }
            }
            YVal zero = v->t == YT_STR ? yv_str("") : yv_int(0);
            yr_var_set(v, (long)pos, &zero);
            yv_free(&zero);
        } else if (n > 0 && pos >= 0 && pos < n) {
            if (v->t == YT_STR) free(v->strs[pos]);
            for (i64 i = pos; i < n - 1; i++) {
                if (v->t == YT_INT) v->ints[i] = v->ints[i + 1];
                else if (v->t == YT_FLT) v->flts[i] = v->flts[i + 1];
                else v->strs[i] = v->strs[i + 1];
            }
            if (v->t == YT_STR) v->strs[n - 1] = NULL;
            resize_1d(y, v, n - 1);
        }
        return YR_NEXT;
    }
    if (yc_int_or(c, "INIT", 0) && set.var) {
        YVar *v = set.var;
        yr_var_declare(y, v, v->t ? v->t : v->init.t, v->ndim, v->dims, &v->init);
        return YR_NEXT;
    }
    YVal src = yr_ref_get(y, c->t, &set);
    YVal result = { 0 };
    char *s = yv_as_str(&src);
    if (yc_int_or(c, "COPY", 0) || yc_int_or(c, "CUT", 0)) {
        int pos = (int)yc_int_or(c, "POS", 1) - 1, len = (int)yc_int_or(c, "LENGTH", 1 << 30);
        if (pos < 0) pos = 0;
        size_t a = char_offset(y, s, pos), b = a + char_offset(y, s + a, len);
        if (yc_int_or(c, "COPY", 0)) result = yv_str_own(yr_strndup(s + a, b - a));
        else {
            size_t n = strlen(s);
            char *r = yr_alloc(n + 1);
            memcpy(r, s, a);
            memcpy(r + a, s + b, n - b + 1);
            result = yv_str_own(r);
        }
    } else if (yc_int_or(c, "TYPE", 0)) {
        result = src.t == YT_STR ? yv_int(strtoll(s, NULL, 10)) : yv_str(s);
    } else if (yc_int_or(c, "UPPER", 0) || yc_int_or(c, "UPPER2", 0) || yc_int_or(c, "LOWER", 0) ||
               yc_int_or(c, "LOWER2", 0)) {
        bool up = yc_int_or(c, "UPPER", 0) || yc_int_or(c, "UPPER2", 0);
        char *r = yr_strdup(s);
        for (u8 *p = (u8 *)r; *p;) {
            int n = yr_char_len(y->enc, p);
            if (n == 1) *p = (u8)(up ? (*p >= 'a' && *p <= 'z' ? *p - 32 : *p) : (*p >= 'A' && *p <= 'Z' ? *p + 32 : *p));
            p += n;
        }
        result = yv_str_own(r);
    } else if (yc_int_or(c, "HANTOZEN", 0)) {
        result = yv_str_own(convert_width(y, s, true));
    } else if (yc_int_or(c, "ZENTOHAN", 0)) {
        result = yv_str_own(convert_width(y, s, false));
    } else {
        yc_unknown(c, "VARACT form");
        result = yv_copy(&src);
    }
    if (hasLet) yr_ref_set(y, c->t, &out, &result);
    else yr_ref_set(y, c->t, &set, &result);
    free(s);
    yv_free(&src);
    yv_free(&result);
    return YR_NEXT;
}

static int varinfo(YCall *c) {
    YR *y = c->y;
    YRef set, out;
    if (!yc_ref(c, "SET", &set) || !yc_ref(c, "LET", &out)) return YR_NEXT;
    YVal r = yv_int(0);
    YVar *v = set.var ? set.var : yr_var(y, set.vid);
    if (yc_int_or(c, "TYPE", 0)) r = yv_int(yr_ref_type(y, &set));
    else if (yc_int_or(c, "LENGTH", 0)) {
        YVal s = yr_ref_get(y, c->t, &set);
        char *str = yv_as_str(&s);
        r = yv_int(char_count(y, str));
        free(str);
        yv_free(&s);
    } else if (yc_int_or(c, "STRTYPE", 0)) {
        YVal s = yr_ref_get(y, c->t, &set);
        char *str = yv_as_str(&s);
        r = yv_int(!*str ? 0 : is_full_width(y, (const u8 *)str) ? 2 : 1);
        free(str);
        yv_free(&s);
    } else if (yc_int_or(c, "SJISCODE", 0) || yc_int_or(c, "UTF8CODE", 0)) {
        YVal s = yr_ref_get(y, c->t, &set);
        char *str = yv_as_str(&s);
        const u8 *p = (const u8 *)str;
        i64 code = 0;
        int n = *p ? yr_char_len(y->enc, p) : 0;
        for (int i = 0; i < n; i++) code = code << 8 | p[i];
        r = yv_int(code);
        free(str);
        yv_free(&s);
    } else if (yc_int_or(c, "DIMNUM", 0)) {
        r = yv_int(v ? v->ndim : 0);
    } else if (yc_int_or(c, "SEARCH", 0) && v) {
        i64 start = yc_int_or(c, "NO", 0), found = -1;
        YVal want = yc_has(c, "STR") ? yc_val(c, "STR") : yc_has(c, "FLT") ? yc_val(c, "FLT") : yc_val(c, "INT");
        for (i64 i = start < 0 ? 0 : start; i < (i64)v->count && found < 0; i++) {
            YVal e = yr_var_get(v, (long)i);
            bool eq = false;
            if (e.t == YT_STR) { char *w = yv_as_str(&want); eq = !strcmp(e.s, w); free(w); }
            else if (e.t == YT_FLT) eq = e.f == yv_as_flt(&want);
            else eq = e.i == yv_as_int(&want);
            if (eq) found = i;
            yv_free(&e);
        }
        yv_free(&want);
        r = yv_int(found);
    } else {
        static const char *const dimKeys[] = { "DIMSIZE", "DIMSIZE2", "DIMSIZE3", "DIMSIZE4", "DIMSIZE5", "DIMSIZE6",
                                               "DIMSIZE7", "DIMSIZE8" };
        bool done = false;
        for (int i = 0; i < 8 && !done; i++)
            if (yc_int_or(c, dimKeys[i], 0)) {
                r = yv_int(v && i < v->ndim ? v->dims[i] : (v && i == 0 && v->ndim == 0 ? 1 : 0));
                done = true;
            }
        if (!done) yc_unknown(c, "VARINFO form");
    }
    yr_ref_set(y, c->t, &out, &r);
    yv_free(&r);
    return YR_NEXT;
}

static int math(YCall *c) {
    YRef out;
    if (!yc_ref(c, "LET", &out)) return YR_NEXT;
    double a = 0, b = 0;
    yc_flt(c, "SET", &a);
    yc_flt(c, "SET2", &b);
    YVal r;
    if (yc_int_or(c, "RND", 0)) {
        i64 lo = (i64)a, hi = (i64)b;
        if (!yc_has(c, "SET2")) { hi = lo; lo = 0; }
        if (hi < lo) { i64 t = hi; hi = lo; lo = t; }
        r = yv_int(lo + (i64)(rng() % (u64)(hi - lo + 1)));
    } else {
        double x;
        if (yc_int_or(c, "SIN", 0)) x = sin(a);
        else if (yc_int_or(c, "ASIN", 0)) x = asin(a);
        else if (yc_int_or(c, "SINH", 0)) x = sinh(a);
        else if (yc_int_or(c, "COS", 0)) x = cos(a);
        else if (yc_int_or(c, "ACOS", 0)) x = acos(a);
        else if (yc_int_or(c, "COSH", 0)) x = cosh(a);
        else if (yc_int_or(c, "TAN", 0)) x = tan(a);
        else if (yc_int_or(c, "ATAN2", 0)) x = atan2(a, b);
        else if (yc_int_or(c, "ATAN", 0)) x = atan(a);
        else if (yc_int_or(c, "TANH", 0)) x = tanh(a);
        else if (yc_int_or(c, "SQRT", 0)) x = sqrt(a);
        else if (yc_int_or(c, "ABS", 0)) x = fabs(a);
        else if (yc_int_or(c, "EXP", 0)) x = exp(a);
        else if (yc_int_or(c, "LOG10", 0)) x = log10(a);
        else if (yc_int_or(c, "LOG", 0)) x = log(a);
        else if (yc_int_or(c, "POW", 0)) x = pow(a, b);
        else { yc_unknown(c, "MATH form"); x = a; }
        r = yv_flt(x);
    }
    yr_ref_set(c->y, c->t, &out, &r);
    return YR_NEXT;
}

static int labelinfo(YCall *c) {
    YRef out;
    char *name = yc_str(c, "#");
    if (yc_ref(c, "LET", &out)) {
        YVal r = yv_int(name && yr_label(c->y, name) ? 1 : 0);
        yr_ref_set(c->y, c->t, &out, &r);
    }
    free(name);
    return YR_NEXT;
}

int yr_cmd_vars(YCall *c, int id) {
    switch (id) {
    case C_INT: case C_S_INT: case C_F_INT: case C_G_INT: case C_G_INT2: case C_G_INT3: case C_G_INT4:
        return declare(c, YT_INT);
    case C_FLT: case C_S_FLT: case C_F_FLT: case C_G_FLT: case C_G_FLT2: case C_G_FLT3: case C_G_FLT4:
        return declare(c, YT_FLT);
    case C_STR: case C_S_STR: case C_F_STR: case C_G_STR: case C_G_STR2: case C_G_STR3: case C_G_STR4:
        return declare(c, YT_STR);
    case C_VAR: case C_S_VAR: case C_F_VAR: case C_G_VAR: return declare(c, YT_INT);
    case C_LET: return let(c);
    case C_VARACT: return varact(c);
    case C_VARINFO: return varinfo(c);
    case C_MATH: return math(c);
    case C_LABELINFO: return labelinfo(c);
    }
    return YR_NEXT;
}

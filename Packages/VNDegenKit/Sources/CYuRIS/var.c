// Values and variables. ysv.ybn lists every variable of the project, system ones (ids below 1000) included, with
// its scope, type, array dimensions and initial value.
#include "yr.h"
#include "vm.h"
#include <inttypes.h>

void yv_free(YVal *v) {
    if (v->t == YT_STR) free(v->s);
    v->s = NULL;
    v->t = YT_NONE;
}

YVal yv_int(i64 i) { return (YVal){ .t = YT_INT, .i = i }; }
YVal yv_flt(double f) { return (YVal){ .t = YT_FLT, .f = f }; }
YVal yv_str(const char *s) { return (YVal){ .t = YT_STR, .s = yr_strdup(s ? s : "") }; }
YVal yv_str_own(char *s) { return (YVal){ .t = YT_STR, .s = s ? s : yr_strdup("") }; }

YVal yv_copy(const YVal *v) {
    YVal c = *v;
    if (v->t == YT_STR) c.s = yr_strdup(v->s ? v->s : "");
    return c;
}

i64 yv_as_int(const YVal *v) {
    switch (v->t) {
    case YT_INT: return v->i;
    case YT_FLT: return (i64)v->f;
    case YT_STR: return v->s ? strtoll(v->s, NULL, 0) : 0;
    default: return 0;
    }
}

double yv_as_flt(const YVal *v) {
    switch (v->t) {
    case YT_INT: return (double)v->i;
    case YT_FLT: return v->f;
    case YT_STR: return v->s ? strtod(v->s, NULL) : 0;
    default: return 0;
    }
}

char *yv_as_str(const YVal *v) {
    char buf[64];
    switch (v->t) {
    case YT_INT: snprintf(buf, sizeof buf, "%" PRId64, v->i); return yr_strdup(buf);
    case YT_FLT: snprintf(buf, sizeof buf, "%g", v->f); return yr_strdup(buf);
    case YT_STR: return yr_strdup(v->s ? v->s : "");
    default: return yr_strdup("");
    }
}

static void var_storage_free(YVar *v) {
    if (v->strs) for (u32 i = 0; i < v->count; i++) free(v->strs[i]);
    free(v->ints);
    free(v->flts);
    free(v->strs);
    v->ints = NULL;
    v->flts = NULL;
    v->strs = NULL;
    v->count = 0;
}

void yr_var_declare(YR *y, YVar *v, u8 t, u8 ndim, const u32 *dims, const YVal *init) {
    (void)y;
    var_storage_free(v);
    if (t < YT_INT || t > YT_STR) t = YT_INT;
    v->t = t;
    v->ndim = ndim > 8 ? 8 : ndim;
    u64 count = 1;
    for (int i = 0; i < v->ndim; i++) {
        v->dims[i] = dims[i] ? dims[i] : 1;
        count *= v->dims[i];
        if (count > (1u << 26)) count = 1u << 26;
    }
    v->count = (u32)count;
    v->declared = 1;
    switch (t) {
    case YT_INT: {
        v->ints = yr_alloc(sizeof(i64) * v->count);
        i64 x = init ? yv_as_int(init) : 0;
        if (x) for (u32 i = 0; i < v->count; i++) v->ints[i] = x;
        break;
    }
    case YT_FLT: {
        v->flts = yr_alloc(sizeof(double) * v->count);
        double x = init ? yv_as_flt(init) : 0;
        for (u32 i = 0; i < v->count; i++) v->flts[i] = x;
        break;
    }
    default: {
        v->strs = yr_alloc(sizeof(char *) * v->count);
        if (init && init->t == YT_STR && init->s && *init->s)
            for (u32 i = 0; i < v->count; i++) v->strs[i] = yr_strdup(init->s);
        else if (init && init->t != YT_STR && init->t != YT_NONE)
            for (u32 i = 0; i < v->count; i++) v->strs[i] = yv_as_str(init);
        break;
    }
    }
}

long yr_var_index(const YVar *v, int n, const i64 *sub) {
    if (n == 0) return 0;
    // One subscript into a multi-dimensional array indexes its elements in order.
    if (n == 1 && v->ndim != 1) return sub[0] >= 0 && sub[0] < (i64)v->count ? (long)sub[0] : -1;
    long idx = 0;
    for (int i = 0; i < n; i++) {
        u32 d = i < v->ndim ? v->dims[i] : 1;
        if (sub[i] < 0 || sub[i] >= (i64)d) return -1;
        idx = idx * (long)d + (long)sub[i];
    }
    for (int i = n; i < v->ndim; i++) idx *= (long)v->dims[i];
    return idx < (long)v->count ? idx : -1;
}

YVal yr_var_get(const YVar *v, long i) {
    if (i < 0 || i >= (long)v->count) {
        if (v->t == YT_STR) return yv_str("");
        return v->t == YT_FLT ? yv_flt(0) : yv_int(0);
    }
    switch (v->t) {
    case YT_INT: return yv_int(v->ints[i]);
    case YT_FLT: return yv_flt(v->flts[i]);
    default: return yv_str(v->strs[i]);
    }
}

void yr_var_set(YVar *v, long i, const YVal *val) {
    if (i < 0 || i >= (long)v->count) return;
    switch (v->t) {
    case YT_INT: v->ints[i] = yv_as_int(val); break;
    case YT_FLT: v->flts[i] = yv_as_flt(val); break;
    default:
        free(v->strs[i]);
        v->strs[i] = val->t == YT_STR ? yr_strdup(val->s ? val->s : "") : yv_as_str(val);
        break;
    }
}

YVar *yr_var(YR *y, u32 id) { return id < y->varCount ? &y->vars[id] : NULL; }

// Entries: scope, (a flag byte in newer versions), script, id, type, dimension count, dimensions, initial value.
static bool parse(YR *y, const u8 *d, size_t size, bool flagByte, bool commit) {
    size_t o = 10;
    while (o < size) {
        size_t head = flagByte ? 8 : 7;
        if (o + head > size) return false;
        u8 scope = d[o];
        u16 script = yr_le16(d + o + (flagByte ? 2 : 1));
        u16 id = yr_le16(d + o + (flagByte ? 4 : 3));
        u8 t = d[o + head - 2], nd = d[o + head - 1];
        if (t > 3 || nd > 8) return false;
        o += head;
        if (o + 4u * nd > size) return false;
        u32 dims[8] = { 0 };
        for (int i = 0; i < nd; i++) dims[i] = yr_le32(d + o + 4 * i);
        o += 4u * nd;
        YVal init = { 0 };
        if (t == YT_INT || t == YT_FLT) {
            if (o + 8 > size) return false;
            u64 bits = yr_le64(d + o);
            if (t == YT_INT) init = yv_int((i64)bits);
            else { double f; memcpy(&f, &bits, 8); init = yv_flt(f); }
            o += 8;
        } else if (t == YT_STR) {
            if (o + 2 > size) return false;
            u16 n = yr_le16(d + o);
            if (o + 2 + n > size) return false;
            if (commit && id < y->varCount && n) {
                YVar *v = &y->vars[id];
                free(v->initExpr);
                v->initExpr = yr_alloc(n);
                memcpy(v->initExpr, d + o + 2, n);
                v->initExprSize = n;
            }
            if (commit) init = yv_str("");
            o += 2u + n;
        }
        if (commit && id < y->varCount) {
            YVar *v = &y->vars[id];
            v->scope = scope;
            v->script = script;
            yv_free(&v->init);
            v->init = init;
            v->initNdim = nd;
            memcpy(v->initDims, dims, sizeof dims);
            if (t) yr_var_declare(y, v, t, nd, dims, &init);
            v->declared = 0;
        } else {
            yv_free(&init);
        }
    }
    return o == size;
}

bool yr_vars_open(YR *y, const u8 *d, size_t size, u32 version) {
    (void)version;
    if (!d || size < 10 || memcmp(d, "YSVR", 4)) return false;
    // The highest id decides the table size; read it with whichever entry layout parses the whole file.
    bool flag = parse(y, d, size, true, false);
    if (!flag && !parse(y, d, size, false, false)) return false;
    u32 maxId = 1000;
    size_t o = 10;
    while (o < size) {
        size_t head = flag ? 8 : 7;
        u16 id = yr_le16(d + o + (flag ? 4 : 3));
        u8 t = d[o + head - 2], nd = d[o + head - 1];
        if (id + 1u > maxId) maxId = id + 1u;
        o += head + 4u * nd;
        if (t == YT_INT || t == YT_FLT) o += 8;
        else if (t == YT_STR) o += 2u + yr_le16(d + o);
    }
    y->varCount = maxId;
    y->vars = yr_alloc(sizeof(YVar) * maxId);
    return parse(y, d, size, flag, true);
}

void yr_vars_init_strings(YR *y) {
    for (u32 i = 0; i < y->varCount; i++) {
        YVar *v = &y->vars[i];
        if (!v->initExpr) continue;
        YVal val = yr_eval(y, NULL, v->initExpr, v->initExprSize);
        yv_free(&v->init);
        v->init = yv_str_own(yv_as_str(&val));
        yv_free(&val);
        yr_var_declare(y, v, YT_STR, v->initNdim, v->initDims, &v->init);
        v->declared = 0;
    }
}

void yr_vars_close(YR *y) {
    for (u32 i = 0; i < y->varCount; i++) {
        var_storage_free(&y->vars[i]);
        yv_free(&y->vars[i].init);
        free(y->vars[i].initExpr);
    }
    free(y->vars);
    y->vars = NULL;
    y->varCount = 0;
}

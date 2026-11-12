// Keyword arguments of the running instruction.
#include "yr.h"
#include "vm.h"

const YArg *yc_arg(const YCall *c, const char *key) {
    for (int k = 0; k < c->in->argc; k++) {
        const YArg *a = &c->s->args[c->in->arg + k];
        if (!strcasecmp(yr_arg_key(c->y, c->in, a), key)) return a;
    }
    return NULL;
}

bool yc_has(const YCall *c, const char *key) { return yc_arg(c, key) != NULL; }

YVal yc_val(const YCall *c, const char *key) {
    const YArg *a = yc_arg(c, key);
    if (!a || !a->size) return (YVal){ 0 };
    return yr_eval(c->y, c->t, a->data, a->size);
}

bool yc_int(const YCall *c, const char *key, i64 *out) {
    const YArg *a = yc_arg(c, key);
    if (!a || !a->size) return false;
    YVal v = yr_eval(c->y, c->t, a->data, a->size);
    *out = yv_as_int(&v);
    yv_free(&v);
    return true;
}

i64 yc_int_or(const YCall *c, const char *key, i64 def) {
    i64 v;
    return yc_int(c, key, &v) ? v : def;
}

bool yc_flt(const YCall *c, const char *key, double *out) {
    const YArg *a = yc_arg(c, key);
    if (!a || !a->size) return false;
    YVal v = yr_eval(c->y, c->t, a->data, a->size);
    *out = yv_as_flt(&v);
    yv_free(&v);
    return true;
}

char *yc_str(const YCall *c, const char *key) {
    const YArg *a = yc_arg(c, key);
    if (!a || !a->size) return NULL;
    YVal v = yr_eval(c->y, c->t, a->data, a->size);
    char *s = yv_as_str(&v);
    yv_free(&v);
    return s;
}

bool yc_ref(const YCall *c, const char *key, YRef *out) {
    const YArg *a = yc_arg(c, key);
    if (!a || !a->size) return false;
    return yr_lvalue(c->y, c->t, a->data, a->size, out);
}

void yc_unknown(const YCall *c, const char *what) {
    YR *y = c->y;
    if (!y->log) return;
    yr_log(y, "unsupported %s in %s (script %u ip %u):", what, y->cmds[c->in->cmd].name, c->t->script, c->t->ip);
    for (int k = 0; k < c->in->argc; k++) yr_log(y, " %s", yr_arg_key(y, c->in, &c->s->args[c->in->arg + k]));
    yr_log(y, "\n");
}

#include "eagls.h"

#include <ctype.h>

static u32 hash_var(const char *s) {
    u32 h = 2166136261u;
    for (; *s; s++) h = (h ^ (u8)tolower((u8)*s)) * 16777619u;
    return h;
}

static void rehash(VarTable *t) {
    free(t->hash);
    t->hashSize = 64;
    while (t->hashSize < t->count * 2) t->hashSize <<= 1;
    t->hash = eg_alloc(sizeof(int) * (size_t)t->hashSize);
    for (int i = 0; i < t->hashSize; i++) t->hash[i] = -1;
    for (int i = 0; i < t->count; i++) {
        u32 h = hash_var(t->vars[i].name) & (u32)(t->hashSize - 1);
        while (t->hash[h] >= 0) h = (h + 1) & (u32)(t->hashSize - 1);
        t->hash[h] = i;
    }
}

Var *var_find(VarTable *t, const char *name) {
    if (!t->hashSize) return NULL;
    u32 h = hash_var(name) & (u32)(t->hashSize - 1);
    while (t->hash[h] >= 0) {
        Var *v = &t->vars[t->hash[h]];
        if (eg_ieq(v->name, name)) return v;
        h = (h + 1) & (u32)(t->hashSize - 1);
    }
    return NULL;
}

Var *var_get(VarTable *t, const char *name, bool create) {
    Var *v = var_find(t, name);
    if (v || !create) return v;
    if (t->count == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 256;
        t->vars = eg_realloc(t->vars, sizeof(Var) * (size_t)t->cap);
    }
    v = &t->vars[t->count++];
    memset(v, 0, sizeof *v);
    v->name = eg_strdup(name);
    if (t->count * 2 > t->hashSize) rehash(t);
    else {
        u32 h = hash_var(name) & (u32)(t->hashSize - 1);
        while (t->hash[h] >= 0) h = (h + 1) & (u32)(t->hashSize - 1);
        t->hash[h] = t->count - 1;
    }
    return v;
}

static void var_free(Var *v) {
    for (int i = 0; i < v->n; i++) free(v->v[i].s);
    free(v->v);
    free(v->name);
}

void var_remove(VarTable *t, const char *name) {
    Var *v = var_find(t, name);
    if (!v) return;
    int idx = (int)(v - t->vars);
    var_free(v);
    t->vars[idx] = t->vars[--t->count];
    rehash(t);
}

void var_clear(VarTable *t) {
    for (int i = 0; i < t->count; i++) var_free(&t->vars[i]);
    free(t->vars);
    free(t->hash);
    memset(t, 0, sizeof *t);
}

Val *var_at(Var *v, int index) {
    if (index < 0) index = 0;
    if (index > 100000) index = 100000;
    if (index >= v->n) {
        int n = index + 1;
        if (n < 8) n = 8;
        v->v = eg_realloc(v->v, sizeof(Val) * (size_t)n);
        memset(v->v + v->n, 0, sizeof(Val) * (size_t)(n - v->n));
        v->n = n;
    }
    return &v->v[index];
}

static char numbuf[8][32];
static int numbufNext;

static const char *num_str(i32 n) {
    char *b = numbuf[numbufNext++ & 7];
    snprintf(b, 32, "%d", n);
    return b;
}

Val *vm_ref(EG *e, const Arg *a, bool create) {
    Var *v = var_get(&e->vars, a->s, create);
    if (!v) return NULL;
    int idx = a->index ? vm_int(e, a->index) : 0;
    return var_at(v, idx);
}

i32 vm_int(EG *e, const Arg *a) {
    switch (a->kind) {
    case A_NUM: return a->num;
    case A_VAR: {
        Val *v = vm_ref(e, a, false);
        if (!v) return 0;
        return v->s && !v->i ? (i32)strtol(v->s, NULL, 10) : v->i;
    }
    case A_STR:
    case A_WORD: {
        if (a->s && (a->s[0] == '_' || a->s[0] == '@' || a->s[0] == ':')) {
            Var *v = var_find(&e->vars, a->s);
            if (v) return var_at(v, 0)->i;
        }
        return a->s ? (i32)strtol(a->s, NULL, 10) : 0;
    }
    default: return 0;
    }
}

const char *vm_str(EG *e, const Arg *a) {
    switch (a->kind) {
    case A_NUM: return num_str(a->num);
    case A_VAR: {
        Val *v = vm_ref(e, a, false);
        if (!v) return "";
        if (v->s) return v->s;
        Var *var = var_find(&e->vars, a->s);
        return var && var->str ? "" : num_str(v->i);
    }
    case A_STR: {
        if (a->s[0] == '_' || a->s[0] == '@' || a->s[0] == ':') {
            Var *v = var_find(&e->vars, a->s);
            if (v) {
                Val *x = var_at(v, 0);
                return x->s ? x->s : v->str ? "" : num_str(x->i);
            }
        }
        return a->s;
    }
    case A_WORD: return a->s ? a->s : "";
    default: return "";
    }
}

const char *vm_name(EG *e, const Arg *a) {
    if (a->kind == A_VAR) return vm_str(e, a);
    return a->s ? a->s : "";
}

void vm_set_int(EG *e, const Arg *a, i32 v) {
    Val *x = vm_ref(e, a, true);
    x->i = v;
    free(x->s);
    x->s = NULL;
}

void vm_set_str_name(EG *e, const char *name, const char *s) {
    char base[128];
    snprintf(base, sizeof base, "%s", name);
    int idx = 0;
    char *br = strchr(base, '[');
    if (br) {
        *br = 0;
        const char *in = br + 1;
        if (*in == '_' || *in == '@' || *in == ':') {
            char iv[128];
            snprintf(iv, sizeof iv, "%s", in);
            char *close = strchr(iv, ']');
            if (close) *close = 0;
            Var *x = var_find(&e->vars, iv);
            idx = x ? var_at(x, 0)->i : 0;
        } else idx = atoi(in);
    }
    Var *v = var_get(&e->vars, base, true);
    v->str = true;
    Val *x = var_at(v, idx);
    char *d = eg_strdup(s);
    free(x->s);
    x->s = d;
    x->i = 0;
}

static i32 eval(EG *e, const Arg *args, int n) {
    i64 st[64];
    int sp = 0;
    for (int i = 0; i < n; i++) {
        const Arg *a = &args[i];
        if (a->kind == A_OP) {
            if (sp < 2) continue;
            i64 b = st[--sp], x = st[--sp], r = 0;
            switch (a->op) {
            case '+': r = x + b; break;
            case '-': r = x - b; break;
            case '*': r = x * b; break;
            case '/': r = b ? x / b : 0; break;
            case '%': r = b ? x % b : 0; break;
            case '=': r = x == b; break;
            case '!': r = x != b; break;
            case '>': r = x > b; break;
            case '<': r = x < b; break;
            case '&': r = x && b; break;
            case '|': r = x || b; break;
            default: break;
            }
            st[sp++] = r;
        } else if (sp < 64) st[sp++] = vm_int(e, a);
    }
    return sp ? (i32)st[sp - 1] : 0;
}

Script *vm_script(EG *e, const char *file) {
    for (int i = 0; i < e->scriptCount; i++)
        if (eg_ieq(e->scripts[i]->name, file)) return e->scripts[i];
    const PakEntry *pe = pak_find(&e->pak[PK_SC], file);
    size_t n = 0;
    u8 *d = NULL;
    if (pe) d = pak_read(&e->pak[PK_SC], pe, &n);
    if (!d) {
        char path[2048];
        snprintf(path, sizeof path, "%s/Script/%s", e->dataDir, file);
        d = eg_read_file(path, &n);
    }
    if (!d) {
        eg_log(e, "missing script %s", file);
        return NULL;
    }
    size_t tn;
    u8 *text = script_decrypt(d, n, &tn);
    free(d);
    if (!text) return NULL;
    Script *s = script_parse(file, text, tn);
    free(text);
    e->scripts = eg_realloc(e->scripts, sizeof(Script *) * (size_t)(e->scriptCount + 1));
    e->scripts[e->scriptCount++] = s;
    return s;
}

static bool locate(EG *e, Thread *t, const char *file, const char *label, Script **sc, int *pc) {
    Script *s = (!file || !*file || !strcmp(file, "0")) ? t->sc : vm_script(e, file);
    if (!s) {
        snprintf(e->problem, sizeof e->problem, "missing script %s", file ? file : "?");
        return false;
    }
    int at = label && *label ? script_label(s, label) : 0;
    if (at < 0) {
        eg_log(e, "missing label %s in %s", label, s->name);
        return false;
    }
    *sc = s;
    *pc = at;
    return true;
}

void vm_goto(EG *e, Thread *t, const char *file, const char *label) {
    Script *s;
    int pc;
    if (!locate(e, t, file, label, &s, &pc)) return;
    t->sc = s;
    t->pc = pc;
    t->active = true;
}

bool vm_call(EG *e, Thread *t, const char *file, const char *label) {
    Script *s;
    int pc;
    if (!locate(e, t, file, label, &s, &pc)) return false;
    if (t->sp >= EG_STACK) {
        eg_log(e, "call stack overflow at %s", label);
        return false;
    }
    t->stack[t->sp].sc = t->sc;
    t->stack[t->sp].pc = t->pc;
    t->sp++;
    t->sc = s;
    t->pc = pc;
    return true;
}

static void thread_return(EG *e, Thread *t) {
    if (t->sp > 0) {
        t->sp--;
        t->sc = t->stack[t->sp].sc;
        t->pc = t->stack[t->sp].pc;
        return;
    }
    if (e->trace) eg_log(e, "thread %d ends in %s", (int)(t - e->th), t->sc ? t->sc->name : "-");
    t->active = false;
}

void vm_init(EG *e) {
    memset(e->th, 0, sizeof e->th);
    Thread *t = &e->th[0];
    t->sc = vm_script(e, "IniScriptPri.dat");
    t->active = t->sc != NULL;
}

static void trace_ins(EG *e, Thread *t, const Ins *in) {
    char buf[512];
    int k = snprintf(buf, sizeof buf, "[%d] %s:%d ", (int)(t - e->th), t->sc->name, in->line);
    switch (in->op) {
    case I_CALL:
        k += snprintf(buf + k, sizeof buf - (size_t)k, "%d(", in->n);
        for (int i = 0; i < in->argc && k < 480; i++) {
            const Arg *a = &in->args[i];
            if (a->kind == A_NUM) k += snprintf(buf + k, sizeof buf - (size_t)k, "%s%d", i ? "," : "", a->num);
            else k += snprintf(buf + k, sizeof buf - (size_t)k, "%s%s=%s", i ? "," : "", a->s ? a->s : "", vm_str(e, a));
        }
        snprintf(buf + k, sizeof buf - (size_t)k, ")");
        break;
    case I_TEXT: snprintf(buf + k, sizeof buf - (size_t)k, "text %d", in->n); break;
    case I_ASSIGN: snprintf(buf + k, sizeof buf - (size_t)k, "%s=", in->var.s); break;
    default: return;
    }
    char *u = eg_sjis_to_utf8(buf);
    eg_log(e, "%s", u);
    free(u);
}

static void run_thread(EG *e, Thread *t) {
    int budget = 200000;
    while (t->active && t->wait == W_NONE && budget-- > 0 && !e->quit) {
        if (!t->sc || t->pc < 0 || t->pc >= t->sc->count) {
            thread_return(e, t);
            continue;
        }
        const Ins *in = &t->sc->ins[t->pc++];
        if (e->trace) trace_ins(e, t, in);
        switch (in->op) {
        case I_NOP:
            if (t->pc >= t->sc->count) thread_return(e, t);
            break;
        case I_JMP: t->pc = in->target; break;
        case I_IF:
            if (!eval(e, in->args, in->argc)) t->pc = in->target;
            break;
        case I_LOOP:
            if (eval(e, in->args, in->argc)) t->pc = in->target;
            break;
        case I_ASSIGN: vm_set_int(e, &in->var, eval(e, in->args, in->argc)); break;
        case I_TEXT:
            t->msgOff = t->pc >= 2 && t->sc->ins[t->pc - 2].op == I_NAME ? t->sc->ins[t->pc - 2].off : in->off;
            msg_show(e, in->n, in->s);
            t->wait = W_TEXT;
            break;
        case I_NAME: msg_name(e, in->s, in->s2); break;
        case I_CALL:
            if (in->n == 43) {
                thread_return(e, t);
                break;
            }
            cmd_exec(e, t, in);
            break;
        default: break;
        }
    }
    if (budget <= 0) {
        eg_log(e, "thread %d ran too long in %s", (int)(t - e->th), t->sc ? t->sc->name : "-");
        t->wait = W_FRAMES;
        t->frames = 1;
    }
}

void vm_run(EG *e) {
    for (int i = 0; i < EG_THREADS; i++) {
        Thread *t = &e->th[i];
        if (!t->active) continue;
        switch (t->wait) {
        case W_FRAMES:
            if (--t->frames <= 0) t->wait = W_NONE;
            break;
        case W_TIME:
            if ((i32)(e->now - t->until) >= 0) t->wait = W_NONE;
            break;
        case W_TRANS:
            if (gfx_transition_done(e)) t->wait = W_NONE;
            break;
        default: break;
        }
        if (t->wait != W_NONE) continue;
        e->cur = t;
        run_thread(e, t);
        e->cur = NULL;
    }
}

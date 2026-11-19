#include "age.h"

#include <stdarg.h>
#include <strings.h>

extern const u8 ag_oplen[0x400];
static OpFn ops[0x400];

static u8 drawsOp[0x400];

void vm_register(int op, OpFn fn) { ops[op] = fn; }
void vm_register_draw(int op, OpFn fn) { ops[op] = fn; drawsOp[op] = 1; }
bool vm_draws(u32 op) { return op < 0x400 && drawsOp[op]; }

void vm_problem(AG *g, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    Frame *f = &g->fr[g->top];
    ag_log(g, "problem: %s (%s@%#x)", buf, f->sc ? f->sc->name : "-", f->pc);
    if (!g->problem) g->problem = ag_fmt("%s (%s@%#x)", buf, f->sc ? f->sc->name : "-", f->pc);
}

u32 vm_now(AG *g) { return g->now; }

void str_set(Str *s, const char *v) {
    size_t n = strlen(v);
    char *d = ag_alloc(n + 1);
    memcpy(d, v, n);
    free(s->s);
    s->s = d;
    s->len = (u32)n;
}

static Frame *cur(AG *g) { return &g->fr[g->top]; }

u32 arg_type(AG *g, int n) { return g->ins[(n - 1) * 2]; }
u32 arg_raw(AG *g, int n) { return g->ins[(n - 1) * 2 + 1]; }

static i32 dummy_int;
static Str dummy_str;

i32 *ref_int(AG *g, Ref r) {
    if (r.bank == BANK_GINT) {
        if (r.index >= 0 && (u32)r.index < g->ngi) return &g->gi[r.index];
    } else if (r.bank == BANK_LINT) {
        Frame *f = &g->fr[r.frame];
        if (r.index >= 0 && (u32)r.index < f->nli) return &f->li[r.index];
    }
    dummy_int = 0;
    return &dummy_int;
}

Str *ref_str(AG *g, Ref r) {
    if (r.bank == BANK_GSTR) {
        if (r.index >= 0 && (u32)r.index < g->ngs) return &g->gs[r.index];
    } else if (r.bank == BANK_LSTR) {
        Frame *f = &g->fr[r.frame];
        if (r.index >= 0 && (u32)r.index < f->nls) return &f->ls[r.index];
    }
    memset(&dummy_str, 0, sizeof dummy_str);
    return &dummy_str;
}

static Ref *local_ref(AG *g, u32 type, u32 v) {
    Frame *f = cur(g);
    if (type == 0xc && v < f->nlir) return &f->lir[v];
    if (type == 0xe && v < f->nlsr) return &f->lsr[v];
    return NULL;
}

static i32 atol_native(const char *s) {
    while (*s == ' ' || (*s >= 9 && *s <= 13)) s++;
    int neg = 0;
    if (*s == '-' || *s == '+') neg = *s++ == '-';
    i64 v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s++ - '0');
        if (v > 0x80000000LL) v = 0x80000000LL;
    }
    if (neg) v = -v;
    if (v > 0x7fffffff) v = 0x7fffffff;
    return (i32)v;
}

i32 arg_i(AG *g, int n) {
    u32 t = arg_type(g, n), v = arg_raw(g, n);
    Frame *f = cur(g);
    switch (t) {
    case 0: return (i32)v;
    case 1: { float x; memcpy(&x, &v, 4); return (i32)x; }
    case 2: return atol_native(script_string(f->sc, v));
    case 3: return v < g->ngi ? g->gi[v] : 0;
    case 5: return v < g->ngs && g->gs[v].s ? atol_native(g->gs[v].s) : 0;
    case 9: return v < f->nli ? f->li[v] : 0;
    case 0xb: return v < f->nls && f->ls[v].s ? atol_native(f->ls[v].s) : 0;
    case 0xc: { Ref *r = local_ref(g, t, v); return r ? *ref_int(g, *r) : 0; }
    default: vm_problem(g, "operand %d of op %#x reads type %#x", n, g->insOp, t); return 0;
    }
}

static void fullwidth(char *out, size_t size, const char *ascii) {
    size_t o = 0;
    for (const char *p = ascii; *p && o + 2 < size; p++) {
        u8 c = (u8)*p;
        u16 w;
        if (c >= '0' && c <= '9') w = (u16)(0x824f + (c - '0'));
        else if (c >= 'A' && c <= 'Z') w = (u16)(0x8260 + (c - 'A'));
        else if (c >= 'a' && c <= 'z') w = (u16)(0x8281 + (c - 'a'));
        else if (c == '-') w = 0x817c;
        else w = 0x8148;
        out[o++] = (char)(w >> 8);
        out[o++] = (char)(w & 0xff);
    }
    out[o] = 0;
}

char *arg_s(AG *g, int n) {
    static char buf[4][128];
    static int which;
    u32 t = arg_type(g, n), v = arg_raw(g, n);
    Frame *f = cur(g);
    switch (t) {
    case 2: return (char *)script_string(f->sc, v);
    case 5: return v < g->ngs && g->gs[v].s ? g->gs[v].s : "";
    case 0xb: return v < f->nls && f->ls[v].s ? f->ls[v].s : "";
    case 0xe: { Ref *r = local_ref(g, t, v); Str *s = r ? ref_str(g, *r) : NULL; return s && s->s ? s->s : ""; }
    default: {
        char a[32];
        snprintf(a, sizeof a, "%d", arg_i(g, n));
        char *o = buf[which = (which + 1) & 3];
        fullwidth(o, sizeof buf[0], a);
        return o;
    }
    }
}

void arg_set_i(AG *g, int n, i32 val) {
    u32 t = arg_type(g, n), v = arg_raw(g, n);
    Frame *f = cur(g);
    switch (t) {
    case 3: if (v < g->ngi) g->gi[v] = val; return;
    case 9: if (v < f->nli) f->li[v] = val; return;
    case 0xc: { Ref *r = local_ref(g, t, v); if (r) *ref_int(g, *r) = val; return; }
    case 5: case 0xb: case 0xe: {
        char a[32];
        snprintf(a, sizeof a, "%d", val);
        arg_set_s(g, n, a);
        return;
    }
    default: vm_problem(g, "operand %d of op %#x writes type %#x", n, g->insOp, t);
    }
}

void arg_set_s(AG *g, int n, const char *s) {
    u32 t = arg_type(g, n), v = arg_raw(g, n);
    Frame *f = cur(g);
    char *copy = ag_strdup(s);
    switch (t) {
    case 5: if (v < g->ngs) str_set(&g->gs[v], copy); break;
    case 0xb: if (v < f->nls) str_set(&f->ls[v], copy); break;
    case 0xe: { Ref *r = local_ref(g, t, v); if (r) str_set(ref_str(g, *r), copy); break; }
    case 3: case 9: case 0xc: arg_set_i(g, n, atol_native(copy)); break;
    default: vm_problem(g, "operand %d of op %#x writes a string to type %#x", n, g->insOp, t);
    }
    free(copy);
}

bool arg_addr(AG *g, int n, Ref *r) {
    u32 t = arg_type(g, n), v = arg_raw(g, n);
    r->frame = (i16)g->top;
    r->index = (i32)v;
    switch (t) {
    case 3: r->bank = BANK_GINT; return true;
    case 9: r->bank = BANK_LINT; return true;
    case 5: r->bank = BANK_GSTR; return true;
    case 0xb: r->bank = BANK_LSTR; return true;
    case 0xc: case 0xe: { Ref *p = local_ref(g, t, v); if (!p) return false; *r = *p; return true; }
    default: vm_problem(g, "operand %d of op %#x has no address (type %#x)", n, g->insOp, t); return false;
    }
}

static void frame_free(Frame *f) {
    script_free(f->sc);
    free(f->li);
    if (f->ls) for (u32 i = 0; i < f->nls; i++) free(f->ls[i].s);
    free(f->ls);
    free(f->lir);
    free(f->lsr);
    memset(f, 0, sizeof *f);
}

static bool frame_load(AG *g, Frame *f, u32 id) {
    char err[256];
    Script *s = script_load(&g->st, id, err, sizeof err);
    if (!s) { vm_problem(g, "%s", err); return false; }
    frame_free(f);
    f->sc = s;
    f->nli = s->hdr[0] + 1;
    f->nls = s->hdr[2] + 1;
    f->nlir = s->hdr[3] + 1;
    f->nlsr = s->hdr[5] + 1;
    f->li = ag_alloc(sizeof(i32) * f->nli);
    f->ls = ag_alloc(sizeof(Str) * f->nls);
    f->lir = ag_alloc(sizeof(Ref) * f->nlir);
    f->lsr = ag_alloc(sizeof(Ref) * f->nlsr);
    f->active = true;
    f->launchReturn = 0;
    f->preload = -1;
    f->handlerA = f->handlerB = 0xffffffffu;
    return true;
}

bool vm_preload(AG *g, u32 id, int slot) {
    if (slot < 0 || slot >= AG_FRAMES) return false;
    return frame_load(g, &g->pre[slot], id);
}

bool vm_call_preloaded(AG *g, int slot) {
    if (slot < 0 || slot >= AG_FRAMES || !g->pre[slot].sc) { vm_problem(g, "preloaded slot %d is empty", slot); return false; }
    if (g->top + 1 >= AG_FRAMES - 3) { vm_problem(g, "script calls nest too deep"); return false; }
    Frame *f = &g->fr[g->top + 1];
    frame_free(f);
    *f = g->pre[slot];
    memset(&g->pre[slot], 0, sizeof g->pre[slot]);
    f->pc = 0;
    f->rsp = 0;
    f->preload = slot;
    g->top++;
    return true;
}

bool vm_call_script(AG *g, u32 id) {
    if (g->top + 1 >= AG_FRAMES - 3) { vm_problem(g, "script calls nest too deep"); g->halted = true; return false; }
    Frame *f = &g->fr[g->top + 1];
    if (!frame_load(g, f, id)) { g->halted = true; return false; }
    g->top++;
    ag_log(g, "call %s (%#x) depth %d", f->sc->name, id, g->top);
    return true;
}

void vm_jump(AG *g, u32 pc) { cur(g)->pc = pc; }

void vm_call_local(AG *g, u32 pc) {
    Frame *f = cur(g);
    if (f->rsp >= AG_RET) { vm_problem(g, "return stack overflow"); g->halted = true; return; }
    f->ret[f->rsp++] = f->pc + g->insLen;
    f->pc = pc;
}

void vm_callback(AG *g, u32 pc) {
    Frame *f = cur(g);
    if (f->rsp >= AG_RET) { vm_problem(g, "return stack overflow"); g->halted = true; return; }
    f->ret[f->rsp++] = f->pc;
    f->pc = pc;
}

static void banks_alloc(AG *g) {
    g->ngi = g->st.bankFull[0] + 1;
    g->ngs = g->st.bankFull[2] + 1;
    g->gi = ag_alloc(sizeof(i32) * g->ngi);
    g->gs = ag_alloc(sizeof(Str) * g->ngs);
}

static void banks_clear(AG *g) {
    memset(g->gi, 0, sizeof(i32) * g->ngi);
    for (u32 i = 0; i < g->ngs; i++) { free(g->gs[i].s); g->gs[i].s = NULL; g->gs[i].len = 0; }
}

void vm_reset_root(AG *g) {
    for (int i = 0; i < AG_FRAMES; i++) { frame_free(&g->fr[i]); frame_free(&g->pre[i]); }
    banks_clear(g);
    g->top = 0;
    g->wait = W_NONE;
    g->blockDepth = 0;
    if (!frame_load(g, &g->fr[0], 0)) g->halted = true;
}

bool vm_init(AG *g) {
    const char *tr = getenv("AG_TRACE");
    for (const char *p = tr; p && *p;) {
        char *e;
        long v = strtol(p, &e, 16);
        if (e == p) break;
        if (v >= 0 && v < 0x400) g->trace[v] = 1;
        p = *e ? e + 1 : e;
    }
    banks_alloc(g);
    g->initialRun = true;
    g->rng = 1;
    vm_register_all();
    g->top = 0;
    return frame_load(g, &g->fr[0], 0);
}

void vm_close(AG *g) {
    for (int i = 0; i < AG_FRAMES; i++) { frame_free(&g->fr[i]); frame_free(&g->pre[i]); }
    if (g->gs) for (u32 i = 0; i < g->ngs; i++) free(g->gs[i].s);
    free(g->gs);
    free(g->gi);
}

int vm_frame_exit(AG *g) {
    if (g->top == 0) {
        ag_log(g, "root script ended");
        g->halted = true;
        return OP_HALT;
    }
    bool launched = g->fr[g->top].launchReturn;
    Frame *f = &g->fr[g->top];
    if (f->preload >= 0 && f->preload < AG_FRAMES) {
        g->pre[f->preload] = *f;
        memset(f, 0, sizeof *f);
    } else frame_free(f);
    g->top--;
    if (launched && g->launchHead < g->launchCount) {
        u32 id = g->launch[g->launchHead++];
        if (vm_call_script(g, id)) g->fr[g->top].launchReturn = 1;
    }
    return OP_STAY;
}

void vm_run(AG *g, int budget) {
    while (!g->halted && g->wait == W_NONE && budget-- > 0) {
        int fi = g->top;
        Frame *f = &g->fr[fi];
        if (!f->sc || f->pc >= f->sc->codeEnd) {
            vm_problem(g, "ran off the end of the code");
            g->halted = true;
            break;
        }
        u32 op = f->sc->w[f->pc];
        if (op >= 0x400 || !ag_oplen[op]) {
            vm_problem(g, "unknown opcode %#x", op);
            g->halted = true;
            break;
        }
        g->ins = &f->sc->w[f->pc + 1];
        g->insOp = op;
        u32 len = 2u * ag_oplen[op] - 1;
        g->insLen = len;
        g->opSeen[op]++;
        if (g->trace[op]) {
            char b[512];
            int n = snprintf(b, sizeof b, "op %#x %s@%#x:", op, f->sc->name, f->pc);
            for (u32 a = 0; a + 1 < len && n < 480; a += 2) {
                u32 t = g->ins[a], v = g->ins[a + 1];
                int idx = (int)a / 2 + 1;
                if (t == 2) n += snprintf(b + n, sizeof b - (size_t)n, " \"%.40s\"", script_string(f->sc, v));
                else n += snprintf(b + n, sizeof b - (size_t)n, " %d", arg_i(g, idx));
            }
            ag_log(g, "%s", b);
        }
        int r;
        if (ops[op]) r = ops[op](g);
        else {
            if (!g->opMissing[op]++) ag_log(g, "missing op %#x at %s@%#x", op, f->sc->name, f->pc);
            r = OP_NEXT;
        }
        g->steps++;
        if (drawsOp[op]) g->gfx.hold = false;
        if (r == OP_WAIT) { g->waitPc = g->fr[fi].pc; g->waitFrame = fi; }
        if (r == OP_NEXT || r == OP_WAIT) g->fr[fi].pc += len;
        else if (r == OP_HALT) g->halted = true;
    }
}

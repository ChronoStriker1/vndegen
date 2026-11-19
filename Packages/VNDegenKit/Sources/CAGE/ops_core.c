#include "age.h"

#include <strings.h>

static Frame *cur(AG *g) { return &g->fr[g->top]; }

static int op_mov(AG *g) {
    u32 t = arg_type(g, 2);
    if (t == 2 || t == 5 || t == 0xb || t == 0xe) {
        u32 d = arg_type(g, 1);
        if (d == 5 || d == 0xb || d == 0xe) { arg_set_s(g, 1, arg_s(g, 2)); return OP_NEXT; }
    }
    arg_set_i(g, 1, arg_i(g, 2));
    return OP_NEXT;
}

#define BIN(name, expr)                                                                                               \
    static int name(AG *g) {                                                                                          \
        i32 a = arg_i(g, 2), b = arg_i(g, 3);                                                                         \
        arg_set_i(g, 1, (expr));                                                                                      \
        return OP_NEXT;                                                                                               \
    }

BIN(op_add, (i32)((u32)a + (u32)b))
BIN(op_sub, (i32)((u32)a - (u32)b))
BIN(op_mul, (i32)((u32)a * (u32)b))
BIN(op_div, b == 0 ? 0 : (a == INT32_MIN && b == -1) ? a : a / b)
BIN(op_mod, b == 0 ? 0 : (a == INT32_MIN && b == -1) ? 0 : a % b)
BIN(op_and, a & b)
BIN(op_or, a | b)
BIN(op_sar, a >> (b & 31))
BIN(op_shl, (i32)((u32)a << (b & 31)))
BIN(op_eq, a == b)
BIN(op_ne, a != b)
BIN(op_lt, a < b)
BIN(op_lte, a <= b)
BIN(op_gr, a > b)
BIN(op_gre, a >= b)

static int op_jmp(AG *g) {
    vm_jump(g, (u32)arg_i(g, 1));
    return OP_JUMP;
}

static int op_jcc(AG *g) {
    i32 c = arg_i(g, 1);
    i32 t = c ? arg_i(g, 2) : arg_i(g, 3);
    if (t == -1) return OP_NEXT;
    vm_jump(g, (u32)t);
    return OP_JUMP;
}

static int op_call(AG *g) {
    i32 t = arg_i(g, 1);
    if (t == -1) return OP_NEXT;
    vm_call_local(g, (u32)t);
    return OP_JUMP;
}

static int op_ret(AG *g) {
    Frame *f = cur(g);
    if (f->rsp <= 0) { vm_problem(g, "return with an empty stack"); return OP_HALT; }
    u32 pc = f->ret[--f->rsp];
    if (pc == 0xffffffff) return OP_NEXT;
    f->pc = pc;
    return OP_JUMP;
}

static int op_exit(AG *g) { return vm_frame_exit(g); }

static int op_call_script(AG *g) {
    u32 id = (u32)arg_i(g, 1);
    Frame *f = cur(g);
    f->t2 = script_table_index(f->sc->t2, f->sc->t2Count, f->pc);
    f->pc += g->insLen;
    if (!vm_call_script(g, id)) return OP_HALT;
    return OP_STAY;
}

static int op_nop(AG *g) { (void)g; return OP_NEXT; }

static int op_block_begin(AG *g) { g->blockDepth++; return OP_NEXT; }
static int op_block_end(AG *g) { if (g->blockDepth > 0) g->blockDepth--; return OP_NEXT; }

static int op_lookup(AG *g) {
    Ref base;
    if (!arg_addr(g, 2, &base)) return OP_NEXT;
    base.index += arg_i(g, 3);
    u32 t = arg_type(g, 1), v = arg_raw(g, 1);
    Frame *f = cur(g);
    if (t == 0xc && v < f->nlir) f->lir[v] = base;
    else if (t == 0xe && v < f->nlsr) f->lsr[v] = base;
    else vm_problem(g, "lookup binds type %#x", t);
    return OP_NEXT;
}

static int op_lookup2(AG *g) {
    Ref base;
    if (!arg_addr(g, 2, &base)) return OP_NEXT;
    base.index += arg_i(g, 3) * arg_i(g, 4) + arg_i(g, 5);
    u32 t = arg_type(g, 1), v = arg_raw(g, 1);
    Frame *f = cur(g);
    if (t == 0xc && v < f->nlir) f->lir[v] = base;
    else if (t == 0xe && v < f->nlsr) f->lsr[v] = base;
    else vm_problem(g, "lookup binds type %#x", t);
    return OP_NEXT;
}

static int op_take_address(AG *g) {
    Ref r;
    if (!arg_addr(g, 2, &r)) return OP_NEXT;
    u32 t = arg_type(g, 1), v = arg_raw(g, 1);
    Frame *f = cur(g);
    if (t == 0xc && v < f->nlir) f->lir[v] = r;
    else if (t == 0xe && v < f->nlsr) f->lsr[v] = r;
    return OP_NEXT;
}

static int op_inline_array(AG *g) {
    Ref r;
    if (!arg_addr(g, 1, &r)) return OP_NEXT;
    Script *s = cur(g)->sc;
    u32 off = (u32)arg_i(g, 2);
    if (off >= s->count) return OP_NEXT;
    u32 n = s->w[off];
    for (u32 i = 0; i < n && off + 1 + i < s->count; i++) {
        Ref c = r;
        c.index += (i32)i;
        *ref_int(g, c) = (i32)s->w[off + 1 + i];
    }
    return OP_NEXT;
}

static int op_zero_range(AG *g) {
    Ref r;
    if (!arg_addr(g, 1, &r)) return OP_NEXT;
    i32 n = arg_i(g, 2);
    for (i32 i = 0; i < n; i++) { Ref c = r; c.index += i; *ref_int(g, c) = 0; }
    return OP_NEXT;
}

static int op_fill(AG *g) {
    Ref r;
    if (!arg_addr(g, 1, &r)) return OP_NEXT;
    i32 v = arg_i(g, 2), n = arg_i(g, 3);
    for (i32 i = 0; i < n; i++) { Ref c = r; c.index += i; *ref_int(g, c) = v; }
    return OP_NEXT;
}

static int op_copy_dwords(AG *g) {
    Ref a, b;
    if (!arg_addr(g, 1, &a) || !arg_addr(g, 2, &b)) return OP_NEXT;
    i32 n = arg_i(g, 3);
    if (n <= 0) return OP_NEXT;
    i32 *tmp = ag_alloc(sizeof(i32) * (size_t)n);
    for (i32 i = 0; i < n; i++) { Ref c = a; c.index += i; tmp[i] = *ref_int(g, c); }
    for (i32 i = 0; i < n; i++) { Ref c = b; c.index += i; *ref_int(g, c) = tmp[i]; }
    free(tmp);
    return OP_NEXT;
}

static int op_bit_set(AG *g) {
    i32 v = arg_i(g, 1), b = arg_i(g, 2);
    arg_set_i(g, 1, (i32)((u32)v | 1u << (b & 31)));
    return OP_NEXT;
}

static int op_bit_reset(AG *g) {
    i32 v = arg_i(g, 1), b = arg_i(g, 2);
    arg_set_i(g, 1, (i32)((u32)v & ~(1u << (b & 31))));
    return OP_NEXT;
}

static int op_bit_check(AG *g) {
    i32 v = arg_i(g, 2), b = arg_i(g, 3);
    arg_set_i(g, 1, (i32)(((u32)v >> (b & 31)) & 1));
    return OP_NEXT;
}

static int op_abs(AG *g) {
    i32 v = arg_i(g, 2);
    arg_set_i(g, 1, v < 0 ? (i32)(0u - (u32)v) : v);
    return OP_NEXT;
}

static int op_random(AG *g) {
    i32 b = arg_i(g, 2);
    g->rng = g->rng * 214013u + 2531011u;
    i32 r = (i32)((g->rng >> 16) & 0x7fff);
    if (b == 0) { arg_set_i(g, 1, 0); vm_problem(g, "random with a zero bound"); return OP_NEXT; }
    arg_set_i(g, 1, r % b);
    return OP_NEXT;
}

static int op_set_string(AG *g) { arg_set_s(g, 1, arg_s(g, 2)); return OP_NEXT; }

static int op_concat(AG *g) {
    char *a = ag_strdup(arg_s(g, 2));
    char *b = ag_strdup(arg_s(g, 3));
    char *c = ag_fmt("%s%s", a, b);
    arg_set_s(g, 1, c);
    free(a);
    free(b);
    free(c);
    return OP_NEXT;
}

static int op_str_eq(AG *g) {
    char *a = ag_strdup(arg_s(g, 2));
    int eq = !strcmp(a, arg_s(g, 3));
    free(a);
    arg_set_i(g, 1, eq);
    return OP_NEXT;
}

static int op_str_ne(AG *g) {
    char *a = ag_strdup(arg_s(g, 2));
    int eq = !strcmp(a, arg_s(g, 3));
    free(a);
    arg_set_i(g, 1, !eq);
    return OP_NEXT;
}

static int op_half_len(AG *g) { arg_set_i(g, 1, (i32)(strlen(arg_s(g, 2)) / 2)); return OP_NEXT; }
static int op_byte_len(AG *g) { arg_set_i(g, 1, (i32)strlen(arg_s(g, 2))); return OP_NEXT; }

static int op_to_string(AG *g) {
    char a[32];
    snprintf(a, sizeof a, "%d", arg_i(g, 2));
    arg_set_s(g, 1, a);
    return OP_NEXT;
}

static int op_time(AG *g) { arg_set_i(g, 1, (i32)vm_now(g)); return OP_NEXT; }

static int op_initial_run(AG *g) { arg_set_i(g, 1, g->initialRun); return OP_NEXT; }

static int op_reset_root(AG *g) {
    g->initialRun = false;
    vm_reset_root(g);
    return OP_STAY;
}

static int op_exit_request(AG *g) {
    ag_log(g, "exit request");
    g->quit = true;
    return OP_HALT;
}

static int op_append_autoruns(AG *g) {
    g->launchCount = g->launchHead = 0;
    for (int sel = 1; sel < 256; sel++)
        if (g->st.cat[sel].count > 0) g->launch[g->launchCount++] = (u32)sel << 24;
    cur(g)->pc += g->insLen;
    if (g->launchHead < g->launchCount) {
        u32 id = g->launch[g->launchHead++];
        if (vm_call_script(g, id)) g->fr[g->top].launchReturn = 1;
    }
    return OP_STAY;
}

static void switch_clear(Switch *s) {
    for (int i = 0; i < s->count; i++) free(s->keys[i]);
    free(s->keys);
    free(s->pcs);
    memset(s, 0, sizeof *s);
}

static int op_switch_begin(AG *g) { switch_clear(&g->sw); return OP_NEXT; }

static int op_switch_case(AG *g) {
    Switch *s = &g->sw;
    s->keys = ag_realloc(s->keys, sizeof(char *) * (size_t)(s->count + 1));
    s->pcs = ag_realloc(s->pcs, sizeof(u32) * (size_t)(s->count + 1));
    s->keys[s->count] = ag_strdup(arg_s(g, 1));
    s->pcs[s->count] = (u32)arg_i(g, 2);
    s->count++;
    return OP_NEXT;
}

static int op_switch_jump(AG *g) {
    const char *k = arg_s(g, 1);
    Switch *s = &g->sw;
    for (int i = 0; i < s->count; i++)
        if (!strcmp(s->keys[i], k)) { vm_jump(g, s->pcs[i]); return OP_JUMP; }
    i32 d = arg_i(g, 2);
    if (d == -1) return OP_NEXT;
    vm_jump(g, (u32)d);
    return OP_JUMP;
}

static Queue *queue(AG *g, Queue *set, i32 id) {
    (void)g;
    if (id < 0 || id > 10) return NULL;
    return &set[id];
}

static int op_queue_reset(AG *g) {
    Queue *q = queue(g, g->q, arg_i(g, 1));
    if (q) q->head = q->count = 0;
    return OP_NEXT;
}

static void queue_push(Queue *q, i32 v) {
    if (q->head + q->count >= q->cap) {
        if (q->head) { memmove(q->v, q->v + q->head, sizeof(i32) * (size_t)q->count); q->head = 0; }
        if (q->count >= q->cap) { q->cap = q->cap ? q->cap * 2 : 64; q->v = ag_realloc(q->v, sizeof(i32) * (size_t)q->cap); }
    }
    q->v[q->head + q->count++] = v;
}

static int op_queue_push(AG *g) {
    Queue *q = queue(g, g->q, arg_i(g, 1));
    if (q) queue_push(q, arg_i(g, 2));
    return OP_NEXT;
}

static int op_queue_pop(AG *g) {
    Queue *q = queue(g, g->q, arg_i(g, 1));
    if (!q || !q->count) { arg_set_i(g, 2, 0); return OP_NEXT; }
    i32 v = q->v[q->head++];
    q->count--;
    arg_set_i(g, 2, 1);
    arg_set_i(g, 3, v);
    return OP_NEXT;
}

static int op_stack_reset(AG *g) {
    Queue *q = queue(g, g->stk, arg_i(g, 1));
    if (q) q->head = q->count = 0;
    return OP_NEXT;
}

static int op_stack_push(AG *g) {
    Queue *q = queue(g, g->stk, arg_i(g, 1));
    if (q) queue_push(q, arg_i(g, 2));
    return OP_NEXT;
}

static int op_stack_pop(AG *g) {
    Queue *q = queue(g, g->stk, arg_i(g, 1));
    if (!q || !q->count) { arg_set_i(g, 2, 0); return OP_NEXT; }
    i32 v = q->v[q->head + --q->count];
    arg_set_i(g, 2, 1);
    arg_set_i(g, 3, v);
    return OP_NEXT;
}

static int op_sort(AG *g) {
    Ref o, a, b;
    if (!arg_addr(g, 1, &o) || !arg_addr(g, 2, &a) || !arg_addr(g, 3, &b)) return OP_NEXT;
    i32 n = arg_i(g, 4);
    if (n <= 0) return OP_NEXT;
    i32 *idx = ag_alloc(sizeof(i32) * (size_t)n);
    i32 *key = ag_alloc(sizeof(i32) * (size_t)n);
    for (i32 i = 0; i < n; i++) {
        Ref ra = a, rb = b;
        ra.index += i;
        rb.index += i;
        idx[i] = i;
        key[i] = (i32)((u32)*ref_int(g, ra) + (u32)*ref_int(g, rb));
    }
    for (i32 i = 1; i < n; i++) {
        i32 v = idx[i], j = i - 1;
        while (j >= 0 && key[idx[j]] > key[v]) { idx[j + 1] = idx[j]; j--; }
        idx[j + 1] = v;
    }
    for (i32 i = 0; i < n; i++) { Ref r = o; r.index += i; *ref_int(g, r) = idx[i]; }
    free(idx);
    free(key);
    return OP_NEXT;
}

static int op_mark_frame(AG *g) { (void)g; return OP_NEXT; }

static int op_preload(AG *g) {
    if (!vm_preload(g, (u32)arg_i(g, 1), arg_i(g, 2))) return OP_HALT;
    return OP_NEXT;
}

static int op_call_preloaded(AG *g) {
    i32 slot = arg_i(g, 1);
    cur(g)->pc += g->insLen;
    if (!vm_call_preloaded(g, slot)) return OP_HALT;
    return OP_STAY;
}

static ProfInt *pint(AG *g, u32 cell, bool create) {
    for (int i = 0; i < g->npint; i++) if (g->pint[i].cell == cell) return &g->pint[i];
    if (!create) return NULL;
    if (g->npint >= g->cpint) { g->cpint = g->cpint ? g->cpint * 2 : 256; g->pint = ag_realloc(g->pint, sizeof(ProfInt) * (size_t)g->cpint); }
    ProfInt *p = &g->pint[g->npint++];
    p->cell = cell;
    p->v = 0;
    return p;
}

static ProfStr *pstr(AG *g, u32 cell, bool create) {
    for (int i = 0; i < g->npstr; i++) if (g->pstr[i].cell == cell) return &g->pstr[i];
    if (!create) return NULL;
    if (g->npstr >= g->cpstr) { g->cpstr = g->cpstr ? g->cpstr * 2 : 64; g->pstr = ag_realloc(g->pstr, sizeof(ProfStr) * (size_t)g->cpstr); }
    ProfStr *p = &g->pstr[g->npstr++];
    p->cell = cell;
    p->v = NULL;
    return p;
}

static int op_prof_store(AG *g) {
    Ref r;
    if (!arg_addr(g, 1, &r) || r.bank != BANK_GINT) return OP_NEXT;
    pint(g, (u32)r.index, true)->v = *ref_int(g, r);
    return OP_NEXT;
}

static int op_prof_load(AG *g) {
    Ref r;
    if (!arg_addr(g, 1, &r) || r.bank != BANK_GINT) return OP_NEXT;
    ProfInt *p = pint(g, (u32)r.index, false);
    *ref_int(g, r) = p ? p->v : 0;
    return OP_NEXT;
}

static int op_prof_store_s(AG *g) {
    Ref r;
    if (!arg_addr(g, 1, &r) || r.bank != BANK_GSTR) return OP_NEXT;
    ProfStr *p = pstr(g, (u32)r.index, true);
    free(p->v);
    Str *s = ref_str(g, r);
    p->v = ag_strdup(s->s ? s->s : "");
    return OP_NEXT;
}

static int op_prof_load_s(AG *g) {
    Ref r;
    if (!arg_addr(g, 1, &r) || r.bank != BANK_GSTR) return OP_NEXT;
    ProfStr *p = pstr(g, (u32)r.index, false);
    str_set(ref_str(g, r), p && p->v ? p->v : "");
    return OP_NEXT;
}

static int op_restore_continue(AG *g) { (void)g; return OP_NEXT; }

static int op_co_handlers(AG *g) {
    Frame *f = cur(g);
    f->handlerA = (u32)arg_i(g, 1);
    f->handlerB = (u32)arg_i(g, 2);
    return OP_NEXT;
}

static int op_co_yield(AG *g) {
    Frame *f = cur(g);
    if (g->coState == 0 && f->handlerA != 0xffffffffu) { co_enter(g, g->top, f->handlerA, f->pc + g->insLen, 1); return OP_JUMP; }
    if (f->handlerB != 0xffffffffu) { co_enter(g, g->top, f->handlerB, f->pc + g->insLen, 3); return OP_JUMP; }
    return OP_NEXT;
}

static int op_co_resume(AG *g) {
    Frame *f = cur(g);
    if (g->coState == 1) {
        f->pc = g->coResume;
        g->coState = 2;
        g->wait = W_HIDDEN;
        g->in.hit = -1;
        return OP_STAY;
    }
    if (g->coState == 3) {
        f->pc = g->coResume;
        g->coState = 0;
        g->in.hit = -1;
        return OP_JUMP;
    }
    vm_problem(g, "coroutine resume outside a yield");
    return OP_NEXT;
}

static int op_save_mark(AG *g) { g->saveFrame = g->top; return OP_NEXT; }

static int op_timed_begin(AG *g) {
    g->ntimed = 0;
    g->timedCursor = 0;
    g->timedRunning = false;
    return OP_NEXT;
}

static int op_timed_append(AG *g) {
    i32 iv = arg_i(g, 1), n = arg_i(g, 2);
    u32 on = (u32)arg_i(g, 3), cu = (u32)arg_i(g, 4);
    u32 at = g->ntimed ? g->timed[g->ntimed - 1].at : 0;
    for (i32 i = 0; i < n; i++) {
        if (g->ntimed >= g->ctimed) { g->ctimed = g->ctimed ? g->ctimed * 2 : 64; g->timed = ag_realloc(g->timed, sizeof(TimedEntry) * (size_t)g->ctimed); }
        at += (u32)(iv > 0 ? iv : 0);
        g->timed[g->ntimed].at = at;
        g->timed[g->ntimed].ontime = on;
        g->timed[g->ntimed].catchup = cu;
        g->ntimed++;
    }
    return OP_NEXT;
}

static int op_timed_run(AG *g) {
    if (!g->timedRunning) {
        g->timedRunning = true;
        g->timedCursor = 0;
        g->timedStart = vm_now(g);
    }
    if (g->timedCursor >= g->ntimed - 1) {
        g->timedRunning = false;
        g->ntimed = 0;
        g->timedCursor = 0;
        return OP_NEXT;
    }
    TimedEntry *e = &g->timed[g->timedCursor];
    u32 el = vm_now(g) - g->timedStart;
    if (el < e->at && !g->in.skip) {
        g->wait = W_TIMED;
        g->waitUntil = g->timedStart + e->at;
        return OP_STAY;
    }
    bool late = el >= g->timed[g->timedCursor + 1].at;
    u32 target = late ? e->catchup : e->ontime;
    g->timedCursor++;
    if (target == 0xffffffffu) return OP_STAY;
    vm_callback(g, target);
    return OP_JUMP;
}

void ops_core_register(void) {
    vm_register(0x1, op_exit_request);
    vm_register(0x2, op_exit);
    vm_register(0x3, op_call_script);
    vm_register(0x5, op_ret);
    vm_register(0x9, op_reset_root);
    vm_register(0x50, op_add);
    vm_register(0x51, op_sub);
    vm_register(0x52, op_mul);
    vm_register(0x53, op_div);
    vm_register(0x54, op_mod);
    vm_register(0x55, op_mov);
    vm_register(0x56, op_and);
    vm_register(0x57, op_or);
    vm_register(0x58, op_sar);
    vm_register(0x59, op_shl);
    vm_register(0x5a, op_eq);
    vm_register(0x5b, op_ne);
    vm_register(0x5c, op_lt);
    vm_register(0x5d, op_lte);
    vm_register(0x5e, op_gr);
    vm_register(0x5f, op_gre);
    vm_register(0x60, op_random);
    vm_register(0x61, op_lookup);
    vm_register(0x63, op_take_address);
    vm_register(0x64, op_inline_array);
    vm_register(0x6c, op_zero_range);
    vm_register(0x8c, op_jmp);
    vm_register(0x8f, op_call);
    vm_register(0xa0, op_jcc);
    vm_register(0xa1, op_switch_begin);
    vm_register(0xa2, op_switch_case);
    vm_register(0xa3, op_switch_jump);
    vm_register(0xd0, op_time);
    vm_register(0x12c, op_lookup2);
    vm_register(0x12f, op_sort);
    vm_register(0x130, op_initial_run);
    vm_register(0x132, op_queue_reset);
    vm_register(0x133, op_queue_push);
    vm_register(0x134, op_queue_pop);
    vm_register(0x135, op_bit_set);
    vm_register(0x136, op_bit_reset);
    vm_register(0x137, op_stack_reset);
    vm_register(0x138, op_stack_push);
    vm_register(0x139, op_stack_pop);
    vm_register(0x13f, op_bit_check);
    vm_register(0x143, op_append_autoruns);
    vm_register(0x191, op_abs);
    vm_register(0x192, op_set_string);
    vm_register(0x193, op_concat);
    vm_register(0x194, op_str_eq);
    vm_register(0x195, op_str_ne);
    vm_register(0x1a6, op_half_len);
    vm_register(0x1a7, op_nop);
    vm_register(0x1a8, op_nop);
    vm_register(0x1b0, op_copy_dwords);
    vm_register(0x1c8, op_to_string);
    vm_register(0x1f4, op_block_begin);
    vm_register(0x1f5, op_block_end);
    vm_register(0x2c5, op_byte_len);
    vm_register(0x2d8, op_fill);
    (void)op_mark_frame;
    vm_register(0x6, op_preload);
    vm_register(0x8, op_call_preloaded);
    vm_register(0x7b, op_co_handlers);
    vm_register(0x199, op_co_yield);
    vm_register(0x7c, op_co_resume);
    vm_register(0x1ad, op_save_mark);
    vm_register(0xd3, op_timed_begin);
    vm_register(0xd4, op_timed_append);
    vm_register(0xd5, op_timed_run);
    vm_register(0x1a2, op_prof_store);
    vm_register(0x1a3, op_prof_load);
    vm_register(0x1a9, op_prof_store_s);
    vm_register(0x1aa, op_prof_load_s);
}

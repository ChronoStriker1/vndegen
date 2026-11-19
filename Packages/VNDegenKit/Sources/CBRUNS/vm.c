#include "br.h"

static i32 zero;

i32 *vm_word(BR *e, i32 idx) {
    if (idx < 0 || idx >= BR_WORDS) {
        zero = 0;
        return &zero;
    }
    return &e->word[idx];
}

static Frame *top(BR *e) { return e->sp > 0 ? &e->stack[e->sp - 1] : NULL; }

static i32 *local_ref(BR *e, i32 idx) {
    Frame *f = top(e);
    if (!f || idx < 0 || idx >= BR_LOCALS) {
        zero = 0;
        return &zero;
    }
    return &f->local[idx];
}

static i32 arg(BR *e, Node *n, int i) { return i < n->n ? vm_eval(e, n->k[i]) : 0; }

i32 vm_eval(BR *e, Node *n) {
    if (!n) return 0;
    if (n->t == N_INT) return n->i;
    if (n->t != N_LIST) return 0;
    i32 a, b, *r;
    switch (n->op) {
    case A_ASSIGN_WORD:
        a = arg(e, n, 1);
        b = arg(e, n, 2);
        return *vm_word(e, a) = b;
    case A_ASSIGN_LOCAL:
        a = arg(e, n, 1);
        b = arg(e, n, 2);
        return *local_ref(e, a) = b;
    case A_ASSIGN_ACC: return e->acc = arg(e, n, 1);
    case A_ASSIGN_REFLAG: return e->reflag = arg(e, n, 1);
    case A_IDREF_WORD: return *vm_word(e, arg(e, n, 1));
    case A_IDREF_LOCAL: return *local_ref(e, arg(e, n, 1));
    case A_IDREF_ACC: return e->acc;
    case A_IDREF_REFLAG: return e->reflag;
    case A_PRE_INCR_WORD: r = vm_word(e, arg(e, n, 1)); return ++*r;
    case A_PRE_INCR_LOCAL: r = local_ref(e, arg(e, n, 1)); return ++*r;
    case A_PRE_INCR_ACC: return ++e->acc;
    case A_PRE_DECR_WORD: r = vm_word(e, arg(e, n, 1)); return --*r;
    case A_PRE_DECR_LOCAL: r = local_ref(e, arg(e, n, 1)); return --*r;
    case A_PRE_DECR_ACC: return --e->acc;
    case A_ADD: return (i32)((u32)arg(e, n, 1) + (u32)arg(e, n, 2));
    case A_SUB: return (i32)((u32)arg(e, n, 1) - (u32)arg(e, n, 2));
    case A_MUL: return (i32)((u32)arg(e, n, 1) * (u32)arg(e, n, 2));
    case A_DIV:
        a = arg(e, n, 1);
        b = arg(e, n, 2);
        return b == 0 || (a == INT32_MIN && b == -1) ? 0 : a / b;
    case A_MOD:
        a = arg(e, n, 1);
        b = arg(e, n, 2);
        return b == 0 || b == -1 ? 0 : a % b;
    case A_EQ: return arg(e, n, 1) == arg(e, n, 2);
    case A_NE: return arg(e, n, 1) != arg(e, n, 2);
    case A_LT: return arg(e, n, 1) < arg(e, n, 2);
    case A_LE: return arg(e, n, 1) <= arg(e, n, 2);
    case A_GT: return arg(e, n, 1) > arg(e, n, 2);
    case A_GE: return arg(e, n, 1) >= arg(e, n, 2);
    case A_LAND: return arg(e, n, 1) && arg(e, n, 2);
    case A_LOR: return arg(e, n, 1) || arg(e, n, 2);
    case A_BAND: return arg(e, n, 1) & arg(e, n, 2);
    case A_BOR: return arg(e, n, 1) | arg(e, n, 2);
    case A_BXOR: return arg(e, n, 1) ^ arg(e, n, 2);
    case A_SHL: return (i32)((u32)arg(e, n, 1) << (arg(e, n, 2) & 31));
    case A_SHR: return arg(e, n, 1) >> (arg(e, n, 2) & 31);
    case A_NOT: return !arg(e, n, 1);
    case A_NEG: return -arg(e, n, 1);
    case A_THREE_OP: return arg(e, n, 1) ? arg(e, n, 2) : arg(e, n, 3);
    default:
        br_log(e, "unknown expression %s", n->n && n->k[0]->s ? n->k[0]->s : "?");
        return 0;
    }
}

static bool push_frame(BR *e, Script *sc, Unit *u) {
    if (!sc || !u) return false;
    if (e->sp >= BR_STACK) {
        br_log(e, "call stack overflow in script %d", sc->num);
        return false;
    }
    Frame *f = &e->stack[e->sp++];
    memset(f, 0, sizeof *f);
    f->sc = sc;
    f->u = u;
    return true;
}

void vm_start(BR *e, int script) {
    e->sp = 0;
    Script *sc = script_get(e, script);
    if (!sc) {
        snprintf(e->problem, sizeof e->problem, "Script %d can't be read.", script);
        return;
    }
    if (e->trace) br_log(e, "start script %d", script);
    push_frame(e, sc, &sc->set);
}

void vm_jump(BR *e, int script) { vm_start(e, script); }

static void call(BR *e, int script, int sub) {
    Script *sc = script_get(e, script);
    Unit *u = script_sub(sc, sub);
    if (!u) {
        br_log(e, "call to missing sub %d:%d", script, sub);
        return;
    }
    if (e->trace) br_log(e, "call %d:%d", script, sub);
    push_frame(e, sc, u);
}

static void go_label(BR *e, Frame *f, int id) {
    int pc = unit_label(f->u, id);
    if (pc < 0) {
        br_log(e, "missing label %d in script %d", id, f->sc->num);
        return;
    }
    f->pc = pc;
}

static void do_switch(BR *e, Frame *f, Node *s) {
    for (int i = 1; i < s->n; i++) {
        Node *c = s->k[i];
        if (c->t != N_LIST || c->n < 2) continue;
        if ((c->k[0]->t == N_INT && c->k[0]->i == e->cond) || sx_sym(c->k[0], "default")) {
            go_label(e, f, sx_int(c->k[1], 0));
            return;
        }
    }
}

static void script_end(BR *e);

void vm_run(BR *e) {
    int budget = 200000;
    while (e->wait == W_NONE && !e->quit && !e->ended && budget-- > 0) {
        Frame *f = top(e);
        if (!f) {
            script_end(e);
            continue;
        }
        if (f->pc >= f->u->count) {
            e->sp--;
            continue;
        }
        Node *s = f->u->st[f->pc++];
        switch (s->op) {
        case A_LABEL: break;
        case A_GOTO: go_label(e, f, arg(e, s, 1)); break;
        case A_IF:
            if (e->cond) go_label(e, f, arg(e, s, 1));
            break;
        case A_EVAL: e->cond = arg(e, s, 1); break;
        case A_EVAL_NOEFFECT: arg(e, s, 1); break;
        case A_SWITCH: do_switch(e, f, s); break;
        case A_CALL_SUB: call(e, f->sc->num, arg(e, s, 1)); break;
        case A_CALL_FAR: call(e, arg(e, s, 1), arg(e, s, 2)); break;
        case A_JUMP: {
            int to = arg(e, s, 1);
            if (to < 0) {
                e->sp = 0;
                break;
            }
            vm_jump(e, to);
            break;
        }
        case A_RETURN: e->sp--; break;
        case A_SEND_STRING: uf_string(e, s); break;
        case A_CALL_USER_FUNC: uf_call(e, arg(e, s, 1), s); break;
        default: br_log(e, "unknown statement %s in script %d", s->n && s->k[0]->s ? s->k[0]->s : "?", f->sc->num); break;
        }
    }
}

static void script_end(BR *e) {
    if (e->trace) br_log(e, "script stack empty");
    if (e->nextScript >= 0) {
        int n = e->nextScript;
        e->nextScript = -1;
        e->systemMode = e->nextSystem;
        vm_start(e, n);
        return;
    }
    e->ended = true;
}

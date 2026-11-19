#include "ex.h"

#include <ctype.h>

static i32 qRegs[16], lRegs[64], fRegs[64];

i32 *reg_ref(EX *e, char kind, int n) {
    static i32 dummy;
    switch (toupper((u8)kind)) {
    case 'R': return n >= 0 && n < e->nR ? &e->R[n] : &dummy;
    case 'S': return n >= 0 && n < e->nS ? &e->S[n] : &dummy;
    case 'Q': return n >= 0 && n < 16 ? &qRegs[n] : &dummy;
    case 'L': return n >= 0 && n < 64 ? &lRegs[n] : &dummy;
    case 'F': return n >= 0 && n < 64 ? &fRegs[n] : &dummy;
    default: dummy = 0; return &dummy;
    }
}

static bool cond_one(EX *e, const char **pp) {
    const char *p = *pp;
    while (*p == ' ') p++;
    bool neg = false;
    if (*p == '!') {
        neg = true;
        p++;
    }
    long lhs;
    if (isalpha((u8)*p) && (isdigit((u8)p[1]) || p[1] == '-')) {
        char k = *p++;
        long n = strtol(p, (char **)&p, 10);
        lhs = *reg_ref(e, k, (int)n);
    } else if (!strncmp(p, "true", 4)) {
        p += 4;
        lhs = 1;
    } else if (!strncmp(p, "false", 5)) {
        p += 5;
        lhs = 0;
    } else lhs = strtol(p, (char **)&p, 0);
    while (*p == ' ') p++;
    bool r = lhs != 0;
    char o1 = p[0], o2 = p[1];
    if ((o1 == '=' && o2 == '=') || (o1 == '!' && o2 == '=') || (o1 == '>' && o2 == '=') || (o1 == '<' && o2 == '=') || o1 == '>' || o1 == '<') {
        p += (o2 == '=') ? 2 : 1;
        while (*p == ' ') p++;
        long rhs;
        if (isalpha((u8)*p) && isdigit((u8)p[1])) {
            char k = *p++;
            long n = strtol(p, (char **)&p, 10);
            rhs = *reg_ref(e, k, (int)n);
        } else rhs = strtol(p, (char **)&p, 0);
        if (o1 == '=') r = lhs == rhs;
        else if (o1 == '!') r = lhs != rhs;
        else if (o1 == '>') r = o2 == '=' ? lhs >= rhs : lhs > rhs;
        else r = o2 == '=' ? lhs <= rhs : lhs < rhs;
    }
    *pp = p;
    return neg ? !r : r;
}

bool vm_cond(EX *e, const char *cond) {
    if (!cond || !*cond || !strcmp(cond, "*")) return true;
    const char *p = cond;
    bool r = cond_one(e, &p);
    for (;;) {
        while (*p == ' ') p++;
        if (p[0] == '&' && p[1] == '&') {
            p += 2;
            bool b = cond_one(e, &p);
            r = r && b;
        } else if (p[0] == '|' && p[1] == '|') {
            p += 2;
            bool b = cond_one(e, &p);
            r = r || b;
        } else break;
    }
    return r;
}

void vm_goto_script(EX *e, const char *name) {
    Script *sc = rld_get(e, name);
    if (!sc) {
        snprintf(e->problem, sizeof e->problem, "missing script %s", name);
        e->ended = true;
        return;
    }
    e->sc = sc;
    e->pc = 0;
    e->sp = 0;
    ex_log(e, "scenario %s", name);
}

static void unknown(EX *e, Op *op) {
    if (!e->unknownSeen) e->unknownSeen = ex_alloc(1024);
    if (op->op < 1024 && !e->unknownSeen[op->op]) {
        e->unknownSeen[op->op] = 1;
        ex_log(e, "unhandled op %d in %s", op->op, e->sc->name);
    }
}

static void skip_block(EX *e) {
    int pc = e->pc + 1;
    while (pc < e->sc->count && e->sc->ops[pc].op != 5) pc++;
    e->pc = pc;
}

static bool jump_label(EX *e, int id) {
    int pc = script_label(e->sc, id);
    if (pc < 0) {
        ex_log(e, "no label %d in %s", id, e->sc->name);
        return false;
    }
    e->pc = pc;
    return true;
}

static void call(EX *e, int id) {
    Script *target = e->sc;
    int pc = script_func(target, id);
    if (pc < 0 && e->sc->library) {
        target = rld_get(e, e->sc->library);
        pc = script_func(target, id);
    }
    if (pc < 0) {
        ex_log(e, "no function %d for %s", id, e->sc->name);
        e->pc++;
        return;
    }
    if (e->sp < 64) e->stack[e->sp++] = (Frame){e->sc, e->pc + 1};
    e->sc = target;
    e->pc = pc + 1;
}

static void ret(EX *e) {
    if (!e->sp) {
        e->pc++;
        return;
    }
    Frame f = e->stack[--e->sp];
    e->sc = f.sc;
    e->pc = f.pc;
}

static void set_reg(EX *e, char *s) {
    char *f[8];
    int n = split_csv(s, f, 8);
    if (n < 4) return;
    *reg_ref(e, 'R', field_int(f[1])) = field_int(f[3]);
}

static void wait_ms(EX *e, int ms, bool click) {
    e->wait = click ? W_CLICK : W_TIME;
    e->waitUntil = e->now + (u32)(ms > 0 ? ms : 0);
}

static bool exec(EX *e, Op *op) {
    char buf[4096];
    char *s0 = NULL;
    if (op->ns) {
        snprintf(buf, sizeof buf, "%s", op->s[0]);
        s0 = buf;
    }
    switch (op->op) {
    case 0:
    case 2:
    case 3:
    case 12:
        break;
    case 5: {
        char *f[8];
        int n = split_csv(s0, f, 8);
        const char *cond = n > 3 ? f[3] : "*";
        if (!vm_cond(e, cond)) {
            skip_block(e);
            return true;
        }
        break;
    }
    case 6:
        set_reg(e, s0);
        break;
    case 13:
        wait_ms(e, op->nv ? op->v[0] : 0, op->nv > 1 && op->v[1]);
        e->pc++;
        return false;
    case 17: {
        char *f[4];
        split_csv(s0, f, 4);
        vm_goto_script(e, f[0]);
        return true;
    }
    case 20:
        if (op->nv && vm_cond(e, op->ns ? op->s[0] : "*") && jump_label(e, op->v[0])) return true;
        break;
    case 22: {
        char *f[4];
        split_csv(s0, f, 4);
        call(e, field_int(f[0]));
        return true;
    }
    case 23:
        ret(e);
        return true;
    case 47: {
        int pc = e->pc + 1;
        while (pc < e->sc->count && e->sc->ops[pc].op != 23) pc++;
        e->pc = pc + 1;
        return true;
    }
    case 50: {
        char *f[4];
        split_csv(s0, f, 4);
        snd_play_music(e, field_int(f[0]), field_int(f[1]));
        break;
    }
    case 51:
        snd_stop(e, 0, op->nv ? op->v[0] : 0);
        break;
    case 56:
    case 58:
        if (op->nv) snd_play_id(e, op->v[0], 2 + (op->nv > 3 ? op->v[3] & 1 : 0), op->nv > 4 && op->v[4] == 1 && op->op == 54, 0);
        break;
    case 57:
        snd_stop(e, 2, 0);
        snd_stop(e, 3, 0);
        break;
    case 74:
        gfx_draw_cg(e, op->nv ? op->v[0] : 0, 0);
        break;
    case 94:
    case 95:
        if ((op->op == 94 ? gfx_billboard(e, s0) : gfx_remove_billboard(e, s0)) > 0) {
            e->wait = W_FADE;
            return false;
        }
        break;
    case 100:
        view_move(e, s0);
        break;
    case 101:
    case 106:
        if (gfx_busy(e)) {
            e->wait = W_FADE;
            return false;
        }
        break;
    case 105:
        view_fade(e, s0, true);
        break;
    case 203:
        if (op->ns) view_prepare(e, op->s[0]);
        break;
    case 204:
        view_show(e, s0);
        break;
    case 205:
        view_select(e, s0);
        e->wait = W_VIEW;
        e->pc++;
        return false;
    case 206:
    case 207:
        view_destroy(e);
        break;
    case 249:
    case 250:
        break;
    default:
        unknown(e, op);
        break;
    }
    e->pc++;
    return true;
}

void vm_run(EX *e) {
    for (int guard = 0; guard < 20000 && !e->ended; guard++) {
        switch (e->wait) {
        case W_TIME:
            if ((i32)(e->now - e->waitUntil) < 0) return;
            break;
        case W_CLICK:
            if ((i32)(e->now - e->waitUntil) < 0 && !e->lPress) return;
            e->lPress = false;
            break;
        case W_FADE:
            if (gfx_busy(e)) return;
            e->pc++;
            break;
        case W_VIEW:
            if (e->view.result == -2) return;
            break;
        default:
            break;
        }
        e->wait = W_NONE;
        if (!e->sc) return;
        if (e->pc >= e->sc->count) {
            if (e->sp) {
                ret(e);
                continue;
            }
            ex_log(e, "%s ran off its end", e->sc->name);
            e->ended = true;
            return;
        }
        Op *op = &e->sc->ops[e->pc];
        if (e->trace) ex_log(e, "%s:%d op %d", e->sc->name, e->pc, op->op);
        if (!exec(e, op)) return;
    }
}

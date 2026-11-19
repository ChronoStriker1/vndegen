#include "ex.h"

#include <ctype.h>

i32 *reg_ref(EX *e, char kind, int n) {
    static i32 dummy;
    switch (toupper((u8)kind)) {
    case 'R':
        if (n >= 1000000) return n - 1000000 < e->nS ? &e->S[n - 1000000] : &dummy;
        return n >= 0 && n < e->nR ? &e->R[n] : &dummy;
    case 'S': return n >= 0 && n < e->nS ? &e->S[n] : &dummy;
    case 'Q': return n >= 0 && n < 16 ? &e->Q[n] : &dummy;
    case 'L': return n >= 0 && n < 128 ? &e->L[n] : &dummy;
    case 'F': return n >= 0 && n < 64 ? &e->F[n] : &dummy;
    default: dummy = 0; return &dummy;
    }
}

typedef struct Parser {
    EX *e;
    const char *p;
} Parser;

static long ex_or(Parser *ps);

static void skip_ws(Parser *ps) {
    while (*ps->p == ' ' || *ps->p == '\t') ps->p++;
}

static long primary(Parser *ps) {
    skip_ws(ps);
    const char *p = ps->p;
    if (*p == '(') {
        ps->p++;
        long v = ex_or(ps);
        skip_ws(ps);
        if (*ps->p == ')') ps->p++;
        return v;
    }
    if (*p == '!') {
        ps->p++;
        return !primary(ps);
    }
    if (*p == '-') {
        ps->p++;
        return -primary(ps);
    }
    if (!strncmp(p, "true", 4)) {
        ps->p += 4;
        return 1;
    }
    if (!strncmp(p, "false", 5)) {
        ps->p += 5;
        return 0;
    }
    if (isalpha((u8)*p) && (isdigit((u8)p[1]) || (p[1] == '-' && isdigit((u8)p[2])))) {
        char k = *p++;
        char *end;
        long n = strtol(p, &end, 10);
        ps->p = end;
        return *reg_ref(ps->e, k, (int)n);
    }
    char *end;
    long v = strtol(p, &end, 0);
    if (end == p) {
        if (*p) ps->p++;
        return 0;
    }
    ps->p = end;
    return v;
}

static long mul(Parser *ps) {
    long v = primary(ps);
    for (;;) {
        skip_ws(ps);
        char c = *ps->p;
        if (c != '*' && c != '/' && c != '%') return v;
        ps->p++;
        long r = primary(ps);
        if (c == '*') v *= r;
        else if (r) v = c == '/' ? v / r : v % r;
    }
}

static long add(Parser *ps) {
    long v = mul(ps);
    for (;;) {
        skip_ws(ps);
        char c = *ps->p;
        if (c != '+' && c != '-') return v;
        ps->p++;
        long r = mul(ps);
        v = c == '+' ? v + r : v - r;
    }
}

static long cmp(Parser *ps) {
    long v = add(ps);
    skip_ws(ps);
    const char *p = ps->p;
    int op = 0;
    if (p[0] == '=' && p[1] == '=') op = 1;
    else if (p[0] == '!' && p[1] == '=') op = 2;
    else if (p[0] == '>' && p[1] == '=') op = 3;
    else if (p[0] == '<' && p[1] == '=') op = 4;
    else if (p[0] == '>') op = 5;
    else if (p[0] == '<') op = 6;
    else if (p[0] == '=') op = 7;
    if (!op) return v;
    ps->p += op <= 4 ? 2 : 1;
    long r = add(ps);
    switch (op) {
    case 1:
    case 7: return v == r;
    case 2: return v != r;
    case 3: return v >= r;
    case 4: return v <= r;
    case 5: return v > r;
    default: return v < r;
    }
}

static long ex_and(Parser *ps) {
    long v = cmp(ps);
    for (;;) {
        skip_ws(ps);
        if (*ps->p != '&') return v;
        ps->p += ps->p[1] == '&' ? 2 : 1;
        long r = cmp(ps);
        v = v && r;
    }
}

static long ex_or(Parser *ps) {
    long v = ex_and(ps);
    for (;;) {
        skip_ws(ps);
        if (*ps->p != '|') return v;
        ps->p += ps->p[1] == '|' ? 2 : 1;
        long r = ex_and(ps);
        v = v || r;
    }
}

long vm_eval(EX *e, const char *s) {
    if (!s || !*s) return 0;
    Parser ps = {e, s};
    return ex_or(&ps);
}

bool vm_cond(EX *e, const char *cond) {
    if (!cond || !*cond || !strcmp(cond, "*")) return true;
    return vm_eval(e, cond) != 0;
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
        pc = target ? script_func(target, id) : -1;
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

static void set_reg(EX *e, char *s, bool sys) {
    char *f[8];
    int n = split_csv(s, f, 8);
    if (n < 4) return;
    if (n > 4 && !vm_cond(e, f[4])) return;
    int r = field_int(f[1]);
    i32 v = (i32)vm_eval(e, f[3]);
    if (sys && r >= 1000000) *reg_ref(e, 'S', r - 1000000) = v;
    else *reg_ref(e, 'R', r) = v;
}

static void wait_ms(EX *e, int ms, bool click) {
    e->wait = click ? W_CLICK : W_TIME;
    e->waitUntil = e->now + (u32)(ms > 0 ? ms : 0);
}

static void draw(EX *e, const i32 *v, int nv) {
    int id = nv ? v[0] : 0;
    int ms = nv > 3 ? v[3] : 0;
    int mode = nv > 1 ? v[1] : 1;
    gfx_draw_cg(e, id, (mode & 8) ? ms : 0);
    if (nv > 10 && v[9] > 0 && v[10] > 0) gfx_camera(e, (float)v[7], (float)v[8], (float)v[9], (float)v[10], 0, 0);
    else gfx_camera(e, 0, 0, EX_W, EX_H, 0, 0);
}

static void env_voice(EX *e, char *s) {
    char *f[8];
    int n = split_csv(s, f, 8);
    if (n < 2) return;
    snd_play_env(e, field_int(f[1]), n > 2 ? field_int(f[2]) : 100);
}

static void chara_op(EX *e, Op *op) {
    switch (op->op) {
    case 33:
        chara_leave_all(e, 0);
        break;
    case 37:
        chara_enter(e, op);
        break;
    case 38:
        if (op->ns) {
            char *f[8];
            char buf[256];
            snprintf(buf, sizeof buf, "%s", op->s[0]);
            int n = split_csv(buf, f, 8);
            if (n >= 3) chara_leave(e, field_int(f[0]), field_int(f[1]));
        }
        break;
    case 35:
        for (int i = 1; i + 3 < op->nv && op->v[i] >= 0; i += 4) chara_move(e, op->v[i], op->v[i + 1], op->v[i + 3], op->v[i + 2]);
        break;
    case 39:
        if (op->nv > 6) chara_move(e, op->v[0], op->v[6], op->v[1], op->v[2]);
        break;
    }
}

void vm_exec_embedded(EX *e, Embedded *b) {
    Op op = {0};
    op.op = (u16)b->op;
    op.nv = (u8)b->nv;
    op.v = b->v;
    switch (b->op) {
    case 37:
    case 38:
    case 33:
    case 35:
    case 39:
        chara_op(e, &op);
        break;
    case 74:
        draw(e, b->v, b->nv);
        break;
    default:
        ex_log(e, "embedded op %d", b->op);
        break;
    }
}

static bool exec(EX *e, Op *op) {
    char buf[8192];
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
    case 44:
    case 46:
    case 61:
    case 65:
    case 66:
    case 70:
    case 71:
    case 189:
    case 192:
    case 193:
    case 195:
    case 249:
    case 250:
    case 269:
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
        set_reg(e, s0, false);
        break;
    case 8:
        set_reg(e, s0, true);
        break;
    case 11:
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
    case 18:
        msg_show(e, op->nv && op->v[0], op->nv > 1 ? op->v[1] : 0);
        break;
    case 19:
        e->msg.ctrlShown = op->nv && op->v[0];
        break;
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
        if (op->ns) {
            char *f[4];
            int n = split_csv(s0, f, 4);
            if (n > 1 && !vm_cond(e, f[1])) break;
        }
        ret(e);
        return true;
    case 28:
        msg_start(e, op);
        e->wait = W_TEXT;
        e->pc++;
        return false;
    case 30:
        msg_clear(e);
        break;
    case 33:
    case 35:
    case 37:
    case 38:
    case 39:
        chara_op(e, op);
        break;
    case 36:
        if (chara_busy(e)) {
            e->wait = W_FADE;
            return false;
        }
        break;
    case 47: {
        int pc = e->pc + 1;
        while (pc < e->sc->count && !(e->sc->ops[pc].op == 23 && (!e->sc->ops[pc].ns || e->sc->ops[pc].s[0][0] == '0'))) pc++;
        e->pc = pc + 1;
        return true;
    }
    case 48:
        chara_create(e, s0);
        break;
    case 49:
        chara_define(e, s0);
        break;
    case 50: {
        char *f[4];
        split_csv(s0, f, 4);
        snd_play_music(e, field_int(f[0]), field_int(f[1]));
        break;
    }
    case 51:
        snd_stop(e, EX_CH_MUSIC, op->nv ? op->v[0] : 0);
        break;
    case 54: {
        char *f[6];
        int n = split_csv(s0, f, 6);
        if (n > 1) snd_play_id(e, field_int(f[0]), EX_CH_LOOP, true, field_int(f[1]));
        break;
    }
    case 55:
        snd_stop(e, EX_CH_LOOP, op->nv ? op->v[0] : 0);
        break;
    case 56:
    case 58:
        if (op->nv) snd_play_id(e, op->v[0], EX_CH_SE, op->nv > 3 && op->v[3] == 1 && op->op == 56 && false, 0);
        break;
    case 57:
        snd_stop(e, EX_CH_SE, 0);
        break;
    case 59:
        break;
    case 64:
        msg_ctrl_define(e, s0);
        break;
    case 72: {
        char *f[4];
        split_csv(s0, f, 4);
        msg_set_frame(e, field_int(f[0]));
        break;
    }
    case 73:
        msg_define(e, s0);
        break;
    case 74:
        draw(e, op->v, op->nv);
        if (op->nv > 3 && (op->v[1] & 8) && op->v[3] > 0 && gfx_busy(e)) {
            e->wait = W_FADE;
            return false;
        }
        break;
    case 81:
        if (op->nv >= 5) gfx_camera(e, (float)op->v[0], (float)op->v[1], (float)op->v[2], (float)op->v[3], op->v[4], op->nv > 6 ? op->v[6] : 0);
        break;
    case 82:
        if (gfx_busy(e)) {
            e->wait = W_FADE;
            return false;
        }
        break;
    case 94:
    case 95:
        if ((op->op == 94 ? gfx_billboard(e, s0) : gfx_remove_billboard(e, s0)) > 0) {
            e->wait = W_FADE;
            return false;
        }
        break;
    case 100:
        gfx_move(e, s0);
        break;
    case 101:
    case 106:
        if (gfx_busy(e)) {
            e->wait = W_FADE;
            return false;
        }
        break;
    case 105:
        gfx_fade(e, s0);
        break;
    case 191: {
        char *f[8];
        int n = split_csv(s0, f, 8);
        if (n >= 7 && field_int(f[0]) == 96) {
            free(e->fontFile);
            e->fontFile = ex_strdup(f[6]);
        }
        break;
    }
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
    case 221:
        msg_wait_mark(e, s0);
        break;
    case 223: {
        char *f[4];
        int n = split_csv(s0, f, 4);
        msg_temp_hide(e, true, n > 1 ? field_int(f[1]) : 0);
        break;
    }
    case 224:
        msg_temp_hide(e, false, op->nv ? op->v[0] : 0);
        break;
    case 240:
        env_voice(e, s0);
        break;
    case 241:
        snd_stop(e, EX_CH_ENV, 200);
        break;
    case 242:
        if (gfx_faceup(e, s0) > 0) {
            e->wait = W_FADE;
            return false;
        }
        break;
    case 251:
        if (op->ns) {
            char *f[4];
            int n = split_csv(s0, f, 4);
            if (n > 1) portrait_def(e, field_int(f[0]), field_int(f[1]));
        }
        break;
    case 252:
        portrait_draw_cmd(e, s0);
        break;
    case 253:
        portrait_clear(e, s0);
        break;
    default:
        unknown(e, op);
        break;
    }
    e->pc++;
    return true;
}

static bool advance_click(EX *e) {
    if (!e->lPress) return false;
    e->lPress = false;
    return true;
}

void vm_run(EX *e) {
    msg_tick(e);
    for (int guard = 0; guard < 20000 && !e->ended; guard++) {
        switch (e->wait) {
        case W_TIME:
            if ((i32)(e->now - e->waitUntil) < 0) return;
            break;
        case W_CLICK:
            if ((i32)(e->now - e->waitUntil) < 0 && !advance_click(e)) return;
            break;
        case W_FADE:
            if (gfx_busy(e) || chara_busy(e)) return;
            e->pc++;
            break;
        case W_VIEW:
            if (e->view.result == -2) return;
            break;
        case W_TEXT:
            if (msg_typing(e)) {
                if (advance_click(e)) msg_finish(e);
                return;
            }
            if (!advance_click(e) && !e->skipping) return;
            msg_flush_embedded(e);
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
            if (e->bootEntry) {
                char *entry = e->bootEntry;
                e->bootEntry = NULL;
                vm_goto_script(e, entry);
                free(entry);
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

#include "bsx.h"

static Val zero;

Val *bx_var_ref(BX *b, int type, i32 index) {
    switch (type) {
    case VT_GLOBAL:
        if (index >= 0 && index < b->sc->varCount) return &b->globals[index];
        break;
    case VT_LOCAL:
        if (index >= 0 && index < BX_LOCALS) return &b->locals[index];
        break;
    case VT_ARG:
        if (index >= 0 && index < BX_ARGS) return &b->args[index];
        break;
    case VT_TEMP:
        if (index >= 0 && index < BX_TEMPS) return &b->temps[index];
        break;
    }
    bx_log(b, "bad variable %d:%d", type, index);
    zero = (Val){0};
    return &zero;
}

int bx_var_by_name(BX *b, const char *name) { return bx_script_var(b->sc, name); }

static Val leaf(BX *b, const u8 *p) {
    int t = p[0];
    i32 v = (i32)rd32(p + 2);
    if (t == 0) return (Val){0, v};
    if (t == 1) return (Val){1, v};
    return *bx_var_ref(b, t, v);
}

Val bx_arg(BX *b, int i) { return i >= 0 && i < BX_ARGS ? b->args[i] : (Val){0}; }

i32 bx_arg_int(BX *b, int i) { return bx_arg(b, i).v; }

const char *bx_arg_str(BX *b, int i) {
    Val v = bx_arg(b, i);
    if (!v.str || v.v < 0 || v.v >= b->sc->strCount) return "";
    return b->sc->strs[v.v];
}

static const char *val_str(BX *b, Val v) { return v.str && v.v >= 0 && v.v < b->sc->strCount ? b->sc->strs[v.v] : NULL; }

static i32 compare(BX *b, Val x, Val y) {
    const char *sx = val_str(b, x), *sy = val_str(b, y);
    if (sx && sy) return strcmp(sx, sy);
    return x.v < y.v ? -1 : x.v > y.v;
}

static void jump(BX *b, u32 off) {
    if (off >= b->sc->codeSize) {
        bx_log(b, "jump outside the script to %x", off);
        b->th.halted = true;
        return;
    }
    b->th.pc = off;
}

void bx_vm_goto(BX *b, int label) {
    if (label < 0 || label >= b->sc->labelCount) {
        bx_log(b, "no label %d", label);
        b->th.halted = true;
        return;
    }
    b->th.halted = false;
    b->th.curLabel = label;
    jump(b, b->sc->labels[label]);
}

void bx_vm_init(BX *b) {
    b->globals = bx_alloc(sizeof(Val) * (size_t)(b->sc->varCount ? b->sc->varCount : 1));
    b->th.skipLabel = -1;
    int boot = bx_script_label(b->sc, "boot");
    bx_vm_goto(b, boot >= 0 ? boot : 0);
}

static void message(BX *b, const u8 *p, int *len) {
    int flag = p[1];
    u32 msg = rd32(p + 2);
    int name = -1;
    const char *voice = NULL;
    *len = 6;
    if (flag >= 1) {
        name = (int)rd32(p + 6);
        *len = 10;
    }
    if (flag >= 2) {
        Val v = {(u8)rd32(p + 10), (i32)rd32(p + 14)};
        voice = v.str ? val_str(b, v) : NULL;
        *len = 18;
    }
    if (flag > 2) bx_log(b, "message flag %d", flag);
    const char *text = msg < (u32)b->sc->msgCount ? b->sc->msgs[msg] : "";
    const char *speaker = name >= 0 && name < b->sc->charCount ? b->sc->chars[name] : NULL;
    if (b->trace) bx_log(b, "msg %u %s: %s", msg, speaker ? speaker : "", text);
    b->msgPc = b->th.pc;
    if (b->histCount == 50) {
        memmove(b->hist, b->hist + 1, sizeof(u32) * 49);
        b->histCount--;
    }
    b->hist[b->histCount++] = b->th.pc;
    b->msgsShown++;
    bool unread = !bx_msg_read(b, msg);
    if (unread && b->skipMode && !b->set.skipUnread && b->common) b->skipMode = false;
    bx_msg_show(b, speaker, text, voice);
    bx_mark_read(b, msg);
    b->th.wait = W_TEXT;
}

static bool step(BX *b) {
    Thread *t = &b->th;
    const u8 *code = b->sc->code;
    u32 size = b->sc->codeSize;
    if (t->pc >= size) {
        bx_log(b, "ran off the end of the script");
        t->halted = true;
        return false;
    }
    const u8 *p = code + t->pc;
    int op = p[0];
    if (op >= 0x1e && op <= 0x37 && op != 0x33 && op != 0x34 && op != 0x36) {
        t->pc++;
        bx_cmd(b, op);
        return true;
    }
    switch (op) {
    case 0x00:
        t->pc++;
        bx_log(b, "end of game at %x", (unsigned)(t->pc - 1));
        bx_system(b, "@end");
        return true;
    case 0x03:
        bx_vm_goto(b, (int)rd32(p + 1));
        return true;
    case 0x04:
        jump(b, rd32(p + 1));
        return true;
    case 0x05:
        if (b->temps[0].v) jump(b, rd32(p + 1));
        else t->pc += 5;
        return true;
    case 0x06:
        if (!b->temps[0].v) jump(b, rd32(p + 1));
        else t->pc += 5;
        return true;
    case 0x08:
        t->skipLabel = (int)rd32(p + 1);
        t->pc += 5;
        return true;
    case 0x09:
        if (t->sp >= BX_STACK) {
            bx_log(b, "call stack overflow");
            t->halted = true;
            return false;
        }
        t->frameLabel[t->sp] = (u32)t->curLabel;
        t->stack[t->sp++] = t->pc + 5;
        bx_vm_goto(b, (int)rd32(p + 1));
        return true;
    case 0x0a:
        if (!t->sp) {
            bx_log(b, "return with an empty stack at %x", (unsigned)t->pc);
            t->halted = true;
            return false;
        }
        t->pc = t->stack[--t->sp];
        t->curLabel = (int)t->frameLabel[t->sp];
        return true;
    case 0x12: {
        Val *dst = bx_var_ref(b, p[6], (i32)rd32(p + 8));
        *dst = leaf(b, p + 12);
        t->pc += 18;
        return true;
    }
    case 0x1b:
    case 0x1c: {
        Val *dst = bx_var_ref(b, p[6], (i32)rd32(p + 8));
        dst->str = 0;
        dst->v += op == 0x1b ? 1 : -1;
        t->pc += 12;
        return true;
    }
    case 0x1d: {
        int len;
        message(b, p, &len);
        t->pc += (u32)len;
        return true;
    }
    case 0x33:
        if (b->choiceCount < 16) {
            Choice *c = &b->choice[b->choiceCount++];
            memset(c, 0, sizeof *c);
            c->id = (int)rd32(p + 1);
            c->label = (int)rd32(p + 5);
            c->msg = (int)rd32(p + 9);
            c->design = bx_arg_int(b, 1);
        }
        t->pc += 13;
        return true;
    case 0x34:
        t->pc += 5;
        bx_choice_begin(b);
        return true;
    case 0x36:
    case 0x38:
        if (op == 0x38) {
            if ((int)rd32(p + 1) == t->skipLabel) t->skipLabel = -1;
            t->curLabel = (int)rd32(p + 1);
        }
        t->pc += 5;
        return true;
    default:
        break;
    }
    if (op >= 0x0b && op <= 0x1a) {
        Val x = leaf(b, p + 6), y = leaf(b, p + 12);
        i32 r = 0;
        switch (op) {
        case 0x0b: r = x.v && y.v; break;
        case 0x0c: r = compare(b, x, y) > 0; break;
        case 0x0d: r = compare(b, x, y) < 0; break;
        case 0x0e: r = compare(b, x, y) >= 0; break;
        case 0x0f: r = compare(b, x, y) <= 0; break;
        case 0x10: r = compare(b, x, y) != 0; break;
        case 0x11: r = compare(b, x, y) == 0; break;
        case 0x13: r = x.v | y.v; break;
        case 0x14: r = x.v & y.v; break;
        case 0x15: r = x.v + y.v; break;
        case 0x16: r = x.v - y.v; break;
        case 0x17: r = x.v * y.v; break;
        case 0x18: r = y.v ? x.v / y.v : 0; break;
        case 0x19: r = y.v ? x.v % y.v : 0; break;
        case 0x1a: r = x.v ^ y.v; break;
        default: break;
        }
        Val *dst = bx_var_ref(b, VT_TEMP, (i32)rd32(p + 2));
        *dst = (Val){0, r};
        t->pc += 18;
        return true;
    }
    bx_log(b, "unknown opcode %02x at %x", op, (unsigned)t->pc);
    snprintf(b->problem, sizeof b->problem, "unknown script opcode %02x at %x", op, (unsigned)t->pc);
    t->halted = true;
    return false;
}

void bx_vm_run(BX *b) {
    for (int n = 0; n < 200000 && !b->th.halted && b->th.wait == W_NONE && b->phase == PH_NONE && !b->quit; n++)
        if (!step(b)) break;
}

#include "sa.h"

#include <ctype.h>
#include <stdarg.h>

static void vlog(const VMHost *h, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void vlog(const VMHost *h, const char *fmt, ...) {
    if (!h || !h->log) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    h->log(h->ctx, buf);
}

static int load_box(VM *vm, Vfs *vfs, const char *file) {
    for (int i = 0; i < vm->boxCount; i++)
        if (sa_ieq(vm->boxes[i].file, file)) return i;
    if (vm->boxCount == SA_BOXES) return -1;
    size_t n;
    u8 *d = sa_vfs_read(vfs, file, &n);
    if (!d) return -1;
    LoadedBox *b = &vm->boxes[vm->boxCount];
    bool ok = sa_box_parse(&b->box, d, n);
    free(d);
    if (!ok) return -1;
    snprintf(b->file, sizeof b->file, "%s", file);
    return vm->boxCount++;
}

static bool push_frame(VM *vm, Vfs *vfs, const char *file, int entry, const VMHost *h) {
    if (vm->depth == SA_STACK) {
        vlog(h, "call depth exceeded at %s #%d", file, entry);
        return false;
    }
    int b = load_box(vm, vfs, file);
    if (b < 0 || entry < 0 || entry >= vm->boxes[b].box.count) {
        vlog(h, "missing script %s #%d", file, entry);
        return false;
    }
    Frame *f = &vm->frames[vm->depth++];
    memset(f, 0, sizeof *f);
    f->box = b;
    f->entry = entry;
    return true;
}

bool sa_vm_start(VM *vm, Vfs *vfs, const char *boxFile, int entry, const VMHost *host) {
    vm->state = VM_RUN;
    return push_frame(vm, vfs, boxFile, entry, host);
}

void sa_vm_free(VM *vm) {
    for (int i = 0; i < vm->boxCount; i++) sa_box_free(&vm->boxes[i].box);
    for (int i = 0; i < 256; i++) free(vm->boxSlot[i]);
    for (int i = 0; i < vm->choiceCount; i++) free(vm->choice[i].label);
    memset(vm, 0, sizeof *vm);
}

static Frame *top(VM *vm) { return vm->depth ? &vm->frames[vm->depth - 1] : NULL; }

i32 sa_vm_get(VM *vm, const u8 *code, const Operand *o) {
    Frame *f = top(vm);
    switch (o->kind) {
    case OPD_INT:
    case OPD_BYTE:
    case OPD_ADDR:
        return o->value;
    case OPD_VAR:
        switch (o->bank) {
        case 0x02:
            return o->index < SA_C_VARS ? vm->c[o->index] : 0;
        case 0x04:
            return o->index < SA_F_VARS ? vm->f[o->index] : 0;
        case 0x22:
            return f && o->index < SA_L_VARS ? f->cl[o->index] : 0;
        case 0x24:
            return f && o->index < SA_L_VARS ? f->fl[o->index] : 0;
        case 0x44:
            return o->index < SA_G_VARS ? vm->g[o->index] : 0;
        }
    }
    return 0;
}

void sa_vm_set(VM *vm, const Operand *o, i32 v) {
    Frame *f = top(vm);
    if (o->kind != OPD_VAR) return;
    switch (o->bank) {
    case 0x02:
        if (o->index < SA_C_VARS) vm->c[o->index] = v;
        break;
    case 0x04:
        if (o->index < SA_F_VARS) vm->f[o->index] = v != 0;
        break;
    case 0x22:
        if (f && o->index < SA_L_VARS) f->cl[o->index] = v;
        break;
    case 0x24:
        if (f && o->index < SA_L_VARS) f->fl[o->index] = v != 0;
        break;
    case 0x44:
        if (o->index < SA_G_VARS) vm->g[o->index] = v != 0;
        break;
    }
}

static bool term(VM *vm, const char **s, i32 *out);
static u32 next_random(VM *vm);

u32 sa_vm_random(VM *vm) { return next_random(vm); }

static u32 next_random(VM *vm) {
    if (!vm->rng) vm->rng = 0x9e3779b9u;
    vm->rng ^= vm->rng << 13;
    vm->rng ^= vm->rng >> 17;
    vm->rng ^= vm->rng << 5;
    return vm->rng;
}

static bool term(VM *vm, const char **s, i32 *out) {
    const char *p = *s;
    while (*p == ' ' || *p == '\t') p++;
    Operand o = {0};
    o.kind = OPD_VAR;
    if (p[0] == 'F' && p[1] == 'G') {
        o.bank = 0x44;
        p += 2;
    } else if (p[0] == 'F' && p[1] == 'L') {
        o.bank = 0x24;
        p += 2;
    } else if (p[0] == 'C' && p[1] == 'L') {
        o.bank = 0x22;
        p += 2;
    } else if (p[0] == 'F') {
        o.bank = 0x04;
        p++;
    } else if (p[0] == 'C') {
        o.bank = 0x02;
        p++;
    } else if (isdigit((u8)*p) || *p == '-') {
        char *e;
        *out = (i32)strtol(p, &e, 10);
        *s = e;
        return e != p;
    } else {
        return false;
    }
    char *e;
    long idx = strtol(p, &e, 10);
    if (e == p) return false;
    if (*e == '[') {
        const char *q = e + 1;
        i32 sub = 0;
        if (!term(vm, &q, &sub)) return false;
        while (*q == ' ') q++;
        if (*q != ']') return false;
        idx += sub;
        e = (char *)q + 1;
    }
    o.index = (u16)idx;
    *out = sa_vm_get(vm, NULL, &o);
    *s = e;
    return true;
}

bool sa_vm_cond(VM *vm, const char *expr) {
    const char *p = expr;
    i32 a = 0, b = 0;
    while (*p == ' ' || *p == '\t') p++;
    bool lhs = *p != '=' && *p != '!' && *p != '<' && *p != '>';
    if (lhs && !term(vm, &p, &a)) return false;
    while (*p == ' ' || *p == '\t') p++;
    if (!strncmp(p, "on", 2)) return a != 0;
    if (!strncmp(p, "off", 3)) return a == 0;
    char op[3] = {0};
    int n = 0;
    while (n < 2 && strchr("=!<>", p[n])) {
        op[n] = p[n];
        n++;
    }
    p += n;
    if (!term(vm, &p, &b)) return false;
    if (!lhs) a = vm->c[0];
    if (!strcmp(op, "==") || !strcmp(op, "=")) return a == b;
    if (!strcmp(op, "!=")) return a != b;
    if (!strcmp(op, ">=")) return a >= b;
    if (!strcmp(op, "<=")) return a <= b;
    if (!strcmp(op, ">")) return a > b;
    if (!strcmp(op, "<")) return a < b;
    return false;
}

void sa_vm_choose(VM *vm, int index) {
    Frame *f = top(vm);
    if (vm->state != VM_WAIT_CHOICE || !f || index < 0 || index >= vm->choiceCount) return;
    f->pc = vm->choice[index].target;
    for (int i = 0; i < vm->choiceCount; i++) free(vm->choice[i].label);
    vm->choiceCount = 0;
    vm->state = VM_RUN;
}

void sa_vm_click(VM *vm) {
    if (vm->state == VM_WAIT_CLICK) vm->state = VM_RUN;
}

static char *str_of(const u8 *code, const Operand *o) {
    return o->kind == OPD_STR ? sa_utf16_to_utf8(code + o->strOffset, o->strLen) : sa_strdup("");
}

static bool unknown_once(VM *vm, u8 op) {
    u32 bit = 1u << (op & 31);
    if (vm->unknownSeen[op >> 5] & bit) return false;
    vm->unknownSeen[op >> 5] |= bit;
    return true;
}

int sa_vm_run(VM *vm, Vfs *vfs, const VMHost *h, int budget) {
    while (budget-- > 0 && vm->state == VM_RUN) {
        Frame *f = top(vm);
        if (!f) {
            vm->state = VM_DONE;
            break;
        }
        BoxEntry *be = &vm->boxes[f->box].box.e[f->entry];
        const u8 *code = be->data;
        Insn in;
        if (f->pc >= be->size || !sa_insn_decode(code, be->size, f->pc, &in)) {
            vm->depth--;
            continue;
        }
        f->pc = in.next;
        const Operand *o = in.o;
        switch (in.op) {
        case 0x00:
            if (in.count == 1 && o[0].kind == OPD_ADDR && f->loopActive) {
                Insn hdr;
                if (sa_insn_decode(code, be->size, f->loopHeader, &hdr) && hdr.count >= 2 &&
                    sa_vm_get(vm, code, &hdr.o[0]) >= sa_vm_get(vm, code, &hdr.o[1])) {
                    f->loopActive = false;
                    f->pc = (u32)o[0].value;
                }
            }
            break;
        case 0x2b:
            break;
        case 0x20:
            if (in.count >= 3) {
                if (f->loopActive && f->loopHeader == in.at) sa_vm_set(vm, &o[0], sa_vm_get(vm, code, &o[0]) + 1);
                else {
                    sa_vm_set(vm, &o[0], sa_vm_get(vm, code, &o[2]));
                    f->loopActive = true;
                    f->loopHeader = in.at;
                }
            }
            break;
        case 0x24:
            if (in.count >= 1) f->pc = (u32)o[0].value;
            break;
        case 0x2e:
            if (in.count >= 3) {
                i32 lo = sa_vm_get(vm, code, &o[1]), hi = sa_vm_get(vm, code, &o[2]);
                if (hi < lo) {
                    i32 t = lo;
                    lo = hi;
                    hi = t;
                }
                sa_vm_set(vm, &o[0], lo + (i32)(next_random(vm) % (u32)(hi - lo + 1)));
            }
            break;
        case 0x02:
            for (int i = 0; i < in.count; i++) {
                char *s = str_of(code, &o[i]);
                if (h->text) h->text(h->ctx, s, true);
                free(s);
            }
            break;
        case 0x03: {
            char *name = in.count > 0 ? str_of(code, &o[0]) : sa_strdup("");
            char *voice = in.count > 1 ? str_of(code, &o[1]) : sa_strdup("");
            char *face = in.count > 2 ? str_of(code, &o[2]) : sa_strdup("");
            if (h->speaker) h->speaker(h->ctx, name, voice, face);
            free(name);
            free(voice);
            free(face);
            break;
        }
        case 0x05:
            vm->state = vm->choiceCount ? VM_WAIT_CHOICE : VM_WAIT_CLICK;
            if (vm->state == VM_WAIT_CLICK && h->page) h->page(h->ctx);
            break;
        case 0x07:
            if (vm->choiceCount < SA_CHOICES && in.count >= 2) {
                vm->choice[vm->choiceCount].target = (u32)o[0].value;
                vm->choice[vm->choiceCount].label = str_of(code, &o[1]);
                vm->choiceCount++;
            }
            break;
        case 0x0a:
            if (in.count >= 3 && o[0].kind == OPD_IVAR) {
                Operand dst = o[1];
                Operand idx = {.kind = OPD_VAR, .bank = 0x22, .index = o[0].index};
                dst.index = (u16)(dst.index + sa_vm_get(vm, code, &idx));
                sa_vm_set(vm, &dst, sa_vm_get(vm, code, &o[2]));
            } else if (in.count >= 2) {
                sa_vm_set(vm, &o[0], sa_vm_get(vm, code, &o[1]));
            }
            break;
        case 0x0b:
            if (in.count >= 3) sa_vm_set(vm, &o[0], sa_vm_get(vm, code, &o[1]) + sa_vm_get(vm, code, &o[2]));
            else if (in.count == 2) sa_vm_set(vm, &o[0], sa_vm_get(vm, code, &o[0]) + sa_vm_get(vm, code, &o[1]));
            break;
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
            if (in.count >= 3) {
                i32 x = sa_vm_get(vm, code, &o[1]), y = sa_vm_get(vm, code, &o[2]), r = 0;
                if (in.op == 0x0c) r = x - y;
                else if (in.op == 0x0d) r = x * y;
                else if (y) r = in.op == 0x0e ? x / y : x % y;
                sa_vm_set(vm, &o[0], r);
            }
            break;
        case 0x15:
        case 0x18:
            if (in.count >= 1) f->pc = (u32)o[0].value;
            break;
        case 0x16:
            if (in.count >= 1 && f->sp < SA_STACK) {
                f->ret[f->sp++] = f->pc;
                f->pc = (u32)o[0].value;
            }
            break;
        case 0x17:
            if (f->sp) f->pc = f->ret[--f->sp];
            else vm->depth--;
            break;
        case 0x1a:
        case 0x1c:
            if (in.count >= 2) {
                char *e = sa_utf16_to_utf8(code + o[0].strOffset, o[0].strLen);
                if (!sa_vm_cond(vm, e)) f->pc = (u32)o[1].value;
                free(e);
            }
            break;
        case 0x6e:
            if (in.count >= 2) {
                char *file = str_of(code, &o[1]);
                push_frame(vm, vfs, file, o[0].value, h);
                free(file);
            }
            break;
        case 0x72:
            if (in.count >= 2 && o[0].value >= 0 && o[0].value < 256) {
                free(vm->boxSlot[o[0].value]);
                vm->boxSlot[o[0].value] = str_of(code, &o[1]);
            }
            break;
        case 0x78:
            vm->depth--;
            break;
        case 0xf8:
        case 0xf9:
            if (in.count >= 1) sa_vm_set(vm, &o[0], 0);
            break;
        default:
            if (h->command) h->command(h->ctx, code, &in, vm);
            else if (unknown_once(vm, in.op)) vlog(h, "opcode %02x not handled (%s #%d at %x)", in.op, vm->boxes[f->box].file, f->entry, in.at);
            break;
        }
    }
    return vm->state;
}

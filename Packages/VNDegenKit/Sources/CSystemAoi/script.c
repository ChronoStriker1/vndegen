#include "sa.h"

static bool is_bank(u8 b) { return b == 0x02 || b == 0x04 || b == 0x22 || b == 0x24 || b == 0x44; }

static bool read_operand(const u8 *c, u32 size, u32 *p, Operand *o) {
    u32 q = *p;
    if (q >= size) return false;
    u8 t = c[q];
    memset(o, 0, sizeof *o);
    if ((t == 0x82 || t == 0x92) && q + 5 <= size) {
        o->kind = t == 0x82 ? OPD_INT : OPD_ADDR;
        o->value = (i32)rd32be(c + q + 1);
        *p = q + 5;
        return true;
    }
    if (t == 0x84 && q + 2 <= size) {
        o->kind = OPD_BYTE;
        o->value = c[q + 1];
        *p = q + 2;
        return true;
    }
    if (t == 0x88 && q + 2 <= size && q + 2 + 2u * c[q + 1] <= size) {
        o->kind = OPD_STR;
        o->strLen = c[q + 1];
        o->strOffset = q + 2;
        *p = q + 2 + 2u * o->strLen;
        return true;
    }
    if (is_bank(t) && q + 3 <= size && c[q + 1] < 0x80) {
        o->kind = OPD_VAR;
        o->bank = t;
        o->index = rd16be(c + q + 1);
        *p = q + 3;
        return true;
    }
    if (t == 0x23 && q + 3 <= size && c[q + 1] < 0x80) {
        o->kind = OPD_IVAR;
        o->bank = t;
        o->index = rd16be(c + q + 1);
        *p = q + 3;
        return true;
    }
    return false;
}

bool sa_insn_decode(const u8 *c, u32 size, u32 at, Insn *in) {
    memset(in, 0, sizeof *in);
    if (at >= size) return false;
    in->at = at;
    in->op = c[at];
    u32 p = at + 1;
    if ((in->op == 0x1a || in->op == 0x1c) && p + 2 <= size && p + 2 + 2u * c[p + 1] <= size) {
        Operand *o = &in->o[in->count++];
        o->kind = OPD_COND;
        o->condKind = c[p];
        o->strLen = c[p + 1];
        o->strOffset = p + 2;
        p += 2 + 2u * o->strLen;
    }
    while (in->count < SA_MAX_OPDS && read_operand(c, size, &p, &in->o[in->count])) in->count++;
    in->next = p;
    return true;
}

char *sa_operand_text(const u8 *c, const Operand *o) {
    switch (o->kind) {
    case OPD_INT:
        return sa_fmt("%d", o->value);
    case OPD_BYTE:
        return sa_fmt("b%d", o->value);
    case OPD_ADDR:
        return sa_fmt("@%x", (u32)o->value);
    case OPD_VAR:
        return sa_fmt("%02x:%x", o->bank, o->index);
    case OPD_IVAR:
        return sa_fmt("%02x[%x]", o->bank, o->index);
    case OPD_STR:
    case OPD_COND: {
        char *s = sa_utf16_to_utf8(c + o->strOffset, o->strLen);
        for (char *q = s; *q; q++)
            if (*q == '\t') *q = ' ';
        char *r = sa_fmt(o->kind == OPD_STR ? "\"%s\"" : "<%s>", s);
        free(s);
        return r;
    }
    }
    return sa_strdup("?");
}

char *sa_insn_text(const u8 *c, const Insn *in) {
    char *s = sa_fmt("%06x  %02x", in->at, in->op);
    for (int i = 0; i < in->count; i++) {
        char *t = sa_operand_text(c, &in->o[i]);
        char *n = sa_fmt("%s %s", s, t);
        free(s);
        free(t);
        s = n;
    }
    return s;
}

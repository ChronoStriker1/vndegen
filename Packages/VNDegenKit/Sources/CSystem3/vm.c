// The System 3.5 to 3.9 bytecode interpreter: decoding, expressions, variables and control flow.
// The encoding was learned by compiling probe scripts with AliceSoft's own compilers (Sys39c.exe
// and SYS35C.EXE) and reading the output; docs/system3-cleanroom.md has the details.
#include "vm.h"

// ---- variables ----

static u16 *slot(S3 *s, u16 v, int offset, bool element) {
    static u16 dummy;
    v &= 0x3fff;
    S3Bind *b = &s->bind[v];
    if (b->on) {
        S3ArrayPage *a = &s->arr[b->page];
        u32 i = (u32)b->base + (element ? (u32)offset : (u32)s->vars[b->pointer] + (u32)offset);
        if (a->v && i < a->n) return &a->v[i];
        dummy = 0;
        return &dummy;
    }
    u32 i = (u32)v + (u32)offset;
    if (i < 65536) return &s->vars[i];
    dummy = 0;
    return &dummy;
}

u16 s3_var_get(S3 *s, u16 v) { return *slot(s, v, 0, false); }
void s3_var_set(S3 *s, u16 v, u16 value) { *slot(s, v, 0, false) = value; }

// Where a reference points: page 0 and the variable number for a normal variable, or the array page and the index in
// it for a variable DS bound to an array.
void s3_lv_locate(S3 *s, const S3LV *lv, int *page, u32 *index) {
    u16 v = lv->var & 0x3fff;
    S3Bind *b = &s->bind[v];
    if (b->on) {
        *page = b->page;
        *index = (u32)b->base + (lv->element ? (u32)lv->offset : (u32)s->vars[b->pointer] + (u32)lv->offset);
    } else {
        *page = 0;
        *index = (u32)v + (u32)lv->offset;
    }
}

u16 *s3_lv_at(S3 *s, const S3LV *lv, int k) { return slot(s, lv->var, lv->offset + k, lv->element); }
u16 s3_lv_get(S3 *s, const S3LV *lv, int k) { return *s3_lv_at(s, lv, k); }
void s3_lv_set(S3 *s, const S3LV *lv, int k, u16 v) { *s3_lv_at(s, lv, k) = v; }

// ---- decoding ----

int s3_byte(S3 *s) {
    if (s->pc >= s->codeSize) { s->vmError = true; return 0x7f; }
    return s->code[s->pc++];
}

u32 s3_u32(S3 *s) {
    if (s->pc + 4 > s->codeSize) { s->vmError = true; s->pc = s->codeSize; return 0; }
    u32 v = s3_le32(s->code + s->pc);
    s->pc += 4;
    return v;
}

u16 s3_u16(S3 *s) {
    if (s->pc + 2 > s->codeSize) { s->vmError = true; s->pc = s->codeSize; return 0; }
    u16 v = s3_le16(s->code + s->pc);
    s->pc += 2;
    return v;
}

// Reads a variable reference whose first byte c was already read.
void s3_var_ref(S3 *s, int c, S3LV *lv) {
    memset(lv, 0, sizeof *lv);
    if (c >= 0x80 && c <= 0xbf) { lv->var = (u16)(c - 0x80); return; }
    if (c == 0xc0) {
        int n = s3_byte(s);
        if (n == 1) {
            int hi = s3_byte(s), lo = s3_byte(s);
            lv->var = (u16)(hi << 8 | lo);
            // An index is an expression that may hold indexed variables; real scripts nest a few deep at most.
            if (++s->exprDepth > 32) { s->vmError = true; s->exprDepth--; return; }
            lv->offset = s3_expr(s);
            s->exprDepth--;
            lv->element = true;
            return;
        }
        lv->var = (u16)n;
        return;
    }
    lv->var = (u16)((c - 0xc0) << 8 | s3_byte(s));
}

static u16 sat(long v) { return v < 0 ? 0 : v > 65535 ? 65535 : (u16)v; }

u16 s3_expr(S3 *s) {
    long st[64];
    int sp = 0;
    for (;;) {
        int c = s3_byte(s);
        if (c == 0x7f) break;
        if (sp >= 64) { s->vmError = true; return 0; }
        if (c < 0x40) { st[sp++] = c << 8 | s3_byte(s); continue; }
        // System 3.0 has no bitwise operators, so 0x74 to 0x76 are the constants 52 to 54 there.
        if (c < 0x74 || (s->sys30 && c < 0x77)) { st[sp++] = c - 0x40; continue; }
        int op = c;
        if (c == 0xc0) {
            int n = s->pc < s->codeSize ? s->code[s->pc] : 0;
            if (n >= 2 && n <= 4) { s->pc++; op = 0x100 + n; }
        }
        if (op >= 0x80 && op < 0x100) {
            S3LV lv;
            s3_var_ref(s, c, &lv);
            st[sp++] = s3_lv_get(s, &lv, 0);
            continue;
        }
        if (sp < 2) { s->vmError = true; return 0; }
        long b = st[--sp], a = st[sp - 1], r;
        switch (op) {
        case 0x74: r = a & b; break;
        case 0x75: r = a | b; break;
        case 0x76: r = a ^ b; break;
        case 0x77: r = a * b; break;
        case 0x78: r = b ? a / b : 0; break;
        case 0x79: r = a + b; break;
        case 0x7a: r = a - b; break;
        case 0x7b: r = a == b; break;
        case 0x7c: r = a < b; break;
        case 0x7d: r = a > b; break;
        case 0x7e: r = a != b; break;
        case 0x102: r = b ? a % b : 0; break;
        case 0x103: r = a <= b; break;
        case 0x104: r = a >= b; break;
        default: s->vmError = true; return 0;
        }
        st[sp - 1] = sat(r);
    }
    return sp ? sat(st[sp - 1]) : 0;
}

// An argument that names a variable: a lone variable reference ended by 0x7f.
void s3_lvarg(S3 *s, S3LV *lv) {
    int c = s3_byte(s);
    if (c < 0x80) {
        // A constant where a variable was expected; read and discard it.
        memset(lv, 0, sizeof *lv);
        s->pc--;
        s3_expr(s);
        lv->var = 0;
        return;
    }
    s3_var_ref(s, c, lv);
    if (s3_byte(s) != 0x7f) s->vmError = true;
}

// A NUL-terminated string argument.
const char *s3_strarg(S3 *s) {
    const char *p = (const char *)s->code + s->pc;
    while (s->pc < s->codeSize && s->code[s->pc]) s->pc++;
    if (s->pc < s->codeSize) s->pc++;
    return p;
}

// ---- pages and frames ----

bool s3_goto_page(S3 *s, int page, u32 addr) {
    S3Blob b;
    if (s->sys30) {
        // System 3.0 pages start with a 16-bit field; their labels are offsets in the page.
        if (!s3_dat_get(&s->adisk, (u32)page + 1, &b) || b.size < 2) {
            s3_fail(s, "The game asked for scenario page %d, which ADISK.DAT does not have.", page);
            return false;
        }
        s->page = (u16)page;
        s->code = b.data;
        s->codeSize = (u32)b.size;
        s->pc = addr ? addr : 2;
        return true;
    }
    if (!s3_archive_get(&s->sa, (u32)page + 1, &b, NULL) || b.size < 0x20) {
        s3_fail(s, "The game asked for scenario page %d, which its archive does not have.", page);
        return false;
    }
    s->page = (u16)page;
    s->code = b.data;
    s->codeSize = (u32)b.size;
    u32 hs = s3_le32(b.data + 4);
    s->pc = addr ? addr : hs;
    return true;
}

static void push(S3 *s, S3Frame f) {
    if (s->sp >= S3_MAX_FRAMES) { s3_fail(s, "The game's call stack overflowed."); return; }
    s->stack[s->sp++] = f;
}

void s3_push_var(S3 *s, u16 var) {
    push(s, (S3Frame){ .kind = S3_FRAME_PUSH, .var = var, .value = s3_var_get(s, var) });
}

// Pops variable pushes, then the frame of the wanted kind, and returns to it.
static void ret(S3 *s, bool page) {
    while (s->sp > 0) {
        S3Frame f = s->stack[--s->sp];
        if (f.kind == S3_FRAME_PUSH) { s3_var_set(s, f.var, f.value); continue; }
        if (f.kind == S3_FRAME_TEXT) continue;
        if (f.kind == S3_FRAME_LABEL && page) continue; // a page return also drops label calls
        if (f.kind == S3_FRAME_PAGE && !page) { s->sp++; break; }
        if (f.kind == S3_FRAME_PAGE) s3_goto_page(s, f.page, f.pc);
        else s->pc = f.pc;
        return;
    }
    s3_fail(s, "The game returned from a call that was never made.");
}

void s3_label_call(S3 *s, u32 addr) {
    push(s, (S3Frame){ .kind = S3_FRAME_LABEL, .page = s->page, .pc = s->pc });
    s->pc = addr;
}

void s3_page_call(S3 *s, int page, u32 addr) {
    push(s, (S3Frame){ .kind = S3_FRAME_PAGE, .page = s->page, .pc = s->pc });
    s3_goto_page(s, page, addr);
}

void s3_return(S3 *s, bool page) { ret(s, page); }

// ---- text ----

// Inline text: Shift-JIS and half-width bytes up to the next command.
static void inline_text(S3 *s) {
    char buf[512];
    int n = 0;
    while (s->pc < s->codeSize && n < (int)sizeof buf - 3) {
        u8 c = s->code[s->pc];
        if (s3_sjis_lead(c)) { buf[n++] = (char)c; buf[n++] = (char)s->code[s->pc + 1]; s->pc += 2; }
        else if ((c >= 0xa0 && c <= 0xdf) || c == ' ') { buf[n++] = (char)c; s->pc++; }
        else break;
    }
    buf[n] = 0;
    s3_msg_text(s, buf);
}

void s3_message(S3 *s, u32 index) {
    if (!s->hasAin || index >= s->ain.msgCount) return;
    s3_msg_text(s, s->ain.msgs[index]);
}

// ---- execution ----

static void exec_one(S3 *s) {
    s->cmdStart = s->pc;
    if (s->trace) s3_log(s, "%d:%05x %02x %02x %02x", s->page, s->pc, s->code[s->pc], s->pc + 1 < s->codeSize ? s->code[s->pc + 1] : 0, s->pc + 2 < s->codeSize ? s->code[s->pc + 2] : 0);
    int c = s3_byte(s);
    S3LV lv;
    switch (c) {
    case 0x21: // ! var: expr
        s3_var_ref(s, s3_byte(s), &lv);
        s3_lv_set(s, &lv, 0, s3_expr(s));
        return;
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: {
        s3_var_ref(s, s3_byte(s), &lv);
        long a = s3_lv_get(s, &lv, 0), b = s3_expr(s), r = 0;
        switch (c) {
        case 0x10: r = a + b; break;
        case 0x11: r = a - b; break;
        case 0x12: r = a * b; break;
        case 0x13: r = b ? a / b : 0; break;
        case 0x14: r = b ? a % b : 0; break;
        case 0x15: r = a & b; break;
        case 0x16: r = a | b; break;
        case 0x17: r = a ^ b; break;
        }
        s3_lv_set(s, &lv, 0, sat(r));
        return;
    }
    case 0x7b: { // { cond: ... }
        u16 v = s3_expr(s);
        u32 to = s3_u32(s);
        if (!v) s->pc = to;
        return;
    }
    case 0x40: s->pc = s3_u32(s); return;   // @label
    case 0x3e: s->pc = s3_u32(s); return;   // > loop back
    case 0x5c: {                             // \label (0: return)
        u32 to = s3_u32(s);
        if (to) s3_label_call(s, to);
        else ret(s, false);
        return;
    }
    case 0x26: {                             // &page
        int p = s3_expr(s);
        s3_goto_page(s, p, 0);
        return;
    }
    case 0x25: {                             // %page (0: return)
        int p = s3_expr(s);
        if (p) s3_page_call(s, p, 0);
        else ret(s, true);
        return;
    }
    case 0x3c: {                             // for loop
        int k = s3_byte(s);
        if (k == 0) { s->forFirst = true; return; }
        if (k != 1) { s->vmError = true; return; }
        u32 end = s3_u32(s);
        S3LV v; s3_lvarg(s, &v);
        u16 limit = s3_expr(s), sign = s3_expr(s), step = s3_expr(s);
        u16 cur = s3_lv_get(s, &v, 0);
        if (!s->forFirst) {
            long n = sign ? (long)cur + step : (long)cur - step;
            cur = sat(n);
            s3_lv_set(s, &v, 0, cur);
            if (!sign && n < 0) { s->pc = end; s->forFirst = false; return; }
        }
        s->forFirst = false;
        if (sign ? cur > limit : cur < limit) s->pc = end;
        return;
    }
    case 0x23: {                             // #label,n data pointer
        u32 addr = s3_u32(s);
        u16 n = s3_expr(s);
        s->dataPage = s->page;
        if (n == 0) s->dataPtr = addr;
        else {
            // The label holds a table of addresses written by _label: entries, numbered from 1.
            u64 at = (u64)addr + (u64)(n - 1) * 4;
            s->dataPtr = at + 4 <= s->codeSize ? s3_le32(s->code + at) : 0;
        }
        return;
    }
    case 0x24:                               // $label$ ... $ menu item
        if (s->menuBuild >= 0) { s->menuBuild = -1; return; }
        if (s->menuCount < S3_MAX_MENU) {
            s->menuBuild = s->menuCount++;
            S3MenuItem *m = &s->menu[s->menuBuild];
            m->page = s->page;
            m->addr = s3_u32(s);
            m->text[0] = 0;
        } else {
            s3_u32(s);
            s->menuBuild = S3_MAX_MENU; // keeps text out of the window; never opened
        }
        return;
    case 0x5d:                               // ] open menu
        s3_cmd_open_menu(s);
        return;
    case 0x7e: {                             // ~ function call, return, get return
        u16 k = s3_u16(s);
        if (k == 0) { s->retValue = s3_expr(s); ret(s, true); return; }
        if (k == 0xffff) { s3_lvarg(s, &lv); s3_lv_set(s, &lv, 0, s->retValue); return; }
        u32 addr = s3_u32(s);
        s3_page_call(s, k - 1, addr);
        return;
    }
    case 0x41: s3_cmd_wait_key(s); return;  // A
    case 0x52: s3_msg_newline(s); return;   // R
    default: break;
    }
    if (s3_sjis_lead((u8)c) || (c >= 0xa0 && c <= 0xdf) || c == ' ') { s->pc--; inline_text(s); return; }
    s->pc--;
    s3_command(s);
}

bool s3_vm_start(S3 *s) {
    s->menuBuild = -1;
    return s3_goto_page(s, 0, 0);
}

void s3_vm_run(S3 *s, int budget) {
    s->polls = 0;
    while (budget-- > 0 && !s->quit && s->wait == S3_WAIT_NONE) {
        if (s->pc >= s->codeSize) { s3_fail(s, "Scenario page %d ran past its end.", s->page); return; }
        if (s->sys30) s3_exec30(s); else exec_one(s);
        if (s->vmError) {
            char hex[64] = "";
            for (u32 i = 0; i < 12 && s->cmdStart + i < s->codeSize; i++)
                snprintf(hex + i * 3, sizeof hex - (size_t)i * 3, "%02x ", s->code[s->cmdStart + i]);
            s3_fail(s, "Unknown command at page %d, address 0x%x: %s", s->page, s->cmdStart, hex);
            return;
        }
        if (s->polls > 8) break; // a busy input loop: let the frame end
    }
}

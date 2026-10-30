#include "vm.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "opcodes.h"
#include "sjis.h"
#include "strfmt.h"

namespace s4 {
namespace {

inline float bitsToFloat(int32_t v) { float f; std::memcpy(&f, &v, 4); return f; }
inline int32_t floatToBits(float f) { int32_t v; std::memcpy(&v, &f, 4); return v; }

// Words a value of this type takes on the stack and in a page: a reference to an int, float, bool or lint is a
// (page, index) pair.
bool isRefPair(int32_t t) { return t == T_REF_INT || t == T_REF_FLOAT || t == T_REF_BOOL || t == 59; }
int retWords(int32_t t) { return t == T_VOID ? 0 : isRefPair(t) ? 2 : 1; }

bool isString(int32_t t) { return t == T_STRING; }
bool isDelegate(int32_t t) { return t == 63; }
bool isStructValue(int32_t t) { return t == T_STRUCT; }
bool isArray(int32_t t) {
    return t == T_ARRAY_INT || t == T_ARRAY_FLOAT || t == T_ARRAY_STRING || t == T_ARRAY_STRUCT ||
           t == T_ARRAY_FUNCTYPE || t == T_ARRAY_BOOL || t == 58 || t == 67;
}
// Reference types that hold a slot and keep it alive.
bool isRefSlot(int32_t t) {
    return t == T_REF_STRING || t == T_REF_STRUCT || (t >= T_REF_ARRAY_INT && t <= T_REF_ARRAY_STRUCT) ||
           t == T_REF_ARRAY_FUNCTYPE || t == T_REF_ARRAY_BOOL;
}
// The base type of an array's elements.
int32_t arrayElem(int32_t t) {
    switch (t) {
    case T_ARRAY_INT: case T_REF_ARRAY_INT: return T_INT;
    case T_ARRAY_FLOAT: case T_REF_ARRAY_FLOAT: return T_FLOAT;
    case T_ARRAY_STRING: case T_REF_ARRAY_STRING: return T_STRING;
    case T_ARRAY_STRUCT: case T_REF_ARRAY_STRUCT: return T_STRUCT;
    case T_ARRAY_FUNCTYPE: case T_REF_ARRAY_FUNCTYPE: return T_FUNCTYPE;
    case T_ARRAY_BOOL: case T_REF_ARRAY_BOOL: return T_BOOL;
    case 58: return 55;
    case 67: return 63;
    default: return T_INT;
    }
}

}  // namespace

std::vector<void (*)()> &HllState::resets() {
    static std::vector<void (*)()> r;
    return r;
}

std::unordered_map<std::string, HllImpl> &HllRegistry::table() {
    static std::unordered_map<std::string, HllImpl> t;
    return t;
}

// ---- HllCall ----

float HllCall::f(int k) const { return bitsToFloat(i(k)); }
const std::string &HllCall::s(int k) const {
    static const std::string empty;
    int32_t sl = i(k);
    return vm.valid(sl) && vm.heap[sl].str ? *vm.heap[sl].str : empty;
}
int32_t &HllCall::ref(int k) const {
    static int32_t dummy;
    int32_t pg = words[argWord[k]], idx = words[argWord[k] + 1];
    Page *p = vm.page(pg);
    if (!p || idx < 0 || size_t(idx) >= p->v.size()) { dummy = 0; return dummy; }
    return p->v[idx];
}
float &HllCall::refF(int k) const { return *reinterpret_cast<float *>(&ref(k)); }
std::string &HllCall::refS(int k) const {
    static std::string dummy;
    int32_t sl = i(k);
    if (vm.valid(sl) && vm.heap[sl].str) return *vm.heap[sl].str;
    dummy.clear();
    return dummy;
}
Page *HllCall::page(int k) const { return vm.page(i(k)); }
void HllCall::retFloat(float v) { retInt(floatToBits(v)); }
void HllCall::retString(const std::string &s) { retInt(vm.newString(s)); }

// ---- VM ----

VM::VM(Ain a) : ain(std::move(a)) {
    heap.reserve(4096);
    auto &table = HllRegistry::table();
    for (auto &lib : ain.libraries) {
        std::vector<HllImpl> fns;
        // SACT2 is StoatSpriteEngine's predecessor; a call they share, with the same arguments, runs Stoat's code.
        std::string alias = lib.name == "SACT2" ? "StoatSpriteEngine" : "";
        for (auto &fn : lib.functions) {
            auto it = table.find(lib.name + "." + fn.name);
            if (it == table.end() && !alias.empty()) it = table.find(alias + "." + fn.name);
            fns.push_back(it == table.end() ? nullptr : it->second);
        }
        hll_.push_back(std::move(fns));
    }
}

std::vector<std::string> VM::unboundHll() const {
    std::vector<std::string> out;
    for (size_t l = 0; l < hll_.size(); l++)
        for (size_t f = 0; f < hll_[l].size(); f++)
            if (!hll_[l][f]) out.push_back(ain.libraries[l].name + "." + ain.libraries[l].functions[f].name);
    return out;
}

int32_t VM::alloc() {
    int32_t s;
    if (!freeSlots.empty()) {
        s = freeSlots.back();
        freeSlots.pop_back();
    } else {
        s = int32_t(heap.size());
        heap.emplace_back();
    }
    heap[s].inUse = true;
    heap[s].refs = 1;
    return s;
}

int32_t VM::newPage(PageKind kind, int32_t index, size_t size) {
    int32_t s = alloc();
    auto p = std::make_unique<Page>();
    p->kind = kind;
    p->index = index;
    p->v.assign(size, 0);
    heap[s].page = std::move(p);
    return s;
}

int32_t VM::newString(std::string str) {
    int32_t s = alloc();
    heap[s].str = std::make_unique<std::string>(std::move(str));
    return s;
}

int32_t VM::newDelegate(Delegate d) {
    int32_t s = alloc();
    for (auto &e : d) incref(e.first);
    heap[s].dg = std::make_unique<Delegate>(std::move(d));
    return s;
}

Delegate *VM::delegate(int32_t slot) { return valid(slot) ? heap[slot].dg.get() : nullptr; }

Page *VM::page(int32_t slot) { return valid(slot) ? heap[slot].page.get() : nullptr; }

std::string &VM::str(int32_t slot) {
    static std::string dummy;
    if (valid(slot) && heap[slot].str) return *heap[slot].str;
    dummy.clear();
    return dummy;
}

void VM::incref(int32_t slot) {
    if (valid(slot)) heap[slot].refs++;
}

void VM::decref(int32_t slot) {
    if (!valid(slot)) return;
    if (--heap[slot].refs > 0) return;
    Slot &s = heap[slot];
    if (s.dg) {
        std::unique_ptr<Delegate> d = std::move(s.dg);
        s.inUse = false;
        for (auto &e : *d) decref(e.first);
        freeSlots.push_back(slot);
        return;
    }
    if (s.page) {
        Page &p = *s.page;
        if (p.kind == PageKind::Struct && p.index >= 0 && size_t(p.index) < ain.structs.size()) {
            int32_t dtor = ain.structs[p.index].destructor;
            if (dtor > 0) {
                s.refs = 1;  // keep it alive while the destructor runs
                call(dtor, {}, slot);
                if (--s.refs > 0) return;
            }
        }
        std::unique_ptr<Page> owned = std::move(s.page);
        s.inUse = false;
        freePage(*owned);
    } else {
        s.str.reset();
        s.inUse = false;
    }
    freeSlots.push_back(slot);
}

VarType VM::varType(const Page &p, size_t i) const {
    switch (p.kind) {
    case PageKind::Global:
        return i < ain.globals.size() ? ain.globals[i].t : VarType{};
    case PageKind::Local:
        if (p.index >= 0 && size_t(p.index) < ain.functions.size() && i < ain.functions[p.index].vars.size())
            return ain.functions[p.index].vars[i].t;
        return {};
    case PageKind::Struct:
        if (p.index >= 0 && size_t(p.index) < ain.structs.size() && i < ain.structs[p.index].members.size())
            return ain.structs[p.index].members[i].t;
        return {};
    case PageKind::Array: {
        VarType t;
        if (p.rank > 1) {
            t.type = T_ARRAY_INT;  // a sub-array: freed as a page
            t.rank = p.rank - 1;
        } else {
            t.type = p.elemType;
        }
        t.structType = p.elemStruct;
        return t;
    }
    }
    return {};
}

void VM::releaseValue(const VarType &t, int32_t v) {
    if (isString(t.type) || isStructValue(t.type) || isArray(t.type) || isRefSlot(t.type) || isDelegate(t.type)) {
        decref(v);
    }
}

void VM::freePage(Page &p) {
    for (size_t i = 0; i < p.v.size(); i++) {
        VarType t = varType(p, i);
        if (isRefPair(t.type)) {
            decref(p.v[i]);
            i++;
            continue;
        }
        releaseValue(t, p.v[i]);
    }
}

void VM::initVar(Page &p, size_t i, const VarType &t) {
    if (isDelegate(t.type)) {
        p.v[i] = newDelegate();
    } else if (isString(t.type)) {
        p.v[i] = newString();
    } else if (isArray(t.type)) {
        int32_t s = newPage(PageKind::Array, -1, 0);
        Page *a = heap[s].page.get();
        a->elemType = arrayElem(t.type);
        a->elemStruct = t.structType;
        a->rank = std::max(1, t.rank);
        p.v[i] = s;
    } else if (isStructValue(t.type)) {
        // Struct locals are made by SH_LOCALCREATE; globals and members are made here.
        p.v[i] = p.kind == PageKind::Local ? -1 : newStruct(t.structType);
    } else if (isRefSlot(t.type) || t.type == T_REF_STRUCT) {
        p.v[i] = -1;
    } else if (isRefPair(t.type)) {
        p.v[i] = -1;
        if (i + 1 < p.v.size()) p.v[i + 1] = 0;
    } else {
        p.v[i] = 0;
    }
}

int32_t VM::newStruct(int32_t type) {
    if (type < 0 || size_t(type) >= ain.structs.size()) return -1;
    const Struct &st = ain.structs[type];
    int32_t s = newPage(PageKind::Struct, type, st.members.size());
    for (size_t i = 0; i < st.members.size(); i++) {
        Page &p = *heap[s].page;  // re-read: nested allocation can move the heap
        initVar(p, i, st.members[i].t);
        if (isRefPair(st.members[i].t.type)) i++;
    }
    if (st.constructor > 0) call(st.constructor, {}, s);
    return s;
}

int32_t VM::newArray(int32_t elemType, int32_t elemStruct, int32_t rank, const int32_t *dims, int ndims) {
    int32_t n = ndims > 0 ? std::max(0, dims[0]) : 0;
    int32_t s = newPage(PageKind::Array, -1, n);
    {
        Page &a = *heap[s].page;
        a.elemType = elemType;
        a.elemStruct = elemStruct;
        a.rank = rank;
    }
    for (int32_t i = 0; i < n; i++) {
        int32_t v;
        if (rank > 1) v = newArray(elemType, elemStruct, rank - 1, dims + 1, ndims - 1);
        else if (elemType == T_STRING) v = newString();
        else if (elemType == T_STRUCT) v = newStruct(elemStruct);
        else if (elemType == 63) v = newDelegate();
        else v = 0;
        heap[s].page->v[i] = v;
    }
    return s;
}

int32_t VM::copyPage(int32_t slot) {
    Page *src = page(slot);
    if (!src) return -1;
    int32_t s = newPage(src->kind, src->index, src->v.size());
    {
        Page &d = *heap[s].page;
        Page &sp = *heap[slot].page;
        d.elemType = sp.elemType;
        d.elemStruct = sp.elemStruct;
        d.rank = sp.rank;
    }
    size_t n = heap[s].page->v.size();
    for (size_t i = 0; i < n; i++) {
        Page &sp = *heap[slot].page;
        VarType t = varType(sp, i);
        int32_t v = sp.v[i];
        int32_t out = v;
        if (isRefPair(t.type)) {
            incref(v);
            heap[s].page->v[i] = v;
            heap[s].page->v[i + 1] = sp.v[i + 1];
            i++;
            continue;
        }
        if (isString(t.type)) out = newString(str(v));
        else if (isDelegate(t.type)) out = newDelegate(delegate(v) ? *delegate(v) : Delegate{});
        else if (isStructValue(t.type) || isArray(t.type)) out = valid(v) ? copyPage(v) : -1;
        else if (isRefSlot(t.type)) incref(v);
        heap[s].page->v[i] = out;
    }
    return s;
}

void VM::structAssign(int32_t dst, int32_t src) {
    if (dst == src || !page(dst) || !page(src)) return;
    int32_t copy = copyPage(src);
    std::swap(heap[dst].page->v, heap[copy].page->v);
    decref(copy);  // frees the old members, now in the copy
}

int32_t VM::pop() {
    if (stack.empty()) fatal("stack underflow");
    int32_t v = stack.back();
    stack.pop_back();
    return v;
}
float VM::popF() { return bitsToFloat(pop()); }
void VM::pushF(float f) { push(floatToBits(f)); }

std::string VM::functionName(int32_t f) const {
    return f >= 0 && size_t(f) < ain.functions.size() ? sjis::toUtf8(ain.functions[f].name) : "?";
}

void VM::fatal(const std::string &msg) {
    std::string where = frames.empty() ? "" : " in " + functionName(frames.back().function);
    throw VMExit{-1, msg + where + " at " + std::to_string(ip)};
}

void VM::enter(int32_t function, int32_t thisPage, bool native) {
    if (function <= 0 || size_t(function) >= ain.functions.size()) fatal("call to bad function " + std::to_string(function));
    const Function &f = ain.functions[function];
    if (stack.size() < size_t(f.argc)) fatal("stack underflow calling " + functionName(function));
    int32_t lp = newPage(PageKind::Local, function, f.vars.size());
    Page &p = *heap[lp].page;
    size_t base = stack.size() - f.argc;
    for (int k = 0; k < f.argc; k++) p.v[k] = stack[base + k];
    stack.resize(base);
    for (size_t k = 0; k < f.vars.size(); k++) {
        const VarType &t = f.vars[k].t;
        if (int(k) < f.argc) {
            // Arguments arrive as values the callee now owns, except references, which share their target.
            if (isRefSlot(t.type)) incref(p.v[k]);
            if (isRefPair(t.type)) { incref(p.v[k]); k++; }
            continue;
        }
        initVar(*heap[lp].page, k, t);
        if (isRefPair(t.type)) k++;
    }
    if (thisPage >= 0) incref(thisPage);
    if (trace) trace(std::string(frames.size(), ' ') + functionName(function) + (thisPage >= 0 ? " this=" + std::to_string(thisPage) : ""));
    frames.push_back(Frame{function, lp, thisPage, ip, stack.size(), native, native ? 0 : curInsn_, thisPage >= 0});
    ip = f.address;
}

void VM::leave() {
    Frame fr = frames.back();
    const Function &f = ain.functions[fr.function];
    int n = retWords(f.ret.type);
    if (stack.size() < size_t(n) || (fr.stackBase != kUnknownBase && stack.size() < fr.stackBase + n)) fatal("stack underflow returning from " + functionName(fr.function));
    std::vector<int32_t> ret(stack.end() - n, stack.end());
    stack.resize(stack.size() - n);
    // Values left above the frame's base are temporaries; drop them. Frames restored from a resume save don't
    // know their base.
    if (fr.stackBase != kUnknownBase && stack.size() > fr.stackBase) stack.resize(fr.stackBase);
    frames.pop_back();
    decref(fr.localPage);
    if (fr.structPage >= 0) decref(fr.structPage);
    if (!fr.discardReturn)
        for (int32_t v : ret) stack.push_back(v);
    ip = fr.returnAddress;
}

int32_t VM::call(int32_t function, const std::vector<int32_t> &args, int32_t thisPage) {
    size_t depth = frames.size();
    uint32_t savedIp = ip;
    for (int32_t a : args) push(a);
    enter(function, thisPage, true);
    frames.back().returnAddress = savedIp;
    run(depth);
    int n = retWords(ain.functions[function].ret.type);
    int32_t r = 0;
    if (n >= 1) {
        r = stack[stack.size() - n];
        stack.resize(stack.size() - n);
    }
    ip = savedIp;
    return r;
}

void VM::start() {
    // Globals get their plain values and GSET initial values first, then the compiler's array allocations in the
    // function named "0", then struct globals in declaration order, whose constructors may use the rest.
    globalPage = newPage(PageKind::Global, -1, ain.globals.size());
    for (size_t i = 0; i < ain.globals.size(); i++) {
        const VarType &t = ain.globals[i].t;
        if (isStructValue(t.type)) heap[globalPage].page->v[i] = -1;
        else initVar(*heap[globalPage].page, i, t);
        if (isRefPair(t.type)) i++;
    }
    for (auto &g : ain.globalInits) {
        Page &p = *heap[globalPage].page;
        if (g.global < 0 || size_t(g.global) >= p.v.size()) continue;
        if (g.type == T_STRING) str(p.v[g.global]) = g.strValue;
        else p.v[g.global] = g.intValue;
    }
    int init = ain.findFunction("0");
    if (init > 0) call(init, {});
    for (size_t i = 0; i < ain.globals.size(); i++)
        if (isStructValue(ain.globals[i].t.type)) {
            int32_t s = newStruct(ain.globals[i].t.structType);
            heap[globalPage].page->v[i] = s;
        }
}

int32_t VM::arg(int k) const {
    int32_t v;
    std::memcpy(&v, ain.code.data() + ip + 2 + 4 * k, 4);
    return v;
}

void VM::callHll(int32_t lib, int32_t fn) {
    if (lib < 0 || size_t(lib) >= ain.libraries.size() || fn < 0 || size_t(fn) >= ain.libraries[lib].functions.size())
        fatal("bad HLL call");
    const HllFunction &decl = ain.libraries[lib].functions[fn];
    HllCall c{*this, decl, {}, {}};
    int words = 0;
    for (auto &a : decl.args) words += isRefPair(a.t.type) ? 2 : (a.t.type == T_VOID ? 0 : 1);
    if (stack.size() < size_t(words)) fatal("stack underflow calling " + ain.libraries[lib].name + "." + decl.name);
    c.words.assign(stack.end() - words, stack.end());
    stack.resize(stack.size() - words);
    int w = 0;
    for (auto &a : decl.args) {
        c.argWord.push_back(w);
        w += isRefPair(a.t.type) ? 2 : (a.t.type == T_VOID ? 0 : 1);
    }
    if (hllTrace) {
        std::string t = ain.libraries[lib].name + "." + decl.name;
        for (int32_t v : c.words) t += " " + std::to_string(v);
        hllTrace(t);
    }
    HllImpl impl = hll_[lib][fn];
    if (impl) {
        impl(c);
    } else {
        std::string name = ain.libraries[lib].name + "." + decl.name;
        if (hllWarned_.insert(name).second && log) log("unimplemented HLL " + name);
    }
    // Arguments passed by value as slots belong to the call.
    for (size_t k = 0; k < decl.args.size(); k++) {
        int32_t t = decl.args[k].t.type;
        if (t == T_STRING || t == T_STRUCT) decref(c.words[c.argWord[k]]);
    }
    int rw = retWords(decl.retType);
    if (rw == 1) {
        if (!c.retSet && decl.retType == T_STRING) c.ret = newString();
        if (!c.retSet && decl.retType == T_STRUCT) c.ret = -1;
        push(c.ret);
    }
}

void VM::run(size_t untilFrames) {
    const std::vector<uint8_t> &code = ain.code;
    while (frames.size() > untilFrames) {
        if (ip + 2 > code.size()) fatal("ran off the end of the code");
        uint16_t op = code[ip] | code[ip + 1] << 8;
        const OpInfo *oi = opInfo(op);
        if (!oi) fatal("unknown opcode " + std::to_string(op));
        uint32_t next = ip + 2 + 4 * oi->args;
        curInsn_ = ip;
        int32_t args[3] = {0, 0, 0};
        std::memcpy(args, code.data() + ip + 2, 4 * oi->args);
        auto A = [&](int k) { return args[k]; };
        Frame &fr = frames.back();
        auto localv = [&](int32_t i) -> int32_t & { return heap[fr.localPage].page->v.at(i); };
        auto thisv = [&](int32_t i) -> int32_t & {
            Page *p = page(fr.structPage);
            if (!p) fatal("no struct for member access");
            return p->v.at(i);
        };
        auto globv = [&](int32_t i) -> int32_t & { return heap[globalPage].page->v.at(i); };
        auto var = [&](int32_t pg, int32_t i) -> int32_t & {
            Page *p = page(pg);
            if (!p || i < 0 || size_t(i) >= p->v.size()) fatal("bad variable reference " + std::to_string(pg) + ":" + std::to_string(i));
            return p->v[i];
        };
        auto arrayAt = [&](int32_t pg, int32_t i) -> Page * {
            int32_t &slot = var(pg, i);
            Page *a = page(slot);
            if (!a) {
                // An array variable freed or never made: make an empty one of the declared type.
                VarType t = varType(*page(pg), i);
                int32_t s = newPage(PageKind::Array, -1, 0);
                Page *np = heap[s].page.get();
                np->elemType = arrayElem(t.type);
                np->elemStruct = t.structType;
                np->rank = std::max(1, t.rank);
                var(pg, i) = s;
                a = np;
            }
            return a;
        };
        auto releaseElem = [&](Page &a, int32_t v) {
            if (a.rank > 1 || a.elemType == T_STRING || a.elemType == T_STRUCT || a.elemType == 63) decref(v);
        };
        auto jump = [&](uint32_t to) { next = to; };
        auto binop = [&](auto f) { int32_t b = pop(), a = pop(); push(f(a, b)); };
        auto fbinop = [&](auto f) { float b = popF(), a = popF(); push(f(a, b)); };
        auto assignop = [&](auto f) {
            int32_t v = pop(), i = pop(), pg = pop();
            int32_t &x = var(pg, i);
            x = f(x, v);
            push(x);
        };
        auto fassignop = [&](auto f) {
            float v = popF();
            int32_t i = pop(), pg = pop();
            int32_t &x = var(pg, i);
            x = floatToBits(f(bitsToFloat(x), v));
            push(x);
        };
        auto sdiv = [&](int32_t a, int32_t b) { return b == 0 ? 0 : (b == -1 ? -a : a / b); };
        auto smod = [&](int32_t a, int32_t b) { return b == 0 || b == -1 ? 0 : a % b; };
        auto callMethodOn = [&](int32_t thisPg, int32_t fn) {
            const Function &f = ain.functions.at(fn);
            // CALLMETHOD's `this` sits below the arguments.
            if (stack.size() < size_t(f.argc)) fatal("stack underflow");
            stack.insert(stack.end() - f.argc, thisPg);
            int32_t t = stack[stack.size() - f.argc - 1];
            stack.erase(stack.end() - f.argc - 1);
            ip = next;
            enter(fn, t, false);
            next = ip;
        };

        switch (op) {
        case OP_PUSH: push(A(0)); break;
        case OP_POP: pop(); break;
        case OP_REF: { int32_t i = pop(), pg = pop(); push(var(pg, i)); break; }
        case OP_REFREF: { int32_t i = pop(), pg = pop(); push(var(pg, i)); push(var(pg, i + 1)); break; }
        case OP_PUSHGLOBALPAGE: push(globalPage); break;
        case OP_PUSHLOCALPAGE: push(fr.localPage); break;
        case OP_INV: push(-pop()); break;
        case OP_NOT: push(!pop()); break;
        case OP_COMPL: push(~pop()); break;
        case OP_ADD: case OP_LI_ADD: binop([](int32_t a, int32_t b) { return int32_t(uint32_t(a) + uint32_t(b)); }); break;
        case OP_SUB: case OP_LI_SUB: binop([](int32_t a, int32_t b) { return int32_t(uint32_t(a) - uint32_t(b)); }); break;
        case OP_MUL: case OP_LI_MUL: binop([](int32_t a, int32_t b) { return int32_t(uint32_t(a) * uint32_t(b)); }); break;
        case OP_DIV: case OP_LI_DIV: binop(sdiv); break;
        case OP_MOD: case OP_LI_MOD: binop(smod); break;
        case OP_AND: binop([](int32_t a, int32_t b) { return a & b; }); break;
        case OP_OR: binop([](int32_t a, int32_t b) { return a | b; }); break;
        case OP_XOR: binop([](int32_t a, int32_t b) { return a ^ b; }); break;
        case OP_LSHIFT: binop([](int32_t a, int32_t b) { return int32_t(uint32_t(a) << (b & 31)); }); break;
        case OP_RSHIFT: binop([](int32_t a, int32_t b) { return a >> (b & 31); }); break;
        case OP_LT: binop([](int32_t a, int32_t b) { return int32_t(a < b); }); break;
        case OP_GT: binop([](int32_t a, int32_t b) { return int32_t(a > b); }); break;
        case OP_LTE: binop([](int32_t a, int32_t b) { return int32_t(a <= b); }); break;
        case OP_GTE: binop([](int32_t a, int32_t b) { return int32_t(a >= b); }); break;
        case OP_NOTE: binop([](int32_t a, int32_t b) { return int32_t(a != b); }); break;
        case OP_EQUALE: binop([](int32_t a, int32_t b) { return int32_t(a == b); }); break;
        case OP_ASSIGN: case OP_LI_ASSIGN: assignop([](int32_t, int32_t v) { return v; }); break;
        case OP_PLUSA: case OP_LI_PLUSA: assignop([](int32_t x, int32_t v) { return int32_t(uint32_t(x) + uint32_t(v)); }); break;
        case OP_MINUSA: case OP_LI_MINUSA: assignop([](int32_t x, int32_t v) { return int32_t(uint32_t(x) - uint32_t(v)); }); break;
        case OP_MULA: case OP_LI_MULA: assignop([](int32_t x, int32_t v) { return int32_t(uint32_t(x) * uint32_t(v)); }); break;
        case OP_DIVA: case OP_LI_DIVA: assignop(sdiv); break;
        case OP_MODA: case OP_LI_MODA: assignop(smod); break;
        case OP_ANDA: case OP_LI_ANDA: assignop([](int32_t x, int32_t v) { return x & v; }); break;
        case OP_ORA: case OP_LI_ORA: assignop([](int32_t x, int32_t v) { return x | v; }); break;
        case OP_XORA: case OP_LI_XORA: assignop([](int32_t x, int32_t v) { return x ^ v; }); break;
        case OP_LSHIFTA: case OP_LI_LSHIFTA: assignop([](int32_t x, int32_t v) { return int32_t(uint32_t(x) << (v & 31)); }); break;
        case OP_RSHIFTA: case OP_LI_RSHIFTA: assignop([](int32_t x, int32_t v) { return x >> (v & 31); }); break;
        case OP_F_ASSIGN: fassignop([](float, float v) { return v; }); break;
        case OP_F_PLUSA: fassignop([](float x, float v) { return x + v; }); break;
        case OP_F_MINUSA: fassignop([](float x, float v) { return x - v; }); break;
        case OP_F_MULA: fassignop([](float x, float v) { return x * v; }); break;
        case OP_F_DIVA: fassignop([](float x, float v) { return v == 0 ? 0.0f : x / v; }); break;
        case OP_DUP2: { int32_t b = stack.at(stack.size() - 1), a = stack.at(stack.size() - 2); push(a); push(b); break; }
        case OP_DUP_X2: { int32_t c = pop(), b = pop(), a = pop(); push(c); push(a); push(b); push(c); break; }
        case OP_DUP2_X1: { int32_t c = pop(), b = pop(), a = pop(); push(b); push(c); push(a); push(b); push(c); break; }
        case OP_DUP: push(stack.at(stack.size() - 1)); break;
        case OP_DUP_U2: push(stack.at(stack.size() - 2)); break;
        case OP_SWAP: { int32_t b = pop(), a = pop(); push(b); push(a); break; }
        case OP_JUMP: jump(A(0)); break;
        case OP_IFZ: if (!pop()) jump(A(0)); break;
        case OP_IFNZ: if (pop()) jump(A(0)); break;
        case OP_RETURN: leave(); next = ip; break;
        case OP_CALLFUNC: ip = next; enter(A(0), -1, false); next = ip; break;
        case OP_CALLMETHOD: {
            const Function &f = ain.functions.at(A(0));
            if (stack.size() < size_t(f.argc) + 1) fatal("stack underflow");
            int32_t t = stack[stack.size() - f.argc - 1];
            stack.erase(stack.end() - f.argc - 1);
            ip = next;
            enter(A(0), t, false);
            next = ip;
            break;
        }
        case OP_CALLFUNC2: {
            pop();  // the function type, or -1
            int32_t fn = pop();
            if (fn <= 0 || size_t(fn) >= ain.functions.size()) fatal("call through a null function pointer");
            ip = next;
            enter(fn, -1, false);
            next = ip;
            break;
        }
        case OP_INC: case OP_LI_INC: { int32_t i = pop(), pg = pop(); var(pg, i)++; break; }
        case OP_DEC: case OP_LI_DEC: { int32_t i = pop(), pg = pop(); var(pg, i)--; break; }
        case OP_FTOI: push(int32_t(popF())); break;
        case OP_ITOF: pushF(float(pop())); break;
        case OP_ITOLI: break;
        case OP_F_INV: pushF(-popF()); break;
        case OP_F_ADD: fbinop([](float a, float b) { return floatToBits(a + b); }); break;
        case OP_F_SUB: fbinop([](float a, float b) { return floatToBits(a - b); }); break;
        case OP_F_MUL: fbinop([](float a, float b) { return floatToBits(a * b); }); break;
        case OP_F_DIV: fbinop([](float a, float b) { return floatToBits(b == 0 ? 0.0f : a / b); }); break;
        case OP_F_LT: fbinop([](float a, float b) { return int32_t(a < b); }); break;
        case OP_F_GT: fbinop([](float a, float b) { return int32_t(a > b); }); break;
        case OP_F_LTE: fbinop([](float a, float b) { return int32_t(a <= b); }); break;
        case OP_F_GTE: fbinop([](float a, float b) { return int32_t(a >= b); }); break;
        case OP_F_NOTE: fbinop([](float a, float b) { return int32_t(a != b); }); break;
        case OP_F_EQUALE: fbinop([](float a, float b) { return int32_t(a == b); }); break;
        case OP_F_PUSH: push(A(0)); break;

        // Strings. A string value on the stack is a slot; operations consume their operands and push new ones.
        case OP_S_PUSH: push(newString(ain.strings.at(A(0)))); break;
        case OP_S_POP: decref(pop()); break;
        case OP_S_ADD: { int32_t b = pop(), a = pop(); push(newString(str(a) + str(b))); decref(a); decref(b); break; }
        case OP_S_ASSIGN: { int32_t src = pop(), dst = pop(); str(dst) = str(src); decref(src); push(newString(str(dst))); break; }
        case OP_S_PLUSA: case OP_S_PLUSA2: { int32_t src = pop(), dst = pop(); str(dst) += str(src); decref(src); push(newString(str(dst))); break; }
        case OP_S_REF: { int32_t i = pop(), pg = pop(); push(newString(str(var(pg, i)))); break; }
        case OP_S_REFREF: { int32_t i = pop(), pg = pop(); push(var(pg, i)); push(0); break; }
        case OP_S_NOTE: case OP_S_EQUALE: case OP_S_LT: case OP_S_GT: case OP_S_LTE: case OP_S_GTE: {
            int32_t b = pop(), a = pop();
            int c = str(a).compare(str(b));
            int32_t r = op == OP_S_NOTE ? c != 0 : op == OP_S_EQUALE ? c == 0 : op == OP_S_LT ? c < 0 : op == OP_S_GT ? c > 0 : op == OP_S_LTE ? c <= 0 : c >= 0;
            decref(a);
            decref(b);
            push(r);
            break;
        }
        case OP_S_LENGTH: { int32_t i = pop(), pg = pop(); push(strfmt::charCount(str(var(pg, i)))); break; }
        case OP_S_LENGTHBYTE: { int32_t i = pop(), pg = pop(); push(int32_t(str(var(pg, i)).size())); break; }
        case OP_S_LENGTH2: { int32_t s = pop(); push(strfmt::charCount(str(s))); decref(s); break; }
        case OP_S_LENGTHBYTE2: { int32_t s = pop(); push(int32_t(str(s).size())); decref(s); break; }
        case OP_S_EMPTY: { int32_t s = pop(); push(str(s).empty()); decref(s); break; }
        case OP_STOI: { int32_t s = pop(); push(strfmt::toInt(str(s))); decref(s); break; }
        case OP_I_STRING: push(newString(std::to_string(pop()))); break;
        case OP_FTOS: { int32_t prec = pop(); float f = popF(); push(newString(strfmt::floatToString(f, prec))); break; }
        case OP_S_FIND: { int32_t b = pop(), a = pop(); push(strfmt::find(str(a), str(b))); decref(a); decref(b); break; }
        case OP_S_GETPART: {
            int32_t len = pop(), start = pop(), s = pop();
            push(newString(strfmt::substr(str(s), start, len)));
            decref(s);
            break;
        }
        case OP_S_PUSHBACK2: { int32_t ch = pop(), s = pop(); strfmt::pushChar(str(s), ch); break; }
        case OP_S_POPBACK2: { int32_t s = pop(); strfmt::popChar(str(s)); break; }
        case OP_S_ERASE2: { int32_t n = pop(), i = pop(), s = pop(); strfmt::erase(str(s), i, n); break; }
        case OP_S_MOD: {
            int32_t type = pop(), v = pop(), fmt = pop();
            std::string out = strfmt::formatOne(str(fmt), type, v, type == 4 ? str(v) : std::string());
            if (type == 4) decref(v);
            decref(fmt);
            push(newString(std::move(out)));
            break;
        }
        case OP_C_REF: { int32_t i = pop(), s = pop(); push(strfmt::charAt(str(s), i)); break; }
        case OP_C_ASSIGN: { int32_t ch = pop(), i = pop(), s = pop(); strfmt::setChar(str(s), i, ch); push(ch); break; }

        // Structs.
        case OP_SR_POP: decref(pop()); break;
        case OP_SR_ASSIGN: { pop(); int32_t src = pop(), dst = pop(); structAssign(dst, src); decref(src); incref(dst); push(dst); break; }
        case OP_SR_REF: { int32_t i = pop(), pg = pop(); push(copyPage(var(pg, i))); break; }
        case OP_SR_REF2: { int32_t s = pop(); push(copyPage(s)); break; }
        case OP_SR_REFREF: { int32_t i = pop(), pg = pop(); push(var(pg, i)); push(0); break; }
        case OP_SH_SR_ASSIGN: { int32_t i = pop(), pg = pop(), dst = pop(); structAssign(dst, var(pg, i)); break; }
        case OP_NEW: { int32_t t = pop(); push(newStruct(t)); break; }
        case OP_DELETE: decref(pop()); break;
        case OP_SP_INC: incref(pop()); break;
        case OP_SP_DEC: decref(pop()); break;
        case OP_PUSHSTRUCTPAGE: push(fr.structPage); break;
        case OP_SH_LOCALCREATE: { int32_t s = newStruct(A(1)); decref(localv(A(0))); localv(A(0)) = s; break; }
        case OP_SH_LOCALDELETE: { int32_t old = localv(A(0)); localv(A(0)) = -1; decref(old); break; }
        case OP_R_ASSIGN: {
            int32_t si = pop(), sp = pop(), di = pop(), dp = pop();
            var(dp, di) = sp;
            var(dp, di + 1) = si;
            push(sp);
            push(si);
            break;
        }
        case OP_R_EQUALE: case OP_R_NOTE: {
            int32_t bi = pop(), bp = pop(), ai = pop(), ap = pop();
            bool eq = ap == bp && ai == bi;
            push(op == OP_R_EQUALE ? eq : !eq);
            break;
        }
        case OP_OBJSWAP: {
            pop();  // type
            int32_t bi = pop(), bp = pop(), ai = pop(), ap = pop();
            std::swap(var(ap, ai), var(bp, bi));
            break;
        }

        // Arrays.
        case OP_A_ALLOC: case OP_A_REALLOC: {
            int32_t nd = pop();
            std::vector<int32_t> dims(std::max(0, nd));
            for (int k = nd - 1; k >= 0; k--) dims[k] = pop();
            int32_t i = pop(), pg = pop();
            Page *a = arrayAt(pg, i);
            int32_t et = a->elemType, es = a->elemStruct, rk = a->rank;
            if (op == OP_A_REALLOC && nd >= 1 && rk == 1) {
                int32_t n = std::max(0, dims[0]);
                int32_t s = var(pg, i);
                Page &ap = *heap[s].page;
                size_t old = ap.v.size();
                if (size_t(n) < old) {
                    std::vector<int32_t> drop(ap.v.begin() + n, ap.v.end());
                    ap.v.resize(n);
                    for (int32_t v : drop) releaseElem(*heap[s].page, v);
                } else {
                    for (size_t k = old; k < size_t(n); k++) {
                        int32_t v = et == T_STRING ? newString() : et == T_STRUCT ? newStruct(es) : 0;
                        heap[s].page->v.push_back(v);
                    }
                }
            } else {
                int32_t s = newArray(et, es, std::max(rk, nd), dims.data(), nd);
                int32_t old = var(pg, i);
                var(pg, i) = s;
                decref(old);
            }
            break;
        }
        case OP_A_FREE: {
            int32_t i = pop(), pg = pop();
            Page *a = arrayAt(pg, i);
            int32_t s = newPage(PageKind::Array, -1, 0);
            Page &np = *heap[s].page;
            np.elemType = a->elemType;
            np.elemStruct = a->elemStruct;
            np.rank = a->rank;
            int32_t old = var(pg, i);
            var(pg, i) = s;
            decref(old);
            break;
        }
        case OP_A_NUMOF: {
            int32_t dim = pop(), i = pop(), pg = pop();
            Page *a = arrayAt(pg, i);
            for (int d = 1; d < dim && a; d++) a = a->v.empty() ? nullptr : page(a->v[0]);
            push(a ? int32_t(a->v.size()) : 0);
            break;
        }
        case OP_A_COPY: {
            int32_t n = pop(), srcStart = pop(), src = pop(), dstStart = pop(), i = pop(), pg = pop();
            Page *d = arrayAt(pg, i);
            Page *s = page(src);
            int32_t done = 0;
            if (d && s) {
                for (; done < n && dstStart + done < int32_t(d->v.size()) && srcStart + done < int32_t(s->v.size()); done++) {
                    int32_t v = s->v[srcStart + done];
                    int32_t &dv = d->v[dstStart + done];
                    if (d->elemType == T_STRING) str(dv) = str(v);
                    else if (d->elemType == T_STRUCT) structAssign(dv, v);
                    else dv = v;
                }
            }
            push(done);
            break;
        }
        case OP_A_FILL: {
            int32_t v = pop(), n = pop(), start = pop(), i = pop(), pg = pop();
            Page *a = arrayAt(pg, i);
            int32_t done = 0;
            for (; done < n && start + done < int32_t(a->v.size()); done++) {
                if (start + done < 0) continue;
                if (a->elemType == T_STRING) str(a->v[start + done]) = str(v);
                else a->v[start + done] = v;
            }
            if (a->elemType == T_STRING) decref(v);
            push(done);
            break;
        }
        case OP_A_REF: break;
        case OP_A_PUSHBACK: {
            int32_t v = pop(), i = pop(), pg = pop();
            arrayAt(pg, i)->v.push_back(v);
            break;
        }
        case OP_A_POPBACK: {
            int32_t i = pop(), pg = pop();
            Page *a = arrayAt(pg, i);
            if (!a->v.empty()) {
                int32_t v = a->v.back();
                a->v.pop_back();
                releaseElem(*a, v);
            }
            break;
        }
        case OP_A_EMPTY: { int32_t i = pop(), pg = pop(); push(arrayAt(pg, i)->v.empty()); break; }
        case OP_A_ERASE: {
            int32_t at = pop(), i = pop(), pg = pop();
            Page *a = arrayAt(pg, i);
            if (at >= 0 && size_t(at) < a->v.size()) {
                int32_t v = a->v[at];
                a->v.erase(a->v.begin() + at);
                releaseElem(*a, v);
                push(1);
            } else {
                push(0);
            }
            break;
        }
        case OP_A_INSERT: {
            int32_t v = pop(), at = pop(), i = pop(), pg = pop();
            Page *a = arrayAt(pg, i);
            at = std::clamp<int32_t>(at, 0, int32_t(a->v.size()));
            a->v.insert(a->v.begin() + at, v);
            break;
        }
        case OP_A_SORT: {
            int32_t fn = pop(), i = pop(), pg = pop();
            int32_t s = var(pg, i);
            Page *a = arrayAt(pg, i);
            std::vector<int32_t> vals = a->v;
            int32_t et = a->elemType;
            if (fn > 0) {
                std::stable_sort(vals.begin(), vals.end(), [&](int32_t x, int32_t y) { return call(fn, {x, y}) < 0; });
            } else if (et == T_STRING) {
                std::stable_sort(vals.begin(), vals.end(), [&](int32_t x, int32_t y) { return str(x) < str(y); });
            } else if (et == T_FLOAT) {
                std::stable_sort(vals.begin(), vals.end(), [](int32_t x, int32_t y) { return bitsToFloat(x) < bitsToFloat(y); });
            } else {
                std::stable_sort(vals.begin(), vals.end());
            }
            if (Page *p = page(s)) p->v = std::move(vals);
            break;
        }
        case OP_A_FIND: {
            int32_t fn = pop(), v = pop(), end = pop(), start = pop(), i = pop(), pg = pop();
            Page *a = arrayAt(pg, i);
            int32_t found = -1;
            end = std::min<int32_t>(end, int32_t(a->v.size()));
            for (int32_t k = std::max(0, start); k < end && found < 0; k++) {
                int32_t e = a->v[k];
                bool eq = fn > 0 ? call(fn, {e, v}) != 0 : a->elemType == T_STRING ? str(e) == str(v) : e == v;
                if (eq) found = k;
            }
            if (a->elemType == T_STRING) decref(v);
            push(found);
            break;
        }
        case OP_A_REVERSE: { int32_t i = pop(), pg = pop(); Page *a = arrayAt(pg, i); std::reverse(a->v.begin(), a->v.end()); break; }

        case OP_MSG: {
            ip = next;
            int32_t n = A(0);
            const std::string &m = n >= 0 && size_t(n) < ain.messages.size() ? ain.messages[n] : std::string();
            push(n);
            push(int32_t(ain.messages.size()));
            push(newString(m));
            enter(ain.messageFunction, -1, false);
            next = ip;
            break;
        }
        case OP_CALLHLL: callHll(A(0), A(1)); break;
        case OP_SH_GLOBALREF: push(globv(A(0))); break;
        case OP_SH_LOCALREF: push(localv(A(0))); break;
        case OP_SH_STRUCTREF: push(thisv(A(0))); break;
        case OP_SWITCH: case OP_STRSWITCH: {
            const Switch &sw = ain.switches.at(A(0));
            int32_t v = pop();
            int32_t target = sw.defaultAddress;
            for (auto &c : sw.cases) {
                bool hit = op == OP_SWITCH ? c.value == v : ain.strings.at(c.value) == str(v);
                if (hit) { target = c.address; break; }
            }
            if (op == OP_STRSWITCH) decref(v);
            if (target >= 0) jump(target);
            break;
        }
        case OP_FUNC: case OP_ENDFUNC: case OP_EOF: break;
        case OP_CALLSYS: ip = next; callSys(A(0)); break;
        case OP_CALLONJUMP: {
            int32_t s = pop();
            push(ain.findFunction(str(s)));
            decref(s);
            break;
        }
        case OP_SJUMP: {
            int32_t fn = pop();
            // A scenario jump abandons every caller, as `jump` does in the language.
            size_t keep = 0;
            while (frames.size() > keep && !frames.back().native) {
                Frame f = frames.back();
                frames.pop_back();
                stack.resize(std::min(stack.size(), f.stackBase));
                decref(f.localPage);
                if (f.structPage >= 0) decref(f.structPage);
            }
            ip = next;
            enter(fn, -1, false);
            next = ip;
            break;
        }
        case OP_FT_ASSIGNS: {
            pop();  // functype
            int32_t s = pop(), i = pop(), pg = pop();
            int f = ain.findFunction(str(s));
            var(pg, i) = f < 0 ? 0 : f;
            push(s);
            break;
        }
        case OP_ASSERT: {
            pop(); int32_t file = pop(), expr = pop(), v = pop();
            if (!v && log) log("assertion failed: " + sjis::toUtf8(str(expr)));
            decref(file);
            decref(expr);
            break;
        }
        case OP_SH_LOCALINC: localv(A(0))++; break;
        case OP_SH_LOCALDEC: localv(A(0))--; break;
        case OP_SH_LOCALASSIGN: localv(A(0)) = A(1); break;
        case OP_ITOB: push(pop() != 0); break;

        // Shortcut instructions of later compilers, each standing for a short sequence of the ones above.
        case OP_SH_MEM_ASSIGN_LOCAL: thisv(A(0)) = localv(A(1)); break;
        case OP_A_NUMOF_GLOB: { Page *a = arrayAt(globalPage, A(0)); push(int32_t(a->v.size())); break; }
        case OP_A_NUMOF_MEM: { Page *a = arrayAt(fr.structPage, A(0)); push(int32_t(a->v.size())); break; }
        case OP_SH_MEM_ASSIGN_IMM: thisv(A(0)) = A(1); break;
        case OP_SH_LOCALREFREF: push(localv(A(0))); push(localv(A(0) + 1)); break;
        case OP_SH_LOCAL_SUB_IMM: localv(A(0)) -= A(1); break;
        case OP_SH_IF_LOC_LT_IMM: if (localv(A(0)) < A(1)) jump(A(2)); break;
        case OP_SH_IF_LOC_GE_IMM: if (localv(A(0)) >= A(1)) jump(A(2)); break;
        case OP_SH_IF_LOC_GT_IMM: if (localv(A(0)) > A(1)) jump(A(2)); break;
        case OP_SH_IF_LOC_NE_IMM: if (localv(A(0)) != A(1)) jump(A(2)); break;
        case OP_SH_LOCREF_ASSIGN_MEM: var(localv(A(0)), localv(A(0) + 1)) = thisv(A(1)); break;
        case OP_PAGE_REF: { int32_t pg = pop(); push(var(pg, A(0))); break; }
        case OP_PAGE_REF2: { int32_t pg = pop(); push(var(var(pg, A(0)), A(1))); break; }
        case OP_SH_GLOBAL_ASSIGN_LOCAL: globv(A(0)) = localv(A(1)); break;
        case OP_SH_MEM_NE_IMM: push(thisv(A(0)) != A(1)); break;
        case OP_SH_LOCAL_ASSIGN_MEM: localv(A(0)) = thisv(A(1)); break;
        case OP_SH_IF_MEM_NE_LOC: if (thisv(A(0)) != localv(A(1))) jump(A(2)); break;
        case OP_SH_IF_MEM_NE_IMM: case OP_SH_IF_MEM_NE_IMM2: if (thisv(A(0)) != A(1)) jump(A(2)); break;
        case OP_SH_IF_MEM_EQ_IMM: if (thisv(A(0)) == A(1)) jump(A(2)); break;
        case OP_SH_IF_MEM_Z: if (!thisv(A(0))) jump(A(1)); break;
        case OP_SH_IF_MEM_A_NOT_EMPTY: { Page *a = page(thisv(A(0))); if (a && !a->v.empty()) jump(A(1)); break; }
        case OP_SH_MEM_REF2: push(var(thisv(A(0)), A(1))); break;
        case OP_SH_MEM_REF3: push(var(var(thisv(A(0)), A(1)), A(2))); break;
        case OP_SH_MEM_CALLMETHOD: callMethodOn(thisv(A(0)), A(1)); break;
        case OP_THIS_CALLMETHOD: callMethodOn(fr.structPage, A(0)); break;
        case OP_SH_MEM_MEM_CALLMETHOD: callMethodOn(var(thisv(A(0)), A(1)), A(2)); break;
        case OP_SH_GLOBAL_ASSIGN_IMM: globv(A(0)) = A(1); break;
        case OP_SH_LOCREF_CHECK: break;
        case OP_SH_MEM_A_PUSHBACK_LOCAL: arrayAt(fr.structPage, A(0))->v.push_back(copyPage(localv(A(1)))); break;
        case OP_SH_GLOBAL_A_PUSHBACK_LOCAL: arrayAt(globalPage, A(0))->v.push_back(copyPage(localv(A(1)))); break;
        case OP_SH_LOCAL_A_PUSHBACK_LOCAL: arrayAt(fr.localPage, A(0))->v.push_back(copyPage(localv(A(1)))); break;
        case OP_SH_LOCAL_A_PUSHBACK_LOCAL_S: arrayAt(fr.localPage, A(0))->v.push_back(newString(str(localv(A(1))))); break;
        case OP_SH_MEM_A_PUSHBACK_LOCAL_S: arrayAt(fr.structPage, A(0))->v.push_back(newString(str(localv(A(1))))); break;
        case OP_SH_IF_SREF_NE_STR: {
            int32_t i = pop(), pg = pop();
            if (str(var(pg, i)) != ain.strings.at(A(0))) jump(A(1));
            break;
        }
        case OP_SH_MEMS_EQ_LOCS: push(str(thisv(A(0))) == str(localv(A(1)))); break;
        case OP_SH_LOCS_EQ_STR: push(str(localv(A(0))) == ain.strings.at(A(1))); break;
        case OP_SH_MEMS_NE_STR: push(str(thisv(A(0))) != ain.strings.at(A(1))); break;
        case OP_SH_LOCS_NE_STR: push(str(localv(A(0))) != ain.strings.at(A(1))); break;
        case OP_SH_MEM_SR_REF: push(copyPage(thisv(A(0)))); break;
        case OP_SH_MEM_S_REF: push(newString(str(thisv(A(0))))); break;
        case OP_PAGE_S_REF: { int32_t pg = pop(); push(newString(str(var(pg, A(0))))); break; }
        case OP_SH_GLOBAL_S_REF: push(newString(str(globv(A(0))))); break;
        case OP_SH_LOCAL_S_REF: push(newString(str(localv(A(0))))); break;
        case OP_SH_LOCREF_S_ASSIGN_LOCAL: str(localv(A(0))) = str(localv(A(1))); break;
        case OP_SH_S_ASSIGN_STR: { int32_t d = pop(); str(d) = ain.strings.at(A(0)); break; }
        case OP_SH_S_ASSIGN_LOCAL: { int32_t d = pop(); str(d) = str(localv(A(0))); break; }
        case OP_SH_S_ASSIGN_MEM: { int32_t d = pop(); str(d) = str(thisv(A(0))); break; }
        case OP_SH_MEM_S_ASSIGN_LOCAL: str(thisv(A(0))) = str(localv(A(1))); break;
        case OP_SH_LOCAL_S_EMPTY: push(str(localv(A(0))).empty()); break;
        case OP_SH_MEM_S_EMPTY: push(str(thisv(A(0))).empty()); break;
        case OP_S_ASSIGN_REF: { int32_t i = pop(), pg = pop(), d = pop(); str(d) = str(var(pg, i)); break; }
        // Delegates. A delegate value is a slot holding (this, function) pairs.
        case OP_DG_NEW: push(newDelegate()); break;
        case OP_DG_COPY: { int32_t d = pop(); push(newDelegate(delegate(d) ? *delegate(d) : Delegate{})); break; }
        case OP_DG_ASSIGN: case OP_DG_PLUSA: case OP_DG_MINUSA: {
            int32_t src = pop(), dst = pop();
            Delegate *d = delegate(dst), *sd = delegate(src);
            if (d) {
                Delegate add = sd ? *sd : Delegate{};
                if (op == OP_DG_ASSIGN) {
                    Delegate old = std::move(*d);
                    *d = {};
                    for (auto &e : add) { incref(e.first); d->push_back(e); }
                    for (auto &e : old) decref(e.first);
                } else if (op == OP_DG_PLUSA) {
                    for (auto &e : add) { incref(e.first); d->push_back(e); }
                } else {
                    for (auto &e : add) {
                        auto it = std::find(d->begin(), d->end(), e);
                        if (it != d->end()) { int32_t obj = it->first; d->erase(it); decref(obj); }
                    }
                }
            }
            decref(src);
            push(dst);
            incref(dst);
            break;
        }
        case OP_DG_POP: decref(pop()); break;
        case OP_DG_CLEAR: {
            int32_t dsl = pop();
            if (Delegate *d = delegate(dsl)) {
                Delegate old = std::move(*d);
                *d = {};
                for (auto &e : old) decref(e.first);
            }
            break;
        }
        case OP_DG_NUMOF: { int32_t dsl = pop(); push(delegate(dsl) ? int32_t(delegate(dsl)->size()) : 0); break; }
        case OP_DG_EXIST: {
            int32_t fn = pop(), obj = pop(), dsl = pop();
            Delegate *d = delegate(dsl);
            push(d && std::find(d->begin(), d->end(), std::make_pair(obj, fn)) != d->end());
            break;
        }
        case OP_DG_ADD: {
            int32_t fn = pop(), obj = pop(), dsl = pop();
            if (Delegate *d = delegate(dsl)) { incref(obj); d->push_back({obj, fn}); }
            break;
        }
        case OP_DG_NEW_FROM_METHOD: { int32_t fn = pop(), obj = pop(); push(newDelegate(Delegate{{obj, fn}})); break; }
        case OP_DG_STR_TO_METHOD: {
            pop();  // the delegate type
            int32_t s = pop();
            int f = ain.findFunction(str(s));
            decref(s);
            push(f < 0 ? 0 : f);
            break;
        }
        // A delegate call: CALLBEGIN finds the delegate below the arguments and pushes an index; each DG_CALL calls
        // the next method with copies of the arguments, or when none are left drops them and jumps past the loop.
        case OP_DG_CALLBEGIN: push(0); break;
        case OP_DG_CALL: {
            const FuncType &ft = ain.delegates.at(A(0));
            int nargs = 0;
            for (int k = 0; k < ft.argc; k++) nargs++;
            size_t top = stack.size();
            if (top < size_t(nargs) + 2) fatal("delegate call without its delegate");
            int32_t idx = stack[top - 1];
            int32_t dsl = stack[top - 2 - nargs];
            Delegate *d = delegate(dsl);
            if (!d || idx >= int32_t(d->size())) {
                stack.resize(top - 2 - nargs);
                jump(A(1));
                break;
            }
            auto [obj, fn] = (*d)[idx];
            stack[top - 1] = idx + 1;
            for (int k = 0; k < nargs; k++) push(stack[top - 1 - nargs + k]);
            ip = next;
            enter(fn, obj >= 0 ? obj : -1, false);
            frames.back().discardReturn = true;
            frames.back().returnAddress = curInsn_;  // back to this instruction for the next method
            next = ip;
            break;
        }
        case OP_SH_LOC_OUT_OF_RANGE: { int32_t v = localv(A(0)); push(v < A(1) || v >= A(2)); break; }
        case OP_SREF_EMPTY: { int32_t i = pop(), pg = pop(); push(str(var(pg, i)).empty()); break; }
        case OP_S_ASSIGN_FUNCSTACKNAME: {
            // Seen only in the games' error reporting: f[i] = the name of the function i levels up.
            int32_t n = pop(), d = pop();
            str(d) = n >= 0 && size_t(n) < frames.size() ? ain.functions[frames[frames.size() - 1 - n].function].name : "";
            break;
        }
        default:
            fatal(std::string("unimplemented opcode ") + oi->name);
        }
        ip = next;
    }
}

}  // namespace s4

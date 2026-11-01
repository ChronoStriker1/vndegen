// system.ResumeSave and ResumeLoad: the VM's whole state in the PC engine's "RSM" layout, so a game saved on the
// PC loads here and the other way round. Worked out from saves the SDK runtime and Test Game 46's VM wrote for test
// programs; docs/system4-cleanroom.md has the details. The layout (version 8):
//
//   "RSM\0", u32 version, key
//   u32 0
//   resume point: u32 next address, function name, u32 offset of the call from the function's start, u32 CRC
//   u32 0
//   u32 n, n stack words
//   u32 n, the local page of each frame, outermost first; the first is a root with -1
//   u32 n, each frame's kind: 4 the root, 0 entered from native code, 1 a function call, 2 a method call
//   u32 n, the `this` page of each method frame
//   u32 n, each frame's return point: -1 for an entry frame, else an address record as above
//   u32 0, u32 0, u32 0
//   u32 n, then each heap slot: -1 when free, else a record by kind:
//     0 globals    u32 0, u32 refs, i32 -1, u32 n, n types, u32 bytes, values
//     1 locals     u32 1, u32 refs, function name, u32 n, n types, i32 `this` (version 8), u32 bytes, values
//     2 string     u32 2, u32 refs, u32 0, u32 n, n bytes: the text, a NUL, and on the PC sometimes stale bytes
//     3 array      u32 3, u32 refs, u32 0, u32 array type, struct name, u32 rank, u32 1, u32 bytes, values
//     4 struct     u32 4, u32 refs, constructor name, destructor name (each empty for none), u32 0, struct name,
//                  u32 n, n types, u32 bytes, values
//     5 delegate   (version 9) u32 5, u32 refs, u32 object number, u32 bytes, then 12 bytes per method: i32 the
//                  object's slot (-1 for none), u32 the function's index, u32 the object's number (0 for none)
//     6 delegate   VNDegen's own record for versions 7 and 8, whose PC encoding isn't known (Test Games 19 and 46
//                  hold no delegates when they save): u32 6, u32 refs, u32 n, then each method's object slot and
//                  function name.
//   u32 n, every function name, by index
//
// Version 9 (Test Game 56's VM, read from a save it wrote under Wine) puts u32 the next object number before the
// heap's slot count, and u32 each live slot's object number after its reference count. VNDegen numbers the slots
// by position: slot s is object s + 1. It also has no frame for the entry function: the root stands for it, so the
// lists of local pages and kinds have one entry fewer and the return points one per local page, the root's -1.
#include <cstring>

#include "opcodes.h"
#include "vm.h"

namespace s4 {
namespace {

constexpr int32_t kDelegateType = 63;  // a delegate variable's type in the code file


struct Out {
    std::vector<uint8_t> b;
    void u32(uint32_t v) { b.insert(b.end(), reinterpret_cast<uint8_t *>(&v), reinterpret_cast<uint8_t *>(&v) + 4); }
    void str(const std::string &s) { b.insert(b.end(), s.begin(), s.end()); b.push_back(0); }
};

struct In {
    const std::vector<uint8_t> &b;
    size_t i = 0;
    bool bad = false;
    uint32_t u32() {
        if (i + 4 > b.size()) { bad = true; i = b.size(); return 0; }
        uint32_t v;
        std::memcpy(&v, b.data() + i, 4);
        i += 4;
        return v;
    }
    int32_t i32() { return int32_t(u32()); }
    // A count of items at least `size` bytes each, bounded by the bytes left so damage can't ask for a huge
    // allocation.
    uint32_t count(size_t size) {
        uint32_t n = u32();
        if (n > (b.size() - i) / size) { bad = true; i = b.size(); return 0; }
        return n;
    }
    std::string str() {
        size_t j = i;
        while (j < b.size() && b[j]) j++;
        if (j >= b.size()) { bad = true; i = b.size(); return {}; }
        std::string s(reinterpret_cast<const char *>(b.data() + i), j - i);
        i = j + 1;
        return s;
    }
};

int32_t arrayTypeOf(int32_t elemType) {
    switch (elemType) {
    case T_INT: return T_ARRAY_INT;
    case T_FLOAT: return T_ARRAY_FLOAT;
    case T_STRING: return T_ARRAY_STRING;
    case T_STRUCT: return T_ARRAY_STRUCT;
    case T_BOOL: return T_ARRAY_BOOL;
    case T_FUNCTYPE: return T_ARRAY_FUNCTYPE;
    case 55: return 58;
    case 63: return 67;
    default: return T_ARRAY_INT;
    }
}
int32_t elemTypeOf(int32_t arrayType) {
    switch (arrayType) {
    case T_ARRAY_FLOAT: return T_FLOAT;
    case T_ARRAY_STRING: return T_STRING;
    case T_ARRAY_STRUCT: return T_STRUCT;
    case T_ARRAY_BOOL: return T_BOOL;
    case T_ARRAY_FUNCTYPE: return T_FUNCTYPE;
    case 58: return 55;
    case 67: return 63;
    default: return T_INT;
    }
}

// The length of the call instruction at an address, so a return point can be rebuilt from its call's offset.
uint32_t insnLength(const std::vector<uint8_t> &code, uint32_t at) {
    if (at >= code.size() || code.size() - at < 2) return 6;
    const OpInfo *oi = opInfo(code[at] | code[at + 1] << 8);
    return oi ? 2 + 4 * oi->args : 6;
}

bool isDelegateCall(const std::vector<uint8_t> &code, uint32_t at) {
    return at < code.size() && code.size() - at >= 2 && (code[at] | code[at + 1] << 8) == OP_DG_CALL;
}

// The PC stores a frame's call site as its return address less 6, the length of a call. A method a delegate called
// returns past its DG_CALL (10 bytes) to the jump back to it, so its call site is stored as the DG_CALL's address
// plus 4 (seen in Test Game 56's PC save).
constexpr uint32_t kDelegateCallSiteShift = 4;

}  // namespace

std::vector<uint8_t> VM::resumeSnapshot(const std::string &key, uint32_t resumeIp) {
    Out o;
    o.b.insert(o.b.end(), {'R', 'S', 'M', 0});
    o.u32(saveFormat.resumeVersion);
    o.str(key);
    o.u32(0);
    auto point = [&](uint32_t next, uint32_t callSite, int32_t fn) {
        const Function &f = ain.functions[fn];
        o.u32(next);
        o.str(f.name);
        o.u32(callSite - (f.address - 6));
        o.u32(f.crc);
    };
    point(resumeIp, curInsn_, frames.back().function);
    o.u32(0);
    o.u32(uint32_t(stack.size()));
    for (int32_t v : stack) o.u32(uint32_t(v));
    const bool v9 = saveFormat.resumeVersion >= 9;
    // Version 9 leaves the entry frame (frames[0], where the game's main runs) out: the root stands for it.
    const size_t first = v9 && !frames.empty() ? 1 : 0;
    o.u32(uint32_t(frames.size() - first + 1));
    o.u32(uint32_t(-1));
    for (size_t k = first; k < frames.size(); k++) o.u32(uint32_t(frames[k].localPage));
    o.u32(uint32_t(frames.size() - first + 1));
    o.u32(4);
    for (size_t k = first; k < frames.size(); k++) o.u32(frames[k].native ? 0 : frames[k].method ? 2 : 1);
    uint32_t methods = 0;
    for (size_t k = first; k < frames.size(); k++) methods += frames[k].method && !frames[k].native;
    o.u32(methods);
    for (size_t k = first; k < frames.size(); k++)
        if (frames[k].method && !frames[k].native) o.u32(uint32_t(frames[k].structPage));
    o.u32(uint32_t(frames.size()));
    for (size_t k = 0; k < frames.size(); k++) {
        if (frames[k].native || k == 0) { o.u32(uint32_t(-1)); continue; }
        uint32_t cs = frames[k].callSite;
        if (frames[k].discardReturn && isDelegateCall(ain.code, cs)) cs += kDelegateCallSiteShift;
        point(frames[k].returnAddress, cs, frames[k - 1].function);
    }
    o.u32(0);
    o.u32(0);
    o.u32(0);
    // References this VM counts that the PC's don't. Versions 7 and 8 leave out the one each method frame holds on
    // its `this`; version 9 keeps it, but its delegates hold none, while this VM's do (both seen by comparing
    // Test Game 56's PC save with VNDegen's of the same moment).
    std::vector<int32_t> uncounted(heap.size());
    if (!v9) {
        for (auto &f : frames)
            if (f.structPage >= 0 && size_t(f.structPage) < heap.size()) uncounted[size_t(f.structPage)]++;
    } else {
        for (const Slot &sl : heap)
            if (sl.inUse && sl.dg)
                for (auto &[obj, fn] : *sl.dg)
                    if (obj >= 0 && size_t(obj) < heap.size()) uncounted[size_t(obj)]++;
    }
    if (v9) o.u32(uint32_t(heap.size() + 1));
    o.u32(uint32_t(heap.size()));
    for (size_t s = 0; s < heap.size(); s++) {
        const Slot &sl = heap[s];
        if (!sl.inUse) { o.u32(uint32_t(-1)); continue; }
        int32_t refs = sl.refs - uncounted[s];
        if (sl.dg && v9) {
            o.u32(5);
            o.u32(uint32_t(refs));
            o.u32(uint32_t(s + 1));
            o.u32(uint32_t(sl.dg->size() * 12));
            for (auto &[obj, fn] : *sl.dg) {
                o.u32(uint32_t(obj));
                o.u32(uint32_t(fn));
                o.u32(obj >= 0 ? uint32_t(obj + 1) : 0);
            }
            continue;
        }
        if (sl.dg) {
            o.u32(6);
            o.u32(uint32_t(refs));
            o.u32(uint32_t(sl.dg->size()));
            for (auto &[obj, fn] : *sl.dg) {
                o.u32(uint32_t(obj));
                o.str(fn >= 0 && size_t(fn) < ain.functions.size() ? ain.functions[size_t(fn)].name : "");
            }
            continue;
        }
        if (sl.str) {
            o.u32(2);
            o.u32(uint32_t(refs));
            if (v9) o.u32(uint32_t(s + 1));
            o.u32(0);
            o.u32(uint32_t(sl.str->size() + 1));
            o.str(*sl.str);
            continue;
        }
        const Page &p = *sl.page;
        auto types = [&] {
            o.u32(uint32_t(p.v.size()));
            for (size_t i = 0; i < p.v.size(); i++) o.u32(uint32_t(varType(p, i).type));
        };
        auto values = [&] {
            o.u32(uint32_t(p.v.size() * 4));
            for (int32_t v : p.v) o.u32(uint32_t(v));
        };
        switch (p.kind) {
        case PageKind::Global:
            o.u32(0);
            o.u32(uint32_t(refs));
            if (v9) o.u32(uint32_t(s + 1));
            o.u32(uint32_t(-1));
            types();
            values();
            break;
        case PageKind::Local: {
            o.u32(1);
            o.u32(uint32_t(refs));
            if (v9) o.u32(uint32_t(s + 1));
            o.str(ain.functions[p.index].name);
            types();
            int32_t self = -1;
            for (auto &f : frames)
                if (f.localPage == int32_t(s)) self = f.structPage;
            if (saveFormat.resumeVersion >= 8) o.u32(uint32_t(self));
            values();
            break;
        }
        case PageKind::Array: {
            o.u32(3);
            o.u32(uint32_t(refs));
            if (v9) o.u32(uint32_t(s + 1));
            o.u32(0);
            o.u32(uint32_t(arrayTypeOf(p.elemType)));
            o.str(p.elemType == T_STRUCT && p.elemStruct >= 0 ? ain.structs[p.elemStruct].name : "");
            o.u32(uint32_t(std::max(1, p.rank)));
            o.u32(1);
            values();
            break;
        }
        case PageKind::Struct:
            o.u32(4);
            o.u32(uint32_t(refs));
            if (v9) o.u32(uint32_t(s + 1));
            const Struct &st = ain.structs[p.index];
            auto fnName = [&](int32_t fn) { return fn >= 0 && size_t(fn) < ain.functions.size() ? ain.functions[fn].name : ""; };
            o.str(fnName(st.constructor));
            o.str(fnName(st.destructor));
            o.u32(0);
            o.str(st.name);
            types();
            values();
            break;
        }
    }
    o.u32(uint32_t(ain.functions.size()));
    for (auto &f : ain.functions) o.str(f.name);
    return o.b;
}

bool VM::resumeRestore(const std::vector<uint8_t> &data, const std::string &key, std::string &error) {
    In in{data};
    if (data.size() < 8 || std::memcmp(data.data(), "RSM", 4) != 0) { error = "not a resume save"; return false; }
    in.i = 4;
    uint32_t version = in.u32();
    if (version < 7 || version > 9) { error = "resume save version " + std::to_string(version) + " is not supported"; return false; }
    if (in.str() != key) { error = "the save belongs to another game"; return false; }
    in.u32();
    // An address record: the function by name, then the call's offset in it.
    auto readPoint = [&](uint32_t &callSite, int32_t &fn) {
        in.u32();
        std::string name = in.str();
        uint32_t off = in.u32();
        in.u32();
        fn = ain.findFunction(name);
        if (fn < 0) { error = "the save names a function this game lacks"; return false; }
        callSite = ain.functions[fn].address - 6 + off;
        return true;
    };
    uint32_t curCall;
    int32_t curFn;
    if (!readPoint(curCall, curFn)) return false;
    in.u32();
    std::vector<int32_t> newStack(in.count(4));
    for (auto &v : newStack) v = in.i32();
    uint32_t nf = in.count(4);
    std::vector<int32_t> locals(nf);
    for (auto &v : locals) v = in.i32();
    std::vector<int32_t> kinds(in.count(4));
    for (auto &v : kinds) v = in.i32();
    std::vector<int32_t> selves(in.count(4));
    for (auto &v : selves) v = in.i32();
    uint32_t nret = in.count(4);
    std::vector<std::pair<uint32_t, int32_t>> rets;
    for (uint32_t k = 0; k < nret; k++) {
        size_t at = in.i;
        if (in.i32() == -1) { rets.push_back({0, -1}); continue; }
        in.i = at;
        uint32_t cs;
        int32_t fn;
        if (!readPoint(cs, fn)) return false;
        rets.push_back({cs, fn});
    }
    in.u32();
    in.u32();
    in.u32();
    // Return points: one per frame below the root, and in version 9 one for the root too (its -1).
    if (in.bad || nf < 1 || kinds.size() != nf || nret != (version >= 9 ? nf : nf - 1) || (version < 9 && nf < 2)) {
        error = "the resume save is damaged";
        return false;
    }
    // The return point of the frame at locals[k], k >= 1.
    auto retAt = [&](uint32_t k) { return rets[version >= 9 ? k : k - 1]; };

    // The new heap, slot for slot as saved.
    if (version >= 9) in.u32();  // the next object number
    std::vector<Slot> newHeap(in.count(4));
    for (size_t s = 0; s < newHeap.size() && !in.bad; s++) {
        int32_t kind = in.i32();
        if (kind == -1) continue;
        Slot &sl = newHeap[s];
        sl.inUse = true;
        sl.refs = in.i32();
        if (version >= 9) in.u32();  // the object number
        auto page = std::make_unique<Page>();
        auto readTypes = [&]() {
            uint32_t n = in.count(4);
            for (uint32_t i = 0; i < n; i++) in.u32();
            return n;
        };
        auto readValues = [&](Page &p) {
            uint32_t bytes = in.u32();
            if (bytes > data.size() - in.i) { in.bad = true; return; }
            p.v.resize(bytes / 4);
            for (auto &v : p.v) v = in.i32();
        };
        switch (kind) {
        case 0:
            in.u32();
            readTypes();
            page->kind = PageKind::Global;
            readValues(*page);
            break;
        case 1: {
            page->kind = PageKind::Local;
            page->index = ain.findFunction(in.str());
            readTypes();
            if (version >= 8) in.u32();
            readValues(*page);
            break;
        }
        case 2: {
            in.u32();
            // The length is the PC's buffer, which can be longer than the text: the text ends at its first NUL.
            uint32_t len = in.u32();
            if (len > data.size() - std::min(data.size(), in.i)) { in.bad = true; break; }
            std::string str(reinterpret_cast<const char *>(data.data() + in.i), len);
            in.i += len;
            str.resize(std::strlen(str.c_str()));
            sl.str = std::make_unique<std::string>(std::move(str));
            continue;
        }
        case 3: {
            page->kind = PageKind::Array;
            in.u32();
            page->elemType = elemTypeOf(in.i32());
            std::string sname = in.str();
            page->elemStruct = sname.empty() ? -1 : ain.findStruct(sname);
            page->rank = in.i32();
            in.u32();
            readValues(*page);
            break;
        }
        case 4: {
            page->kind = PageKind::Struct;
            in.str();  // constructor and destructor names: the struct's own, found by its name below
            in.str();
            in.u32();
            page->index = ain.findStruct(in.str());
            readTypes();
            readValues(*page);
            break;
        }
        case 5: {
            uint32_t bytes = in.u32();
            if (bytes % 12 || bytes > data.size() - std::min(data.size(), in.i)) { in.bad = true; break; }
            auto dg = std::make_unique<Delegate>();
            for (uint32_t k = 0; k < bytes / 12; k++) {
                int32_t obj = in.i32(), fn = in.i32();
                in.u32();  // the object's number
                if (fn < 0 || size_t(fn) >= ain.functions.size()) in.bad = true;
                dg->push_back({obj, fn});
            }
            sl.dg = std::move(dg);
            continue;
        }
        case 6: {
            auto dg = std::make_unique<Delegate>();
            uint32_t n = in.count(5);
            for (uint32_t k = 0; k < n && !in.bad; k++) {
                int32_t obj = in.i32();
                int32_t fn = ain.findFunction(in.str());
                if (fn < 0) in.bad = true;
                dg->push_back({obj, fn});
            }
            sl.dg = std::move(dg);
            continue;
        }
        default:
            error = "the resume save has an unknown kind of data";
            return false;
        }
        sl.page = std::move(page);
    }
    if (in.bad) { error = "the resume save is damaged"; return false; }

    // The state must hold together before it replaces the live one: the VM indexes these without checks.
    auto pageAt = [&](int32_t s, PageKind kind) -> Page * {
        if (s < 0 || size_t(s) >= newHeap.size() || !newHeap[size_t(s)].page) return nullptr;
        Page *p = newHeap[size_t(s)].page.get();
        return p->kind == kind ? p : nullptr;
    };
    auto consistent = [&] {
        bool global = false;
        for (const Slot &sl : newHeap) {
            if (sl.inUse && sl.refs < 0) return false;
            if (!sl.page) continue;
            const Page &p = *sl.page;
            if (p.kind == PageKind::Global) global = p.v.size() >= ain.globals.size();
            if (p.kind == PageKind::Struct &&
                (p.index < 0 || size_t(p.index) >= ain.structs.size() || p.v.size() < ain.structs[size_t(p.index)].members.size()))
                return false;
            if (p.kind == PageKind::Local &&
                (p.index <= 0 || size_t(p.index) >= ain.functions.size() || p.v.size() < ain.functions[size_t(p.index)].vars.size()))
                return false;
        }
        if (!global || curCall >= ain.code.size()) return false;
        // Every slot a page holds must be NULL (-1) or a live slot of the kind its type needs; the VM trusts this.
        enum Want { String, StructPage, ArrayPage, AnyPage, DelegateSlot };
        auto holds = [&](int32_t v, Want want) {
            if (v == -1) return true;
            if (v < 0 || size_t(v) >= newHeap.size() || !newHeap[size_t(v)].inUse) return false;
            const Slot &sl = newHeap[size_t(v)];
            switch (want) {
            case String: return bool(sl.str);
            case StructPage: return sl.page && sl.page->kind == PageKind::Struct;
            case ArrayPage: return sl.page && sl.page->kind == PageKind::Array;
            case AnyPage: return bool(sl.page);
            case DelegateSlot: return bool(sl.dg);
            }
            return false;
        };
        for (const Slot &sl : newHeap)
            if (sl.dg)
                for (auto &[obj, fn] : *sl.dg)
                    if (!holds(obj, StructPage)) return false;
        for (const Slot &sl : newHeap) {
            if (!sl.page) continue;
            const Page &p = *sl.page;
            for (size_t i = 0; i < p.v.size(); i++) {
                int32_t t = varType(p, i).type, v = p.v[i];
                bool ok = true;
                if (t == T_STRING || t == T_REF_STRING) ok = holds(v, String);
                else if (t == T_STRUCT || t == T_REF_STRUCT) ok = holds(v, StructPage);
                else if (t == kDelegateType) ok = holds(v, DelegateSlot);
                else if (isArray(t) || (t >= T_REF_ARRAY_INT && t <= T_REF_ARRAY_STRUCT) || t == T_REF_ARRAY_FUNCTYPE ||
                         t == T_REF_ARRAY_BOOL)
                    ok = holds(v, ArrayPage);
                else if (t == T_REF_INT || t == T_REF_FLOAT || t == T_REF_BOOL || t == 59) {
                    // A (page, index) pair: the index must lie in the page.
                    if (i + 1 >= p.v.size()) return false;
                    int32_t idx = p.v[++i];
                    ok = holds(v, AnyPage) &&
                         (v == -1 || (idx >= 0 && size_t(idx) < newHeap[size_t(v)].page->v.size()));
                }
                if (!ok) return false;
            }
        }
        size_t methods = 0;
        for (uint32_t k = 1; k < nf; k++) {
            if (!pageAt(locals[k], PageKind::Local)) return false;
            if (kinds[k] == 2 && (methods >= selves.size() || !pageAt(selves[methods++], PageKind::Struct))) return false;
            if (kinds[k] != 0 && retAt(k).first >= ain.code.size()) return false;
        }
        return true;
    };
    if (!consistent()) { error = "the resume save is damaged"; return false; }

    // Everything checked: replace the state.
    heap = std::move(newHeap);
    freeSlots.clear();
    for (size_t s = heap.size(); s-- > 0;)
        if (!heap[s].inUse) freeSlots.push_back(int32_t(s));
    globalPage = -1;
    for (size_t s = 0; s < heap.size(); s++)
        if (heap[s].page && heap[s].page->kind == PageKind::Global) { globalPage = int32_t(s); break; }
    stack = std::move(newStack);
    frames.clear();
    size_t self = 0;
    for (uint32_t k = 1; k < nf; k++) {
        Frame f;
        f.localPage = locals[k];
        Page *lp = page(f.localPage);
        f.function = lp ? lp->index : -1;
        f.native = kinds[k] == 0;
        f.method = kinds[k] == 2;
        f.structPage = f.method && self < selves.size() ? selves[self++] : -1;
        f.callSite = retAt(k).first;
        f.returnAddress = f.native ? 0 : f.callSite + insnLength(ain.code, f.callSite);
        // A method a delegate called returns to the DG_CALL itself, which calls the next one, and drops its value.
        if (!f.native && f.callSite >= kDelegateCallSiteShift && isDelegateCall(ain.code, f.callSite - kDelegateCallSiteShift)) {
            f.callSite -= kDelegateCallSiteShift;
            f.returnAddress = f.callSite;
            f.discardReturn = true;
        }
        f.stackBase = kUnknownBase;
        frames.push_back(f);
    }
    if (version >= 9) {
        // VNDegen runs the entry function as a frame of its own, which version 9 doesn't keep: the function the
        // first saved frame returns into (or the one the save was made in), with fresh locals.
        Frame e;
        e.function = nf > 1 ? retAt(1).second : curFn;
        const Function &fn = ain.functions[size_t(e.function)];
        e.localPage = newPage(PageKind::Local, e.function, fn.vars.size());
        for (size_t k = 0; k < fn.vars.size(); k++) {
            int32_t t = fn.vars[k].t.type;
            initVar(*heap[size_t(e.localPage)].page, k, fn.vars[k].t);
            if (t == T_REF_INT || t == T_REF_FLOAT || t == T_REF_BOOL || t == 59) k++;  // a (page, index) pair
        }
        e.native = true;
        e.stackBase = kUnknownBase;
        frames.insert(frames.begin(), e);
    }
    // The first frame is the entry; when it returns, the game ends.
    if (!frames.empty()) frames.front().native = true;
    // Put back the references the saved counts leave out: before version 9 each method frame's on its `this`, from
    // version 9 each delegate's on its methods' objects.
    if (version < 9) {
        for (auto &f : frames)
            if (f.structPage >= 0) incref(f.structPage);
    } else {
        for (Slot &sl : heap)
            if (sl.inUse && sl.dg)
                for (auto &[obj, fn] : *sl.dg)
                    if (obj >= 0) incref(obj);
    }
    ip = curCall + insnLength(ain.code, curCall);
    curInsn_ = curCall;
    push(0);  // ResumeSave returns 0 after a load
    return true;
}

}  // namespace s4

#include "serial.h"

#include <algorithm>
#include <cstring>

#include "vm.h"

namespace s4::serial {

void putString(std::vector<int32_t> &out, const std::string &s) {
    for (char ch : s) out.push_back(int32_t(static_cast<signed char>(ch)));
    out.push_back(0);
}

bool getString(const std::vector<int32_t> &in, size_t &i, std::string &s) {
    s.clear();
    for (; i < in.size(); i++) {
        if (in[i] == 0) { i++; return true; }
        s += char(in[i]);
    }
    return false;
}

void putTag(std::vector<int32_t> &out, const char *tag) { putString(out, tag); }

bool getTag(const std::vector<int32_t> &in, size_t &i, const char *tag) {
    std::string s;
    return getString(in, i, s) && s == tag;
}

namespace {

// A value's type: for an array, its elements' base type and the dimensions left.
struct Type {
    bool array = false;
    int32_t base = T_INT;
    int32_t structType = -1;
    int32_t rank = 0;
};

Type memberType(const VarType &t) {
    if (isArray(t.type)) return {true, arrayElem(t.type), t.structType, std::max(1, t.rank)};
    return {false, t.type, t.structType, 0};
}
// The type of an array's elements: sub-arrays while dimensions remain, then the base type.
Type elementOf(const Type &t) {
    if (t.rank > 1) return {true, t.base, t.structType, t.rank - 1};
    return {false, t.base, t.structType, 0};
}
bool holdsSlot(const Type &t) { return t.array || t.base == T_STRING || t.base == T_STRUCT; }

void putValue(VM &vm, std::vector<int32_t> &out, const Type &t, int32_t v) {
    if (t.array) {
        Page *a = vm.page(v);
        if (!a) { out.push_back(0); return; }
        out.push_back(int32_t(a->v.size()));
        Type et = elementOf(t);
        for (size_t k = 0; k < vm.page(v)->v.size(); k++) putValue(vm, out, et, vm.page(v)->v[k]);
    } else if (t.base == T_STRING) {
        putString(out, vm.valid(v) && vm.heap[size_t(v)].str ? *vm.heap[size_t(v)].str : std::string());
    } else if (t.base == T_STRUCT) {
        putStruct(vm, out, v);
    } else {
        out.push_back(v);  // int, float bits, bool
    }
}

bool skipValue(VM &vm, const std::vector<int32_t> &in, size_t &i, const Type &t) {
    if (t.array) {
        if (i >= in.size() || in[i] < 0) return false;
        int32_t n = in[i++];
        Type et = elementOf(t);
        for (int32_t k = 0; k < n; k++)
            if (!skipValue(vm, in, i, et)) return false;
        return true;
    }
    if (t.base == T_STRING) {
        std::string s;
        return getString(in, i, s);
    }
    if (t.base == T_STRUCT) return skipStruct(vm, in, i, t.structType);
    if (i >= in.size()) return false;
    i++;
    return true;
}

// Reads a value into target: a scalar is stored; a string, struct or array is filled in place, made first when the
// slot is empty.
bool getValue(VM &vm, const std::vector<int32_t> &in, size_t &i, const Type &t, int32_t &target) {
    if (t.array) {
        if (i >= in.size() || in[i] < 0) return false;
        int32_t n = in[i++];
        Type et = elementOf(t);
        int32_t dims[1] = {0};
        if (!vm.page(target)) target = vm.newArray(t.base, t.structType, t.rank, dims, 1);
        int32_t arr = target;
        for (int32_t old : vm.page(arr)->v)
            if (holdsSlot(et)) vm.decref(old);
        vm.page(arr)->v.assign(size_t(n), holdsSlot(et) ? -1 : 0);
        for (int32_t k = 0; k < n; k++) {
            int32_t v = holdsSlot(et) ? -1 : 0;
            if (!getValue(vm, in, i, et, v)) return false;
            vm.page(arr)->v[size_t(k)] = v;  // re-read: reading may have allocated
        }
        return true;
    }
    if (t.base == T_STRING) {
        std::string s;
        if (!getString(in, i, s)) return false;
        if (vm.valid(target) && vm.heap[size_t(target)].str) *vm.heap[size_t(target)].str = s;
        else target = vm.newString(s);
        return true;
    }
    if (t.base == T_STRUCT) {
        if (!vm.page(target)) target = vm.newStruct(t.structType);
        return getStruct(vm, in, i, target);
    }
    if (i >= in.size()) return false;
    target = in[i++];
    return true;
}

}  // namespace

void putStruct(VM &vm, std::vector<int32_t> &out, int32_t structSlot) {
    Page *p = vm.page(structSlot);
    if (!p || p->index < 0 || size_t(p->index) >= vm.ain.structs.size()) return;
    const auto &members = vm.ain.structs[size_t(p->index)].members;
    for (size_t m = 0; m < members.size() && m < p->v.size(); m++)
        putValue(vm, out, memberType(members[m].t), vm.page(structSlot)->v[m]);
}

bool getStruct(VM &vm, const std::vector<int32_t> &in, size_t &i, int32_t structSlot) {
    Page *p = vm.page(structSlot);
    if (!p || p->index < 0 || size_t(p->index) >= vm.ain.structs.size()) return false;
    const auto members = vm.ain.structs[size_t(p->index)].members;
    for (size_t m = 0; m < members.size(); m++) {
        int32_t v = vm.page(structSlot)->v[m];
        if (!getValue(vm, in, i, memberType(members[m].t), v)) return false;
        vm.page(structSlot)->v[m] = v;  // re-read: reading may have allocated
    }
    return true;
}

bool skipStruct(VM &vm, const std::vector<int32_t> &in, size_t &i, int32_t structType) {
    if (structType < 0 || size_t(structType) >= vm.ain.structs.size()) return false;
    for (auto &m : vm.ain.structs[size_t(structType)].members)
        if (!skipValue(vm, in, i, memberType(m.t))) return false;
    return true;
}

}  // namespace s4::serial

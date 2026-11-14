#include <cstdio>
#include <string>
#include <vector>

#include "opcodes.h"
#include "vm.h"

using namespace s4;

namespace {

int g_checks = 0, g_failed = 0;

void check(bool ok, const std::string &what) {
    g_checks++;
    if (ok) return;
    g_failed++;
    std::printf("FAIL %s\n", what.c_str());
}

struct Code {
    std::vector<uint8_t> b;
    void op(uint16_t o, std::vector<int32_t> args = {}) {
        b.push_back(uint8_t(o));
        b.push_back(uint8_t(o >> 8));
        for (int32_t a : args)
            for (int k = 0; k < 4; k++) b.push_back(uint8_t(uint32_t(a) >> (8 * k)));
    }
    uint32_t func(int32_t n) {
        op(OP_FUNC, {n});
        return uint32_t(b.size());
    }
};

void arrayByValue() {
    const VarType intArray{T_ARRAY_INT, -1, 1};
    Ain ain;
    ain.globals.push_back({"list", intArray});
    ain.functions.resize(3);
    Code c;
    ain.functions[1].name = "main";
    ain.functions[1].address = c.func(1);
    c.op(OP_PUSHGLOBALPAGE);
    c.op(OP_PUSH, {0});
    c.op(OP_PUSH, {3});
    c.op(OP_PUSH, {1});
    c.op(OP_A_ALLOC);
    c.op(OP_SH_GLOBALREF, {0});
    c.op(OP_A_REF);
    c.op(OP_CALLFUNC, {2});
    c.op(OP_RETURN);
    c.op(OP_ENDFUNC, {1});
    ain.functions[2].name = "callee";
    ain.functions[2].argc = 1;
    ain.functions[2].vars.push_back({"list", intArray});
    ain.functions[2].address = c.func(2);
    c.op(OP_PUSHLOCALPAGE);
    c.op(OP_PUSH, {0});
    c.op(OP_PUSH, {1});
    c.op(OP_PUSH, {1});
    c.op(OP_A_ALLOC);
    c.op(OP_RETURN);
    c.op(OP_ENDFUNC, {2});
    ain.code = c.b;
    ain.mainFunction = 1;

    VM vm(std::move(ain));
    vm.start();
    vm.call(1, {});
    int32_t list = vm.page(vm.globalPage)->v[0];
    check(vm.valid(list) && vm.page(list), "an array passed by value is still the caller's after the call returns");
    if (!vm.valid(list) || !vm.page(list)) return;
    check(vm.heap[size_t(list)].refs == 1, "the caller's array keeps one reference, got " + std::to_string(vm.heap[size_t(list)].refs));
    check(vm.page(list)->v.size() == 3, "the callee's changes stay in its copy: the caller's array has " +
                                            std::to_string(vm.page(list)->v.size()) + " elements, want 3");
    size_t live = 0;
    for (const Slot &s : vm.heap) live += s.inUse;
    check(live == 2, "the callee's copy is freed: " + std::to_string(live) + " live slots, want 2");
}

}  // namespace

int main() {
    arrayByValue();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}

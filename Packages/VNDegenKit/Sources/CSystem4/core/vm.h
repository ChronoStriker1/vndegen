// The System 4 virtual machine: a stack machine over 32-bit values with a heap of reference-counted slots.
// A slot holds either a page (an array of values: the globals, a function's locals, a struct, or an array) or
// a string. The bytecode sees slot numbers directly, for example as the value of a string or struct variable,
// so the heap model follows what the code expects. docs/system4-cleanroom.md says where each rule came from.
#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ain.h"

namespace s4 {

enum class PageKind : uint8_t { Global, Local, Struct, Array };

struct Page {
    PageKind kind;
    int32_t index = -1;      // function for Local, struct type for Struct
    // Arrays: the element's base type (T_INT, T_STRING, T_STRUCT, ...), its struct type, and the dimensions
    // left including this one. An array of rank > 1 holds pages of rank - 1.
    int32_t elemType = 0;
    int32_t elemStruct = -1;
    int32_t rank = 0;
    std::vector<int32_t> v;
};

// A delegate: methods to call in order, each a `this` page (or -1) and a function.
using Delegate = std::vector<std::pair<int32_t, int32_t>>;

struct Slot {
    int32_t refs = 0;
    bool inUse = false;
    std::unique_ptr<Page> page;
    std::unique_ptr<std::string> str;
    std::unique_ptr<Delegate> dg;
};

class VM;

// Arguments of an HLL call, as the bytecode pushed them, and its result.
struct HllCall {
    VM &vm;
    const HllFunction &decl;
    std::vector<int32_t> words;   // raw stack words
    std::vector<int> argWord;     // index of each declared argument's first word
    int32_t ret = 0;              // int, bool or float bits; for strings and structs a slot the call made
    bool retSet = false;

    int32_t i(int k) const { return words[argWord[k]]; }
    float f(int k) const;
    bool b(int k) const { return i(k) != 0; }
    const std::string &s(int k) const;
    // ref int, ref float, ref bool: the variable the argument refers to.
    int32_t &ref(int k) const;
    float &refF(int k) const;
    std::string &refS(int k) const;   // ref string
    Page *page(int k) const;          // struct, ref struct, arrays and ref arrays: the page, or nullptr
    int32_t slot(int k) const { return i(k); }

    void retInt(int32_t v) { ret = v; retSet = true; }
    void retBool(bool v) { retInt(v ? 1 : 0); }
    void retFloat(float v);
    void retString(const std::string &s);
};

using HllImpl = void (*)(HllCall &);

// Every HLL function VNDegen implements registers here by "Library.Function".
struct HllRegistry {
    static std::unordered_map<std::string, HllImpl> &table();
    struct Add { Add(const char *name, HllImpl fn) { table()[name] = fn; } };
};
#define S4_HLL(lib, fn) \
    static void hll_##lib##_##fn(s4::HllCall &c); \
    static s4::HllRegistry::Add reg_##lib##_##fn(#lib "." #fn, hll_##lib##_##fn); \
    static void hll_##lib##_##fn([[maybe_unused]] s4::HllCall &c)

constexpr size_t kUnknownBase = SIZE_MAX;

struct Frame {
    int32_t function;
    int32_t localPage;
    int32_t structPage;   // `this`, or -1
    uint32_t returnAddress;
    size_t stackBase;     // stack size when the call started, minus its arguments
    bool native = false;  // a call from native code: returning ends VM::call
    uint32_t callSite = 0;  // address of the call instruction in the caller
    bool method = false;
    bool discardReturn = false;  // a delegate call: its return value is dropped
};

// Thrown when system.ResumeLoad has replaced the VM's state: the runner continues with VM::run(0).
struct ResumeJump {};

// Thrown to stop the VM: system.Exit, a fatal error, or the host stopping the game.
struct VMExit { int code; std::string reason; };

class VM {
public:
    explicit VM(Ain ain);
    Ain ain;

    // Heap.
    std::vector<Slot> heap;
    std::vector<int32_t> freeSlots;
    int32_t alloc();
    int32_t newPage(PageKind kind, int32_t index, size_t size);
    int32_t newString(std::string s = {});
    int32_t newDelegate(Delegate d = {});
    Delegate *delegate(int32_t slot);
    Page *page(int32_t slot);
    std::string &str(int32_t slot);
    void incref(int32_t slot);
    void decref(int32_t slot);
    bool valid(int32_t slot) const { return slot >= 0 && size_t(slot) < heap.size() && heap[slot].inUse; }

    // Variable types, for initializing and freeing pages.
    VarType varType(const Page &p, size_t i) const;
    void initVar(Page &p, size_t i, const VarType &t);
    int32_t newStruct(int32_t type);  // allocates members and runs the constructor
    int32_t newArray(int32_t elemType, int32_t elemStruct, int32_t rank, const int32_t *dims, int ndims);
    int32_t copyPage(int32_t slot);    // deep copy of a struct or array page
    void releaseValue(const VarType &t, int32_t v);

    // Execution.
    std::vector<int32_t> stack;
    std::vector<Frame> frames;
    uint32_t ip = 0;
    int32_t globalPage = -1;

    void start();                       // initializes globals; main runs on the first runUntilYield
    // Calls a function from native code and runs it to completion. Arguments are raw words.
    int32_t call(int32_t function, const std::vector<int32_t> &args, int32_t thisPage = -1);
    void run(size_t untilFrames);       // executes until the frame count drops to untilFrames

    void push(int32_t v) { stack.push_back(v); }
    int32_t pop();
    float popF();
    void pushF(float f);

    // Hooks the HLL modules and the host set.
    std::function<void(const std::string &)> log;
    std::function<void(const std::string &)> trace;  // every call, for debugging
    std::function<void()> peek;             // system.Peek and other points where the host may run a frame
    std::function<uint32_t()> timeMs;       // the game clock
    std::string saveDir;                    // the game's SaveData folder on this machine
    std::string saveFolderName = "SaveData";  // its name as the game sees it (the ini's SaveFolder)
    std::string rootDir;                    // the game's folder
    std::string gameName;

    std::string functionName(int32_t f) const;

    // system.ResumeSave and ResumeLoad: the whole VM state in the PC engine's "RSM" layout (resume.cpp).
    std::vector<uint8_t> resumeSnapshot(const std::string &key, uint32_t resumeIp);
    bool resumeRestore(const std::vector<uint8_t> &data, const std::string &key, std::string &error);
    [[noreturn]] void fatal(const std::string &msg);

private:
    void enter(int32_t function, int32_t thisPage, bool native);
    uint32_t curInsn_ = 0;
    void leave();
    void callHll(int32_t lib, int32_t fn);
    void callSys(int32_t n);
    std::vector<std::vector<HllImpl>> hll_;
    std::vector<bool> hllWarned_;
    int32_t arg(int k) const;
    void structAssign(int32_t dst, int32_t src);
    void freePage(Page &p);
};

}  // namespace s4

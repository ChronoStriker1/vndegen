// The .ain code file: bytecode plus the tables the VM needs (functions, globals, structs, HLL
// declarations, switch tables, strings and messages). Layouts are documented in
// docs/system4-cleanroom.md with how each was worked out.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace s4 {

// Data types, numbered as in the SDK's EObjType42.h, plus the later numbers the v5 and v6 games use.
enum Type : int32_t {
    T_VOID = 0,
    T_INT = 10, T_FLOAT = 11, T_STRING = 12, T_STRUCT = 13,
    T_ARRAY_INT = 14, T_ARRAY_FLOAT = 15, T_ARRAY_STRING = 16, T_ARRAY_STRUCT = 17,
    T_REF_INT = 18, T_REF_FLOAT = 19, T_REF_STRING = 20, T_REF_STRUCT = 21,
    T_REF_ARRAY_INT = 22, T_REF_ARRAY_FLOAT = 23, T_REF_ARRAY_STRING = 24, T_REF_ARRAY_STRUCT = 25,
    T_IMAINSYSTEM = 26,
    T_FUNCTYPE = 27, T_ARRAY_FUNCTYPE = 30, T_REF_FUNCTYPE = 31, T_REF_ARRAY_FUNCTYPE = 32,
    T_BOOL = 47, T_ARRAY_BOOL = 50, T_REF_BOOL = 51, T_REF_ARRAY_BOOL = 52,
};

struct VarType {
    int32_t type = 0;
    int32_t structType = -1;  // struct index for struct types, functype index for functypes
    int32_t rank = 0;         // array dimensions
};

struct Variable {
    std::string name;  // Shift-JIS, as stored
    VarType t;
};

struct Function {
    uint32_t address = 0;  // first instruction after the FUNC marker
    std::string name;
    int32_t isLabel = 0;
    VarType ret;
    int32_t argc = 0;      // argument slots; a ref int argument takes two
    std::vector<Variable> vars;  // arguments first, then locals
    uint32_t crc = 0;
};

struct Global {
    std::string name;
    VarType t;
    int32_t group = -1;  // index into objGroups for grouped save data
};

struct GlobalInit {
    int32_t global = 0;
    int32_t type = 0;
    int32_t intValue = 0;   // int, bool, and float bits
    std::string strValue;   // strings
};

struct Struct {
    std::string name;
    int32_t constructor = -1;
    int32_t destructor = -1;
    std::vector<Variable> members;
};

struct HllFunction {
    std::string name;
    int32_t retType = 0;
    std::vector<Variable> args;  // VarType.structType and rank unused
};

struct HllLibrary {
    std::string name;
    std::vector<HllFunction> functions;
};

struct SwitchCase { int32_t value; int32_t address; };
struct Switch {
    int32_t caseType = 0;  // 2 int, 4 string
    int32_t defaultAddress = -1;
    std::vector<SwitchCase> cases;
};

struct FuncType {  // functype and delegate declarations
    std::string name;
    VarType ret;
    int32_t argc = 0;
    std::vector<Variable> vars;
};

struct Ain {
    int32_t version = 0;
    int32_t keycode = 0;
    std::vector<uint8_t> code;
    std::vector<Function> functions;
    std::vector<Global> globals;
    std::vector<GlobalInit> globalInits;
    std::vector<Struct> structs;
    std::vector<std::string> messages;
    int32_t mainFunction = -1;
    int32_t messageFunction = -1;
    std::vector<HllLibrary> libraries;
    std::vector<Switch> switches;
    int32_t gameVersion = 0;
    std::vector<std::string> strings;
    std::vector<std::string> fileNames;
    int32_t onJump = -1;
    std::vector<FuncType> functypes;
    std::vector<FuncType> delegates;
    std::vector<std::string> objGroups;

    // Loads an .ain file. Returns an empty string on success, or what went wrong.
    std::string load(const std::string &path);
    std::string parse(const std::vector<uint8_t> &plain);

    int findFunction(const std::string &name) const;
    int findStruct(const std::string &name) const;
    int findLibrary(const std::string &name) const;
};

}  // namespace s4

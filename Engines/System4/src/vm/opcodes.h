// Opcode numbers and argument counts. Numbers 0 to 155 and their names come from the System 4.0
// SDK (ECmdList42.h and the IDE's mnemonic table); later ones were worked out from game bytecode.
#pragma once
#include <cstdint>

namespace s4 {

struct OpInfo {
    const char *name;
    int8_t args;  // -1 marks numbers no test game uses
};

constexpr int kOpCount = 258;
extern const OpInfo kOps[kOpCount];

inline const OpInfo *opInfo(uint16_t op) { return op < kOpCount && kOps[op].args >= 0 ? &kOps[op] : nullptr; }

}  // namespace s4

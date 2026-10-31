// The PC sprite engine's encoding of game values into an int array, which games keep in their saves (the backlog,
// back scenes, character sprites). Read from a save Test Game 19 wrote on the PC: an int or bool is its value, a
// string its bytes one per element (as signed chars) and a NUL, a struct its members in order, and an array its
// element count and then its elements.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace s4 {

class VM;

namespace serial {

void putString(std::vector<int32_t> &out, const std::string &s);
// Reads a NUL-terminated string at i; false when the array ends first.
bool getString(const std::vector<int32_t> &in, size_t &i, std::string &s);
// A four-character tag such as "ADL": its characters and a NUL.
void putTag(std::vector<int32_t> &out, const char *tag);
bool getTag(const std::vector<int32_t> &in, size_t &i, const char *tag);

// A struct page's members.
void putStruct(VM &vm, std::vector<int32_t> &out, int32_t structSlot);
// Fills an existing struct page from the array; false when the data runs out or doesn't fit.
bool getStruct(VM &vm, const std::vector<int32_t> &in, size_t &i, int32_t structSlot);
// Steps over one encoded struct of a type, without a page to fill.
bool skipStruct(VM &vm, const std::vector<int32_t> &in, size_t &i, int32_t structType);

}  // namespace serial
}  // namespace s4

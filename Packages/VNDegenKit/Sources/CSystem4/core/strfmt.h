// String helpers for the VM. Game strings are CP932, and the language counts characters, so a two-byte
// character is one character for indexes and lengths. A character value is its byte, or lead | trail << 8.
#pragma once
#include <cstdint>
#include <string>

namespace s4::strfmt {

int32_t charCount(const std::string &s);
size_t byteOffset(const std::string &s, int32_t chars);  // clamped to s.size()
int32_t toInt(const std::string &s);                     // also reads full-width digits; 0 when it can't
std::string floatToString(float f, int32_t precision);   // precision < 0: six decimals
int32_t find(const std::string &s, const std::string &needle);  // character index, -1 when absent
std::string substr(const std::string &s, int32_t start, int32_t len);
void pushChar(std::string &s, int32_t ch);
void popChar(std::string &s);
void erase(std::string &s, int32_t start, int32_t n);
int32_t charAt(const std::string &s, int32_t i);
void setChar(std::string &s, int32_t i, int32_t ch);
// Replaces the first conversion in fmt (%d, %5d, %02d, %x, %f, %.2f, %s, %c, %%) with one value. type is the
// SDK's value type number: 2 int, 3 float, 4 string, 48 bool.
std::string formatOne(const std::string &fmt, int32_t type, int32_t value, const std::string &strValue);

}  // namespace s4::strfmt

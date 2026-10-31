// CP932 (Windows Shift-JIS), the text encoding of System 4 games, to and from UTF-8.
#pragma once
#include <string>

namespace s4::sjis {

std::string toUtf8(const std::string &sjis);
// Characters CP932 cannot hold become '?'.
std::string fromUtf8(const std::string &utf8);
// Byte length of the character starting at s[i]: 2 for a lead byte followed by a trail byte, else 1.
int charLength(const std::string &s, size_t i);
bool isLeadByte(unsigned char c);
// Unicode code point of the character at s[i], with its length in *len.
char32_t decodeAt(const std::string &s, size_t i, int *len);

}  // namespace s4::sjis

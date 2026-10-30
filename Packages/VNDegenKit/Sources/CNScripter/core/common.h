// Shared helpers for VNDegen's NScripter runtime: byte buffers, logging, and Shift-JIS text.
// Script text, file names and string variables stay Shift-JIS (CP932) bytes throughout the runtime, as in NScripter,
// so string lengths, substrings and save files match the PC engine. Text becomes Unicode only when it is drawn.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace vnns {

using Bytes = std::vector<uint8_t>;

// Appends a line to the runtime's log (stderr until vnns_start names a file).
void logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void setLogFile(const char *path);

inline bool sjisLead(uint8_t c) { return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC); }

// Bytes in the character at s[i]: 2 for a lead byte followed by another byte, else 1.
inline size_t sjisCharLen(std::string_view s, size_t i) {
    return (sjisLead(uint8_t(s[i])) && i + 1 < s.size()) ? 2 : 1;
}

// The Unicode code point of the CP932 character at s[i], and its length in bytes. Unmapped characters give U+FFFD.
uint32_t sjisDecode(std::string_view s, size_t i, size_t *len);
std::string sjisToUtf8(std::string_view s);
// Characters CP932 can't hold become '?'.
std::string utf8ToSjis(std::string_view s);
// True when s is valid UTF-8 that isn't plain ASCII, the test for an on-disk name that needs converting.
bool looksUtf8(std::string_view s);

// ASCII letters lowered, leaving the second byte of two-byte characters alone.
std::string sjisLower(std::string_view s);
// A file name as the runtime compares them: lowercase, '\' separators, no leading "./" or separator.
std::string normalizePath(std::string_view s);

bool readFile(const std::string &path, Bytes &out);
bool writeFile(const std::string &path, const void *data, size_t size);

inline uint32_t be32(const uint8_t *p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
inline uint16_t be16(const uint8_t *p) { return uint16_t(p[0] << 8 | p[1]); }
inline uint32_t le32(const uint8_t *p) { return uint32_t(p[3]) << 24 | uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0]; }
inline uint16_t le16(const uint8_t *p) { return uint16_t(p[1] << 8 | p[0]); }

}  // namespace vnns

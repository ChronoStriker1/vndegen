// Shared helpers for VNDegen's NScripter runtime: byte buffers, logging, and the script's text encoding.
// Script text, file names and string variables stay in the script's encoding throughout the runtime, as in the PC
// engine, so string lengths, substrings and save files match it. That is Shift-JIS (CP932) for NScripter and UTF-8 for
// Ponscripter. Text becomes Unicode only when it is drawn.
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

enum class TextEncoding { ShiftJis, Utf8 };
// The encoding every helper below reads, on the calling thread: Shift-JIS unless the engine the thread is running is
// a Ponscripter one. Engine::start sets it, and each call into an engine through the C interface sets it for the call
// (EncodingScope), so engines of both kinds can run at once. The runtime's own threads only mix and decode sound.
TextEncoding textEncoding();
void setTextEncoding(TextEncoding encoding);
class EncodingScope {
public:
    explicit EncodingScope(TextEncoding encoding) : saved_(textEncoding()) { setTextEncoding(encoding); }
    ~EncodingScope() { setTextEncoding(saved_); }
    EncodingScope(const EncodingScope &) = delete;
    EncodingScope &operator=(const EncodingScope &) = delete;

private:
    TextEncoding saved_;
};

inline bool sjisLead(uint8_t c) { return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC); }

// Bytes in the character at s[i]. Shift-JIS: 2 for a lead byte followed by another byte, else 1. UTF-8: the length the
// first byte gives, or 1 for a malformed or cut-off sequence.
size_t charLen(std::string_view s, size_t i);

// The Unicode code point of the character at s[i], and its length in bytes. Unmapped characters give U+FFFD.
uint32_t decodeChar(std::string_view s, size_t i, size_t *len);
// Whether the character at s[i] takes half a cell in NScripter's fixed-pitch text: a one-byte character in Shift-JIS,
// and in UTF-8 the characters Shift-JIS holds in one byte (ASCII and half-width katakana) and other Latin letters.
bool isHalfWidth(std::string_view s, size_t i);
// s's length without a character cut off at its end, as a byte-counted mid can leave.
size_t completeLength(std::string_view s);
// Whether s[i] starts a full-width space (U+3000), which separates arguments in some scripts.
bool isWideSpace(std::string_view s, size_t i);
// Script text to and from UTF-8. In Shift-JIS, characters CP932 can't hold become '?'.
std::string toUtf8(std::string_view s);
std::string fromUtf8(std::string_view s);
// True when s is valid UTF-8 that isn't plain ASCII, the test for an on-disk name that needs converting.
bool looksUtf8(std::string_view s);

// ASCII letters lowered, leaving the bytes of multi-byte characters alone.
std::string lowerAscii(std::string_view s);
// A file name as the runtime compares them: lowercase, '\' separators, no leading "./" or separator.
std::string normalizePath(std::string_view s);

bool readFile(const std::string &path, Bytes &out);
bool writeFile(const std::string &path, const void *data, size_t size);

inline uint32_t be32(const uint8_t *p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
inline uint16_t be16(const uint8_t *p) { return uint16_t(p[0] << 8 | p[1]); }
inline uint32_t le32(const uint8_t *p) { return uint32_t(p[3]) << 24 | uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0]; }
inline uint16_t le16(const uint8_t *p) { return uint16_t(p[1] << 8 | p[0]); }

}  // namespace vnns

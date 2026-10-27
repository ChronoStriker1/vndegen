#include "sjis.h"

#include <mutex>
#include <unordered_map>

namespace s4::sjis {
namespace {

const char16_t kTable[60][192] = {
#include "sjis_table.inc"
};

int leadRow(unsigned char c) {
    if (c >= 0x81 && c <= 0x9F) return c - 0x81;
    if (c >= 0xE0 && c <= 0xFC) return c - 0xE0 + 31;
    return -1;
}

void putUtf8(std::string &out, char32_t cp) {
    if (cp < 0x80) out += char(cp);
    else if (cp < 0x800) { out += char(0xC0 | cp >> 6); out += char(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { out += char(0xE0 | cp >> 12); out += char(0x80 | (cp >> 6 & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
    else { out += char(0xF0 | cp >> 18); out += char(0x80 | (cp >> 12 & 0x3F)); out += char(0x80 | (cp >> 6 & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
}

const std::unordered_map<char32_t, uint16_t> &reverse() {
    static std::unordered_map<char32_t, uint16_t> map;
    static std::once_flag once;
    std::call_once(once, [] {
        for (int r = 0; r < 60; r++) {
            int lead = r < 31 ? 0x81 + r : 0xE0 + r - 31;
            for (int t = 0; t < 192; t++) {
                char32_t cp = kTable[r][t];
                // The first pair listed for a code point wins, as in Windows' best-fit-free mapping.
                if (cp && !map.count(cp)) map[cp] = uint16_t(lead << 8 | (0x40 + t));
            }
        }
    });
    return map;
}

}  // namespace

bool isLeadByte(unsigned char c) { return leadRow(c) >= 0; }

int charLength(const std::string &s, size_t i) {
    return i + 1 < s.size() && isLeadByte(s[i]) ? 2 : 1;
}

char32_t decodeAt(const std::string &s, size_t i, int *len) {
    unsigned char c = s[i];
    if (c < 0x80) { *len = 1; return c; }
    if (c >= 0xA1 && c <= 0xDF) { *len = 1; return 0xFF61 + (c - 0xA1); }
    int r = leadRow(c);
    if (r >= 0 && i + 1 < s.size()) {
        unsigned char t = s[i + 1];
        *len = 2;
        if (t >= 0x40) {
            char32_t cp = kTable[r][t - 0x40];
            if (cp) return cp;
        }
        return 0x30FB;  // unmapped pair: KATAKANA MIDDLE DOT stands in
    }
    *len = 1;
    return c == 0x80 ? 0x80 : 0xFFFD;
}

std::string toUtf8(const std::string &s) {
    std::string out;
    out.reserve(s.size() * 3 / 2);
    for (size_t i = 0; i < s.size();) {
        int len;
        putUtf8(out, decodeAt(s, i, &len));
        i += len;
    }
    return out;
}

std::string fromUtf8(const std::string &u) {
    std::string out;
    const auto &rev = reverse();
    for (size_t i = 0; i < u.size();) {
        unsigned char c = u[i];
        char32_t cp;
        int n;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 4; }
        else { out += '?'; i++; continue; }
        if (i + n > u.size()) { out += '?'; break; }
        for (int k = 1; k < n; k++) cp = cp << 6 | (u[i + k] & 0x3F);
        i += n;
        if (cp < 0x80) out += char(cp);
        else if (cp >= 0xFF61 && cp <= 0xFF9F) out += char(0xA1 + (cp - 0xFF61));
        else if (auto it = rev.find(cp); it != rev.end()) { out += char(it->second >> 8); out += char(it->second & 0xFF); }
        else out += '?';
    }
    return out;
}

}  // namespace s4::sjis

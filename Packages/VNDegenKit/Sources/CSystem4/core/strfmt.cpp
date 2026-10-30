#include "strfmt.h"

#include <cstdio>
#include <cstring>

#include "sjis.h"

namespace s4::strfmt {

int32_t charCount(const std::string &s) {
    int32_t n = 0;
    for (size_t i = 0; i < s.size(); i += sjis::charLength(s, i)) n++;
    return n;
}

size_t byteOffset(const std::string &s, int32_t chars) {
    size_t i = 0;
    while (chars > 0 && i < s.size()) {
        i += sjis::charLength(s, i);
        chars--;
    }
    return std::min(i, s.size());
}

int32_t toInt(const std::string &s) {
    // Full-width digits and minus are CP932 0x824F-0x8258 and 0x817C.
    std::string ascii;
    for (size_t i = 0; i < s.size();) {
        int len = sjis::charLength(s, i);
        unsigned char a = s[i], b = len == 2 ? s[i + 1] : 0;
        if (len == 2 && a == 0x82 && b >= 0x4F && b <= 0x58) ascii += char('0' + b - 0x4F);
        else if (len == 2 && a == 0x81 && b == 0x7C) ascii += '-';
        else if (len == 1) ascii += char(a);
        else ascii += ' ';
        i += len;
    }
    return int32_t(std::strtol(ascii.c_str(), nullptr, 10));
}

std::string floatToString(float f, int32_t precision) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", precision < 0 ? 6 : precision, double(f));
    return buf;
}

int32_t find(const std::string &s, const std::string &needle) {
    size_t pos = s.find(needle);
    // Only a match on a character boundary counts.
    while (pos != std::string::npos) {
        size_t i = 0;
        int32_t n = 0;
        while (i < pos) { i += sjis::charLength(s, i); n++; }
        if (i == pos) return n;
        pos = s.find(needle, pos + 1);
    }
    return -1;
}

std::string substr(const std::string &s, int32_t start, int32_t len) {
    if (start < 0) start = 0;
    size_t a = byteOffset(s, start);
    size_t b = len < 0 ? s.size() : byteOffset(s, start + len);
    return b > a ? s.substr(a, b - a) : std::string();
}

// A two-byte character's code holds the lead byte low and the trail byte high, as the SDK compiler encodes 'あ'
// (CP932 0x82 0xA0) as 0xA082.
void pushChar(std::string &s, int32_t ch) {
    s += char(ch & 0xFF);
    if (ch > 0xFF) s += char((ch >> 8) & 0xFF);
}

void popChar(std::string &s) {
    int32_t n = charCount(s);
    if (n > 0) s.resize(byteOffset(s, n - 1));
}

void erase(std::string &s, int32_t start, int32_t n) {
    if (start < 0 || n <= 0) return;
    size_t a = byteOffset(s, start), b = byteOffset(s, start + n);
    s.erase(a, b - a);
}

int32_t charAt(const std::string &s, int32_t i) {
    if (i < 0) return 0;
    size_t a = byteOffset(s, i);
    if (a >= s.size()) return 0;
    if (sjis::charLength(s, a) == 2) return uint8_t(s[a]) | (uint8_t(s[a + 1]) << 8);
    return uint8_t(s[a]);
}

void setChar(std::string &s, int32_t i, int32_t ch) {
    if (i < 0) return;
    size_t a = byteOffset(s, i);
    if (a >= s.size()) return;
    std::string rep;
    pushChar(rep, ch);
    s.replace(a, sjis::charLength(s, a), rep);
}

std::string formatOne(const std::string &fmt, int32_t type, int32_t value, const std::string &strValue) {
    for (size_t i = 0; i < fmt.size(); i += sjis::charLength(fmt, i)) {
        if (fmt[i] != '%') continue;
        if (i + 1 < fmt.size() && fmt[i + 1] == '%') { i++; continue; }
        size_t j = i + 1;
        while (j < fmt.size() && std::strchr("-+ #0123456789.", fmt[j])) j++;
        if (j >= fmt.size()) break;
        std::string spec = fmt.substr(i, j - i);
        char conv = fmt[j];
        char buf[512];
        std::string out;
        float f;
        std::memcpy(&f, &value, 4);
        switch (conv) {
        case 'd': case 'i': case 'x': case 'X': case 'u': case 'o': {
            long long v = type == 3 ? (long long)f : value;
            std::snprintf(buf, sizeof buf, (spec + "ll" + conv).c_str(), v);
            out = buf;
            break;
        }
        case 'f': case 'g': case 'e':
            std::snprintf(buf, sizeof buf, (spec + conv).c_str(), double(type == 3 ? f : float(value)));
            out = buf;
            break;
        case 'c': {
            std::string c;
            pushChar(c, value);
            out = c;
            break;
        }
        case 's':
            out = type == 4 ? strValue : std::to_string(value);
            break;
        default:
            continue;
        }
        return fmt.substr(0, i) + out + fmt.substr(j + 1);
    }
    return fmt;
}

}  // namespace s4::strfmt

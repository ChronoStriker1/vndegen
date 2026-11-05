#include "common.h"

#include "casepath.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdarg>
#include <mutex>
#include <unordered_map>

namespace vnns {

#include "sjis_table.inc"

namespace {
std::mutex gLogMutex;
FILE *gLog = nullptr;
thread_local TextEncoding tEncoding = TextEncoding::ShiftJis;
}  // namespace

TextEncoding textEncoding() { return tEncoding; }
void setTextEncoding(TextEncoding encoding) { tEncoding = encoding; }

void setLogFile(const char *path) {
    std::lock_guard<std::mutex> lock(gLogMutex);
    if (gLog && gLog != stderr) fclose(gLog);
    gLog = path ? fopen(path, "a") : nullptr;
}

void logf(const char *fmt, ...) {
    std::lock_guard<std::mutex> lock(gLogMutex);
    FILE *f = gLog ? gLog : stderr;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fflush(f);
}

static uint32_t sjisDecode(std::string_view s, size_t i, size_t *len) {
    uint8_t c = uint8_t(s[i]);
    if (!sjisLead(c) || i + 1 >= s.size()) {
        *len = 1;
        if (c < 0x80) return c;
        if (c >= 0xA1 && c <= 0xDF) return 0xFF61 + (c - 0xA1);  // half-width katakana
        return 0xFFFD;
    }
    *len = 2;
    uint8_t t = uint8_t(s[i + 1]);
    if (t < 0x40) return 0xFFFD;
    int row = c <= 0x9F ? c - 0x81 : c - 0xE0 + 31;
    uint16_t u = kSjisTable[row * 192 + (t - 0x40)];
    return u ? u : 0xFFFD;
}

static void appendUtf8(std::string &out, uint32_t cp) {
    if (cp < 0x80) {
        out += char(cp);
    } else if (cp < 0x800) {
        out += char(0xC0 | cp >> 6);
        out += char(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += char(0xE0 | cp >> 12);
        out += char(0x80 | (cp >> 6 & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    } else {
        out += char(0xF0 | cp >> 18);
        out += char(0x80 | (cp >> 12 & 0x3F));
        out += char(0x80 | (cp >> 6 & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    }
}

static std::string sjisToUtf8(std::string_view s) {
    std::string out;
    out.reserve(s.size() * 3 / 2);
    for (size_t i = 0; i < s.size();) {
        size_t n;
        appendUtf8(out, sjisDecode(s, i, &n));
        i += n;
    }
    return out;
}

// Decodes one UTF-8 sequence; returns 0 and advances one byte on malformed input.
static uint32_t utf8Next(std::string_view s, size_t &i, bool *ok) {
    uint8_t c = uint8_t(s[i]);
    int n = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
    if (n < 0) {
        *ok = false;
        i++;
        return 0;
    }
    uint32_t cp = n == 0 ? c : c & (0x3F >> n);
    for (int k = 1; k <= n; k++) {
        if (i + k >= s.size() || (uint8_t(s[i + k]) & 0xC0) != 0x80) {
            *ok = false;
            i++;
            return 0;
        }
        cp = cp << 6 | (uint8_t(s[i + k]) & 0x3F);
    }
    // Overlong forms, surrogates and values past U+10FFFF aren't characters.
    static const uint32_t least[4] = {0, 0x80, 0x800, 0x10000};
    if (cp < least[n] || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
        *ok = false;
        i++;
        return 0;
    }
    i += n + 1;
    return cp;
}

bool looksUtf8(std::string_view s) {
    bool ok = true, high = false;
    for (size_t i = 0; i < s.size() && ok;) {
        if (uint8_t(s[i]) >= 0x80) high = true;
        utf8Next(s, i, &ok);
    }
    return ok && high;
}

static std::string utf8ToSjis(std::string_view s) {
    static std::unordered_map<uint32_t, uint16_t> reverse;
    static std::once_flag once;
    std::call_once(once, [] {
        for (int row = 0; row < int(sizeof(kSjisTable) / sizeof(kSjisTable[0]) / 192); row++) {
            int lead = row < 31 ? 0x81 + row : 0xE0 + row - 31;
            for (int t = 0; t < 192; t++) {
                uint16_t u = kSjisTable[row * 192 + t];
                if (u) reverse.emplace(u, uint16_t(lead << 8 | (t + 0x40)));
            }
        }
    });
    std::string out;
    for (size_t i = 0; i < s.size();) {
        bool ok = true;
        uint32_t cp = utf8Next(s, i, &ok);
        if (!ok) {
            out += '?';
            continue;
        }
        if (cp < 0x80) {
            out += char(cp);
        } else if (cp >= 0xFF61 && cp <= 0xFF9F) {
            out += char(0xA1 + (cp - 0xFF61));
        } else if (auto it = reverse.find(cp); it != reverse.end()) {
            out += char(it->second >> 8);
            out += char(it->second & 0xFF);
        } else {
            out += '?';
        }
    }
    return out;
}

size_t charLen(std::string_view s, size_t i) {
    uint8_t c = uint8_t(s[i]);
    if (textEncoding() == TextEncoding::ShiftJis) return (sjisLead(c) && i + 1 < s.size()) ? 2 : 1;
    // A malformed sequence counts one byte at a time, as decodeChar reads it.
    size_t at = i;
    bool ok = true;
    utf8Next(s, at, &ok);
    return ok ? at - i : 1;
}

uint32_t decodeChar(std::string_view s, size_t i, size_t *len) {
    if (textEncoding() == TextEncoding::ShiftJis) return sjisDecode(s, i, len);
    size_t at = i;
    bool ok = true;
    uint32_t cp = utf8Next(s, at, &ok);
    *len = at - i;
    return ok ? cp : 0xFFFD;
}

bool isHalfWidth(std::string_view s, size_t i) {
    if (textEncoding() == TextEncoding::ShiftJis) return charLen(s, i) == 1;
    size_t n;
    uint32_t cp = decodeChar(s, i, &n);
    return cp < 0x2000 || (cp >= 0xFF61 && cp <= 0xFF9F);
}

size_t completeLength(std::string_view s) {
    if (textEncoding() == TextEncoding::ShiftJis) {
        // A lead byte alone at the end.
        size_t last = 0;
        for (size_t i = 0; i < s.size(); i += charLen(s, i)) last = i;
        return !s.empty() && last + 1 == s.size() && sjisLead(uint8_t(s[last])) ? last : s.size();
    }
    // Within the last three bytes, a lead byte whose sequence runs past the end.
    for (size_t back = 1; back <= 3 && back <= s.size(); back++) {
        uint8_t b = uint8_t(s[s.size() - back]);
        if ((b & 0xC0) == 0x80) continue;
        size_t want = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : b >= 0xC0 ? 2 : 1;
        return want > back ? s.size() - back : s.size();
    }
    return s.size();
}

bool isWideSpace(std::string_view s, size_t i) {
    if (textEncoding() == TextEncoding::ShiftJis)
        return i + 1 < s.size() && uint8_t(s[i]) == 0x81 && uint8_t(s[i + 1]) == 0x40;
    return i + 2 < s.size() && uint8_t(s[i]) == 0xE3 && uint8_t(s[i + 1]) == 0x80 && uint8_t(s[i + 2]) == 0x80;
}

std::string toUtf8(std::string_view s) {
    return textEncoding() == TextEncoding::ShiftJis ? sjisToUtf8(s) : std::string(s);
}

std::string fromUtf8(std::string_view s) {
    return textEncoding() == TextEncoding::ShiftJis ? utf8ToSjis(s) : std::string(s);
}

std::string lowerAscii(std::string_view s) {
    std::string out(s);
    for (size_t i = 0; i < out.size();) {
        size_t n = charLen(out, i);
        if (n == 1 && out[i] >= 'A' && out[i] <= 'Z') out[i] = char(out[i] - 'A' + 'a');
        i += n;
    }
    return out;
}

std::string normalizePath(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        size_t n = charLen(s, i);
        if (n > 1) {
            out.append(s.substr(i, n));
        } else {
            char c = s[i];
            if (c == '/') c = '\\';
            if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
            out += c;
        }
        i += n;
    }
    while (out.size() >= 2 && out[0] == '.' && out[1] == '\\') out.erase(0, 2);
    while (!out.empty() && out[0] == '\\') out.erase(0, 1);
    return out;
}

bool readFile(const std::string &path, Bytes &out) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize(size > 0 ? size_t(size) : 0);
    bool ok = out.empty() || fread(out.data(), 1, out.size(), f) == out.size();
    fclose(f);
    return ok;
}

// Creates the folders above path, as the PC engine expects the game to have done.
static void makeParents(const std::string &path) {
    for (size_t slash = path.find('/', 1); slash != std::string::npos; slash = path.find('/', slash + 1))
        if (mkdir(path.substr(0, slash).c_str(), 0755) == 0) casepath_changed(path.substr(0, slash).c_str());
}

bool writeFile(const std::string &path, const void *data, size_t size) {
    makeParents(path);
    // A new file of a unique name, which mkstemp creates without following a link already there.
    std::string tmp = path + ".XXXXXX";
    int fd = mkstemp(tmp.data());
    if (fd < 0) return false;
    fchmod(fd, 0644);  // mkstemp makes it private; saves are ordinary files
    FILE *f = fdopen(fd, "wb");
    if (!f) {
        close(fd);
        remove(tmp.c_str());
        return false;
    }
    bool ok = fwrite(data, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    if (ok) ok = rename(tmp.c_str(), path.c_str()) == 0;
    if (!ok) remove(tmp.c_str());
    if (ok) casepath_changed(path.c_str());
    return ok;
}

}  // namespace vnns

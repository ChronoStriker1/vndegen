#include "common.h"

#include <sys/stat.h>

#include <cstdarg>
#include <mutex>
#include <unordered_map>

namespace vnns {

#include "sjis_table.inc"

namespace {
std::mutex gLogMutex;
FILE *gLog = nullptr;
}  // namespace

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

uint32_t sjisDecode(std::string_view s, size_t i, size_t *len) {
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

std::string sjisToUtf8(std::string_view s) {
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

std::string utf8ToSjis(std::string_view s) {
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

std::string sjisLower(std::string_view s) {
    std::string out(s);
    for (size_t i = 0; i < out.size();) {
        size_t n = sjisCharLen(out, i);
        if (n == 1 && out[i] >= 'A' && out[i] <= 'Z') out[i] = char(out[i] - 'A' + 'a');
        i += n;
    }
    return out;
}

std::string normalizePath(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        size_t n = sjisCharLen(s, i);
        if (n == 2) {
            out.append(s.substr(i, 2));
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
        mkdir(path.substr(0, slash).c_str(), 0755);
}

bool writeFile(const std::string &path, const void *data, size_t size) {
    makeParents(path);
    std::string tmp = path + ".tmp";
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    if (ok) ok = rename(tmp.c_str(), path.c_str()) == 0;
    if (!ok) remove(tmp.c_str());
    return ok;
}

}  // namespace vnns

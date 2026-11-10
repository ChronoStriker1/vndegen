#include "script.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <dirent.h>
#include <strings.h>

#include "fs.h"

namespace vnns {

bool Script::load(FileSystem &fs, std::string *error) {
    Bytes raw;
    std::string joined;
    // 0.txt takes priority over nscript.dat. Numbered files join in numeric order, 0.txt style or 00.txt style.
    auto unreadable = [&](const std::string &name) {
        if (error) *error = "The game's script file " + name + " can't be read.";
        return false;
    };
    auto joinNumbered = [&](bool twoDigit, const char *ext) {
        for (int i = 0; i < 100; i++) {
            char name[16];
            snprintf(name, sizeof name, twoDigit ? "%02d.%s" : "%d.%s", i, ext);
            if (!fs.read(name, raw)) {
                if (fs.exists(name)) return unreadable(name);
                continue;
            }
            joined.append(reinterpret_cast<const char *>(raw.data()), raw.size());
            if (!joined.empty() && joined.back() != '\n') joined += '\n';
        }
        return true;
    };
    // Ponscripter's 0.utf comes first: such a game may also carry a 0.txt for other engines.
    if (fs.exists("0.utf")) {
        if (!joinNumbered(false, "utf")) return false;
    } else if (fs.exists("00.utf")) {
        if (!joinNumbered(true, "utf")) return false;
    } else if (fs.exists("0.txt")) {
        if (!joinNumbered(false, "txt")) return false;
    } else if (fs.exists("00.txt")) {
        if (!joinNumbered(true, "txt")) return false;
    } else if (fs.exists("nscript.dat")) {
        if (!fs.read("nscript.dat", raw)) return unreadable("nscript.dat");
        fromDat_ = true;
        joined.resize(raw.size());
        for (size_t i = 0; i < raw.size(); i++) joined[i] = char(raw[i] ^ 0x84);
    } else {
        if (error) *error = "The game has no 0.txt, 00.txt, nscript.dat or 0.utf script.";
        return false;
    }
    text_.reserve(joined.size());
    for (char c : joined)
        if (c != '\r') text_ += c;
    loadedLength_ = text_.size();
    if (text_.empty() || text_.back() != '\n') text_ += '\n';

    lineStarts_.clear();
    lineStarts_.push_back(0);
    for (size_t i = 0; i + 1 < text_.size(); i++)
        if (text_[i] == '\n') lineStarts_.push_back(i + 1);

    for (int line = 0; line < lineCount(); line++) {
        size_t p = lineStarts_[size_t(line)];
        while (text_[p] == ' ' || text_[p] == '\t') p++;
        if (text_[p] != '*') continue;
        size_t start = ++p;
        while (p < text_.size() && text_[p] != '\n' && text_[p] != ' ' && text_[p] != '\t' && text_[p] != ';' &&
               text_[p] != ':')
            p += charLen(text_, p);
        std::string name = lowerAscii(std::string_view(text_).substr(start, p - start));
        if (labels_.count(name)) continue;
        labels_.emplace(name, int(labelNames_.size()));
        labelNames_.push_back(name);
        labelLines_.push_back(line);
    }
    parseSettings();
    return true;
}

// The first line may set the screen and variable counts: ";mode800", ";value500", ";mode800,value600", or the newer
// ";$V2000G1000S800,600L10000" (letters in any case).
void Script::parseSettings() {
    std::string first = lowerAscii(std::string_view(text_).substr(0, text_.find('\n')));
    if (first.empty() || first[0] != ';') return;
    if (first.size() > 1 && first[1] == '$') {
        size_t p = 2;
        auto number = [&]() {
            int v = 0;
            while (p < first.size() && isdigit(uint8_t(first[p]))) v = v * 10 + (first[p++] - '0');
            return v;
        };
        while (p < first.size()) {
            char c = first[p++];
            if (c == 'v') {
                settings_.variables = number();
            } else if (c == 'g') {
                settings_.globalBorder = number();
            } else if (c == 's') {
                settings_.width = number();
                if (p < first.size() && first[p] == ',') {
                    p++;
                    settings_.height = number();
                }
            } else if (c == 'l') {
                settings_.labelLimit = number();
            } else if (c == ' ' || c == '\t' || c == ';') {
                break;
            }
        }
        return;
    }
    auto at = [&](const char *key) -> int {
        size_t k = first.find(key);
        return k == std::string::npos ? -1 : atoi(first.c_str() + k + strlen(key));
    };
    switch (at("mode")) {
    case 800: settings_.width = 800; settings_.height = 600; break;
    case 400: settings_.width = 400; settings_.height = 300; break;
    case 320: settings_.width = 320; settings_.height = 240; break;
    default: break;
    }
    if (int v = at("value"); v >= 0) settings_.globalBorder = v;
}

bool Script::isPonscripter(const std::string &root) {
    DIR *d = opendir(root.c_str());
    if (!d) return false;
    bool found = false;
    while (dirent *e = readdir(d))
        if (!strcasecmp(e->d_name, "0.utf") || !strcasecmp(e->d_name, "00.utf")) found = true;
    closedir(d);
    return found;
}

int Script::lineAt(size_t offset) const {
    auto it = std::upper_bound(lineStarts_.begin(), lineStarts_.end(), offset);
    return int(it - lineStarts_.begin()) - 1;
}

int Script::labelIndex(std::string_view name) const {
    if (!name.empty() && name[0] == '*') name.remove_prefix(1);
    auto it = labels_.find(lowerAscii(name));
    return it == labels_.end() ? -1 : it->second;
}

std::string_view Script::labelAt(int line) const {
    auto it = std::upper_bound(labelLines_.begin(), labelLines_.end(), line);
    if (it == labelLines_.begin()) return {};
    return labelNames_[size_t(it - labelLines_.begin() - 1)];
}

int Script::labelLine(std::string_view name) const {
    int i = labelIndex(name);
    return i < 0 ? -1 : labelLines_[size_t(i)];
}

}  // namespace vnns

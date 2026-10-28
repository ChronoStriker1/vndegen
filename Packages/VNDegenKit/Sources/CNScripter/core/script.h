// The game's script: 0.txt .. 99.txt joined in order, or 00.txt .. 99.txt, else nscript.dat (each byte XOR 0x84).
// Kept as Shift-JIS bytes with carriage returns removed, with an index of line starts and labels. Saves record script
// positions as line numbers, so lines count exactly as in the PC engine.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "common.h"

namespace vnns {

class FileSystem;

struct ScriptSettings {
    int width = 640, height = 480;
    int variables = 4096;     // V: numeric and string variables %0.. and $0..
    int globalBorder = 200;   // G: variables from here on are global
    int labelLimit = 10000;   // L
};

class Script {
public:
    bool load(FileSystem &fs, std::string *error);

    const std::string &text() const { return exec_.empty() ? text_ : exec_.back(); }
    // NSExec runs one statement given as text: until the matching endExec, the parser reads that text in place of
    // the script. Calls nest (a statement run this way may run another), so each endExec restores the one below.
    void beginExec(const std::string &statement) { exec_.push_back(statement + "\n"); }
    void endExec() {
        if (!exec_.empty()) exec_.pop_back();
    }
    int execDepth() const { return int(exec_.size()); }
    Script() = default;
    Script(const Script &) = delete;
    Script &operator=(const Script &) = delete;
    const ScriptSettings &settings() const { return settings_; }
    // kidoku.dat's size for a new file, as the PC engine sizes it: (length + 106) / 8 bytes for text scripts and
    // length / 8 for nscript.dat, the length counting the script without carriage returns (checked on four games).
    size_t kidokuSize() const { return fromDat_ ? loadedLength_ / 8 : (loadedLength_ + 106) / 8; }
    int lineCount() const { return int(lineStarts_.size()); }
    size_t lineStart(int line) const { return lineStarts_[size_t(line)]; }
    int lineAt(size_t offset) const;

    // Finds a label by name, without its '*', ignoring case. Returns its line, or -1.
    int labelLine(std::string_view name) const;
    // Labels in script order, for the label log.
    const std::vector<std::string> &labels() const { return labelNames_; }
    int labelIndex(std::string_view name) const;

private:
    void parseSettings();
    std::string text_;
    std::vector<std::string> exec_;  // statements NSExec is running, innermost last
    bool fromDat_ = false;
    size_t loadedLength_ = 0;  // the script's length before a closing newline was added
    ScriptSettings settings_;
    std::vector<size_t> lineStarts_;
    std::unordered_map<std::string, int> labels_;  // lowercase name -> label index
    std::vector<std::string> labelNames_;
    std::vector<int> labelLines_;
};

}  // namespace vnns

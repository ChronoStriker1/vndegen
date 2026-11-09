// The `yabaiplay` game part: Test Game 96's story pieces that its PC keeps in the exe (set 2).
//   chapter N   the scene scripts cfg\chapter_file.ini lists under `#chapter N`, in order
// Its chapters number the interrogation as 1000 * heroine (1 Ayano, 2 Misuzu, 3 Rina) + 100 * (truth - 1)
// + 10 * (stage - 1) + question, with the prologue, the truths' openings, the votes and endings below 1000.
#include <map>
#include <sstream>

#include "parts.h"

namespace neko {
namespace {

// `#chapter N`, then one `scr\\FILE.txt` a line (the file's own doubled backslash), then `##`. `//` starts a comment.
std::map<int, std::vector<std::string>> parseChapters(const std::string &text) {
    std::map<int, std::vector<std::string>> chapters;
    std::istringstream in(text);
    std::vector<std::string> *open = nullptr;
    for (std::string line; std::getline(in, line);) {
        if (size_t cut = line.find("//"); cut != std::string::npos) line.erase(cut);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line.erase(0, start);
        if (line.rfind("#chapter", 0) == 0) {
            open = &chapters[std::atoi(line.c_str() + 8)];
        } else if (line == "##") {
            open = nullptr;
        } else if (open) {
            // A file may be followed by `,0` or `,1` (whether the swimsuit choice changes it); 96 writes none.
            line = line.substr(0, line.find(','));
            std::string path;
            for (size_t i = 0; i < line.size(); i++)
                if (!(line[i] == '\\' && i + 1 < line.size() && line[i + 1] == '\\')) path += line[i];
            open->push_back(path);
        }
    }
    return chapters;
}

class YabaiPlayPart : public GamePart {
public:
    std::vector<std::string> chapter(PartHost &host, int number) override {
        if (!loaded_) {
            loaded_ = true;
            if (auto bytes = host.read("cfg\\chapter_file.ini")) chapters_ = parseChapters(std::string(bytes->begin(), bytes->end()));
            else host.log("chapter list missing: cfg\\chapter_file.ini");
        }
        auto it = chapters_.find(number);
        return it == chapters_.end() ? std::vector<std::string>{} : it->second;
    }

private:
    bool loaded_ = false;
    std::map<int, std::vector<std::string>> chapters_;
};

}  // namespace

std::unique_ptr<GamePart> makeYabaiPlayPart() { return std::make_unique<YabaiPlayPart>(); }

}  // namespace neko

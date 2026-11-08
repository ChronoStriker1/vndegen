// Checks the script reader against a game's own bytes: every script must parse to its last byte, every `next` must name
// a node, no two nodes in a script may share a number, and the numbers a node shows in its readable first string must equal
// its parameters. Every message must also fit on the screen as the runtime sets it, in its stand-in fonts.
// usage: nekocheck <game folder>...
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

#include "archive.h"
#include "engine.h"
#include "script.h"
#include "sjis.h"

using namespace neko;

namespace {

// The number after `key` in the node's readable string, or -1 when it isn't there.
bool field(const std::string &s, const char *key, long &out) {
    auto at = s.find(key);
    if (at == std::string::npos) return false;
    out = std::strtol(s.c_str() + at + std::strlen(key), nullptr, 10);
    return true;
}

struct Totals {
    long scripts = 0, nodes = 0, badParse = 0, trailing = 0, badNext = 0, compared = 0, differ = 0, duplicates = 0, known = 0;
    long messages = 0, offScreen = 0;
};

// Two nodes sharing a number are a fault in the game's script, and each one needs checking against the PC game before it's
// listed here. Test Game 95's second node 1125 points at itself, and the PC follows the first, which ends the scene.
struct KnownDuplicate {
    const char *game, *script;
    int id;
};
const KnownDuplicate kKnownDuplicates[] = {{"waffle\\alchemy", "scr\\ep_004_000_000.txt", 1125}};

// The game's name for its save folder, from init.ini, which is what tells one game's scripts from another's.
std::string gameKey(const Files &files) {
    Bytes ini = files.read("cfg\\init.ini");
    if (!ini) return "";
    std::string text(ini->begin(), ini->end());
    auto at = text.find("UserDataDirectory");
    if (at == std::string::npos) return "";
    at = text.find_first_not_of(" \t", at + std::strlen("UserDataDirectory"));
    auto end = at == std::string::npos ? at : text.find_first_of(" \t\r\n", at);
    return at == std::string::npos ? "" : normalizedPath(text.substr(at, end - at));
}

void compare(const Node &n, const char *key, int value, Totals &t, const std::string &script) {
    long shown;
    if (!field(n.s[0], key, shown)) return;
    t.compared++;
    if (shown != value) {
        t.differ++;
        if (t.differ <= 20) std::printf("  differs: %s #%d op %d %s shows %ld, parameter %d\n", script.c_str(), n.id, n.opcode, key, shown, value);
    }
}

// Lays out every message of the game's scenes as the runtime sets it, and counts those whose ink runs past the canvas.
void checkMessages(const char *root, const Files &files, Totals &t) {
    Engine engine;
    std::string error;
    if (!engine.start(root, "", "/dev/null", NEKO_FONTS_DIR, error)) {
        std::printf("  the runtime didn't start: %s\n", error.c_str());
        t.offScreen++;
        return;
    }
    for (const std::string &path : files.list("scr")) {
        Bytes data = files.read(path);
        Script script;
        if (!data || !parseScript(data->data(), data->size(), script)) continue;
        for (const Node &n : script.nodes) {
            if (n.opcode != 5) continue;
            t.messages++;
            int right, bottom;
            engine.messageExtent(sjis::toUtf8(n.s[1]), sjis::toUtf8(n.s[2]), right, bottom);
            if (right <= engine.canvasWidth() && bottom <= engine.canvasHeight()) continue;
            if (++t.offScreen <= 10) std::printf("  off the screen: %s #%d reaches %d, %d\n", path.c_str(), n.id, right, bottom);
        }
    }
}

}  // namespace

int main(int argc, char **argv) {
    Totals all;
    for (int a = 1; a < argc; a++) {
        Files files;
        if (!files.open(argv[a])) {
            std::printf("%s: no archives\n", argv[a]);
            continue;
        }
        Totals t;
        const std::string key = gameKey(files);
        for (const char *folder : {"scr", "flow", "gt"}) {
            for (const std::string &path : files.list(folder)) {
                if (path.size() < 4 || path.compare(path.size() - 4, 4, ".txt")) continue;
                // GOOD Timing's lists of scripts, which are plain text and not scripts.
                if (path.rfind("gt\\cfg\\", 0) == 0) continue;
                Bytes data = files.read(path);
                Script script;
                size_t trailing = 0;
                t.scripts++;
                if (!data || !parseScript(data->data(), data->size(), script, &trailing)) {
                    t.badParse++;
                    std::printf("  unreadable: %s\n", path.c_str());
                    continue;
                }
                if (trailing) t.trailing++;
                for (int id : script.duplicateIds) {
                    bool known = false;
                    for (const KnownDuplicate &k : kKnownDuplicates)
                        known = known || (key == normalizedPath(k.game) && path == normalizedPath(k.script) && id == k.id);
                    if (known) t.known++;
                    else t.duplicates++;
                    std::printf("  two nodes numbered %d%s: %s\n", id, known ? " (known, the PC follows the first)" : "", path.c_str());
                }
                for (const Node &n : script.nodes) {
                    t.nodes++;
                    if (n.next != 0 && !script.node(n.next)) t.badNext++;
                    if (std::strcmp(folder, "flow") == 0) continue;
                    switch (n.opcode) {
                    case 2: compare(n, "time:", n.p[0], t, path); break;
                    case 9:
                        compare(n, "type:", n.p[0], t, path);
                        compare(n, "time:", n.p[1], t, path);
                        break;
                    case 30:
                    case 35:
                    case 41:
                        compare(n, "ch:", n.p[0], t, path);
                        compare(n, "vol:", n.p[1], t, path);
                        compare(n, "pos:", n.p[2], t, path);
                        compare(n, "tm:", n.p[3], t, path);
                        compare(n, "loop:", n.p[4], t, path);
                        break;
                    case 36:
                    case 42:
                        compare(n, "ch:", n.p[0], t, path);
                        compare(n, "tm:", n.p[1], t, path);
                        break;
                    case 53: compare(n, "type:", n.p[0], t, path); break;
                    default: break;
                    }
                }
            }
        }
        checkMessages(argv[a], files, t);
        std::printf("%s: scripts %ld, nodes %ld, unreadable %ld, with bytes after the last node %ld, next naming no node %ld, fields compared %ld, differing %ld\n",
                    argv[a], t.scripts, t.nodes, t.badParse, t.trailing, t.badNext, t.compared, t.differ);
        std::printf("%s: messages %ld, off the screen %ld\n", argv[a], t.messages, t.offScreen);
        all.scripts += t.scripts;
        all.nodes += t.nodes;
        all.badParse += t.badParse;
        all.trailing += t.trailing;
        all.badNext += t.badNext;
        all.differ += t.differ;
        all.duplicates += t.duplicates;
        all.offScreen += t.offScreen;
        std::printf("%s: new duplicate node numbers %ld, known %ld\n", argv[a], t.duplicates, t.known);
    }
    return all.badParse || all.trailing || all.badNext || all.differ || all.duplicates || all.offScreen ? 1 : 0;
}

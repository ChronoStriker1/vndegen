// Checks the script reader against a game's own bytes: every script must parse to its last byte, every `next` must name
// a node, and the numbers a node shows in its readable first string must equal its parameters.
// usage: nekocheck <game folder>...
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

#include "archive.h"
#include "script.h"

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
    long scripts = 0, nodes = 0, badParse = 0, trailing = 0, badNext = 0, compared = 0, differ = 0;
};

void compare(const Node &n, const char *key, int value, Totals &t, const std::string &script) {
    long shown;
    if (!field(n.s[0], key, shown)) return;
    t.compared++;
    if (shown != value) {
        t.differ++;
        if (t.differ <= 20) std::printf("  differs: %s #%d op %d %s shows %ld, parameter %d\n", script.c_str(), n.id, n.opcode, key, shown, value);
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
        for (const char *folder : {"scr", "flow", "gt"}) {
            for (const std::string &path : files.list(folder)) {
                if (path.size() < 4 || path.compare(path.size() - 4, 4, ".txt")) continue;
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
                std::map<int, int> ids;
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
        std::printf("%s: scripts %ld, nodes %ld, unreadable %ld, with bytes after the last node %ld, next naming no node %ld, fields compared %ld, differing %ld\n",
                    argv[a], t.scripts, t.nodes, t.badParse, t.trailing, t.badNext, t.compared, t.differ);
        all.scripts += t.scripts;
        all.nodes += t.nodes;
        all.badParse += t.badParse;
        all.trailing += t.trailing;
        all.badNext += t.badNext;
        all.differ += t.differ;
    }
    return all.badParse || all.trailing || all.badNext || all.differ ? 1 : 0;
}

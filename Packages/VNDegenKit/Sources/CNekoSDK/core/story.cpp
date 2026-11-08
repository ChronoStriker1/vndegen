// The story table: for the games with no flow scripts, whose order of scenes, choices and route variables the PC keeps
// in its exe, the order as traced under Wine, written as `flow` blocks under `screen story` in the game's table. Each
// flow compiles to the nodes a flow script has, so the flow interpreter runs it as it runs the other games' flows.
//
//   flow NAME                 a flow; New Game starts at the first
//   scene PATH                play a scene script to its end
//   choice PICTURE COUNT      ask a choice
//   on N goto LABEL           when the last choice or screen gave N, go to LABEL, else on; a choice's options count from 0
//   goto LABEL / label LABEL
//   set S|G NUMBER OP VALUE   OP = + - += -=; VALUE a number, or S n or G n
//   if S|G NUMBER CMP VALUE goto LABEL     CMP == != <= >= < >
//   open NAME ARGS... [-> S|G NUMBER]      a game part's screen; its result to the variable and the last result
//   chapter N                 the scenes a game part gives for chapter N
//   end / staffroll / jump FLOW
//
// Words from one starting with `#` are a comment. Paths are UTF-8 here and CP932 in the game's files.
#include <cstdlib>

#include "engine.h"
#include "sjis.h"

namespace neko {
namespace {

// Opcodes the story table needs that flow scripts don't have. Flow scripts' own stop well below these.
constexpr int kStoryGoto = 1001, kStoryOpen = 1002, kStoryChapter = 1003;

std::vector<std::string> withoutComment(const std::vector<std::string> &w) {
    std::vector<std::string> out;
    for (const std::string &word : w) {
        if (!word.empty() && word[0] == '#') break;
        out.push_back(word);
    }
    return out;
}

int toInt(const std::string &s) { return std::atoi(s.c_str()); }

}  // namespace

std::string Engine::firstStoryFlow() const {
    auto it = screens_.find("story");
    if (it == screens_.end()) return "";
    for (const auto &line : it->second.lines)
        if (line.size() >= 2 && line[0] == "flow") return line[1];
    return "";
}

std::shared_ptr<Script> Engine::compileStory(const std::string &flowName) {
    auto table = screens_.find("story");
    if (table == screens_.end()) return nullptr;
    auto script = std::make_shared<Script>();
    script->name = "story:" + flowName;
    std::map<std::string, int> labels;
    std::vector<std::pair<size_t, std::string>> pendingJumps;  // node index, label for `otherwise`
    bool inFlow = false;
    auto add = [&](int opcode) -> Node & {
        Node n;
        n.id = int(script->nodes.size()) + 1;
        n.opcode = opcode;
        n.next = n.id + 1;
        script->nodes.push_back(n);
        return script->nodes.back();
    };
    auto fail = [&](const std::vector<std::string> &w, const std::string &why) {
        std::string text;
        for (const std::string &word : w) text += word + " ";
        complain("Story table, flow " + flowName + ": " + why + ": " + text);
    };
    // A variable by the number saves keep: the flow nodes name it by its place in macrolist.ini's list.
    auto variable = [&](const std::string &scope, const std::string &number, Node &n, const std::vector<std::string> &w) {
        int s = scope == "S" ? 1 : 0, value = toInt(number), index = -1;
        if (scope != "S" && scope != "G") fail(w, "a variable's scope is S or G");
        const auto &list = s == 1 ? scenarioMacros_ : globalMacros_;
        for (size_t i = 0; i < list.size(); i++)
            if (list[i] == value) index = int(i);
        // A game with no list names its variables by number, as a place past a list's end does.
        if (index < 0 && !list.empty()) fail(w, "this number has no place in macrolist.ini");
        if (index < 0) index = value;
        n.p[0] = s;
        n.p[1] = n.p[2] = index;
    };
    // The other side: a number, or S n or G n.
    auto operand = [&](const std::vector<std::string> &w, size_t at, Node &n) {
        if (at < w.size() && (w[at] == "S" || w[at] == "G") && at + 1 < w.size()) {
            n.p[4] = w[at] == "S" ? 1 : 0;
            n.p[5] = toInt(w[at + 1]);
            return at + 2;
        }
        n.p[4] = 2;
        n.p[5] = at < w.size() ? toInt(w[at]) : 0;
        return at + 1;
    };
    add(0);
    for (const auto &raw : table->second.lines) {
        auto w = withoutComment(raw);
        if (w.empty()) continue;
        if (w[0] == "flow") {
            if (inFlow) break;
            inFlow = w.size() >= 2 && w[1] == flowName;
            continue;
        }
        if (!inFlow) continue;
        if (w[0] == "label" && w.size() >= 2) {
            labels[w[1]] = int(script->nodes.size()) + 1;
            continue;
        } else if (w[0] == "scene" && w.size() >= 2) {
            add(1).s[1] = sjis::fromUtf8(w[1]);
        } else if (w[0] == "choice" && w.size() >= 3) {
            Node &n = add(3);
            n.s[1] = sjis::fromUtf8(w[1]);
            // A flow's choice gives its count less one, and in parameters 10 to 13 each route's answer, 4 for none.
            n.p[0] = n.p[1] = toInt(w[2]) - 1;
            for (size_t r = 10; r < 14; r++) n.p[r] = 4;
        } else if (w[0] == "on" && w.size() >= 4 && w[2] == "goto") {
            Node &n = add(4);
            n.p[0] = toInt(w[1]);
            pendingJumps.push_back({script->nodes.size() - 1, w[3]});
        } else if (w[0] == "goto" && w.size() >= 2) {
            add(kStoryGoto);
            pendingJumps.push_back({script->nodes.size() - 1, w[1]});
        } else if (w[0] == "set" && w.size() >= 4) {
            Node &n = add(6);
            variable(w[1], w[2], n, w);
            const std::string &op = w[3];
            n.p[3] = op == "=" ? 0 : op == "+" ? 1 : op == "-" ? 2 : op == "+=" ? 3 : op == "-=" ? 4 : -1;
            if (n.p[3] < 0) fail(w, "a change is = + - += or -=");
            operand(w, 4, n);
        } else if (w[0] == "if" && w.size() >= 7) {
            Node &n = add(5);
            variable(w[1], w[2], n, w);
            const std::string &cmp = w[3];
            n.p[3] = cmp == "==" ? 0 : cmp == "!=" ? 1 : cmp == "<=" ? 2 : cmp == ">=" ? 3 : cmp == "<" ? 4 : cmp == ">" ? 5 : -1;
            if (n.p[3] < 0) fail(w, "a comparison is == != <= >= < or >");
            size_t at = operand(w, 4, n);
            if (at + 1 < w.size() && w[at] == "goto") pendingJumps.push_back({script->nodes.size() - 1, w[at + 1]});
            else fail(w, "an `if` ends with goto LABEL");
        } else if (w[0] == "open" && w.size() >= 2) {
            Node &n = add(kStoryOpen);
            n.s[1] = w[1];
            size_t i = 2;
            int count = 0;
            for (; i < w.size() && w[i] != "->" && count < 24; i++) n.p[size_t(4 + count++)] = toInt(w[i]);
            n.p[3] = count;
            n.p[0] = -1;
            if (i + 2 < w.size() && w[i] == "->") {
                n.p[0] = w[i + 1] == "S" ? 1 : 0;
                n.p[1] = toInt(w[i + 2]);
            }
        } else if (w[0] == "chapter" && w.size() >= 2) {
            add(kStoryChapter).p[0] = toInt(w[1]);
        } else if (w[0] == "end") {
            add(2);
        } else if (w[0] == "staffroll") {
            add(2).p[0] = 1;
        } else if (w[0] == "jump" && w.size() >= 2) {
            add(31).s[1] = "story:" + w[1];
        } else {
            fail(w, "not a story line");
        }
    }
    if (script->nodes.size() <= 1) {
        complain("Story table has no flow " + flowName);
        return nullptr;
    }
    // The last line goes nowhere: a flow that runs off its end ends there, as a flow script does.
    script->nodes.back().next = 0;
    for (auto &[at, name] : pendingJumps) {
        auto it = labels.find(name);
        if (it == labels.end()) {
            complain("Story table, flow " + flowName + ": no label " + name);
            continue;
        }
        script->nodes[at].otherwise = it->second;
    }
    for (size_t i = 0; i < script->nodes.size(); i++) script->byId[script->nodes[i].id] = int(i);
    return script;
}

std::shared_ptr<Script> Engine::compileChapter(int number) {
    std::vector<std::string> scenes;
    for (auto &[name, part] : parts_) {
        scenes = part->chapter(*partHost_, number);
        if (!scenes.empty()) break;
    }
    if (scenes.empty()) {
        complain("No game part plays chapter " + std::to_string(number));
        return nullptr;
    }
    auto script = std::make_shared<Script>();
    script->name = "chapter:" + std::to_string(number);
    Node start;
    start.id = 1;
    start.next = 2;
    script->nodes.push_back(start);
    for (const std::string &scene : scenes) {
        Node n;
        n.id = int(script->nodes.size()) + 1;
        n.opcode = 1;
        n.next = n.id + 1;
        n.s[1] = scene;
        script->nodes.push_back(n);
    }
    script->nodes.back().next = 0;
    for (size_t i = 0; i < script->nodes.size(); i++) script->byId[script->nodes[i].id] = int(i);
    return script;
}

// A PC save of a game without flow scripts names only its scene, so the story goes on after the first `scene` line that
// names it, in any flow. A save names a scene a patch gives by the patch's name for it, and the PC's saves of the same
// game unpatched by its own.
bool Engine::resumeStoryAt(const std::string &scene) {
    auto table = screens_.find("story");
    if (table == screens_.end()) return false;
    std::string want = normalizedPath(scene);
    auto names = [&](const Node &n) {
        return n.opcode == 1 && (normalizedPath(n.s[1]) == want || normalizedPath(files_.savedName(n.s[1])) == want);
    };
    for (const auto &line : table->second.lines) {
        if (line.size() < 2 || line[0] != "flow") continue;
        auto flow = loadScript("story:" + line[1]);
        if (!flow) continue;
        for (const Node &n : flow->nodes) {
            if (names(n)) {
                stack_.push_back({flow, flow->node(n.next)});
                flowCallId_ = n.id;
                return true;
            }
            // Or inside a chapter: the flow goes on after the chapter, and the chapter after the scene.
            if (n.opcode == kStoryChapter)
                if (auto chapter = loadScript("chapter:" + std::to_string(n.p[0])))
                    for (const Node &c : chapter->nodes)
                        if (names(c)) {
                            stack_.push_back({flow, flow->node(n.next)});
                            stack_.push_back({chapter, chapter->node(c.next)});
                            flowCallId_ = n.id;
                            return true;
                        }
        }
    }
    return false;
}

}  // namespace neko

// The story table: for the games with no flow scripts, whose order of scenes, choices and route variables the PC keeps
// in its exe, the order as traced under Wine, written as `flow` blocks under `screen story` in the game's table. Each
// flow compiles to the nodes a flow script has, so the flow interpreter runs it as it runs the other games' flows.
//
//   flow NAME                 a flow; New Game starts at the first
//   scene PATH                play a scene script to its end
//   choice PICTURE COUNT [cells C...]   ask a choice; `cells` names each option's column of the picture, which
//                             `choicecolumns N` under `screen choice` divides, as Test Game 82's one sheet of all its options
//   on N goto LABEL           when the last choice or screen gave N, go to LABEL, else on; a choice's options count from 0
//   goto LABEL / label LABEL
//   set S|G NUMBER OP VALUE   OP = + - += -=; VALUE a number, or S n or G n
//   if S|G NUMBER CMP VALUE goto LABEL     CMP == != <= >= < >
//   open NAME ARGS... [-> S|G NUMBER]      a game part's screen; its result to the variable and the last result
//   chapter N / chapter S n / chapter G n   a game part's scenes for chapter N, or the number a variable holds
//   end / staffroll [NAME] / jump FLOW
//
// A `scene` or `choice` line may end with `@N`, the PC's own number for that place in its story, which its saves hold as
// the flow node and need to go on from it (Test Game 87, set 1): the runtime writes it in its saves, and a PC save goes on
// from the line with its number before the first line naming its scene.
//
// Words from one starting with `#` are a comment. Paths are UTF-8 here and CP932 in the game's files.
#include <cstdlib>

#include "engine.h"
#include "sjis.h"

namespace neko {
namespace {

std::vector<std::string> withoutComment(const std::vector<std::string> &w) {
    std::vector<std::string> out;
    for (const std::string &word : w) {
        if (!word.empty() && word[0] == '#') break;
        out.push_back(word);
    }
    return out;
}

// A line's `@N`, taken off its words: the PC's number for the place, or 0.
int pcNumber(std::vector<std::string> &w) {
    if (w.size() < 2 || w.back().size() < 2 || w.back()[0] != '@') return 0;
    int number = std::atoi(w.back().c_str() + 1);
    w.pop_back();
    return number;
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

bool Engine::storyFlowExists(const std::string &name) const {
    auto it = screens_.find("story");
    if (it == screens_.end()) return false;
    for (const auto &line : it->second.lines)
        if (line.size() >= 2 && line[0] == "flow" && line[1] == name) return true;
    return false;
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
        int pc = pcNumber(w);
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
            Node &n = add(1);
            n.s[1] = sjis::fromUtf8(w[1]);
            n.p[kStoryPcNumber] = pc;
        } else if (w[0] == "choice" && w.size() >= 3) {
            Node &n = add(3);
            n.s[1] = sjis::fromUtf8(w[1]);
            // A flow's choice gives its count less one, and in parameters 10 to 13 each route's answer, 4 for none.
            n.p[0] = n.p[1] = toInt(w[2]) - 1;
            for (size_t r = 10; r < 14; r++) n.p[r] = 4;
            n.p[kStoryPcNumber] = pc;
            // Each option's column, in parameters no flow script's choice uses: how many in 14, the columns from 15.
            if (w.size() > 4 && w[3] == "cells") {
                for (size_t i = 4; i < w.size() && i - 4 < kChoiceCellsMax; i++) n.p[kChoiceCells + 1 + (i - 4)] = toInt(w[i]);
                n.p[kChoiceCells] = int(std::min(w.size() - 4, size_t(kChoiceCellsMax)));
            }
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
        } else if (w[0] == "resume" && w.size() >= 7) {
            // `resume S n OP VALUE at LABEL`: a PC save without VNDegen's lines, whose variable holds, goes on from
            // LABEL, as Test Game 96's saves at its hub and its vote's pick say where they are only in S 105 (set 2).
            // The story passes over the line itself.
            Node &n = add(kStoryResume);
            variable(w[1], w[2], n, w);
            const std::string &cmp = w[3];
            n.p[3] = cmp == "==" ? 0 : cmp == "!=" ? 1 : cmp == "<=" ? 2 : cmp == ">=" ? 3 : cmp == "<" ? 4 : cmp == ">" ? 5 : -1;
            if (n.p[3] < 0) fail(w, "a comparison is == != <= >= < or >");
            size_t at = operand(w, 4, n);
            if (at + 1 < w.size() && w[at] == "at") pendingJumps.push_back({script->nodes.size() - 1, w[at + 1]});
            else fail(w, "a `resume` ends with at LABEL");
        } else if (w[0] == "open" && w.size() >= 2) {
            Node &n = add(kStoryOpen);
            n.s[1] = w[1];
            size_t i = 2;
            int count = 0;
            for (; i < w.size() && w[i] != "->" && count < 24; i++) n.p[size_t(4 + count++)] = toInt(w[i]);
            n.p[3] = count;
            n.p[0] = -1;
            // `@N` on an open line: the PC's number while that screen waits, as Test Game 90's select screen has one for
            // each half-week (set 1).
            n.p[kStoryPcNumber] = pc;
            if (i + 2 < w.size() && w[i] == "->") {
                n.p[0] = w[i + 1] == "S" ? 1 : 0;
                n.p[1] = toInt(w[i + 2]);
            }
        } else if (w[0] == "chapter" && w.size() >= 2) {
            // `chapter N`, or `chapter S n` / `chapter G n` for the number a variable holds when the line runs, as Test
            // Game 96's hub leaves the chapter it chose in a variable (set 2).
            Node &chapter = add(kStoryChapter);
            operand(w, 1, chapter);
        } else if (w[0] == "end") {
            add(2);
        } else if (w[0] == "staffroll") {
            // `staffroll NAME` opens the table's `screen staffroll_NAME`, as Test Game 82 has a roll to each ending (set 2).
            Node &roll = add(2);
            roll.p[0] = 1;
            if (w.size() >= 2) roll.s[1] = w[1];
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

std::shared_ptr<Script> Engine::compileChapter(int number, bool complains) {
    std::vector<std::string> scenes;
    for (auto &[name, part] : parts_) {
        scenes = part->chapter(*partHost_, number);
        if (!scenes.empty()) break;
    }
    if (scenes.empty()) {
        if (complains) complain("No game part plays chapter " + std::to_string(number));
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
        // `scr\x.txt @N`: the scene and the PC's number for it, as Test Game 90's picks have one for each (set 1).
        size_t at = scene.rfind(" @");
        n.s[1] = at == std::string::npos ? scene : scene.substr(0, at);
        if (at != std::string::npos) n.p[kStoryPcNumber] = std::atoi(scene.c_str() + at + 2);
        script->nodes.push_back(n);
    }
    script->nodes.back().next = 0;
    for (size_t i = 0; i < script->nodes.size(); i++) script->byId[script->nodes[i].id] = int(i);
    return script;
}

uint32_t Engine::storyTableHash() const {
    uint32_t hash = 2166136261u;
    auto table = screens_.find("story");
    if (table == screens_.end()) return hash;
    for (const auto &line : table->second.lines) {
        for (const std::string &word : withoutComment(line)) {
            for (char c : word) hash = (hash ^ uint8_t(c)) * 16777619u;
            hash = (hash ^ 0x20u) * 16777619u;
        }
        hash = (hash ^ 0x0Au) * 16777619u;
    }
    return hash;
}

// The exact place VNDegen's lines beside its own save hold: each flow frame, the last result, and a part screen open.
bool Engine::restoreStoryPlace(const Sidecar &side, bool &atChoice) {
    if (side.story.empty()) return false;
    if (side.hasResult) choiceResult_ = side.result;
    if (side.hasStoryTable && side.storyTable != storyTableHash()) return false;
    std::vector<Frame> frames;
    for (const auto &[name, id] : side.story) {
        auto script = loadScript(name);
        if (!script) return false;
        frames.push_back({script, id ? script->node(id) : nullptr});
    }
    stack_ = std::move(frames);
    const Node *top = stack_.back().node;
    // Only a save made while the choice waited asks it again; one in the scene before goes on from the scene.
    atChoice = side.atChoice && top && top->opcode == 3;
    for (auto &[name, part] : parts_)
        if (name == side.screen) {
            partScreen_ = part.get();
            partResultScope_ = side.screenScope;
            partResultNumber_ = side.screenNumber;
        }
    return true;
}

// Whether a story line is a `scene` line for this scene, by the table's name for it or a patch's.
bool Engine::storySceneIs(const Node &n, const std::string &scene) {
    std::string want = normalizedPath(scene);
    return n.opcode == 1 && (normalizedPath(n.s[1]) == want || normalizedPath(files_.savedName(n.s[1])) == want);
}

bool Engine::storyConditionHolds(const Node &n) {
    int left = variableValue(n.p[0], variableIndex(n)), right = operand(n);
    return n.p[3] == 0 ? left == right : n.p[3] == 1 ? left != right : n.p[3] == 2 ? left <= right : n.p[3] == 3 ? left >= right : n.p[3] == 4 ? left < right : left > right;
}

// A PC save, without VNDegen's lines beside it, at a place only its variables tell: the first `resume` line whose
// condition holds is the place, its label's line run at once.
bool Engine::resumeStoryByRule(bool &atScreen) {
    auto table = screens_.find("story");
    if (table == screens_.end()) return false;
    for (const auto &line : table->second.lines) {
        if (line.size() < 2 || line[0] != "flow") continue;
        auto flow = loadScript("story:" + line[1]);
        if (!flow) continue;
        for (const Node &n : flow->nodes) {
            if (n.opcode != kStoryResume || !n.otherwise || !storyConditionHolds(n)) continue;
            stack_.push_back({flow, flow->node(n.otherwise)});
            atScreen = true;
            return true;
        }
    }
    return false;
}

// A PC save of a game without flow scripts names its scene and the PC's number for its place in the story, 3 for while a
// choice waits: the line with that number, if the table gives numbers, is the place, a `scene` or `choice` line or an
// `open` line, whose screen opens again. Numbers repeat, as both options of
// Test Game 87's choices and each ending's last scene share one (set 1), so the line whose scene the save names wins.
bool Engine::resumeStoryAtNumber(int number, int active, const std::string &scene, bool &atChoice, bool &atScreen) {
    auto table = screens_.find("story");
    if (table == screens_.end() || number <= 0) return false;
    std::shared_ptr<Script> found, chapter;
    const Node *at = nullptr, *call = nullptr;
    // A line of a flow, or of a chapter a flow plays, with the number; the one whose scene the save names wins.
    auto consider = [&](const std::shared_ptr<Script> &flow, const Node &n, const std::shared_ptr<Script> &in, const Node *from) {
        if (n.p[kStoryPcNumber] != number || (at && !storySceneIs(n, scene))) return false;
        found = flow;
        chapter = in;
        at = &n;
        call = from;
        return storySceneIs(n, scene);
    };
    bool named = false;
    for (const auto &line : table->second.lines) {
        if (named) break;
        if (line.size() < 2 || line[0] != "flow") continue;
        auto flow = loadScript("story:" + line[1]);
        if (!flow) continue;
        for (const Node &n : flow->nodes) {
            if ((named = consider(flow, n, nullptr, nullptr))) break;
            if (n.opcode == kStoryChapter)
                if (auto inside = compileChapter(operand(n), false))
                    for (const Node &c : inside->nodes)
                        if ((named = consider(flow, c, inside, &n))) break;
            if (named) break;
        }
    }
    if (!at) return false;
    // Inside a chapter: the flow goes on after the chapter, and the chapter after the scene.
    if (chapter) {
        stack_.push_back({found, found->node(call->next)});
        stack_.push_back({chapter, chapter->node(at->next)});
        flowCallId_ = number;
        flowCallNumbered_ = true;
        return true;
    }
    // A save while the choice waits asks it again; one after it goes on past it. One at a screen's number opens the
    // screen again.
    atChoice = at->opcode == 3 && active == 3;
    atScreen = at->opcode == kStoryOpen;
    stack_.push_back({found, atChoice || atScreen ? at : found->node(at->next)});
    flowCallId_ = number;
    flowCallNumbered_ = true;
    return true;
}

// A PC save of a game without flow scripts names only its scene, so the story goes on after the first `scene` line that
// names it, in any flow. A save names a scene a patch gives by the patch's name for it, and the PC's saves of the same
// game unpatched by its own.
bool Engine::resumeStoryAt(const std::string &scene) {
    auto table = screens_.find("story");
    if (table == screens_.end()) return false;
    auto names = [&](const Node &n) { return storySceneIs(n, scene); };
    for (const auto &line : table->second.lines) {
        if (line.size() < 2 || line[0] != "flow") continue;
        auto flow = loadScript("story:" + line[1]);
        if (!flow) continue;
        for (const Node &n : flow->nodes) {
            // The place is the line's PC number when it has one, as running it sets, so a save after this one holds it
            // (set 2: after a jump, Test Game 82's saves had held the line's node instead).
            if (names(n)) {
                stack_.push_back({flow, flow->node(n.next)});
                flowCallNumbered_ = n.p[kStoryPcNumber] != 0;
                flowCallId_ = flowCallNumbered_ ? n.p[kStoryPcNumber] : n.id;
                return true;
            }
            // Or inside a chapter: the flow goes on after the chapter, and the chapter after the scene.
            if (n.opcode == kStoryChapter)
                if (auto chapter = compileChapter(operand(n), false))
                    for (const Node &c : chapter->nodes)
                        if (names(c)) {
                            stack_.push_back({flow, flow->node(n.next)});
                            stack_.push_back({chapter, chapter->node(c.next)});
                            flowCallNumbered_ = c.p[kStoryPcNumber] || n.p[kStoryPcNumber];
                            flowCallId_ = c.p[kStoryPcNumber] ? c.p[kStoryPcNumber] : n.p[kStoryPcNumber] ? n.p[kStoryPcNumber] : n.id;
                            return true;
                        }
        }
    }
    return false;
}

}  // namespace neko

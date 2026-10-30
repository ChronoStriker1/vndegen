// The event interpreter: runs map event pages and common events one command at a time.
#include "game.h"
#include "game_internal.h"
#include "text.h"

#include <algorithm>
#include <cmath>

#include "images.h"

namespace wolf {

namespace {

// 600050 to 600150 name a common event relative to the running one, 600100 being itself (WodiLib's CommonEventId).
int resolveRelativeCommon(int id, const Thread &t) {
    if (id < 600050 || id > 600150 || t.stack.empty() || t.stack.back().commonEvent < 0) return id;
    return 500000 + t.stack.back().commonEvent + id - 600100;
}

}  // namespace

void Game::run(Thread &t) {
    if (t.wait > 0) {
        t.wait--;
        return;
    }
    // A thread runs until it waits. The cap stops a loop without waits from hanging the frame.
    for (int budget = 0; budget < 100000 && !t.done(); budget++) {
        if (!step(t)) return;
    }
}

void Game::jumpToBranchEnd(Thread::Call &c, int indent) {
    const Commands &cmds = *c.commands;
    while (c.pc < cmds.size() && !(cmds[c.pc].indent == indent && cmds[c.pc].code == 499)) c.pc++;
    if (c.pc < cmds.size()) c.pc++;
}

// After a branching command, moves to the body of the case it chose, or past the branch.
bool Game::jumpToCase(Thread::Call &c, int indent, int caseValue) {
    const Commands &cmds = *c.commands;
    size_t pc = c.pc;
    for (; pc < cmds.size(); pc++) {
        auto &cmd = cmds[pc];
        if (cmd.indent != indent) continue;
        if (cmd.code == 499) break;
        bool match = (cmd.code == 401 && cmd.arg(0) == caseValue) || (caseValue == 0 && cmd.code == 420) ||
                     (caseValue == -2 && cmd.code == 421);
        if (match) {
            c.pc = pc + 1;
            return true;
        }
    }
    c.pc = pc < cmds.size() ? pc + 1 : pc;
    return false;
}

void Game::unsupported(const Command &c) {
    if (unsupported_[c.code]++ == 0) log(format("command %d is not supported yet", c.code));
}

bool Game::step(Thread &t) {
    Thread::Call &c = t.stack.back();
    if (t.waitingMessage) {
        if (message_.active) return false;
        t.waitingMessage = false;
    }
    if (t.waitingRoute) {
        if (t.waitingRoute->route) return false;
        t.waitingRoute = nullptr;
    }
    if (t.waitingChoice) {
        if (choice_.active) return false;
        t.waitingChoice = false;
        // Choice n's case is numbered n + 1; cancel has its own case when the choice allows it.
        jumpToCase(c, t.choiceIndent, choice_.result > 0 ? choice_.result + 1 : -2);
        return true;
    }
    if (c.pc >= c.commands->size()) {
        Thread::Call done = c;
        t.stack.pop_back();
        if (done.returnTo && done.commonEvent >= 0 && !t.stack.empty()) {
            const CommonEvent *ce = commonById(done.commonEvent);
            int slot = ce ? ce->returnVariable : -1;
            if (slot >= 0 && slot < 100) {
                if (isStringVar(done.returnTo)) writeStr(done.returnTo, cselfStr_[done.commonEvent][slot], t);
                else writeVar(done.returnTo, cself_[done.commonEvent][slot], t);
            }
        }
        return true;
    }
    const Command &cmd = (*c.commands)[c.pc];
    int indent = cmd.indent;
    if (trace_)
        log(format("f%llu %s%d line %zu: %d", (unsigned long long)frame_, c.commonEvent >= 0 ? "ce" : "ev",
                   c.commonEvent >= 0 ? c.commonEvent : c.mapEvent, c.pc, cmd.code));
    c.pc++;

    switch (cmd.code) {
        case 0:    // blank line
        case 99:   // checkpoint
        case 103:  // comment
        case 106:  // debug text
        case 107:  // clear debug text
        case 212:  // label
            return true;

        case 101:  // message: the thread resumes once the player closes the window
            showMessage(expandText(cmd.str(0), &t));
            t.waitingMessage = true;
            return false;

        case 102: {  // choices
            int flags = cmd.arg(0);
            int count = flags & 0x0F;
            int cancel = (flags & 0xF0) >> 4;
            choice_ = ChoiceState{};
            choice_.active = true;
            for (int i = 0; i < count; i++) {
                std::string text = expandText(cmd.str(i), &t);
                choice_.items.push_back(text);
                if (!text.empty()) choice_.visible.push_back(i);
            }
            // 0 sends cancel to its own case, 1 ignores it, 2 and up pick a choice.
            choice_.cancelCase = cancel == 1 ? -1 : cancel == 0 ? -2 : cancel - 1;
            t.waitingChoice = true;
            t.choiceIndent = indent;
            return false;
        }

        case 111: {  // numeric condition
            int flags = cmd.arg(0);
            int count = flags & 0x0F;
            bool elseCase = (flags & 0xF0) != 0;
            int chosen = 0;
            for (int i = 0; i < count; i++) {
                int32_t left = readVar(cmd.arg(1 + i * 3), t);
                int32_t rflags = cmd.arg(3 + i * 3);
                int32_t right = (rflags & 0xF0) ? cmd.arg(2 + i * 3) : value(cmd.arg(2 + i * 3), t);
                if (compare(left, right, rflags & 0x0F)) {
                    chosen = i + 1;
                    break;
                }
            }
            if (chosen == 0 && !elseCase) chosen = -1;
            jumpToCase(c, indent, chosen);
            return true;
        }

        case 112: {  // string condition
            int flags = cmd.arg(0);
            int count = flags & 0x0F;
            bool elseCase = (flags & 0xF0) != 0;
            int chosen = 0;
            for (int i = 0; i < count; i++) {
                int32_t leftWord = cmd.arg(1 + i);
                int32_t leftId = leftWord & 0x00FFFFFF;
                if (leftId < 1000000) leftId += 0;
                int op = (leftWord >> 24) & 0xF0;
                bool rightIsVar = ((leftWord >> 24) & 0x0F) == 1;
                std::string left = readStr(leftId >= 1000000 ? leftId : 3000000 + leftId, t);
                std::string right = rightIsVar ? readStr(cmd.arg(1 + count + i), t) : cmd.str(i);
                bool ok = false;
                switch (op) {
                    case 0x00: ok = left == right; break;
                    case 0x10: ok = left != right; break;
                    case 0x20: ok = left.find(right) != std::string::npos; break;
                    case 0x30: ok = left.starts_with(right); break;
                }
                if (ok) {
                    chosen = i + 1;
                    break;
                }
            }
            if (chosen == 0 && !elseCase) chosen = -1;
            jumpToCase(c, indent, chosen);
            return true;
        }

        case 401:
        case 420:
        case 421:
            // Reaching another case of the same branch ends the case that ran.
            jumpToBranchEnd(c, indent);
            return true;
        case 499:
            return true;

        case 121:
            setVariable(t, cmd);
            return true;
        case 122:
            // Typing text on the keyboard waits for the player in Wolf. With no text entry yet the command keeps the
            // text already there and takes a frame, so a loop around it runs once a frame instead of spinning.
            if ((uint32_t(cmd.arg(1)) & 0x0F) == 3) {
                unsupported(cmd);
                t.wait = 1;
                return false;
            }
            setString(t, cmd);
            return true;

        case 150:
            pictureCommand(t, cmd);
            return true;

        case 151: {  // change screen colour
            int32_t packed = cmd.arg(0);
            int frames = value(cmd.arg(1), t);
            // The colour is three percentages packed into one value, red in the lowest byte.
            tintR_.set(packed & 0xFF, frames);
            tintG_.set((packed >> 8) & 0xFF, frames);
            tintB_.set((packed >> 16) & 0xFF, frames);
            return true;
        }

        case 170:  // loop
            return true;
        case 179: {  // loop a number of times
            size_t at = c.pc - 1;
            auto it = c.loopCounts.find(at);
            if (it == c.loopCounts.end() || !c.repeating) it = c.loopCounts.insert_or_assign(at, value(cmd.arg(0), t)).first;
            c.repeating = false;
            if (it->second-- <= 0) {
                c.loopCounts.erase(it);
                const Commands &cmds = *c.commands;
                while (c.pc < cmds.size() && !(cmds[c.pc].indent == indent && cmds[c.pc].code == 498)) c.pc++;
                if (c.pc < cmds.size()) c.pc++;
            }
            return true;
        }
        case 498: {  // loop end: back to the loop's start
            const Commands &cmds = *c.commands;
            size_t pc = c.pc - 1;
            while (pc > 0) {
                pc--;
                if (cmds[pc].indent == indent && (cmds[pc].code == 170 || cmds[pc].code == 179)) break;
            }
            c.pc = pc;
            c.repeating = true;
            return true;
        }
        case 171:    // break out of the innermost loop
        case 176: {  // go back to the start of the innermost loop
            const Commands &cmds = *c.commands;
            size_t pc = c.pc - 1;
            int depth = indent;
            while (pc > 0) {
                pc--;
                if (cmds[pc].indent < depth && (cmds[pc].code == 170 || cmds[pc].code == 179)) break;
                if (cmds[pc].indent < depth) depth = cmds[pc].indent;
            }
            int loopIndent = cmds[pc].indent;
            if (cmd.code == 176) {
                c.pc = pc;
                c.repeating = true;
                return true;
            }
            c.loopCounts.erase(pc);
            c.pc = pc + 1;
            while (c.pc < cmds.size() && !(cmds[c.pc].indent == loopIndent && cmds[c.pc].code == 498)) c.pc++;
            if (c.pc < cmds.size()) c.pc++;
            return true;
        }
        case 172:  // end this event; inside a called common event, only that one ends and its caller goes on
            c.pc = c.commands->size();
            return true;
        case 173:  // erase this event
            if (c.mapEvent >= 0 && c.map == map_) {
                events_[c.mapEvent].erased = true;
                refreshPages();
            }
            return true;
        case 180: {  // wait
            t.wait = value(cmd.arg(0), t);
            return t.wait <= 0;
        }

        case 213: {  // jump to label, whose name may hold text codes, as "Back\cself[90]"
            const Commands &cmds = *c.commands;
            const std::string name = expandText(cmd.str(0), &t);
            for (size_t i = 0; i < cmds.size(); i++) {
                if (cmds[i].code == 212 && cmds[i].str(0) == name) {
                    c.pc = i + 1;
                    return true;
                }
            }
            return true;
        }

        case 210: {  // call a common event, or a page of a map event
            int id = resolveRelativeCommon(value(cmd.arg(0), t), t);
            if (id >= 500000 && id < 600000) {
                callCommon(t, id - 500000, cmd, 1);
            } else {
                int page = value(cmd.arg(1), t);
                for (size_t i = 0; i < events_.size(); i++) {
                    auto &e = events_[i];
                    if (e.data->id != id) continue;
                    if (page >= 0 && page < int(e.data->pages.size()))
                        t.stack.push_back({&e.data->pages[page].commands, 0, -1, int(i), 0, map_, mapId_});
                    break;
                }
            }
            return true;
        }
        case 211: {  // run a common event after this event ends
            int id = resolveRelativeCommon(value(cmd.arg(0), t), t);
            if (id >= 500000) id -= 500000;
            if (const CommonEvent *ce = commonById(id)) {
                Thread r;
                r.stack.push_back({&ce->commands, 0, id, -1});
                threads_.push_back(std::move(r));
            }
            return true;
        }
        case 300: {
            int id = commonByName(cmd.str(0));
            if (id < 0) {
                log("no common event named " + cmd.str(0));
                return true;
            }
            callCommon(t, id, cmd, 1);
            return true;
        }

        case 220:  // save or load the game
        case 221:  // read a variable from a save
        case 222:  // write a variable into a save
            saveCommand(t, cmd);
            return true;

        case 130: {  // move a character, the hero by default, to a position or another map
            int target = cmd.arg(0);
            int x = value(cmd.arg(1), t), y = value(cmd.arg(2), t);
            int mapId = value(cmd.arg(3), t);
            if (cmd.arg(4) & 0x01) {  // precise coordinates, in half tiles
                x /= 2;
                y /= 2;
            }
            if (target == -1) {
                hero_.placeAt(x, y);
                for (auto &p : party_) p.placeAt(x, y);
                if (mapId >= 0 && mapId != mapId_) {
                    loadMap(mapId);
                    updateCamera();
                    // The event goes on running after the move, on the next frame.
                    return false;
                }
                updateCamera();
            } else if (Actor *a = actorFor(target == -2 ? -1 : target, t)) {
                a->placeAt(x, y);
            }
            return true;
        }

        case 201: {  // move route
            Actor *a = actorFor(cmd.arg(0), t);
            if (a && cmd.route) {
                a->route = cmd.route;
                a->routeStep = 0;
                a->routeWait = 0;
                a->routeFromCommand = true;
                // With "wait until done", the event waits for the route to finish.
                if (cmd.route->flags & 0x04) {
                    t.waitingRoute = a;
                    return false;
                }
            }
            return true;
        }
        case 202:  // wait until every move route has finished
            if (anyRouteRunning()) {
                c.pc--;
                return false;
            }
            return true;
        case 230:
            moveDuringEvents_ = true;
            return true;
        case 231:
            moveDuringEvents_ = false;
            return true;

        case 250:
            databaseCommand(t, cmd);
            return true;
        case 124:
            setVariablePlus(t, cmd);
            return true;
        case 123:
            return keyInput(t, cmd);
        case 125: {  // press keys automatically
            autoKeys_ |= uint32_t(cmd.arg(0));
            return true;
        }
        case 126:  // enable or disable standard keys
            keyControl_ = cmd.arg(0);
            return true;

        case 270:
            partyCommand(t, cmd);
            return true;
        case 290:
            effectCommand(t, cmd);
            return true;
        case 280: {  // shake the screen
            uint32_t f = uint32_t(cmd.arg(0));
            shake_.speed = ((f & 0xF0) >> 4) + 1;
            shake_.power = (f & 0x0F) + 1;
            shake_.type = (f >> 8) & 0xFF;
            shake_.frames = value(cmd.arg(1), t);
            return true;
        }
        case 281: {  // scroll the screen
            uint32_t f = uint32_t(cmd.arg(0));
            int type = f & 0x0F;
            if (type == 0) {
                bool pixels = (f >> 8) & 0x02;
                double tile = pixels ? 1 : gameDat_.tileSize();
                scrollX_ += value(cmd.arg(1), t) * tile;
                scrollY_ += value(cmd.arg(2), t) * tile;
            } else if (type == 1) {
                scrollX_ = scrollY_ = 0;
            } else {
                scrollLocked_ = type == 2;
            }
            return true;
        }

        case 160:  // transition settings
            transitionFrames_ = value(cmd.arg(1) & 0xFFFF, t);
            return true;
        case 161:  // freeze the screen until the transition runs
            transition_.prepared = true;
            transition_.generation++;
            return true;
        case 162: {  // run the transition from the frozen screen
            transition_.prepared = false;
            transition_.frames = std::max(1, transitionFrames_ ? transitionFrames_ : 30);
            transition_.elapsed = 0;
            return true;
        }
        case 177:
            haltNonPicture_ = true;
            return true;
        case 178:
            haltNonPicture_ = false;
            return true;
        case 174:  // back to the title
            pendingTitle_ = true;
            t.stack.clear();
            return false;
        case 175:  // end the game
            quitRequested_ = true;
            t.stack.clear();
            return false;
        case 105:  // close the message window
            message_.active = false;
            return true;
        case 140:
            soundCommand(t, cmd);
            return true;
        case 240:
        case 241:
        case 242:
            chipCommand(t, cmd);
            return true;
        case 251:  // CSV import and export of database data
        case 260:  // download a file
            unsupported(cmd);
            return true;

        default:
            unsupported(cmd);
            return true;
    }
}

int Game::commonByName(const std::string &name) const {
    for (auto &ce : commons_)
        if (ce.name == name) return ce.id;
    return -1;
}

void Game::callCommon(Thread &t, int id, const Command &cmd, int argBase) {
    if (id < 0 || id >= int(cself_.size())) return;
    const CommonEvent *ce = commonById(id);
    if (!ce) return;
    int flags = cmd.arg(argBase);
    int numbers = flags & 0x0F;
    int strings = (flags >> 4) & 0x0F;
    int refFlags = (flags >> 8) & 0xFF;
    bool wantsReturn = ((flags >> 24) & 0xFF) == 1;
    // Arguments are read in the caller's context before any reaches the callee, so a text code such as \cself[0] in a
    // string argument means the caller's variable, even when an event calls itself.
    numbers = std::min(numbers, 5);
    strings = std::min(strings, 5);
    std::array<int32_t, 5> nums{};
    std::array<std::string, 5> strs;
    for (int i = 0; i < numbers; i++) nums[i] = value(cmd.arg(argBase + 1 + i), t);
    int next = argBase + 1 + numbers;
    for (int i = 0; i < strings; i++)
        strs[i] = (refFlags & (1 << i)) ? readStr(cmd.arg(next++), t) : expandText(cmd.str(1 + i), &t);
    for (int i = 0; i < numbers; i++) cself_[id][i] = nums[i];
    for (int i = 0; i < strings; i++) cselfStr_[id][5 + i] = std::move(strs[i]);
    int32_t returnTo = wantsReturn ? cmd.arg(next) : 0;
    t.stack.push_back({&ce->commands, 0, id, -1, returnTo});
}

void Game::setVariable(Thread &t, const Command &c) {
    int32_t target = c.arg(0);
    int32_t r1 = c.arg(1), r2 = c.arg(2);
    uint32_t flags = uint32_t(c.arg(3));
    uint8_t b0 = flags & 0xFF, b1 = (flags >> 8) & 0xFF;
    int calc = b1 & 0xF0, assign = b1 & 0x0F;
    if (b0 & 0x10) target = readVar(target, t);
    int32_t a = (b0 & 0x04) ? r1 : value(r1, t);
    if (b0 & 0x20) a = readVar(readVar(r1, t), t);
    int32_t b = (b0 & 0x08) ? r2 : value(r2, t);
    if (b0 & 0x40) b = readVar(readVar(r2, t), t);
    int32_t rhs = a;
    switch (calc) {
        case 0x00: rhs = a + b; break;
        case 0x10: rhs = a - b; break;
        case 0x20: rhs = a * b; break;
        case 0x30: rhs = b ? a / b : 0; break;
        case 0x40: rhs = b ? a % b : 0; break;
        case 0x50: rhs = a & b; break;
        case 0x60: rhs = std::uniform_int_distribution<int>(std::min(a, b), std::max(a, b))(rng_); break;
        case 0xF0: rhs = int32_t(std::lround(std::atan2(double(a), double(b)) * 1800 / M_PI)); break;
    }
    // The multi-target flag (byte 2 bit 0) applies the result to the target and the next arg(4) variables.
    int32_t count = (flags >> 16) & 0x01 ? std::max(0, c.arg(4)) : 0;
    for (int32_t k = 0; k <= count; k++, target++) {
    // Right side 2 of exactly zero with addition is how the editor writes "assign the first value".
    int32_t cur = assign == 0 ? 0 : readVar(target, t);
    int32_t out = cur;
    switch (assign) {
        case 0x00: out = rhs; break;
        case 0x01: out = cur + rhs; break;
        case 0x02: out = cur - rhs; break;
        case 0x03: out = cur * rhs; break;
        case 0x04: out = rhs ? cur / rhs : cur; break;
        case 0x05: out = rhs ? cur % rhs : cur; break;
        case 0x06: out = std::max(cur, rhs); break;
        case 0x07: out = std::min(cur, rhs); break;
        case 0x08: out = std::abs(rhs); break;
        case 0x09: out = int32_t(std::lround(std::atan2(double(rhs), 1.0) * 1800 / M_PI)); break;
        case 0x0A: out = int32_t(std::lround(std::sin(rhs * M_PI / 1800) * 1000)); break;
        case 0x0B: out = int32_t(std::lround(std::cos(rhs * M_PI / 1800) * 1000)); break;
    }
    writeVar(target, out, t);
    }
}

// A message starting with "@n" shows face graphic n beside it; the code's line is not text.
void Game::showMessage(std::string text) {
    message_ = MessageState{};
    if (text.size() > 1 && text[0] == '@' && std::isdigit(static_cast<unsigned char>(text[1]))) {
        size_t end = 1;
        while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) end++;
        message_.face = std::atoi(text.c_str() + 1);
        if (end < text.size() && text[end] == '\r') end++;
        if (end < text.size() && text[end] == '\n') end++;
        text.erase(0, end);
    }
    message_.text = std::move(text);
    message_.total = visibleChars(message_.text);
    message_.active = true;
}

// The text at a variable address: a string variable's text, or a number written out.
std::string Game::textAt(int32_t address, const Thread &t) {
    if (isStringVar(address)) return readStr(address, t);
    if (address >= 1000000000) {
        const Database &db = address >= 1300000000 ? sdb_ : address >= 1100000000 ? cdb_ : udb_;
        int type = (address / 1000000) % 100, field = address % 100;
        if (type < int(db.types.size()) && db.types[type].isString(field)) return readStr(address, t);
    }
    return std::to_string(readVar(address, t));
}

namespace {

// Splits off the first line, without its line break.
std::pair<std::string, std::string> splitFirstLine(const std::string &s) {
    size_t nl = s.find('\n');
    if (nl == std::string::npos) return {s, ""};
    size_t end = nl > 0 && s[nl - 1] == '\r' ? nl - 1 : nl;
    return {s.substr(0, end), s.substr(nl + 1)};
}

size_t utf8Length(const std::string &s) {
    if (s.empty()) return 0;
    auto c = static_cast<unsigned char>(s[0]);
    size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    return std::min(n, s.size());
}

}  // namespace

void Game::setString(Thread &t, const Command &c) {
    int32_t target = c.arg(0);
    uint32_t flags = uint32_t(c.arg(1));
    int mode = flags & 0x0F;
    int assign = (flags >> 8) & 0x0F;
    if (flags & 0xF0) target = readVar(target, t);
    if (target < 1000000) target += 3000000;
    // The right side: typed text, the text at an address, or the text at an address held in a variable.
    int32_t source = 0;
    std::string rhs;
    switch (mode) {
        case 0: rhs = expandText(c.str(0), &t); break;
        case 1: source = c.arg(2); rhs = textAt(source, t); break;
        case 2: source = value(c.arg(2), t); rhs = textAt(source, t); break;
        default: unsupported(c); return;
    }
    std::string cur = readStr(target, t);
    switch (assign) {
        case 0x00: writeStr(target, rhs, t); break;
        case 0x01: writeStr(target, cur + rhs, t); break;
        case 0x02: writeStr(target, splitFirstLine(rhs).first, t); break;  // copy the first line
        case 0x03: {  // cut the first line out of the source
            auto [first, rest] = splitFirstLine(rhs);
            writeStr(target, first, t);
            if (source && isStringVar(source)) writeStr(source, rest, t);
            break;
        }
        case 0x04: {  // cut the first character out of the source
            size_t n = utf8Length(rhs);
            writeStr(target, rhs.substr(0, n), t);
            if (source && isStringVar(source)) writeStr(source, rhs.substr(n), t);
            break;
        }
        case 0x05: {  // read a text file of the game
            auto bytes = files_.read(rhs);
            std::string text = bytes ? std::string(bytes->begin(), bytes->end()) : "";
            writeStr(target, toUtf8(text, gameDat_.encoding), t);
            break;
        }
        case 0x07: {  // the files in a folder, one name a line; <<ERROR>> for a folder packed in an archive
            std::string list;
            if (auto names = files_.listFolder(rhs)) {
                for (auto &n : *names) list += (list.empty() ? "" : "\n") + n;
            } else {
                list = "<<ERROR>>";
            }
            writeStr(target, list, t);
            break;
        }
        case 0x08: {  // remove every occurrence
            if (!rhs.empty())
                for (size_t p; (p = cur.find(rhs)) != std::string::npos;) cur.erase(p, rhs.size());
            writeStr(target, cur, t);
            break;
        }
        case 0x09: {  // replace every occurrence of the first text with the second
            std::string from = mode == 0 ? expandText(c.str(0), &t) : rhs, to = expandText(c.str(1), &t);
            if (!from.empty()) {
                for (size_t p = 0; (p = cur.find(from, p)) != std::string::npos; p += to.size()) cur.replace(p, from.size(), to);
            }
            writeStr(target, cur, t);
            break;
        }
        default: unsupported(c); break;
    }
}

// Applies a picture command to one picture, or to a range of pictures when its multi-target flag is set.
void Game::pictureCommand(Thread &t, const Command &c) {
    uint32_t flags = uint32_t(c.arg(0));
    bool multi = (flags >> 24) & 0x01;
    int first = value(c.arg(1), t);
    int last = first;
    if (multi) last = value((flags & 0x0F) == 2 ? c.arg(5) : c.arg(14), t);
    for (int n = first; n <= std::max(first, last) && n - first < 10000; n++) pictureCommandOne(t, c, n);
}

void Game::pictureCommandOne(Thread &t, const Command &c, int number) {
    uint32_t flags = uint32_t(c.arg(0));
    // The flags hold, from the low byte up: the display type and the operation; the anchor and the blend mode (15
    // keeps the current one); the zoom type and the relative flag; and the multi-target, scroll link and free
    // position flags (WodiLib's PictureDrawBase).
    int display = flags & 0xF0;
    int exec = flags & 0x0F;
    int anchor = (flags >> 8) & 0xF0;
    int blend = (flags >> 8) & 0x0F;
    int zoomType = (flags >> 16) & 0xF0;
    bool relative = ((flags >> 16) & 0x0F) == 0x01;
    int frames = value(c.arg(2), t);

    if (exec == 2) {  // erase
        auto it = pictures_.find(number);
        if (it == pictures_.end()) return;
        if (frames <= 0) pictures_.erase(it);
        else {
            it->second.opacity.set(0, frames);
            it->second.erasing = true;
        }
        return;
    }
    if (exec == 3) return;  // reset delays: the runtime runs every picture command at once, so none wait

    bool show = exec == 0;
    if (!show && !pictures_.count(number)) return;  // moving a picture that isn't shown does nothing
    Picture &p = pictures_[number];
    double oldX = p.x.value(), oldY = p.y.value();
    // A field of -1,000,000 leaves the picture's current value as it is.
    constexpr int32_t keep = -1000000;
    auto field = [&](size_t i, int32_t fallback) {
        if (i >= c.args.size()) return fallback;
        int32_t v = value(c.arg(i), t);
        return v == keep ? fallback : v;
    };
    if (show) {
        Picture fresh;
        switch (display) {
            case 0x00: fresh.kind = PictureKind::File; fresh.source = c.str(0); break;
            case 0x10: fresh.kind = PictureKind::File; fresh.source = readStr(c.arg(11), t); break;
            case 0x20: fresh.kind = PictureKind::Text; fresh.source = expandText(c.str(0), &t); break;
            case 0x30: fresh.kind = PictureKind::Window; fresh.source = c.str(0); break;
            default: fresh.kind = PictureKind::Window; fresh.source = readStr(c.arg(11), t); break;
        }
        fresh.generation = ++generation_;
        if (fresh.kind == PictureKind::File) {
            if (auto bytes = files_.read(fresh.source)) imageSize(*bytes, fresh.imageW, fresh.imageH);
            fresh.divX = std::max(1, field(3, 1));
            fresh.divY = std::max(1, field(4, 1));
        } else if (fresh.kind == PictureKind::Window) {
            // A window picture's two division fields are its size, which is also the size it reports.
            fresh.windowW = fresh.imageW = field(3, 0);
            fresh.windowH = fresh.imageH = field(4, 0);
        } else {
            // The picture variable command reads a text picture's size, which the standard systems use to centre
            // text and to shrink names too wide for their column.
            if (!textMeasure_) textMeasure_ = std::make_shared<TextRenderer>(*this, nullptr);
            auto size = textMeasure_->measure(fresh.source, baseTextStyle(*this));
            fresh.imageW = size[0];
            fresh.imageH = size[1];
        }
        fresh.anchor = anchor >> 4;
        fresh.opacity.set(0, 0);
        fresh.zoomX.set(100, 0);
        fresh.zoomY.set(100, 0);
        for (Tween *tw : {&fresh.r, &fresh.g, &fresh.b}) tw->set(100, 0);
        p = fresh;
        p.x.set(field(7, 0) + (relative ? oldX : 0), 0);
        p.y.set(field(8, 0) + (relative ? oldY : 0), 0);
    } else {
        // A move sets no division or size: the standard systems pass 0 there.
        p.x.set(field(7, int32_t(p.x.to)) + (relative ? oldX : 0), frames);
        p.y.set(field(8, int32_t(p.y.to)) + (relative ? oldY : 0), frames);
    }
    if (blend != 0x0F) p.blend = blend;
    p.pattern = field(5, p.pattern);
    int over = show ? 0 : frames;
    p.opacity.set(field(6, int32_t(p.opacity.to)), frames);
    // Zoom type 0x00 gives one zoom for both directions, 0x40 keeps the current zoom, and the others give the width's
    // zoom here and the height's in field 18.
    if (zoomType == 0x00) {
        int zoom = field(9, int32_t(p.zoomX.to));
        p.zoomX.set(zoom, over);
        p.zoomY.set(zoom, over);
    } else if (zoomType != 0x40) {
        p.zoomX.set(field(9, int32_t(p.zoomX.to)), over);
        p.zoomY.set(field(18, int32_t(p.zoomY.to)), over);
    }
    p.angle.set(field(10, int32_t(p.angle.to)), over);
    // Field 12's top byte is 1 when the colour stays as it is.
    bool sameColor = c.args.size() > 12 && (uint32_t(c.arg(12)) >> 24) == 1;
    if (c.args.size() > 17 && !sameColor) {
        p.r.set(field(15, int32_t(p.r.to)), over);
        p.g.set(field(16, int32_t(p.g.to)), over);
        p.b.set(field(17, int32_t(p.b.to)), over);
    }
}

}  // namespace wolf

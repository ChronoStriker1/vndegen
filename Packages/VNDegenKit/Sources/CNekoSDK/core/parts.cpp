#include "parts.h"

#include <cstdio>

#include "engine.h"

namespace neko {

// Each game part's maker, defined in the part's own file. A set adds its declaration and its line in the list below.
std::unique_ptr<GamePart> makeSelfTestPart();

namespace {

struct PartEntry {
    const char *name;
    std::unique_ptr<GamePart> (*make)();
};

const PartEntry kParts[] = {
    {"selftest", makeSelfTestPart},
};

// A part for the runtime's own tests: `open selftest N` finishes at once with N, `open selftestwait N` after N presses
// with N, kept with a save, and chapter N is scr\cN.txt. Scene command 9000 loads its picture into slot p3 at p1, p2
// through PartHost::loadImage, additive when p4 is 1 and scrolling p5 and p6 pixels a second.
class SelfTestPart : public GamePart {
public:
    bool command(PartHost &host, const Node &n) override {
        if (n.opcode != 9000) return false;
        host.loadImage(n.p[3], n.s[1], n.p[1], n.p[2], n.p[4] == 1, float(n.p[5]), float(n.p[6]));
        return true;
    }
    std::vector<std::string> chapter(PartHost &, int number) override { return {"scr\\c" + std::to_string(number) + ".txt"}; }
    bool openScreen(PartHost &host, const std::string &name, const std::vector<int> &args) override {
        int count = args.empty() ? 0 : args[0];
        if (name == "selftest") {
            host.finishScreen(count);
            return true;
        }
        if (name != "selftestwait") return false;
        presses_ = 0;
        needed_ = count;
        return true;
    }
    bool press(PartHost &host, int, int, int) override {
        if (++presses_ >= needed_) host.finishScreen(presses_);
        return true;
    }
    void save(std::vector<std::string> &lines) const override { lines.push_back(std::to_string(presses_) + " " + std::to_string(needed_)); }
    void load(const std::vector<std::string> &lines) override {
        if (!lines.empty()) std::sscanf(lines[0].c_str(), "%d %d", &presses_, &needed_);
    }
    void reset() override { presses_ = needed_ = 0; }

private:
    int presses_ = 0, needed_ = 0;
};

}  // namespace

std::unique_ptr<GamePart> makeSelfTestPart() { return std::make_unique<SelfTestPart>(); }

std::unique_ptr<GamePart> makePart(const std::string &name) {
    for (const PartEntry &entry : kParts)
        if (name == entry.name) return entry.make();
    return nullptr;
}

ImageRef PartHost::image(const std::string &path) { return e_.images_->get(path); }
std::shared_ptr<const std::vector<uint8_t>> PartHost::read(const std::string &path) { return e_.files_.read(path); }
bool PartHost::exists(const std::string &path) { return e_.files_.exists(path); }

std::vector<std::vector<std::string>> PartHost::setting(const std::string &screen, const std::string &key) {
    std::vector<std::vector<std::string>> out;
    auto it = e_.screens_.find(screen);
    if (it == e_.screens_.end()) return out;
    for (auto [line, end] = it->second.settings.equal_range(key); line != end; ++line) out.push_back(line->second);
    return out;
}

int PartHost::variable(int scope, int number) {
    const auto &table = scope == 1 ? e_.scenario_ : e_.globals_;
    auto it = table.find(number);
    return it == table.end() ? 0 : it->second;
}

void PartHost::setVariable(int scope, int number, int value) {
    (scope == 1 ? e_.scenario_ : e_.globals_)[number] = value;
    if (scope != 1) e_.saveGlobals();
}

int64_t PartHost::nowMs() { return e_.now_; }
int PartHost::width() { return e_.width_; }
int PartHost::height() { return e_.height_; }
void PartHost::redraw() { e_.dirty_ = true; }

void PartHost::waitClick() { e_.waiting_ = Engine::Waiting::Click; }

void PartHost::waitMs(int ms) {
    e_.waiting_ = Engine::Waiting::Time;
    e_.waitUntil_ = e_.now_ + ms;
}

void PartHost::finishScreen(int result) { e_.finishPartScreen(result); }

void PartHost::playSound(int kind, int channel, const std::string &path, int volume, bool loop) {
    Node n;
    n.p[0] = channel;
    n.p[1] = volume;
    // A loop count of 0 repeats for ever, as the scripts' sound commands give it.
    n.p[4] = loop ? 0 : 1;
    n.s[1] = path;
    e_.playSound(n, kind);
}

void PartHost::log(const std::string &line) { e_.log("%s", line.c_str()); }

// MARK: The engine's side

// `parts NAME...` under `screen defaults` turns on those parts, made fresh for this game.
void Engine::loadParts() {
    parts_.clear();
    partScreen_ = nullptr;
    partHost_ = std::make_unique<PartHost>(*this);
    auto it = screens_.find("defaults");
    if (it == screens_.end()) return;
    for (auto [line, end] = it->second.settings.equal_range("parts"); line != end; ++line)
        for (const std::string &name : line->second) {
            auto part = makePart(name);
            if (part) parts_.push_back({name, std::move(part)});
            else complain("The table names a game part this build doesn't have: " + name);
        }
}

void Engine::resetParts() {
    partScreen_ = nullptr;
    for (auto &[name, part] : parts_) part->reset();
}

bool PartHost::loadImage(int slot, const std::string &path, int x, int y, bool additive, float scrollX, float scrollY) {
    return e_.loadPartImage(slot, path, x, y, additive, scrollX, scrollY);
}

bool Engine::partCommand(const Node &n) {
    for (auto &[name, part] : parts_)
        if (part->command(*partHost_, n)) return true;
    return false;
}

void Engine::drawParts(Canvas &out, Layer layer, int fromSlot, int toSlot) {
    for (auto &[name, part] : parts_) {
        if (layer == Layer::Screen && part.get() != partScreen_) continue;
        part->draw(*partHost_, out, layer, fromSlot, toSlot);
    }
}

void Engine::finishPartScreen(int result) {
    partScreen_ = nullptr;
    choiceResult_ = result;
    if (partResultScope_ >= 0) (partResultScope_ == 1 ? scenario_ : globals_)[partResultNumber_] = result;
    if (partResultScope_ == 0) saveGlobals();
    log("part screen gave %d", result);
    if (waiting_ == Waiting::PartScreen) waiting_ = Waiting::None;
    dirty_ = true;
}

}  // namespace neko

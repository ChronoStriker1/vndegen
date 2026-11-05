#include "save.h"

#include <zlib.h>

#include <cstring>

namespace neko {

namespace {
struct Patch {
    int offset;
    const char *hex;
};
}  // namespace

struct SaveLayout {
    int recordExtra;
    std::vector<Patch> beforeVariables, rootHead, spriteHead, spriteTail;
    int afterSize;
    int afterListAt;  // where the block holds a count and that many sprite numbers, or -1
    std::vector<Patch> after;
    int tailSize;
    std::vector<Patch> tail;
    int textTail = 16;  // the bytes of a text record after its strings
};

namespace {

#include "save_layouts.inc"

// Where the fields sit, from the end of a record's two path strings. A newer engine adds `recordExtra` bytes before them.
constexpr int kRecordX = 0x169, kRecordScale = 0x17D, kRecordAnchor = 0x185, kRecordAlpha = 0x18D, kRecordAnimated = 0x1B3;
constexpr int kRecordAnimationAt = 0x1B5, kRecordBlurOn = 0x191, kRecordBlur = 0x192;
constexpr size_t kSplashExtra = 35;
// In the tail after the scene's name: GOOD Timing's numbers, and then its two strings.
constexpr size_t kTailGtState = 819, kTailGtCount = 823, kTailGtStart = 831, kTailGtMarker = 835, kTailGtStrings = 839;
constexpr int kDateFlag = 360;

constexpr int kFlowHeader = 0xCD, kBeforeChoice = 4196, kAfterChoice = 39596, kDateBlock = 400;
constexpr int kVariables = 10000, kBeforeVariables = 39600 - 8, kAfterCount = 40;
constexpr int kChannels = 243;

void apply(const std::vector<Patch> &patches, uint8_t *to) {
    auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (const Patch &p : patches) {
        uint8_t *d = to + p.offset;
        for (const char *h = p.hex; h[0] && h[1]; h += 2) *d++ = uint8_t(nibble(h[0]) * 16 + nibble(h[1]));
    }
}

class Reader {
public:
    explicit Reader(const std::vector<uint8_t> &b) : b_(b) {}
    bool ok = true;
    size_t at = 0;
    int i32() {
        if (at + 4 > b_.size()) { ok = false; return 0; }
        int32_t v;
        std::memcpy(&v, &b_[at], 4);
        at += 4;
        return v;
    }
    float f32At(size_t p) const {
        float v = 0;
        if (p + 4 <= b_.size()) std::memcpy(&v, &b_[p], 4);
        return v;
    }
    int i32At(size_t p) const {
        int32_t v = 0;
        if (p + 4 <= b_.size()) std::memcpy(&v, &b_[p], 4);
        return v;
    }
    uint8_t u8() {
        if (at >= b_.size()) { ok = false; return 0; }
        return b_[at++];
    }
    double f64() {
        double v = 0;
        if (at + 8 > b_.size()) { ok = false; return 0; }
        std::memcpy(&v, &b_[at], 8);
        at += 8;
        return v;
    }
    std::string str() {
        int n = i32();
        if (n < 0 || at + size_t(n) > b_.size()) { ok = false; return ""; }
        std::string s(reinterpret_cast<const char *>(&b_[at]), size_t(n));
        at += size_t(n);
        return s;
    }
    void skip(size_t n) {
        at += n;
        if (at > b_.size()) ok = false;
    }

private:
    const std::vector<uint8_t> &b_;
};

class Writer {
public:
    std::vector<uint8_t> b;
    size_t zeros(size_t n) {
        size_t at = b.size();
        b.resize(at + n, 0);
        return at;
    }
    void u8(uint8_t v) { b.push_back(v); }
    void i32(int32_t v) { std::memcpy(&b[zeros(4)], &v, 4); }
    void f32(float v) { std::memcpy(&b[zeros(4)], &v, 4); }
    void f64(double v) { std::memcpy(&b[zeros(8)], &v, 8); }
    void str(const std::string &s) {
        i32(int32_t(s.size()));
        b.insert(b.end(), s.begin(), s.end());
    }
    void setI32(size_t at, int32_t v) { std::memcpy(&b[at], &v, 4); }
    void setF32(size_t at, float v) { std::memcpy(&b[at], &v, 4); }
};

void readText(Reader &r, SavedText &t, int trail) {
    t.tag = r.i32();
    t.name = r.str();
    t.text = r.str();
    t.voice = r.str();
    t.volume = r.i32();
    r.skip(4);
    t.kind = r.i32();
    r.skip(size_t(trail - 12));
}

void writeText(Writer &w, const SavedText &t, int trail) {
    w.i32(t.tag);
    w.str(t.name);
    w.str(t.text);
    w.str(t.voice);
    w.i32(t.volume);
    w.i32(0);
    w.i32(t.kind);
    if (trail > 12) w.zeros(size_t(trail - 12));
}

}  // namespace

const SaveLayout *saveLayout(const std::string &folder) {
    if (folder == "waffle\\x2019_retry") return &kChristmas;
    if (folder == "waffle\\alchemy") return &kAlchemist;
    if (folder == "waffle\\mother3") return &kMother3;
    if (folder == "waffle\\tmgb") return &kTsumaGibo;
    return nullptr;
}

bool parseSave(const std::vector<uint8_t> &plain, const SaveLayout &layout, SaveData &out) {
    if (plain.size() < 100000) return false;
    Reader r(plain);
    r.skip(kFlowHeader);
    out.flow = r.str();
    r.skip(kBeforeChoice);
    out.choiceImage = r.str();
    r.skip(kAfterChoice);
    for (int &d : out.date) d = r.i32();
    out.dateFlag = r.i32At(r.at + kDateFlag - 24);
    r.skip(kDateBlock - 24);
    out.flowActive = r.i32();
    out.flowNode = r.i32();
    r.skip(kBeforeVariables);
    out.variables.clear();
    for (int i = 0; i < kVariables; i++) {
        int v = r.i32();
        if (v) out.variables[i] = v;
    }
    int count = r.i32();
    r.skip(kAfterCount);
    if (!r.ok || count < 0 || count > 2000) return false;
    out.sprites.clear();
    int extra = layout.recordExtra;
    for (int k = 0; k < count && r.ok; k++) {
        SavedSprite s;
        r.skip(1);
        bool ordinary = r.u8() != 0;
        s.slot = r.i32();
        int number = r.i32();
        if (!ordinary) s.flipFrame = number;
        s.path = r.str();
        s.nextPath = r.str();
        size_t f = r.at;
        // The whole fixed part, so one cut short is caught here and not read past the end below.
        if (!r.ok || f + kRecordAnimationAt + extra + 4 > plain.size()) {
            r.ok = false;
            break;
        }
        s.x = r.f32At(f + kRecordX + extra);
        s.y = r.f32At(f + kRecordX + 4 + extra);
        s.scaleX = r.f32At(f + kRecordScale + extra);
        s.scaleY = r.f32At(f + kRecordScale + 4 + extra);
        s.anchorX = r.i32At(f + kRecordAnchor + extra);
        s.anchorY = r.i32At(f + kRecordAnchor + 4 + extra);
        s.alpha = r.i32At(f + kRecordAlpha + extra);
        s.blurOn = plain[f + kRecordBlurOn + extra] != 0;
        s.blur = r.i32At(f + kRecordBlur + extra);
        s.animated = f + kRecordAnimated + extra < plain.size() && plain[f + kRecordAnimated + extra] != 0;
        r.at = f + kRecordAnimationAt + extra;
        s.animationFrames = r.i32();
        s.animation = r.str();
        size_t g = r.at;
        s.endX = r.f32At(g + 4);
        s.endY = r.f32At(g + 8);
        s.endAlpha = r.i32At(g + 12);
        s.endScaleX = r.f32At(g + 20);
        s.endScaleY = r.f32At(g + 24);
        r.at = g + 0x261 - 0x1BD;
        // A splash effect's record has no picture, and 35 more bytes that describe it. VNDegen starts splashes afresh.
        if (s.path.empty() && s.slot != -999999) r.skip(kSplashExtra);
        else if (s.slot != -999999) out.sprites.push_back(s);
        else {
            out.width = r.i32At(f + 0x11);
            out.height = r.i32At(f + 0x15);
        }
    }
    size_t afterStart = r.at;
    r.skip(size_t(layout.afterSize));
    if (layout.afterListAt >= 0) {
        int listed = r.i32At(afterStart + size_t(layout.afterListAt));
        if (listed < 0 || listed > 2000) return false;
        r.skip(size_t(listed) * 4);
    }
    if (!r.ok) return false;
    out.windowShown = r.u8() != 0;
    r.u8();
    readText(r, out.current, layout.textTail);
    r.skip(0x18);
    r.skip(4);
    int logged = r.i32();
    if (!r.ok || logged < 0 || logged > 100000) return false;
    out.backlog.clear();
    for (int i = 0; i < logged && r.ok; i++) {
        SavedText t;
        readText(r, t, layout.textTail);
        out.backlog.push_back(t);
    }
    r.skip(1);
    out.sounds.clear();
    for (int i = 0; i < kChannels && r.ok; i++) {
        SavedSound s;
        s.index = i;
        s.playing = r.u8() != 0;
        s.path = r.str();
        s.volume = r.i32();
        s.pan = r.i32();
        s.seconds = r.f64();
        r.skip(2);
        if (!s.path.empty()) out.sounds.push_back(s);
    }
    r.skip(16);
    out.scene = r.str();
    size_t tail = r.at;
    out.node = r.i32At(tail);
    out.gtState = r.i32At(tail + kTailGtState);
    out.gtCount = r.i32At(tail + kTailGtCount);
    out.gtStartNode = r.i32At(tail + kTailGtStart);
    out.gtMarker = r.i32At(tail + kTailGtMarker);
    r.at = tail + kTailGtStrings;
    out.gtScene = r.str();
    out.gtConfig = r.str();
    return r.ok;
}

std::vector<uint8_t> buildSave(const SaveData &d, const SaveLayout &layout) {
    Writer w;
    w.zeros(kFlowHeader);
    w.b[0] = 1;
    w.str(d.flow);
    w.zeros(kBeforeChoice);
    w.str(d.choiceImage);
    w.zeros(kAfterChoice);
    for (int v : d.date) w.i32(v);
    size_t dateRest = w.zeros(kDateBlock - 24);
    w.setI32(dateRest + kDateFlag - 24, d.dateFlag);
    w.i32(d.flowActive);
    w.i32(d.flowNode);
    size_t before = w.zeros(kBeforeVariables);
    apply(layout.beforeVariables, &w.b[before]);
    size_t table = w.zeros(size_t(kVariables) * 4);
    for (const auto &[index, value] : d.variables)
        if (index >= 0 && index < kVariables) w.setI32(table + size_t(index) * 4, value);
    w.i32(int32_t(d.sprites.size()) + 1);
    w.zeros(kAfterCount);

    int extra = layout.recordExtra;
    auto record = [&](const SavedSprite &s, bool root, bool hasNext) {
        w.u8(1);
        w.u8(s.flipFrame >= 0 ? 0 : 1);
        w.i32(root ? -999999 : s.slot);
        w.i32(root ? 3 : s.flipFrame >= 0 ? s.flipFrame : 0);
        w.str(s.path);
        w.str(s.nextPath);
        size_t f = w.zeros(size_t(kRecordAnimationAt) + extra);
        apply(root ? layout.rootHead : layout.spriteHead, &w.b[f]);
        if (root) {
            w.setI32(f + 0x11, d.width);
            w.setI32(f + 0x15, d.height);
        }
        w.setF32(f + kRecordX + extra, s.x);
        w.setF32(f + kRecordX + 4 + extra, s.y);
        w.setF32(f + kRecordScale + extra, s.scaleX);
        w.setF32(f + kRecordScale + 4 + extra, s.scaleY);
        w.setI32(f + kRecordAnchor + extra, s.anchorX);
        w.setI32(f + kRecordAnchor + 4 + extra, s.anchorY);
        w.setI32(f + kRecordAlpha + extra, s.alpha);
        w.b[f + kRecordBlurOn + extra] = s.blurOn ? 1 : 0;
        w.setI32(f + kRecordBlur + extra, s.blur);
        w.b[f + kRecordAnimated + extra] = s.animated ? 1 : 0;
        w.i32(s.animationFrames);
        w.str(s.animation);
        size_t g = w.zeros(0x261 - 0x1BD);
        apply(layout.spriteTail, &w.b[g]);
        w.setI32(g, root ? 0 : s.slot);
        w.setF32(g + 4, s.endX);
        w.setF32(g + 8, s.endY);
        w.setI32(g + 12, s.endAlpha);
        w.setI32(g + 16, s.blur);
        // A third copy of where the sprite started.
        w.setF32(g + 0x3C, s.x);
        w.setF32(g + 0x40, s.y);
        w.setF32(g + 20, s.endScaleX);
        w.setF32(g + 24, s.endScaleY);
        w.setI32(g + 28, s.anchorX);
        w.setI32(g + 32, s.anchorY);
        if (!layout.recordExtra) w.setI32(g + 0x261 - 0x1BD - 4, hasNext ? 1 : 0);
    };
    SavedSprite rootSprite;
    rootSprite.alpha = rootSprite.endAlpha = 256;
    record(rootSprite, true, false);
    for (size_t i = 0; i < d.sprites.size(); i++) record(d.sprites[i], false, i + 1 < d.sprites.size());

    size_t after = w.zeros(size_t(layout.afterSize));
    apply(layout.after, &w.b[after]);
    w.u8(d.windowShown ? 1 : 0);
    w.u8(d.windowShown ? 1 : 0);
    writeText(w, d.current, layout.textTail);
    w.zeros(0x18);
    w.i32(60);
    w.i32(int32_t(d.backlog.size()));
    for (const SavedText &t : d.backlog) writeText(w, t, layout.textTail);

    w.zeros(1);
    std::map<int, const SavedSound *> byIndex;
    for (const SavedSound &s : d.sounds) byIndex[s.index] = &s;
    for (int i = 0; i < kChannels; i++) {
        auto it = byIndex.find(i);
        if (it == byIndex.end()) {
            w.u8(0);
            w.str("");
            w.zeros(18);
            continue;
        }
        const SavedSound &s = *it->second;
        w.u8(s.playing ? 1 : 0);
        w.str(s.path);
        w.i32(s.volume);
        w.i32(s.pan);
        w.f64(s.seconds);
        w.zeros(2);
    }
    w.zeros(16);
    w.str(d.scene);
    size_t tail = w.zeros(size_t(layout.tailSize));
    apply(layout.tail, &w.b[tail]);
    w.setI32(tail, d.node);
    w.setI32(tail + kTailGtState, d.gtState);
    w.setI32(tail + kTailGtCount, d.gtCount);
    w.setI32(tail + kTailGtCount + 4, d.gtCount);
    w.setI32(tail + kTailGtStart, d.gtStartNode);
    w.setI32(tail + kTailGtMarker, d.gtMarker);
    // The tail's two strings are empty in the layouts, so their text goes in place of those two lengths.
    Writer strings;
    strings.str(d.gtScene);
    strings.str(d.gtConfig);
    auto at = w.b.begin() + std::ptrdiff_t(tail + kTailGtStrings);
    w.b.erase(at, at + 8);
    w.b.insert(w.b.begin() + std::ptrdiff_t(tail + kTailGtStrings), strings.b.begin(), strings.b.end());
    return w.b;
}

std::vector<uint8_t> packEntry(const std::vector<uint8_t> &plain) {
    uLongf bound = compressBound(uLong(plain.size()));
    std::vector<uint8_t> out(bound + 4);
    if (compress2(out.data(), &bound, plain.data(), uLong(plain.size()), Z_BEST_COMPRESSION) != Z_OK) return {};
    out.resize(bound + 4);
    uint32_t size = uint32_t(plain.size());
    std::memcpy(&out[bound], &size, 4);
    uint32_t key = uint32_t(out.size() / 8 + 0x22);
    for (int i = 0; i < 4; i++) {
        out[size_t(i)] ^= uint8_t(key);
        key <<= 3;
    }
    return out;
}

}  // namespace neko

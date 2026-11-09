#include "save.h"

#include <zlib.h>

#include <algorithm>
#include <cmath>
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
    int rootSizeAt = 0x11;  // where the first record holds the screen size
    int channels = 243;  // the sound channels saved
    // A sprite record's last block, after its animation's text: 164 bytes, or 137 in Test Game 90.
    int recordTail = 0x261 - 0x1BD;
    // The newer engine's byte before the sound channels, which Test Game 90's saves don't have.
    bool byteBeforeSounds = true;
    // The GOOD Timing fields after the scene's node (SaveData::gtState and on), which Test Game 90's tail doesn't have:
    // its fixed bytes sit 31 earlier than Test Game 94's, and it keeps zeros where an off GOOD Timing keeps -1.
    bool gtTail = true;
    // What a shown sprite's alpha and end alpha hold: 256, or 255 in Test Game 90's records, as the older engine's hold;
    // its first record keeps 256 either way.
    int shownAlpha = 256;
    // The older engine of the games that keep their story in the exe: a sprite record's fixed part, 0 in the newer
    // layouts, then the zeros before each record, the first's within the 40 bytes after the count, and those between the
    // sound channels and the scene's name.
    int olderRecord = 0, olderRecordGap = 0, olderSoundGap = 0;
    // Test Game 87 names a choice's picture in the older engine's first place, 236 bytes after the flow, and leaves the
    // newer place empty, at a choice and after it; its PC shows a choice with no options from a save that has it the
    // other way round (set 1, under Wine). Test Games 82 and 83 name it in the newer place.
    bool choiceInFirstPlace = false;
    ChoiceFlag choiceFlag = ChoiceFlag::Picture;
    // Test Games 82 and 83's PC writes a Previous Choice file, `saveNNNs.sav`, beside each save, and reads it with the
    // save: the state at the last choice, which Return to Choice goes back to (sets 2 and 3).
    bool previousChoice = false;
    // What 116 bytes into the block before the variables holds (SaveData::choiceColumns): Test Game 82's choice sheet's
    // columns, or in Test Game 83 the option taken, and the count while a choice waits (sets 2 and 3).
    bool choiceResultAt116 = false;
    // The number global 0 gives the first card, the one NEW marks after a save to it: 1 in Test Game 94 and The Alchemist,
    // whose cfg.sav held 2 and 1 for their second and first cards, and 0 in Test Game 87, which held 2 after a save to
    // its third and crashed opening a Load page whose global 0 was one past its last card (set 1, under Wine). Test
    // Games 82 and 83 count from 0 too, each 2 after a save to its third card, which NEW marked (sets 2 and 3), so the
    // older engine's layouts all do.
    int firstCard = 1;
    // Test Game 96's saves name a second picture right after the choice's, where the other newer layouts have 4 zeros
    // (an empty name), and every PC save of it names the same two: these, which VNDegen writes when it has none (set 2's
    // 18 saves, lead).
    const char *choiceName = nullptr, *afterChoiceName = nullptr;
    // Bytes of the date block after the date that hold the same every time, from its start.
    std::vector<Patch> dateBlock;
};

namespace {

#include "save_layouts.inc"

// The older engine's layout, from PC saves made under Wine: Test Game 83's seven (set 3) and Test Game 87's 21 (set 1),
// every region checked in each. A save names a picture 236 bytes after the flow and another where the newer engine names
// the choice's, 4,196 bytes after the flow, with all that follows moved by the first name's length; which one holds the
// choice's picture is per game. A sprite record's fixed part is 383 bytes, with x and y as ints, and zeros come
// before each record, the first's within the 40 bytes after the record count, as a save with no records shows. Text records end 8 bytes after their strings, as Test Game 94's do. Sound channels come with no byte
// before them, each ending a byte after its time, the first 30 for music and the rest for effects. How many channels
// there are and how many zeros follow them can't be told apart in these saves, whose last channels are all empty; the
// most channels that fit is taken.
SaveLayout older(int recordGap, int afterSize, std::vector<Patch> after, int tailSize, std::vector<Patch> tail, int channels) {
    SaveLayout l{};
    l.recordExtra = 0;
    l.firstCard = 0;
    l.rootHead = {{12, "ff"}, {14, "05"}, {17, "d002"}, {107, "803f"}, {127, "803f"}, {147, "803f"}, {167, "803f"}, {235, "803f"}, {239, "803f"}, {250, "01"}, {306, "01"}, {315, "803f"}, {319, "803f"}};
    l.spriteHead = {{107, "803f"}, {127, "803f"}, {147, "803f"}, {167, "803f"}, {235, "803f"}, {239, "803f"}, {249, "ff"}, {305, "ff"}, {315, "803f"}, {319, "803f"}};
    l.afterSize = afterSize;
    l.afterListAt = -1;
    l.after = std::move(after);
    l.tailSize = tailSize;
    l.tail = std::move(tail);
    l.textTail = 8;
    l.rootSizeAt = 13;
    l.channels = channels;
    l.olderRecord = 383;
    l.olderRecordGap = recordGap;
    l.olderSoundGap = 19;
    return l;
}

const SaveLayout kAniMother = kTsumaGibo;
// Test Game 90's, from set 1's 12 PC saves (lead): Test Game 94's, with sprite records 8 bytes shorter and their last
// block 137 bytes, no byte before the sound channels, and a tail 6,063 bytes long with no GOOD Timing fields. Each save
// parses and is built again byte for byte.
const SaveLayout kMankitsu = [] {
    SaveLayout l = kRinjin;
    l.recordExtra = -148;
    l.recordTail = 137;
    // Its sprites' last blocks hold none of Test Game 94's fixed bytes, and a shown sprite holds 255 (set 1: with 256 and
    // 94's bytes the PC drew a CG of VNDegen's save as nothing).
    l.spriteTail = {};
    l.shownAlpha = 255;
    // And the fixed bytes of its records' heads sit where its 12 PC saves have them, 8 earlier than 94's past the screen
    // size: 94's places left four 1.0s where 90's PC reads 0, and it drew the CG of VNDegen's save as nothing (set 1).
    l.rootHead = {{12, "ff00050000d002"}, {107, "803f"}, {127, "803f"}, {147, "803f"}, {167, "803f"}, {235, "803f0000803f"}, {250, "01"}};
    l.spriteHead = {{107, "803f"}, {127, "803f"}, {147, "803f"}, {167, "803f"}};
    l.byteBeforeSounds = false;
    l.gtTail = false;
    l.tailSize = 6063;
    l.tail = {{862, "ffffffff"}, {932, "ffffffff"}, {1006, "3c"}};
    l.firstCard = 0;
    return l;
}();
// Test Game 96's, from set 2's 18 PC saves (lead): the older engine's records, 396 bytes each after their names with 26
// zeros before each, and the older text records, sound channels and tail, with the newer place for the choice's picture
// and a second picture's name after it. 154 channels and 14 zeros before the scene's name are the most that fit; the tail
// is 819 bytes with the node. The date block holds 1 at 240 and 0 at the flag in every save.
const SaveLayout kYabai = [] {
    SaveLayout l = older(26, 0, {}, 819, {}, 154);
    l.olderRecord = 396;
    l.olderSoundGap = 14;
    l.choiceFlag = ChoiceFlag::Never;
    l.choiceName = "obj\\a003_001.bmp";
    l.afterChoiceName = "obj\\t000_000.bmp";
    l.dateBlock = {{240, "01"}};
    return l;
}();
const SaveLayout kFunbag2 = [] {
    SaveLayout l = older(26, 0, {}, 818, {}, 123);
    l.choiceFlag = ChoiceFlag::Never;
    l.previousChoice = true;
    l.choiceResultAt116 = true;
    return l;
}();
// Test Game 82's 32 PC saves (set 2), half of them Previous Choice files, are Test Game 83's layout with sprite records
// 4 bytes shorter and the zeros before each 4 bytes longer.
const SaveLayout kFunbag1 = [] {
    SaveLayout l = older(30, 0, {}, 818, {}, 123);
    l.olderRecord = 379;
    l.choiceFlag = ChoiceFlag::Never;
    l.previousChoice = true;
    return l;
}();
const SaveLayout kSideboob2 = [] {
    SaveLayout l = older(39, 33, {{5, "64"}}, 5779, {{903, "3c"}}, 183);
    l.choiceInFirstPlace = true;
    l.choiceFlag = ChoiceFlag::Made;
    return l;
}();

// Where the fields sit, from the end of a record's two path strings. A newer engine adds `recordExtra` bytes before them.
constexpr int kRecordX = 0x169, kRecordScale = 0x17D, kRecordAnchor = 0x185, kRecordAlpha = 0x18D, kRecordAnimated = 0x1B3;
constexpr int kRecordAnimationAt = 0x1B5, kRecordBlurOn = 0x191, kRecordBlur = 0x192, kRecordWave = 0x1A6;
// A splash record names its droplet's picture this far into its fixed part, before the engine's extra bytes: an empty
// name in every other record. Only Test Game 88 has splashes, so it's checked in that layout alone.
constexpr int kSplashNameAt = 201;
// The bytes of a record after its animation text, and a flipbook's fields among them: whether it runs backward and
// repeats (parameters 1 and 14), two bytes of 1 while it plays, parameters 15 and 16, and a byte of 0 where every other
// record has 100.
constexpr size_t kTailFlipReverse = 97, kTailFlipRepeat = 101, kTailFlipPlaying = 102, kTailFlipParam15 = 104,
                 kTailFlipStep = 108, kTailFlipHundred = 120;
constexpr int kNoiseKind = 4;
// In the tail after the scene's name: GOOD Timing's numbers, and then its two strings.
constexpr size_t kTailGtState = 819, kTailGtCount = 823, kTailGtStart = 831, kTailGtMarker = 835, kTailGtStrings = 839;
constexpr int kDateFlag = 360;
// The older engine's first picture name, after the flow; its record's fields from the end of the two path strings; and its
// sound channels for music, before the effects'.
constexpr int kOlderBeforeChoice = 236;
constexpr int kOlderX = 0xD5, kOlderScale = 0xE9, kOlderAlpha = 0xF9, kOlderEndX = 0x129, kOlderEndAlpha = 0x131;
constexpr int kOlderEndScale = 0x139, kOlderThirdX = 0x161, kOlderMusicChannels = 30;
// A named animation's text, command 19's, is a string 0x125 into an older record's fixed part, empty unless one runs, and
// the fields after it move by its length (Test Game 87's saves at a standing picture bouncing, set 1).
constexpr int kOlderAnimation = 0x125;
// Where an older record names a part's three pictures (SavedSprite::names), each a length and its bytes: in Test Game
// 83's saves, the only ones seen with such a record.
constexpr size_t kOlderNames = 57;

// A save opens with a byte of 1 and the card's comment, then this many bytes before the flow script's name: a save
// with no comment names its flow at 0xCD.
constexpr int kAfterComment = 200;
constexpr int kBeforeChoice = 4196, kAfterChoice = 39596, kDateBlock = 400;
constexpr int kVariables = kSavedVariables, kBeforeVariables = 39600 - 8, kAfterCount = 40;
// The latest choice's columns and cells in the block before the variables (SaveData::choiceColumns), up to 8 cells read.
constexpr size_t kBeforeChoiceColumns = 116, kBeforeChoiceCells = 152, kBeforeChoiceCellsMax = 8;

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
    t.pan = r.i32();
    // Test Game 94's records end there, 8 bytes after the strings.
    if (trail < 12) return;
    t.kind = r.i32();
    r.skip(size_t(trail - 12));
}

void writeText(Writer &w, const SavedText &t, int trail) {
    w.i32(t.tag);
    w.str(t.name);
    w.str(t.text);
    w.str(t.voice);
    w.i32(t.volume);
    w.i32(t.pan);
    if (trail < 12) return;
    w.i32(t.kind);
    w.zeros(size_t(trail - 12));
}

}  // namespace

std::vector<uint8_t> withComment(const std::vector<uint8_t> &plain, std::string comment) {
    if (plain.size() < 5) return {};
    uint32_t old;
    std::memcpy(&old, &plain[1], 4);
    if (old > plain.size() - 5) return {};
    // Test Game 94's box stopped at 36 bytes, 36 letters or 18 kana, under Wine. A character isn't split.
    size_t keep = 0;
    for (size_t i = 0; i < comment.size();) {
        unsigned char c = static_cast<unsigned char>(comment[i]);
        size_t width = (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC) ? 2 : 1;
        if (i + width > 36 || i + width > comment.size()) break;
        i += width;
        keep = i;
    }
    comment.resize(keep);
    std::vector<uint8_t> out(plain.begin(), plain.begin() + 1);
    const uint32_t length = uint32_t(comment.size());
    out.insert(out.end(), reinterpret_cast<const uint8_t *>(&length), reinterpret_cast<const uint8_t *>(&length) + 4);
    out.insert(out.end(), comment.begin(), comment.end());
    out.insert(out.end(), plain.begin() + 5 + old, plain.end());
    return out;
}

const SaveLayout *saveLayout(const std::string &folder) {
    if (folder == "waffle\\x2019_retry") return &kChristmas;
    if (folder == "waffle\\alchemy") return &kAlchemist;
    if (folder == "waffle\\mother3") return &kMother3;
    if (folder == "waffle\\tmgb") return &kTsumaGibo;
    if (folder == "waffle\\funbagfantasy3") return &kFunbagFantasy;
    if (folder == "waffle\\funbag4") return &kFunbag4;
    if (folder == "waffle\\moralorc") return &kMoralOrc;
    // Test Game 92 saves as Ore o Hoshigaru Tsuma to Gibo does: its PC save reads with that layout.
    if (folder == "waffle\\animother") return &kAniMother;
    if (folder == "waffle\\隣人に壊されていく俺の妻") return &kRinjin;
    // The runtime's own story-table test game saves as The Alchemist does.
    if (folder == "vndegen\\storytest" || folder == "vndegen\\storytestload") return &kAlchemist;
    // And so do the save card and confession tree test games.
    if (folder == "vndegen\\cardtest" || folder == "vndegen\\treetest") return &kAlchemist;
    // And as Test Game 82 does, with a Previous Choice file beside each save.
    if (folder == "vndegen\\storytestpc") return &kFunbag1;
    if (folder == "waffle\\funbagfantasy") return &kFunbag1;
    if (folder == "waffle\\funbagfantasy2") return &kFunbag2;
    // The older layout's test game saves as Test Game 83 does, under a folder of its own so it gets no menu table.
    if (folder == "vndegen\\layoutold" || folder == "vndegen\\partslot") return &kFunbag2;
    if (folder == "waffle\\funbag_fantasy_sideboob_story_2") return &kSideboob2;
    // Test Game 96, and its layout's test game under a folder of its own, with no menu table.
    if (folder == "waffle\\ヤバい！－復讐・闇サイト－" || folder == "vndegen\\layout96") return &kYabai;
    // The vote's test game saves as Test Game 96 does, and the newer layout's test game as Test Game 94 does.
    if (folder == "vndegen\\votetest") return &kYabai;
    if (folder == "vndegen\\layout94") return &kRinjin;
    // Test Game 90, and its layout's test game under a folder of its own, with no menu table.
    if (folder == "waffle\\漫喫ハプニング" || folder == "vndegen\\layout90") return &kMankitsu;
    return nullptr;
}

bool namesPictureTwice(const SaveLayout &layout) {
    // The PC's own saves name it twice in the Christmas game, The Alchemist, Test Games 85, 88 and 93; Test Games 84, 89,
    // 92 and 94's and the older engine's leave it empty. Test Game 94's PC, given a save of the runtime's with the name
    // twice, lost the picture and went on elsewhere, and with it once came back as from its own save there (under Wine).
    for (const SaveLayout *twice : {&kChristmas, &kAlchemist, &kMother3, &kTsumaGibo, &kFunbag4})
        if (&layout == twice) return true;
    return false;
}

ChoiceFlag choiceFlag(const SaveLayout &layout) { return layout.choiceFlag; }
bool writesPreviousChoice(const SaveLayout &layout) { return layout.previousChoice; }
bool olderEngine(const SaveLayout &layout) { return layout.olderRecord > 0; }
bool choiceResultAt116(const SaveLayout &layout) { return layout.choiceResultAt116; }
int newCardNumber(const SaveLayout *layout, int slot) { return slot - 1 + (layout ? layout->firstCard : 1); }
int newCardSlot(const SaveLayout *layout, int number) { return number + 1 - (layout ? layout->firstCard : 1); }

bool parseSave(const std::vector<uint8_t> &plain, const SaveLayout &layout, SaveData &out) {
    if (plain.size() < 100000) return false;
    Reader r(plain);
    out.fromFile = true;
    r.skip(1);
    out.comment = r.str();
    r.skip(kAfterComment);
    out.flow = r.str();
    bool older = layout.olderRecord > 0;
    if (older) {
        r.skip(kOlderBeforeChoice);
        (layout.choiceInFirstPlace ? out.choiceImage : out.olderImage) = r.str();
        r.skip(kBeforeChoice - kOlderBeforeChoice - 4);
        (layout.choiceInFirstPlace ? out.olderImage : out.choiceImage) = r.str();
    } else {
        r.skip(kBeforeChoice);
        out.choiceImage = r.str();
    }
    if (layout.afterChoiceName) {
        out.afterChoiceImage = r.str();
        r.skip(kAfterChoice - 4);
    } else {
        r.skip(kAfterChoice);
    }
    for (int &d : out.date) d = r.i32();
    out.dateFlag = r.i32At(r.at + kDateFlag - 24);
    r.skip(kDateBlock - 24);
    out.flowActive = r.i32();
    out.flowNode = r.i32();
    if (r.at + kBeforeVariables <= plain.size()) {
        out.beforeVariables.assign(plain.begin() + std::ptrdiff_t(r.at), plain.begin() + std::ptrdiff_t(r.at + kBeforeVariables));
        auto i32 = [&](size_t at) { return int32_t(uint32_t(plain[r.at + at]) | uint32_t(plain[r.at + at + 1]) << 8 | uint32_t(plain[r.at + at + 2]) << 16 | uint32_t(plain[r.at + at + 3]) << 24); };
        out.lastChoice = i32(112);
        out.choiceColumns = i32(kBeforeChoiceColumns);
        for (size_t i = 0; i < kBeforeChoiceCellsMax; i++) out.choiceCells.push_back(i32(kBeforeChoiceCells + 4 * i));
    }
    r.skip(kBeforeVariables);
    out.variables.clear();
    for (int i = 0; i < kVariables; i++) {
        int v = r.i32();
        if (v) out.variables[i] = v;
    }
    int count = r.i32();
    r.skip(older ? size_t(kAfterCount - layout.olderRecordGap) : kAfterCount);
    if (!r.ok || count < 0 || count > 2000) return false;
    out.noRecords = count == 0;
    out.sprites.clear();
    int extra = layout.recordExtra;
    const size_t kRecordTail = size_t(layout.recordTail);
    for (int k = 0; k < count && r.ok; k++) {
        SavedSprite s;
        if (older) r.skip(size_t(layout.olderRecordGap));
        r.skip(1);
        bool ordinary = r.u8() != 0;
        s.slot = r.i32();
        int number = r.i32();
        if (!ordinary) s.flipFrame = number;
        if (!ordinary && number == kNoiseKind) {
            s.noise = true;
            s.flipFrame = -1;
        }
        s.path = r.str();
        s.nextPath = r.str();
        size_t f = r.at;
        if (older) {
            // A part's record, an ordinary one of another kind than a picture's, names three pictures in its fixed
            // part, and all after them moves by their length.
            size_t named = 0;
            if (ordinary && number != 0 && s.slot != -999999) {
                s.kind = number;
                r.at = f + kOlderNames;
                for (std::string &name : s.names) {
                    name = r.str();
                    named += name.size();
                }
                r.at = f;
            }
            size_t animationAt = f + kOlderAnimation + named;
            int text = r.ok && animationAt + 4 <= plain.size() ? r.i32At(animationAt) : -1;
            if (!r.ok || text < 0 || f + size_t(layout.olderRecord) + named + size_t(text) > plain.size()) {
                r.ok = false;
                break;
            }
            size_t textAt = animationAt + 4, end = f + size_t(layout.olderRecord) + named + size_t(text);
            s.animation.assign(plain.begin() + std::ptrdiff_t(textAt), plain.begin() + std::ptrdiff_t(textAt + size_t(text)));
            // The fixed part as it would be with no names and no text, which every field is read from.
            std::vector<uint8_t> fixed(plain.begin() + std::ptrdiff_t(f), plain.begin() + std::ptrdiff_t(f + kOlderNames));
            fixed.resize(fixed.size() + 12, 0);
            fixed.insert(fixed.end(), plain.begin() + std::ptrdiff_t(f + kOlderNames + 12 + named), plain.begin() + std::ptrdiff_t(textAt));
            fixed.insert(fixed.end(), plain.begin() + std::ptrdiff_t(textAt + size_t(text)), plain.begin() + std::ptrdiff_t(end));
            std::fill(fixed.begin() + kOlderAnimation, fixed.begin() + kOlderAnimation + 4, uint8_t(0));
            Reader fr(fixed);
            s.x = float(fr.i32At(kOlderX));
            s.y = float(fr.i32At(kOlderX + 4));
            s.scaleX = fr.f32At(kOlderScale);
            s.scaleY = fr.f32At(kOlderScale + 4);
            // A shown picture holds 255 here, where the newer engine's hold 256.
            s.alpha = std::min(256, fr.i32At(kOlderAlpha) == 255 ? 256 : fr.i32At(kOlderAlpha));
            s.endX = float(fr.i32At(kOlderEndX));
            s.endY = float(fr.i32At(kOlderEndX + 4));
            s.endAlpha = fr.i32At(kOlderEndAlpha) == 255 ? 256 : fr.i32At(kOlderEndAlpha);
            s.endScaleX = fr.f32At(kOlderEndScale);
            s.endScaleY = fr.f32At(kOlderEndScale + 4);
            r.at = end;
            if (s.slot == -999999) {
                out.width = fr.i32At(size_t(layout.rootSizeAt));
                out.height = fr.i32At(size_t(layout.rootSizeAt) + 4);
                out.rootAlpha = fr.i32At(kOlderAlpha);
                out.rootFixed = fixed;
            }
            s.rawFixed = std::move(fixed);
            if (s.slot != -999999 && (s.noise || !s.path.empty() || s.kind != 0)) {
                // The film noise's record has no picture, as in the newer engine: Test Game 82's opening shows it. Nor
                // has a part's emitter.
                out.sprites.push_back(s);
            }
            continue;
        }
        s.splash = !s.noise && s.path.empty() && s.slot != -999999;
        size_t nameLength = 0;
        if (s.splash) {
            size_t at = f + size_t(kSplashNameAt + extra);
            int n = r.i32At(at);
            if (!r.ok || n < 0 || at + 4 + size_t(n) + 48 > plain.size()) {
                r.ok = false;
                break;
            }
            nameLength = size_t(n);
            s.splashImage.assign(reinterpret_cast<const char *>(&plain[at + 4]), nameLength);
            for (size_t i = 0; i < s.splashNumbers.size(); i++) s.splashNumbers[i] = r.i32At(at + 4 + nameLength + 4 * i);
        }
        size_t fixedEnd = f + size_t(kRecordAnimationAt + extra) + nameLength;
        // The whole fixed part, so one cut short is caught here and not read past the end below.
        if (!r.ok || fixedEnd + 4 > plain.size()) {
            r.ok = false;
            break;
        }
        s.rawFixed.assign(plain.begin() + std::ptrdiff_t(f), plain.begin() + std::ptrdiff_t(fixedEnd));
        s.x = r.f32At(f + kRecordX + extra);
        s.y = r.f32At(f + kRecordX + 4 + extra);
        s.scaleX = r.f32At(f + kRecordScale + extra);
        s.scaleY = r.f32At(f + kRecordScale + 4 + extra);
        s.anchorX = r.i32At(f + kRecordAnchor + extra);
        s.anchorY = r.i32At(f + kRecordAnchor + 4 + extra);
        s.alpha = r.i32At(f + kRecordAlpha + extra);
        if (layout.shownAlpha == 255 && s.slot != -999999 && s.alpha == 255) s.alpha = 256;
        s.blurOn = plain[f + kRecordBlurOn + extra] != 0;
        s.blur = r.i32At(f + kRecordBlur + extra);
        s.waveOn = plain[f + kRecordWave + extra] != 0;
        s.waveSpeed = r.i32At(f + kRecordWave + 1 + extra);
        s.waveRows = r.i32At(f + kRecordWave + 5 + extra);
        s.waveSize = r.i32At(f + kRecordWave + 9 + extra);
        s.animated = f + kRecordAnimated + extra < plain.size() && plain[f + kRecordAnimated + extra] != 0;
        s.repeat = plain[f + kRecordAnimated + 1 + extra] != 0;
        r.at = fixedEnd;
        s.animationFrames = r.i32();
        s.animation = r.str();
        size_t g = r.at;
        if (!r.ok || g + kRecordTail > plain.size()) {
            r.ok = false;
            break;
        }
        s.endX = r.f32At(g + 4);
        s.endY = r.f32At(g + 8);
        s.endAlpha = r.i32At(g + 12);
        if (layout.shownAlpha == 255 && s.slot != -999999 && s.endAlpha == 255) s.endAlpha = 256;
        s.endScaleX = r.f32At(g + 20);
        s.endScaleY = r.f32At(g + 24);
        s.rawTail.assign(plain.begin() + std::ptrdiff_t(g), plain.begin() + std::ptrdiff_t(g + kRecordTail));
        // A finished one-shot flipbook keeps its fields with the playing bytes at 0, as Test Game 84's reversed slash does.
        if (s.flipFrame >= 0 && (plain[g + kTailFlipPlaying] || plain[g + kTailFlipPlaying + 1] || r.i32At(g + kTailFlipStep))) {
            s.flipRepeat = plain[g + kTailFlipRepeat];
            s.flipReverse = plain[g + kTailFlipReverse];
            s.flipPlaying = plain[g + kTailFlipPlaying] != 0;
            s.flipStep = r.i32At(g + kTailFlipStep);
            s.flipParam15 = r.i32At(g + kTailFlipParam15);
        }
        r.at = g + kRecordTail;
        // The film noise's has no picture and nothing more, under Wine in The Alchemist.
        if (s.slot != -999999) {
            out.sprites.push_back(s);
        } else {
            out.width = r.i32At(f + size_t(layout.rootSizeAt));
            out.height = r.i32At(f + size_t(layout.rootSizeAt) + 4);
            out.rootAlpha = s.alpha;
            out.rootFixed = std::move(s.rawFixed);
            out.rootTail = std::move(s.rawTail);
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
    out.afterRecords.assign(plain.begin() + std::ptrdiff_t(afterStart), plain.begin() + std::ptrdiff_t(r.at));
    out.windowShown = r.u8() != 0;
    out.windowAfter = r.u8();
    if (out.windowAfter == int(out.windowShown)) out.windowAfter = -1;
    readText(r, out.current, layout.textTail);
    for (uint8_t &b : out.afterText) b = r.u8();
    out.logLength = r.i32();
    int logged = r.i32();
    if (!r.ok || logged < 0 || logged > 100000) return false;
    out.backlog.clear();
    for (int i = 0; i < logged && r.ok; i++) {
        SavedText t;
        readText(r, t, layout.textTail);
        out.backlog.push_back(t);
    }
    if (!older && layout.byteBeforeSounds) r.skip(1);
    out.sounds.clear();
    for (int i = 0; i < layout.channels && r.ok; i++) {
        SavedSound s;
        // The engine numbers effects from 60, as the newer engine saves them; the older saves them from 30.
        s.index = older && i >= kOlderMusicChannels ? i - kOlderMusicChannels + 60 : i;
        s.playing = r.u8() != 0;
        s.path = r.str();
        s.volume = r.i32();
        s.pan = r.i32();
        s.seconds = r.f64();
        s.trailer[0] = r.u8();
        if (!older) s.trailer[1] = r.u8();
        // A channel with no sound can keep its volume and pan, which a save written back keeps too.
        if (!s.path.empty() || s.volume || s.pan || s.seconds != 0 || s.trailer[0] || s.trailer[1]) out.sounds.push_back(s);
    }
    r.skip(older ? size_t(layout.olderSoundGap) : 16);
    out.scene = r.str();
    size_t tail = r.at;
    // The tail starts with the scene's node; a save cut short before it doesn't read.
    if (tail + 4 > plain.size()) return false;
    out.node = r.i32At(tail);
    if (older || !layout.gtTail) {
        if (r.ok && tail <= plain.size()) out.tailBefore.assign(plain.begin() + std::ptrdiff_t(tail), plain.end());
        return r.ok;
    }
    out.gtState = r.i32At(tail + kTailGtState);
    out.gtCount = r.i32At(tail + kTailGtCount);
    out.gtStartNode = r.i32At(tail + kTailGtStart);
    out.gtMarker = r.i32At(tail + kTailGtMarker);
    r.at = tail + kTailGtStrings;
    out.gtScene = r.str();
    out.gtConfig = r.str();
    if (r.ok) {
        out.tailBefore.assign(plain.begin() + std::ptrdiff_t(tail), plain.begin() + std::ptrdiff_t(tail + kTailGtStrings));
        out.tailAfter.assign(plain.begin() + std::ptrdiff_t(r.at), plain.end());
    }
    return r.ok;
}

std::vector<uint8_t> buildSave(const SaveData &d, const SaveLayout &layout) {
    Writer w;
    w.u8(1);
    w.str(d.comment);
    w.zeros(kAfterComment);
    w.str(d.flow);
    bool older = layout.olderRecord > 0;
    std::string choice = d.choiceImage.empty() && layout.choiceName && !d.fromFile ? std::string(layout.choiceName) : d.choiceImage;
    if (older) {
        w.zeros(kOlderBeforeChoice);
        w.str(layout.choiceInFirstPlace ? choice : d.olderImage);
        w.zeros(kBeforeChoice - kOlderBeforeChoice - 4);
        w.str(layout.choiceInFirstPlace ? d.olderImage : choice);
    } else {
        w.zeros(kBeforeChoice);
        w.str(choice);
    }
    if (layout.afterChoiceName) {
        w.str(d.afterChoiceImage.empty() && !d.fromFile ? std::string(layout.afterChoiceName) : d.afterChoiceImage);
        w.zeros(kAfterChoice - 4);
    } else {
        w.zeros(kAfterChoice);
    }
    for (int v : d.date) w.i32(v);
    size_t dateRest = w.zeros(kDateBlock - 24);
    apply(layout.dateBlock, w.b.data() + dateRest - 24);
    w.setI32(dateRest + kDateFlag - 24, d.dateFlag);
    w.i32(d.flowActive);
    w.i32(d.flowNode);
    size_t before = w.zeros(kBeforeVariables);
    if (d.beforeVariables.size() == size_t(kBeforeVariables)) std::copy(d.beforeVariables.begin(), d.beforeVariables.end(), w.b.begin() + std::ptrdiff_t(before));
    else apply(layout.beforeVariables, w.b.data() + before);
    if (d.lastChoice >= 0) w.setI32(before + 112, d.lastChoice);
    if (d.choiceColumns >= 0) w.setI32(before + kBeforeChoiceColumns, d.choiceColumns);
    for (size_t i = 0; i < std::min(d.choiceCells.size(), kBeforeChoiceCellsMax); i++) w.setI32(before + kBeforeChoiceCells + 4 * i, d.choiceCells[i]);
    size_t table = w.zeros(size_t(kVariables) * 4);
    for (const auto &[index, value] : d.variables)
        if (index >= 0 && index < kVariables) w.setI32(table + size_t(index) * 4, value);
    w.i32(d.noRecords ? 0 : int32_t(d.sprites.size()) + 1);
    w.zeros(older ? size_t(kAfterCount - layout.olderRecordGap) : kAfterCount);

    int extra = layout.recordExtra;
    const size_t kRecordTail = size_t(layout.recordTail);
    auto record = [&](const SavedSprite &s, bool root, bool hasNext) {
        if (older) w.zeros(size_t(layout.olderRecordGap));
        w.u8(1);
        w.u8(s.flipFrame >= 0 || s.noise ? 0 : 1);
        w.i32(root ? -999999 : s.slot);
        // A splash's record is kind 25, as Test Game 88's saves have it.
        w.i32(root ? 3 : s.noise ? kNoiseKind : s.splash ? 25 : s.flipFrame >= 0 ? s.flipFrame : older ? s.kind : 0);
        w.str(s.path);
        w.str(s.nextPath);
        if (older) {
            size_t f = w.zeros(size_t(layout.olderRecord));
            const auto &raw = root ? d.rootFixed : s.rawFixed;
            if (raw.size() == size_t(layout.olderRecord)) {
                std::copy(raw.begin(), raw.end(), w.b.begin() + std::ptrdiff_t(f));
            } else {
                apply(root || s.noise ? layout.rootHead : layout.spriteHead, w.b.data() + f);
                for (const auto &[at, bytes] : s.fixedBytes)
                    if (at >= 0 && size_t(at) + bytes.size() <= size_t(layout.olderRecord)) std::copy(bytes.begin(), bytes.end(), w.b.begin() + std::ptrdiff_t(f) + at);
            }
            if (root || s.noise) {
                // The film noise's record is the screen's without its first byte, at full alpha, 256.
                if (s.noise) w.b[f + 12] = 0;
                w.setI32(f + size_t(layout.rootSizeAt), d.width);
                w.setI32(f + size_t(layout.rootSizeAt) + 4, d.height);
                w.setI32(f + kOlderAlpha, root ? d.rootAlpha : 256);
                w.setI32(f + kOlderEndAlpha, root ? d.rootAlpha : 256);
            } else {
                // Each place is kept three times, all the same in the saves seen but those with an animation running,
                // whose third the PC sets otherwise: a PC record's is kept while its place is.
                auto rawAt = [&](int at) { int32_t v; std::memcpy(&v, raw.data() + at, 4); return v; };
                bool kept = raw.size() == size_t(layout.olderRecord) && rawAt(kOlderX) == int32_t(std::lround(s.x)) && rawAt(kOlderX + 4) == int32_t(std::lround(s.y));
                for (int at : {kOlderX, kOlderEndX, kOlderThirdX}) {
                    if (at == kOlderThirdX && kept) continue;
                    w.setI32(f + size_t(at), int32_t(std::lround(at == kOlderEndX ? s.endX : s.x)));
                    w.setI32(f + size_t(at) + 4, int32_t(std::lround(at == kOlderEndX ? s.endY : s.y)));
                }
                w.setF32(f + kOlderScale, s.scaleX);
                w.setF32(f + kOlderScale + 4, s.scaleY);
                w.setF32(f + kOlderEndScale, s.endScaleX);
                w.setF32(f + kOlderEndScale + 4, s.endScaleY);
                w.setI32(f + kOlderAlpha, std::min(255, s.alpha));
                w.setI32(f + kOlderEndAlpha, std::min(255, s.endAlpha));
                // A PC record's animation text goes back where it was. A record of the runtime's own leaves it empty, as
                // before: which of the older head's bytes say an animation runs isn't known.
                if (raw.size() == size_t(layout.olderRecord) && !s.animation.empty()) {
                    w.setI32(f + kOlderAnimation, int32_t(s.animation.size()));
                    w.b.insert(w.b.begin() + std::ptrdiff_t(f + kOlderAnimation + 4), s.animation.begin(), s.animation.end());
                }
                // A part's three pictures, named last since all after them moves.
                size_t at = f + kOlderNames;
                for (const std::string &name : s.names) {
                    w.setI32(at, int32_t(name.size()));
                    w.b.insert(w.b.begin() + std::ptrdiff_t(at + 4), name.begin(), name.end());
                    at += 4 + name.size();
                }
            }
            return;
        }
        const auto &rawFixed = root ? d.rootFixed : s.rawFixed;
        const auto &rawTail = root ? d.rootTail : s.rawTail;
        size_t fixedSize = size_t(kRecordAnimationAt + extra) + (s.splash ? s.splashImage.size() : 0);
        size_t f = w.zeros(fixedSize);
        if (rawFixed.size() == fixedSize) {
            std::copy(rawFixed.begin(), rawFixed.end(), w.b.begin() + std::ptrdiff_t(f));
        } else {
            apply(root ? layout.rootHead : layout.spriteHead, w.b.data() + f);
            if (s.splash) {
                // The droplet's picture is named where every other record has an empty name, which moves the rest, and
                // command 200's numbers follow it.
                size_t at = f + size_t(kSplashNameAt + extra);
                w.b.erase(w.b.end() - std::ptrdiff_t(s.splashImage.size()), w.b.end());
                w.b.insert(w.b.begin() + std::ptrdiff_t(at + 4), s.splashImage.begin(), s.splashImage.end());
                w.setI32(at, int32_t(s.splashImage.size()));
                for (size_t i = 0; i < s.splashNumbers.size(); i++) w.setI32(at + 4 + s.splashImage.size() + 4 * i, s.splashNumbers[i]);
            }
        }
        if (s.splash) {
            w.i32(s.animationFrames);
            w.str(s.animation);
            size_t g = w.zeros(kRecordTail);
            if (rawTail.size() == kRecordTail) std::copy(rawTail.begin(), rawTail.end(), w.b.begin() + std::ptrdiff_t(g));
            else apply(layout.spriteTail, w.b.data() + g);
            return;
        }
        if (root || s.noise) {
            w.setI32(f + size_t(layout.rootSizeAt), d.width);
            w.setI32(f + size_t(layout.rootSizeAt) + 4, d.height);
        }
        w.setF32(f + kRecordX + extra, s.x);
        w.setF32(f + kRecordX + 4 + extra, s.y);
        w.setF32(f + kRecordScale + extra, s.scaleX);
        w.setF32(f + kRecordScale + 4 + extra, s.scaleY);
        w.setI32(f + kRecordAnchor + extra, s.anchorX);
        w.setI32(f + kRecordAnchor + 4 + extra, s.anchorY);
        // Only a layout whose shown sprites hold 255 changes the value, and only that one: any other is written as it is,
        // as a PC save of Test Game 94 with 512 there must come back with 512.
        auto shown = [&](int alpha) { return layout.shownAlpha == 255 && !root && alpha == 256 ? 255 : alpha; };
        w.setI32(f + kRecordAlpha + extra, shown(s.alpha));
        w.b[f + kRecordBlurOn + extra] = s.blurOn ? 1 : 0;
        w.setI32(f + kRecordBlur + extra, s.blur);
        w.b[f + kRecordWave + extra] = s.waveOn ? 1 : 0;
        w.setI32(f + kRecordWave + 1 + extra, s.waveSpeed);
        w.setI32(f + kRecordWave + 5 + extra, s.waveRows);
        w.setI32(f + kRecordWave + 9 + extra, s.waveSize);
        w.b[f + kRecordAnimated + extra] = s.animated ? 1 : 0;
        w.b[f + kRecordAnimated + 1 + extra] = s.animated && s.repeat ? 1 : 0;
        w.i32(s.animationFrames);
        w.str(s.animation);
        size_t g = w.zeros(kRecordTail);
        // A PC save's last block goes back as it was: its slot, end anchor and third copy of the place aren't always the
        // record's own, and the runtime doesn't read them.
        if (rawTail.size() == kRecordTail) {
            std::copy(rawTail.begin(), rawTail.end(), w.b.begin() + std::ptrdiff_t(g));
            return;
        }
        apply(layout.spriteTail, w.b.data() + g);
        w.setI32(g, root || s.noise ? 0 : s.slot);
        w.setF32(g + 4, s.endX);
        w.setF32(g + 8, s.endY);
        w.setI32(g + 12, shown(s.endAlpha));
        w.setI32(g + 16, s.blur);
        // A third copy of where the sprite started.
        w.setF32(g + 0x3C, s.x);
        w.setF32(g + 0x40, s.y);
        w.setF32(g + 20, s.endScaleX);
        w.setF32(g + 24, s.endScaleY);
        w.setI32(g + 28, s.anchorX);
        w.setI32(g + 32, s.anchorY);
        if (s.flipFrame >= 0 && s.flipRepeat >= 0) {
            w.b[g + kTailFlipReverse] = s.flipReverse ? 1 : 0;
            w.b[g + kTailFlipRepeat] = s.flipRepeat ? 1 : 0;
            w.b[g + kTailFlipPlaying] = w.b[g + kTailFlipPlaying + 1] = s.flipPlaying ? 1 : 0;
            w.setI32(g + kTailFlipParam15, s.flipParam15);
            w.setI32(g + kTailFlipStep, s.flipStep);
            w.b[g + kTailFlipHundred] = 0;
        }
        if (!layout.recordExtra) w.setI32(g + kRecordTail - 4, hasNext ? 1 : 0);
    };
    SavedSprite rootSprite;
    rootSprite.alpha = rootSprite.endAlpha = d.rootAlpha;
    if (!d.noRecords) {
        record(rootSprite, true, false);
        for (size_t i = 0; i < d.sprites.size(); i++) record(d.sprites[i], false, i + 1 < d.sprites.size());
    }

    if (!d.afterRecords.empty()) {
        w.b.insert(w.b.end(), d.afterRecords.begin(), d.afterRecords.end());
    } else {
        size_t after = w.zeros(size_t(layout.afterSize));
        apply(layout.after, w.b.data() + after);
    }
    w.u8(d.windowShown ? 1 : 0);
    w.u8(uint8_t(d.windowAfter >= 0 ? d.windowAfter : d.windowShown ? 1 : 0));
    writeText(w, d.current, layout.textTail);
    w.b.insert(w.b.end(), d.afterText.begin(), d.afterText.end());
    w.i32(d.logLength);
    w.i32(int32_t(d.backlog.size()));
    for (const SavedText &t : d.backlog) writeText(w, t, layout.textTail);

    if (!older && layout.byteBeforeSounds) w.zeros(1);
    std::map<int, const SavedSound *> byIndex;
    for (const SavedSound &s : d.sounds) byIndex[older && s.index >= 60 ? s.index - 60 + kOlderMusicChannels : s.index] = &s;
    size_t trail = older ? 1 : 2;
    for (int i = 0; i < layout.channels; i++) {
        auto it = byIndex.find(i);
        if (it == byIndex.end()) {
            w.u8(0);
            w.str("");
            w.zeros(16 + trail);
            continue;
        }
        const SavedSound &s = *it->second;
        w.u8(s.playing ? 1 : 0);
        w.str(s.path);
        w.i32(s.volume);
        w.i32(s.pan);
        w.f64(s.seconds);
        for (size_t i = 0; i < trail; i++) w.u8(s.trailer[i]);
    }
    w.zeros(older ? size_t(layout.olderSoundGap) : 16);
    w.str(d.scene);
    // A PC save's tail goes back as it was, with what the runtime reads written over it.
    bool wholeTail = older || !layout.gtTail;
    bool rawTail = wholeTail ? d.tailBefore.size() >= 4 : d.tailBefore.size() == kTailGtStrings;
    size_t tail = w.b.size();
    if (rawTail) w.b.insert(w.b.end(), d.tailBefore.begin(), d.tailBefore.end());
    else {
        // The vector grows first: data() taken in the same expression could point at the storage the growth frees.
        size_t zeros = w.zeros(size_t(layout.tailSize));
        apply(layout.tail, w.b.data() + zeros);
    }
    w.setI32(tail, d.node);
    if (wholeTail) return w.b;
    w.setI32(tail + kTailGtState, d.gtState);
    w.setI32(tail + kTailGtCount, d.gtCount);
    w.setI32(tail + kTailGtCount + 4, d.gtCount);
    w.setI32(tail + kTailGtStart, d.gtStartNode);
    w.setI32(tail + kTailGtMarker, d.gtMarker);
    // The tail's two strings are empty in the layouts, so their text goes in place of those two lengths.
    Writer strings;
    strings.str(d.gtScene);
    strings.str(d.gtConfig);
    if (rawTail) {
        w.b.insert(w.b.end(), strings.b.begin(), strings.b.end());
        w.b.insert(w.b.end(), d.tailAfter.begin(), d.tailAfter.end());
        return w.b;
    }
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

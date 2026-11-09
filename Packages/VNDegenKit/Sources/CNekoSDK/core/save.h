// The PC engine's save file, read and written as it is, so saves move between VNDegen and a PC copy of the game. It's a
// stream of fields, mostly zero. docs/nekosdk-cleanroom.md says what's known of it. Fields VNDegen doesn't understand keep
// the values the PC wrote in the saves the layouts were taken from.
#pragma once
#include <cstdint>
#include <array>
#include <map>
#include <string>
#include <vector>

namespace neko {

struct SavedSprite {
    int slot = 0;
    int flipFrame = -1;  // the picture a flipbook is on, or -1 for an ordinary sprite
    std::string path, nextPath;
    float x = 0, y = 0, scaleX = 1, scaleY = 1;
    int anchorX = 0, anchorY = 0, alpha = 256;
    bool blurOn = false;
    int blur = 0;
    bool waveOn = false;
    int waveSpeed = 0, waveRows = 0, waveSize = 0;
    bool animated = false;
    // Whether the animation repeats, the byte after `animated`: 1 in every PC save of a named animation running, and the
    // PC plays one without it once and drops it, as loading such saves under Wine shows.
    bool repeat = false;
    int animationFrames = 0;  // how far into the animation
    std::string animation;    // lines such as `relative_pos_x,-200,0,2400,linear`
    float endX = 0, endY = 0, endScaleX = 1, endScaleY = 1;
    int endAlpha = 256;
    // The film noise's record: no picture, kind 4, and the window's size where the first record keeps the screen's.
    bool noise = false;
    // A splash effect's record (Test Game 88's 200 and 201): no picture, its droplet's picture named in the record, and
    // command 200's parameters 0, 5, 1 to 4, 6, 10, 11 and 7 to 9, in that order.
    // A flipbook's: whether it repeats and runs backward, command 121's parameters 15 and 16, and whether it's still
    // playing, which a PC save needs to keep the flipbook (Test Games 88 and 84, under Wine), or -1 when the record
    // doesn't say.
    int flipRepeat = -1, flipReverse = 0, flipStep = 0, flipParam15 = 0;
    bool flipPlaying = true;
    bool splash = false;
    std::string splashImage;
    std::array<int, 12> splashNumbers{};
    // The record's fixed part and its last 164 bytes as a PC save holds them, kept so the save is written back as it
    // was, with what the runtime reads written over them. Empty in the runtime's own saves.
    std::vector<uint8_t> rawFixed, rawTail;
};

struct SavedText {
    int tag = -10;
    std::string name, text, voice;  // CP932
    int volume = 100, kind = 4;
    // The 4 bytes after the voice's volume: 60 and -60 in some of Test Game 83's lines, most likely the voice's pan.
    int pan = 0;
};

struct SavedSound {
    int index = 0;  // 0..59 music channels, then sound effect channels
    bool playing = false;
    std::string path;
    int volume = 0, pan = 0;
    double seconds = 0;
    // The byte or two after the time, whose meaning isn't known: 1 on some channels in Test Game 88's saves.
    std::array<uint8_t, 2> trailer{};
};

struct SaveData {
    std::string flow, choiceImage;
    // The older engine's other picture name, empty in every PC save seen and kept as read: the first, 236 bytes after the
    // flow, in Test Games 82 and 83, which name a choice's picture where the newer engine does, or the newer place in Test
    // Game 87, which names it in the first.
    std::string olderImage;
    // The card's comment, typed on the PC's save screen and kept at the save's start; a comment there moves all that follows.
    std::string comment;  // CP932
    int date[6] = {2026, 1, 1, 0, 0, 0};
    int flowActive = 1, flowNode = 0;
    // The block before the variables as a PC save holds it, kept so the save is written back as it was: in saves at a
    // choice it holds numbers the runtime doesn't use yet. Empty in the runtime's own saves, which write the layout's.
    std::vector<uint8_t> beforeVariables;
    // In the story-in-exe games, the last choice's result, 2 while one waits, or -1 to leave the layout's (set 1, Test
    // Game 87), or in Test Game 83 2 once any choice is made (set 3): 112 bytes into the block before the variables.
    int lastChoice = -1;
    // Test Games 82 and 83 keep the latest choice asked in the same block: its options' count in lastChoice's place, the
    // choice sheet's columns 116 bytes in, and from 152 the cells it showed, a list the PC never clears, so a choice with
    // fewer options leaves the last one's later cells (sets 2 and 3's PC saves). -1 and none when not written.
    int choiceColumns = -1;
    std::vector<int> choiceCells;
    std::map<int, int> variables;  // the 10,000 ints, by number
    int dateFlag = 0;  // the PC sets it in a game with a flow past its first script. Its meaning isn't known
    int width = 1280, height = 720;
    std::vector<SavedSprite> sprites;
    bool windowShown = true;
    // The byte after it, which is 0 in Test Game 83's saves at a choice, or -1 for the same as windowShown.
    int windowAfter = -1;
    // A save with no records at all, not even the screen's: the PC's Previous Choice file before any choice.
    bool noRecords = false;
    // The screen's record's two alpha fields: 256, or 255 in a save the PC made just after a load.
    int rootAlpha = 256;
    // As read from a PC save, kept so it's written back as it was: the screen record's raw parts, the block after the
    // records with its list of slots, and the tail after the scene's name around the fields the runtime reads.
    std::vector<uint8_t> rootFixed, rootTail, afterRecords, tailBefore, tailAfter;
    SavedText current;
    // The 24 bytes after the text on screen, whose meaning isn't known: zeros in most saves, kept as the PC wrote them.
    std::array<uint8_t, 24> afterText{};
    // Then the number before the log's lines: 60, most likely the log's length, and 0 in a save with nothing in it.
    int logLength = 60;
    std::vector<SavedText> backlog;  // newest first
    std::vector<SavedSound> sounds;
    std::string scene;
    int node = 0;
    // The GOOD Timing system: -1 when it's off, the scene holding its marker, and its config.
    int gtState = -1, gtCount = 0, gtStartNode = 0, gtMarker = 0;
    std::string gtScene, gtConfig;
};

struct SaveLayout;
// What a game's PC writes 360 bytes into the date block (SaveData::dateFlag) in its main saves: 1 while it holds a choice's
// picture, in the newer engine's games; 1 once a choice has been made, in Test Game 87, whose saves at its first choice
// have 0 (set 1); or always 0, in Test Games 82 and 83, whose Previous Choice files alone have 1 (sets 2 and 3).
enum class ChoiceFlag { Picture, Made, Never };
// The layout for a game, by its normalized UserDataDirectory, or null when VNDegen hasn't checked one yet.
const SaveLayout *saveLayout(const std::string &userDataDirectory);

bool parseSave(const std::vector<uint8_t> &plain, const SaveLayout &layout, SaveData &out);
// Whether a game's PC names a record's picture a second time and loads it from there. Test Game 93's PC loses every
// picture of a save without it, and Test Game 84's crashes loading a save with it (under Wine), so it's per game, from
// the PC's own saves; a game none of whose PC saves is known leaves it empty, as every save VNDegen wrote before did.
bool namesPictureTwice(const SaveLayout &layout);
ChoiceFlag choiceFlag(const SaveLayout &layout);
// Whether the game's PC keeps a Previous Choice file, `saveNNNs.sav`, beside each save.
bool writesPreviousChoice(const SaveLayout &layout);
// The NEW card as cfg.sav's global 0 holds it, from a save slot (1 for save001), and back: per game, counted from 1 or 0.
int newCardNumber(const SaveLayout *layout, int slot);
int newCardSlot(const SaveLayout *layout, int number);
std::vector<uint8_t> buildSave(const SaveData &data, const SaveLayout &layout);

// A save with its card's comment replaced and nothing else changed, as the PC rewrites it, or nothing when the save is
// too short to hold one. The comment is CP932, cut to the 36 bytes the PC's box takes.
std::vector<uint8_t> withComment(const std::vector<uint8_t> &plain, std::string comment);

// The container save files share with archive entries: packed, and unpacked with unpackEntry.
std::vector<uint8_t> packEntry(const std::vector<uint8_t> &plain);

}  // namespace neko

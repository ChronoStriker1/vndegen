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
    int animationFrames = 0;  // how far into the animation
    std::string animation;    // lines such as `relative_pos_x,-200,0,2400,linear`
    float endX = 0, endY = 0, endScaleX = 1, endScaleY = 1;
    int endAlpha = 256;
    // The film noise's record: no picture, kind 4, and the window's size where the first record keeps the screen's.
    bool noise = false;
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
};

struct SaveData {
    std::string flow, choiceImage;
    // The older engine's other picture name, 236 bytes after the flow's: empty in saves at a choice, which name theirs
    // where the newer engine does, and a choice's picture in one of Test Game 87's saves made after it. Kept as read.
    std::string olderImage;
    // The card's comment, typed on the PC's save screen and kept at the save's start; a comment there moves all that follows.
    std::string comment;  // CP932
    int date[6] = {2026, 1, 1, 0, 0, 0};
    int flowActive = 1, flowNode = 0;
    // The block before the variables as a PC save holds it, kept so the save is written back as it was: in saves at a
    // choice it holds numbers the runtime doesn't use yet. Empty in the runtime's own saves, which write the layout's.
    std::vector<uint8_t> beforeVariables;
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
// The layout for a game, by its normalized UserDataDirectory, or null when VNDegen hasn't checked one yet.
const SaveLayout *saveLayout(const std::string &userDataDirectory);

bool parseSave(const std::vector<uint8_t> &plain, const SaveLayout &layout, SaveData &out);
std::vector<uint8_t> buildSave(const SaveData &data, const SaveLayout &layout);

// A save with its card's comment replaced and nothing else changed, as the PC rewrites it, or nothing when the save is
// too short to hold one. The comment is CP932, cut to the 36 bytes the PC's box takes.
std::vector<uint8_t> withComment(const std::vector<uint8_t> &plain, std::string comment);

// The container save files share with archive entries: packed, and unpacked with unpackEntry.
std::vector<uint8_t> packEntry(const std::vector<uint8_t> &plain);

}  // namespace neko

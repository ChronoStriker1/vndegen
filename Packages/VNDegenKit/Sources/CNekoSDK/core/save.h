// The PC engine's save file, read and written as it is, so saves move between VNDegen and a PC copy of the game. It's a
// stream of fields, mostly zero. docs/nekosdk-cleanroom.md says what's known of it. Fields VNDegen doesn't understand keep
// the values the PC wrote in the saves the layouts were taken from.
#pragma once
#include <cstdint>
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
    bool animated = false;
    int animationFrames = 0;  // how far into the animation
    std::string animation;    // lines such as `relative_pos_x,-200,0,2400,linear`
    float endX = 0, endY = 0, endScaleX = 1, endScaleY = 1;
    int endAlpha = 256;
};

struct SavedText {
    int tag = -10;
    std::string name, text, voice;  // CP932
    int volume = 100, kind = 4;
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
    int date[6] = {2026, 1, 1, 0, 0, 0};
    int flowActive = 1, flowNode = 0;
    std::map<int, int> variables;  // the 10,000 ints, by number
    int dateFlag = 0;  // the PC sets it in a game with a flow past its first script. Its meaning isn't known
    int width = 1280, height = 720;
    std::vector<SavedSprite> sprites;
    bool windowShown = true;
    SavedText current;
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

// The container save files share with archive entries: packed, and unpacked with unpackEntry.
std::vector<uint8_t> packEntry(const std::vector<uint8_t> &plain);

}  // namespace neko

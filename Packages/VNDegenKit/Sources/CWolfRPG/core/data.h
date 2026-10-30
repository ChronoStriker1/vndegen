// Wolf RPG Editor game data: Game.dat, the databases, common events, maps, and tilesets.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace wolf {

// Wolf RPG Editor 2 writes Shift-JIS (code page 932). Version 3 can write UTF-8, which its files mark with a 'U' in
// their magic number.
enum class Encoding { ShiftJIS, UTF8 };

std::string toUtf8(const std::string &raw, Encoding enc);

struct RouteStep {
    uint8_t id = 0;
    std::vector<int32_t> args;
};

struct MoveRoute {
    uint8_t flags = 0;  // 1 repeat, 2 skip impossible moves, 4 wait until done
    std::vector<RouteStep> steps;
};

struct Command {
    int32_t code = 0;
    std::vector<int32_t> args;
    std::vector<std::string> strings;  // UTF-8
    uint8_t indent = 0;
    std::shared_ptr<MoveRoute> route;  // Move commands only
    int32_t arg(size_t i, int32_t fallback = 0) const { return i < args.size() ? args[i] : fallback; }
    const std::string &str(size_t i) const;
};

using Commands = std::vector<Command>;

struct CommonEvent {
    int32_t id = 0;
    std::string name;
    uint8_t runCondition = 0;  // 0 call only, 1 autorun, 2 parallel, 3 parallel always
    uint8_t conditionOperator = 0;
    int32_t conditionVariable = 0;
    int32_t conditionValue = 0;
    uint8_t numberArgCount = 0;
    uint8_t stringArgCount = 0;
    Commands commands;
    std::vector<int32_t> numberArgDefaults;
    std::array<std::string, 100> cselfNames;
    std::string returnName;
    int32_t returnVariable = -1;
};

struct GameDat {
    Encoding encoding = Encoding::ShiftJIS;
    std::vector<uint8_t> settings;  // the byte settings block
    std::string title, serial, encryptionKey;
    std::array<std::string, 4> fonts;  // base font and three sub fonts
    std::string startHeroGraphic;
    std::string subtitle;
    int screenWidth = 0, screenHeight = 0;
    int editorVersion = 0;
    std::vector<uint8_t> randoms;
    int tileSize() const { return settings.size() > 0 ? settings[0] : 16; }
    int fps() const { return settings.size() > 4 && settings[4] == 30 ? 30 : 60; }
};

struct DatabaseType {
    std::string name;
    std::vector<std::string> fieldNames;
    std::vector<std::string> dataNames;
    std::vector<uint32_t> fieldKinds;  // raw position words: kind 1 number, 2 string; low digits the slot
    std::vector<int32_t> defaults;
    uint32_t idMethod = 0;
    // Each row holds the fields in their own order, numbers and strings together.
    struct Row {
        std::vector<int32_t> numbers;
        std::vector<std::string> strings;
    };
    std::vector<Row> rows;
    bool isString(size_t field) const { return field < fieldKinds.size() && fieldKinds[field] / 1000 != 1; }
    size_t slot(size_t field) const { return field < fieldKinds.size() ? fieldKinds[field] % 1000 : 0; }
};

struct Database {
    std::vector<DatabaseType> types;
};

struct MapPage {
    std::string graphic;
    int32_t chip = 0;
    uint8_t direction = 0, frame = 0, opacity = 255, blend = 0;
    uint8_t trigger = 0;  // 0 confirm, 1 autorun, 2 parallel, 3 hero touch, 4 event touch
    std::array<uint8_t, 4> condOps{};
    std::array<int32_t, 4> condVars{};
    std::array<int32_t, 4> condValues{};
    uint8_t moveType = 0, moveSpeed = 0, moveFrequency = 0, animSpeed = 0;
    uint8_t flags = 0;
    MoveRoute route;
    Commands commands;
    uint8_t shadow = 0, rangeX = 0, rangeY = 0;
};

struct MapEvent {
    int32_t id = 0;
    std::string name;
    int32_t x = 0, y = 0;
    std::vector<MapPage> pages;
};

struct Map {
    std::string name;
    int32_t tileset = 0, width = 0, height = 0;
    int layers = 3;
    std::vector<int32_t> tiles;  // layer-major, then x-major: tiles[(layer * width + x) * height + y]
    std::vector<MapEvent> events;
    int32_t tile(int layer, int x, int y) const { return tiles[(size_t(layer) * width + x) * height + y]; }
};

struct Tileset {
    std::string name;
    std::string baseFile;
    std::vector<std::string> autotileFiles;
    std::vector<uint8_t> tags;
    std::vector<uint32_t> passability;
};

struct MapTreeNode {
    int32_t parent = -1;
};

// The passability word of a map tile value. Autotile slot 0 is the empty tile and slots 1 onwards are the tileset's
// autotile files (values 100000 and up, by hundred thousands); the base image's tiles follow the autotile slots.
inline uint32_t tilePassability(const Tileset &ts, int32_t value) {
    if (value < 0) return 0;
    size_t index = value >= 100000 ? size_t(value / 100000 - 1) : ts.autotileFiles.size() + 1 + size_t(value);
    return index < ts.passability.size() ? ts.passability[index] : 0;
}

GameDat parseGameDat(std::span<const uint8_t> data);
std::vector<CommonEvent> parseCommonEvents(std::span<const uint8_t> data);
// A database is a .project file with types, field names and row names, and a .dat file with the values.
Database parseDatabase(std::span<const uint8_t> project, std::span<const uint8_t> dat, Encoding enc);
Map parseMap(std::span<const uint8_t> data);
std::vector<Tileset> parseTilesets(std::span<const uint8_t> data);

// Encrypted basic data files start with a nonzero byte where a plain file has zero: ten bytes of header take the place
// of the magic number, and the rest is scrambled by seeds taken from the header. This turns such a file back into a
// plain one in place and returns true, or returns false and leaves `bytes` alone when the file is plain. The scheme is
// the one UberWolf's FileCoder (MIT) names v2.0, as docs/wolfrpg-cleanroom.md records.
enum class BasicFile { GameDat, CommonEvent, Other };
bool decryptBasicData(std::vector<uint8_t> &bytes, BasicFile kind);

}  // namespace wolf

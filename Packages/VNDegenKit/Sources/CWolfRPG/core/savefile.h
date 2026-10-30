// Wolf RPG Editor save files (Save/SaveDataNN.sav): parse and write them byte for byte. docs/wolfrpg-save-format.md
// describes the layout and how each field's meaning was established. Fields whose meaning is unknown keep generic
// names (unknownN, or the position in a block) so that they still round-trip.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace wolf {

// A string as the file stores it, in the game's encoding (Shift-JIS, or UTF-8 when the header says so). The engine
// writes a terminating NUL that the length counts; `terminated` records whether it was there.
struct SaveString {
    std::string bytes;
    bool terminated = true;
};

// One step of a move route, as in map data.
struct SaveRouteStep {
    uint8_t id = 0;
    std::vector<int32_t> args;
    std::vector<uint8_t> extra;
};

struct SaveRoute {
    // For the page's own route: move type, speed, frequency, animation speed, the page's option bits (as MapPage::flags)
    // and the route's flags (as MoveRoute::flags). Compared against map data, not tested by changing them.
    std::array<uint8_t, 6> flags{};
    std::vector<SaveRouteStep> steps;
};

// The state of one character on the map: a map event, the hero, or a party member.
struct SaveCharacter {
    int32_t id = 0;          // map event id; -2 for the hero, -3 to -7 for the party members
    int32_t x = 0, y = 0;    // position in half tiles
    int32_t unknown3 = 0;
    uint8_t unknown4 = 0;
    int32_t unknown5 = 0;
    SaveString graphic;      // character image file, empty when none
    std::array<uint16_t, 4> appearance{};  // opacity, blend mode, direction (numeric keypad), animation frame
    uint8_t unknown7 = 0;
    uint8_t hidden = 0;      // 1 when nothing is shown: an event without an active page, an empty party slot
    SaveRoute route, route2;
    std::array<uint16_t, 6> unknown9{};
    std::array<uint8_t, 3> unknown10{};
    std::vector<int32_t> selfVariables;
    // Present from file version 0x70, 0x73, 0x78, 0x85, 0x8A, 0x8B and 0x8C on.
    std::array<uint8_t, 2> v70{};
    std::array<int32_t, 4> v73{};
    int32_t v78 = 0;
    std::array<int32_t, 3> v85{};
    std::vector<SaveString> v8aStrings;
    int32_t v8a1 = 0;
    uint8_t v8a2 = 0;
    int32_t v8a3 = 0, v8a4 = 0;
    int32_t v8b = 0;
    int32_t v8c = 0;
};

// Section 1: the current map.
struct SaveMap {
    int32_t mapId = -1;  // row of the system database's map list; -1 in a save made before any map
    std::array<int32_t, 2> unknown1{};
    // From file version 0x69: the map's size and, unless the file stores -1 instead, its three tile layers, laid out
    // like Map::tiles.
    int32_t width = 0, height = 0;
    bool hasTiles = false;
    std::vector<int32_t> tiles;
    // The map's tileset: base image, autotile images (7, 15 from version 0x73, 31 from 0x8A) and the passability of
    // every tile, as Tileset holds them.
    SaveString tilesetBase;
    std::vector<SaveString> autotiles;
    std::vector<int32_t> tileFlags;    // u32 from version 0x64, u8 before
    std::vector<SaveCharacter> events;  // every event of the map, in map order
    std::array<int32_t, 11> v72{};     // from version 0x72
    std::vector<int32_t> values2;
};

// Section 2: the system state. The fields are listed in file order; see the doc for what is known of them.
struct SaveSystem {
    std::array<uint8_t, 2> a{};
    // b[0] play time in seconds (system variable 29); b[6] to b[13] system variables 0 to 7.
    std::array<int32_t, 19> b{};
    std::array<uint16_t, 3> c{};
    int32_t d = 0;
    std::array<uint16_t, 3> e{};
    std::array<int32_t, 9> f{};
    uint8_t g = 0;
    uint16_t h = 0;
    std::array<int32_t, 2> i{};
    std::array<uint16_t, 4> j{};
    std::array<int32_t, 3> k{};
    uint8_t l = 0;
    int32_t upTo96 = 0;        // version <= 96 only
    int32_t m = 0;
    std::array<int32_t, 2> v98{};
    // version >= 100
    std::array<int32_t, 2> v100a{};
    uint8_t v100b = 0;
    std::vector<SaveString> v100Strings;
    std::vector<int32_t> v100Values;
    std::array<int32_t, 5> v100c{};
    std::array<int32_t, 6> v101{};
    uint16_t v102 = 0;
    std::array<SaveString, 2> v103;
    int32_t frameCount = 0;          // system variable 28, version >= 104
    std::array<int32_t, 3> volumes{};  // system variables 100 to 102, the BGM, BGS and SE volume in percent, >= 106
    SaveString v108s1;
    std::array<int32_t, 3> v108a{};
    SaveString v108s2;
    std::array<int32_t, 3> v108b{};
    std::array<SaveString, 2> v109s;
    uint8_t v109 = 0;
    uint8_t v110 = 0;
    std::array<SaveString, 3> v119;
    int32_t v121 = 0, v122 = 0, v124 = 0, v126 = 0, v128 = 0;
    std::array<int32_t, 2> v129{};
    int32_t v130 = 0, v131 = 0;
    std::array<int32_t, 4> v132{};
    uint8_t v134 = 0, v136 = 0;
    std::array<int32_t, 28> v137{};
    std::array<int32_t, 4> v8a{};
    SaveString v8aString;
    std::vector<SaveString> v8aStrings;
    std::array<uint8_t, 14> v8d{};
    std::array<uint8_t, 256> v8dBytes{};
    std::array<uint8_t, 2> v8d2{};
    int32_t v8e = 0;
};

// Section 3: the variables.
struct SaveVariables {
    int32_t tickCount = 0;  // differs from session to session (the engine's start time?); any value loads
    // Map event self variables: eventSelf[map][event], each list cut after its last nonzero value.
    std::vector<std::vector<std::vector<int32_t>>> eventSelf;
    // Normal variables, then spare variables 1 to 9, each as long as the system database lists names for them.
    std::vector<std::vector<int32_t>> numbers;
    std::vector<SaveString> strings;                    // string variables
    std::vector<std::vector<int32_t>> commonSelf;       // common event self variables 0-99, one list per common event
    std::vector<std::vector<SaveString>> commonSelfStrings;  // common event self variables 5-9, the string ones
};

// Section 4: the hero and the party.
struct SaveParty {
    SaveCharacter hero;
    std::vector<SaveCharacter> members;
    std::array<uint8_t, 2> unknown1{};
    std::vector<int32_t> trail;  // the hero's last steps as directions, newest last, which the members follow
    std::vector<uint64_t> v8a;   // version >= 0x8A
};

// Section 5: one shown picture.
struct SavePicture {
    int32_t number = 0;
    std::array<uint8_t, 2> unknown2{};
    uint16_t unknown3 = 0;
    std::array<uint8_t, 2> kind{};  // 0 0 an image file, 1 0 text, 0 1 a window or <SQUARE>
    SaveString file;                // image file, the text with the engine's own control codes, or the window image
    // x, y, divisions across and down (width and height for a window), frame, opacity, three unknown, zoom across,
    // zoom down, angle.
    std::array<int32_t, 12> a{};
    std::array<int32_t, 6> b{};
    std::array<int32_t, 18> v69{};
    uint8_t v6b = 0;
    std::array<uint8_t, 2> v72a{};
    std::array<int32_t, 8> v72b{};
    std::array<int32_t, 4> v73a{};
    std::array<int32_t, 5> v74{};
    std::array<int32_t, 6> v75{};
    std::array<int32_t, 6> v73b{};
    std::array<int32_t, 3> v76{};
    std::array<int32_t, 21> v81{};
    std::array<int32_t, 5> v87{};
    std::vector<std::vector<int32_t>> v89Lists;
    std::array<int32_t, 7> v89{};
};

struct SavePictures {
    uint16_t count = 0;  // the raw count; with bit 15 set the list is empty
    std::vector<SavePicture> pictures;
};

// The variable database (CDataBase, the CDB) as it stands in the save. Rows hold values the way DatabaseType::Row does.
struct SaveDbType {
    int32_t head = 0;      // the field count, or -1 or -2 followed by the fields below
    int32_t idMethod = 0;  // only when head <= -2; as DatabaseType::idMethod
    std::vector<uint32_t> fieldKinds;  // below 2000 a number field, 2000 and up a string field
    struct Row {
        std::vector<int32_t> numbers;     // the number fields in field order
        std::vector<SaveString> strings;  // the string fields in field order
    };
    std::vector<Row> rows;
};

struct SaveVariableDb {
    uint8_t unknown = 0;
    std::vector<SaveDbType> types;
};

// Section 7.
struct SaveSection7 {
    uint8_t present = 0;
    struct Entry {
        uint8_t kind = 0;
        uint8_t small = 0;   // kind < 0xFA
        uint32_t value = 0;  // kind >= 0xFA
    };
    std::vector<Entry> entries;
};

struct SaveFile {
    std::array<uint8_t, 20> header{};  // stored in clear: XOR seeds at 0, 3 and 9, checksum at 2
    uint8_t startMarker = 0x19;
    SaveString title;                  // u16 length
    uint16_t version = 0;
    SaveMap map;
    SaveSystem system;
    SaveVariables variables;
    SaveParty party;
    SavePictures pictures;
    SaveVariableDb variableDb;  // the CDB
    SaveSection7 section7;
    uint8_t endMarker = 0x19;

    bool utf8() const { return header[6] == 'U'; }
};

// Throws FormatError (reader.h) on malformed input.
SaveFile parseSave(std::span<const uint8_t> fileBytes);
// Writes the file: fixes the checksum in header[2], then encrypts with the header's seeds.
std::vector<uint8_t> serializeSave(const SaveFile &save);

// The XOR layer on its own, for tools. It is its own inverse.
void cryptSave(std::span<uint8_t> fileBytes);

}  // namespace wolf

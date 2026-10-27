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

// One step of a move route.
struct SaveRouteStep {
    uint8_t id = 0;
    std::vector<int32_t> args;
    std::vector<uint8_t> extra;
};

struct SaveRoute {
    std::array<uint8_t, 6> flags{};
    std::vector<SaveRouteStep> steps;
};

// The state of one character on the map: a map event, the hero, or a party member.
struct SaveCharacter {
    int32_t id = 0;          // event id; -1 for the hero and party members
    int32_t x = 0, y = 0;    // position in tiles
    int32_t unknown3 = 0;
    uint8_t unknown4 = 0;
    int32_t unknown5 = 0;
    SaveString graphic;      // character chip file
    std::array<uint16_t, 4> unknown6{};
    uint8_t unknown7 = 0, unknown8 = 0;
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
    std::array<int32_t, 3> unknown1{};
    // From file version 0x69: the map's size and, unless the file stores -1 instead, its three tile layers, laid out
    // like Map::tiles.
    int32_t width = 0, height = 0;
    bool hasTiles = false;
    std::vector<int32_t> tiles;
    SaveString unknown2;
    std::vector<SaveString> strings;   // 7, 15 from version 0x73, 31 from 0x8A
    std::vector<int32_t> values;       // u32 from version 0x64, u8 before
    std::vector<SaveCharacter> events;
    std::array<int32_t, 11> v72{};     // from version 0x72
    std::vector<int32_t> values2;
};

// Section 2: the system state. The fields are listed in file order; see the doc for what is known of them.
struct SaveSystem {
    std::array<uint8_t, 2> a{};
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
    int32_t v104 = 0;
    std::array<int32_t, 3> v106{};
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

// Section 3.
struct SaveSection3 {
    int32_t unknown1 = 0;
    std::vector<std::vector<std::vector<int32_t>>> lists;  // u32 count, u8 count, u32 values
    std::vector<std::vector<int32_t>> numbers;             // u32 count, u32 values
    std::vector<SaveString> strings;
    std::vector<std::vector<int32_t>> numbers2;            // u8 count, u32 values
    std::vector<std::vector<SaveString>> strings2;         // u8 count, strings
};

// Section 4: the hero and the party.
struct SaveParty {
    SaveCharacter hero;
    std::vector<SaveCharacter> members;
    std::array<uint8_t, 2> unknown1{};
    std::vector<int32_t> values;
    std::vector<uint64_t> v8a;  // version >= 0x8A
};

// Section 5: one shown picture.
struct SavePicture {
    int32_t unknown1 = 0;
    std::array<uint8_t, 2> unknown2{};
    uint16_t unknown3 = 0;
    std::array<uint8_t, 2> unknown4{};
    SaveString file;
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

// The variable database (CDataBase) as it stands in the save.
struct SaveDbType {
    int32_t head = 0;      // the field count, or -1 or -2 followed by the fields below
    int32_t unknown = 0;   // only when head <= -2
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
    SaveSection3 section3;
    SaveParty party;
    SavePictures pictures;
    SaveVariableDb variableDb;
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

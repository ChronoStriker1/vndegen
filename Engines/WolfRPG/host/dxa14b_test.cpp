#include "dxa.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace wolf::crypt14b;

static const uint8_t storedHeader[64] = {
    0x44, 0x58, 0x08, 0x00, 0xAC, 0x30, 0x00, 0x00, 0xC6, 0x1D, 0x41, 0x4D, 0xCE, 0x6D, 0x24, 0x3B,
    0xDF, 0xD8, 0x9A, 0x79, 0xEF, 0x30, 0x0B, 0x22, 0x90, 0x0E, 0x3D, 0x11, 0x54, 0x40, 0x2E, 0x4E,
    0xFB, 0x56, 0x80, 0x5E, 0x10, 0x60, 0x78, 0x2D, 0xA4, 0x03, 0x00, 0x00, 0x00, 0x00, 0x4B, 0x01,
    0x0C, 0xEA, 0x4A, 0x14, 0x23, 0x22, 0x55, 0x16, 0xF2, 0x77, 0x50, 0x7B, 0x05, 0xB8, 0x5A, 0x34,
};

static const uint8_t clearHeader[0x30] = {
    0x44, 0x58, 0x08, 0x00, 0xAC, 0x30, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x58, 0xC7, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF4, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x8C, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA4, 0x03, 0x00, 0x00, 0x00, 0x00, 0x4B, 0x01,
};

static const Password password = {0xEA, 0x4A, 0x14, 0x23, 0x22, 0x55, 0x16, 0xF2,
                                  0x77, 0x50, 0x7B, 0x05, 0xB8, 0x5A, 0x34};

template <size_t N> static bool starts(const uint8_t *data, const uint8_t (&expected)[N]) {
    return std::memcmp(data, expected, N) == 0;
}

static void workedExample() {
    uint8_t h[64];
    std::memcpy(h, storedHeader, sizeof(h));
    assert(std::memcmp(h + 0x31, password.data(), password.size()) == 0);
    unscrambleHeader(h);
    assert(std::memcmp(h, clearHeader, sizeof(clearHeader)) == 0);
    assert(std::memcmp(h + 0x30, storedHeader + 0x30, 16) == 0);

    assert(tableSum(password, false) == 93);
    assert(tableSum(password, true) == 225);
    assert(tableSeed(password, false) == 1977);
    assert(tableSeed(password, true) == 2109);

    StreamTable data = streamTable(password, false);
    assert(starts(data.data(), {0x8F, 0xDE, 0x4F, 0x76, 0xFD, 0x3A, 0xF4, 0x4B}));
    assert(starts(data.data() + 256, {0x5C, 0x72, 0x22, 0x2F}));
    assert(starts(data.data() + 512, {0x66, 0x38, 0xDE, 0xC7}));
    const uint8_t dataStream[8] = {0xB5, 0xE4, 0x75, 0x4C, 0xC7, 0x00, 0xCE, 0x71};
    for (size_t i = 0; i < 8; i++) assert(streamByte(data, i) == dataStream[i]);
    const uint8_t at2051[4] = {0x2A, 0xA1, 0x66, 0xA8};
    for (size_t i = 0; i < 4; i++) assert(streamByte(data, 2051 + i) == at2051[i]);

    StreamTable file = streamTable(password, true);
    assert(starts(file.data(), {0x21, 0xF1, 0x4C, 0xA2, 0xA4, 0xBF, 0x57, 0xB0}));

    Block key = aesKey(password);
    assert((key == Block{0xEA, 0xF3, 0x38, 0x1F, 0x6A, 0x6E, 0xDC, 0x53, 0x45, 0x74, 0xDF, 0x8D, 0xE0, 0xF3, 0x3B,
                         0xAC}));
    Block counter = initialCounter(password);
    assert((counter == Block{0xEA, 0x04, 0xEE, 0x1A, 0x24, 0x62, 0xF2, 0xE3, 0x1A, 0xFF, 0xF1, 0xD1, 0x28, 0xCE,
                             0x5E, 0x96}));
    RoundKeys keys = expandKey(key);
    assert(starts(keys.data() + 16, {0xE6, 0xFD, 0x56, 0xDC, 0x8C, 0x93, 0x8A, 0x8F, 0xC9, 0xE7, 0x55, 0x02, 0x29,
                                     0x14, 0x6E, 0xAE}));
    assert((encryptBlock(keys, counter) == Block{0x25, 0x30, 0x66, 0x71, 0x8B, 0xED, 0xBB, 0x61, 0x13, 0x04, 0x7D,
                                                  0x75, 0x2F, 0xDB, 0xB8, 0xBF}));
}

static std::vector<uint8_t> load(const fs::path &path) {
    std::ifstream f(path, std::ios::binary);
    assert(f);
    return {std::istreambuf_iterator<char>(f), {}};
}

static void mapData(const fs::path &path) {
    std::vector<uint8_t> raw = load(path);
    assert(raw.size() == 252043);
    assert(std::memcmp(raw.data(), storedHeader, 64) == 0);
    const uint8_t tableStored[16] = {0x30, 0x59, 0x6C, 0xEF, 0x36, 0xD7, 0x0E, 0xF7,
                                     0x28, 0xA3, 0xD9, 0x51, 0x57, 0x70, 0x81, 0x1F};
    assert(starts(raw.data() + 0x3C758, tableStored));

    std::string error;
    auto a = wolf::Archive::open(path.string(), &error);
    assert(a && a->version() == 8 && a->cryptVersion() == 0x14B);
    auto map = a->read("Map001.mps");
    assert(map && map->size() == 2051);
    const uint8_t start[15] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x57, 0x4F, 0x4C, 0x46, 0x4D};
    assert(starts(map->data(), start));
}

int main() {
    workedExample();
    std::puts("dxa 0x14B worked example ok");

    const char *dir = std::getenv("WOLF_FM2_DIR");
    if (!dir) {
        std::puts("dxa 0x14B: set WOLF_FM2_DIR to False Myth 2's folder to open its archives");
        return 77;
    }
    fs::path data = fs::path(dir) / "Data";
    mapData(data / "MapData.wolf");

    size_t archives = 0, files = 0;
    for (auto &entry : fs::directory_iterator(data)) {
        if (entry.path().extension() != ".wolf") continue;
        std::string error;
        auto a = wolf::Archive::open(entry.path().string(), &error);
        if (!a) std::fprintf(stderr, "%s: %s\n", entry.path().filename().c_str(), error.c_str());
        assert(a && a->cryptVersion() == 0x14B);
        for (auto &p : a->paths()) {
            auto bytes = a->read(p);
            if (!bytes) std::fprintf(stderr, "%s: cannot read %s\n", entry.path().filename().c_str(), p.c_str());
            assert(bytes);
            files++;
        }
        archives++;
    }
    std::printf("dxa 0x14B: %zu archives, %zu files\n", archives, files);
    assert(archives == 42 && files == 11203);
    auto basic = wolf::Archive::open((data / "BasicData.wolf").string());
    assert(basic);
    auto game = basic->read("Game.dat");
    assert(game && game->size() == 29351);
    std::puts("dxa 0x14B ok");
}

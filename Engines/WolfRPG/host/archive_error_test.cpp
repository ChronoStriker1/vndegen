#include "game.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

static void put32(std::string &b, size_t at, uint32_t v) {
    for (int i = 0; i < 4; i++) b[at + i] = char(v >> (8 * i));
}

static std::string startError(const fs::path &root) {
    wolf::Game game(root);
    std::string error;
    assert(!game.load(&error));
    return error;
}

int main() {
    fs::path root = fs::temp_directory_path() / ("wolf-archive-error-" + std::to_string(::getpid()));
    fs::remove_all(root);
    fs::create_directories(root / "Data");
    std::ofstream(root / "Game.exe").put('x');

    assert(startError(root) == "missing BasicData/Game.dat");

    std::string header(64, '\0');
    header[0] = 'D';
    header[1] = 'X';
    header[2] = 8;
    put32(header, 4, 0x4B8);
    const uint32_t scrambled[8] = {0x4466C800, 0x728E4141, 0x1E60F8A9, 0x7A484915,
                                   0x33B87593, 0x67E841A1, 0x36D89CE9, 0x779552A3};
    for (int i = 0; i < 8; i++) put32(header, 8 + 4 * size_t(i), scrambled[i]);
    put32(header, 0x28, 932);
    put32(header, 0x2C, 0x015E0000);
    std::ofstream(root / "Data" / "BasicData.wolf", std::ios::binary).write(header.data(), std::streamsize(header.size()));
    assert(startError(root) == "cannot read BasicData.wolf: its header is encrypted with a Wolf RPG Editor crypt version "
                               "later than 3.31's, which VNDegen doesn't read yet");

    put32(header, 0x2C, 0x014B0000);
    std::ofstream(root / "Data" / "BasicData.wolf", std::ios::binary).write(header.data(), std::streamsize(header.size()));
    assert(startError(root) == "cannot read BasicData.wolf: header points outside the file");

    put32(header, 0x2C, 0x013A0000);
    std::ofstream(root / "Data" / "BasicData.wolf", std::ios::binary).write(header.data(), std::streamsize(header.size()));
    assert(startError(root) == "cannot read BasicData.wolf: header points outside the file");

    fs::remove_all(root);
    std::puts("archive error ok");
}

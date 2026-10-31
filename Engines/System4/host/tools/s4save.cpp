// Save file tool. "s4save dump <file>" prints a GD save's plain bytes to stdout. "s4save roundtrip <game> <file> <key>
// <group> <out>" loads a group save into the game's globals and writes it back, to compare with the original.
#include <cstdio>
#include <cstring>

#include "game.h"
#include "save.h"

int main(int argc, char **argv) {
    if (argc >= 3 && !std::strcmp(argv[1], "dump")) {
        std::vector<uint8_t> d;
        if (!s4::save::readGD(argv[2], d)) { std::fprintf(stderr, "not a GD save\n"); return 1; }
        std::fwrite(d.data(), 1, d.size(), stdout);
        return 0;
    }
    if (argc >= 7 && !std::strcmp(argv[1], "roundtrip")) {
        s4::GameConfig cfg;
        cfg.root = argv[2];
        cfg.saveDir = std::string(argv[3]).substr(0, std::string(argv[3]).find_last_of('/'));
        s4::Game g(cfg);
        g.noThread = true;
        std::string err;
        if (!g.start(err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
        g.vm->start();
        std::string file = std::string(argv[3]).substr(std::string(argv[3]).find_last_of('/') + 1);
        int32_t n = 0;
        int r = s4::save::globalLoad(*g.vm, argv[4], file, argv[5], &n);
        std::printf("load %d, %d globals\n", r, n);
        r = s4::save::globalSave(*g.vm, argv[4], argv[6], argv[5], &n);
        std::printf("save %d, %d globals\n", r, n);
        return 0;
    }
    std::fprintf(stderr, "usage: s4save dump <file> | roundtrip <game> <savefile> <key> <group> <outname>\n");
    return 2;
}

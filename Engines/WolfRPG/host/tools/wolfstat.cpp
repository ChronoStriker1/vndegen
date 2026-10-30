// Parses a Wolf RPG Editor game's Data folder and prints what it holds, with a count of every event command code.
#include "data.h"
#include "reader.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>

namespace fs = std::filesystem;
using namespace wolf;

static std::vector<uint8_t> readFile(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

static void dump(const Commands &cmds) {
    for (auto &c : cmds) {
        std::printf("%*s%d", c.indent * 2, "", c.code);
        for (auto a : c.args) std::printf(" %d", a);
        for (auto &s : c.strings) std::printf(" \"%s\"", s.c_str());
        if (c.route) std::printf(" [route %zu steps flags %d]", c.route->steps.size(), c.route->flags);
        std::printf("\n");
    }
}

int main(int argc, char **argv) {
    // wolfstat <Data> ce <id>: dumps one common event. wolfstat <Data> map <file.mps>: dumps a map's events.
    // wolfstat <Data> db <DataBase|CDataBase|SysDatabase>: lists the types and fields.
    if (argc >= 4) {
        fs::path basic = fs::path(argv[1]) / "BasicData";
        std::string what = argv[2];
        if (what == "ce") {
            auto ces = parseCommonEvents(readFile(basic / "CommonEvent.dat"));
            for (auto &ce : ces) {
                if (ce.id != std::atoi(argv[3])) continue;
                std::printf("common %d \"%s\" run %d op %d var %d value %d args %d/%d\n", ce.id, ce.name.c_str(),
                            ce.runCondition, ce.conditionOperator, ce.conditionVariable, ce.conditionValue,
                            ce.numberArgCount, ce.stringArgCount);
                dump(ce.commands);
            }
        } else if (what == "list") {
            auto ces = parseCommonEvents(readFile(basic / "CommonEvent.dat"));
            for (auto &ce : ces)
                if (ce.commands.size() > 1)
                    std::printf("%d run %d var %d op %d val %d lines %zu \"%s\"\n", ce.id, ce.runCondition,
                                ce.conditionVariable, ce.conditionOperator, ce.conditionValue, ce.commands.size(),
                                ce.name.c_str());
        } else if (what == "map") {
            auto m = parseMap(readFile(argv[3]));
            std::printf("map \"%s\" tileset %d %dx%d\n", m.name.c_str(), m.tileset, m.width, m.height);
            for (auto &ev : m.events) {
                std::printf("event %d \"%s\" at %d,%d\n", ev.id, ev.name.c_str(), ev.x, ev.y);
                for (auto &pg : ev.pages) {
                    std::printf(" page trigger %d graphic \"%s\" conds", pg.trigger, pg.graphic.c_str());
                    for (int i = 0; i < 4; i++) std::printf(" [%d %d %d]", pg.condOps[i], pg.condVars[i], pg.condValues[i]);
                    std::printf("\n");
                    dump(pg.commands);
                }
            }
        } else if (what == "db") {
            std::string n = argv[3];
            auto g = parseGameDat(readFile(basic / "Game.dat"));
            auto db = parseDatabase(readFile(basic / (n + ".project")), readFile(basic / (n + ".dat")), g.encoding);
            int ti = 0;
            for (auto &t : db.types) {
                std::printf("type %d \"%s\" %zu rows:", ti++, t.name.c_str(), t.rows.size());
                for (size_t f = 0; f < t.fieldNames.size(); f++) std::printf(" [%zu %s]", f, t.fieldNames[f].c_str());
                std::printf("\n");
                if (argc > 4 && std::atoi(argv[4]) == ti - 1) {
                    for (size_t r = 0; r < t.rows.size(); r++) {
                        std::printf("  row %zu %s:", r, r < t.dataNames.size() ? t.dataNames[r].c_str() : "");
                        for (size_t f = 0; f < t.fieldKinds.size(); f++) {
                            if (t.isString(f)) std::printf(" \"%s\"", t.rows[r].strings[t.slot(f)].c_str());
                            else std::printf(" %d", t.rows[r].numbers[t.slot(f)]);
                        }
                        std::printf("\n");
                    }
                }
            }
        }
        return 0;
    }
    if (argc < 2) {
        std::fprintf(stderr, "usage: wolfstat <Data folder>\n");
        return 2;
    }
    fs::path data = argv[1];
    fs::path basic = data / "BasicData";
    std::map<int, int> codes;
    int failures = 0;
    auto count = [&](const Commands &cmds) { for (auto &c : cmds) codes[c.code]++; };

    try {
        auto g = parseGameDat(readFile(basic / "Game.dat"));
        std::printf("Game.dat: %s, %dx%d, tile %d, %d fps, editor version %d, %s\n", g.title.c_str(), g.screenWidth,
                    g.screenHeight, g.tileSize(), g.fps(), g.editorVersion,
                    g.encoding == Encoding::UTF8 ? "UTF-8" : "Shift-JIS");
        std::printf("  fonts: %s | %s | %s | %s\n", g.fonts[0].c_str(), g.fonts[1].c_str(), g.fonts[2].c_str(), g.fonts[3].c_str());
        Encoding enc = g.encoding;

        auto ces = parseCommonEvents(readFile(basic / "CommonEvent.dat"));
        size_t lines = 0, used = 0;
        std::map<int, int> runs;
        for (auto &ce : ces) {
            count(ce.commands);
            lines += ce.commands.size();
            if (ce.commands.size() > 1) used++;
            runs[ce.runCondition]++;
        }
        std::printf("CommonEvent.dat: %zu events (%zu non-empty), %zu lines; run conditions:", ces.size(), used, lines);
        for (auto [k, v] : runs) std::printf(" %d=%d", k, v);
        std::printf("\n");

        for (const char *name : {"DataBase", "CDataBase", "SysDatabase"}) {
            try {
                auto db = parseDatabase(readFile(basic / (std::string(name) + ".project")),
                                        readFile(basic / (std::string(name) + ".dat")), enc);
                size_t rows = 0;
                for (auto &t : db.types) rows += t.rows.size();
                std::printf("%s: %zu types, %zu rows\n", name, db.types.size(), rows);
            } catch (const std::exception &e) {
                std::printf("%s: FAILED %s\n", name, e.what());
                failures++;
            }
        }
        auto ts = parseTilesets(readFile(basic / "TileSetData.dat"));
        std::printf("TileSetData.dat: %zu tilesets\n", ts.size());
    } catch (const std::exception &e) {
        std::printf("FAILED: %s\n", e.what());
        failures++;
    }

    size_t maps = 0, events = 0, pages = 0;
    for (auto &entry : fs::recursive_directory_iterator(data)) {
        auto ext = entry.path().extension().string();
        for (auto &c : ext) c = char(tolower(c));
        if (ext != ".mps") continue;
        try {
            auto m = parseMap(readFile(entry.path()));
            maps++;
            for (auto &ev : m.events) {
                events++;
                for (auto &pg : ev.pages) {
                    pages++;
                    count(pg.commands);
                }
            }
        } catch (const std::exception &e) {
            std::printf("map %s: FAILED %s\n", entry.path().filename().c_str(), e.what());
            failures++;
        }
    }
    std::printf("maps: %zu parsed, %zu events, %zu pages\n", maps, events, pages);
    std::printf("command codes:");
    for (auto [k, v] : codes) std::printf(" %d:%d", k, v);
    std::printf("\n");
    return failures ? 1 : 0;
}

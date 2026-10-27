// Reads Wolf RPG Editor save files.
//   wolfsave dump <SaveDataNN.sav>        prints every field, one per line, so two dumps diff cleanly
//   wolfsave check <file.sav>...          parses and re-serializes each file and compares the bytes
//   wolfsave decrypt <in.sav> <out.bin>   writes the file with the XOR layer removed
#include "../src/data.h"
#include "../src/reader.h"
#include "../src/savefile.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

using namespace wolf;

static std::vector<uint8_t> readFile(const char *path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error(std::string("cannot open ") + path);
    return {std::istreambuf_iterator<char>(f), {}};
}

namespace {

struct Printer {
    Encoding enc;
    std::string prefix;

    void line(const std::string &key, const std::string &value) const {
        std::printf("%s%s: %s\n", prefix.c_str(), key.c_str(), value.c_str());
    }
    void num(const std::string &key, long long v) const { line(key, std::to_string(v)); }
    void str(const std::string &key, const SaveString &s) const {
        line(key, "\"" + toUtf8(s.bytes, enc) + "\"" + (s.terminated ? "" : " (no NUL)"));
    }
    template <class C> void list(const std::string &key, const C &c) const {
        std::string v = "[" + std::to_string(c.size()) + "]";
        for (auto x : c) v += " " + std::to_string(x);
        line(key, v);
    }
    // Long lists print 20 values per line so that a change shows as a small diff.
    template <class C> void wrapped(const std::string &key, const C &c) const {
        num(key + ".size", c.size());
        for (size_t i = 0; i < c.size(); i += 20) {
            std::string v;
            for (size_t j = i; j < std::min(c.size(), i + 20); j++) v += (j > i ? " " : "") + std::to_string(c[j]);
            line(key + "[" + std::to_string(i) + "]", v);
        }
    }
    Printer sub(const std::string &name) const { return {enc, prefix + name + "."}; }
};

void dumpRoute(const Printer &p, const SaveRoute &r) {
    p.list("flags", r.flags);
    for (size_t i = 0; i < r.steps.size(); i++) {
        auto &s = r.steps[i];
        std::string v = std::to_string(s.id) + " args";
        for (auto a : s.args) v += " " + std::to_string(a);
        v += " extra";
        for (auto e : s.extra) v += " " + std::to_string(e);
        p.line("step[" + std::to_string(i) + "]", v);
    }
}

void dumpCharacter(const Printer &p, const SaveCharacter &c) {
    p.num("id", c.id);
    p.num("x", c.x);
    p.num("y", c.y);
    p.num("unknown3", c.unknown3);
    p.num("unknown4", c.unknown4);
    p.num("unknown5", c.unknown5);
    p.str("graphic", c.graphic);
    p.list("unknown6", c.unknown6);
    p.num("unknown7", c.unknown7);
    p.num("unknown8", c.unknown8);
    dumpRoute(p.sub("route"), c.route);
    dumpRoute(p.sub("route2"), c.route2);
    p.list("unknown9", c.unknown9);
    p.list("unknown10", c.unknown10);
    p.list("selfVariables", c.selfVariables);
    p.list("v70", c.v70);
    p.list("v73", c.v73);
    p.num("v78", c.v78);
    p.list("v85", c.v85);
    for (size_t i = 0; i < c.v8aStrings.size(); i++) p.str("v8aStrings[" + std::to_string(i) + "]", c.v8aStrings[i]);
    p.num("v8a1", c.v8a1);
    p.num("v8a2", c.v8a2);
    p.num("v8a3", c.v8a3);
    p.num("v8a4", c.v8a4);
    p.num("v8b", c.v8b);
    p.num("v8c", c.v8c);
}

void dump(const SaveFile &f) {
    Printer p{f.utf8() ? Encoding::UTF8 : Encoding::ShiftJIS, ""};
    p.list("header", f.header);
    p.str("title", f.title);
    p.num("version", f.version);

    auto m = p.sub("map");
    m.list("unknown1", f.map.unknown1);
    m.num("width", f.map.width);
    m.num("height", f.map.height);
    m.num("hasTiles", f.map.hasTiles);
    m.wrapped("tiles", f.map.tiles);
    m.str("unknown2", f.map.unknown2);
    for (size_t i = 0; i < f.map.strings.size(); i++) m.str("strings[" + std::to_string(i) + "]", f.map.strings[i]);
    m.wrapped("values", f.map.values);
    for (size_t i = 0; i < f.map.events.size(); i++)
        dumpCharacter(m.sub("events[" + std::to_string(i) + "]"), f.map.events[i]);
    m.list("v72", f.map.v72);
    m.wrapped("values2", f.map.values2);

    auto s = p.sub("system");
    auto &y = f.system;
    s.list("a", y.a);
    s.list("b", y.b);
    s.list("c", y.c);
    s.num("d", y.d);
    s.list("e", y.e);
    s.list("f", y.f);
    s.num("g", y.g);
    s.num("h", y.h);
    s.list("i", y.i);
    s.list("j", y.j);
    s.list("k", y.k);
    s.num("l", y.l);
    s.num("upTo96", y.upTo96);
    s.num("m", y.m);
    s.list("v98", y.v98);
    s.list("v100a", y.v100a);
    s.num("v100b", y.v100b);
    for (size_t i = 0; i < y.v100Strings.size(); i++) s.str("v100Strings[" + std::to_string(i) + "]", y.v100Strings[i]);
    s.wrapped("v100Values", y.v100Values);
    s.list("v100c", y.v100c);
    s.list("v101", y.v101);
    s.num("v102", y.v102);
    for (size_t i = 0; i < 2; i++) s.str("v103[" + std::to_string(i) + "]", y.v103[i]);
    s.num("v104", y.v104);
    s.list("v106", y.v106);
    s.str("v108s1", y.v108s1);
    s.list("v108a", y.v108a);
    s.str("v108s2", y.v108s2);
    s.list("v108b", y.v108b);
    for (size_t i = 0; i < 2; i++) s.str("v109s[" + std::to_string(i) + "]", y.v109s[i]);
    s.num("v109", y.v109);
    s.num("v110", y.v110);
    for (size_t i = 0; i < 3; i++) s.str("v119[" + std::to_string(i) + "]", y.v119[i]);
    s.num("v121", y.v121);
    s.num("v122", y.v122);
    s.num("v124", y.v124);
    s.num("v126", y.v126);
    s.num("v128", y.v128);
    s.list("v129", y.v129);
    s.num("v130", y.v130);
    s.num("v131", y.v131);
    s.list("v132", y.v132);
    s.num("v134", y.v134);
    s.num("v136", y.v136);
    s.list("v137", y.v137);
    s.list("v8a", y.v8a);
    s.str("v8aString", y.v8aString);
    for (size_t i = 0; i < y.v8aStrings.size(); i++) s.str("v8aStrings[" + std::to_string(i) + "]", y.v8aStrings[i]);
    s.list("v8d", y.v8d);
    s.wrapped("v8dBytes", y.v8dBytes);
    s.list("v8d2", y.v8d2);
    s.num("v8e", y.v8e);

    auto t = p.sub("section3");
    t.num("unknown1", f.section3.unknown1);
    for (size_t i = 0; i < f.section3.lists.size(); i++)
        for (size_t j = 0; j < f.section3.lists[i].size(); j++)
            t.list("lists[" + std::to_string(i) + "][" + std::to_string(j) + "]", f.section3.lists[i][j]);
    for (size_t i = 0; i < f.section3.numbers.size(); i++)
        t.wrapped("numbers[" + std::to_string(i) + "]", f.section3.numbers[i]);
    for (size_t i = 0; i < f.section3.strings.size(); i++)
        t.str("strings[" + std::to_string(i) + "]", f.section3.strings[i]);
    for (size_t i = 0; i < f.section3.numbers2.size(); i++)
        t.list("numbers2[" + std::to_string(i) + "]", f.section3.numbers2[i]);
    for (size_t i = 0; i < f.section3.strings2.size(); i++)
        for (size_t j = 0; j < f.section3.strings2[i].size(); j++)
            t.str("strings2[" + std::to_string(i) + "][" + std::to_string(j) + "]", f.section3.strings2[i][j]);

    auto q = p.sub("party");
    dumpCharacter(q.sub("hero"), f.party.hero);
    for (size_t i = 0; i < f.party.members.size(); i++)
        dumpCharacter(q.sub("members[" + std::to_string(i) + "]"), f.party.members[i]);
    q.list("unknown1", f.party.unknown1);
    q.wrapped("values", f.party.values);
    q.list("v8a", f.party.v8a);

    auto pp = p.sub("pictures");
    pp.num("count", f.pictures.count);
    for (size_t i = 0; i < f.pictures.pictures.size(); i++) {
        auto &pic = f.pictures.pictures[i];
        auto o = pp.sub("[" + std::to_string(i) + "]");
        o.num("unknown1", pic.unknown1);
        o.list("unknown2", pic.unknown2);
        o.num("unknown3", pic.unknown3);
        o.list("unknown4", pic.unknown4);
        o.str("file", pic.file);
        o.list("a", pic.a);
        o.list("b", pic.b);
        o.list("v69", pic.v69);
        o.num("v6b", pic.v6b);
        o.list("v72a", pic.v72a);
        o.list("v72b", pic.v72b);
        o.list("v73a", pic.v73a);
        o.list("v74", pic.v74);
        o.list("v75", pic.v75);
        o.list("v73b", pic.v73b);
        o.list("v76", pic.v76);
        o.list("v81", pic.v81);
        o.list("v87", pic.v87);
        for (size_t j = 0; j < pic.v89Lists.size(); j++) o.list("v89Lists[" + std::to_string(j) + "]", pic.v89Lists[j]);
        o.list("v89", pic.v89);
    }

    auto d = p.sub("variableDb");
    d.num("unknown", f.variableDb.unknown);
    for (size_t i = 0; i < f.variableDb.types.size(); i++) {
        auto &ty = f.variableDb.types[i];
        auto o = d.sub("types[" + std::to_string(i) + "]");
        o.num("head", ty.head);
        o.num("unknown", ty.unknown);
        o.list("fieldKinds", ty.fieldKinds);
        for (size_t r = 0; r < ty.rows.size(); r++) {
            auto ro = o.sub("rows[" + std::to_string(r) + "]");
            ro.list("numbers", ty.rows[r].numbers);
            for (size_t k = 0; k < ty.rows[r].strings.size(); k++)
                ro.str("strings[" + std::to_string(k) + "]", ty.rows[r].strings[k]);
        }
    }

    auto e = p.sub("section7");
    e.num("present", f.section7.present);
    for (size_t i = 0; i < f.section7.entries.size(); i++) {
        auto &en = f.section7.entries[i];
        e.line("entries[" + std::to_string(i) + "]",
               std::to_string(en.kind) + " " + std::to_string(en.kind < 0xFA ? en.small : en.value));
    }
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: wolfsave dump <file.sav> | check <file.sav>... | decrypt <in> <out>\n");
        return 2;
    }
    std::string cmd = argv[1];
    try {
        if (cmd == "dump") {
            dump(parseSave(readFile(argv[2])));
        } else if (cmd == "check") {
            int failed = 0;
            for (int i = 2; i < argc; i++) {
                try {
                    auto bytes = readFile(argv[i]);
                    auto again = serializeSave(parseSave(bytes));
                    bool same = again == bytes;
                    std::printf("%s %s (%zu bytes)\n", same ? "OK  " : "DIFF", argv[i], bytes.size());
                    failed += !same;
                } catch (const std::exception &e) {
                    std::printf("FAIL %s: %s\n", argv[i], e.what());
                    failed++;
                }
            }
            return failed ? 1 : 0;
        } else if (cmd == "decrypt" && argc >= 4) {
            auto bytes = readFile(argv[2]);
            cryptSave(bytes);
            std::ofstream(argv[3], std::ios::binary).write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        } else {
            std::fprintf(stderr, "unknown command %s\n", cmd.c_str());
            return 2;
        }
    } catch (const std::exception &e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}

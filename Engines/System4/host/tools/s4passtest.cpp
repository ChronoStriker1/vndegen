#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "game.h"

using namespace s4;

namespace {

int g_checks = 0, g_failed = 0;

void check(bool ok, const std::string &what) {
    g_checks++;
    if (ok) return;
    g_failed++;
    std::printf("FAIL %s\n", what.c_str());
}

struct Arg {
    bool isString;
    int32_t i = 0;
    std::string s;
};

int32_t call(const std::string &name, const std::vector<Arg> &args) {
    auto it = HllRegistry::table().find(name);
    if (it == HllRegistry::table().end()) {
        check(false, name + " isn't registered");
        return 0;
    }
    VM &vm = *game().vm;
    HllFunction decl;
    decl.name = name;
    HllCall c{vm, decl};
    for (const Arg &a : args) {
        c.argWord.push_back(int(c.words.size()));
        c.words.push_back(a.isString ? vm.newString(a.s) : a.i);
    }
    it->second(c);
    return c.ret;
}

std::string contents(const std::filesystem::path &p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: s4passtest <empty scratch folder>\n");
        return 2;
    }
    std::filesystem::path dir = argv[1], file = dir / "pass.asd";
    Game g(GameConfig{argv[1], argv[1]});
    g.vm = std::make_unique<VM>(Ain{});
    g.vm->rootDir = g.vm->saveDir = argv[1];

    call("PassRegister.SetFileName", {{false, 1}, {true, 0, "pass.asd"}});
    call("PassRegister.RegistText", {{false, 1}, {true, 0, "CG01"}});
    call("PassRegister.RegistNumber", {{false, 1}, {false, 7}});
    check(contents(file) == "N7\nTCG01\n", "the registered values are written, got \"" + contents(file) + "\"");

    std::filesystem::remove(file);
    call("PassRegister.RegistText", {{false, 1}, {true, 0, "CG01"}});
    call("PassRegister.RegistNumber", {{false, 1}, {false, 7}});
    call("PassRegister.UnregistText", {{false, 1}, {true, 0, "CG02"}});
    call("PassRegister.UnregistNumber", {{false, 1}, {false, 8}});
    check(!std::filesystem::exists(file), "registering what is already there, or removing what isn't, writes nothing");
    check(call("PassRegister.ExistText", {{false, 1}, {true, 0, "CG01"}}) == 1, "a registered text is still there");

    call("PassRegister.RegistText", {{false, 1}, {true, 0, "CG02"}});
    check(contents(file) == "N7\nTCG01\nTCG02\n", "a new text writes the whole set, got \"" + contents(file) + "\"");
    call("PassRegister.UnregistNumber", {{false, 1}, {false, 7}});
    check(contents(file) == "TCG01\nTCG02\n", "removing a number writes the set, got \"" + contents(file) + "\"");
    call("PassRegister.UnregistAll", {{false, 1}});
    check(contents(file).empty(), "removing everything writes an empty set");
    std::filesystem::remove(file);
    call("PassRegister.UnregistAll", {{false, 1}});
    check(!std::filesystem::exists(file), "removing everything from an empty set writes nothing");

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}

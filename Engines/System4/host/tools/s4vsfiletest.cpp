#include <csignal>
#include <cstdio>
#include <string>
#include <sys/resource.h>
#include <vector>

#include "game.h"

using namespace s4;

namespace {

int g_checks = 0, g_failed = 0;
int32_t g_refs = -1;

void check(bool ok, const std::string &what) {
    g_checks++;
    if (ok) return;
    g_failed++;
    std::printf("FAIL %s\n", what.c_str());
}

struct Arg {
    enum Kind { Int, Str, RefInt, RefStr } kind;
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
        switch (a.kind) {
            case Arg::Int: c.words.push_back(a.i); break;
            case Arg::Str: c.words.push_back(vm.newString(a.s)); break;
            case Arg::RefInt: c.words.push_back(g_refs); c.words.push_back(a.i); break;
            case Arg::RefStr: c.words.push_back(a.i); break;
        }
    }
    it->second(c);
    return c.ret;
}

void writeSlot(int32_t number, const std::string &text, bool close) {
    check(call("VSFile.OpenForWrite", {{Arg::Str, 0, "slot.vsf"}}) == 1, "OpenForWrite");
    call("VSFile.WriteInt", {{Arg::Int, number}});
    call("VSFile.WriteString", {{Arg::Str, 0, text}});
    if (close) call("VSFile.Close", {});
}

bool readSlot(int32_t &number, std::string &text) {
    VM &vm = *game().vm;
    int32_t textSlot = vm.newString();
    bool ok = call("VSFile.OpenForRead", {{Arg::Str, 0, "slot.vsf"}}) == 1 &&
              call("VSFile.ReadInt", {{Arg::RefInt, 0}}) == 1 &&
              call("VSFile.ReadString", {{Arg::RefStr, textSlot}}) == 1;
    call("VSFile.Close", {});
    number = vm.page(g_refs)->v[0];
    text = *vm.heap[textSlot].str;
    return ok;
}

void expectSlot(const std::string &when, int32_t wantNumber, const std::string &wantText) {
    int32_t number = 0;
    std::string text;
    bool ok = readSlot(number, text);
    check(ok, when + ": the slot details read back");
    check(number == wantNumber && text == wantText,
          when + ": got " + std::to_string(number) + " \"" + text + "\", want " + std::to_string(wantNumber) + " \"" +
              wantText + "\"");
}

}  // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: s4vsfiletest <empty scratch folder>\n");
        return 2;
    }
    Game g(GameConfig{argv[1], argv[1]});
    g.vm = std::make_unique<VM>(Ain{});
    g.vm->rootDir = g.vm->saveDir = argv[1];
    g_refs = g.vm->newPage(PageKind::Local, -1, 4);

    writeSlot(1, "Chapter 1", true);
    expectSlot("after a normal save", 1, "Chapter 1");

    std::signal(SIGXFSZ, SIG_IGN);
    rlimit old{};
    getrlimit(RLIMIT_FSIZE, &old);
    rlimit tight = old;
    tight.rlim_cur = 16;
    setrlimit(RLIMIT_FSIZE, &tight);
    writeSlot(2, "Chapter 2, a longer description than the disk has room for", true);
    setrlimit(RLIMIT_FSIZE, &old);
    expectSlot("after a write that ran out of room", 1, "Chapter 1");

    writeSlot(3, "Chapter 3", false);
    HllState::finishAll();
    expectSlot("after the game ended without Close", 3, "Chapter 3");

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}

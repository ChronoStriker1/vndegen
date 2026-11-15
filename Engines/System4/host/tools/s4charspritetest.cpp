#include <cstdio>
#include <string>
#include <vector>

#include "game.h"
#include "serial.h"

using namespace s4;

namespace {

int g_checks = 0, g_failed = 0;

void check(bool ok, const std::string &what) {
    g_checks++;
    if (ok) return;
    g_failed++;
    std::printf("FAIL %s\n", what.c_str());
}

int32_t call(const std::string &name, const std::vector<int32_t> &args) {
    auto it = HllRegistry::table().find(name);
    if (it == HllRegistry::table().end()) {
        check(false, name + " isn't registered");
        return 0;
    }
    HllFunction decl;
    decl.name = name;
    HllCall c{*game().vm, decl};
    for (int32_t a : args) {
        c.argWord.push_back(int(c.words.size()));
        c.words.push_back(a);
    }
    it->second(c);
    return c.ret;
}

int32_t create() { return call("StoatSpriteEngine.CharSpriteManager_CreateHandle", {}); }
void release(int32_t h) { call("StoatSpriteEngine.CharSpriteManager_ReleaseHandle", {h}); }

std::vector<int32_t> save() {
    VM &vm = *game().vm;
    int32_t slot = vm.newPage(PageKind::Array, -1, 0);
    check(call("StoatSpriteEngine.CharSpriteManager_Save", {slot}) == 1, "Save succeeds");
    return vm.page(slot)->v;
}

bool load(const std::vector<int32_t> &data) {
    VM &vm = *game().vm;
    int32_t slot = vm.newPage(PageKind::Array, -1, 0);
    vm.page(slot)->v = data;
    return call("StoatSpriteEngine.CharSpriteManager_Load", {slot}) == 1;
}

int32_t savedCount(const std::vector<int32_t> &data) {
    std::vector<int32_t> head;
    serial::putTag(head, "CSM");
    return data.size() > head.size() + 1 ? data[head.size() + 1] : -1;
}

std::vector<int32_t> pcSave(int32_t count, const std::vector<int32_t> &live) {
    std::vector<int32_t> out;
    serial::putTag(out, "CSM");
    out.push_back(0);
    out.push_back(count);
    for (int32_t h = 0; h < count; h++) {
        bool isLive = false;
        for (int32_t l : live) isLive = isLive || l == h;
        if (!isLive) { out.push_back(0); continue; }
        out.push_back(1);
        serial::putString(out, "A");
        for (int32_t v : {0, 16, 255, 255, 255, 255, 0, 0, 0, 0, 0, 255, 10, 20, 5, 255, 1, 2000 + h}) out.push_back(v);
    }
    return out;
}

}  // namespace

int main() {
    Game g(GameConfig{".", "."});
    g.vm = std::make_unique<VM>(Ain{});

    int32_t a = create(), b = create(), c = create();
    check(a == 1 && b == 2 && c == 3, "the first handles are 1, 2, 3, got " + std::to_string(a) + ", " +
                                          std::to_string(b) + ", " + std::to_string(c));
    release(b);
    int32_t again = create();
    check(again == 2, "a released handle is given out again, got " + std::to_string(again));

    std::vector<int32_t> many;
    for (int k = 0; k < 1000; k++) many.push_back(create());
    for (int32_t h : many) release(h);
    int32_t n = savedCount(save());
    check(n == 4, "a save holds handles up to the highest live one, 1 to 3: count " + std::to_string(n) + ", want 4");

    release(a);
    release(again);
    release(c);
    n = savedCount(save());
    check(n == 1, "a save with no live handle has a count of 1, got " + std::to_string(n));

    check(load(pcSave(620, {33, 34})), "a PC save with 620 slots and handles 33 and 34 loads");
    int32_t first = create();
    check(first == 1, "after that load the lowest free handle is given out, got " + std::to_string(first));
    std::vector<int32_t> back = save();
    n = savedCount(back);
    check(n == 35, "that state saves 35 slots, got " + std::to_string(n));
    check(load(back), "VNDegen's own save loads back");
    int32_t next = create();
    check(next == 2, "and gives out the next free handle, got " + std::to_string(next));

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}

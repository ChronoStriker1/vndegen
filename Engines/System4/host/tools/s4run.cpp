// Runs a System 4 game's bytecode headless, logging what it does, for a number of frames.
// Usage: s4run <game folder> [frames]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "ain.h"
#include "sjis.h"
#include "vm.h"

namespace fs = std::filesystem;
using namespace s4;

struct StopRun {};

int main(int argc, char **argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: s4run <game folder> [frames]\n"); return 2; }
    fs::path root = argv[1];
    long maxFrames = argc > 2 ? std::atol(argv[2]) : 600;
    fs::path ainPath;
    for (auto &e : fs::directory_iterator(root))
        if (e.path().extension() == ".ain" || e.path().extension() == ".AIN") ainPath = e.path();
    Ain ain;
    std::string err = ain.load(ainPath.string());
    if (!err.empty()) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    VM vm(std::move(ain));
    vm.saveDir = (root / "SaveData").string();
    vm.gameName = ainPath.stem().string();
    vm.log = [](const std::string &s) { std::printf("log: %s\n", s.c_str()); };
    if (std::getenv("S4TRACE")) vm.trace = [](const std::string &s) { std::printf("call %s\n", s.c_str()); };
    long frames = 0;
    uint32_t clock = 0;
    vm.timeMs = [&] { return clock; };
    vm.peek = [&] {
        clock += 16;
        if (++frames >= maxFrames) throw StopRun{};
    };
    try {
        vm.start();
        int32_t r = vm.call(vm.ain.mainFunction, {});
        std::printf("main returned %d\n", r);
    } catch (const VMExit &e) {
        std::printf("exit %d: %s\n", e.code, e.reason.c_str());
    } catch (const StopRun &) {
        std::printf("stopped after %ld frames in %s\n", frames, vm.frames.empty() ? "?" : vm.functionName(vm.frames.back().function).c_str());
    }
    std::printf("heap slots in use: %zu\n", vm.heap.size() - vm.freeSlots.size());
    return 0;
}

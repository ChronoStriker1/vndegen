// system.* calls. Numbers 0 to 23 are the order of the SDK compiler's system function table; later ones were
// read from how the games call them (see docs/system4-cleanroom.md).
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>

#include "save.h"
#include "sjis.h"
#include "vm.h"

namespace s4 {

namespace {
int lockCount = 0;
}

void VM::callSys(int32_t n) {
    auto popStr = [&]() { int32_t s = pop(); std::string v = str(s); decref(s); return v; };
    switch (n) {
    case 0:  // Exit(int)
        throw VMExit{pop(), "system.Exit"};
    case 1: {  // GlobalSave(key, file) -> int
        std::string file = popStr(), key = popStr();
        push(save::globalSave(*this, key, file, ""));
        break;
    }
    case 2: {  // GlobalLoad(key, file) -> int
        std::string file = popStr(), key = popStr();
        push(save::globalLoad(*this, key, file, ""));
        break;
    }
    case 3: push(++lockCount); break;  // LockPeek
    case 4: push(lockCount > 0 ? --lockCount : 0); break;  // UnlockPeek
    case 5: throw VMExit{0, "system.Reset"};
    case 6: case 7: case 15: {  // Output, MsgBox, Error: log, return the text
        int32_t s = pop();
        if (log) log((n == 15 ? "error: " : n == 7 ? "message box: " : "") + sjis::toUtf8(str(s)));
        push(s);
        break;
    }
    case 8: {  // ResumeSave(key, file, ref int result) -> int: 1 after saving, 0 when execution resumes from a load
        int32_t ri = pop(), rp = pop();
        std::string file = popStr(), key = popStr();
        std::vector<uint8_t> snap = resumeSnapshot(key, ip);
        bool ok = save::writeGD(save::hostPath(*this, file, true), snap, saveFormat.xorData);
        if (log) log(std::string(ok ? "resume saved: " : "resume save failed: ") + sjis::toUtf8(file));
        if (Page *p = page(rp); p && ri >= 0 && size_t(ri) < p->v.size()) p->v[ri] = ok;
        push(1);
        break;
    }
    case 9: {  // ResumeLoad(key, file): on success execution continues where the save was made
        std::string file = popStr(), key = popStr();
        std::vector<uint8_t> d;
        std::string err;
        if (!save::readGD(save::hostPath(*this, file, true), d)) err = "can't read the save";
        else if (resumeRestore(d, key, err)) throw ResumeJump{};
        if (log) log("resume load failed: " + sjis::toUtf8(file) + ": " + err);
        break;
    }
    case 10: case 16: {  // ExistFile(name), ExistSaveFile(name) -> int
        std::string name = popStr();
        push(save::exists(*this, name, n == 16));
        break;
    }
    case 11: popStr(); break;  // OpenWeb
    case 12: push(newString(saveFolderName)); break;  // GetSaveFolderName: relative, resolved by save::hostPath
    case 13: push(int32_t(timeMs ? timeMs() : 0)); break;  // GetTime
    case 14: push(newString(gameName)); break;  // GetGameName
    case 17: push(0); break;  // IsDebugMode
    case 18: { int32_t s = pop(); decref(s); push(1); break; }  // MsgBoxOkCancel: OK
    case 19: {  // GetFuncStackName(index)
        int32_t i = pop();
        push(newString(i >= 0 && size_t(i) < frames.size() ? ain.functions[frames[frames.size() - 1 - i].function].name : ""));
        break;
    }
    case 20: if (peek) peek(); break;  // Peek
    case 21: {  // Sleep(ms)
        int32_t ms = pop();
        if (peek) {
            uint32_t until = (timeMs ? timeMs() : 0) + uint32_t(std::max(0, ms));
            do peek(); while (timeMs && int32_t(timeMs() - until) < 0);
        }
        break;
    }
    case 22: case 23: {  // ResumeWriteComment, ResumeReadComment(key, file, ref array@string) -> bool
        pop(); popStr(); popStr();
        push(0);
        break;
    }
    case 24: case 25: {  // GroupSave, GroupLoad(key, file, group, ref int count) -> int
        int32_t ci = pop(), cp = pop();
        std::string group = popStr(), file = popStr(), key = popStr();
        int32_t count = 0;
        int32_t r = n == 24 ? save::globalSave(*this, key, file, group, &count) : save::globalLoad(*this, key, file, group, &count);
        if (Page *p = page(cp); p && ci >= 0 && size_t(ci) < p->v.size()) p->v[ci] = count;
        push(r);
        break;
    }
    case 26: push(save::remove(*this, popStr())); break;  // DeleteSaveFile(name) -> int
    case 28: {  // CopySaveFile(dst, src) -> int
        std::string src = popStr(), dst = popStr();
        push(save::copy(*this, dst, src));
        break;
    }
    default:
        fatal("unimplemented system call " + std::to_string(n));
    }
}

}  // namespace s4

// Save files, in the PC engine's formats so saves move between VNDegen and Windows both ways.
// gloval.sav: for each variable from the global border up, a little-endian int32 then a NUL-terminated string.
#include <sys/stat.h>

#include <ctime>

#include "engine.h"

namespace vnns {

static std::string joinPath(const std::string &a, const std::string &b) { return a.empty() ? b : a + "\\" + b; }

std::string Engine::savePath(int slot) const {
    return fs_.hostPath(joinPath(saveDir_, "save" + std::to_string(slot) + ".dat"));
}

void Engine::loadGlobals() {
    if (!globalOn_) return;
    Bytes data;
    if (!readFile(fs_.hostPath(joinPath(saveDir_, "gloval.sav")), data)) return;
    size_t p = 0;
    for (size_t i = size_t(globalBorder_); i < num_.size() && p + 4 <= data.size(); i++) {
        num_[i] = int32_t(le32(&data[p]));
        p += 4;
        size_t end = p;
        while (end < data.size() && data[end]) end++;
        str_[i].assign(reinterpret_cast<const char *>(&data[p]), end - p);
        p = end + 1;
    }
}

void Engine::saveGlobals() {
    if (!globalOn_) return;
    Bytes out;
    for (size_t i = size_t(globalBorder_); i < num_.size(); i++) {
        uint32_t v = uint32_t(num_[i]);
        for (int k = 0; k < 4; k++) out.push_back(uint8_t(v >> (8 * k)));
        out.insert(out.end(), str_[i].begin(), str_[i].end());
        out.push_back(0);
    }
    std::string path = fs_.hostPath(joinPath(saveDir_, "gloval.sav"));
    if (!writeFile(path, out.data(), out.size())) logf("could not write %s", path.c_str());
}

void Engine::saveSystemData() { saveGlobals(); }

void Engine::noteTextLineStart() {}

bool Engine::saveGame(int slot, const std::string &note) {
    (void)note;
    logf("savegame %d: not implemented yet", slot);
    return false;
}

bool Engine::loadGame(int slot) {
    logf("loadgame %d: not implemented yet", slot);
    return false;
}

bool Engine::readSaveInfo(int slot, SaveInfo &info) {
    struct stat st;
    if (stat(savePath(slot).c_str(), &st) != 0) return false;
    struct tm lt;
    time_t t = st.st_mtime;
    localtime_r(&t, &lt);
    info.month = lt.tm_mon + 1;
    info.day = lt.tm_mday;
    info.hour = lt.tm_hour;
    info.minute = lt.tm_min;
    return true;
}

namespace {
using E = Engine;

void cmdSaveGame(E &e, const std::string &name) {
    int slot = e.readInt();
    std::string note;
    if (name == "savegame2" && e.readComma()) note = e.readStr();
    e.saveGame(slot, note);
}

void cmdLoadGame(E &e, const std::string &) { e.loadGame(e.readInt()); }
void cmdSaveOn(E &e, const std::string &name) { e.saveOn_ = name == "saveon"; }

void cmdSaveFileExist(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    E::SaveInfo info;
    e.setVar(v, e.readSaveInfo(e.readInt(), info) ? 1 : 0);
}

void cmdSaveTime(E &e, const std::string &) {
    int slot = e.readInt();
    E::SaveInfo info;
    bool ok = e.readSaveInfo(slot, info);
    int vals[4] = {ok ? info.month : 0, info.day, info.hour, info.minute};
    for (int i = 0; i < 4 && e.readComma(); i++) e.setVar(e.readVar(), vals[i]);
}

void cmdGetSaveStr(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    E::SaveInfo info;
    e.readSaveInfo(e.readInt(), info);
    e.setVar(v, info.note);
}

void cmdScreenshot(E &e, const std::string &) { e.skipStatement(); }

}  // namespace

void registerSaveCommands(std::unordered_map<std::string, CommandFn> &t) {
    t["savegame"] = cmdSaveGame;
    t["savegame2"] = cmdSaveGame;
    t["loadgame"] = cmdLoadGame;
    t["saveon"] = cmdSaveOn;
    t["saveoff"] = cmdSaveOn;
    t["savepoint"] = [](E &, const std::string &) {};
    t["savefileexist"] = cmdSaveFileExist;
    t["savetime"] = cmdSaveTime;
    t["getsavestr"] = cmdGetSaveStr;
    for (const char *n : {"getscreenshot", "savescreenshot", "savescreenshot2", "deletescreenshot"}) t[n] = cmdScreenshot;
}

}  // namespace vnns

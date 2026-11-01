#include "vns4.h"

#include <exception>
#include <filesystem>
#include <map>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

#include "game.h"
#include "sjis.h"

struct VNS4Engine {
    std::unique_ptr<s4::Game> game;
    std::vector<uint8_t> frame;
    std::string caption, problem, cheatState;
};

namespace {
std::string g_startError;

// vns4_compatibility_note's answers by code file, kept while the file's size and time are unchanged: the game page asks
// first, off the main thread, so a launch right after it doesn't parse the file again.
struct NoteCache {
    std::mutex mu;
    struct Entry { uintmax_t size; std::filesystem::file_time_type time; std::string note; };
    std::map<std::string, Entry> byPath;
};
NoteCache &noteCache() {
    static NoteCache c;
    return c;
}
}

extern "C" {

VNS4Engine *vns4_start(const VNS4Launch *l) {
    g_startError.clear();
    if (!l || !l->gameRoot) { g_startError = "No game folder."; return nullptr; }
    s4::GameConfig cfg;
    cfg.root = l->gameRoot;
    cfg.saveDir = l->saveDir ? l->saveDir : cfg.root + "/SaveData";
    if (l->logPath) cfg.logPath = l->logPath;
    if (l->fontsDir) cfg.fontsDir = l->fontsDir;
    // Nothing may throw into the caller: damaged game files end here as a start error.
    auto e = std::make_unique<VNS4Engine>();
    try {
        e->game = std::make_unique<s4::Game>(cfg);
        if (!e->game->start(g_startError)) return nullptr;
        e->caption = s4::sjis::toUtf8(e->game->vm->gameName);
    } catch (const std::exception &x) {
        g_startError = std::string("The game's files couldn't be read: ") + x.what();
        return nullptr;
    }
    return e.release();
}

const char *vns4_compatibility_note(const char *gameRoot) {
    static thread_local std::string note;
    note.clear();
    if (!gameRoot) return nullptr;
    try {
        std::string root = gameRoot;
        std::string path = s4::codeFile(root, s4::readIni(root));
        if (path.empty()) return nullptr;
        std::error_code ec;
        uintmax_t size = std::filesystem::file_size(path, ec);
        auto time = std::filesystem::last_write_time(path, ec);
        auto &cache = noteCache();
        {
            std::lock_guard<std::mutex> lock(cache.mu);
            auto it = cache.byPath.find(path);
            if (!ec && it != cache.byPath.end() && it->second.size == size && it->second.time == time) {
                note = it->second.note;
                return note.empty() ? nullptr : note.c_str();
            }
        }
        s4::Ain ain;
        if (!ain.load(path).empty()) return nullptr;
        note = s4::compatibilityNote(ain);
        if (!ec) {
            std::lock_guard<std::mutex> lock(cache.mu);
            cache.byPath[path] = {size, time, note};
        }
    } catch (const std::exception &) {
        return nullptr;  // starting the game reports the damage
    }
    return note.empty() ? nullptr : note.c_str();
}

const char *vns4_start_error(void) { return g_startError.empty() ? nullptr : g_startError.c_str(); }
bool vns4_step(VNS4Engine *e, int ms) { return e && e->game->step(ms); }

const uint8_t *vns4_frame(VNS4Engine *e, int *w, int *h) {
    if (!e) return nullptr;
    e->game->takeFrame(e->frame, *w, *h);
    return e->frame.data();
}

const char *vns4_caption(VNS4Engine *e) { return e ? e->caption.c_str() : ""; }
void vns4_audio_render(VNS4Engine *e, float *out, int frames) { if (e) e->game->audio.render(out, frames); }
void vns4_key(VNS4Engine *e, int vk, bool down) { if (e) e->game->key(vk, down); }
void vns4_mouse_move(VNS4Engine *e, int x, int y) { if (e) e->game->mouseMove(x, y); }
void vns4_wheel(VNS4Engine *e, int n) { if (e) e->game->wheel(n); }
void vns4_text(VNS4Engine *e, const char *utf8) { if (e && utf8) e->game->typed(s4::sjis::fromUtf8(utf8)); }
void vns4_set_paused(VNS4Engine *e, bool p) { if (e) e->game->setPaused(p); }
uint32_t vns4_frames_done(VNS4Engine *e) { return e ? e->game->yields.load() : 0; }
void vns4_stop(VNS4Engine *e) { delete e; }

void vns4_debug_dump_sprites(VNS4Engine *e) { if (e) e->game->dumpSpritesOnNextFrame(); }
void vns4_debug_call(VNS4Engine *e, const char *utf8Name) { if (e) e->game->debugCall(s4::sjis::fromUtf8(utf8Name)); }

void vns4_set_speed(VNS4Engine *e, int speed) { if (e) e->game->setSpeed(speed); }

const char *vns4_cheat_state(VNS4Engine *e) {
    if (!e) return nullptr;
    e->cheatState = e->game->cheatState();
    return e->cheatState.empty() ? nullptr : e->cheatState.c_str();
}

int vns4_cheat_set_global(VNS4Engine *e, int index, int32_t value) { return e ? e->game->cheatSetGlobal(index, value) : VNS4_CHEAT_BUSY; }
bool vns4_cheat_add_gold(VNS4Engine *e, int amount) { return e && e->game->cheatAddGold(amount); }
bool vns4_cheat_has_gold(VNS4Engine *e) { return e && e->game->hasGold(); }

const char *vns4_problem(VNS4Engine *e) {
    if (!e) return nullptr;
    e->problem = e->game->lastProblem();
    return e->problem.empty() ? nullptr : e->problem.c_str();
}

}  // extern "C"

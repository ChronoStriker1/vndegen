#include "vns4.h"

#include <string>
#include <vector>

#include "game.h"
#include "sjis.h"

struct VNS4Engine {
    std::unique_ptr<s4::Game> game;
    std::vector<uint8_t> frame;
    std::string caption, problem;
};

namespace {
std::string g_startError;
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
    auto e = new VNS4Engine;
    e->game = std::make_unique<s4::Game>(cfg);
    if (!e->game->start(g_startError)) {
        delete e;
        return nullptr;
    }
    e->caption = s4::sjis::toUtf8(e->game->vm->gameName);
    return e;
}

const char *vns4_unsupported(const char *gameRoot) {
    static thread_local std::string reason;
    reason.clear();
    if (!gameRoot) return nullptr;
    std::string root = gameRoot;
    std::string path = s4::codeFile(root, s4::readIni(root));
    if (path.empty()) return nullptr;
    s4::Ain ain;
    if (!ain.load(path).empty()) return nullptr;
    reason = s4::unsupportedReason(ain);
    return reason.empty() ? nullptr : reason.c_str();
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
void vns4_set_paused(VNS4Engine *e, bool p) { if (e) e->game->setPaused(p); }
void vns4_stop(VNS4Engine *e) { delete e; }

void vns4_debug_dump_sprites(VNS4Engine *e) { if (e) e->game->dumpSpritesOnNextFrame(); }
void vns4_debug_call(VNS4Engine *e, const char *utf8Name) { if (e) e->game->debugCall(s4::sjis::fromUtf8(utf8Name)); }

const char *vns4_problem(VNS4Engine *e) {
    if (!e || e->game->lastProblem.empty()) return nullptr;
    e->problem = e->game->lastProblem;
    return e->problem.c_str();
}

}  // extern "C"

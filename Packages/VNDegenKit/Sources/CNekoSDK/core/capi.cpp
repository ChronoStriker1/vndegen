#include <dlfcn.h>

#include <algorithm>
#include <string>

#include "engine.h"
#include "tables.h"
#include "vnneko.h"

using neko::Engine;

struct VNNekoEngine {
    Engine engine;
};

static std::string startError;

extern "C" {

const VNNekoMovies *vnneko_find_movies(void) {
    static VNNekoMovies movies;
    static bool found = false;
    if (found) return &movies;
    void *pointers[5];
    const char *names[5] = {"vnffmovie_open", "vnffmovie_frame", "vnffmovie_audio", "vnffmovie_seek", "vnffmovie_close"};
    for (int i = 0; i < 5; i++)
        if (!(pointers[i] = dlsym(RTLD_DEFAULT, names[i]))) {
            startError = std::string("no ") + names[i];
            return nullptr;
        }
    movies.open = reinterpret_cast<decltype(movies.open)>(pointers[0]);
    movies.frame = reinterpret_cast<decltype(movies.frame)>(pointers[1]);
    movies.audio = reinterpret_cast<decltype(movies.audio)>(pointers[2]);
    movies.seek = reinterpret_cast<decltype(movies.seek)>(pointers[3]);
    movies.close = reinterpret_cast<decltype(movies.close)>(pointers[4]);
    found = true;
    return &movies;
}

VNNekoEngine *vnneko_start(const VNNekoLaunch *launch) {
    auto e = std::make_unique<VNNekoEngine>();
    startError.clear();
    if (launch->movies) e->engine.setMovies(*launch->movies);
    if (!e->engine.start(launch->gameRoot, launch->dataDir ? launch->dataDir : "", launch->logPath ? launch->logPath : "", launch->fontDir ? launch->fontDir : "", startError)) return nullptr;
    return e.release();
}
const char *vnneko_start_error(void) { return startError.c_str(); }
bool vnneko_step(VNNekoEngine *e, int ms) { return e->engine.step(ms); }
const uint8_t *vnneko_frame(VNNekoEngine *e, int *width, int *height, bool *changed) {
    bool c = false;
    const uint8_t *px = e->engine.frame(*width, *height, c);
    if (changed) *changed = c;
    return px;
}
void vnneko_audio_render(VNNekoEngine *e, float *out, int frames) { e->engine.audio().render(out, frames); }
void vnneko_mouse_move(VNNekoEngine *e, float x, float y) { e->engine.mouseMove(x, y); }
void vnneko_mouse_button(VNNekoEngine *e, int button, bool down) { e->engine.mouseButton(button, down); }
void vnneko_mouse_wheel(VNNekoEngine *e, int notches) { e->engine.mouseWheel(notches); }
bool vnneko_move_focus(VNNekoEngine *e, int dx, int dy) { return e->engine.moveFocus(dx, dy); }
const char *vnneko_user_data_directory(const char *gameRoot) {
    static std::string answer;
    answer.clear();
    neko::Files files;
    if (!files.open(gameRoot)) return nullptr;
    neko::Bytes init = files.read("cfg\\init.ini");
    if (!init) return nullptr;
    neko::Config config;
    config.parse(*init);
    answer = config.text("UserDataDirectory");
    std::replace(answer.begin(), answer.end(), '\\', '/');
    return answer.c_str();
}
bool vnneko_is_supported(const char *gameRoot) {
    neko::Files files;
    if (!files.open(gameRoot)) return false;
    neko::Bytes init = files.read("cfg\\init.ini");
    if (!init) return false;
    neko::Config config;
    config.parse(*init);
    return neko::menuTable(neko::normalizedPath(config.text("UserDataDirectory"))) != nullptr;
}
int64_t vnneko_debug_file_size(const char *gameRoot, const char *path) {
    neko::Files files;
    return files.open(gameRoot) ? files.storedSize(path) : -1;
}
void vnneko_set_skip(VNNekoEngine *e, bool on) { e->engine.setSkip(on); }
void vnneko_set_paused(VNNekoEngine *e, bool paused) { e->engine.setPaused(paused); }
void vnneko_stop(VNNekoEngine *e) { delete e; }
const char *vnneko_problem(VNNekoEngine *e) { return e->engine.problem(); }
const char *vnneko_debug_status(VNNekoEngine *e) { return e->engine.debugStatus(); }
bool vnneko_debug_waiting_for_click(VNNekoEngine *e) { return e->engine.waitingForClick(); }
int vnneko_debug_text_count(VNNekoEngine *e) { return e->engine.textCount(); }
bool vnneko_debug_press(VNNekoEngine *e, const char *action) { return e->engine.pressAction(action); }
int vnneko_debug_choice_count(VNNekoEngine *e) { return e->engine.choiceCount(); }
void vnneko_debug_choose(VNNekoEngine *e, int index) { e->engine.choose(index); }
bool vnneko_debug_jump(VNNekoEngine *e, const char *script, int node) { return e->engine.jump(script, node); }

}

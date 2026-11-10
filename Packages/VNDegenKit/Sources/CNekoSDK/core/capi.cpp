#include <dlfcn.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "engine.h"
#include "image.h"
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
bool vnneko_debug_art_pixels(const char *gameRoot, const char *path, const char *artSet, uint32_t out[3]) {
    neko::Files files;
    if (!files.open(gameRoot)) return false;
    neko::Images images(files);
    // With the decode thread busy on it first, as when a scene prefetches its pictures.
    images.prefetch({{path, 0}});
    const char *sets[3] = {"", artSet, ""};
    for (int i = 0; i < 3; i++) {
        images.setArtSet(sets[i]);
        neko::ImageRef image = images.get(path);
        if (!image || image->rgba.size() < 4) return false;
        out[i] = uint32_t(image->rgba[0]) << 24 | uint32_t(image->rgba[1]) << 16 | uint32_t(image->rgba[2]) << 8 | image->rgba[3];
    }
    return true;
}
int64_t vnneko_debug_file_size(const char *gameRoot, const char *path) {
    neko::Files files;
    return files.open(gameRoot) ? files.storedSize(path) : -1;
}
bool vnneko_debug_saved_name(const char *gameRoot, const char *path, char *out, size_t size) {
    neko::Files files;
    if (!files.open(gameRoot) || !size) return false;
    std::snprintf(out, size, "%s", files.savedName(path).c_str());
    return true;
}
int vnneko_debug_variable(VNNekoEngine *e, int scope, int number) { return e->engine.variable(scope, number); }
bool vnneko_debug_sound_playing(VNNekoEngine *e, int kind, int channel) { return e->engine.soundPlaying(kind, channel); }
size_t vnneko_debug_rebuild_save(const char *userDataDirectory, const uint8_t *plain, size_t size, uint8_t *out, size_t outSize) {
    const neko::SaveLayout *layout = neko::saveLayout(userDataDirectory);
    neko::SaveData data;
    if (!layout || !neko::parseSave(std::vector<uint8_t>(plain, plain + size), *layout, data)) return 0;
    std::vector<uint8_t> built = neko::buildSave(data, *layout);
    std::copy_n(built.begin(), std::min(built.size(), outSize), out);
    return built.size();
}
int vnneko_debug_macro_number(const char *gameRoot, int scope, int index) {
    neko::Files files;
    if (!files.open(gameRoot)) return -1;
    neko::Bytes data = files.read("cfg\\macrolist.ini");
    if (!data) return -1;
    auto lists = neko::parseMacroList(*data);
    const auto &list = scope == 1 ? lists.scenario : lists.global;
    return index >= 0 && index < int(list.size()) ? list[size_t(index)] : -1;
}
void vnneko_set_skip(VNNekoEngine *e, bool on) { e->engine.setSkip(on); }
void vnneko_set_paused(VNNekoEngine *e, bool paused) { e->engine.setPaused(paused); }
void vnneko_set_window_state(VNNekoEngine *e, bool canFullScreen, bool fullScreen) { e->engine.setWindowState(canFullScreen, fullScreen); }
void vnneko_set_view_size(VNNekoEngine *e, float width, float height) { e->engine.setViewSize(width, height); }
int vnneko_take_window_request(VNNekoEngine *e) { return e->engine.takeWindowRequest(); }
bool vnneko_pauses_when_inactive(VNNekoEngine *e) { return e->engine.pausesWhenInactive(); }
int vnneko_confirm_key(VNNekoEngine *e) { return e->engine.confirmKey(); }
void vnneko_stop(VNNekoEngine *e) { delete e; }
const char *vnneko_problem(VNNekoEngine *e) { return e->engine.problem(); }
const char *vnneko_debug_status(VNNekoEngine *e) { return e->engine.debugStatus(); }
bool vnneko_debug_waiting_for_click(VNNekoEngine *e) { return e->engine.waitingForClick(); }
int vnneko_debug_text_count(VNNekoEngine *e) { return e->engine.textCount(); }
int vnneko_debug_text_advance(VNNekoEngine *e, uint32_t codepoint) { return e->engine.debugTextAdvance(codepoint); }
bool vnneko_debug_press(VNNekoEngine *e, const char *action) { return e->engine.pressAction(action); }
int vnneko_debug_choice_count(VNNekoEngine *e) { return e->engine.choiceCount(); }
void vnneko_debug_choose(VNNekoEngine *e, int index) { e->engine.choose(index); }
bool vnneko_debug_jump(VNNekoEngine *e, const char *script, int node) { return e->engine.jump(script, node); }
int vnneko_take_comment_request(VNNekoEngine *e, char *utf8, size_t size) {
    std::string comment;
    int slot = e->engine.takeCommentRequest(comment);
    if (slot && utf8 && size) std::snprintf(utf8, size, "%s", comment.c_str());
    return slot;
}
bool vnneko_set_comment(VNNekoEngine *e, int slot, const char *utf8) { return e->engine.setComment(slot, utf8 ? utf8 : ""); }
bool vnneko_take_host_request(VNNekoEngine *e, char *kind, size_t kindSize, char *payload, size_t payloadSize) {
    std::string k, p;
    if (!e->engine.takeHostRequest(k, p)) return false;
    if (kind && kindSize) std::snprintf(kind, kindSize, "%s", k.c_str());
    if (payload && payloadSize) std::snprintf(payload, payloadSize, "%s", p.c_str());
    return true;
}
void vnneko_debug_table_prefix(const char *text) { neko::menuTablePrefix() = text ? text : ""; }
void vnneko_debug_host_taps(VNNekoEngine *e, bool on) { e->engine.setHostTaps(on); }
int vnneko_debug_choice_result(VNNekoEngine *e) { return e->engine.choiceResult(); }
bool vnneko_debug_choice_kept(VNNekoEngine *e) { return e->engine.choiceKept(); }
bool vnneko_debug_choice_line(VNNekoEngine *e) { return e->engine.choiceLine(); }
bool vnneko_debug_save(VNNekoEngine *e, int slot) { return e->engine.saveToSlot(slot); }
bool vnneko_debug_load(VNNekoEngine *e, int slot) { return e->engine.loadFromSlot(slot); }
void vnneko_debug_toggle_lock(VNNekoEngine *e, int slot) { e->engine.toggleLock(slot); }

}

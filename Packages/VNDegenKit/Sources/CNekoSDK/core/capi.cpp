#include <string>

#include "engine.h"
#include "vnneko.h"

using neko::Engine;

struct VNNekoEngine {
    Engine engine;
};

static std::string startError;

extern "C" {

VNNekoEngine *vnneko_start(const VNNekoLaunch *launch) {
    auto e = std::make_unique<VNNekoEngine>();
    startError.clear();
    if (!e->engine.start(launch->gameRoot, launch->logPath ? launch->logPath : "", launch->fontDir ? launch->fontDir : "", startError)) return nullptr;
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

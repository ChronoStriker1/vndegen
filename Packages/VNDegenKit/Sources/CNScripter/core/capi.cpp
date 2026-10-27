// vnns.h, the C interface, over the Engine class.
#include <algorithm>
#include <cmath>

#include "../include/vnns.h"
#include "engine.h"

using vnns::Engine;

struct VNNSEngine {
    std::unique_ptr<Engine> engine;
    std::string caption;
};

static std::string gStartError;

extern "C" {

VNNSEngine *vnns_start(const VNNSLaunch *launch) {
    gStartError.clear();
    if (!launch || !launch->gameRoot) {
        gStartError = "No game folder was given.";
        return nullptr;
    }
    vnns::setLogFile(launch->logPath);
    Engine::Config cfg;
    cfg.gameRoot = launch->gameRoot;
    cfg.saveRoot = launch->gameRoot;
    cfg.englishWordWrap = launch->englishWordWrap;
    auto engine = std::make_unique<Engine>(cfg);
    std::string error;
    if (!engine->start(&error)) {
        gStartError = error.empty() ? "The game could not be started." : error;
        vnns::logf("start failed: %s", gStartError.c_str());
        return nullptr;
    }
    auto *e = new VNNSEngine;
    e->engine = std::move(engine);
    return e;
}

const char *vnns_start_error(void) { return gStartError.empty() ? nullptr : gStartError.c_str(); }

bool vnns_step(VNNSEngine *e, int ms) { return e->engine->step(std::clamp(ms, 0, 1000)); }

const uint8_t *vnns_frame(VNNSEngine *e, int *width, int *height, bool *changed) {
    const vnns::Image &f = e->engine->frame();
    if (width) *width = f.w;
    if (height) *height = f.h;
    if (changed) *changed = e->engine->frameChanged();
    return reinterpret_cast<const uint8_t *>(f.px.data());
}

const char *vnns_caption(VNNSEngine *e) {
    e->caption = vnns::sjisToUtf8(e->engine->caption());
    return e->caption.c_str();
}

void vnns_audio_render(VNNSEngine *e, float *out, int frames) { e->engine->audio().render(out, frames); }

static int toPixel(float v, int size) { return std::clamp(int(std::floor(v * size)), 0, size - 1); }

void vnns_mouse_move(VNNSEngine *e, float x, float y) {
    e->engine->mouseMove(toPixel(x, e->engine->width()), toPixel(y, e->engine->height()));
}

void vnns_mouse_button(VNNSEngine *e, int button, bool down) { e->engine->mouseButton(button, down); }
void vnns_wheel(VNNSEngine *e, int notches) { e->engine->wheel(notches); }
void vnns_key(VNNSEngine *e, int key, bool down) { e->engine->key(key, down); }
void vnns_text(VNNSEngine *e, const char *utf8) {
    if (utf8) e->engine->typeText(utf8);
}

void vnns_set_paused(VNNSEngine *e, bool paused) { e->engine->setPaused(paused); }
void vnns_request_quit(VNNSEngine *e) { e->engine->requestQuit(); }
void vnns_save_system_data(VNNSEngine *e) { e->engine->saveSystemData(); }
void vnns_stop(VNNSEngine *e) { delete e; }

const char *vnns_problem(VNNSEngine *e) {
    const std::string &p = e->engine->problem();
    return p.empty() ? nullptr : p.c_str();
}

void vnns_set_glyph_function(VNNSGlyphFunction fn, void *ctx) { vnns::setGlyphCallback(fn, ctx); }

}  // extern "C"

// vnwolf.h: the runtime behind a C interface for the app.
#include "vnwolf.h"

#include "game.h"
#include "render.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <string>

using namespace wolf;

struct VNWolfEngine {
    std::unique_ptr<Game> game;
    std::unique_ptr<Renderer> renderer;
    Bitmap frame;
    FILE *log = nullptr;
    double elapsedMs = 0;
    int speed = 1;
    bool paused = false, advanced = true;
    std::string cheatState;
    std::map<int, int> keysDown;  // key code to the number of times it is held
    uint32_t pressed = 0;         // Wolf keys pressed since the last frame, so a quick tap still counts
    std::string caption, status, problem;
};

namespace {

std::string gStartError;

// Wolf's keys: Z, Return and Space confirm, X and Escape cancel, C and Shift are sub, and the arrows and the number pad's
// 2, 4, 6 and 8 move.
uint32_t wolfKey(int key) {
    switch (key) {
        case 'Z': case VNWOLF_KEY_RETURN: case VNWOLF_KEY_SPACE: return KeyConfirm;
        case 'X': case VNWOLF_KEY_ESCAPE: return KeyCancel;
        case 'C': case VNWOLF_KEY_SHIFT: return KeySub;
        case VNWOLF_KEY_UP: case '8': return KeyUp;
        case VNWOLF_KEY_DOWN: case '2': return KeyDown;
        case VNWOLF_KEY_LEFT: case '4': return KeyLeft;
        case VNWOLF_KEY_RIGHT: case '6': return KeyRight;
        default: return 0;
    }
}

uint32_t heldKeys(const VNWolfEngine *e) {
    uint32_t held = 0;
    for (auto &[k, n] : e->keysDown)
        if (n > 0) held |= wolfKey(k);
    return held;
}

}  // namespace

extern "C" {

VNWolfEngine *vnwolf_start(const VNWolfLaunch *launch) {
    gStartError.clear();
    if (!launch || !launch->gameRoot) {
        gStartError = "no game folder";
        return nullptr;
    }
    auto e = std::make_unique<VNWolfEngine>();
    if (launch->logPath) e->log = std::fopen(launch->logPath, "a");
    FILE *log = e->log ? e->log : stderr;
    VNWolfEngine *raw = e.get();
    e->game = std::make_unique<Game>(launch->gameRoot);
    e->game->log = [log, raw](const std::string &line) {
        std::fprintf(log, "%s\n", line.c_str());
        std::fflush(log);
        if (line.starts_with("missing") || line.starts_with("cannot")) raw->problem = line;
    };
    if (launch->soundFontPath)
        e->game->setSoundFont(launch->soundFontPath, launch->fallbackSoundFontPath ? launch->fallbackSoundFontPath : "");
    std::string error;
    if (!e->game->load(&error)) {
        gStartError = error;
        if (e->log) std::fclose(e->log);
        return nullptr;
    }
    e->renderer = std::make_unique<Renderer>(*e->game);
    e->caption = e->game->gameDat().title;
    e->renderer->draw(e->frame);
    return e.release();
}

const char *vnwolf_start_error(void) { return gStartError.empty() ? nullptr : gStartError.c_str(); }

bool vnwolf_step(VNWolfEngine *e, int ms) {
    if (!e || e->paused) return !e || !e->game->quitRequested();
    double frameMs = 1000.0 / e->game->gameDat().fps();
    e->elapsedMs += double(ms) * e->speed;
    // At most four of the game's own frames per step, so a stall doesn't make the game race to catch up.
    int frames = 0, most = 4 * e->speed;
    while (e->elapsedMs >= frameMs && frames < most) {
        Input in;
        in.held = heldKeys(e) | e->pressed;
        in.pressed = e->pressed;
        e->pressed = 0;
        e->game->update(in);
        e->elapsedMs -= frameMs;
        frames++;
        if (e->game->quitRequested()) return false;
    }
    if (e->elapsedMs > frameMs * most) e->elapsedMs = 0;
    if (frames) {
        e->renderer->draw(e->frame);
        e->advanced = true;
    }
    return true;
}

const uint8_t *vnwolf_frame(VNWolfEngine *e, int *width, int *height, bool *changed) {
    if (!e) return nullptr;
    *width = e->frame.w;
    *height = e->frame.h;
    *changed = e->advanced;
    e->advanced = false;
    return e->frame.px.data();
}

const char *vnwolf_caption(VNWolfEngine *e) { return e ? e->caption.c_str() : ""; }

void vnwolf_audio_render(VNWolfEngine *e, float *out, int frames) {
    if (e && e->game->audio()) e->game->audio()->render(out, frames);
    else std::fill(out, out + frames * 2, 0.0f);
}

void vnwolf_key(VNWolfEngine *e, int key, bool down) {
    if (!e) return;
    int &n = e->keysDown[key];
    if (down) {
        if (n++ == 0) e->pressed |= wolfKey(key);
    } else if (n > 0) {
        n--;
    }
}

void vnwolf_set_paused(VNWolfEngine *e, bool paused) {
    if (!e) return;
    e->paused = paused;
    e->keysDown.clear();
    e->pressed = 0;
    e->elapsedMs = 0;
    if (Audio *a = e->game->audio()) {
        if (paused) a->pause();
        else a->resume();
    }
}

void vnwolf_stop(VNWolfEngine *e) {
    if (!e) return;
    FILE *log = e->log;
    delete e;
    if (log) std::fclose(log);
}

const char *vnwolf_debug_status(VNWolfEngine *e) {
    if (!e) return "";
    e->status = "map " + std::to_string(e->game->mapId()) + ", frame " + std::to_string(e->game->frameCount()) +
                (e->game->message().active ? ", message" : "") + (e->game->choice().active ? ", choice" : "");
    return e->status.c_str();
}

const char *vnwolf_problem(VNWolfEngine *e) { return e && !e->problem.empty() ? e->problem.c_str() : nullptr; }


void vnwolf_set_speed(VNWolfEngine *e, int speed) {
    if (!e) return;
    e->speed = std::clamp(speed, 1, 8);
    e->game->setSpeed(e->speed);
}

bool vnwolf_cheat_has_party(VNWolfEngine *e) { return e && e->game->hasStandardParty(); }

const char *vnwolf_cheat_state(VNWolfEngine *e) {
    if (!e) return nullptr;
    e->cheatState = e->game->cheatState();
    return e->cheatState.c_str();
}

bool vnwolf_cheat_add_gold(VNWolfEngine *e, int amount) { return e && e->game->cheatAddGold(amount); }

bool vnwolf_cheat_add_item(VNWolfEngine *e, int kind, int id, int count) {
    return e && e->game->cheatAddItem(kind, id, count);
}

bool vnwolf_cheat_restore_party(VNWolfEngine *e) { return e && e->game->cheatRestoreParty(); }

bool vnwolf_cheat_set_variable(VNWolfEngine *e, int id, int value) { return e && e->game->cheatSetVariable(id, value); }

void vnwolf_cheat_walk_through_walls(VNWolfEngine *e, bool on) {
    if (e) e->game->setWalkThroughWalls(on);
}

}  // extern "C"

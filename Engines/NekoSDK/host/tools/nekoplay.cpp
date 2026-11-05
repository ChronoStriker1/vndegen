// Plays a game in a window, for checking by hand. Click to advance; hold Ctrl to skip; Escape quits.
// usage: nekoplay <game folder> [script node]
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "vnneko.h"

namespace {

void SDLCALL pullAudio(void *userdata, SDL_AudioStream *stream, int additional, int) {
    auto *e = static_cast<VNNekoEngine *>(userdata);
    int frames = additional / 8;
    if (frames <= 0) return;
    std::vector<float> buffer(size_t(frames) * 2);
    vnneko_audio_render(e, buffer.data(), frames);
    SDL_PutAudioStreamData(stream, buffer.data(), frames * 8);
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: nekoplay <game folder> [script node]\n");
        return 2;
    }
    VNNekoLaunch launch{argv[1], nullptr, NEKO_FONTS_DIR};
    VNNekoEngine *e = vnneko_start(&launch);
    if (!e) {
        std::fprintf(stderr, "did not start: %s\n", vnneko_start_error());
        return 1;
    }
    if (argc >= 4) vnneko_debug_jump(e, argv[2], std::atoi(argv[3]));
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    int w = 0, h = 0;
    bool changed = false;
    vnneko_frame(e, &w, &h, &changed);
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_CreateWindowAndRenderer("VNDegen NekoSDK host", w, h, SDL_WINDOW_RESIZABLE, &window, &renderer);
    SDL_SetRenderLogicalPresentation(renderer, w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
    SDL_AudioSpec spec{SDL_AUDIO_F32, 2, 44100};
    SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, pullAudio, e);
    if (stream) SDL_ResumeAudioStreamDevice(stream);
    Uint64 last = SDL_GetTicks();
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_LCTRL) vnneko_set_skip(e, true);
            if (event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_LCTRL) vnneko_set_skip(e, false);
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) vnneko_mouse_button(e, event.button.button - 1, true);
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) vnneko_mouse_button(e, event.button.button - 1, false);
        }
        Uint64 now = SDL_GetTicks();
        if (!vnneko_step(e, int(now - last))) running = false;
        last = now;
        const uint8_t *px = vnneko_frame(e, &w, &h, &changed);
        if (changed) SDL_UpdateTexture(texture, nullptr, px, w * 4);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        SDL_Delay(8);
    }
    if (stream) SDL_DestroyAudioStream(stream);
    vnneko_stop(e);
    SDL_Quit();
    return 0;
}

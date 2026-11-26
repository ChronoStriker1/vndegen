#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>

#include "vnsmilegame.h"

static int vk(SDL_Keycode k) {
    switch (k) {
    case SDLK_RETURN: case SDLK_KP_ENTER: return 0x0d;
    case SDLK_SPACE: return 0x20;
    case SDLK_ESCAPE: return 0x1b;
    case SDLK_LEFT: return 0x25;
    case SDLK_UP: return 0x26;
    case SDLK_RIGHT: return 0x27;
    case SDLK_DOWN: return 0x28;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return 0x10;
    default:
        if (k >= SDLK_a && k <= SDLK_z) return 'A' + (k - SDLK_a);
        return -1;
    }
}

static VNSmileGame *game;
static void audio(void *u, Uint8 *stream, int len) {
    (void)u;
    vnsmilegame_audio(game, (float *)stream, len / 8);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: sgplay gameDir [saveDir]\n");
        return 2;
    }
    VNSmileGameConfig cfg = {.gameDir = argv[1], .saveDir = argc > 2 ? argv[2] : NULL, .audioRate = 44100, .logPath = "/dev/stderr"};
    char err[512];
    game = vnsmilegame_open(&cfg, err, sizeof err);
    if (!game) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    int w, h;
    vnsmilegame_pixels(game, &w, &h);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window *win = SDL_CreateWindow(vnsmilegame_title(game), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *r = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(r, w, h);
    SDL_Texture *tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    SDL_AudioSpec want = {.freq = 44100, .format = AUDIO_F32SYS, .channels = 2, .samples = 1024, .callback = audio}, have;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev) SDL_PauseAudioDevice(dev, 0);
    Uint32 last = SDL_GetTicks(), shown = ~0u;
    for (bool run = true; run;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) run = false;
            else if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && !e.key.repeat) {
                int k = vk(e.key.keysym.sym);
                if (k >= 0) vnsmilegame_key(game, k, e.type == SDL_KEYDOWN);
            }
        }
        Uint32 now = SDL_GetTicks();
        if (!vnsmilegame_frame(game, now - last)) run = false;
        last = now;
        if (vnsmilegame_frame_serial(game) != shown) {
            shown = vnsmilegame_frame_serial(game);
            SDL_UpdateTexture(tex, NULL, vnsmilegame_pixels(game, &w, &h), w * 4);
        }
        SDL_RenderClear(r);
        SDL_RenderCopy(r, tex, NULL, NULL);
        SDL_RenderPresent(r);
    }
    if (dev) SDL_CloseAudioDevice(dev);
    vnsmilegame_close(game);
    SDL_Quit();
    return 0;
}

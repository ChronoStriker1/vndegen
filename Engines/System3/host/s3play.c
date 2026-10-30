// Plays a System 3 game in an SDL window on Linux, for testing by hand. Not part of the app.
//   s3play <gameDir> [saveDir]
#include "vns3.h"
#include "vnfont.h"
#include <SDL.h>
#include <stdio.h>

static int vk(SDL_Keycode k) {
    switch (k) {
    case SDLK_RETURN: case SDLK_KP_ENTER: return 0x0d;
    case SDLK_SPACE: return 0x20;
    case SDLK_ESCAPE: return 0x1b;
    case SDLK_TAB: return 0x09;
    case SDLK_LEFT: return 0x25;
    case SDLK_UP: return 0x26;
    case SDLK_RIGHT: return 0x27;
    case SDLK_DOWN: return 0x28;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return 0x10;
    case SDLK_LCTRL: case SDLK_RCTRL: return 0x11;
    default:
        if (k >= SDLK_a && k <= SDLK_z) return 'A' + (k - SDLK_a);
        if (k >= SDLK_0 && k <= SDLK_9) return '0' + (k - SDLK_0);
        if (k >= SDLK_F1 && k <= SDLK_F12) return 0x70 + (k - SDLK_F1);
        return -1;
    }
}

static VNS3 *game;
static void audio(void *u, Uint8 *stream, int len) { (void)u; vns3_audio(game, (float *)stream, len / 8); }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: s3play gameDir [saveDir]\n"); return 2; }
    const char *dirs[] = { S3_FONTS_DIR };
    vnfont_init(S3_FONTS_DIR "/fonts.tsv", dirs, 1);
    VNS3Config cfg = { .gameDir = argv[1], .saveDir = argc > 2 ? argv[2] : NULL, .audioRate = 44100,
                       .soundFontPath = S3_SOUNDFONT, .logPath = "/dev/stderr" };
    char err[512];
    game = vns3_open(&cfg, err, sizeof err);
    if (!game) { fprintf(stderr, "%s\n", err); return 1; }
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window *win = SDL_CreateWindow(vns3_title(game), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *r = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture *tex = NULL;
    int tw = 0, th = 0;
    SDL_AudioSpec want = { .freq = 44100, .format = AUDIO_F32SYS, .channels = 2, .samples = 1024, .callback = audio }, have;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev) SDL_PauseAudioDevice(dev, 0);
    Uint32 last = SDL_GetTicks(), shown = ~0u;
    for (bool run = true; run;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) run = false;
            else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) { int k = vk(e.key.keysym.sym); if (k >= 0) vns3_key(game, k, e.type == SDL_KEYDOWN); }
            else if (e.type == SDL_MOUSEMOTION) vns3_mouse_move(game, e.motion.x * tw / 640, e.motion.y * th / 480);
            else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
                int b = e.button.button == SDL_BUTTON_LEFT ? VNS3_MOUSE_LEFT : e.button.button == SDL_BUTTON_RIGHT ? VNS3_MOUSE_RIGHT : 0;
                if (b) vns3_mouse_button(game, b, e.type == SDL_MOUSEBUTTONDOWN);
            } else if (e.type == SDL_MOUSEWHEEL) vns3_wheel(game, e.wheel.y);
        }
        Uint32 now = SDL_GetTicks();
        if (!vns3_frame(game, now - last)) run = false;
        last = now;
        if (vns3_frame_serial(game) != shown) {
            shown = vns3_frame_serial(game);
            int w, h;
            const uint32_t *px = vns3_pixels(game, &w, &h);
            if (w != tw || h != th) {
                if (tex) SDL_DestroyTexture(tex);
                tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
                tw = w; th = h;
                SDL_RenderSetLogicalSize(r, w, h);
            }
            SDL_UpdateTexture(tex, NULL, px, w * 4);
        }
        SDL_RenderClear(r);
        if (tex) SDL_RenderCopy(r, tex, NULL, NULL);
        SDL_RenderPresent(r);
    }
    if (vns3_problem(game)) fprintf(stderr, "problem: %s\n", vns3_problem(game));
    if (dev) SDL_CloseAudioDevice(dev);
    vns3_close(game);
    SDL_Quit();
    return 0;
}

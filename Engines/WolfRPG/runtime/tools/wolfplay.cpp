// Runs a Wolf RPG Editor game headless for a number of frames, with scripted key presses, and saves screenshots.
// For checks on Linux; it opens no window and plays no sound.
//
//   wolfplay <game folder> --frames 300 --shot 120:title.png --press 200:confirm
#include "../src/game.h"
#include "../src/render.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

using namespace wolf;

static uint32_t keyNamed(const std::string &n) {
    static const std::map<std::string, uint32_t> keys = {
        {"down", KeyDown}, {"left", KeyLeft}, {"right", KeyRight}, {"up", KeyUp},
        {"confirm", KeyConfirm}, {"cancel", KeyCancel}, {"sub", KeySub},
    };
    auto it = keys.find(n);
    return it == keys.end() ? 0 : it->second;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: wolfplay <game folder> [--frames N] [--shot F:file.png] [--press F:key]\n");
        return 2;
    }
    int frames = 120;
    std::multimap<int, std::string> shots;
    std::multimap<int, uint32_t> presses;
    for (int i = 2; i + 1 < argc; i += 2) {
        std::string opt = argv[i], val = argv[i + 1];
        size_t colon = val.find(':');
        if (opt == "--frames") frames = std::atoi(val.c_str());
        else if (opt == "--shot" && colon != std::string::npos)
            shots.emplace(std::atoi(val.c_str()), val.substr(colon + 1));
        else if (opt == "--press" && colon != std::string::npos)
            presses.emplace(std::atoi(val.c_str()), keyNamed(val.substr(colon + 1)));
    }

    Game game(argv[1]);
    game.log = [](const std::string &s) { std::fprintf(stderr, "wolf: %s\n", s.c_str()); };
    std::string error;
    if (!game.load(&error)) {
        std::fprintf(stderr, "load failed: %s\n", error.c_str());
        return 1;
    }
    std::printf("%s: %dx%d\n", game.gameDat().title.c_str(), game.screenWidth(), game.screenHeight());

    if (!TTF_Init()) {
        std::fprintf(stderr, "TTF_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Surface *target = SDL_CreateSurface(game.screenWidth(), game.screenHeight(), SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer *sdl = SDL_CreateSoftwareRenderer(target);
    if (!sdl) {
        std::fprintf(stderr, "renderer: %s\n", SDL_GetError());
        return 1;
    }
    {
        Renderer renderer(sdl, game);
        uint32_t held = 0;
        for (int f = 1; f <= frames; f++) {
            Input in;
            auto [a, b] = presses.equal_range(f);
            for (auto it = a; it != b; ++it) in.pressed |= it->second;
            // A scripted press holds the key for this frame only.
            in.held = held | in.pressed;
            game.update(in);
            auto [sa, sb] = shots.equal_range(f);
            if (sa != sb) {
                renderer.draw();
                SDL_RenderPresent(sdl);
                for (auto it = sa; it != sb; ++it) {
                    stbi_write_png(it->second.c_str(), target->w, target->h, 4, target->pixels, target->pitch);
                    std::printf("frame %d: %s\n", f, it->second.c_str());
                }
            }
        }
    }
    for (auto [code, n] : game.unsupported()) std::printf("unsupported command %d x%d\n", code, n);
    SDL_DestroyRenderer(sdl);
    SDL_DestroySurface(target);
    TTF_Quit();
    return 0;
}

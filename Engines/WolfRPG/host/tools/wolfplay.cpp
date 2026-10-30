// Runs a Wolf RPG Editor game headless for a number of frames, with scripted key presses, and saves screenshots.
// For checks on Linux; it opens no window and plays no sound. Frames are drawn on the CPU, as on every platform.
//
//   wolfplay <game folder> --frames 300 --shot 120:title.png --press 200:confirm
//   wolfplay <game folder> --every 20:confirm --hold 500-560:left   (press every 20 frames; hold a key)
//   wolfplay <game folder> --map 3,10,7 --frames 2 --shot 1:map.png   (map id, hero x, y; skips the title)
#include "game.h"
#include "render.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../../Packages/VNDegenKit/Sources/CWolfRPG/third_party/stb/stb_image_write.h"

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
    int mapId = -1, mapX = 0, mapY = 0;
    std::multimap<int, std::string> shots;
    std::multimap<int, uint32_t> presses;
    std::vector<std::pair<int, uint32_t>> repeats;  // press a key every N frames
    std::vector<std::tuple<int, int, uint32_t>> holds;  // hold a key from one frame to another
    for (int i = 2; i + 1 < argc; i += 2) {
        std::string opt = argv[i], val = argv[i + 1];
        size_t colon = val.find(':');
        if (opt == "--frames") frames = std::atoi(val.c_str());
        else if (opt == "--map") std::sscanf(val.c_str(), "%d,%d,%d", &mapId, &mapX, &mapY);
        else if (opt == "--shot" && colon != std::string::npos)
            shots.emplace(std::atoi(val.c_str()), val.substr(colon + 1));
        else if (opt == "--press" && colon != std::string::npos)
            presses.emplace(std::atoi(val.c_str()), keyNamed(val.substr(colon + 1)));
        else if (opt == "--every" && colon != std::string::npos)
            repeats.emplace_back(std::max(1, std::atoi(val.c_str())), keyNamed(val.substr(colon + 1)));
        else if (opt == "--hold" && colon != std::string::npos) {
            int from = 0, to = 0;
            std::sscanf(val.c_str(), "%d-%d", &from, &to);
            holds.emplace_back(from, to, keyNamed(val.substr(colon + 1)));
        }
    }

    Game game(argv[1]);
    game.log = [](const std::string &s) { std::fprintf(stderr, "wolf: %s\n", s.c_str()); };
    std::string error;
    if (!game.load(&error)) {
        std::fprintf(stderr, "load failed: %s\n", error.c_str());
        return 1;
    }
    if (mapId >= 0 && !game.debugTeleport(mapId, mapX, mapY)) {
        std::fprintf(stderr, "cannot open map %d\n", mapId);
        return 1;
    }
    std::printf("%s: %dx%d\n", game.gameDat().title.c_str(), game.screenWidth(), game.screenHeight());

    {
        Renderer renderer(game);
        Bitmap frame;
        uint32_t held = 0;
        for (int f = 1; f <= frames; f++) {
            Input in;
            auto [a, b] = presses.equal_range(f);
            for (auto it = a; it != b; ++it) in.pressed |= it->second;
            for (auto [n, k] : repeats)
                if (f % n == 0) in.pressed |= k;
            for (auto [from, to, k] : holds) {
                if (f == from) in.pressed |= k;
                if (f >= from && f <= to) held |= k;
                if (f == to + 1) held &= ~k;
            }
            // A scripted press holds the key for this frame only.
            in.held = held | in.pressed;
            game.update(in);
            auto [sa, sb] = shots.equal_range(f);
            if (sa != sb) {
                renderer.draw(frame);
                for (auto it = sa; it != sb; ++it) {
                    stbi_write_png(it->second.c_str(), frame.w, frame.h, 4, frame.px.data(), frame.w * 4);
                    std::printf("frame %d: %s\n", f, it->second.c_str());
                }
            }
        }
    }
    for (auto [code, n] : game.unsupported()) std::printf("unsupported command %d x%d\n", code, n);
    return 0;
}

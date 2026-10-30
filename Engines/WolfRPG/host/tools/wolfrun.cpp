// Runs a Wolf RPG Editor game without drawing, for checks of the interpreter: scripted keys in, a summary of the
// game state and the commands it couldn't run out.
//
//   wolfrun <game folder> --frames 3000 --press 300:confirm --every 40:confirm
#include "game.h"

#include <cstdio>
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
        std::fprintf(stderr, "usage: wolfrun <game folder> [--frames N] [--press F:key] [--every N:key]\n");
        return 2;
    }
    int frames = 600;
    std::multimap<int, uint32_t> presses;
    std::vector<std::pair<int, uint32_t>> repeats;
    for (int i = 2; i + 1 < argc; i += 2) {
        std::string opt = argv[i], val = argv[i + 1];
        size_t colon = val.find(':');
        if (opt == "--frames") frames = std::atoi(val.c_str());
        else if (opt == "--press" && colon != std::string::npos)
            presses.emplace(std::atoi(val.c_str()), keyNamed(val.substr(colon + 1)));
        else if (opt == "--every" && colon != std::string::npos)
            repeats.emplace_back(std::max(1, std::atoi(val.c_str())), keyNamed(val.substr(colon + 1)));
    }
    Game game(argv[1]);
    game.log = [](const std::string &s) { std::fprintf(stderr, "wolf: %s\n", s.c_str()); };
    std::string error;
    if (!game.load(&error)) {
        std::fprintf(stderr, "load failed: %s\n", error.c_str());
        return 1;
    }
    for (int f = 1; f <= frames && !game.quitRequested(); f++) {
        Input in;
        auto [a, b] = presses.equal_range(f);
        for (auto it = a; it != b; ++it) in.pressed |= it->second;
        for (auto [n, k] : repeats)
            if (f % n == 0) in.pressed |= k;
        in.held = in.pressed;
        game.update(in);
    }
    std::printf("after %d frames: map %d, %zu pictures, message %s\n", frames, game.mapId(), game.pictures().size(),
                game.message().active ? game.message().text.c_str() : "(none)");
    for (auto [code, n] : game.unsupported()) std::printf("unsupported command %d x%d\n", code, n);
    return 0;
}

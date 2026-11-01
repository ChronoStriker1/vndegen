// Runs a Wolf RPG Editor game headless for a number of frames, with scripted key presses, and saves screenshots.
// For checks on Linux; it opens no window and plays no sound. Frames are drawn on the CPU, as on every platform.
//
//   wolfplay <game folder> --frames 300 --shot 120:title.png --press 200:confirm
//   wolfplay <game folder> --every 20:confirm --hold 500-560:left   (press every 20 frames, until frame U with 20:confirm:U; hold a key)
//   wolfplay <game folder> --map 3,10,7 --frames 2 --shot 1:map.png   (map id, hero x, y; skips the title)
//   wolfplay <game folder> --mouse 200:640,480 --click 210:640,480   (move the pointer, or move and left click)
//   wolfplay <game folder> --type 300:Ann --key 310:128   (types text into a text entry, then presses Enter)
//   wolfplay <game folder> --pics 200   (lists the pictures shown at that frame)
//   wolfplay <game folder> --rclick 210:640,480 --wheel 220:-1 --key 230:144   (a keyboard key by Wolf's code)
#include "game.h"
#include "render.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../../Packages/VNDegenKit/Sources/CWolfRPG/third_party/stb/stb_image_write.h"

#include <cstdio>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
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
    int frames = 120, titleAt = -1;
    int mapId = -1, mapX = 0, mapY = 0;
    std::multimap<int, std::string> shots;
    std::multimap<int, uint32_t> presses;
    std::vector<std::tuple<int, uint32_t, int>> repeats;  // press a key every N frames, until a frame
    std::vector<std::tuple<int, int, uint32_t>> holds;  // hold a key from one frame to another
    struct MouseAt { int x, y; uint32_t button; };
    std::multimap<int, MouseAt> mouse;
    std::multimap<int, int> wheels, keyCodes;
    std::set<int> picLists;
    std::multimap<int, std::string> typed;
    for (int i = 2; i + 1 < argc; i += 2) {
        std::string opt = argv[i], val = argv[i + 1];
        size_t colon = val.find(':');
        if (opt == "--frames") frames = std::atoi(val.c_str());
        else if (opt == "--title") titleAt = std::atoi(val.c_str());
        else if (opt == "--map") std::sscanf(val.c_str(), "%d,%d,%d", &mapId, &mapX, &mapY);
        else if (opt == "--shot" && colon != std::string::npos)
            shots.emplace(std::atoi(val.c_str()), val.substr(colon + 1));
        else if (opt == "--press" && colon != std::string::npos)
            presses.emplace(std::atoi(val.c_str()), keyNamed(val.substr(colon + 1)));
        else if (opt == "--every" && colon != std::string::npos)
        {
            // N:key, or N:key:until
            std::string key = val.substr(colon + 1);
            int until = INT32_MAX;
            if (size_t c2 = key.find(':'); c2 != std::string::npos) {
                until = std::atoi(key.c_str() + c2 + 1);
                key.resize(c2);
            }
            repeats.emplace_back(std::max(1, std::atoi(val.c_str())), keyNamed(key), until);
        }
        else if ((opt == "--mouse" || opt == "--click" || opt == "--rclick") && colon != std::string::npos) {
            int x = 0, y = 0;
            std::sscanf(val.c_str() + colon + 1, "%d,%d", &x, &y);
            mouse.emplace(std::atoi(val.c_str()), MouseAt{x, y, opt == "--click" ? MouseLeft : opt == "--rclick" ? MouseRight : 0u});
        }
        else if (opt == "--type" && colon != std::string::npos) typed.emplace(std::atoi(val.c_str()), val.substr(colon + 1));
        else if (opt == "--pics") picLists.insert(std::atoi(val.c_str()));
        else if (opt == "--wheel" && colon != std::string::npos)
            wheels.emplace(std::atoi(val.c_str()), std::atoi(val.c_str() + colon + 1));
        else if (opt == "--key" && colon != std::string::npos)
            keyCodes.emplace(std::atoi(val.c_str()), std::atoi(val.c_str() + colon + 1));
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
        int mouseX = 0, mouseY = 0;
        bool wasTyping = false;
        for (int f = 1; f <= frames; f++) {
            Input in;
            // A scripted click holds the button for this frame only.
            for (auto [it, end] = mouse.equal_range(f); it != end; ++it) {
                mouseX = it->second.x;
                mouseY = it->second.y;
                in.mouseHeld |= it->second.button;
                in.mousePressed |= it->second.button;
            }
            in.mouseX = mouseX;
            in.mouseY = mouseY;
            for (auto [it, end] = wheels.equal_range(f); it != end; ++it) in.wheel += it->second;
            for (auto [it, end] = keyCodes.equal_range(f); it != end; ++it) {
                in.keysHeld.set(it->second);
                in.keysPressed.set(it->second);
            }
            auto [a, b] = presses.equal_range(f);
            for (auto it = a; it != b; ++it) in.pressed |= it->second;
            for (auto [n, k, until] : repeats)
                if (f % n == 0 && f <= until) in.pressed |= k;
            for (auto [from, to, k] : holds) {
                if (f == from) in.pressed |= k;
                if (f >= from && f <= to) held |= k;
                if (f == to + 1) held &= ~k;
            }
            // A scripted press holds the key for this frame only.
            in.held = held | in.pressed;
            if (f == titleAt) game.debugReturnToTitle();
            for (auto [it, end] = typed.equal_range(f); it != end; ++it) game.typeText(it->second);
            game.update(in);
            if (std::getenv("WOLF_HERO") && f % 30 == 0) {
                auto h = game.heroTile();
                std::printf("frame %d: map %d hero %d,%d facing %d%s%s\n", f, game.mapId(), h[0], h[1], h[2],
                            game.message().active ? " (message)" : "", game.choice().active ? " (choice)" : "");
            }
            if (bool typing = game.textEntry().active; typing != wasTyping) {
                wasTyping = typing;
                std::printf("frame %d: text entry %s \"%s\"\n", f, typing ? "starts" : "ends", game.textEntry().text.c_str());
            }
            if (picLists.count(f)) {
                for (auto &[n, p] : game.pictures())
                    std::printf("pic %d kind %d blend %d rgb %.0f,%.0f,%.0f %.0f,%.0f anchor %d size %dx%d zoom %.0f/%.0f opacity %.0f \"%.50s\"\n", n, int(p.kind), p.blend, p.r.value(), p.g.value(), p.b.value(), p.x.value(),
                                p.y.value(), p.anchor, p.imageW, p.imageH, p.zoomX.value(), p.zoomY.value(), p.opacity.value(), p.source.c_str());
            }
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

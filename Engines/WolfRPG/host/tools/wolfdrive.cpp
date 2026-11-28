#include "game.h"
#include "render.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../../Packages/VNDegenKit/Sources/CWolfRPG/third_party/stb/stb_image_write.h"

#include <cstdio>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

using namespace wolf;

static uint32_t keyNamed(const std::string &n) {
    static const std::map<std::string, uint32_t> keys = {
        {"down", KeyDown}, {"left", KeyLeft}, {"right", KeyRight}, {"up", KeyUp},
        {"confirm", KeyConfirm}, {"z", KeyConfirm}, {"cancel", KeyCancel}, {"x", KeyCancel}, {"sub", KeySub},
    };
    auto it = keys.find(n);
    return it == keys.end() ? 0 : it->second;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: wolfdrive <game folder> < commands\n");
        return 2;
    }
    Game game(argv[1]);
    game.log = [](const std::string &s) { std::printf("wolf: %s\n", s.c_str()); };
    std::string error;
    if (!game.load(&error)) {
        std::fprintf(stderr, "load failed: %s\n", error.c_str());
        return 1;
    }
    Renderer renderer(game);
    Bitmap frame;
    uint32_t held = 0;
    KeyCodes heldCodes;
    int mouseX = 0, mouseY = 0;
    long frameNo = 0;

    auto step = [&](uint32_t pressed, const KeyCodes &codesPressed, uint32_t mousePressed = 0) {
        Input in;
        in.pressed = pressed;
        in.held = held | pressed;
        in.keysPressed = codesPressed;
        in.keysHeld = heldCodes | codesPressed;
        in.mouseX = mouseX;
        in.mouseY = mouseY;
        in.mousePressed = mousePressed;
        in.mouseHeld = mousePressed;
        in.touched = mousePressed != 0;
        game.update(in);
        renderer.draw(frame);
        frameNo++;
    };
    auto run = [&](int n) {
        for (int i = 0; i < n; i++) step(0, {});
    };
    auto pressFor = [&](uint32_t k, int frames) {
        uint32_t before = held;
        step(k, {});
        held |= k;
        for (int i = 1; i < frames; i++) step(0, {});
        held = before;
    };
    auto codeFor = [&](int code, int frames) {
        KeyCodes c;
        c.set(code);
        step(0, c);
        heldCodes.set(code);
        for (int i = 1; i < frames; i++) step(0, {});
        heldCodes.reset(code);
    };

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;
        if (cmd.empty()) continue;
        if (cmd == "run") {
            int n = 1;
            ss >> n;
            run(n);
        } else if (cmd == "tap" || cmd == "hold") {
            std::string k;
            int frames = cmd == "tap" ? 2 : 30, after = cmd == "tap" ? 10 : 0;
            ss >> k >> frames >> after;
            pressFor(keyNamed(k), frames);
            run(after);
        } else if (cmd == "taps") {
            std::string k;
            int count = 1, gap = 20;
            ss >> k >> count >> gap;
            for (int i = 0; i < count; i++) {
                pressFor(keyNamed(k), 2);
                run(gap);
            }
        } else if (cmd == "down") {
            std::string k;
            ss >> k;
            held |= keyNamed(k);
        } else if (cmd == "up") {
            std::string k;
            ss >> k;
            held &= ~keyNamed(k);
        } else if (cmd == "code") {
            int code = 0, frames = 2, after = 10;
            ss >> code >> frames >> after;
            codeFor(code, frames);
            run(after);
        } else if (cmd == "click") {
            int after = 10;
            ss >> mouseX >> mouseY >> after;
            step(0, {}, MouseLeft);
            run(after);
        } else if (cmd == "mouse") {
            ss >> mouseX >> mouseY;
        } else if (cmd == "shot") {
            std::string file;
            ss >> file;
            stbi_write_png(file.c_str(), frame.w, frame.h, 4, frame.px.data(), frame.w * 4);
        } else if (cmd == "save" || cmd == "load") {
            int slot = 1;
            ss >> slot;
            std::string err;
            bool ok = cmd == "save" ? game.saveSlot(slot, &err) : game.loadSlot(slot, &err);
            std::printf("%s slot %d %s %s\n", cmd.c_str(), slot, ok ? "ok" : "failed:", err.c_str());
            if (cmd == "load") run(1);
        } else if (cmd == "pics") {
            for (auto &[n, p] : game.pictures())
                std::printf("pic %d kind %d blend %d pat %d %.0f,%.0f anchor %d size %dx%d zoom %.0f/%.0f opacity %.0f \"%.60s\"\n", n,
                            int(p.kind), p.blend, p.pattern, p.x.value(), p.y.value(), p.anchor, p.imageW, p.imageH, p.zoomX.value(),
                            p.zoomY.value(), p.opacity.value(), p.source.c_str());
        } else if (cmd == "sys") {
            int i;
            while (ss >> i) std::printf("sys %d=%d\n", i, game.systemNumber(i));
        } else if (cmd == "state") {
            auto h = game.heroTile();
            std::printf("frame %ld map %d hero %d,%d facing %d\n", frameNo, game.mapId(), h[0], h[1], h[2]);
            if (game.message().active) std::printf("message: %s\n", game.message().text.c_str());
            if (game.choice().active) {
                std::printf("choice cursor %d:", game.choice().cursor);
                for (auto &s : game.choice().items) std::printf(" [%s]", s.c_str());
                std::printf("\n");
            }
            if (game.textEntry().active) std::printf("text entry\n");
        } else if (cmd == "unsupported") {
            for (auto [code, n] : game.unsupported()) std::printf("unsupported command %d x%d\n", code, n);
        } else if (cmd == "quit") {
            break;
        } else {
            std::printf("unknown command %s\n", cmd.c_str());
        }
        std::printf("ok %ld\n", frameNo);
        std::fflush(stdout);
    }
    return 0;
}

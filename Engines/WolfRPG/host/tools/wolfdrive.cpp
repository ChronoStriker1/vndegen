#include "game.h"
#include "render.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../../Packages/VNDegenKit/Sources/CWolfRPG/third_party/stb/stb_image_write.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

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
    double slowMs = 0;

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
        auto t0 = std::chrono::steady_clock::now();
        game.update(in);
        auto t1 = std::chrono::steady_clock::now();
        renderer.draw(frame);
        auto t2 = std::chrono::steady_clock::now();
        double u = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double d = std::chrono::duration<double, std::milli>(t2 - t1).count();
        if (slowMs > 0 && u + d > slowMs) std::printf("slow frame %ld update %.1f ms draw %.1f ms\n", frameNo, u, d);
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
            for (auto &[n, p] : game.pictures())
                if (p.freeForm)
                    std::printf("pic %d corners %.0f,%.0f %.0f,%.0f %.0f,%.0f %.0f,%.0f\n", n, p.x.value(), p.y.value(),
                                p.corners[0].value(), p.corners[1].value(), p.corners[2].value(), p.corners[3].value(),
                                p.corners[4].value(), p.corners[5].value());
        } else if (cmd == "sys") {
            int i;
            while (ss >> i) std::printf("sys %d=%d\n", i, game.systemNumber(i));
        } else if (cmd == "state") {
            auto h = game.heroTile();
            auto hh = game.heroHalfTile();
            std::printf("frame %ld map %d hero %d,%d facing %d half %d,%d\n", frameNo, game.mapId(), h[0], h[1], h[2], hh[0],
                        hh[1]);
            if (game.message().active) std::printf("message: %s\n", game.message().text.c_str());
            if (game.choice().active) {
                std::printf("choice cursor %d:", game.choice().cursor);
                for (auto &s : game.choice().items) std::printf(" [%s]", s.c_str());
                std::printf("\n");
            }
            if (game.textEntry().active) std::printf("text entry\n");
        } else if (cmd == "hero") {
            const Actor &a = game.debugHero();
            std::printf("hero through %d visible %d opacity %d graphic \"%s\" transparent %d speed %d fixed %d route %d unit %d square %d\n", a.through, a.chara.visible, a.chara.opacity, a.chara.graphic.c_str(), game.debugTransparent(), a.speed, a.fixedDirection, a.route != nullptr, a.unit, a.square);
        } else if (cmd == "unsupported") {
            for (auto [code, n] : game.unsupported()) std::printf("unsupported command %d x%d\n", code, n);
        } else if (cmd == "slow") {
            ss >> slowMs;
        } else if (cmd == "time") {
            int n = 600;
            ss >> n;
            std::vector<double> updates, draws;
            for (int i = 0; i < n; i++) {
                Input in;
                in.held = held;
                in.keysHeld = heldCodes;
                in.mouseX = mouseX;
                in.mouseY = mouseY;
                auto t0 = std::chrono::steady_clock::now();
                game.update(in);
                auto t1 = std::chrono::steady_clock::now();
                renderer.draw(frame);
                auto t2 = std::chrono::steady_clock::now();
                frameNo++;
                updates.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                draws.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
            }
            auto report = [](const char *name, std::vector<double> v) {
                double sum = 0;
                for (double x : v) sum += x;
                std::sort(v.begin(), v.end());
                std::printf("%s avg %.2f ms p99 %.2f max %.2f\n", name, sum / v.size(), v[v.size() * 99 / 100], v.back());
            };
            report("update", updates);
            report("draw", draws);
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

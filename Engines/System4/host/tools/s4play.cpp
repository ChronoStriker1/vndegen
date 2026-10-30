// Runs a System 4 game headless through the runtime's C interface and saves frames as PNG.
// Usage: s4play <game folder> <save folder> <frames> [script]
// The script is lines of "<frame> <command>": "key <vk> down|up", "click <x> <y>", "move <x> <y>", "shot <file.png>",
// "wheel <n>". Each frame advances the game clock by 16 ms. At the end the last frame is saved to s4play-last.png.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "vns4.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

static void shot(VNS4Engine *e, const std::string &file) {
    int w, h;
    const uint8_t *px = vns4_frame(e, &w, &h);
    stbi_write_png(file.c_str(), w, h, 4, px, w * 4);
    std::printf("saved %s\n", file.c_str());
}

int main(int argc, char **argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: s4play <game> <savedir> <frames> [script]\n"); return 2; }
    std::multimap<long, std::string> script;
    if (argc > 4) {
        std::ifstream f(argv[4]);
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            long fr;
            ss >> fr;
            std::string rest;
            std::getline(ss, rest);
            script.insert({fr, rest.substr(rest.find_first_not_of(' '))});
        }
    }
    VNS4Launch l{argv[1], argv[2], nullptr, std::getenv("S4_FONTS")};
    VNS4Engine *e = vns4_start(&l);
    if (!e) { std::fprintf(stderr, "start failed: %s\n", vns4_start_error()); return 1; }
    long frames = std::atol(argv[3]);
    std::vector<float> audio(2 * 735);
    long f = 0;
    for (; f < frames; f++) {
        auto range = script.equal_range(f);
        for (auto it = range.first; it != range.second; ++it) {
            std::istringstream ss(it->second);
            std::string cmd;
            ss >> cmd;
            if (cmd == "key") { int vk; std::string ud; ss >> vk >> ud; vns4_key(e, vk, ud == "down"); }
            else if (cmd == "move") { int x, y; ss >> x >> y; vns4_mouse_move(e, x, y); }
            else if (cmd == "click") { int x, y; ss >> x >> y; vns4_mouse_move(e, x, y); vns4_key(e, 1, true); }
            else if (cmd == "release") { vns4_key(e, 1, false); }
            else if (cmd == "wheel") { int n; ss >> n; vns4_wheel(e, n); }
            else if (cmd == "sprites") vns4_debug_dump_sprites(e);
            else if (cmd == "call") { std::string fn; ss >> fn; vns4_debug_call(e, fn.c_str()); }
            else if (cmd == "shot") { std::string file; ss >> file; shot(e, file); }
        }
        if (!vns4_step(e, 16)) { std::printf("game ended at frame %ld\n", f); break; }
        vns4_audio_render(e, audio.data(), 735);
    }
    shot(e, "s4play-last.png");
    if (const char *p = vns4_problem(e)) std::printf("problem: %s\n", p);
    vns4_stop(e);
    return 0;
}

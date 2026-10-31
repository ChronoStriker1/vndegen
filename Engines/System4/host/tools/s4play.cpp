// Runs a System 4 game headless through the runtime's C interface and saves frames as PNG.
// Usage: s4play <game folder> <save folder> <frames> [script]
// The script is lines of "<frame> <command>": "key <vk> down|up", "click <x> <y>", "move <x> <y>", "shot <file.png>",
// "wheel <n>". Each frame advances the game clock by 16 ms. At the end the last frame is saved to s4play-last.png.
#include <cmath>
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
    // Audio is pulled at the game clock's rate, 16 ms a frame. S4_AUDIOLOG prints the mix's RMS level each second;
    // S4_WAV writes the mix to a 16-bit WAV file. S4_HITCH=n makes every nth frame a 200 ms hitch, as a slow host
    // frame would be: the step and the sound pulled after it cover 200 ms.
    std::vector<float> audio(2 * 8820);
    long hitch = std::getenv("S4_HITCH") ? std::atol(std::getenv("S4_HITCH")) : 0;
    long gameMs = 0, sampleClock = 0;  // time stepped and sound frames pulled so far
    bool audioLog = std::getenv("S4_AUDIOLOG") != nullptr;
    std::FILE *wav = std::getenv("S4_WAV") ? std::fopen(std::getenv("S4_WAV"), "wb") : nullptr;
    if (wav) std::fseek(wav, 44, SEEK_SET);
    uint32_t wavFrames = 0;
    double sumSq = 0;
    long sampled = 0;
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
        int ms = hitch > 0 && f % hitch == hitch - 1 ? 200 : 16;
        if (!vns4_step(e, ms)) { std::printf("game ended at frame %ld\n", f); break; }
        gameMs += ms;
        int n = int(gameMs * 441 / 10 - sampleClock);  // whole frames due by now, so no fraction is lost
        sampleClock += n;
        vns4_audio_render(e, audio.data(), n);
        if (wav) {
            std::vector<int16_t> pcm(size_t(n) * 2);
            for (size_t i = 0; i < pcm.size(); i++) pcm[i] = int16_t(std::lround(audio[i] * 32767.0f));
            std::fwrite(pcm.data(), 2, pcm.size(), wav);
            wavFrames += uint32_t(n);
        }
        if (audioLog) {
            for (int i = 0; i < n * 2; i++) sumSq += double(audio[i]) * audio[i];
            sampled += n;
            if (sampled >= 44100) {
                std::printf("audio at frame %ld: rms %.4f\n", f, std::sqrt(sumSq / double(sampled * 2)));
                sumSq = 0;
                sampled = 0;
            }
        }
    }
    if (wav) {
        uint32_t data = wavFrames * 4, riff = 36 + data, fmtLen = 16, rate = 44100, byteRate = 44100 * 4;
        uint16_t pcmTag = 1, channels = 2, align = 4, bits = 16;
        std::fseek(wav, 0, SEEK_SET);
        std::fwrite("RIFF", 1, 4, wav); std::fwrite(&riff, 4, 1, wav); std::fwrite("WAVEfmt ", 1, 8, wav);
        std::fwrite(&fmtLen, 4, 1, wav); std::fwrite(&pcmTag, 2, 1, wav); std::fwrite(&channels, 2, 1, wav);
        std::fwrite(&rate, 4, 1, wav); std::fwrite(&byteRate, 4, 1, wav); std::fwrite(&align, 2, 1, wav);
        std::fwrite(&bits, 2, 1, wav); std::fwrite("data", 1, 4, wav); std::fwrite(&data, 4, 1, wav);
        std::fclose(wav);
    }
    shot(e, "s4play-last.png");
    if (const char *p = vns4_problem(e)) std::printf("problem: %s\n", p);
    vns4_stop(e);
    return 0;
}

// Runs a game headless: steps the clock, clicks through text, writes a PNG of chosen moments, and renders the sound.
// usage: nekorun <game folder> [--out dir] [--every n] [--click ms] [--max ms] [--dt ms] [--jump script node]
//                [--shots ms,ms,...] [--wav file] [--period ms]
// --period clicks on a fixed beat whatever the game is doing, as a recording of the PC game was driven.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "vnneko.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

namespace {

void writePng(const std::string &path, const uint8_t *px, int w, int h) { stbi_write_png(path.c_str(), w, h, 4, px, w * 4); }

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: nekorun <game folder> [options]\n");
        return 2;
    }
    std::string out, wav, jumpScript;
    int every = 0, clickMs = 300, maxMs = 3600 * 1000, dt = 16, jumpNode = 0, period = 0, nextBeat = 0;
    std::vector<int> shots;
    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--out") out = next();
        else if (a == "--every") every = std::atoi(next());
        else if (a == "--click") clickMs = std::atoi(next());
        else if (a == "--max") maxMs = std::atoi(next());
        else if (a == "--dt") dt = std::atoi(next());
        else if (a == "--wav") wav = next();
        else if (a == "--period") { period = std::atoi(next()); nextBeat = period; }
        else if (a == "--jump") { jumpScript = next(); jumpNode = std::atoi(next()); }
        else if (a == "--shots") {
            std::string list = next();
            for (size_t at = 0; at < list.size();) {
                shots.push_back(std::atoi(list.c_str() + at));
                at = list.find(',', at);
                if (at == std::string::npos) break;
                at++;
            }
        }
    }
    VNNekoLaunch launch{argv[1], nullptr, NEKO_FONTS_DIR};
    VNNekoEngine *e = vnneko_start(&launch);
    if (!e) {
        std::fprintf(stderr, "did not start: %s\n", vnneko_start_error());
        return 1;
    }
    if (!jumpScript.empty() && !vnneko_debug_jump(e, jumpScript.c_str(), jumpNode)) {
        std::fprintf(stderr, "no such node\n");
        return 1;
    }
    FILE *wavFile = wav.empty() ? nullptr : std::fopen(wav.c_str(), "wb");
    uint32_t wavFrames = 0;
    if (wavFile) std::fseek(wavFile, 44, SEEK_SET);
    std::vector<float> audio;
    double audioOwed = 0, sumSquares = 0;
    long loudFrames = 0, audioFrames = 0;

    int elapsed = 0, waited = 0, lastCount = 0, shotIndex = 0;
    long drawn = 0;
    double slowest = 0, totalDraw = 0;
    bool ended = false;
    while (elapsed < maxMs) {
        if (!vnneko_step(e, dt)) { ended = true; break; }
        elapsed += dt;
        if (wavFile || true) {
            audioOwed += 44100.0 * dt / 1000;
            int frames = int(audioOwed);
            audioOwed -= frames;
            audio.resize(size_t(frames) * 2);
            vnneko_audio_render(e, audio.data(), frames);
            for (int i = 0; i < frames; i++) {
                double s = audio[size_t(i) * 2] * audio[size_t(i) * 2];
                sumSquares += s;
                if (s > 1e-6) loudFrames++;
            }
            audioFrames += frames;
            if (wavFile) {
                std::vector<int16_t> pcm(audio.size());
                for (size_t i = 0; i < audio.size(); i++) pcm[i] = int16_t(std::clamp(audio[i], -1.0f, 1.0f) * 32767);
                std::fwrite(pcm.data(), 2, pcm.size(), wavFile);
                wavFrames += uint32_t(frames);
            }
        }
        int w, h;
        bool changed;
        auto began = std::chrono::steady_clock::now();
        const uint8_t *px = vnneko_frame(e, &w, &h, &changed);
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
        if (changed) {
            drawn++;
            totalDraw += ms;
            slowest = std::max(slowest, ms);
        }
        while (shotIndex < int(shots.size()) && elapsed >= shots[size_t(shotIndex)]) {
            if (!out.empty()) {
                char name[64];
                std::snprintf(name, sizeof name, "/t%07d.png", shots[size_t(shotIndex)]);
                writePng(out + name, px, w, h);
            }
            shotIndex++;
        }
        int count = vnneko_debug_text_count(e);
        if (period && elapsed >= nextBeat) {
            nextBeat += period;
            vnneko_mouse_button(e, 0, true);
            vnneko_mouse_button(e, 0, false);
        }
        if (vnneko_debug_waiting_for_click(e)) {
            waited += dt;
            if (waited >= clickMs) {
                if (every > 0 && !out.empty() && count % every == 0) {
                    char name[64];
                    std::snprintf(name, sizeof name, "/text%05d.png", count);
                    writePng(out + name, px, w, h);
                    std::printf("text %d: %s\n", count, vnneko_debug_status(e));
                }
                if (clickMs < 1000000) {
                    vnneko_mouse_button(e, 0, true);
                    vnneko_mouse_button(e, 0, false);
                }
                waited = 0;
            }
        } else {
            waited = 0;
        }
        lastCount = count;
    }
    if (wavFile) {
        // A plain 16-bit stereo WAV header, written once the length is known.
        uint32_t bytes = wavFrames * 4, rate = 44100, byteRate = rate * 4, chunk = 36 + bytes, sixteen = 16;
        uint16_t pcm = 1, channels = 2, align = 4, bits = 16;
        std::fseek(wavFile, 0, SEEK_SET);
        std::fwrite("RIFF", 1, 4, wavFile);
        std::fwrite(&chunk, 4, 1, wavFile);
        std::fwrite("WAVEfmt ", 1, 8, wavFile);
        std::fwrite(&sixteen, 4, 1, wavFile);
        std::fwrite(&pcm, 2, 1, wavFile);
        std::fwrite(&channels, 2, 1, wavFile);
        std::fwrite(&rate, 4, 1, wavFile);
        std::fwrite(&byteRate, 4, 1, wavFile);
        std::fwrite(&align, 2, 1, wavFile);
        std::fwrite(&bits, 2, 1, wavFile);
        std::fwrite("data", 1, 4, wavFile);
        std::fwrite(&bytes, 4, 1, wavFile);
        std::fclose(wavFile);
    }
    const char *problem = vnneko_problem(e);
    std::printf("%s after %.1f s of game time: %s; text boxes %d; frames drawn %ld, average %.2f ms, slowest %.2f ms; sound on for %.1f s, rms %.4f; last problem: %s\n",
                ended ? "ENDED" : "STOPPED", elapsed / 1000.0, vnneko_debug_status(e), lastCount, drawn, drawn ? totalDraw / double(drawn) : 0, slowest,
                double(loudFrames) / 44100, audioFrames ? std::sqrt(sumSquares / double(audioFrames)) : 0, problem ? problem : "none");
    vnneko_stop(e);
    return ended ? 0 : 3;
}

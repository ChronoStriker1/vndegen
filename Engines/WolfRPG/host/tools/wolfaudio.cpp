// Renders a game's sound through wolf::Audio into a 16-bit WAV file, or plays it on an SDL3 audio device. The SDL
// device stands in for the host's audio output; wolf::Audio itself doesn't use SDL.
#include "audio.h"
#include "files.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

void usage() {
    std::fprintf(stderr,
                 "usage: wolfaudio <game folder> <sound path> [options]\n"
                 "  --out FILE.wav     where to write (default wolfaudio.wav)\n"
                 "  --kind bgm|bgs|se  channel (default bgm)\n"
                 "  --volume N --freq N --loop MS --start MS --fade-in FRAMES\n"
                 "  --seconds S        length to render (default 10)\n"
                 "  --stop-at S --fade-out FRAMES   stop the music at S seconds\n"
                 "  --replay-at S      play the same file again at S seconds (should not restart)\n"
                 "  --freq-at S:N      play it again at S seconds with frequency N\n"
                 "  --se-count N --se-every MS      play the sound as an SE N times\n"
                 "  --pause-at S --resume-at S\n"
                 "  --soundfont FILE.sf2\n"
                 "  --fallback-soundfont FILE.sf2   loaded when --soundfont can't be\n"
                 "  --rate HZ          output rate (default 44100)\n"
                 "  --device           play on the SDL audio device instead of writing a file\n");
}

// A 16-bit stereo WAV file written as the samples come, with the sizes filled in at the end.
struct WavWriter {
    FILE *f;
    uint32_t rate, bytes = 0;
    WavWriter(const char *path, uint32_t r) : f(std::fopen(path, "wb")), rate(r) {
        if (!f) {
            std::perror(path);
            std::exit(1);
        }
        header();
    }
    void u32(uint32_t v) { std::fwrite(&v, 4, 1, f); }
    void u16(uint16_t v) { std::fwrite(&v, 2, 1, f); }
    void header() {
        std::fwrite("RIFF", 1, 4, f);
        u32(36 + bytes);
        std::fwrite("WAVEfmt ", 1, 8, f);
        u32(16);
        u16(1);
        u16(2);
        u32(rate);
        u32(rate * 4);
        u16(4);
        u16(16);
        std::fwrite("data", 1, 4, f);
        u32(bytes);
    }
    void write(const float *samples, size_t n) {
        std::vector<int16_t> pcm(n);
        for (size_t i = 0; i < n; i++) pcm[i] = int16_t(std::lround(std::clamp(samples[i], -1.0f, 1.0f) * 32767));
        std::fwrite(pcm.data(), 2, n, f);
        bytes += uint32_t(n * 2);
    }
    ~WavWriter() {
        std::fseek(f, 0, SEEK_SET);
        header();
        std::fclose(f);
    }
};

}  // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        usage();
        return 2;
    }
    std::string root = argv[1], sound = argv[2], out = "wolfaudio.wav", kind = "bgm", soundfont, fallback;
    wolf::MusicParams params;
    double seconds = 10, stopAt = -1, replayAt = -1, freqAt = -1, pauseAt = -1, resumeAt = -1;
    int fadeOut = 0, seCount = 1, seEvery = 100, freqAtValue = 100, rate = 44100;
    bool device = false;
    for (int i = 3; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> const char * {
            if (i + 1 >= argc) {
                usage();
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--out") out = next();
        else if (a == "--kind") kind = next();
        else if (a == "--volume") params.volume = std::atoi(next());
        else if (a == "--freq") params.frequency = std::atoi(next());
        else if (a == "--loop") params.loopStartMs = std::atoi(next());
        else if (a == "--start") params.startMs = std::atoi(next());
        else if (a == "--fade-in") params.fadeFrames = std::atoi(next());
        else if (a == "--seconds") seconds = std::atof(next());
        else if (a == "--stop-at") stopAt = std::atof(next());
        else if (a == "--fade-out") fadeOut = std::atoi(next());
        else if (a == "--replay-at") replayAt = std::atof(next());
        else if (a == "--freq-at") std::sscanf(next(), "%lf:%d", &freqAt, &freqAtValue);
        else if (a == "--se-count") seCount = std::atoi(next());
        else if (a == "--se-every") seEvery = std::atoi(next());
        else if (a == "--pause-at") pauseAt = std::atof(next());
        else if (a == "--resume-at") resumeAt = std::atof(next());
        else if (a == "--soundfont") soundfont = next();
        else if (a == "--fallback-soundfont") fallback = next();
        else if (a == "--rate") rate = std::atoi(next());
        else if (a == "--device") device = true;
        else {
            usage();
            return 2;
        }
    }

    wolf::GameFiles files(root);
    // On a device the decoding thread runs, as in the app. For a file render() decodes, so runs repeat exactly.
    wolf::Audio audio(files, rate, device, [](const std::string &s) { std::fprintf(stderr, "%s\n", s.c_str()); });
    // Declared after `audio`, so the stream and its pulls stop before the Audio goes away.
    auto closeStream = [](SDL_AudioStream *s) {
        SDL_DestroyAudioStream(s);
        SDL_Quit();
    };
    std::unique_ptr<SDL_AudioStream, decltype(closeStream)> stream(nullptr, closeStream);
    if (device) {
        if (!SDL_Init(SDL_INIT_AUDIO)) {
            std::fprintf(stderr, "SDL audio: %s\n", SDL_GetError());
            return 1;
        }
        SDL_AudioSpec spec{SDL_AUDIO_F32, 2, rate};
        // SDL's audio thread pulls the mix, as the host's real-time thread does.
        auto pull = [](void *user, SDL_AudioStream *s, int additional, int) {
            constexpr int chunk = 1024;
            float buf[chunk * 2];
            for (int frames = additional / int(sizeof(float) * 2); frames > 0; frames -= chunk) {
                int n = std::min(frames, chunk);
                static_cast<wolf::Audio *>(user)->render(buf, n);
                SDL_PutAudioStreamData(s, buf, n * int(sizeof(float) * 2));
            }
        };
        stream.reset(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, pull, &audio));
        if (!stream) {
            std::fprintf(stderr, "SDL audio device: %s\n", SDL_GetError());
            return 1;
        }
        std::printf("SDL audio driver %s\n", SDL_GetCurrentAudioDriver());
        SDL_ResumeAudioStreamDevice(stream.get());
    }
    if (!soundfont.empty()) audio.setSoundFontPath(soundfont, fallback);
    auto channel = kind == "bgs" ? wolf::SoundKind::BGS : wolf::SoundKind::BGM;
    bool se = kind == "se";

    const int fps = 60, perFrame = rate / fps;
    const int totalFrames = int(seconds * fps);
    std::vector<float> samples(size_t(perFrame) * 2);
    std::unique_ptr<WavWriter> wav;
    if (!device) wav = std::make_unique<WavWriter>(out.c_str(), uint32_t(rate));
    float peak = 0;
    size_t clipped = 0;
    int seLeft = se ? seCount : 0, seNext = 0;
    if (!se && !audio.playMusic(channel, sound, params)) return 1;

    // One iteration per game frame, as the interpreter will drive it.
    for (int frame = 0; frame < totalFrames; frame++) {
        double t = double(frame) / fps;
        auto at = [&](double when) { return when >= 0 && frame == int(when * fps); };
        if (seLeft > 0 && frame >= seNext) {
            if (!audio.playSE(sound, params.volume, params.frequency)) return 1;
            seLeft--;
            seNext = frame + std::max(1, seEvery * fps / 1000);
        }
        if (at(stopAt)) audio.stopMusic(channel, fadeOut);
        if (at(replayAt)) audio.playMusic(channel, sound, params);
        if (at(freqAt)) {
            auto p = params;
            p.frequency = freqAtValue;
            audio.playMusic(channel, sound, p);
        }
        if (at(pauseAt)) audio.pause();
        if (at(resumeAt)) audio.resume();
        if (!se && frame % fps == 0)
            std::printf("t=%5.1fs position %d ms%s\n", t, audio.musicPositionMs(channel),
                        audio.musicPath(channel).empty() ? " (stopped)" : "");
        if (device) {
            SDL_Delay(1000 / fps);
        } else {
            audio.render(samples.data(), perFrame);
            for (float v : samples) {
                peak = std::max(peak, std::fabs(v));
                if (std::fabs(v) > 1) clipped++;
            }
            wav->write(samples.data(), samples.size());
        }
    }
    if (!device)
        std::printf("%d frames, peak %.3f, %zu samples over full scale\n", totalFrames * perFrame, peak, clipped);
    return 0;
}

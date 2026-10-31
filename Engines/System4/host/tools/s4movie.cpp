// Decodes a movie (.alm) as fast as possible in 1/60 s steps, pulling audio as a 44.1 kHz audio thread would, and
// writes a PNG of a frame halfway through. Usage: s4movie <movie.alm> [seconds] [out.png]
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "movie.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: s4movie <movie.alm> [seconds] [out.png]\n");
        return 2;
    }
    double limit = argc > 2 ? std::atof(argv[2]) : 0;
    const char *png = argc > 3 ? argv[3] : "s4movie.png";

    auto start = std::chrono::steady_clock::now();
    s4::Movie m;
    if (!m.open(argv[1])) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    double openMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::printf("%dx%d %.3f fps, duration %.3f s, audio %d Hz, open %.1f ms\n", m.width(), m.height(), m.frameRate(),
                m.duration(), m.sourceAudioRate(), openMs);

    double end = limit > 0 ? std::min(limit, m.duration()) : m.duration();
    double snapAt = end / 2;
    bool snapped = false;
    constexpr int kPull = s4::Movie::kAudioRate / 60;
    std::vector<float> pcm(size_t(kPull) * 2);
    long frames = 0, audioFrames = 0, underruns = 0;
    double sumSq = 0, lastFrameTime = 0;
    double t = 0;
    for (int step = 0; !m.ended() && (limit <= 0 || t < limit) && t < m.duration() + 10; ++step) {
        t = step / 60.0;
        if (m.advanceTo(t)) {
            ++frames;
            lastFrameTime = t;
            if (!snapped && t >= snapAt) {
                snapped = stbi_write_png(png, m.width(), m.height(), 4, m.frame().data(), m.width() * 4);
                std::printf("frame at %.3f s -> %s\n", t, png);
            }
        }
        int got = m.readAudio(pcm.data(), kPull);
        if (got < kPull && t > 0.5 && t < end - 0.5) ++underruns;
        audioFrames += got;
        for (int i = 0; i < got * 2; ++i) sumSq += double(pcm[size_t(i)]) * pcm[size_t(i)];
    }
    double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("frames %ld, last frame at %.3f s, decoded to %.3f s, audio frames %ld (%.3f s, rms %.4f), "
                "underruns %ld, ended %d\n",
                frames, lastFrameTime, t, audioFrames, double(audioFrames) / s4::Movie::kAudioRate,
                audioFrames ? std::sqrt(sumSq / double(audioFrames * 2)) : 0.0, underruns, int(m.ended()));
    std::printf("wall %.3f s, %.2fx realtime\n", wall, t / wall);
    return snapped ? 0 : 1;
}

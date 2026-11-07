// Plays a movie with vnffmovie against the wall clock, as a host would, and prints what came out: the format, the frames and the seconds
// of sound, and a checksum of every 30th frame so two runs can be compared. With -seek T it seeks to T seconds first.
// usage: vnffmovie_test [-seek T] [-loop] <movie>
#include "vnffmovie.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

static int64_t readFile(void *user, uint64_t offset, void *buffer, size_t length) {
    auto *f = static_cast<FILE *>(user);
    if (std::fseek(f, long(offset), SEEK_SET)) return -1;
    return int64_t(std::fread(buffer, 1, length, f));
}

int main(int argc, char **argv) {
    double seek = -1;
    int loop = 0;
    const char *path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "-seek") && i + 1 < argc) seek = std::atof(argv[++i]);
        else if (!std::strcmp(argv[i], "-loop")) loop = 1;
        else path = argv[i];
    }
    FILE *f = path ? std::fopen(path, "rb") : nullptr;
    if (!f) return std::fprintf(stderr, "usage: vnffmovie_test [-seek T] [-loop] <movie>\n"), 2;
    std::fseek(f, 0, SEEK_END);
    uint64_t size = uint64_t(std::ftell(f));
    vnffmovie_info info;
    char error[200] = "";
    vnffmovie *m = vnffmovie_open(readFile, f, size, loop, &info, error, sizeof error);
    if (!m) return std::printf("can't open: %s\n", error), 1;
    std::printf("%dx%d %.3f fps, %.2f s, sound %d Hz x %d\n", info.width, info.height, info.fps, info.duration, info.audio_rate, info.audio_channels);
    if (seek >= 0) vnffmovie_seek(m, seek);
    auto began = std::chrono::steady_clock::now();
    double t = seek > 0 ? seek : 0;
    long frames = 0, soundFrames = 0;
    std::vector<float> sound(4096 * 2);
    for (;;) {
        int changed = 0, ended = 0;
        const uint8_t *px = vnffmovie_frame(m, t, &changed, &ended);
        soundFrames += long(vnffmovie_audio(m, sound.data(), 2048));
        if (px && changed) {
            if (frames % 30 == 0) {
                uint32_t sum = 0;
                for (size_t i = 0; i < size_t(info.width) * size_t(info.height) * 4; i += 97) sum = sum * 31 + px[i];
                std::printf("frame %ld at %.3f checksum %08x\n", frames, t, sum);
            }
            frames++;
        }
        if (ended || (loop && t > 2 * info.duration + 1)) break;
        // The movie's clock is the wall clock from its first frame.
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        if (!px) began = std::chrono::steady_clock::now();
        else t = (seek > 0 ? seek : 0) + std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    }
    double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    std::printf("%ld frames, %.2f s of sound, clock %.2f s, played in %.2f s\n", frames, info.audio_rate ? double(soundFrames) / info.audio_rate : 0, t, took);
    vnffmovie_close(m);
    std::fclose(f);
}

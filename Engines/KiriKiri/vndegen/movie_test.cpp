// KiriKiriMovieTest: decodes movies with the engine's movie decoder, as fast as it can.
//   KiriKiriMovieTest <file> [--frames <count>] [--dump <ppm>]
//   KiriKiriMovieTest <archive.xp3> <entry> [--frames <count>] [--dump <ppm>]
// An entry is read through the engine's archive code, so archive ciphers apply. For each movie it
// prints the stream info, decodes every frame and all the audio, then seeks to the middle and
// checks that the first frame after the seek is at the target, then does the same for a rewind. --frames checks the number of
// frames decoded against another decoder's count, such as ffprobe -count_frames. --dump writes the
// frame in the middle of the movie as a PPM image. A second decoder then seeks to the middle before it
// has decoded anything, as a game that sets a frame before playing does. A third writes its audio to an
// output that stays full for a while, as a sound buffer is before the movie plays. A movie with few
// frames per second, such as one ffmpeg makes from color=r=1:d=20 and sine=d=20 as H.264 and AAC, reads
// far ahead for video. Exits nonzero if a check fails.
#include "tjsCommHead.h"
#include "TVPStorage.h"
#include "XP3Archive.h"
#include "movie_decoder.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

using Clock = std::chrono::steady_clock;

class FileStream : public tTJSBinaryStream {
public:
    explicit FileStream(FILE *file) : file(file) {}
    ~FileStream() { fclose(file); }
    tjs_uint64 Seek(tjs_int64 offset, tjs_int whence) override
    {
        fseeko(file, offset, whence);
        return ftello(file);
    }
    tjs_uint Read(void *buffer, tjs_uint size) override { return (tjs_uint)fread(buffer, 1, size, file); }
    tjs_uint Write(const void *, tjs_uint) override { return 0; }
    bool Flush() override { return true; }
    tjs_uint64 GetSize() override
    {
        off_t at = ftello(file);
        fseeko(file, 0, SEEK_END);
        off_t size = ftello(file);
        fseeko(file, at, SEEK_SET);
        return size;
    }

private:
    FILE *file;
};

// Takes all the audio at once.
class CountingSink : public MovieAudioSink {
public:
    size_t Queued() override { return 0; }
    void Write(const int16_t *, size_t frames, double pts) override
    {
        if (!this->frames)
            firstPts = pts;
        this->frames += frames;
    }
    void End() override {}
    size_t frames = 0;
    double firstPts = 0;
};

// Holds what it is given until Free, as a full sound buffer does, then plays it all at once.
class HeldSink : public MovieAudioSink {
public:
    size_t Queued() override { return full ? frames.load() : 0; }
    void Write(const int16_t *, size_t count, double) override { frames += count; }
    void End() override {}
    void Free() { full = false; }
    std::atomic<size_t> frames{0};

private:
    std::atomic<bool> full{true};
};

static int failures;

static void check(bool ok, const std::string &what)
{
    printf("%s: %s\n", ok ? "pass" : "FAIL", what.c_str());
    if (!ok)
        failures++;
}

static tTJSBinaryStream *openEntry(const char *archivePath, const char *entry)
{
    // Storage names are absolute.
    char path[PATH_MAX];
    if (!realpath(archivePath, path))
        return nullptr;
    tTVPArchive *archive = TVPOpenArchive(TVPNormalizeStorageName(ttstr(path)), true);
    if (!archive)
        return nullptr;
    std::string wanted = entry;
    std::transform(wanted.begin(), wanted.end(), wanted.begin(), ::tolower);
    tTJSBinaryStream *stream = nullptr;
    for (tjs_uint i = 0; i < archive->GetCount() && !stream; i++) {
        std::string name = archive->GetName(i).AsStdString();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name == wanted || (name.size() > wanted.size() &&
                               name.compare(name.size() - wanted.size(), wanted.size(), wanted) == 0 &&
                               name[name.size() - wanted.size() - 1] == '/'))
            stream = archive->CreateStreamByIndex(i);
    }
    archive->Release();
    return stream;
}

static void dump(const MovieFrame &frame, const MovieInfo &info, const char *path)
{
    FILE *out = fopen(path, "wb");
    fprintf(out, "P6\n%d %d\n255\n", info.width, info.height);
    for (size_t i = 0; i < frame.pixels.size(); i += 4)
        fwrite(&frame.pixels[i], 1, 3, out);
    fclose(out);
}

// Waits for the next frame, up to a few seconds.
static bool nextFrame(MovieDecoder &decoder, MovieFrame &frame)
{
    for (int tries = 0; tries < 5000; tries++) {
        if (decoder.PopFrame(frame))
            return true;
        if (decoder.VideoEnded())
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

int main(int argc, char *argv[])
{
    const char *dumpPath = nullptr;
    long expectedFrames = -1;
    std::vector<const char *> args;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dump") && i + 1 < argc)
            dumpPath = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            expectedFrames = atol(argv[++i]);
        else
            args.push_back(argv[i]);
    }
    if (args.empty() || args.size() > 2) {
        fprintf(stderr, "usage: %s <file> | <archive.xp3> <entry> [--frames <count>] [--dump <ppm>]\n", argv[0]);
        return 2;
    }
    auto openStream = [&]() -> tTJSBinaryStream * {
        if (args.size() == 2) {
            try {
                return openEntry(args[0], args[1]);
            } catch (const eTJSError &e) {
                fprintf(stderr, "%s\n", e.GetMessage().AsStdString().c_str());
                return nullptr;
            }
        }
        FILE *file = fopen(args[0], "rb");
        return file ? new FileStream(file) : nullptr;
    };
    tTJSBinaryStream *stream = openStream();
    if (!stream) {
        fprintf(stderr, "cannot open %s\n", args.back());
        return 1;
    }

    std::string error;
    std::unique_ptr<MovieDecoder> decoder = MovieDecoder::Open(stream, error);
    check(decoder != nullptr, "opens" + (error.empty() ? "" : ": " + error));
    if (!decoder)
        return 1;
    const MovieInfo info = decoder->Info(); // a copy, since later checks replace the decoder
    printf("%s, %.3f fps, %.2f s%s, %d audio stream(s)\n", info.format.c_str(), info.fps, info.duration,
           info.hardware ? ", VideoToolbox" : "", info.audioStreams);

    CountingSink sink;
    auto start = Clock::now();
    decoder->Start(&sink);
    MovieFrame frame;
    int64_t count = 0;
    double firstPts = -1, lastPts = -1;
    bool ordered = true, dumped = false;
    size_t lit = 0; // pixels of the middle frame that are not black
    while (nextFrame(*decoder, frame)) {
        if (count == 0)
            firstPts = frame.pts;
        ordered &= frame.pts > lastPts;
        lastPts = frame.pts;
        count++;
        if (!dumped && frame.pts >= info.duration / 2) {
            dumped = true;
            for (size_t i = 0; i < frame.pixels.size(); i += 4)
                lit += std::max({frame.pixels[i], frame.pixels[i + 1], frame.pixels[i + 2]}) > 16;
            if (dumpPath)
                dump(frame, info, dumpPath);
        }
        decoder->Recycle(std::move(frame));
    }
    double seconds = std::chrono::duration<double>(Clock::now() - start).count();
    for (int tries = 0; tries < 500 && !decoder->AudioEnded(); tries++)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::string decodeError = decoder->TakeError();
    double audioSeconds = info.audioRate ? sink.frames / (double)info.audioRate : 0;
    printf("%lld frames (%lld by VideoToolbox) from %.3f to %.3f s, %.2f s of audio from %.3f s, "
           "decoded in %.2f s (%.1fx)\n",
           (long long)count, (long long)decoder->Stats().hardwareFrames, firstPts, lastPts, audioSeconds,
           sink.firstPts, seconds, info.duration / seconds);
    check(decodeError.empty(), "decodes without errors" + (decodeError.empty() ? "" : ": " + decodeError));
    if (expectedFrames >= 0)
        check(count == expectedFrames, "decodes " + std::to_string(count) + " of " +
                                           std::to_string(expectedFrames) + " frames");
    // Containers may count preroll or a longer audio stream in their duration.
    check(count > 0 && lastPts >= info.duration - 1, "frames reach the end of the movie");
    check(ordered, "frames come in time order");
    check(lit > 0, "the middle frame has an image, " + std::to_string(lit * 100 / (info.width * info.height)) +
                       "% of its pixels lit");
    if (info.audioRate)
        check(std::fabs(audioSeconds - info.duration) < 0.5, "audio covers the movie");

    // The middle, then a rewind, as a looping movie does.
    for (double target : {info.duration / 2, 0.0}) {
        decoder->Seek(target, [] {});
        bool landed = nextFrame(*decoder, frame);
        double expected = std::max(target, firstPts);
        check(landed && std::fabs(frame.pts - expected) <= 1.5 / info.fps,
              "a seek to " + std::to_string(target) + " s lands at " + std::to_string(landed ? frame.pts : -1));
        if (landed)
            decoder->Recycle(std::move(frame));
    }

    // MPEG-1 video may carry its sequence header only at the start, so this decoder must not skip it.
    double target = info.duration / 2;
    decoder = MovieDecoder::Open(openStream(), error);
    decoder->Start(&sink);
    decoder->Seek(target, [] {});
    bool landed = nextFrame(*decoder, frame);
    check(landed && std::fabs(frame.pts - target) <= 1.5 / info.fps,
          "a seek before the first frame to " + std::to_string(target) + " s lands at " +
              std::to_string(landed ? frame.pts : -1));

    // A sound buffer drops what it is given past its limit, so audio must wait for room, however far
    // the decoder reads ahead for video.
    if (info.audioRate) {
        HeldSink held;
        decoder = MovieDecoder::Open(openStream(), error);
        decoder->Start(&held);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        double heldSeconds = held.frames / (double)info.audioRate;
        check(heldSeconds < 1, "audio waits while the output is full (" + std::to_string(heldSeconds) + " s written)");
        held.Free();
        while (nextFrame(*decoder, frame))
            decoder->Recycle(std::move(frame));
        for (int tries = 0; tries < 2000 && !decoder->AudioEnded(); tries++)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        double total = held.frames / (double)info.audioRate;
        check(std::fabs(total - info.duration) < 0.5,
              "the held audio still covers the movie (" + std::to_string(total) + " s)");
    }
    printf("%s\n", failures ? "movie test failed" : "movie test passed");
    return failures ? 1 : 0;
}

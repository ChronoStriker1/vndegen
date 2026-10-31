// Sound: Ogg Vorbis and WAV players mixed into 44.1 kHz stereo float. The game thread controls players by handle;
// the host's audio thread pulls the mix with render().
#pragma once
#include <cstdint>
#include <functional>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace s4 {

struct Decoder;

class Audio {
public:
    Audio();
    ~Audio();
    // Makes a player for the file's bytes. Returns 0 when the data can't be decoded.
    int open(std::shared_ptr<const std::vector<uint8_t>> data);
    void close(int h);
    void play(int h);
    void stop(int h);
    void pause(int h, bool paused);
    bool playing(int h);
    bool paused(int h);
    // Fades to volume (0..100) over ms, stopping at the end when stop is set. ms 0 sets the volume now.
    void fade(int h, int ms, int volume, bool stop);
    bool fading(int h);
    void stopFade(int h);
    void setLoops(int h, int count);  // 0 loops forever, n plays n times
    int loops(int h);
    // Loop points in samples; end 0 is the end of the data.
    void setLoopPoints(int h, int64_t start, int64_t end);
    int64_t positionMs(int h);
    int64_t lengthMs(int h);
    int64_t positionSamples(int h);
    int64_t lengthSamples(int h);
    void seekMs(int h, int64_t ms);
    void setGroupVolume(int group, int volume);  // master, music and so on: 0..100
    void setGroup(int h, int group);
    // A stream mixed in with the players, such as a movie's sound: read fills interleaved stereo at 44.1 kHz and
    // returns the frames written. It runs on the audio thread; once clearStream returns it is no longer called.
    void setStream(std::function<int(float *, int)> read, int group, int volume);
    void setStreamVolume(int volume);  // 0..100
    void clearStream();
    void render(float *out, int frames);  // the audio thread
    // Renders that took more than half the time their frames last, which risks the host running out of sound, and
    // the longest render in microseconds. Taking them resets them.
    struct Timing { int slow = 0; int longestUs = 0; };
    Timing takeTiming();

private:
    struct Player;
    void renderMix(float *out, int frames);
    std::mutex mu_;
    std::map<int, std::unique_ptr<Player>> players_;
    std::map<int, int> groupVolume_;
    int next_ = 1;
    std::function<int(float *, int)> stream_;
    int streamGroup_ = 0;
    float streamVolume_ = 1;
    std::vector<float> streamBuf_;
    std::atomic<int> slow_{0}, longestUs_{0};
    bool mute_;
};

}  // namespace s4

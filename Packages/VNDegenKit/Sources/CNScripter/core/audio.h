// The runtime's audio: decoders for Ogg Vorbis, PCM WAV and MP3, and a mixer the host pulls 44.1 kHz stereo float
// samples from on its audio thread. A decoder thread of the mixer's own decodes each sound ahead into a ring buffer, so
// the audio thread only resamples and mixes, and a slow decode can't make it miss a deadline. The engine thread
// starts and stops sounds; it and the audio thread lock the mixer only briefly.
#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "common.h"

namespace vnns {

class AudioSource {
public:
    virtual ~AudioSource() = default;
    // Writes up to frames stereo frames at the source's own rate; returns the number written, 0 at the end.
    virtual int read(float *out, int frames) = 0;
    virtual bool rewind(double seconds) = 0;
    virtual int rate() const = 0;
};

std::unique_ptr<AudioSource> openAudio(Bytes data);

class AudioMixer {
public:
    static constexpr int kRate = 44100;
    // dwave channels are 0..49; these follow them.
    static constexpr int kBgm = 50, kWave = 51, kMovie = 52, kLoopBgmTail = 53, kChannels = 54;

    AudioMixer();
    ~AudioMixer();
    AudioMixer(const AudioMixer &) = delete;
    AudioMixer &operator=(const AudioMixer &) = delete;

    // Starts data on a channel, replacing what it played. loopStart is where a loop restarts, in seconds.
    bool play(int channel, Bytes data, bool loop, int volume, int fadeInMs = 0, double loopStart = 0);
    void stop(int channel, int fadeOutMs = 0);
    bool playing(int channel);
    void setVolume(int channel, int volume);  // 0..100
    void setPaused(bool paused);
    // A movie's decoded sound, at the movie's sample rate; the mixer resamples it to its own.
    void queueMovieSamples(const float *interleaved, int frames, int volume, int rate);
    void clearMovieSamples();
    int movieQueuedFrames();

    void render(float *out, int frames);

private:
    // One sound being played: the decoder thread fills `ring` from `src`, the audio thread drains it. A stream is
    // never reused, so a sound that is stopped or replaced takes its unplayed samples with it.
    struct Stream {
        std::unique_ptr<AudioSource> src;  // the decoder thread's alone once playing
        int rate = kRate;
        bool loop = false;
        double loopStart = 0;
        std::vector<float> ring;            // stereo frames
        size_t capacity = 0;                // in frames
        std::atomic<size_t> written{0}, read{0};
        std::atomic<bool> eof{false};       // the decoder has written everything there is
        std::atomic<bool> dropped{false};   // no channel plays it any more
    };
    struct Channel {
        std::shared_ptr<Stream> stream;
        float volume = 1, gain = 1, fadeStep = 0;  // gain ramps by fadeStep per frame
        bool stopAfterFade = false;
        double pos = 0;  // resampler phase between cur and next
        float cur[2] = {0, 0}, next[2] = {0, 0};
        bool primed = false, ended = false;
        bool lastFrame = false;  // cur is the sound's last frame: it ends once that has played
    };
    bool pull(Channel &c, float &l, float &r);
    static bool popFrame(Stream &s, float *frame);
    void release(Channel &c);  // marks its stream dropped; call with mutex_ held
    void decodeLoop();
    static void fill(Stream &s);

    std::mutex mutex_;
    std::array<Channel, kChannels> ch_;
    std::vector<float> movie_;
    size_t movieRead_ = 0;
    double movieSrcPos_ = 0;  // resampling position, in source frames from the start of the next block
    float movieLast_[2] = {0, 0};  // the previous block's last frame
    float movieVolume_ = 1;
    bool paused_ = false;

    std::mutex decodeMutex_;  // guards streams_ and quit_
    std::condition_variable decodeWake_;
    std::vector<std::shared_ptr<Stream>> streams_;  // what the decoder thread keeps filled
    bool quit_ = false;
    std::thread decoder_;
};

}  // namespace vnns

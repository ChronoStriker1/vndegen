// The runtime's audio: decoders for Ogg Vorbis and PCM WAV, and a mixer the host pulls 44.1 kHz stereo float samples
// from on its audio thread. The engine thread starts and stops sounds; both sides lock the mixer briefly.
#pragma once

#include <array>
#include <memory>
#include <mutex>
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

    // Starts data on a channel, replacing what it played. loopStart is where a loop restarts, in seconds.
    bool play(int channel, Bytes data, bool loop, int volume, int fadeInMs = 0, double loopStart = 0);
    void stop(int channel, int fadeOutMs = 0);
    bool playing(int channel);
    void setVolume(int channel, int volume);  // 0..100
    void setPaused(bool paused);
    // Movie audio arrives decoded, at 44.1 kHz stereo.
    void queueMovieSamples(const float *interleaved, int frames, int volume);
    void clearMovieSamples();
    int movieQueuedFrames();

    void render(float *out, int frames);

private:
    struct Channel {
        std::unique_ptr<AudioSource> src;
        bool loop = false;
        double loopStart = 0;
        float volume = 1, gain = 1, fadeStep = 0;  // gain ramps by fadeStep per frame
        bool stopAfterFade = false;
        double pos = 0;  // resampler phase
        std::vector<float> buf;
        int bufFrames = 0, bufIndex = 0;
        bool ended = false;
    };
    bool pull(Channel &c, float &l, float &r);
    std::mutex mutex_;
    std::array<Channel, kChannels> ch_;
    std::vector<float> movie_;
    size_t movieRead_ = 0;
    float movieVolume_ = 1;
    bool paused_ = false;
};

}  // namespace vnns

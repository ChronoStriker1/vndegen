// Movie playback for .alm files: MPEG program streams carrying MPEG-1 video and MP2 audio. The demuxer here reads
// both MPEG-1 system streams and MPEG-2 program streams (some later games wrap MPEG-1 video in the MPEG-2
// container); pl_mpeg decodes the video and audio. The file is streamed, never loaded whole.
//
// Threads: open, advanceTo, frame, ended and close belong to one thread. readAudio may run on another (the host's
// audio thread). Stop the audio thread before destroying the Movie.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

struct plm_buffer_t;
struct plm_video_t;
struct plm_audio_t;

namespace s4 {

class Movie {
public:
    static constexpr int kAudioRate = 44100;

    Movie() = default;
    ~Movie();
    Movie(const Movie &) = delete;
    Movie &operator=(const Movie &) = delete;

    // Opens the file. False when it is missing or has no MPEG-1 video stream.
    bool open(const std::string &path);
    int width() const { return width_; }
    int height() const { return height_; }
    double frameRate() const { return fps_; }
    double duration() const { return duration_; }  // seconds, from the first and last video timestamps
    int sourceAudioRate() const { return srcRate_; }  // the MP2 stream's rate; 0 without audio

    // Decodes video up to the playback time and keeps half a second of sound decoded ahead of what has played.
    // The playback time is seconds (the game clock since the start), moved ahead when the sound shows the clock has
    // dropped more than 50 ms (see advanceTo), so a clock that falls behind real time neither starves the sound nor
    // lets the picture lag it. Returns true
    // when frame() changed. Time only moves forward; a large jump decodes every frame in between.
    bool advanceTo(double seconds);
    // RGBA8, width*height*4, alpha 255. Black until the first frame.
    const std::vector<uint8_t> &frame() const { return frame_; }
    // True once the video has no more frames and the audio has been played out (or playback time passed its end).
    bool ended();

    // Interleaved stereo float at 44.1 kHz. Returns the frames written; 0 when nothing is buffered.
    int readAudio(float *out, int frames);
    // Reads that came up short after the sound started and before it ended, and the frames missing from them.
    // Each is a gap the listener hears as a click.
    struct Dry { int reads = 0; int64_t frames = 0; };
    Dry dry();
    void close();

private:
    struct Packet {
        int stream = 0;  // 0xE0 video, 0xC0 audio
        double pts = -1;
        const uint8_t *data = nullptr;
        size_t size = 0;
    };

    void seekFile(long pos);
    bool fill(size_t need);
    bool nextPacket(Packet &p);
    bool probe();
    void scanDuration(long fileSize);
    void pump(plm_buffer_t *wanted);
    void endStreams();
    void pushAudio(const float *in, int frames);
    void writeRing(const float *in, size_t frames);
    static void onVideoLoad(plm_buffer_t *, void *user);
    static void onAudioLoad(plm_buffer_t *, void *user);

    std::FILE *file_ = nullptr;
    std::vector<uint8_t> in_;  // read window over the file
    size_t inPos_ = 0, inLen_ = 0;
    long winOff_ = 0;  // file offset of in_[0]
    bool eof_ = false;
    bool needPack_ = false;  // after a seek, skip to the next pack header before trusting start codes

    plm_buffer_t *videoBuf_ = nullptr, *audioBuf_ = nullptr;  // owned by the decoders
    plm_video_t *video_ = nullptr;
    plm_audio_t *audio_ = nullptr;
    bool videoDone_ = false, audioDone_ = false;

    int width_ = 0, height_ = 0, srcRate_ = 0;
    double fps_ = 0, duration_ = 0, firstVideoPts_ = -1, firstAudioPts_ = -1, now_ = 0;
    std::vector<uint8_t> frame_;

    // Resampler state: pending source frames (stereo) with one frame of history, and the read position in them.
    std::vector<float> rsIn_, rsOut_;
    double rsPos_ = 1;
    int64_t audioSkew_ = 0;  // output frames of the sound's lead-in to drop, when it starts before the picture
    double audioStart_ = 0;  // seconds into the movie the sound starts; negative when it starts first
    int64_t audioPushed_ = 0;  // output frames produced so far, in movie time

    std::mutex mu_;  // guards the ring and the counts below it
    std::vector<float> ring_;
    size_t ringHead_ = 0, ringCount_ = 0;  // in frames
    bool audioEnded_ = false;  // the decoder has pushed its last samples
    int64_t audioRead_ = 0;    // frames the host has played
    double minLead_ = INFINITY;  // the sound read's smallest lead over the game clock: the host's output latency
    int64_t leadSilence_ = 0;  // silence still to play before the sound, when it starts after the picture
    Dry dry_;
};

}  // namespace s4

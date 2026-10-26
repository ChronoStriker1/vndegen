// Decodes a movie with FFmpeg on a worker thread. Video comes out as RGBA frames, the engine's
// pixel order, and audio as interleaved signed 16-bit PCM written to an AudioSink. Times are in
// seconds from the start of the movie.
#ifndef VNKK_MOVIE_DECODER_H
#define VNKK_MOVIE_DECODER_H

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace TJS { class tTJSBinaryStream; }
using TJS::tTJSBinaryStream;
struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVIOContext;
struct AVPacket;
struct SwrContext;
struct SwsContext;

// Where decoded audio goes. The decoder calls these on its worker thread, with its lock held.
class MovieAudioSink {
public:
    virtual ~MovieAudioSink() {}
    // Audio written but not yet played, in sample frames.
    virtual size_t Queued() = 0;
    virtual void Write(const int16_t *samples, size_t frames, double pts) = 0;
    // No more audio will be written until the next seek.
    virtual void End() = 0;
};

struct MovieInfo {
    int width = 0, height = 0;
    double fps = 0;
    double duration = 0;
    int audioRate = 0, audioChannels = 0; // 0 without audio
    int audioStreams = 0;
    std::string format;     // container and codecs, for the log
    bool hardware = false;  // video decodes through VideoToolbox
};

struct MovieFrame {
    double pts = 0;
    std::vector<uint8_t> pixels; // width * height RGBA, rows packed
};

struct MovieStats {
    int64_t videoFrames = 0;  // decoded
    int64_t hardwareFrames = 0; // of those, decoded by VideoToolbox
    int64_t audioFrames = 0;  // sample frames written to the sink
};

class MovieDecoder {
public:
    // Takes ownership of stream. On failure returns null and sets error.
    static std::unique_ptr<MovieDecoder> Open(tTJSBinaryStream *stream, std::string &error);
    ~MovieDecoder();

    const MovieInfo &Info() const { return info; }

    // Starts decoding. Audio goes to sink, which must outlive the decoder; null drops the audio.
    void Start(MovieAudioSink *sink);

    // Drops every queued frame and decodes again from time t. flush runs under the decoder's lock,
    // so the caller can clear the sink's audio without the worker writing stale audio after it.
    template<typename Flush> void Seek(double t, Flush flush)
    {
        std::lock_guard<std::mutex> lock(mutex);
        RequestSeek(t);
        flush();
    }

    // The oldest decoded frame, moved out of the queue, if one is queued.
    bool PopFrame(MovieFrame &frame);
    // The pts of the oldest decoded frame, or a negative number when none is queued.
    double PeekFramePts();
    // Hands a frame's buffer back for reuse.
    void Recycle(MovieFrame &&frame);

    // No more frames will be queued until the next seek.
    bool VideoEnded();
    // No more audio will be written until the next seek.
    bool AudioEnded();
    MovieStats Stats();
    // The first decode error since the last call, or an empty string.
    std::string TakeError();

private:
    MovieDecoder() = default;
    bool OpenStreams(std::string &error);
    void Prime();
    void RequestSeek(double t);
    void Run();
    void DoSeek(std::unique_lock<std::mutex> &lock);
    int SeekContainer(double t, bool fromFirstByte);
    // Queues a video packet or decodes an audio one, and frees what it does not keep.
    void Route(AVPacket *packet, std::unique_lock<std::mutex> &lock);
    void DecodeVideo(AVPacket *packet, std::unique_lock<std::mutex> &lock);
    void DecodeAudio(AVPacket *packet, std::unique_lock<std::mutex> &lock);
    void QueueFrame(AVFrame *frame, std::unique_lock<std::mutex> &lock);
    void WriteAudio(AVFrame *frame, std::unique_lock<std::mutex> &lock);
    double Seconds(int64_t pts, int streamIndex) const;
    void Fail(const std::string &message);

    MovieInfo info;
    tTJSBinaryStream *stream = nullptr;
    AVIOContext *io = nullptr;
    AVFormatContext *format = nullptr;
    AVCodecContext *video = nullptr, *audio = nullptr;
    int videoIndex = -1, audioIndex = -1;
    int64_t origin = 0; // the container's start time, in AV_TIME_BASE units
    SwsContext *scaler = nullptr;
    SwrContext *resampler = nullptr;
    AVFrame *decoded = nullptr, *transferred = nullptr;
    std::vector<int16_t> pcm;

    std::thread worker;
    std::mutex mutex;
    std::condition_variable wake;
    MovieAudioSink *sink = nullptr;
    bool quit = false;
    // Set by RequestSeek, cleared by the worker once it has seeked.
    bool seekPending = false;
    double seekTarget = 0;
    // Frames and audio before this time are decoded but dropped, so a seek lands on its target
    // rather than the keyframe before it.
    double dropBefore = 0;
    // Bumped by every seek. Work the worker started before a seek is dropped when it finishes.
    uint64_t generation = 0;
    bool demuxEnded = false, videoEnded = false, audioEnded = false;
    std::deque<AVPacket *> videoPackets;
    size_t videoPacketBytes = 0;
    std::deque<MovieFrame> frames;
    std::vector<std::vector<uint8_t>> spareBuffers;
    MovieStats stats;
    std::string error;
};

#endif

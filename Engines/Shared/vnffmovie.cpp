#include "vnffmovie.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

// How far ahead the decode thread runs: a few frames, since a 1280x720 frame is 3.7 MB, and sound up to a few seconds
// past the picture, which matters once the picture has ended before the sound.
constexpr size_t kMaxFrames = 4;
constexpr double kSoundAhead = 4.0;
// Sound whose timestamp is further than this from where the queued sound ends gets silence before it, or loses its
// overlap, so it stays on the picture's clock. Smaller differences are timestamp rounding and play as they come, and
// they can't add up, since the end is counted from the sound handed out.
constexpr double kSoundSlack = 0.05;
// Sound this far behind the picture is dropped, as for a movie whose sound nobody plays, or plays again after another's.
constexpr double kSoundBehind = 0.1;
constexpr int kIoBuffer = 64 << 10;

struct Frame {
    double pts = 0;
    std::vector<uint8_t> pixels;
};

}  // namespace

struct vnffmovie {
    vnffmovie_read_fn read = nullptr;
    void *user = nullptr;
    uint64_t size = 0, position = 0;
    bool loop = false;
    vnffmovie_info info{};

    AVIOContext *io = nullptr;
    AVFormatContext *format = nullptr;
    AVCodecContext *video = nullptr, *audio = nullptr;
    int videoIndex = -1, audioIndex = -1;
    SwsContext *scaler = nullptr;
    SwrContext *resampler = nullptr;
    AVFrame *decoded = nullptr;
    AVPacket *packet = nullptr;
    double start = 0;  // the video stream's first time, which counts as 0

    std::thread worker;
    std::mutex mutex;
    std::condition_variable wake;
    bool quit = false, finished = false, seekPending = false, passHadFrame = false;
    double seekTarget = 0, dropBefore = 0, loopOffset = 0, lastPts = 0;
    // The movie time of the sound at the front of the queue. Each pull moves it on by what was asked for, since the host
    // plays silence for what the queue doesn't have yet.
    double playClock = 0;
    double soundEnd = 0;  // the movie time the queued sound reaches
    double pictureTime = 0;  // the host's clock at its last frame asked for
    // The movie time of the next decoded sound, from the last timestamp plus the sound since: WMA stamps only a packet's
    // first frame. Below 0 when no timestamp has come since a seek or loop.
    double audioClock = 0;
    std::deque<Frame> frames;
    std::deque<float> sound;
    Frame shown;
    bool hasShown = false;
};

namespace {

int readPacket(void *opaque, uint8_t *buffer, int size) {
    auto *m = static_cast<vnffmovie *>(opaque);
    int64_t got = m->read(m->user, m->position, buffer, size_t(size));
    if (got < 0) return AVERROR(EIO);
    if (got == 0) return AVERROR_EOF;
    m->position += uint64_t(got);
    return int(got);
}

int64_t seekStream(void *opaque, int64_t offset, int whence) {
    auto *m = static_cast<vnffmovie *>(opaque);
    if (whence & AVSEEK_SIZE) return int64_t(m->size);
    int64_t base = 0;
    switch (whence & ~AVSEEK_FORCE) {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = int64_t(m->position); break;
    case SEEK_END: base = int64_t(m->size); break;
    default: return -1;
    }
    if (base + offset < 0) return -1;
    m->position = uint64_t(base + offset);
    return int64_t(m->position);
}

AVCodecContext *openCodec(AVStream *stream) {
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) return nullptr;
    AVCodecContext *context = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(context, stream->codecpar);
    context->pkt_timebase = stream->time_base;
    context->thread_count = 0;
    if (avcodec_open2(context, codec, nullptr) < 0) avcodec_free_context(&context);
    return context;
}

double seconds(const vnffmovie *m, int64_t pts, int index) {
    if (pts == AV_NOPTS_VALUE) return m->lastPts;
    return double(pts) * av_q2d(m->format->streams[index]->time_base) - m->start;
}

void destroy(vnffmovie *m) {
    sws_freeContext(m->scaler);
    swr_free(&m->resampler);
    av_frame_free(&m->decoded);
    av_packet_free(&m->packet);
    avcodec_free_context(&m->video);
    avcodec_free_context(&m->audio);
    if (m->format) avformat_close_input(&m->format);
    if (m->io) {
        av_freep(&m->io->buffer);
        avio_context_free(&m->io);
    }
    delete m;
}

void takeVideo(vnffmovie *m) {
    AVFrame *f = m->decoded;
    double pts = seconds(m, f->best_effort_timestamp, m->videoIndex) + m->loopOffset;
    m->lastPts = std::max(m->lastPts, pts);
    m->passHadFrame = true;
    if (pts + 0.5 / m->info.fps < m->dropBefore) return;
    Frame out;
    out.pts = pts;
    out.pixels.resize(size_t(m->info.width) * size_t(m->info.height) * 4);
    m->scaler = sws_getCachedContext(m->scaler, f->width, f->height, AVPixelFormat(f->format), m->info.width, m->info.height,
                                     AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    // sws_scale reads four planes and strides whatever the format.
    uint8_t *planes[4] = {out.pixels.data(), nullptr, nullptr, nullptr};
    int strides[4] = {m->info.width * 4, 0, 0, 0};
    if (m->scaler) sws_scale(m->scaler, f->data, f->linesize, 0, f->height, planes, strides);
    std::lock_guard<std::mutex> lock(m->mutex);
    if (!m->seekPending) m->frames.push_back(std::move(out));
}

void takeAudio(vnffmovie *m) {
    AVFrame *f = m->decoded;
    if (f->best_effort_timestamp != AV_NOPTS_VALUE) m->audioClock = seconds(m, f->best_effort_timestamp, m->audioIndex) + m->loopOffset;
    else if (m->audioClock < 0) {
        // After a seek, sound with no time yet can't be placed; a new pass carries on from the sound before.
        if (m->dropBefore > 0) return;
        m->audioClock = m->soundEnd;
    }
    double pts = m->audioClock;
    m->audioClock += double(f->nb_samples) / double(m->info.audio_rate);
    int outFrames = swr_get_out_samples(m->resampler, f->nb_samples);
    if (outFrames <= 0) return;
    std::vector<float> pcm(size_t(outFrames) * size_t(m->info.audio_channels));
    uint8_t *out[1] = {reinterpret_cast<uint8_t *>(pcm.data())};
    int got = swr_convert(m->resampler, out, outFrames, const_cast<const uint8_t **>(f->extended_data), f->nb_samples);
    if (got <= 0) return;
    size_t channels = size_t(m->info.audio_channels), rate = size_t(m->info.audio_rate);
    std::lock_guard<std::mutex> lock(m->mutex);
    if (m->seekPending) return;
    // Sound before a seek's target goes with the frames. Otherwise the packet goes where its timestamp says: after
    // silence over a gap, or without what overlaps the sound queued or already handed out.
    double tail = m->playClock + double(m->sound.size() / channels) / double(rate), skipTo = m->dropBefore;
    if (pts > tail + kSoundSlack) m->sound.insert(m->sound.end(), size_t((pts - tail) * double(rate)) * channels, 0.0f);
    else if (pts < tail - kSoundSlack) skipTo = std::max(skipTo, tail);
    size_t skip = pts < skipTo ? std::min(size_t(got), size_t((skipTo - pts) * double(rate))) : 0;
    m->sound.insert(m->sound.end(), pcm.begin() + std::ptrdiff_t(skip * channels), pcm.begin() + std::ptrdiff_t(size_t(got) * channels));
    m->soundEnd = m->playClock + double(m->sound.size() / channels) / double(rate);
}

// Drops the sound that has fallen behind the picture.
void dropLateSound(vnffmovie *m) {
    if (!m->info.audio_channels) return;
    size_t channels = size_t(m->info.audio_channels);
    double late = m->pictureTime - kSoundBehind - m->playClock;
    if (late <= 0) return;
    size_t drop = std::min(m->sound.size() / channels, size_t(late * double(m->info.audio_rate)));
    m->sound.erase(m->sound.begin(), m->sound.begin() + std::ptrdiff_t(drop * channels));
    m->playClock += double(drop) / double(m->info.audio_rate);
}

// Sends a packet, or null to drain, and takes each frame that comes out. A decoder that refuses the packet until its
// output is read gets it again.
void decode(vnffmovie *m, AVCodecContext *context, AVPacket *packet) {
    for (;;) {
        int sent = avcodec_send_packet(context, packet), received;
        while ((received = avcodec_receive_frame(context, m->decoded)) >= 0) {
            if (context == m->video) takeVideo(m);
            else takeAudio(m);
            av_frame_unref(m->decoded);
        }
        if (sent != AVERROR(EAGAIN) || received != AVERROR(EAGAIN)) break;
    }
}

// False when the container can't seek there.
bool restart(vnffmovie *m, double target) {
    if (av_seek_frame(m->format, -1, int64_t((target + m->start) * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD) < 0) return false;
    avcodec_flush_buffers(m->video);
    if (m->audio) avcodec_flush_buffers(m->audio);
    m->passHadFrame = false;
    m->audioClock = -1;
    return true;
}

void run(vnffmovie *m) {
    std::unique_lock<std::mutex> lock(m->mutex);
    while (!m->quit) {
        if (m->seekPending) {
            m->seekPending = false;
            m->frames.clear();
            m->sound.clear();
            m->hasShown = false;
            m->finished = false;
            m->loopOffset = 0;
            m->lastPts = 0;
            m->dropBefore = m->soundEnd = m->playClock = m->pictureTime = m->seekTarget;
            lock.unlock();
            bool restarted = restart(m, m->dropBefore);
            lock.lock();
            if (!restarted) m->finished = true;
            continue;
        }
        if (m->finished || m->frames.size() >= kMaxFrames || m->soundEnd - m->pictureTime > kSoundAhead) {
            m->wake.wait(lock);
            continue;
        }
        lock.unlock();
        int result = av_read_frame(m->format, m->packet);
        if (result < 0) {
            decode(m, m->video, nullptr);
            if (m->audio) decode(m, m->audio, nullptr);
            // A read error ends the movie, as does a loop that can't start over or whose last pass showed nothing.
            bool atEnd = !m->format->pb->error && (result == AVERROR_EOF || avio_feof(m->format->pb));
            bool again = atEnd && m->loop && m->passHadFrame;
            if (again) {
                // The next pass carries on the clock from one frame after this pass's last.
                m->loopOffset = m->lastPts + 1 / m->info.fps;
                m->dropBefore = 0;
                again = restart(m, 0);
            }
            lock.lock();
            if (!again) m->finished = true;
            continue;
        }
        if (m->packet->stream_index == m->videoIndex) decode(m, m->video, m->packet);
        else if (m->packet->stream_index == m->audioIndex) decode(m, m->audio, m->packet);
        av_packet_unref(m->packet);
        lock.lock();
    }
}

void fail(char *error, size_t size, const std::string &why) {
    if (error && size) std::snprintf(error, size, "%s", why.c_str());
}

}  // namespace

extern "C" vnffmovie *vnffmovie_open(vnffmovie_read_fn read, void *user, uint64_t size, int loop, vnffmovie_info *info,
                                     char *error, size_t errorSize) {
    av_log_set_level(AV_LOG_ERROR);
    auto *m = new vnffmovie;
    m->read = read;
    m->user = user;
    m->size = size;
    m->loop = loop != 0;
    auto *buffer = static_cast<uint8_t *>(av_malloc(kIoBuffer));
    m->io = avio_alloc_context(buffer, kIoBuffer, 0, m, readPacket, nullptr, seekStream);
    m->format = avformat_alloc_context();
    m->format->pb = m->io;
    m->format->flags |= AVFMT_FLAG_CUSTOM_IO;
    if (avformat_open_input(&m->format, nullptr, nullptr, nullptr) < 0) {
        m->format = nullptr;  // freed by avformat_open_input
        fail(error, errorSize, "unrecognized container");
        destroy(m);
        return nullptr;
    }
    if (avformat_find_stream_info(m->format, nullptr) < 0 ||
        (m->videoIndex = av_find_best_stream(m->format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0)) < 0) {
        fail(error, errorSize, "no video stream");
        destroy(m);
        return nullptr;
    }
    AVStream *stream = m->format->streams[m->videoIndex];
    if (!(m->video = openCodec(stream)) || m->video->width <= 0 || m->video->height <= 0) {
        fail(error, errorSize, std::string("no decoder for ") + avcodec_get_name(stream->codecpar->codec_id));
        destroy(m);
        return nullptr;
    }
    m->info.width = m->video->width;
    m->info.height = m->video->height;
    AVRational rate = av_guess_frame_rate(m->format, stream, nullptr);
    m->info.fps = rate.num > 0 && rate.den > 0 ? av_q2d(rate) : 30;
    if (stream->start_time != AV_NOPTS_VALUE) m->start = std::max(0.0, double(stream->start_time) * av_q2d(stream->time_base));
    if (m->format->duration > 0) m->info.duration = double(m->format->duration) / AV_TIME_BASE;
    m->audioIndex = av_find_best_stream(m->format, AVMEDIA_TYPE_AUDIO, -1, m->videoIndex, nullptr, 0);
    if (m->audioIndex >= 0 && (m->audio = openCodec(m->format->streams[m->audioIndex]))) {
        AVChannelLayout layout = m->audio->ch_layout.nb_channels == 1 ? AVChannelLayout(AV_CHANNEL_LAYOUT_MONO)
                                                                      : AVChannelLayout(AV_CHANNEL_LAYOUT_STEREO);
        if (swr_alloc_set_opts2(&m->resampler, &layout, AV_SAMPLE_FMT_FLT, m->audio->sample_rate, &m->audio->ch_layout,
                                m->audio->sample_fmt, m->audio->sample_rate, 0, nullptr) >= 0 &&
            swr_init(m->resampler) >= 0) {
            m->info.audio_rate = m->audio->sample_rate;
            m->info.audio_channels = layout.nb_channels;
        } else {
            // The movie plays silent rather than not at all.
            avcodec_free_context(&m->audio);
            swr_free(&m->resampler);
            m->audioIndex = -1;
        }
    } else {
        m->audioIndex = -1;
    }
    m->decoded = av_frame_alloc();
    m->packet = av_packet_alloc();
    if (info) *info = m->info;
    m->worker = std::thread(run, m);
    return m;
}

extern "C" const uint8_t *vnffmovie_frame(vnffmovie *m, double time, int *changed, int *ended) {
    std::lock_guard<std::mutex> lock(m->mutex);
    bool moved = false;
    while (!m->frames.empty() && (m->frames.front().pts <= time || !m->hasShown)) {
        m->shown = std::move(m->frames.front());
        m->frames.pop_front();
        m->hasShown = moved = true;
    }
    // A later clock lets the decoder run on, if sound far enough ahead held it.
    if (moved || time > m->pictureTime) m->wake.notify_one();
    m->pictureTime = std::max(m->pictureTime, time);
    dropLateSound(m);
    if (changed) *changed = moved;
    // By the clock rather than by what's been pulled, so a movie whose sound nobody plays ends too.
    if (ended) *ended = m->finished && m->frames.empty() && time >= m->shown.pts && time >= m->soundEnd;
    return m->hasShown ? m->shown.pixels.data() : nullptr;
}

extern "C" size_t vnffmovie_audio(vnffmovie *m, float *out, size_t frames) {
    std::lock_guard<std::mutex> lock(m->mutex);
    if (!m->info.audio_channels) return 0;
    size_t channels = size_t(m->info.audio_channels), got = std::min(frames, m->sound.size() / channels);
    std::copy(m->sound.begin(), m->sound.begin() + std::ptrdiff_t(got * channels), out);
    m->sound.erase(m->sound.begin(), m->sound.begin() + std::ptrdiff_t(got * channels));
    m->playClock += double(frames) / double(m->info.audio_rate);
    return got;
}

extern "C" void vnffmovie_seek(vnffmovie *m, double time) {
    std::lock_guard<std::mutex> lock(m->mutex);
    m->seekPending = true;
    m->seekTarget = std::max(0.0, time);
    m->wake.notify_one();
}

extern "C" void vnffmovie_close(vnffmovie *m) {
    if (!m) return;
    {
        std::lock_guard<std::mutex> lock(m->mutex);
        m->quit = true;
    }
    m->wake.notify_one();
    if (m->worker.joinable()) m->worker.join();
    destroy(m);
}

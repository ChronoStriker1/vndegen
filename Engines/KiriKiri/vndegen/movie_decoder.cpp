#include "tjsCommHead.h"
#include "movie_decoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

// How far ahead the worker decodes. Frames are the expensive part: a 1280x720 frame is 3.7 MB.
static const size_t maxFrames = 4;
static const double audioAhead = 0.5; // seconds written to the sink before the clock reaches them
static const size_t minVideoPackets = 16;
static const size_t maxVideoPacketBytes = 32 << 20;
static const size_t maxAudioPacketBytes = 8 << 20;
static const size_t maxLeadPackets = 2048; // read after a seek to find where it landed
static const int ioBufferSize = 64 << 10;

static std::string describe(int code)
{
    char text[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(code, text, sizeof text);
    return text;
}

// Sends a packet, or null to drain, and hands each frame that comes out to onFrame.
template<typename OnFrame>
static int decode(AVCodecContext *context, AVPacket *packet, AVFrame *frame, OnFrame onFrame)
{
    int sent;
    for (;;) {
        sent = avcodec_send_packet(context, packet);
        int received;
        while ((received = avcodec_receive_frame(context, frame)) >= 0) {
            onFrame(frame);
            av_frame_unref(frame);
        }
        if (sent != AVERROR(EAGAIN))
            break;
        if (received != AVERROR(EAGAIN))
            break; // the decoder neither takes input nor gives output
    }
    return sent == AVERROR_EOF ? 0 : sent;
}

// The engine's storage streams, which decrypt archive entries as they read, behind FFmpeg's I/O.
static int readStream(void *opaque, uint8_t *buffer, int size)
{
    try {
        tjs_uint read = ((tTJSBinaryStream *)opaque)->Read(buffer, size);
        return read ? (int)read : AVERROR_EOF;
    } catch (...) {
        return AVERROR(EIO);
    }
}

static int64_t seekStream(void *opaque, int64_t offset, int whence)
{
    auto *stream = (tTJSBinaryStream *)opaque;
    try {
        if (whence & AVSEEK_SIZE)
            return (int64_t)stream->GetSize();
        // SEEK_SET, SEEK_CUR, and SEEK_END have the values of TJS_BS_SEEK_*.
        return (int64_t)stream->Seek(offset, whence & ~AVSEEK_FORCE);
    } catch (...) {
        return AVERROR(EIO);
    }
}

std::unique_ptr<MovieDecoder> MovieDecoder::Open(tTJSBinaryStream *stream, std::string &error)
{
    // Warnings such as the MP4 demuxer's note that custom I/O has no protocol name are noise.
    av_log_set_level(AV_LOG_ERROR);
    std::unique_ptr<MovieDecoder> decoder(new MovieDecoder);
    decoder->stream = stream;
    if (!decoder->OpenStreams(error))
        return nullptr;
    return decoder;
}

static AVCodecContext *openCodec(AVStream *stream, bool hardware, std::string &error)
{
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        error = std::string("no decoder for ") + avcodec_get_name(stream->codecpar->codec_id);
        return nullptr;
    }
    AVCodecContext *context = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(context, stream->codecpar);
    context->pkt_timebase = stream->time_base;
    context->thread_count = 0;
    if (hardware) {
        // The default get_format picks VideoToolbox frames once a device is attached, and falls
        // back to software for streams VideoToolbox rejects.
        AVBufferRef *device = nullptr;
        if (av_hwdevice_ctx_create(&device, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0) >= 0)
            context->hw_device_ctx = device;
    }
    int result = avcodec_open2(context, codec, nullptr);
    if (result < 0) {
        error = std::string("cannot open the ") + codec->name + " decoder: " + describe(result);
        avcodec_free_context(&context);
    }
    return context;
}

bool MovieDecoder::OpenStreams(std::string &error)
{
    auto *buffer = (uint8_t *)av_malloc(ioBufferSize);
    io = avio_alloc_context(buffer, ioBufferSize, 0, stream, readStream, nullptr, seekStream);
    format = avformat_alloc_context();
    format->pb = io;
    format->flags |= AVFMT_FLAG_CUSTOM_IO;
    int result = avformat_open_input(&format, nullptr, nullptr, nullptr);
    if (result < 0) {
        error = "unrecognized container: " + describe(result);
        return false; // avformat_open_input freed format
    }
    result = avformat_find_stream_info(format, nullptr);
    if (result < 0) {
        error = "cannot read the streams: " + describe(result);
        return false;
    }
    videoIndex = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoIndex < 0) {
        error = "no video stream";
        return false;
    }
    AVStream *videoStream = format->streams[videoIndex];
    AVCodecID videoCodec = videoStream->codecpar->codec_id;
    video = openCodec(videoStream, videoCodec == AV_CODEC_ID_H264 || videoCodec == AV_CODEC_ID_HEVC, error);
    if (!video)
        return false;
    info.hardware = video->hw_device_ctx != nullptr;
    info.width = video->width;
    info.height = video->height;
    if (info.width <= 0 || info.height <= 0) {
        error = "the video has no size";
        return false;
    }
    AVRational rate = av_guess_frame_rate(format, videoStream, nullptr);
    info.fps = rate.num > 0 && rate.den > 0 ? av_q2d(rate) : 30;
    origin = format->start_time != AV_NOPTS_VALUE ? format->start_time : 0;
    if (format->duration > 0)
        info.duration = format->duration / (double)AV_TIME_BASE;
    else if (videoStream->duration > 0)
        info.duration = videoStream->duration * av_q2d(videoStream->time_base);
    info.format = std::string(format->iformat->name) + ", " + avcodec_get_name(videoCodec) + " " +
                  std::to_string(info.width) + "x" + std::to_string(info.height);

    for (unsigned i = 0; i < format->nb_streams; i++)
        if (format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            info.audioStreams++;
    audioIndex = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, videoIndex, nullptr, 0);
    if (audioIndex >= 0) {
        std::string audioError;
        audio = openCodec(format->streams[audioIndex], false, audioError);
        AVChannelLayout layout = AV_CHANNEL_LAYOUT_STEREO;
        if (audio && audio->ch_layout.nb_channels == 1)
            layout = AV_CHANNEL_LAYOUT_MONO;
        if (audio && swr_alloc_set_opts2(&resampler, &layout, AV_SAMPLE_FMT_S16, audio->sample_rate,
                                         &audio->ch_layout, audio->sample_fmt, audio->sample_rate, 0,
                                         nullptr) >= 0 &&
            swr_init(resampler) >= 0) {
            info.audioRate = audio->sample_rate;
            info.audioChannels = layout.nb_channels;
            info.format += std::string(", ") + avcodec_get_name(audio->codec_id) + " " +
                           std::to_string(info.audioRate) + " Hz";
        } else {
            // The movie plays silent rather than not at all.
            info.format += ", audio not decodable" + (audioError.empty() ? "" : ": " + audioError);
            avcodec_free_context(&audio);
            swr_free(&resampler);
            audioIndex = -1;
        }
    }
    decoded = av_frame_alloc();
    transferred = av_frame_alloc();
    if (!videoStream->codecpar->extradata_size)
        Prime();
    return true;
}

// Video whose stream carries no extradata, such as MPEG-1 in a program stream, may have its sequence
// header only at the start of the file, and a decoder that has not read it decodes nothing after a
// seek. This decodes up to the first picture, then has the worker seek back to the start, which
// checks where the container landed.
void MovieDecoder::Prime()
{
    AVPacket *packet = av_packet_alloc();
    bool pictured = false;
    for (size_t i = 0; i < maxLeadPackets && !pictured && av_read_frame(format, packet) >= 0; i++) {
        if (packet->stream_index == videoIndex)
            decode(video, packet, decoded, [&](AVFrame *) { pictured = true; });
        av_packet_unref(packet);
    }
    av_packet_free(&packet);
    avcodec_flush_buffers(video);
    seekPending = true;
    seekTarget = 0;
}

MovieDecoder::~MovieDecoder()
{
    Stop();
    FreePackets();
    av_frame_free(&decoded);
    av_frame_free(&transferred);
    sws_freeContext(scaler);
    swr_free(&resampler);
    avcodec_free_context(&video);
    avcodec_free_context(&audio);
    avformat_close_input(&format);
    if (io) {
        av_freep(&io->buffer);
        avio_context_free(&io);
    }
    delete stream;
}

void MovieDecoder::Start(MovieAudioSink *audioSink)
{
    sink = audioSink;
    if (!sink || audioIndex < 0)
        audioEnded = true;
    worker = std::thread([this] { Run(); });
}

void MovieDecoder::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        quit = true;
    }
    wake.notify_all();
    if (worker.joinable())
        worker.join();
}

void MovieDecoder::RequestSeek(double t)
{
    seekPending = true;
    seekTarget = t;
    generation++;
    while (!frames.empty()) {
        spareBuffers.push_back(std::move(frames.front().pixels));
        frames.pop_front();
    }
    videoEnded = false;
    audioEnded = !sink || audioIndex < 0;
    wake.notify_all();
}

bool MovieDecoder::PopFrame(MovieFrame &frame)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (frames.empty())
        return false;
    frame = std::move(frames.front());
    frames.pop_front();
    wake.notify_all();
    return true;
}

double MovieDecoder::PeekFramePts()
{
    std::lock_guard<std::mutex> lock(mutex);
    return frames.empty() ? -1 : frames.front().pts;
}

void MovieDecoder::Recycle(MovieFrame &&frame)
{
    std::lock_guard<std::mutex> lock(mutex);
    spareBuffers.push_back(std::move(frame.pixels));
}

bool MovieDecoder::VideoEnded()
{
    std::lock_guard<std::mutex> lock(mutex);
    return videoEnded && frames.empty();
}

bool MovieDecoder::AudioEnded()
{
    std::lock_guard<std::mutex> lock(mutex);
    return audioEnded;
}

MovieStats MovieDecoder::Stats()
{
    std::lock_guard<std::mutex> lock(mutex);
    return stats;
}

std::string MovieDecoder::TakeError()
{
    std::lock_guard<std::mutex> lock(mutex);
    return std::move(error);
}

void MovieDecoder::Fail(const std::string &message)
{
    if (error.empty())
        error = message;
}

double MovieDecoder::Seconds(int64_t pts, int streamIndex) const
{
    AVStream *s = format->streams[streamIndex];
    return (av_rescale_q(pts, s->time_base, AV_TIME_BASE_Q) - origin) / (double)AV_TIME_BASE;
}

void MovieDecoder::Run()
{
    std::unique_lock<std::mutex> lock(mutex);
    while (!quit) {
        if (seekPending) {
            DoSeek(lock);
            continue;
        }
        if (frames.size() < maxFrames && !videoPackets.empty()) {
            AVPacket *packet = videoPackets.front();
            videoPackets.pop_front();
            videoPacketBytes -= packet->size;
            DecodeVideo(packet, lock);
            av_packet_free(&packet);
            continue;
        }
        if (demuxEnded && videoPackets.empty() && !videoEnded && frames.size() < maxFrames) {
            DecodeVideo(nullptr, lock);
            continue;
        }
        bool audioWanted = AudioWanted();
        if (audioWanted && !audioPackets.empty()) {
            AVPacket *packet = audioPackets.front();
            audioPackets.pop_front();
            audioPacketBytes -= packet->size;
            DecodeAudio(packet, lock);
            av_packet_free(&packet);
            continue;
        }
        if (demuxEnded && audioPackets.empty() && !audioEnded) {
            DecodeAudio(nullptr, lock);
            continue;
        }
        bool wantVideo = !demuxEnded && videoPackets.size() < minVideoPackets;
        if (((audioWanted && !demuxEnded) || wantVideo) && videoPacketBytes < maxVideoPacketBytes &&
            audioPacketBytes < maxAudioPacketBytes) {
            uint64_t readGeneration = generation;
            AVPacket *packet = av_packet_alloc();
            lock.unlock();
            int result = av_read_frame(format, packet);
            lock.lock();
            if (readGeneration != generation) {
                av_packet_free(&packet);
                continue;
            }
            if (result < 0) {
                av_packet_free(&packet);
                if (result != AVERROR_EOF)
                    Fail("read: " + describe(result));
                demuxEnded = true;
                continue;
            }
            Route(packet);
            continue;
        }
        wake.wait_for(lock, 10ms);
    }
}

void MovieDecoder::Route(AVPacket *packet)
{
    if (packet->stream_index == videoIndex) {
        videoPacketBytes += packet->size;
        videoPackets.push_back(packet);
    } else if (packet->stream_index == audioIndex && sink) {
        audioPacketBytes += packet->size;
        audioPackets.push_back(packet);
    } else {
        av_packet_free(&packet);
    }
}

bool MovieDecoder::AudioWanted()
{
    return audioIndex >= 0 && sink && sink->Queued() < (size_t)(audioAhead * info.audioRate);
}

void MovieDecoder::FreePackets()
{
    for (std::deque<AVPacket *> *queue : {&videoPackets, &audioPackets}) {
        for (AVPacket *packet : *queue)
            av_packet_free(&packet);
        queue->clear();
    }
    videoPacketBytes = audioPacketBytes = 0;
}

int MovieDecoder::SeekContainer(double t, bool fromFirstByte)
{
    if (fromFirstByte) {
        int64_t result = avio_seek(format->pb, 0, SEEK_SET);
        avformat_flush(format);
        return result < 0 ? (int)result : 0;
    }
    int64_t target = origin + (int64_t)(std::max(t, 0.0) * AV_TIME_BASE);
    return avformat_seek_file(format, -1, INT64_MIN, target, target, 0);
}

void MovieDecoder::DoSeek(std::unique_lock<std::mutex> &lock)
{
    double t = seekTarget;
    uint64_t seekGeneration = generation;
    seekPending = false;
    FreePackets();
    lock.unlock();

    // Containers without an index, such as MPEG program streams, seek by searching timestamps and
    // can land past the target. Read up to the first video keyframe to see where a seek landed, and
    // start earlier until it lands at or before the target, or read from the first byte, which
    // packet-based containers allow. The packets read are kept and played.
    std::vector<AVPacket *> lead;
    auto dropLead = [&] {
        for (AVPacket *packet : lead)
            av_packet_free(&packet);
        lead.clear();
    };
    int result;
    double backoff = 0;
    bool fromFirstByte = false;
    for (;;) {
        dropLead();
        result = SeekContainer(t - backoff, fromFirstByte);
        if (result < 0 && !fromFirstByte && t - backoff <= 0) {
            fromFirstByte = true;
            continue;
        }
        if (result < 0)
            break;
        double keyframe = -1;
        while (keyframe < 0 && lead.size() < maxLeadPackets) {
            AVPacket *packet = av_packet_alloc();
            if (av_read_frame(format, packet) < 0) {
                av_packet_free(&packet);
                break;
            }
            lead.push_back(packet);
            int64_t ts = packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
            if (packet->stream_index == videoIndex && (packet->flags & AV_PKT_FLAG_KEY) && ts != AV_NOPTS_VALUE)
                keyframe = Seconds(ts, videoIndex);
        }
        if (keyframe <= t + 0.5 / info.fps || fromFirstByte)
            break;
        if (t - backoff <= 0)
            fromFirstByte = true;
        else
            backoff = backoff ? backoff * 2 : 1;
    }
    avcodec_flush_buffers(video);
    if (audio)
        avcodec_flush_buffers(audio);

    lock.lock();
    if (result < 0)
        Fail("seek to " + std::to_string(t) + " s: " + describe(result));
    if (seekGeneration != generation) {
        dropLead();
        return;
    }
    dropBefore = t;
    demuxEnded = videoEnded = false;
    audioEnded = !sink || audioIndex < 0;
    for (AVPacket *packet : lead)
        Route(packet);
}

void MovieDecoder::DecodeVideo(AVPacket *packet, std::unique_lock<std::mutex> &lock)
{
    uint64_t decodeGeneration = generation;
    double drop = dropBefore - 0.5 / info.fps;
    std::vector<std::vector<uint8_t>> buffers = std::move(spareBuffers);
    spareBuffers.clear();
    std::vector<MovieFrame> out;
    int64_t hardwareFrames = 0;
    lock.unlock();

    int result = decode(video, packet, decoded, [&](AVFrame *frame) {
        MovieFrame movieFrame;
        movieFrame.pts = frame->best_effort_timestamp != AV_NOPTS_VALUE
                             ? Seconds(frame->best_effort_timestamp, videoIndex)
                             : (out.empty() ? drop : out.back().pts + 1 / info.fps);
        if (movieFrame.pts < drop)
            return;
        AVFrame *source = frame;
        if (frame->format == AV_PIX_FMT_VIDEOTOOLBOX) {
            av_frame_unref(transferred);
            if (av_hwframe_transfer_data(transferred, frame, 0) < 0)
                return;
            source = transferred;
            hardwareFrames++;
        }
        scaler = sws_getCachedContext(scaler, source->width, source->height, (AVPixelFormat)source->format,
                                      info.width, info.height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr,
                                      nullptr, nullptr);
        if (!scaler)
            return;
        if (source->colorspace != AVCOL_SPC_UNSPECIFIED || source->color_range == AVCOL_RANGE_JPEG) {
            int colorspace = source->colorspace != AVCOL_SPC_UNSPECIFIED ? source->colorspace : SWS_CS_DEFAULT;
            sws_setColorspaceDetails(scaler, sws_getCoefficients(colorspace),
                                     source->color_range == AVCOL_RANGE_JPEG,
                                     sws_getCoefficients(SWS_CS_DEFAULT), 1, 0, 1 << 16, 1 << 16);
        }
        if (!buffers.empty()) {
            movieFrame.pixels = std::move(buffers.back());
            buffers.pop_back();
        }
        movieFrame.pixels.resize((size_t)info.width * info.height * 4);
        uint8_t *planes[4] = {movieFrame.pixels.data()};
        int strides[4] = {info.width * 4};
        sws_scale(scaler, source->data, source->linesize, 0, source->height, planes, strides);
        out.push_back(std::move(movieFrame));
    });

    lock.lock();
    for (auto &buffer : buffers)
        spareBuffers.push_back(std::move(buffer));
    if (result < 0)
        Fail("video: " + describe(result));
    if (decodeGeneration != generation) {
        for (MovieFrame &frame : out)
            spareBuffers.push_back(std::move(frame.pixels));
        return;
    }
    stats.hardwareFrames += hardwareFrames;
    for (MovieFrame &frame : out) {
        stats.videoFrames++;
        frames.push_back(std::move(frame));
    }
    if (!packet)
        videoEnded = true;
}

void MovieDecoder::DecodeAudio(AVPacket *packet, std::unique_lock<std::mutex> &lock)
{
    // Audio decodes in well under a millisecond per packet, so it stays under the lock, which
    // keeps a seek from slipping between decoding and writing.
    int result = decode(audio, packet, decoded, [&](AVFrame *frame) { WriteAudio(frame, lock); });
    // A seek in a program stream can start inside an audio frame, which the decoder drops.
    if (result < 0 && result != AVERROR_INVALIDDATA)
        Fail("audio: " + describe(result));
    if (!packet) {
        audioEnded = true;
        sink->End();
    }
}

void MovieDecoder::WriteAudio(AVFrame *frame, std::unique_lock<std::mutex> &lock)
{
    int channels = info.audioChannels;
    pcm.resize((size_t)frame->nb_samples * channels);
    uint8_t *out = (uint8_t *)pcm.data();
    int count = swr_convert(resampler, &out, frame->nb_samples, (const uint8_t **)frame->extended_data,
                            frame->nb_samples);
    if (count <= 0)
        return;
    double pts = frame->best_effort_timestamp != AV_NOPTS_VALUE
                     ? Seconds(frame->best_effort_timestamp, audioIndex)
                     : dropBefore;
    int skip = 0;
    if (pts < dropBefore)
        skip = std::min(count, (int)std::lround((dropBefore - pts) * info.audioRate));
    if (skip == count)
        return;
    sink->Write(pcm.data() + (size_t)skip * channels, count - skip, pts + skip / (double)info.audioRate);
    stats.audioFrames += count - skip;
}

#include "movie.h"

#include <algorithm>
#include <cmath>
#include <cstring>

// pl_mpeg's implementation, compiled once for the runtime. Its stdio loaders are unused: the demuxer below reads
// the file and hands pl_mpeg the elementary streams.
#define PLM_NO_STDIO
#define PL_MPEG_IMPLEMENTATION
#include "plm_names.h"
#include "../third_party/pl_mpeg/pl_mpeg.h"

namespace s4 {

namespace {

constexpr size_t kWindow = 256 * 1024;      // read window; a PES packet is at most 65541 bytes
constexpr long kProbeBytes = 2 * 1024 * 1024;  // how far open() looks for the first video and audio packets
constexpr size_t kMaxQueued = 4 * 1024 * 1024;  // cap on demuxed data waiting for the other decoder
constexpr double kAudioLead = 0.5;           // seconds of sound kept decoded ahead of what has played
constexpr double kAudioSlack = 0.05;         // how far the picture may trail the sound before it catches up
constexpr int kVideoStream = 0xE0, kAudioStream = 0xC0;

// A 33-bit timestamp in the 5-byte PES/pack layout, in seconds.
double readPts(const uint8_t *b) {
    int64_t v = (int64_t((b[0] >> 1) & 7) << 30) | (int64_t(b[1]) << 22) | (int64_t(b[2] >> 1) << 15) |
                (int64_t(b[3]) << 7) | (b[4] >> 1);
    return double(v) / 90000.0;
}

// 4-point Hermite interpolation between x0 and x1 at t in [0, 1).
float hermite(float xm1, float x0, float x1, float x2, float t) {
    float c1 = 0.5f * (x1 - xm1);
    float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

}  // namespace

Movie::~Movie() { close(); }

void Movie::seekFile(long pos) {
    std::fseek(file_, pos, SEEK_SET);
    winOff_ = pos;
    inPos_ = inLen_ = 0;
    eof_ = false;
    needPack_ = true;
}

bool Movie::fill(size_t need) {
    if (inLen_ - inPos_ >= need) return true;
    if (inPos_ > 0) {
        std::memmove(in_.data(), in_.data() + inPos_, inLen_ - inPos_);
        winOff_ += long(inPos_);
        inLen_ -= inPos_;
        inPos_ = 0;
    }
    while (inLen_ < need) {
        size_t got = std::fread(in_.data() + inLen_, 1, in_.size() - inLen_, file_);
        if (got == 0) return false;
        inLen_ += got;
    }
    return true;
}

// Returns the next video or audio packet. Handles MPEG-1 (ISO 11172-1) and MPEG-2 (ISO 13818-1) pack headers and
// PES headers; skips system headers, padding and other streams. False at the end of the file.
bool Movie::nextPacket(Packet &p) {
    for (;;) {
        if (!fill(4)) return false;
        const uint8_t *b = in_.data() + inPos_;
        if (b[0] != 0 || b[1] != 0 || b[2] != 1) {
            ++inPos_;
            continue;
        }
        int code = b[3];
        if (code == 0xBA) {
            if (!fill(14)) return false;
            b = in_.data() + inPos_;
            size_t len;
            if ((b[4] & 0xC0) == 0x40) len = 14 + (b[13] & 7);  // MPEG-2: '01' marker, then stuffing length
            else if ((b[4] & 0xF0) == 0x20) len = 12;           // MPEG-1: '0010' marker
            else {
                inPos_ += 4;
                continue;
            }
            if (!fill(len)) return false;
            inPos_ += len;
            needPack_ = false;
            continue;
        }
        if (needPack_ || code < 0xBB) {  // not at a packet boundary: keep scanning
            inPos_ += code == 0xB9 ? 4 : 3;
            continue;
        }
        if (!fill(6)) return false;
        b = in_.data() + inPos_;
        size_t total = 6 + ((size_t(b[4]) << 8) | b[5]);
        if (!fill(total)) return false;
        b = in_.data() + inPos_;
        inPos_ += total;
        if (code != kVideoStream && code != kAudioStream) continue;

        const uint8_t *q = b + 6, *end = b + total;
        double pts = -1;
        if (q < end && (q[0] & 0xC0) == 0x80) {  // MPEG-2 PES header
            if (end - q < 3) continue;
            int flags = q[1], hlen = q[2];
            if ((flags & 0x80) && hlen >= 5 && end - q >= 8) pts = readPts(q + 3);
            q += 3 + hlen;
        } else {  // MPEG-1: stuffing, optional STD buffer size, then PTS, PTS+DTS or 0x0F
            while (q < end && *q == 0xFF) ++q;
            if (end - q >= 2 && (*q & 0xC0) == 0x40) q += 2;
            if (end - q >= 5 && (*q & 0xF0) == 0x20) {
                pts = readPts(q);
                q += 5;
            } else if (end - q >= 10 && (*q & 0xF0) == 0x30) {
                pts = readPts(q);
                q += 10;
            } else if (q < end && *q == 0x0F) {
                ++q;
            } else {
                continue;
            }
        }
        if (q > end) continue;
        p.stream = code;
        p.pts = pts;
        p.data = q;
        p.size = size_t(end - q);
        return true;
    }
}

// Finds the last video timestamp near the end of the file; the window doubles until one turns up.
void Movie::scanDuration(long fileSize) {
    double last = -1;
    for (long range = 256 * 1024; last < 0; range *= 2) {
        long from = std::max(0L, fileSize - range);
        seekFile(from);
        Packet p;
        while (nextPacket(p))
            if (p.stream == kVideoStream && p.pts >= 0) last = p.pts;
        if (from == 0 || range >= 16 * 1024 * 1024) break;
    }
    duration_ = last >= firstVideoPts_ ? last - firstVideoPts_ : 0;
}

// pl_mpeg calls this when a decoder needs more data. Demuxes until the wanted stream gets a packet, the other
// decoder's queue is full, or the file ends.
void Movie::pump(plm_buffer_t *wanted) {
    if (eof_) return;
    Packet p;
    while (nextPacket(p)) {
        plm_buffer_t *dst = p.stream == kVideoStream ? videoBuf_ : audioBuf_;
        if (!dst) continue;
        plm_buffer_write(dst, const_cast<uint8_t *>(p.data), p.size);
        if (dst == wanted || plm_buffer_get_remaining(dst) > kMaxQueued) return;
    }
    endStreams();
}

void Movie::endStreams() {
    eof_ = true;
    for (plm_buffer_t *b : {videoBuf_, audioBuf_}) {
        if (!b) continue;
        plm_buffer_signal_end(b);
        b->has_ended = TRUE;  // signal_end alone never marks an empty buffer as ended
    }
}

void Movie::onVideoLoad(plm_buffer_t *, void *user) {
    auto *m = static_cast<Movie *>(user);
    m->pump(m->videoBuf_);
}

void Movie::onAudioLoad(plm_buffer_t *, void *user) {
    auto *m = static_cast<Movie *>(user);
    m->pump(m->audioBuf_);
}

bool Movie::open(const std::string &path) {
    close();
    file_ = std::fopen(path.c_str(), "rb");
    if (!file_) return false;
    std::fseek(file_, 0, SEEK_END);
    long fileSize = std::ftell(file_);
    in_.resize(kWindow);

    // Probe the start for the streams and their first timestamps.
    seekFile(0);
    bool hasAudio = false;
    Packet p;
    while (winOff_ + long(inPos_) < kProbeBytes && nextPacket(p)) {
        if (p.stream == kVideoStream && firstVideoPts_ < 0) firstVideoPts_ = p.pts;
        if (p.stream == kAudioStream) {
            hasAudio = true;
            if (firstAudioPts_ < 0) firstAudioPts_ = p.pts;
        }
        if (firstVideoPts_ >= 0 && firstAudioPts_ >= 0) break;
    }
    if (firstVideoPts_ < 0) {
        close();
        return false;
    }
    scanDuration(fileSize);
    seekFile(0);

    videoBuf_ = plm_buffer_create_with_capacity(PLM_BUFFER_DEFAULT_SIZE);
    plm_buffer_set_load_callback(videoBuf_, onVideoLoad, this);
    video_ = plm_video_create_with_buffer(videoBuf_, TRUE);
    if (hasAudio) {
        audioBuf_ = plm_buffer_create_with_capacity(PLM_BUFFER_DEFAULT_SIZE);
        plm_buffer_set_load_callback(audioBuf_, onAudioLoad, this);
        audio_ = plm_audio_create_with_buffer(audioBuf_, TRUE);
    }
    if (!plm_video_has_header(video_)) {
        close();
        return false;
    }
    width_ = plm_video_get_width(video_);
    height_ = plm_video_get_height(video_);
    fps_ = plm_video_get_framerate(video_);
    if (fps_ > 0) duration_ += 1.0 / fps_;  // the last timestamp marks the start of the last frame

    if (audio_ && plm_audio_has_header(audio_)) {
        srcRate_ = plm_audio_get_samplerate(audio_);
    } else if (audio_) {
        plm_audio_destroy(audio_);
        audio_ = nullptr;
        audioBuf_ = nullptr;
    }
    if (audio_ && firstAudioPts_ >= 0) {
        audioStart_ = firstAudioPts_ - firstVideoPts_;
        audioSkew_ = std::llround(-audioStart_ * kAudioRate);
    }
    // Sound that starts after the picture is preceded by silence, counted rather than stored.
    int64_t leadSilence = std::max<int64_t>(0, -audioSkew_);
    audioSkew_ = std::max<int64_t>(0, audioSkew_);

    frame_.assign(size_t(width_) * height_ * 4, 0);
    for (size_t i = 3; i < frame_.size(); i += 4) frame_[i] = 255;
    rsIn_.assign(2, 0.0f);  // one frame of history before the first sample
    rsPos_ = 1;
    std::lock_guard<std::mutex> lock(mu_);
    ring_.assign(size_t(kAudioRate) * 2, 0.0f);  // one second
    ringHead_ = ringCount_ = 0;
    leadSilence_ = leadSilence;
    audioPushed_ = leadSilence;
    return true;
}

bool Movie::advanceTo(double seconds) {
    if (!video_) return false;
    auto buffered = [this] {
        std::lock_guard<std::mutex> lock(mu_);
        return ringCount_ + size_t(leadSilence_);
    };
    {
        // The game clock drives the picture, stepping evenly with the display. The sound played moves in audio
        // buffers, so it only takes over when it is well ahead, as after a hitch that the game clock dropped. Once the
        // sound has played out, the picture catches up to it fully, or its last frames would wait for the game clock.
        std::lock_guard<std::mutex> lock(mu_);
        double slack = audioEnded_ && ringCount_ == 0 ? 0 : kAudioSlack;
        now_ = std::max({now_, seconds, double(audioRead_) / kAudioRate - slack});
    }

    // plm_video_get_time is the presentation time of the next frame to decode. Only the newest frame is converted.
    plm_frame_t *shown = nullptr;
    while (!videoDone_ && plm_video_get_time(video_) <= now_) {
        plm_frame_t *f = plm_video_decode(video_);
        if (!f) {
            videoDone_ = eof_;
            break;
        }
        shown = f;
    }
    // Sound runs ahead of the game clock (as without a host pulling it) and of what has played. The decoder's clock
    // starts at the sound's own start, audioStart_ into the movie.
    while (audio_ && !audioDone_ &&
           (plm_audio_get_time(audio_) + audioStart_ < now_ + kAudioLead || buffered() < size_t(kAudioLead * kAudioRate))) {
        plm_samples_t *s = plm_audio_decode(audio_);
        if (!s) {
            audioDone_ = eof_;
            if (audioDone_) {
                std::lock_guard<std::mutex> lock(mu_);
                audioEnded_ = true;
            }
            break;
        }
        pushAudio(s->interleaved, int(s->count));
    }
    if (shown) plm_frame_to_rgba(shown, frame_.data(), width_ * 4);
    return shown != nullptr;
}

void Movie::pushAudio(const float *in, int frames) {
    const float *src = in;
    size_t n = size_t(frames);
    if (srcRate_ != kAudioRate) {
        rsIn_.insert(rsIn_.end(), in, in + size_t(frames) * 2);
        size_t avail = rsIn_.size() / 2;
        double step = double(srcRate_) / kAudioRate;
        rsOut_.clear();
        for (size_t i; (i = size_t(rsPos_)) + 2 < avail; rsPos_ += step) {
            float t = float(rsPos_ - double(i));
            const float *x = rsIn_.data() + (i - 1) * 2;
            for (int ch = 0; ch < 2; ++ch) rsOut_.push_back(hermite(x[ch], x[2 + ch], x[4 + ch], x[6 + ch], t));
        }
        size_t drop = size_t(rsPos_) - 1;  // keep one frame of history before the next read position
        rsIn_.erase(rsIn_.begin(), rsIn_.begin() + ptrdiff_t(drop * 2));
        rsPos_ -= double(drop);
        src = rsOut_.data();
        n = rsOut_.size() / 2;
    }
    if (audioSkew_ > 0) {  // audio starts before the video: drop the lead-in
        size_t d = std::min(n, size_t(audioSkew_));
        src += d * 2;
        n -= d;
        audioSkew_ -= int64_t(d);
    }
    std::lock_guard<std::mutex> lock(mu_);
    writeRing(src, n);
    audioPushed_ += int64_t(n);
}

// Appends to the ring, dropping the oldest audio when the reader has fallen more than the ring's length behind.
void Movie::writeRing(const float *in, size_t frames) {
    size_t cap = ring_.size() / 2;
    if (frames > cap) {
        in += (frames - cap) * 2;
        frames = cap;
    }
    if (ringCount_ + frames > cap) {
        size_t over = ringCount_ + frames - cap;
        ringHead_ = (ringHead_ + over) % cap;
        ringCount_ -= over;
    }
    size_t tail = (ringHead_ + ringCount_) % cap;
    size_t first = std::min(frames, cap - tail);
    std::memcpy(ring_.data() + tail * 2, in, first * 2 * sizeof(float));
    std::memcpy(ring_.data(), in + first * 2, (frames - first) * 2 * sizeof(float));
    ringCount_ += frames;
}

int Movie::readAudio(float *out, int frames) {
    std::lock_guard<std::mutex> lock(mu_);
    // The silence before sound that starts after the picture comes first.
    int quiet = int(std::min<int64_t>(leadSilence_, std::max(frames, 0)));
    std::memset(out, 0, size_t(quiet) * 2 * sizeof(float));
    leadSilence_ -= quiet;
    audioRead_ += quiet;
    out += size_t(quiet) * 2;
    frames -= quiet;
    size_t cap = ring_.size() / 2;
    size_t n = std::min(size_t(std::max(frames, 0)), ringCount_);
    size_t first = std::min(n, cap - ringHead_);
    std::memcpy(out, ring_.data() + ringHead_ * 2, first * 2 * sizeof(float));
    std::memcpy(out + first * 2, ring_.data(), (n - first) * 2 * sizeof(float));
    if (cap) ringHead_ = (ringHead_ + n) % cap;
    ringCount_ -= n;
    audioRead_ += int64_t(n);
    if (int(n) < frames && audioPushed_ > 0 && !audioEnded_) {
        dry_.reads++;
        dry_.frames += frames - int64_t(n);
    }
    return quiet + int(n);
}

Movie::Dry Movie::dry() {
    std::lock_guard<std::mutex> lock(mu_);
    return dry_;
}

bool Movie::ended() {
    if (!video_) return true;
    if (!videoDone_) return false;
    if (!audio_) return true;
    if (!audioDone_) return false;
    std::lock_guard<std::mutex> lock(mu_);
    return ringCount_ == 0 || now_ >= double(audioPushed_) / kAudioRate;
}

void Movie::close() {
    if (video_) plm_video_destroy(video_);  // destroys its buffer too
    if (audio_) plm_audio_destroy(audio_);
    video_ = nullptr;
    audio_ = nullptr;
    videoBuf_ = audioBuf_ = nullptr;
    if (file_) std::fclose(file_);
    file_ = nullptr;
    in_ = {};
    inPos_ = inLen_ = 0;
    winOff_ = 0;
    eof_ = needPack_ = videoDone_ = audioDone_ = false;
    width_ = height_ = srcRate_ = 0;
    fps_ = duration_ = now_ = 0;
    firstVideoPts_ = firstAudioPts_ = -1;
    frame_ = {};
    rsIn_ = {};
    rsOut_ = {};
    rsPos_ = 1;
    audioSkew_ = audioPushed_ = 0;
    std::lock_guard<std::mutex> lock(mu_);
    ring_ = {};
    ringHead_ = ringCount_ = 0;
    audioEnded_ = false;
    audioRead_ = 0;
    leadSilence_ = 0;
    audioStart_ = 0;
    dry_ = {};
}

}  // namespace s4

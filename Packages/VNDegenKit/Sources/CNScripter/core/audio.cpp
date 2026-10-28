#include "audio.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstring>

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "../third_party/stb_vorbis.c"
// stb_vorbis leaks single-letter macros that clash with later code.
#undef L
#undef C
#undef R

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "../third_party/dr_mp3.h"

namespace vnns {

namespace {

class OggSource : public AudioSource {
public:
    explicit OggSource(Bytes data) : data_(std::move(data)) {
        int err = 0;
        v_ = stb_vorbis_open_memory(data_.data(), int(data_.size()), &err, nullptr);
        if (v_) {
            stb_vorbis_info info = stb_vorbis_get_info(v_);
            rate_ = int(info.sample_rate);
            channels_ = info.channels;
        }
    }
    ~OggSource() override {
        if (v_) stb_vorbis_close(v_);
    }
    bool ok() const { return v_ != nullptr; }
    int read(float *out, int frames) override {
        int n = stb_vorbis_get_samples_float_interleaved(v_, 2, out, frames * 2);
        if (channels_ == 1)  // stb_vorbis leaves the second channel silent for mono files
            for (int i = 0; i < n; i++) out[i * 2 + 1] = out[i * 2];
        return n;
    }
    bool rewind(double seconds) override {
        return stb_vorbis_seek(v_, unsigned(std::max(0.0, seconds) * rate_)) != 0;
    }
    int rate() const override { return rate_; }

private:
    Bytes data_;
    stb_vorbis *v_ = nullptr;
    int rate_ = 44100, channels_ = 2;
};

// PCM WAV: 8-bit unsigned, 16-bit, 24-bit or 32-bit signed, or 32-bit float, mono or stereo.
class WavSource : public AudioSource {
public:
    explicit WavSource(Bytes data) : data_(std::move(data)) {
        if (data_.size() < 12 || memcmp(data_.data(), "RIFF", 4) || memcmp(data_.data() + 8, "WAVE", 4)) return;
        size_t p = 12;
        int format = 0;
        while (p + 8 <= data_.size()) {
            size_t size = le32(&data_[p + 4]), left = data_.size() - (p + 8);
            const uint8_t *body = &data_[p + 8];
            if (!memcmp(&data_[p], "fmt ", 4) && size >= 16 && size <= left) {
                format = le16(body);
                channels_ = le16(body + 2);
                rate_ = int(le32(body + 4));
                bits_ = le16(body + 14);
                if (format == 0xFFFE && size >= 26) format = le16(body + 24);
            } else if (!memcmp(&data_[p], "data", 4)) {
                begin_ = p + 8;
                end_ = begin_ + std::min(size, left);  // a data chunk cut short plays what there is
                break;
            }
            if (size + (size & 1) >= left) break;
            p += 8 + size + (size & 1);
        }
        float_ = format == 3;
        ok_ = begin_ && (format == 1 || format == 3) && channels_ >= 1 && rate_ > 0 &&
              (bits_ == 8 || bits_ == 16 || bits_ == 24 || bits_ == 32);
        pos_ = begin_;
    }
    bool ok() const { return ok_; }
    int read(float *out, int frames) override {
        int bytes = bits_ / 8, frameBytes = bytes * channels_;
        int n = 0;
        for (; n < frames && pos_ + size_t(frameBytes) <= end_; n++, pos_ += size_t(frameBytes)) {
            float s[2];
            for (int c = 0; c < 2; c++) {
                const uint8_t *q = &data_[pos_ + size_t(std::min(c, channels_ - 1) * bytes)];
                switch (bits_) {
                case 8: s[c] = (int(q[0]) - 128) / 128.0f; break;
                case 16: s[c] = int16_t(le16(q)) / 32768.0f; break;
                case 24: s[c] = float(int32_t(uint32_t(q[0]) << 8 | uint32_t(q[1]) << 16 | uint32_t(q[2]) << 24) / 2147483648.0); break;
                default: {
                    uint32_t v = le32(q);
                    if (float_) {
                        float f;
                        memcpy(&f, &v, 4);
                        s[c] = f;
                    } else {
                        s[c] = float(int32_t(v) / 2147483648.0);
                    }
                }
                }
            }
            out[n * 2] = s[0];
            out[n * 2 + 1] = s[1];
        }
        return n;
    }
    bool rewind(double seconds) override {
        size_t frameBytes = size_t(bits_ / 8 * channels_);
        pos_ = std::min(end_, begin_ + size_t(std::max(0.0, seconds) * rate_) * frameBytes);
        return true;
    }
    int rate() const override { return rate_; }

private:
    Bytes data_;
    size_t begin_ = 0, end_ = 0, pos_ = 0;
    int channels_ = 0, rate_ = 0, bits_ = 0;
    bool float_ = false, ok_ = false;
};

// MP3, which the mp3 and bgm commands play. Mono files play on both channels.
class Mp3Source : public AudioSource {
public:
    explicit Mp3Source(Bytes data) : data_(std::move(data)) {
        ok_ = drmp3_init_memory(&mp3_, data_.data(), data_.size(), nullptr) && mp3_.channels >= 1;
    }
    ~Mp3Source() override {
        if (ok_) drmp3_uninit(&mp3_);
    }
    bool ok() const { return ok_; }
    int read(float *out, int frames) override {
        int ch = int(mp3_.channels);
        if (ch == 2) return int(drmp3_read_pcm_frames_f32(&mp3_, drmp3_uint64(frames), out));
        tmp_.resize(size_t(frames * ch));
        int n = int(drmp3_read_pcm_frames_f32(&mp3_, drmp3_uint64(frames), tmp_.data()));
        for (int i = 0; i < n; i++) {
            out[i * 2] = tmp_[size_t(i * ch)];
            out[i * 2 + 1] = tmp_[size_t(i * ch + (ch > 1 ? 1 : 0))];
        }
        return n;
    }
    bool rewind(double seconds) override {
        return drmp3_seek_to_pcm_frame(&mp3_, drmp3_uint64(std::max(0.0, seconds) * mp3_.sampleRate)) != 0;
    }
    int rate() const override { return int(mp3_.sampleRate); }

private:
    Bytes data_;
    drmp3 mp3_{};
    std::vector<float> tmp_;
    bool ok_ = false;
};

}  // namespace

std::unique_ptr<AudioSource> openAudio(Bytes data) {
    if (data.size() >= 4 && !memcmp(data.data(), "OggS", 4)) {
        auto s = std::make_unique<OggSource>(std::move(data));
        if (s->ok()) return s;
        return nullptr;
    }
    if (data.size() >= 4 && !memcmp(data.data(), "RIFF", 4)) {
        auto s = std::make_unique<WavSource>(std::move(data));
        if (s->ok()) return s;
        return nullptr;
    }
    auto s = std::make_unique<Mp3Source>(std::move(data));
    if (s->ok()) return s;
    return nullptr;
}

AudioMixer::AudioMixer() : decoder_([this] { decodeLoop(); }) {}

AudioMixer::~AudioMixer() {
    {
        std::lock_guard<std::mutex> lock(decodeMutex_);
        quit_ = true;
    }
    decodeWake_.notify_one();
    decoder_.join();
}

bool AudioMixer::play(int channel, Bytes data, bool loop, int volume, int fadeInMs, double loopStart) {
    auto src = openAudio(std::move(data));
    std::shared_ptr<Stream> stream;
    if (src) {
        stream = std::make_shared<Stream>();
        stream->rate = std::max(1, src->rate());
        stream->src = std::move(src);
        stream->loop = loop;
        stream->loopStart = loopStart;
        stream->capacity = size_t(std::max(stream->rate * 3 / 4, 4096));  // 0.75 s ahead
        stream->ring.resize(stream->capacity * 2);
    }
    Channel old;  // freed after the lock is let go, so the audio thread doesn't wait on it
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Channel &c = ch_[size_t(channel)];
        release(c);
        old = std::move(c);
        c = Channel();
        if (!stream) return false;
        c.stream = stream;
        c.volume = std::clamp(volume, 0, 100) / 100.0f;
        if (fadeInMs > 0) {
            c.gain = 0;
            c.fadeStep = 1.0f / (fadeInMs * kRate / 1000.0f);
        }
    }
    {
        std::lock_guard<std::mutex> lock(decodeMutex_);
        streams_.push_back(std::move(stream));
    }
    decodeWake_.notify_one();
    return true;
}

void AudioMixer::release(Channel &c) {
    if (c.stream) c.stream->dropped.store(true, std::memory_order_relaxed);
}

void AudioMixer::stop(int channel, int fadeOutMs) {
    Channel old;  // freed after the lock is let go
    std::lock_guard<std::mutex> lock(mutex_);
    Channel &c = ch_[size_t(channel)];
    if (!c.stream) return;
    if (fadeOutMs > 0 && !paused_ && !c.ended) {
        c.fadeStep = -c.gain / (fadeOutMs * kRate / 1000.0f);
        c.stopAfterFade = true;
    } else {
        release(c);
        old = std::move(c);
        c = Channel();
    }
}

bool AudioMixer::playing(int channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    Channel &c = ch_[size_t(channel)];
    return c.stream && !c.ended;
}

void AudioMixer::setVolume(int channel, int volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    ch_[size_t(channel)].volume = std::clamp(volume, 0, 100) / 100.0f;
}

void AudioMixer::setPaused(bool paused) {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = paused;
}

// ---- the decoder thread ----

void AudioMixer::fill(Stream &s) {
    constexpr size_t kChunk = 1024;
    float tmp[kChunk * 2];
    size_t w = s.written.load(std::memory_order_relaxed);
    while (!s.eof.load(std::memory_order_relaxed) && !s.dropped.load(std::memory_order_relaxed) &&
           s.capacity - (w - s.read.load(std::memory_order_acquire)) >= kChunk) {
        int n = s.src->read(tmp, int(kChunk));
        if (n <= 0 && s.loop && s.src->rewind(s.loopStart)) n = s.src->read(tmp, int(kChunk));
        if (n <= 0) {
            s.eof.store(true, std::memory_order_release);
            break;
        }
        for (int i = 0; i < n; i++, w++) {
            size_t k = (w % s.capacity) * 2;
            s.ring[k] = tmp[i * 2];
            s.ring[k + 1] = tmp[i * 2 + 1];
        }
        s.written.store(w, std::memory_order_release);
    }
}

void AudioMixer::decodeLoop() {
    std::vector<std::shared_ptr<Stream>> work;
    std::unique_lock<std::mutex> lock(decodeMutex_);
    while (!quit_) {
        streams_.erase(std::remove_if(streams_.begin(), streams_.end(),
                                      [](const auto &s) { return s->dropped.load() || s->eof.load(); }),
                       streams_.end());
        work = streams_;
        lock.unlock();
        for (auto &s : work) fill(*s);
        work.clear();  // a stream nobody plays any more is freed here, off the audio thread
        lock.lock();
        // Every 10 ms is often enough to keep 0.75 s rings full; play() wakes the thread at once.
        decodeWake_.wait_for(lock, std::chrono::milliseconds(10));
    }
}

void AudioMixer::queueMovieSamples(const float *in, int frames, int volume, int rate) {
    if (frames <= 0) return;
    std::vector<float> out;
    if (rate == kRate || rate <= 0) {
        out.assign(in, in + frames * 2);
    } else {
        // Linear interpolation, carrying the position and last frame across blocks; frame -1 is the previous block's.
        double step = double(rate) / kRate;
        auto at = [&](int k, int c) { return k < 0 ? movieLast_[c] : in[k * 2 + c]; };
        double p = movieSrcPos_;
        for (; p < frames - 1; p += step) {
            int i = int(std::floor(p));
            float t = float(p - i);
            for (int c = 0; c < 2; c++) out.push_back(at(i, c) + (at(i + 1, c) - at(i, c)) * t);
        }
        movieSrcPos_ = p - frames;
        movieLast_[0] = in[(frames - 1) * 2];
        movieLast_[1] = in[(frames - 1) * 2 + 1];
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (movieRead_ > 0 && movieRead_ * 2 > movie_.size()) {
        movie_.erase(movie_.begin(), movie_.begin() + long(movieRead_));
        movieRead_ = 0;
    }
    movie_.insert(movie_.end(), out.begin(), out.end());
    movieVolume_ = std::clamp(volume, 0, 100) / 100.0f;
}

void AudioMixer::clearMovieSamples() {
    std::lock_guard<std::mutex> lock(mutex_);
    movie_.clear();
    movieRead_ = 0;
    movieSrcPos_ = 0;
    movieLast_[0] = movieLast_[1] = 0;
}

int AudioMixer::movieQueuedFrames() {
    std::lock_guard<std::mutex> lock(mutex_);
    return int((movie_.size() - movieRead_) / 2);
}

bool AudioMixer::popFrame(Stream &s, float *frame) {
    size_t r = s.read.load(std::memory_order_relaxed);
    if (r == s.written.load(std::memory_order_acquire)) return false;
    size_t k = (r % s.capacity) * 2;
    frame[0] = s.ring[k];
    frame[1] = s.ring[k + 1];
    s.read.store(r + 1, std::memory_order_release);
    return true;
}

// Next output frame of a channel, resampled linearly from the stream's rate. False when there is nothing to play
// yet, or any more (c.ended).
bool AudioMixer::pull(Channel &c, float &l, float &r) {
    Stream &s = *c.stream;
    // The next frame: 1, or 0 while the decoder is late, or -1 at the end of the sound.
    auto fetch = [&](float *f) {
        if (popFrame(s, f)) return 1;
        if (!s.eof.load(std::memory_order_acquire)) return 0;
        return popFrame(s, f) ? 1 : -1;  // written just before the decoder finished, or the end
    };
    if (!c.primed) {
        int got = fetch(c.cur);
        if (got < 0) c.ended = true;
        if (got <= 0) return false;
        int more = fetch(c.next);
        if (more != 1) {
            c.next[0] = c.cur[0];
            c.next[1] = c.cur[1];
            c.lastFrame = more < 0;
        }
        c.primed = true;
        c.pos = 0;
    }
    float t = float(c.pos);
    l = c.cur[0] + (c.next[0] - c.cur[0]) * t;
    r = c.cur[1] + (c.next[1] - c.cur[1]) * t;
    c.pos += double(s.rate) / kRate;
    while (c.pos >= 1.0) {
        c.pos -= 1.0;
        if (c.lastFrame) {
            c.ended = true;
            break;
        }
        c.cur[0] = c.next[0];
        c.cur[1] = c.next[1];
        int got = fetch(c.next);
        if (got != 1) {
            // The end: hold the last frame for its turn. A late decoder: hold it until it catches up, rather
            // than click.
            c.next[0] = c.cur[0];
            c.next[1] = c.cur[1];
            c.lastFrame = got < 0;
        }
    }
    return true;
}

void AudioMixer::render(float *out, int frames) {
    std::fill(out, out + frames * 2, 0.0f);
    std::lock_guard<std::mutex> lock(mutex_);
    if (paused_) return;
    for (Channel &c : ch_) {
        if (!c.stream || c.ended) continue;
        for (int f = 0; f < frames && !c.ended; f++) {
            float l, r;
            if (!pull(c, l, r)) break;
            if (c.fadeStep != 0) {
                c.gain += c.fadeStep;
                if (c.gain >= 1) {
                    c.gain = 1;
                    c.fadeStep = 0;
                } else if (c.gain <= 0) {
                    c.gain = 0;
                    c.fadeStep = 0;
                    if (c.stopAfterFade) {
                        c.ended = true;
                        release(c);
                        break;
                    }
                }
            }
            float g = c.volume * c.gain;
            out[f * 2] += l * g;
            out[f * 2 + 1] += r * g;
        }
    }
    size_t avail = (movie_.size() - movieRead_) / 2;
    size_t n = std::min(avail, size_t(frames));
    for (size_t f = 0; f < n; f++) {
        out[f * 2] += movie_[movieRead_ + f * 2] * movieVolume_;
        out[f * 2 + 1] += movie_[movieRead_ + f * 2 + 1] * movieVolume_;
    }
    movieRead_ += n * 2;
    for (int f = 0; f < frames * 2; f++) out[f] = std::clamp(out[f], -1.0f, 1.0f);
}

}  // namespace vnns

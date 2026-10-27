#include "audio.h"

#include <algorithm>
#include <cstring>

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "../third_party/stb_vorbis.c"
// stb_vorbis leaks single-letter macros that clash with later code.
#undef L
#undef C
#undef R

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
            uint32_t size = le32(&data_[p + 4]);
            const uint8_t *body = &data_[p + 8];
            if (!memcmp(&data_[p], "fmt ", 4) && size >= 16) {
                format = le16(body);
                channels_ = le16(body + 2);
                rate_ = int(le32(body + 4));
                bits_ = le16(body + 14);
                if (format == 0xFFFE && size >= 26) format = le16(body + 24);
            } else if (!memcmp(&data_[p], "data", 4)) {
                begin_ = p + 8;
                end_ = std::min(data_.size(), begin_ + size);
                break;
            }
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

}  // namespace

std::unique_ptr<AudioSource> openAudio(Bytes data) {
    if (data.size() >= 4 && !memcmp(data.data(), "OggS", 4)) {
        auto s = std::make_unique<OggSource>(std::move(data));
        if (s->ok()) return s;
        return nullptr;
    }
    auto s = std::make_unique<WavSource>(std::move(data));
    if (s->ok()) return s;
    return nullptr;
}

bool AudioMixer::play(int channel, Bytes data, bool loop, int volume, int fadeInMs, double loopStart) {
    auto src = openAudio(std::move(data));
    std::lock_guard<std::mutex> lock(mutex_);
    Channel &c = ch_[size_t(channel)];
    c = Channel();
    if (!src) return false;
    c.src = std::move(src);
    c.loop = loop;
    c.loopStart = loopStart;
    c.volume = std::clamp(volume, 0, 100) / 100.0f;
    if (fadeInMs > 0) {
        c.gain = 0;
        c.fadeStep = 1.0f / (fadeInMs * kRate / 1000.0f);
    }
    return true;
}

void AudioMixer::stop(int channel, int fadeOutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    Channel &c = ch_[size_t(channel)];
    if (!c.src) return;
    if (fadeOutMs > 0 && !paused_) {
        c.fadeStep = -c.gain / (fadeOutMs * kRate / 1000.0f);
        c.stopAfterFade = true;
    } else {
        c = Channel();
    }
}

bool AudioMixer::playing(int channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    Channel &c = ch_[size_t(channel)];
    return c.src && !c.ended;
}

void AudioMixer::setVolume(int channel, int volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    ch_[size_t(channel)].volume = std::clamp(volume, 0, 100) / 100.0f;
}

void AudioMixer::setPaused(bool paused) {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = paused;
}

void AudioMixer::queueMovieSamples(const float *interleaved, int frames, int volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (movieRead_ > 0 && movieRead_ * 2 > movie_.size()) {
        movie_.erase(movie_.begin(), movie_.begin() + long(movieRead_));
        movieRead_ = 0;
    }
    movie_.insert(movie_.end(), interleaved, interleaved + frames * 2);
    movieVolume_ = std::clamp(volume, 0, 100) / 100.0f;
}

void AudioMixer::clearMovieSamples() {
    std::lock_guard<std::mutex> lock(mutex_);
    movie_.clear();
    movieRead_ = 0;
}

int AudioMixer::movieQueuedFrames() {
    std::lock_guard<std::mutex> lock(mutex_);
    return int((movie_.size() - movieRead_) / 2);
}

// Next output frame of a channel, resampled linearly from the source's rate.
bool AudioMixer::pull(Channel &c, float &l, float &r) {
    double ratio = double(c.src->rate()) / kRate;
    // Keep two source frames ahead of the read position for interpolation.
    while (c.bufIndex + 1 >= c.bufFrames) {
        if (c.ended) return false;
        float last[2] = {0, 0};
        bool haveLast = c.bufFrames > 0 && c.bufIndex < c.bufFrames;
        if (haveLast) {
            last[0] = c.buf[size_t(c.bufIndex) * 2];
            last[1] = c.buf[size_t(c.bufIndex) * 2 + 1];
        }
        c.buf.resize(2 * 1025);
        int start = 0;
        if (haveLast) {
            c.buf[0] = last[0];
            c.buf[1] = last[1];
            start = 1;
        }
        int got = c.src->read(c.buf.data() + start * 2, 1024);
        if (got <= 0) {
            if (c.loop && c.src->rewind(c.loopStart)) got = c.src->read(c.buf.data() + start * 2, 1024);
            if (got <= 0) {
                c.ended = true;
                if (!haveLast) return false;
                c.buf[2] = last[0];
                c.buf[3] = last[1];
                got = 1;
            }
        }
        c.bufFrames = start + got;
        c.bufIndex = 0;
    }
    float t = float(c.pos);
    size_t i = size_t(c.bufIndex) * 2;
    l = c.buf[i] + (c.buf[i + 2] - c.buf[i]) * t;
    r = c.buf[i + 1] + (c.buf[i + 3] - c.buf[i + 1]) * t;
    c.pos += ratio;
    while (c.pos >= 1.0) {
        c.pos -= 1.0;
        c.bufIndex++;
        if (c.bufIndex + 1 >= c.bufFrames && c.ended) break;
    }
    return true;
}

void AudioMixer::render(float *out, int frames) {
    std::fill(out, out + frames * 2, 0.0f);
    std::lock_guard<std::mutex> lock(mutex_);
    if (paused_) return;
    for (Channel &c : ch_) {
        if (!c.src || (c.ended && c.bufIndex + 1 >= c.bufFrames)) continue;
        for (int f = 0; f < frames; f++) {
            float l, r;
            if (!pull(c, l, r)) {
                c.ended = true;
                break;
            }
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
                        c.bufFrames = 0;
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

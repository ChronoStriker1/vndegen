#include "audio.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_HEADER_ONLY
#include "../third_party/stb/stb_vorbis.c"

namespace s4 {

namespace {
constexpr int kRate = 44100;

uint32_t rd32(const uint8_t *p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
uint16_t rd16(const uint8_t *p) { uint16_t v; std::memcpy(&v, p, 2); return v; }
}  // namespace

// A source of interleaved float samples at its own rate.
struct Decoder {
    virtual ~Decoder() = default;
    virtual int read(float *out, int frames) = 0;  // returns frames read, 0 at the end
    virtual void seek(int64_t frame) = 0;
    int channels = 2, rate = kRate;
    int64_t length = 0;  // frames
};

namespace {

struct VorbisDecoder : Decoder {
    std::shared_ptr<const std::vector<uint8_t>> data;
    stb_vorbis *v = nullptr;
    explicit VorbisDecoder(std::shared_ptr<const std::vector<uint8_t>> d) : data(std::move(d)) {
        int err = 0;
        v = stb_vorbis_open_memory(data->data(), int(data->size()), &err, nullptr);
        if (!v) return;
        stb_vorbis_info info = stb_vorbis_get_info(v);
        channels = info.channels;
        rate = int(info.sample_rate);
        length = stb_vorbis_stream_length_in_samples(v);
    }
    ~VorbisDecoder() override { if (v) stb_vorbis_close(v); }
    int read(float *out, int frames) override {
        return v ? stb_vorbis_get_samples_float_interleaved(v, channels, out, frames * channels) : 0;
    }
    void seek(int64_t f) override { if (v) stb_vorbis_seek(v, unsigned(std::max<int64_t>(0, f))); }
};

struct WavDecoder : Decoder {
    std::shared_ptr<const std::vector<uint8_t>> data;
    size_t start = 0, bytes = 0, pos = 0;
    int bits = 16;
    explicit WavDecoder(std::shared_ptr<const std::vector<uint8_t>> d) : data(std::move(d)) {
        const auto &b = *data;
        if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) || std::memcmp(b.data() + 8, "WAVE", 4)) return;
        size_t i = 12;
        while (i + 8 <= b.size()) {
            uint32_t sz = rd32(b.data() + i + 4);
            if (!std::memcmp(b.data() + i, "fmt ", 4) && i + 24 <= b.size()) {
                channels = rd16(b.data() + i + 10);
                rate = int(rd32(b.data() + i + 12));
                bits = rd16(b.data() + i + 22);
            } else if (!std::memcmp(b.data() + i, "data", 4)) {
                start = i + 8;
                bytes = std::min<size_t>(sz, b.size() - start);
                break;
            }
            i += 8 + sz + (sz & 1);
        }
        if (channels < 1 || (bits != 8 && bits != 16)) { bytes = 0; channels = 1; }
        length = int64_t(bytes / (channels * bits / 8));
    }
    int read(float *out, int frames) override {
        int fb = channels * bits / 8;
        int n = int(std::min<size_t>(frames, (bytes - pos) / fb));
        const uint8_t *p = data->data() + start + pos;
        for (int k = 0; k < n * channels; k++)
            out[k] = bits == 16 ? int16_t(rd16(p + k * 2)) / 32768.0f : (p[k] - 128) / 128.0f;
        pos += size_t(n) * fb;
        return n;
    }
    void seek(int64_t f) override { pos = std::min<size_t>(bytes, size_t(std::max<int64_t>(0, f)) * (channels * bits / 8)); }
};

}  // namespace

struct Audio::Player {
    std::unique_ptr<Decoder> dec;
    bool playing = false, paused = false;
    float volume = 1.0f;          // 0..1
    float fadeFrom = 1, fadeTo = 1;
    int64_t fadeLen = 0, fadePos = 0;
    bool fadeStop = false;
    int loops = 1;                // 0 forever
    int played = 0;
    int64_t loopStart = 0, loopEnd = 0;
    int64_t pos = 0;              // frames of the source played
    int group = 0;
    std::vector<float> buf;
    double frac = 0;              // resampling position between source frames
    float prev[2] = {0, 0}, cur[2] = {0, 0};
    bool primed = false;

    bool next(float &l, float &r) {
        // Linear interpolation from the source rate to 44.1 kHz.
        double step = double(dec->rate) / kRate;
        while (!primed || frac >= 1.0) {
            float f[16];
            if (loopEnd > 0 && pos >= loopEnd) {
                if (!loopAgain()) return false;
            }
            int n = dec->read(f, 1);
            if (n <= 0) {
                if (!loopAgain()) return false;
                n = dec->read(f, 1);
                if (n <= 0) return false;
            }
            pos++;
            prev[0] = cur[0];
            prev[1] = cur[1];
            cur[0] = f[0];
            cur[1] = dec->channels > 1 ? f[1] : f[0];
            if (!primed) { prev[0] = cur[0]; prev[1] = cur[1]; primed = true; frac = 1.0; }
            frac -= 1.0;
        }
        float t = float(frac);
        l = prev[0] + (cur[0] - prev[0]) * t;
        r = prev[1] + (cur[1] - prev[1]) * t;
        frac += step;
        return true;
    }
    bool loopAgain() {
        played++;
        if (loops != 0 && played >= loops) return false;
        dec->seek(loopStart);
        pos = loopStart;
        return true;
    }
};

Audio::Audio() = default;
Audio::~Audio() = default;

int Audio::open(std::shared_ptr<const std::vector<uint8_t>> data) {
    if (!data || data->size() < 12) return 0;
    std::unique_ptr<Decoder> d;
    if (!std::memcmp(data->data(), "OggS", 4)) {
        auto v = std::make_unique<VorbisDecoder>(data);
        if (v->v) d = std::move(v);
    } else if (!std::memcmp(data->data(), "RIFF", 4)) {
        auto w = std::make_unique<WavDecoder>(data);
        if (w->length > 0) d = std::move(w);
    }
    if (!d) return 0;
    auto p = std::make_unique<Player>();
    p->dec = std::move(d);
    std::lock_guard<std::mutex> lock(mu_);
    int h = next_++;
    players_[h] = std::move(p);
    return h;
}

#define WITH(h) std::lock_guard<std::mutex> lock(mu_); auto it = players_.find(h); if (it == players_.end()) return
void Audio::close(int h) { std::lock_guard<std::mutex> lock(mu_); players_.erase(h); }
void Audio::play(int h) {
    WITH(h);
    auto &p = *it->second;
    p.dec->seek(0);
    p.pos = 0;
    p.played = 0;
    p.primed = false;
    p.playing = true;
    p.paused = false;
    if (p.fadeLen == 0) p.volume = 1.0f;
}
void Audio::stop(int h) { WITH(h); it->second->playing = false; }
void Audio::pause(int h, bool v) { WITH(h); it->second->paused = v; }
bool Audio::playing(int h) { WITH(h) false; return it->second->playing; }
bool Audio::paused(int h) { WITH(h) false; return it->second->paused; }
void Audio::fade(int h, int ms, int volume, bool stop) {
    WITH(h);
    auto &p = *it->second;
    float to = std::clamp(volume, 0, 100) / 100.0f;
    if (ms <= 0) {
        p.volume = to;
        p.fadeLen = 0;
        if (stop) p.playing = false;
        return;
    }
    p.fadeFrom = p.volume;
    p.fadeTo = to;
    p.fadeLen = int64_t(ms) * kRate / 1000;
    p.fadePos = 0;
    p.fadeStop = stop;
}
bool Audio::fading(int h) { WITH(h) false; return it->second->fadeLen > 0 && it->second->playing; }
void Audio::stopFade(int h) { WITH(h); it->second->fadeLen = 0; }
void Audio::setLoops(int h, int count) { WITH(h); it->second->loops = std::max(0, count); }
int Audio::loops(int h) { WITH(h) 0; return it->second->loops; }
void Audio::setLoopPoints(int h, int64_t s, int64_t e) { WITH(h); it->second->loopStart = s; it->second->loopEnd = e; }
int64_t Audio::positionSamples(int h) { WITH(h) 0; return it->second->pos; }
int64_t Audio::lengthSamples(int h) { WITH(h) 0; return it->second->dec->length; }
int64_t Audio::positionMs(int h) { WITH(h) 0; return it->second->pos * 1000 / std::max(1, it->second->dec->rate); }
int64_t Audio::lengthMs(int h) { WITH(h) 0; return it->second->dec->length * 1000 / std::max(1, it->second->dec->rate); }
void Audio::seekMs(int h, int64_t ms) {
    WITH(h);
    auto &p = *it->second;
    p.pos = ms * p.dec->rate / 1000;
    p.dec->seek(p.pos);
    p.primed = false;
}
void Audio::setGroup(int h, int g) { WITH(h); it->second->group = g; }
#undef WITH
void Audio::setGroupVolume(int g, int v) { std::lock_guard<std::mutex> lock(mu_); groupVolume_[g] = std::clamp(v, 0, 100); }

void Audio::render(float *out, int frames) {
    std::memset(out, 0, sizeof(float) * 2 * frames);
    std::lock_guard<std::mutex> lock(mu_);
    for (auto &[h, pp] : players_) {
        Player &p = *pp;
        if (!p.playing || p.paused) continue;
        auto gv = groupVolume_.find(p.group);
        float group = gv == groupVolume_.end() ? 1.0f : gv->second / 100.0f;
        for (int i = 0; i < frames; i++) {
            if (p.fadeLen > 0) {
                p.fadePos++;
                p.volume = p.fadeFrom + (p.fadeTo - p.fadeFrom) * float(p.fadePos) / float(p.fadeLen);
                if (p.fadePos >= p.fadeLen) {
                    p.volume = p.fadeTo;
                    p.fadeLen = 0;
                    if (p.fadeStop) { p.playing = false; break; }
                }
            }
            float l, r;
            if (!p.next(l, r)) { p.playing = false; break; }
            float v = p.volume * group;
            out[i * 2] += l * v;
            out[i * 2 + 1] += r * v;
        }
    }
    for (int i = 0; i < frames * 2; i++) out[i] = std::clamp(out[i], -1.0f, 1.0f);
}

}  // namespace s4

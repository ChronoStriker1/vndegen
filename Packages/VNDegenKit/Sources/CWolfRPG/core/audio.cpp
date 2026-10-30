#include "audio.h"

#include "vnmidi.h"

#include "files.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iterator>
#include <mutex>
#include <thread>
#include <vector>

// The decoders are compiled into this file only. dr_mp3 and dr_wav are static, and stb_vorbis's functions take names
// of their own, so they don't clash with the copies other engines in the app compile.
#define DRMP3_API static
#define DRMP3_PRIVATE static
#define DR_MP3_IMPLEMENTATION
#include "../third_party/dr_libs/dr_mp3.h"
#define DRWAV_API static
#define DRWAV_PRIVATE static
#define DR_WAV_IMPLEMENTATION
#include "../third_party/dr_libs/dr_wav.h"
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define stb_vorbis_close wolf_vorbis_close
#define stb_vorbis_decode_memory wolf_vorbis_decode_memory
#define stb_vorbis_get_comment wolf_vorbis_get_comment
#define stb_vorbis_get_error wolf_vorbis_get_error
#define stb_vorbis_get_file_offset wolf_vorbis_get_file_offset
#define stb_vorbis_get_frame_float wolf_vorbis_get_frame_float
#define stb_vorbis_get_frame_short wolf_vorbis_get_frame_short
#define stb_vorbis_get_frame_short_interleaved wolf_vorbis_get_frame_short_interleaved
#define stb_vorbis_get_info wolf_vorbis_get_info
#define stb_vorbis_get_sample_offset wolf_vorbis_get_sample_offset
#define stb_vorbis_get_samples_float wolf_vorbis_get_samples_float
#define stb_vorbis_get_samples_float_interleaved wolf_vorbis_get_samples_float_interleaved
#define stb_vorbis_get_samples_short wolf_vorbis_get_samples_short
#define stb_vorbis_get_samples_short_interleaved wolf_vorbis_get_samples_short_interleaved
#define stb_vorbis_open_memory wolf_vorbis_open_memory
#define stb_vorbis_seek wolf_vorbis_seek
#define stb_vorbis_seek_frame wolf_vorbis_seek_frame
#define stb_vorbis_seek_start wolf_vorbis_seek_start
#define stb_vorbis_stream_length_in_samples wolf_vorbis_stream_length_in_samples
#define stb_vorbis_stream_length_in_seconds wolf_vorbis_stream_length_in_seconds
#include "../third_party/stb/stb_vorbis.c"

namespace wolf {

namespace {

// A file decoded a piece at a time. Positions count frames at the file's own rate. MIDI renders at the output rate
// and counts song time in frames of that rate.
struct Decoder {
    int channels = 2, rate = 44100;
    int64_t length = -1;                         // in frames, or -1 until the end is reached
    int64_t tagLoopStart = -1, tagLoopEnd = -1;  // the file's own loop, if it has one
    int64_t pos = 0;

    virtual ~Decoder() = default;
    // Reads up to `frames` frames of interleaved samples. Fewer means the file ended.
    virtual int read(float *out, int frames) = 0;
    virtual bool seek(int64_t frame) = 0;
    // MIDI changes tempo itself; sampled files are resampled by the mixer instead. Called from the game thread.
    virtual bool setTempo(double) { return false; }
};

struct OggDecoder : Decoder {
    std::vector<uint8_t> bytes;
    stb_vorbis *v = nullptr;

    ~OggDecoder() override { if (v) stb_vorbis_close(v); }

    static std::unique_ptr<Decoder> open(std::vector<uint8_t> data) {
        auto d = std::make_unique<OggDecoder>();
        d->bytes = std::move(data);
        int error = 0;
        d->v = stb_vorbis_open_memory(d->bytes.data(), int(d->bytes.size()), &error, nullptr);
        if (!d->v) return nullptr;
        stb_vorbis_info info = stb_vorbis_get_info(d->v);
        d->channels = std::min(info.channels, 2);
        d->rate = int(info.sample_rate);
        if (unsigned n = stb_vorbis_stream_length_in_samples(d->v)) d->length = n;
        // RPG Maker style loop tags, in samples: LOOPSTART=n and LOOPLENGTH=n.
        stb_vorbis_comment c = stb_vorbis_get_comment(d->v);
        int64_t start = -1, len = -1;
        for (int i = 0; i < c.comment_list_length; i++) {
            std::string s = c.comment_list[i];
            auto eq = s.find('=');
            if (eq == std::string::npos) continue;
            std::string key = s.substr(0, eq);
            for (char &ch : key) ch = char(std::toupper((unsigned char)ch));
            if (key == "LOOPSTART") start = std::atoll(s.c_str() + eq + 1);
            if (key == "LOOPLENGTH") len = std::atoll(s.c_str() + eq + 1);
        }
        if (start >= 0) {
            d->tagLoopStart = start;
            if (len > 0) d->tagLoopEnd = start + len;
        }
        return d;
    }
    int read(float *out, int frames) override {
        int n = stb_vorbis_get_samples_float_interleaved(v, channels, out, frames * channels);
        pos += n;
        return n;
    }
    bool seek(int64_t frame) override {
        if (!stb_vorbis_seek(v, unsigned(frame))) return false;
        pos = frame;
        return true;
    }
};

struct Mp3Decoder : Decoder {
    std::vector<uint8_t> bytes;
    drmp3 mp3{};
    bool ok = false;

    ~Mp3Decoder() override { if (ok) drmp3_uninit(&mp3); }

    static std::unique_ptr<Decoder> open(std::vector<uint8_t> data) {
        auto d = std::make_unique<Mp3Decoder>();
        d->bytes = std::move(data);
        if (!(d->ok = drmp3_init_memory(&d->mp3, d->bytes.data(), d->bytes.size(), nullptr))) return nullptr;
        d->channels = int(d->mp3.channels);
        d->rate = int(d->mp3.sampleRate);
        // Known from a Xing or LAME header; without one the length is found on reaching the end.
        auto &m = d->mp3;
        if (m.totalPCMFrameCount != DRMP3_UINT64_MAX)
            d->length = int64_t(m.totalPCMFrameCount) - m.delayInPCMFrames - m.paddingInPCMFrames;
        return d;
    }
    int read(float *out, int frames) override {  // MP3 has one or two channels
        int n = int(drmp3_read_pcm_frames_f32(&mp3, frames, out));
        pos += n;
        return n;
    }
    bool seek(int64_t frame) override {
        if (!drmp3_seek_to_pcm_frame(&mp3, drmp3_uint64(frame))) return false;
        pos = frame;
        return true;
    }
};

struct WavDecoder : Decoder {
    std::vector<uint8_t> bytes;
    drwav wav{};
    bool ok = false;
    std::vector<float> wide;  // for files with more than two channels

    ~WavDecoder() override { if (ok) drwav_uninit(&wav); }

    static std::unique_ptr<Decoder> open(std::vector<uint8_t> data) {
        auto d = std::make_unique<WavDecoder>();
        d->bytes = std::move(data);
        if (!(d->ok = drwav_init_memory(&d->wav, d->bytes.data(), d->bytes.size(), nullptr))) return nullptr;
        if (d->wav.channels == 0 || d->wav.sampleRate == 0) return nullptr;
        d->channels = std::min(int(d->wav.channels), 2);
        d->rate = int(d->wav.sampleRate);
        d->length = int64_t(d->wav.totalPCMFrameCount);
        return d;
    }
    int read(float *out, int frames) override {
        if (wav.channels <= 2) {
            int n = int(drwav_read_pcm_frames_f32(&wav, frames, out));
            pos += n;
            return n;
        }
        wide.resize(size_t(frames) * wav.channels);
        int n = int(drwav_read_pcm_frames_f32(&wav, frames, wide.data()));
        for (int i = 0; i < n; i++) std::copy_n(&wide[size_t(i) * wav.channels], 2, out + i * 2);
        pos += n;
        return n;
    }
    bool seek(int64_t frame) override {
        if (!drwav_seek_to_pcm_frame(&wav, drwav_uint64(frame))) return false;
        pos = frame;
        return true;
    }
};

// MIDI, through VNDegen's shared MIDI player (Engines/Shared/vnmidi.h).

struct SoundFont {
    std::mutex mutex;
    std::string path, fallback;
    vnmidi_font *font = nullptr;
    bool tried = false;

    ~SoundFont() { if (font) vnmidi_font_free(font); }
};

struct MidiDecoder : Decoder {
    vnmidi_player *player = nullptr;

    ~MidiDecoder() override { vnmidi_player_close(player); }
    int read(float *out, int frames) override {
        int n = vnmidi_player_render(player, out, frames);
        pos += n;
        return n;
    }
    bool seek(int64_t frame) override {
        vnmidi_player_seek(player, int(frame * 1000 / rate));
        pos = frame;
        return true;
    }
    bool setTempo(double t) override {
        vnmidi_player_set_tempo(player, float(t));
        return true;
    }
};

// `keyShift` transposes every channel but the drums by that many semitones.
std::unique_ptr<Decoder> openMidi(std::vector<uint8_t> bytes, SoundFont &sf, int rate, int keyShift,
                                  const std::function<void(const std::string &)> &log) {
    vnmidi_font *font = nullptr;
    {
        std::lock_guard lock(sf.mutex);
        if (!sf.tried) {
            sf.tried = true;
            char error[256] = "";
            if (sf.path.empty()) log("audio: no SoundFont set, MIDI files play silently");
            else {
                sf.font = vnmidi_font_load_fallback(sf.path.c_str(), sf.fallback.c_str(), error, sizeof error);
                if (!sf.font) log("audio: can't load SoundFont: " + std::string(error));
                else if (*error) log("audio: " + std::string(error));
            }
        }
        font = sf.font;
    }
    if (!font) return nullptr;
    char error[256] = "";
    vnmidi_player *player = vnmidi_player_open(font, bytes.data(), bytes.size(), rate, error, sizeof error);
    if (!player) {
        log(std::string("audio: can't read MIDI file: ") + error);
        return nullptr;
    }
    auto d = std::make_unique<MidiDecoder>();
    d->player = player;
    d->rate = rate;
    vnmidi_player_set_transpose(player, keyShift);
    d->length = int64_t(vnmidi_player_length_ms(player)) * rate / 1000;
    // CC111 marks the loop start, as RPG Maker MIDI files do. The mixer loops the song itself, by seeking.
    if (int marker = vnmidi_player_marker_ms(player); marker >= 0) d->tagLoopStart = int64_t(marker) * rate / 1000;
    return d;
}

// End of the MIDI section.

bool isMidi(const std::vector<uint8_t> &b) { return b.size() >= 4 && std::memcmp(b.data(), "MThd", 4) == 0; }

std::unique_ptr<Decoder> openSampled(std::vector<uint8_t> bytes) {
    if (bytes.size() >= 4 && std::memcmp(bytes.data(), "OggS", 4) == 0) return OggDecoder::open(std::move(bytes));
    if (bytes.size() >= 12 && (std::memcmp(bytes.data(), "RIFF", 4) == 0 || std::memcmp(bytes.data(), "RIFX", 4) == 0))
        return WavDecoder::open(std::move(bytes));
    return Mp3Decoder::open(std::move(bytes));
}

std::string pathKey(const std::string &path) {
    std::string s = normalizePath(path);
    for (char &c : s) c = char(std::toupper((unsigned char)c));
    return s;
}

constexpr int chunkFrames = 1024;

// One playing file. The decoding thread owns the decoder and writes the ring; the audio thread reads it. The mixer
// state is read and written under Impl::mutex.
struct Source {
    SoundKind kind;
    std::string path, key;
    std::unique_ptr<Decoder> dec;
    bool tempoNative = false;  // MIDI
    bool loop = false;
    int64_t startFrame = 0, loopStart = 0;
    std::atomic<int64_t> loopEnd{-1};

    std::vector<float> ring;  // stereo
    uint64_t capacity = 0;    // frames
    std::atomic<uint64_t> written{0}, consumed{0};
    std::atomic<bool> ended{false};
    std::atomic<bool> dead{false};

    // Where each decoded chunk starts in the file, so the game thread can tell the position of what it hears.
    struct Mark {
        uint64_t at;
        int64_t pos;
        double perFrame;
    };
    std::mutex markMutex;
    std::deque<Mark> marks;

    // Mixer state.
    double phase = 0, step = 1;
    float volume = 1, env = 1, envTarget = 1, envStep = 0, gain = 0;
    bool stopping = false, started = false;
    int64_t delay = 0;  // output frames before it starts
};

// Many sound effects at once can add up past full scale. Below the knee samples pass unchanged; above it they bend
// smoothly towards 1 instead of clipping hard.
float softClip(float x) {
    constexpr float knee = 0.9f, room = 1 - knee;
    float a = std::fabs(x);
    if (a <= knee) return x;
    return std::copysign(knee + room * std::tanh((a - knee) / room), x);
}

float percentGain(int volume) { return std::max(0, volume) / 100.0f; }
double rateOf(int frequency) { return (frequency <= 0 ? 100 : frequency) / 100.0; }

}  // namespace

struct Audio::Impl {
    const GameFiles &files;
    Audio::Log log;
    const int rate;
    const float gainSlew;  // about 5 ms from full volume to silence, so starts, stops and changes don't click
    SoundFont font;
    int fps = 60;

    std::mutex mutex;  // sources, music, master, paused, and the sources' mixer state
    std::vector<std::shared_ptr<Source>> sources;
    std::shared_ptr<Source> music[2];
    float master[3] = {1, 1, 1};
    bool paused = false;
    // Sources the mixer has dropped, freed by the decoding thread. Its capacity always covers every source, so the
    // mixer never allocates.
    std::vector<std::shared_ptr<Source>> finished;

    std::vector<std::shared_ptr<Source>> snapshot, dropped;  // the decoding thread's
    std::thread worker;
    std::mutex workerMutex;
    std::condition_variable wake;
    bool quit = false;

    Impl(const GameFiles &f, int r, Audio::Log l)
        : files(f), log(l ? std::move(l) : [](const std::string &) {}), rate(r), gainSlew(200.0f / r) {
        if (const char *env = std::getenv("WOLF_SOUNDFONT")) font.path = env;
    }

    int framesOf(int gameFrames) const { return int(int64_t(std::max(0, gameFrames)) * rate / fps); }

    std::shared_ptr<Source> open(SoundKind kind, const std::string &path, int keyShift) {
        auto bytes = files.read(path);
        if (!bytes) {
            log("audio: missing " + path);
            return nullptr;
        }
        bool midi = isMidi(*bytes);
        auto dec = midi ? openMidi(std::move(*bytes), font, rate, keyShift, log) : openSampled(std::move(*bytes));
        if (!dec) {
            if (!midi) log("audio: can't decode " + path);
            else if (font.font) log("audio: can't play " + path);
            return nullptr;
        }
        auto s = std::make_shared<Source>();
        s->kind = kind;
        s->path = path;
        s->key = pathKey(path);
        s->tempoNative = midi;
        s->dec = std::move(dec);
        s->capacity = uint64_t((kind == SoundKind::SE ? 0.5 : 2.0) * s->dec->rate) + chunkFrames * 2;
        s->ring.resize(s->capacity * 2);
        return s;
    }

    // Fades `s` out over `frames` output frames and then drops it. Called with `mutex` held.
    void fadeOut(Source &s, int frames) {
        s.stopping = true;
        s.envTarget = 0;
        if (frames <= 0 || !s.started) s.env = 0;
        else s.envStep = s.env / frames;
        if (!s.started) s.gain = 0;
    }
    void setRate(Source &s, int frequency) {
        double r = rateOf(frequency);
        if (s.tempoNative) s.dec->setTempo(r);
        else s.step = double(s.dec->rate) * r / rate;
    }

    // Adds a playing source. Called with `mutex` held.
    void add(std::shared_ptr<Source> s) {
        sources.push_back(std::move(s));
        finished.reserve(sources.size() + finished.size());
    }

    void notify() {
        std::lock_guard lock(workerMutex);
        wake.notify_one();
    }

    // Decodes ahead for every source until its ring is full, and frees finished ones.
    void pump() {
        {
            // Copied into buffers that keep their capacity, so the audio thread rarely waits on an allocation.
            std::lock_guard lock(mutex);
            snapshot.assign(sources.begin(), sources.end());
            dropped.assign(std::make_move_iterator(finished.begin()), std::make_move_iterator(finished.end()));
            finished.clear();
        }
        dropped.clear();
        for (auto &s : snapshot)
            if (!s->dead.load(std::memory_order_relaxed)) fill(*s);
        snapshot.clear();
    }

    void fill(Source &s) {
        Decoder &d = *s.dec;
        float buf[chunkFrames * 2];
        uint64_t w = s.written.load(std::memory_order_relaxed);
        if (w == 0 && d.pos != s.startFrame && !d.seek(s.startFrame)) {
            s.ended = true;
            return;
        }
        bool emptySinceSeek = false;  // a loop that yields nothing ends the sound instead of spinning
        while (!s.ended.load(std::memory_order_relaxed) &&
               s.capacity - (w - s.consumed.load(std::memory_order_acquire)) >= chunkFrames) {
            int64_t end = s.loopEnd.load(std::memory_order_relaxed);
            int want = chunkFrames;
            if (s.loop && end > 0 && !s.tempoNative) want = int(std::clamp<int64_t>(end - d.pos, 0, chunkFrames));
            int64_t before = d.pos;
            int n = want > 0 ? d.read(buf, want) : 0;
            if (n > 0) {
                emptySinceSeek = false;
                for (int i = 0; i < n; i++) {
                    size_t at = size_t((w + i) % s.capacity) * 2;
                    s.ring[at] = buf[i * d.channels];
                    s.ring[at + 1] = buf[i * d.channels + d.channels - 1];
                }
                {
                    std::lock_guard lock(s.markMutex);
                    s.marks.push_back({w, before, double(d.pos - before) / n});
                    uint64_t c = s.consumed.load(std::memory_order_relaxed);
                    while (s.marks.size() > 1 && s.marks[1].at <= c) s.marks.pop_front();
                }
                w += n;
                s.written.store(w, std::memory_order_release);
            }
            bool fileEnded = n < want;
            if (!fileEnded && !(s.loop && end > 0 && d.pos >= end)) continue;
            if (fileEnded) {
                if (d.length < 0) d.length = d.pos;
                if (end < 0 || end > d.pos) s.loopEnd.store(d.pos, std::memory_order_relaxed);
            }
            if (!s.loop || emptySinceSeek || !d.seek(s.loopStart)) {
                s.ended = true;
                break;
            }
            emptySinceSeek = true;
        }
    }

    void runWorker() {
        std::unique_lock lock(workerMutex);
        while (!quit) {
            lock.unlock();
            pump();
            lock.lock();
            wake.wait_for(lock, std::chrono::milliseconds(5));
        }
    }

    void mix(float *out, int frames) {
        std::fill_n(out, size_t(frames) * 2, 0.0f);
        std::lock_guard lock(mutex);
        if (paused) return;
        for (auto &sp : sources) mixOne(*sp, out, frames);
        for (int i = 0; i < frames * 2; i++) out[i] = softClip(out[i]);
        for (size_t i = 0; i < sources.size();) {
            Source &s = *sources[i];
            bool gone = (s.stopping && s.env <= 0 && s.gain <= 0) ||
                        (s.ended && s.written.load(std::memory_order_acquire) - s.consumed.load() < 2);
            if (gone) {
                s.dead = true;
                for (auto &m : music)
                    if (m == sources[i]) m = nullptr;
                finished.push_back(std::move(sources[i]));
                sources.erase(sources.begin() + i);
            } else {
                i++;
            }
        }
    }

    void mixOne(Source &s, float *out, int frames) {
        int i = 0;
        if (s.delay > 0) {
            int skip = int(std::min<int64_t>(s.delay, frames));
            s.delay -= skip;
            i = skip;
        }
        uint64_t r = s.consumed.load(std::memory_order_relaxed);
        uint64_t w = s.written.load(std::memory_order_acquire);
        for (; i < frames; i++) {
            if (r + 2 > w) break;  // not decoded yet, or the end
            if (s.env != s.envTarget) {
                s.env = s.env < s.envTarget ? std::min(s.envTarget, s.env + s.envStep)
                                            : std::max(s.envTarget, s.env - s.envStep);
            }
            float target = s.volume * s.env * master[int(s.kind)];
            if (!s.started) {
                s.gain = target;
                s.started = true;
            }
            s.gain += std::clamp(target - s.gain, -gainSlew, gainSlew);
            const float *a = &s.ring[(r % s.capacity) * 2];
            const float *b = &s.ring[((r + 1) % s.capacity) * 2];
            float t = float(s.phase);
            out[i * 2] += (a[0] + (b[0] - a[0]) * t) * s.gain;
            out[i * 2 + 1] += (a[1] + (b[1] - a[1]) * t) * s.gain;
            s.phase += s.step;
            auto whole = uint64_t(s.phase);
            r += whole;
            s.phase -= double(whole);
            if (s.stopping && s.env <= 0 && s.gain <= 0) break;
        }
        // A starved or stopped source still lets its gain settle, so it can be removed.
        if (i < frames && s.stopping && s.env <= 0) s.gain = 0;
        s.consumed.store(std::min(r, w), std::memory_order_release);
    }

    int positionMs(SoundKind channel) {
        std::shared_ptr<Source> s;
        {
            std::lock_guard lock(mutex);
            s = music[int(channel)];
        }
        if (!s) return 0;
        uint64_t c = s->consumed.load(std::memory_order_acquire);
        std::lock_guard lock(s->markMutex);
        if (s->marks.empty()) return int(s->startFrame * 1000 / s->dec->rate);
        auto it = std::upper_bound(s->marks.begin(), s->marks.end(), c,
                                   [](uint64_t v, const Source::Mark &m) { return v < m.at; });
        if (it != s->marks.begin()) --it;
        double pos = double(it->pos) + double(c - std::min(c, it->at)) * it->perFrame;
        return int(pos * 1000.0 / s->dec->rate);
    }
};

Audio::Audio(const GameFiles &files, int sampleRate, bool decodeThread, Log log)
    : impl_(std::make_unique<Impl>(files, std::max(8000, sampleRate), std::move(log))) {
    if (decodeThread) impl_->worker = std::thread([impl = impl_.get()] { impl->runWorker(); });
}

Audio::~Audio() {
    if (impl_->worker.joinable()) {
        {
            std::lock_guard lock(impl_->workerMutex);
            impl_->quit = true;
        }
        impl_->wake.notify_one();
        impl_->worker.join();
    }
}

int Audio::sampleRate() const { return impl_->rate; }

void Audio::setSoundFontPath(std::string path, std::string fallback) {
    std::lock_guard lock(impl_->font.mutex);
    if (path == impl_->font.path && fallback == impl_->font.fallback) return;
    impl_->font.path = std::move(path);
    impl_->font.fallback = std::move(fallback);
    impl_->font.tried = false;
    if (impl_->font.font) vnmidi_font_free(impl_->font.font);
    impl_->font.font = nullptr;
}

void Audio::setFrameRate(int fps) { impl_->fps = std::max(1, fps); }

bool Audio::playMusic(SoundKind channel, const std::string &path, const MusicParams &p, bool restart) {
    int ch = int(channel);
    {
        std::lock_guard lock(impl_->mutex);
        auto &cur = impl_->music[ch];
        if (cur && !restart && cur->key == pathKey(path)) {
            cur->volume = percentGain(p.volume);
            impl_->setRate(*cur, p.frequency);
            return true;
        }
    }
    auto s = impl_->open(channel, path, p.loopStartMs);
    if (s) {
        Decoder &d = *s->dec;
        auto toFrames = [&](int ms) { return int64_t(std::max(0, ms)) * d.rate / 1000; };
        s->loop = true;
        if (s->tempoNative || p.loopStartMs <= 0) {
            s->loopStart = std::max<int64_t>(0, d.tagLoopStart);
            s->loopEnd = d.tagLoopEnd;
        } else {
            s->loopStart = toFrames(p.loopStartMs);
        }
        if (d.length > 0 && s->loopStart >= d.length) s->loopStart = 0;
        s->startFrame = toFrames(p.startMs);
        if (d.length > 0 && s->startFrame >= d.length) s->startFrame = 0;
        s->volume = percentGain(p.volume);
        impl_->setRate(*s, p.frequency);
        if (p.fadeFrames > 0) {
            s->env = 0;
            s->envStep = 1.0f / impl_->framesOf(p.fadeFrames);
            s->started = true;
        }
    }
    {
        std::lock_guard lock(impl_->mutex);
        auto &cur = impl_->music[ch];
        if (cur) impl_->fadeOut(*cur, 0);
        cur = s;
        if (s) impl_->add(s);
    }
    impl_->notify();
    return s != nullptr;
}

void Audio::stopMusic(SoundKind channel, int fadeFrames) {
    std::lock_guard lock(impl_->mutex);
    auto &cur = impl_->music[int(channel)];
    if (!cur) return;
    impl_->fadeOut(*cur, impl_->framesOf(fadeFrames));
    cur = nullptr;
}

std::string Audio::musicPath(SoundKind channel) const {
    std::lock_guard lock(impl_->mutex);
    auto &cur = impl_->music[int(channel)];
    return cur ? cur->path : std::string();
}

int Audio::musicPositionMs(SoundKind channel) const { return impl_->positionMs(channel); }

bool Audio::playSE(const std::string &path, int volume, int frequency, int delayFrames) {
    auto s = impl_->open(SoundKind::SE, path, 0);
    if (!s) return false;
    s->volume = percentGain(volume);
    impl_->setRate(*s, frequency);
    s->delay = impl_->framesOf(delayFrames);
    {
        std::lock_guard lock(impl_->mutex);
        impl_->add(s);
    }
    impl_->notify();
    return true;
}

void Audio::stopAllSE() {
    std::lock_guard lock(impl_->mutex);
    for (auto &s : impl_->sources)
        if (s->kind == SoundKind::SE) impl_->fadeOut(*s, 0);
}

void Audio::setMasterVolume(SoundKind kind, float volume) {
    std::lock_guard lock(impl_->mutex);
    impl_->master[int(kind)] = std::clamp(volume, 0.0f, 1.0f);
}

void Audio::pause() {
    std::lock_guard lock(impl_->mutex);
    impl_->paused = true;
}

void Audio::resume() {
    std::lock_guard lock(impl_->mutex);
    impl_->paused = false;
}

void Audio::render(float *out, int frames) {
    for (; frames > 0; frames -= chunkFrames, out += chunkFrames * 2) {
        if (!impl_->worker.joinable()) impl_->pump();
        impl_->mix(out, std::min(frames, chunkFrames));
    }
}

}  // namespace wolf

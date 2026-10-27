// Movies: MPEG-1 video with MPEG-1 Layer II audio, decoded by pl_mpeg (MIT). mpegplay, movie and avi all play
// through it; a file pl_mpeg can't read is skipped with a log line, as a missing codec would be on Windows.
#include "engine.h"

#include <cstdio>
#include <cstdlib>

#define PL_MPEG_IMPLEMENTATION
#include "../third_party/pl_mpeg.h"

namespace vnns {

struct Engine::Movie {
    Bytes data;
    plm_t *plm = nullptr;
    bool clickStops = false, loop = false, async = false;
    int x = 0, y = 0, w = 0, h = 0;
    Image picture;
    bool newPicture = false;
    ~Movie() {
        if (plm) plm_destroy(plm);
    }
};

// The engine is built and destroyed here, where its movie type is complete.
Engine::Engine(Config cfg) : cfg_(std::move(cfg)) {}
Engine::~Engine() = default;

static uint8_t clamp8(int v) { return uint8_t(v < 0 ? 0 : v > 255 ? 255 : v); }

void Engine::playMovie(const std::string &file, bool clickStops, bool loop, bool async, int x, int y, int w, int h) {
    auto m = std::make_unique<Movie>();
    if (!fs_.read(file, m->data)) {
        errorf("movie %s not found", sjisToUtf8(file).c_str());
        return;
    }
    plm_buffer_t *buf = plm_buffer_create_with_memory(m->data.data(), m->data.size(), 0);
    m->plm = plm_create_with_buffer(buf, 1);
    if (!m->plm || !plm_probe(m->plm, 5000 * 1024)) {
        logf("movie %s is not MPEG-1 and can't be played", sjisToUtf8(file).c_str());
        return;
    }
    m->clickStops = clickStops;
    m->loop = loop;
    m->async = async;
    m->x = x;
    m->y = y;
    m->w = w > 0 ? w : width_;
    m->h = h > 0 ? h : height_;
    plm_set_loop(m->plm, loop ? 1 : 0);
    plm_set_audio_enabled(m->plm, 1);
    plm_set_audio_stream(m->plm, 0);
    if (plm_get_num_audio_streams(m->plm) > 0)
        plm_set_audio_lead_time(m->plm, 4096.0 / plm_get_samplerate(m->plm));
    plm_set_video_decode_callback(
        m->plm,
        [](plm_t *, plm_frame_t *f, void *user) {
            Movie *mv = static_cast<Movie *>(user);
            // YCbCr 4:2:0 to RGB, scaled to the movie's rectangle by nearest neighbour.
            Image &out = mv->picture;
            if (out.w != mv->w || out.h != mv->h) out = Image(mv->w, mv->h);
            for (int yy = 0; yy < mv->h; yy++) {
                int sy = int(int64_t(yy) * f->height / mv->h);
                const uint8_t *Y = f->y.data + size_t(sy) * f->y.width;
                const uint8_t *Cb = f->cb.data + size_t(sy / 2) * f->cb.width;
                const uint8_t *Cr = f->cr.data + size_t(sy / 2) * f->cr.width;
                uint32_t *d = out.row(yy);
                for (int xx = 0; xx < mv->w; xx++) {
                    int sx = int(int64_t(xx) * f->width / mv->w);
                    int c = (Y[sx] - 16) * 298, cb = Cb[sx / 2] - 128, cr = Cr[sx / 2] - 128;
                    d[xx] = rgba(clamp8((c + 409 * cr + 128) >> 8), clamp8((c - 100 * cb - 208 * cr + 128) >> 8),
                                 clamp8((c + 516 * cb + 128) >> 8));
                }
            }
            mv->newPicture = true;
        },
        m.get());
    plm_set_audio_decode_callback(
        m->plm,
        [](plm_t *, plm_samples_t *s, void *user) {
            Engine *e = static_cast<Engine *>(user);
            e->audio_.queueMovieSamples(s->interleaved, int(s->count), e->volBgm_);
        },
        this);
    audio_.clearMovieSamples();
    if (!async) audio_.stop(AudioMixer::kBgm);
    movie_ = std::move(m);
    if (!async) wait_ = WaitKind::Movie;
}

void Engine::stepMovie(int ms) {
    Movie &m = *movie_;
    bool stop = m.clickStops && !m.async && (in_.click || in_.rclick);
    for (int k : in_.keys)
        if (m.clickStops && (k == VNNS_KEY_RETURN || k == VNNS_KEY_SPACE || k == VNNS_KEY_ESCAPE)) stop = true;
    if (!stop) plm_decode(m.plm, ms / 1000.0);
    if (m.newPicture) {
        m.newPicture = false;
        sceneDirty_ = true;
    }
    if (stop || plm_has_ended(m.plm)) {
        movie_.reset();
        audio_.clearMovieSamples();
        sceneDirty_ = true;
        if (wait_ == WaitKind::Movie) wait_ = WaitKind::None;
    }
}

// Movies draw over everything, text included.
void Engine::drawMovie(Image &out) {
    if (movie_ && !movie_->picture.empty())
        blit(out, movie_->picture, movie_->x, movie_->y, 0, 0, movie_->picture.w, movie_->picture.h, 255);
}

namespace {
using E = Engine;

void cmdMpegPlay(E &e, const std::string &name) {
    std::string file = e.readStr();
    int click = 0;
    if (e.readComma()) click = e.readInt();
    (void)name;
    e.playMovie(file, click != 0, false, false, 0, 0, 0, 0);
}

// movie "file"[,pos,x,y,w,h][,click][,loop][,async]  or  movie stop
void cmdMovie(E &e, const std::string &) {
    e.skipSpaces();
    size_t save = e.cur_.pos;
    if (e.readWord() == "stop") {
        e.movie_.reset();
        e.audio_.clearMovieSamples();
        e.sceneDirty_ = true;
        return;
    }
    e.cur_.pos = save;
    std::string file = e.readStr();
    bool click = false, loop = false, async = false;
    int x = 0, y = 0, w = 0, h = 0;
    while (e.readComma()) {
        std::string opt = e.readWord();
        if (opt == "pos") {
            e.readComma();
            x = e.readInt();
            e.readComma();
            y = e.readInt();
            e.readComma();
            w = e.readInt();
            e.readComma();
            h = e.readInt();
        } else if (opt == "click") {
            click = true;
        } else if (opt == "loop") {
            loop = true;
        } else if (opt == "async") {
            async = true;
        }
    }
    e.playMovie(file, click, loop, async, x, y, w, h);
}

}  // namespace

void registerMovieCommands(std::unordered_map<std::string, CommandFn> &t) {
    t["mpegplay"] = cmdMpegPlay;
    t["avi"] = cmdMpegPlay;
    t["movie"] = cmdMovie;
}

}  // namespace vnns

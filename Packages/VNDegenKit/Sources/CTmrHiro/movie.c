#include "th.h"
#include "vnvideo.h"

#include <sys/types.h>
#include <time.h>

struct Movie {
    FILE *file;
    u64 size;
    vnvideo *video;
    vnvideo_info info;
    char name[64];
    double clock;
    u64 played;
    double srcPos;
    float *buf;
    size_t bufCap, have;
    bool started, blanked;
    double opened;
};

static int64_t read_movie(void *user, uint64_t offset, void *buffer, size_t length) {
    Movie *m = user;
    if (offset >= m->size) return 0;
    if (length > m->size - offset) length = (size_t)(m->size - offset);
    if (fseeko(m->file, (off_t)offset, SEEK_SET) != 0) return -1;
    return (int64_t)fread(buffer, 1, length, m->file);
}

static double wall_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

static void movie_free(Movie *m) {
    if (m->video) vnvideo_close(m->video);
    if (m->file) fclose(m->file);
    free(m->buf);
    free(m);
}

void movie_stop(TH *t) {
    Movie *m = t->movie;
    if (!m) return;
    mixer_set_movie(t->mixer, NULL);
    t->movie = NULL;
    movie_free(m);
    if (t->wait == W_MOVIE) t->wait = W_NONE;
    t->sceneDirty = true;
}

bool movie_play(TH *t, const char *name) {
    movie_stop(t);
    char rel[1024], real[1024];
    snprintf(rel, sizeof rel, "%s/movie/%s", t->gameDir, name);
    if (th_path_resolve(rel, real, sizeof real) < 0) {
        th_log(t, "missing movie %s", name);
        return false;
    }
    Movie *m = th_alloc(sizeof *m);
    snprintf(m->name, sizeof m->name, "%s", name);
    m->file = fopen(real, "rb");
    if (!m->file || fseeko(m->file, 0, SEEK_END) != 0) {
        th_log(t, "can't open movie %s", name);
        movie_free(m);
        return false;
    }
    m->size = (u64)ftello(m->file);
    char err[256] = "";
    m->video = vnvideo_open(read_movie, m, m->size, 0, err, sizeof err);
    if (!m->video) {
        th_log(t, "can't play movie %s: %s", name, err);
        movie_free(m);
        return false;
    }
    vnvideo_get_info(m->video, &m->info);
    m->opened = wall_seconds();
    m->bufCap = 16384 * (size_t)(m->info.audio_channels > 0 ? m->info.audio_channels : 1);
    m->buf = th_alloc(sizeof(float) * m->bufCap);
    th_log(t, "movie %s: %dx%d at %.2f fps, sound %d Hz", name, m->info.width, m->info.height, m->info.fps,
           m->info.audio_rate);
    t->movie = m;
    mixer_set_movie(t->mixer, m);
    t->sceneDirty = true;
    return true;
}

void movie_mix(Movie *m, float *out, int frames, float vol, int rate) {
    int ch = m->info.audio_channels;
    if (m->info.audio_rate <= 0 || ch <= 0) return;
    double step = (double)m->info.audio_rate / rate;
    size_t need = (size_t)(m->srcPos + frames * step) + 2;
    if (need * (size_t)ch > m->bufCap) {
        m->bufCap = need * (size_t)ch;
        m->buf = th_realloc(m->buf, sizeof(float) * m->bufCap);
    }
    if (m->have < need) m->have += vnvideo_read_audio(m->video, m->buf + m->have * (size_t)ch, need - m->have);
    int i = 0;
    for (; i < frames && (size_t)m->srcPos + 1 < m->have; i++) {
        size_t k = (size_t)m->srcPos;
        float f = (float)(m->srcPos - (double)k);
        const float *a = m->buf + k * (size_t)ch, *b = a + ch;
        float l = a[0] + (b[0] - a[0]) * f;
        float r = ch > 1 ? a[1] + (b[1] - a[1]) * f : l;
        out[i * 2] += l * vol;
        out[i * 2 + 1] += r * vol;
        m->srcPos += step;
    }
    size_t used = (size_t)m->srcPos;
    if (used > m->have) used = m->have;
    memmove(m->buf, m->buf + used * (size_t)ch, sizeof(float) * (m->have - used) * (size_t)ch);
    m->have -= used;
    m->srcPos -= (double)used;
    __atomic_add_fetch(&m->played, (u64)i, __ATOMIC_RELAXED);
}

static double movie_time(TH *t, Movie *m) {
    if (m->info.audio_rate > 0) return (double)__atomic_load_n(&m->played, __ATOMIC_RELAXED) / t->audioRate;
    return m->clock;
}

void movie_update(TH *t, u32 elapsedMs) {
    Movie *m = t->movie;
    if (!m) return;
    if (t->click || t->keyPress[0x0d] || t->keyPress[0x20] || t->keyPress[0x1b]) {
        th_log(t, "movie %s skipped", m->name);
        movie_stop(t);
        return;
    }
    m->clock += elapsedMs / 1000.0;
    bool stalled = !m->started && wall_seconds() - m->opened > 5;
    if (vnvideo_ended(m->video, movie_time(t, m)) || stalled) {
        const char *e = vnvideo_error(m->video);
        th_log(t, "movie %s ended%s%s", m->name, e || stalled ? ": " : "", e ? e : stalled ? "no frame in 5 seconds" : "");
        movie_stop(t);
    }
}

bool movie_draw(TH *t) {
    Movie *m = t->movie;
    if (!m) return false;
    int changed = 0;
    const u8 *px = vnvideo_frame(m->video, movie_time(t, m), &changed);
    if (!px) {
        if (!m->blanked) {
            m->blanked = true;
            for (int i = 0; i < t->W * t->H; i++) t->screen[i] = 0xff000000u;
            t->serial++;
        }
        return true;
    }
    if (m->started && !changed) return true;
    m->started = true;
    int w = m->info.width, h = m->info.height;
    for (int y = 0; y < t->H; y++) {
        const u8 *row = px + (size_t)((long)y * h / t->H) * (size_t)w * 4;
        u32 *dst = t->screen + (size_t)y * (size_t)t->W;
        for (int x = 0; x < t->W; x++) {
            const u8 *p = row + (size_t)((long)x * w / t->W) * 4;
            dst[x] = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | 0xff000000u;
        }
    }
    t->serial++;
    return true;
}

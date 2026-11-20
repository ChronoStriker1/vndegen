#include "ikura.h"
#include "ik_casepath.h"
#include "vnvideo.h"

#include <sys/types.h>
#include <time.h>

struct Movie {
    FILE *file;
    uint64_t size;
    vnvideo *video;
    vnvideo_info info;
    char name[32];
    double clock, opened, srcPos;
    uint64_t played;
    float *buf;
    size_t bufCap, have;
    bool started, skippable;
    int x, y, w, h, newFrames;
    Image *frame;
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
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void movie_free(Movie *m) {
    if (m->video) vnvideo_close(m->video);
    if (m->file) fclose(m->file);
    image_free(m->frame);
    free(m->buf);
    free(m);
}

void movie_stop(IK *t) {
    Movie *m = t->movie;
    if (!m) return;
    if (m->clock > 0)
        ik_log(t, "movie %s: %d new frames in %.0f ms, %.2f a second, made at %.2f", m->name, m->newFrames, m->clock * 1000,
               m->newFrames / m->clock, m->info.fps);
    snd_set_movie(t, NULL);
    t->movie = NULL;
    movie_free(m);
    if (t->wait == W_MOVIE) t->wait = W_NONE;
    t->dirty = true;
}

bool movie_play(IK *t, const char *name, int x, int y, int w, int h, bool skippable) {
    movie_stop(t);
    char want[2048], real[2048];
    snprintf(want, sizeof want, "%s/%s%s", t->gameDir, name, strchr(name, '.') ? "" : ".MPG");
    if (ik_casepath_resolve(want, real, sizeof real) < 0) {
        ik_log(t, "missing movie %s", name);
        return false;
    }
    Movie *m = ik_alloc(sizeof *m);
    snprintf(m->name, sizeof m->name, "%s", name);
    m->file = fopen(real, "rb");
    if (!m->file || fseeko(m->file, 0, SEEK_END) != 0) {
        ik_log(t, "can't open movie %s", name);
        movie_free(m);
        return false;
    }
    m->size = (uint64_t)ftello(m->file);
    char err[256] = "";
    m->video = vnvideo_open(read_movie, m, m->size, 0, err, sizeof err);
    if (!m->video) {
        ik_log(t, "can't play movie %s: %s", name, err);
        movie_free(m);
        return false;
    }
    vnvideo_get_info(m->video, &m->info);
    m->x = x;
    m->y = y;
    m->w = w > 0 ? w : t->W;
    m->h = h > 0 ? h : t->H;
    m->skippable = skippable;
    m->opened = wall_seconds();
    m->bufCap = 16384 * (size_t)(m->info.audio_channels > 0 ? m->info.audio_channels : 1);
    m->buf = ik_alloc(sizeof(float) * m->bufCap);
    m->frame = image_new(m->w, m->h);
    for (int i = 0; i < m->w * m->h; i++) m->frame->px[i] = 0xff000000u;
    ik_log(t, "movie %s: %dx%d at %.2f fps, sound %d Hz", name, m->info.width, m->info.height, m->info.fps, m->info.audio_rate);
    t->movie = m;
    snd_set_movie(t, m);
    t->dirty = true;
    return true;
}

void movie_mix(Movie *m, float *out, int frames, float vol, int rate) {
    int ch = m->info.audio_channels;
    if (m->info.audio_rate <= 0 || ch <= 0) return;
    double step = (double)m->info.audio_rate / rate;
    size_t need = (size_t)(m->srcPos + frames * step) + 2;
    if (need * (size_t)ch > m->bufCap) {
        m->bufCap = need * (size_t)ch;
        m->buf = ik_realloc(m->buf, sizeof(float) * m->bufCap);
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
    __atomic_add_fetch(&m->played, (uint64_t)i, __ATOMIC_RELAXED);
}

static double movie_time(IK *t, Movie *m) {
    if (m->info.audio_rate > 0) return (double)__atomic_load_n(&m->played, __ATOMIC_RELAXED) / t->audioRate;
    return m->clock;
}

void movie_update(IK *t, double elapsedMs) {
    Movie *m = t->movie;
    if (!m) return;
    if (m->skippable && (t->click || t->keyPress[0x0d] || t->keyPress[0x20] || t->keyPress[0x1b])) {
        t->click = false;
        ik_log(t, "movie %s skipped", m->name);
        movie_stop(t);
        return;
    }
    m->clock += elapsedMs / 1000.0;
    bool stalled = !m->started && wall_seconds() - m->opened > 5;
    if (vnvideo_ended(m->video, movie_time(t, m)) || stalled) {
        const char *e = vnvideo_error(m->video);
        ik_log(t, "movie %s ended%s%s", m->name, e || stalled ? ": " : "", e ? e : stalled ? "no frame in 5 seconds" : "");
        movie_stop(t);
        return;
    }
    int changed = 0;
    const u8 *px = vnvideo_frame(m->video, movie_time(t, m), &changed);
    if (!px || (m->started && !changed)) return;
    if (changed) m->newFrames++;
    m->started = true;
    int sw = m->info.width, sh = m->info.height;
    for (int y = 0; y < m->h; y++) {
        const u8 *row = px + (size_t)((long)y * sh / m->h) * (size_t)sw * 4;
        u32 *dst = m->frame->px + (size_t)y * (size_t)m->w;
        for (int x = 0; x < m->w; x++) {
            const u8 *p = row + (size_t)((long)x * sw / m->w) * 4;
            dst[x] = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | 0xff000000u;
        }
    }
    t->dirty = true;
}

void movie_draw(IK *t) {
    Movie *m = t->movie;
    if (!m) return;
    static const int white[4] = {255, 255, 255, 255};
    Image screen = {.w = t->W, .h = t->H, .px = t->screen};
    image_blit(&screen, m->frame, 0, 0, m->w, m->h, m->x, m->y, white, 0, true);
}

#include "bsx.h"
#include "vnvideo.h"

#include <sys/types.h>

struct Movie {
    FILE *file;
    u64 size;
    vnvideo *video;
    vnvideo_info info;
    double clock;
    u64 played;
    double srcPos;
    float *buf;
    size_t bufCap, have;
    u32 lastNow;
    bool ended;
};

static int64_t read_movie(void *user, uint64_t offset, void *buffer, size_t length) {
    Movie *m = user;
    if (offset >= m->size) return 0;
    if (length > m->size - offset) length = (size_t)(m->size - offset);
    if (fseeko(m->file, (off_t)offset, SEEK_SET) != 0) return -1;
    return (int64_t)fread(buffer, 1, length, m->file);
}

static void movie_free(Movie *m) {
    if (!m) return;
    if (m->video) vnvideo_close(m->video);
    if (m->file) fclose(m->file);
    free(m->buf);
    free(m);
}

void bx_movie_stop(BX *b) {
    Movie *m = b->movie;
    if (!m) return;
    bx_mixer_lock(b->mixer, true);
    b->movie = NULL;
    bx_mixer_lock(b->mixer, false);
    movie_free(m);
    b->shownDirty = true;
}

bool bx_movie_play(BX *b, const char *name) {
    bx_movie_stop(b);
    char rel[2048], real[2048];
    snprintf(rel, sizeof rel, "%s/movie/%s.mpg", b->gameDir, name);
    if (bx_path_resolve(rel, real, sizeof real) <= 0) {
        bx_log(b, "missing movie %s", name);
        return false;
    }
    Movie *m = bx_alloc(sizeof *m);
    m->file = fopen(real, "rb");
    if (!m->file || fseeko(m->file, 0, SEEK_END) != 0) {
        movie_free(m);
        return false;
    }
    m->size = (u64)ftello(m->file);
    char err[256] = "";
    m->video = vnvideo_open(read_movie, m, m->size, 0, err, sizeof err);
    if (!m->video) {
        bx_log(b, "can't play movie %s: %s", name, err);
        movie_free(m);
        return false;
    }
    vnvideo_get_info(m->video, &m->info);
    m->bufCap = 16384 * (size_t)(m->info.audio_channels > 0 ? m->info.audio_channels : 1);
    m->buf = bx_alloc(sizeof(float) * m->bufCap);
    m->lastNow = b->now;
    bx_log(b, "movie %s: %dx%d at %.2f fps, sound %d Hz", name, m->info.width, m->info.height, m->info.fps, m->info.audio_rate);
    bx_mixer_lock(b->mixer, true);
    b->movie = m;
    bx_mixer_lock(b->mixer, false);
    return true;
}

void bx_movie_mix(BX *b, float *out, int frames, float vol) {
    Movie *m = b->movie;
    int ch = m ? m->info.audio_channels : 0;
    if (!m || m->info.audio_rate <= 0 || ch <= 0) return;
    double step = (double)m->info.audio_rate / b->audioRate;
    size_t need = (size_t)(m->srcPos + frames * step) + 2;
    if (need * (size_t)ch > m->bufCap) {
        m->bufCap = need * (size_t)ch;
        m->buf = bx_realloc(m->buf, sizeof(float) * m->bufCap);
    }
    if (m->have < need) m->have += vnvideo_read_audio(m->video, m->buf + m->have * (size_t)ch, need - m->have);
    int i = 0;
    for (; i < frames && (size_t)m->srcPos + 1 < m->have; i++) {
        size_t k = (size_t)m->srcPos;
        float f = (float)(m->srcPos - (double)k);
        const float *a = m->buf + k * (size_t)ch, *c = a + ch;
        float l = a[0] + (c[0] - a[0]) * f;
        float r = ch > 1 ? a[1] + (c[1] - a[1]) * f : l;
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

static double movie_time(BX *b, Movie *m) {
    if (m->info.audio_rate > 0) return (double)__atomic_load_n(&m->played, __ATOMIC_RELAXED) / b->audioRate;
    return m->clock;
}

bool bx_movie_update(BX *b) {
    Movie *m = b->movie;
    if (!m) return false;
    m->clock += (double)(b->now - m->lastNow) / 1000.0;
    m->lastNow = b->now;
    if (b->click || b->rclick || b->skipMode || vnvideo_ended(m->video, movie_time(b, m))) {
        b->click = b->rclick = false;
        const char *err = vnvideo_error(m->video);
        if (err) bx_log(b, "movie stopped: %s", err);
        bx_movie_stop(b);
        return false;
    }
    return true;
}

bool bx_movie_draw(BX *b, u32 *dst) {
    Movie *m = b->movie;
    if (!m) return false;
    int changed = 0;
    const u8 *px = vnvideo_frame(m->video, movie_time(b, m), &changed);
    size_t n = (size_t)b->W * (size_t)b->H;
    if (!px) {
        for (size_t i = 0; i < n; i++) dst[i] = 0xff000000u;
        return true;
    }
    int mw = m->info.width, mh = m->info.height;
    for (int y = 0; y < b->H; y++) {
        const u8 *row = px + (size_t)(y * mh / b->H) * (size_t)mw * 4;
        u32 *d = dst + (size_t)y * b->W;
        for (int x = 0; x < b->W; x++) {
            const u8 *p = row + (size_t)(x * mw / b->W) * 4;
            d[x] = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | 0xff000000u;
        }
    }
    return true;
}

#include "eagls.h"
#include "vnvideo.h"

#include <sys/types.h>

struct Movie {
    FILE *file;
    u64 size;
    vnvideo *video;
    vnvideo_info info;
    char name[64];
    int plane;
    bool playing, loop, ended;
    double clock;
    u64 played;
    double srcPos;
    float *buf;
    size_t bufCap, have;
    int newFrames;
    u32 lastNow;
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

static Movie *slot(EG *e, int ch) { return ch >= 0 && ch < 4 ? e->movies[ch] : NULL; }

void movie_stop(EG *e, int ch) {
    Movie *m = slot(e, ch);
    if (!m) return;
    if (m->clock > 0)
        eg_log(e, "movie %s: %d new frames in %.0f ms, %.2f a second, made at %.2f", m->name, m->newFrames, m->clock * 1000,
               m->newFrames / m->clock, m->info.fps);
    mixer_set_movie(e->mixer, NULL, ch);
    e->movies[ch] = NULL;
    for (int i = 0; i < 4; i++)
        if (e->movies[i] && e->movies[i]->info.audio_rate > 0) mixer_set_movie(e->mixer, e->movies[i], i);
    movie_free(m);
}

void movie_close_all(EG *e) {
    for (int i = 0; i < 4; i++) movie_stop(e, i);
}

bool movie_open(EG *e, int ch, const char *file, int plane) {
    if (ch < 0 || ch >= 4) return false;
    movie_stop(e, ch);
    char rel[1024], real[1024];
    snprintf(rel, sizeof rel, "%s/MOVIE/%s", e->dataDir, file);
    if (eg_path_resolve(rel, real, sizeof real) < 0) {
        eg_log(e, "missing movie %s", file);
        return false;
    }
    Movie *m = eg_alloc(sizeof *m);
    snprintf(m->name, sizeof m->name, "%s", file);
    m->plane = plane;
    m->file = fopen(real, "rb");
    if (!m->file || fseeko(m->file, 0, SEEK_END) != 0) {
        movie_free(m);
        return false;
    }
    m->size = (u64)ftello(m->file);
    char err[256] = "";
    m->video = vnvideo_open(read_movie, m, m->size, 0, err, sizeof err);
    if (!m->video) {
        eg_log(e, "can't play movie %s: %s", file, err);
        movie_free(m);
        return false;
    }
    vnvideo_get_info(m->video, &m->info);
    m->bufCap = 16384 * (size_t)(m->info.audio_channels > 0 ? m->info.audio_channels : 1);
    m->buf = eg_alloc(sizeof(float) * m->bufCap);
    eg_log(e, "movie %s: %dx%d at %.2f fps, sound %d Hz", file, m->info.width, m->info.height, m->info.fps, m->info.audio_rate);
    e->movies[ch] = m;
    if (plane >= 0 && plane < EG_PLANES) {
        image_free(e->plane[plane]);
        e->plane[plane] = image_new(m->info.width, m->info.height);
        for (size_t i = 0; i < (size_t)m->info.width * (size_t)m->info.height; i++) e->plane[plane]->px[i] = 0xff000000u;
        snprintf(e->planeName[plane], sizeof e->planeName[plane], "%s", file);
    }
    return true;
}

void movie_play(EG *e, int ch, bool loop) {
    Movie *m = slot(e, ch);
    if (!m) return;
    if (loop && !m->loop) {
        char err[256] = "";
        vnvideo_close(m->video);
        m->video = vnvideo_open(read_movie, m, m->size, 1, err, sizeof err);
        if (!m->video) {
            eg_log(e, "can't loop movie %s: %s", m->name, err);
            m->ended = true;
            return;
        }
        vnvideo_get_info(m->video, &m->info);
    }
    m->loop = loop;
    m->playing = true;
    m->lastNow = e->now;
    if (m->info.audio_rate > 0) mixer_set_movie(e->mixer, m, ch);
}

int movie_state(EG *e, int ch) {
    Movie *m = slot(e, ch);
    if (!m || m->ended) return 2;
    return m->playing ? 1 : 0;
}

void movie_size(EG *e, int ch, int *w, int *h) {
    Movie *m = slot(e, ch);
    *w = m ? m->info.width : 0;
    *h = m ? m->info.height : 0;
}

bool movie_any(EG *e) {
    for (int i = 0; i < 4; i++)
        if (e->movies[i] && e->movies[i]->playing) return true;
    return false;
}

void movie_mix(Movie *m, float *out, int frames, float vol, int rate) {
    int ch = m->info.audio_channels;
    if (m->info.audio_rate <= 0 || ch <= 0 || !m->playing) return;
    double step = (double)m->info.audio_rate / rate;
    size_t need = (size_t)(m->srcPos + frames * step) + 2;
    if (need * (size_t)ch > m->bufCap) {
        m->bufCap = need * (size_t)ch;
        m->buf = eg_realloc(m->buf, sizeof(float) * m->bufCap);
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

static double movie_time(EG *e, Movie *m) {
    if (m->info.audio_rate > 0) return (double)__atomic_load_n(&m->played, __ATOMIC_RELAXED) / e->audioRate;
    return m->clock;
}

void movie_update(EG *e) {
    for (int ch = 0; ch < 4; ch++) {
        Movie *m = e->movies[ch];
        if (!m || !m->playing || m->ended) continue;
        m->clock += (double)(e->now - m->lastNow) / 1000.0;
        m->lastNow = e->now;
        int changed = 0;
        const u8 *px = vnvideo_frame(m->video, movie_time(e, m), &changed);
        if (px && changed && m->plane >= 0 && m->plane < EG_PLANES && e->plane[m->plane]) {
            m->newFrames++;
            Image *im = e->plane[m->plane];
            int w = m->info.width < im->w ? m->info.width : im->w, h = m->info.height < im->h ? m->info.height : im->h;
            for (int y = 0; y < h; y++) {
                const u8 *row = px + (size_t)y * (size_t)m->info.width * 4;
                u32 *dst = im->px + (size_t)y * (size_t)im->w;
                for (int x = 0; x < w; x++) dst[x] = (u32)row[x * 4] | (u32)row[x * 4 + 1] << 8 | (u32)row[x * 4 + 2] << 16 | 0xff000000u;
            }
        }
        if (!m->loop && vnvideo_ended(m->video, movie_time(e, m))) {
            const char *err = vnvideo_error(m->video);
            eg_log(e, "movie %s ended%s%s", m->name, err ? ": " : "", err ? err : "");
            m->ended = true;
        }
    }
}

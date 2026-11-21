#include "anim.h"
#include "vnvideo.h"

#include <sys/types.h>

struct Movie {
    FILE *file;
    u64 size;
    vnvideo *video;
    vnvideo_info info;
    bool loop;
    double clock;
    u64 played;
    double srcPos;
    float *buf;
    size_t bufCap, have;
    u32 lastNow, fadeStart, fadeTime;
    char name[64];
};

static int64_t read_movie(void *user, uint64_t offset, void *buffer, size_t length) {
    struct Movie *m = user;
    if (offset >= m->size) return 0;
    if (length > m->size - offset) length = (size_t)(m->size - offset);
    if (fseeko(m->file, (off_t)offset, SEEK_SET) != 0) return -1;
    return (int64_t)fread(buffer, 1, length, m->file);
}

static void movie_free(struct Movie *m) {
    if (!m) return;
    if (m->video) vnvideo_close(m->video);
    if (m->file) fclose(m->file);
    free(m->buf);
    free(m);
}

static char *movie_path(AN *a, const char *name) {
    char *rel = an_fmt("%s/movie/%s", a->dir, name);
    char real[2048];
    int found = an_path_resolve(rel, real, sizeof real);
    free(rel);
    return found > 0 ? an_strdup(real) : NULL;
}

bool an_movie_exists(AN *a, const char *name) {
    char *p = movie_path(a, name);
    free(p);
    return p != NULL;
}

void an_movie_stop(AN *a) {
    struct Movie *m = a->movie;
    if (!m) return;
    an_snd_lock(a, true);
    a->movie = NULL;
    an_snd_lock(a, false);
    movie_free(m);
}

bool an_movie_play(AN *a, const char *name, bool loop, u32 fadeMs) {
    an_movie_stop(a);
    char *path = movie_path(a, name);
    if (!path) {
        an_log(a, "missing movie %s", name);
        return false;
    }
    struct Movie *m = an_alloc(sizeof *m);
    snprintf(m->name, sizeof m->name, "%s", name);
    m->file = fopen(path, "rb");
    free(path);
    if (!m->file || fseeko(m->file, 0, SEEK_END) != 0) {
        movie_free(m);
        return false;
    }
    m->size = (u64)ftello(m->file);
    char err[256] = "";
    m->video = vnvideo_open(read_movie, m, m->size, loop, err, sizeof err);
    if (!m->video) {
        an_log(a, "can't play movie %s: %s", name, err);
        movie_free(m);
        return false;
    }
    vnvideo_get_info(m->video, &m->info);
    m->loop = loop;
    m->bufCap = 16384 * (size_t)(m->info.audio_channels > 0 ? m->info.audio_channels : 1);
    m->buf = an_alloc(sizeof(float) * m->bufCap);
    m->lastNow = m->fadeStart = a->now;
    m->fadeTime = fadeMs;
    an_snd_lock(a, true);
    a->movie = m;
    an_snd_lock(a, false);
    return true;
}

bool an_movie_looping(AN *a) { return a->movie && a->movie->loop; }

void an_movie_mix(AN *a, float *out, int frames) {
    struct Movie *m = a->movie;
    int ch = m ? m->info.audio_channels : 0;
    if (!m || m->info.audio_rate <= 0 || ch <= 0) return;
    double step = (double)m->info.audio_rate / a->rate;
    size_t need = (size_t)(m->srcPos + frames * step) + 2;
    if (need * (size_t)ch > m->bufCap) {
        m->bufCap = need * (size_t)ch;
        m->buf = an_realloc(m->buf, sizeof(float) * m->bufCap);
    }
    if (m->have < need) m->have += vnvideo_read_audio(m->video, m->buf + m->have * (size_t)ch, need - m->have);
    int i = 0;
    for (; i < frames && (size_t)m->srcPos + 1 < m->have; i++) {
        size_t k = (size_t)m->srcPos;
        float f = (float)(m->srcPos - (double)k);
        const float *p = m->buf + k * (size_t)ch, *q = p + ch;
        float l = p[0] + (q[0] - p[0]) * f;
        float r = ch > 1 ? p[1] + (q[1] - p[1]) * f : l;
        out[i * 2] += l;
        out[i * 2 + 1] += r;
        m->srcPos += step;
    }
    size_t used = (size_t)m->srcPos;
    if (used > m->have) used = m->have;
    memmove(m->buf, m->buf + used * (size_t)ch, sizeof(float) * (m->have - used) * (size_t)ch);
    m->have -= used;
    m->srcPos -= (double)used;
    __atomic_add_fetch(&m->played, (u64)i, __ATOMIC_RELAXED);
}

static double movie_time(AN *a, struct Movie *m) {
    if (m->info.audio_rate > 0) return (double)__atomic_load_n(&m->played, __ATOMIC_RELAXED) / a->rate;
    return m->clock;
}

bool an_movie_update(AN *a) {
    struct Movie *m = a->movie;
    if (!m) return false;
    m->clock += (double)(a->now - m->lastNow) / 1000.0;
    m->lastNow = a->now;
    if (!m->loop && vnvideo_ended(m->video, movie_time(a, m))) {
        const char *err = vnvideo_error(m->video);
        if (err) an_log(a, "movie stopped: %s", err);
        an_movie_stop(a);
        return false;
    }
    return true;
}

void an_movie_draw(AN *a, u32 *dst) {
    struct Movie *m = a->movie;
    if (!m) return;
    int changed = 0;
    const u8 *px = vnvideo_frame(m->video, movie_time(a, m), &changed);
    if (!px) {
        if (!m->loop) an_gfx_fill(dst, a->W, a->H, 0xff000000u);
        return;
    }
    u32 alpha = 255;
    if (m->fadeTime && a->now - m->fadeStart < m->fadeTime) alpha = (a->now - m->fadeStart) * 255u / m->fadeTime;
    int mw = m->info.width, mh = m->info.height;
    u32 ia = 255 - alpha;
    for (int y = 0; y < a->H; y++) {
        const u8 *row = px + (size_t)(y * mh / a->H) * (size_t)mw * 4;
        u32 *d = dst + (size_t)y * (size_t)a->W;
        for (int x = 0; x < a->W; x++) {
            const u8 *p = row + (size_t)(x * mw / a->W) * 4;
            u32 s = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16;
            if (alpha >= 255) d[x] = s | 0xff000000u;
            else {
                u32 rb = (((s & 0xff00ffu) * alpha + (d[x] & 0xff00ffu) * ia) >> 8) & 0xff00ffu;
                u32 g = (((s & 0xff00u) * alpha + (d[x] & 0xff00u) * ia) >> 8) & 0xff00u;
                d[x] = rb | g | 0xff000000u;
            }
        }
    }
}

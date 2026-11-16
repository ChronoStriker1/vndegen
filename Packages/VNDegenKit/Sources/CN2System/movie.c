#include "n2.h"
#include "vnvideo.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

struct Movie {
    FILE *file;
    u8 *data;
    u64 size;
    vnvideo *video;
    vnvideo_info info;
    struct Channel *ch;
    bool skippable;
    double clock;
    double srcPos;
    float *buf;
    size_t bufCap;
    size_t have;
    bool started;
};

static int64_t read_movie(void *user, uint64_t offset, void *buffer, size_t length) {
    struct Movie *m = user;
    if (offset >= m->size) return 0;
    if (length > m->size - offset) length = (size_t)(m->size - offset);
    if (m->data) {
        memcpy(buffer, m->data + offset, length);
        return (int64_t)length;
    }
    if (fseeko(m->file, (off_t)offset, SEEK_SET) != 0) return -1;
    return (int64_t)fread(buffer, 1, length, m->file);
}

static void movie_free(N2 *n, struct Movie *m) {
    if (m->video) vnvideo_close(m->video);
    if (m->ch) mixer_stream_close(n, m->ch);
    if (m->file) fclose(m->file);
    free(m->data);
    free(m->buf);
    free(m);
}

void movie_stop(N2 *n) {
    if (!n->movie) return;
    movie_free(n, n->movie);
    n->movie = NULL;
    for (int i = 0; i < n->threadCount; i++)
        if (n->threads[i]->wait == WAIT_MOVIE) n->threads[i]->wait = WAIT_NONE;
    n->dirty = true;
}

bool movie_active(N2 *n) { return n->movie != NULL; }

bool movie_play(N2 *n, const char *path, bool skippable) {
    movie_stop(n);
    struct Movie *m = n2_alloc(sizeof *m);
    m->skippable = skippable;
    char *real = fs_loose_path(&n->fs, path);
    if (real) {
        m->file = fopen(real, "rb");
        free(real);
        if (m->file && fseeko(m->file, 0, SEEK_END) == 0) m->size = (u64)ftello(m->file);
    }
    if (!m->file) {
        size_t size = 0;
        m->data = fs_read(&n->fs, path, &size);
        m->size = size;
    }
    if (!m->file && !m->data) {
        n2_log(n, "missing movie %s", path);
        movie_free(n, m);
        return false;
    }
    char err[256] = "";
    m->video = vnvideo_open(read_movie, m, m->size, 0, err, sizeof err);
    if (!m->video) {
        n2_log(n, "can't play movie %s: %s", path, err);
        movie_free(n, m);
        return false;
    }
    vnvideo_get_info(m->video, &m->info);
    if (m->info.audio_rate > 0) m->ch = mixer_stream_open(n);
    n->movie = m;
    n2_log(n, "movie %s: %dx%d at %.2f fps, sound %d Hz", path, m->info.width, m->info.height, m->info.fps,
           m->info.audio_rate);
    return true;
}

static void feed_audio(N2 *n, struct Movie *m) {
    int want = n->audioRate / 4 - mixer_stream_queued(n, m->ch);
    if (want <= 0) return;
    int ch = m->info.audio_channels;
    double step = (double)m->info.audio_rate / n->audioRate;
    size_t need = (size_t)(m->srcPos + want * step) + 2;
    if (need * (size_t)ch > m->bufCap) {
        m->bufCap = need * (size_t)ch;
        m->buf = realloc(m->buf, sizeof(float) * m->bufCap);
    }
    if (m->have < need) m->have += vnvideo_read_audio(m->video, m->buf + m->have * (size_t)ch, need - m->have);
    float *out = n2_alloc(sizeof(float) * 2 * (size_t)want);
    int frames = 0;
    while (frames < want && (size_t)m->srcPos + 1 < m->have) {
        size_t i = (size_t)m->srcPos;
        float f = (float)(m->srcPos - (double)i);
        const float *a = m->buf + i * (size_t)ch, *b = a + ch;
        float l = a[0] + (b[0] - a[0]) * f;
        float r = ch > 1 ? a[1] + (b[1] - a[1]) * f : l;
        out[frames * 2] = l;
        out[frames * 2 + 1] = r;
        frames++;
        m->srcPos += step;
    }
    size_t used = (size_t)m->srcPos;
    if (used > m->have) used = m->have;
    memmove(m->buf, m->buf + used * (size_t)ch, sizeof(float) * (m->have - used) * (size_t)ch);
    m->have -= used;
    m->srcPos -= (double)used;
    mixer_stream_write(n, m->ch, out, frames, (float)sound_kind_volume(n, 0));
    free(out);
}

static double movie_time(N2 *n, struct Movie *m) {
    return m->ch ? (double)mixer_stream_played(n, m->ch) / n->audioRate : m->clock;
}

void movie_update(N2 *n, u32 elapsedMs, bool skip) {
    struct Movie *m = n->movie;
    if (!m) return;
    if (skip && m->skippable) {
        n2_log(n, "movie skipped");
        movie_stop(n);
        return;
    }
    if (m->ch) feed_audio(n, m);
    m->clock += elapsedMs / 1000.0;
    bool stalled = !m->started && m->clock > 5;
    if (vnvideo_ended(m->video, movie_time(n, m)) || stalled) {
        const char *e = vnvideo_error(m->video);
        n2_log(n, "movie ended%s%s", e ? ": " : "", e ? e : stalled ? ": no frame in 5 seconds" : "");
        movie_stop(n);
        return;
    }
    n->dirty = true;
}

bool movie_draw(N2 *n) {
    struct Movie *m = n->movie;
    if (!m) return false;
    int changed = 0;
    const u8 *px = vnvideo_frame(m->video, movie_time(n, m), &changed);
    if (!px) {
        for (int i = 0; i < N2_W * N2_H; i++) n->screen[i] = 0xff000000u;
        return true;
    }
    m->started = true;
    int w = m->info.width, h = m->info.height;
    for (int y = 0; y < N2_H; y++) {
        int sy = (int)((long)y * h / N2_H);
        const u8 *row = px + (size_t)sy * (size_t)w * 4;
        u32 *dst = n->screen + (size_t)y * N2_W;
        for (int x = 0; x < N2_W; x++) {
            const u8 *p = row + (size_t)((long)x * w / N2_W) * 4;
            dst[x] = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | 0xff000000u;
        }
    }
    return true;
}

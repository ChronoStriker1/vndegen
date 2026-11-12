#include "yr.h"
#include "vm.h"
#include "gfx.h"
#include "vnvideo.h"

struct YMovie {
    FILE *file;
    YFile mem;
    uint64_t size;
    vnvideo *video;
    vnvideo_info info;
    bool shown, playing, drawn;
    double clock;
    int newFrames;
    char name[256];
    int x, y, sx, sy, volume;
    float *buf;
    size_t bufCap;
    double resamplePos;
};

static int64_t read_movie(void *user, uint64_t offset, void *buffer, size_t length) {
    YMovie *m = user;
    if (offset >= m->size) return 0;
    if (length > m->size - offset) length = (size_t)(m->size - offset);
    if (m->file) {
        if (fseeko(m->file, (off_t)offset, SEEK_SET) != 0) return -1;
        return (int64_t)fread(buffer, 1, length, m->file);
    }
    memcpy(buffer, m->mem.data + offset, length);
    return (int64_t)length;
}

static void movie_free(YMovie *m) {
    if (!m) return;
    if (m->video) vnvideo_close(m->video);
    if (m->file) fclose(m->file);
    yr_file_free(&m->mem);
    free(m->buf);
    free(m);
}

void yr_movie_close(YR *y) {
    YMovie *m = y->movie;
    if (!m) return;
    if (m->clock > 0)
        yr_log(y, "movie %s: %d new frames in %.0f ms, %.2f a second, made at %.2f\n", m->name, m->newFrames, m->clock,
               m->newFrames * 1000.0 / m->clock, m->info.fps);
    yr_snd_lock(y, true);
    y->movie = NULL;
    yr_snd_lock(y, false);
    movie_free(m);
    if (y->gfx) y->gfx->dirty = true;
}

static bool open_source(YR *y, const char *name, FILE **file, YFile *mem, uint64_t *size) {
    char key[1024];
    snprintf(key, sizeof key, "%s", name);
    for (char *p = key; *p; p++) if (*p == '\\') *p = '/';
    char *path = yr_find_file(y->gameDir, key);
    if (path) {
        FILE *f = fopen(path, "rb");
        free(path);
        if (f && fseeko(f, 0, SEEK_END) == 0) {
            *size = (uint64_t)ftello(f);
            *file = f;
            return true;
        }
        if (f) fclose(f);
    }
    if (yr_fs_read(y, name, NULL, mem)) {
        *size = mem->size;
        return true;
    }
    return false;
}

static void load(YR *y, const char *name) {
    yr_movie_close(y);
    YMovie *m = yr_alloc(sizeof(YMovie));
    m->volume = 256;
    snprintf(m->name, sizeof m->name, "%s", name);
    if (!open_source(y, name, &m->file, &m->mem, &m->size)) {
        yr_log(y, "MOVIE: file not found: %s\n", name);
        movie_free(m);
        return;
    }
    char err[256] = "";
    m->video = vnvideo_open(read_movie, m, m->size, 0, err, sizeof err);
    if (!m->video) {
        yr_log(y, "MOVIE: cannot play %s: %s\n", name, err);
        movie_free(m);
        return;
    }
    vnvideo_get_info(m->video, &m->info);
    m->shown = true;
    yr_snd_lock(y, true);
    y->movie = m;
    yr_snd_lock(y, false);
}

bool yr_movie_draw(YR *y, bool *changedOut) {
    YMovie *m = y->movie;
    if (!m || !m->shown) return false;
    int changed = 0;
    const uint8_t *px = m->playing ? vnvideo_frame(m->video, m->clock / 1000.0, &changed) : NULL;
    if (changed) m->newFrames++;
    *changedOut = changed || !m->drawn || y->gfx->dirty;
    if (!*changedOut) return true;
    m->drawn = true;
    y->gfx->dirty = false;
    for (size_t i = 0; i < (size_t)y->width * (size_t)y->height; i++) y->screen[i] = 0xff000000u;
    if (!px) return true;
    int w = m->info.width, h = m->info.height;
    int dw = m->sx > 0 ? m->sx : y->width, dh = m->sy > 0 ? m->sy : y->height;
    for (int Y = 0; Y < dh; Y++) {
        int ty = m->y + Y;
        if (ty < 0 || ty >= y->height) continue;
        int sy = (int)((long)Y * h / dh);
        for (int X = 0; X < dw; X++) {
            int tx = m->x + X;
            if (tx < 0 || tx >= y->width) continue;
            const uint8_t *p = px + ((size_t)sy * (size_t)w + (size_t)((long)X * w / dw)) * 4;
            y->screen[(size_t)ty * (size_t)y->width + (size_t)tx] = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | 0xff000000u;
        }
    }
    return true;
}

void yr_movie_tick(YR *y, uint32_t ms) {
    YMovie *m = y->movie;
    if (!m || !m->playing) return;
    m->clock += ms;
    if (vnvideo_ended(m->video, m->clock / 1000.0)) m->playing = false;
}

void yr_movie_mix(YR *y, float *out, int frames) {
    YMovie *m = y->movie;
    if (m && m->playing && m->info.audio_rate > 0) {
        double step = (double)m->info.audio_rate / y->audioRate;
        size_t need = (size_t)(frames * step) + 2;
        int ch = m->info.audio_channels;
        if (need * (size_t)ch > m->bufCap) {
            m->bufCap = need * (size_t)ch;
            m->buf = yr_realloc(m->buf, sizeof(float) * m->bufCap);
        }
        size_t got = vnvideo_read_audio(m->video, m->buf, need - 1);
        float g = (float)(m->volume / 256.0);
        for (int f = 0; f < frames; f++) {
            size_t i = (size_t)(f * step);
            if (i >= got) break;
            float l = m->buf[i * (size_t)ch], r = ch > 1 ? m->buf[i * (size_t)ch + 1] : l;
            out[f * 2] += l * g;
            out[f * 2 + 1] += r * g;
        }
    }
}

int yr_cmd_movie(YCall *c, int id) {
    YR *y = c->y;
    if (id == C_MOVIEEND) {
        yr_movie_close(y);
        return YR_NEXT;
    }
    if (id == C_MOVIEINFO) {
        YRef out;
        if (!yc_ref(c, "LET", &out)) return YR_NEXT;
        YMovie *m = y->movie;
        YVal r = yv_int(0);
        char *codec = yc_str(c, "CODEC");
        if (codec) {
            FILE *f = NULL;
            YFile mem = { 0 };
            uint64_t size = 0;
            if (open_source(y, codec, &f, &mem, &size)) {
                u8 head[4] = { 0 };
                if (f) { fseeko(f, 0, SEEK_SET); if (fread(head, 1, 4, f) != 4) memset(head, 0, 4); fclose(f); }
                else if (mem.size >= 4) memcpy(head, mem.data, 4);
                yr_file_free(&mem);
                r = yv_int(head[0] == 0 && head[1] == 0 && head[2] == 1 ? 1 : 2);
            }
            free(codec);
        } else if (yc_int_or(c, "PLAY", 0)) r = yv_int(m && m->playing);
        else if (!m) r = yv_int(0);
        else if (yc_int_or(c, "X", 0)) r = yv_int(m->x);
        else if (yc_int_or(c, "Y", 0)) r = yv_int(m->y);
        else if (yc_int_or(c, "SX", 0)) r = yv_int(m->sx > 0 ? m->sx : y->width);
        else if (yc_int_or(c, "SY", 0)) r = yv_int(m->sy > 0 ? m->sy : y->height);
        else if (yc_int_or(c, "VOLUME", 0)) r = yv_int(m->volume);
        yr_ref_set(y, c->t, &out, &r);
        yv_free(&r);
        return YR_NEXT;
    }
    char *file = yc_str(c, "FILE");
    if (file) {
        load(y, file);
        free(file);
    }
    YMovie *m = y->movie;
    if (!m) return YR_NEXT;
    i64 v;
    if (yc_int(c, "X", &v)) m->x = (int)v;
    if (yc_int(c, "Y", &v)) m->y = (int)v;
    if (yc_int(c, "SX", &v)) m->sx = (int)v;
    if (yc_int(c, "SY", &v)) m->sy = (int)v;
    if (yc_int(c, "VOLUME", &v)) m->volume = v < 0 ? 0 : v > 256 ? 256 : (int)v;
    if (yc_int(c, "PLAY", &v)) m->playing = v != 0;
    y->gfx->dirty = true;
    return YR_NEXT;
}

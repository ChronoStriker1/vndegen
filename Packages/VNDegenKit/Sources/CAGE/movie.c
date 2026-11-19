#include "age.h"
#include "vnvideo.h"

#include <sys/types.h>

typedef struct Movie {
    u8 *mem;
    FILE *file;
    u64 base, size;
    vnvideo *video;
    vnvideo_info info;
    i32 slot;
    u32 startAt;
    double rate;
    double clock;
    u32 last;
    bool started, ended, modal, audio;
} Movie;

static int64_t read_movie(void *user, uint64_t offset, void *buffer, size_t length) {
    Movie *m = user;
    if (offset >= m->size) return 0;
    if (length > m->size - offset) length = (size_t)(m->size - offset);
    if (m->mem) { memcpy(buffer, m->mem + offset, length); return (int64_t)length; }
    if (fseeko(m->file, (off_t)(m->base + offset), SEEK_SET) != 0) return -1;
    return (int64_t)fread(buffer, 1, length, m->file);
}

static void movie_free(AG *g, Movie *m) {
    if (!m) return;
    if (m->video) snd_movie(g, m->video, false);
    if (m->video) vnvideo_close(m->video);
    if (m->file) fclose(m->file);
    free(m->mem);
    free(m);
}

static void strip_audio(u8 *d, size_t n) {
    size_t i = 0;
    while (i + 4 <= n) {
        if (d[i] != 0 || d[i + 1] != 0 || d[i + 2] != 1) { i++; continue; }
        u8 id = d[i + 3];
        if (id == 0xb9) break;
        if (id == 0xba) { i += (d[i + 4] & 0xc0) == 0x40 ? 14 + (d[i + 13] & 7) : 12; continue; }
        if (i + 6 > n) break;
        size_t len = (size_t)d[i + 4] << 8 | d[i + 5];
        if (id == 0xbb && i + 12 <= n) {
            d[i + 9] &= 0x03;
            for (size_t k = i + 12; k + 3 <= i + 6 + len && k + 3 <= n; k += 3)
                if (d[k] >= 0xc0 && d[k] <= 0xdf) d[k] = 0xbe;
        } else if (id >= 0xc0 && id <= 0xdf) d[i + 3] = 0xbe;
        i += 6 + len;
    }
}

void movie_stop(AG *g, i32 slot) {
    if (slot < 0 || slot >= AG_SURFACES) return;
    Movie *m = g->gfx.surf[slot].movie;
    g->gfx.surf[slot].movie = NULL;
    movie_free(g, m);
}

void movie_stop_all(AG *g) {
    for (int i = 0; i < AG_SURFACES; i++) if (g->gfx.surf[i].movie) movie_stop(g, i);
}

static bool open_entry(AG *g, Movie *m, u32 id) {
    const CatEntry *e = store_entry(&g->st, id);
    if (!e) return false;
    char real[2048];
    const char *dirs[2] = { "patch", "" };
    for (int k = 0; k < 2; k++) {
        char *p = *dirs[k] ? ag_fmt("%s/%s/%s", g->gameDir, dirs[k], e->name) : ag_fmt("%s/%s", g->gameDir, e->name);
        int r = ag_path_resolve(p, real, sizeof real);
        free(p);
        if (r > 0 && (m->file = fopen(real, "rb"))) {
            fseeko(m->file, 0, SEEK_END);
            m->size = (u64)ftello(m->file);
            m->base = 0;
            return true;
        }
    }
    Catalog *c = &g->st.cat[(i32)id >> 24];
    if (e->arc < 0 || e->arc >= c->arcCount) return false;
    char *p = ag_fmt("%s/%s", g->gameDir, c->arcs[e->arc]);
    int r = ag_path_resolve(p, real, sizeof real);
    free(p);
    if (r <= 0 || !(m->file = fopen(real, "rb"))) return false;
    m->base = e->offset;
    m->size = e->size;
    return true;
}

bool movie_play(AG *g, u32 id, i32 slot, i32 delay, i32 posMs, bool modal) {
    if (slot < 0 || slot >= AG_SURFACES) return false;
    movie_stop(g, slot);
    Movie *m = ag_alloc(sizeof *m);
    m->slot = slot;
    m->rate = 1;
    if (!open_entry(g, m, id)) { ag_log(g, "movie %#x can't be opened", id); movie_free(g, m); return false; }
    char err[256] = "";
    m->video = vnvideo_open(read_movie, m, m->size, 0, err, sizeof err);
    if (!m->video && m->size < (64u << 20)) {
        m->mem = ag_alloc((size_t)m->size + 4);
        if (fseeko(m->file, (off_t)m->base, SEEK_SET) == 0 && fread(m->mem, 1, (size_t)m->size, m->file) == m->size) {
            strip_audio(m->mem, (size_t)m->size);
            m->video = vnvideo_open(read_movie, m, m->size, 0, err, sizeof err);
            if (m->video) ag_log(g, "movie %#x plays without its sound", id);
        }
    }
    if (!m->video) { ag_log(g, "movie %#x can't be played: %s", id, err); movie_free(g, m); return false; }
    vnvideo_get_info(m->video, &m->info);
    m->startAt = g->now + (u32)(delay > 0 ? delay : 0);
    m->clock = posMs > 0 ? posMs / 1000.0 : 0;
    m->modal = modal;
    if (!gfx_surface(g, slot)) gfx_set_surface(g, slot, image_new(m->info.width, m->info.height), -1, true);
    if (m->info.audio_rate > 0) {
        m->audio = true;
        snd_movie(g, m->video, true);
    }
    g->gfx.surf[slot].movie = m;
    return true;
}

static void blit_frame(Image *im, const u8 *px, int w, int h) {
    if (im->w == w && im->h == h) { memcpy(im->px, px, (size_t)w * (size_t)h * 4); return; }
    for (int y = 0; y < im->h; y++) {
        int sy = y * h / im->h;
        for (int x = 0; x < im->w; x++) {
            int sx = x * w / im->w;
            memcpy(&im->px[(size_t)y * (size_t)im->w + (size_t)x], px + ((size_t)sy * (size_t)w + (size_t)sx) * 4, 4);
        }
    }
}

void movie_tick(AG *g) {
    for (int i = 0; i < AG_SURFACES; i++) {
        Movie *m = g->gfx.surf[i].movie;
        if (!m || m->ended) continue;
        if ((i32)(g->now - m->startAt) < 0) continue;
        if (!m->started) { m->started = true; m->last = g->now; }
        m->clock += (double)(g->now - m->last) / 1000.0 * m->rate;
        m->last = g->now;
        double t = m->audio ? vnvideo_audio_time(m->video) : m->clock;
        if (m->audio && t < m->clock - 0.25) t = m->clock;
        int changed = 0;
        const u8 *px = vnvideo_frame(m->video, t, &changed);
        Image *im = gfx_surface(g, i);
        if (px && changed && im) blit_frame(im, px, m->info.width, m->info.height);
        if (vnvideo_ended(m->video, t)) {
            m->ended = true;
            if (m->audio) { snd_movie(g, m->video, false); m->audio = false; }
        }
    }
}

bool movie_modal_done(AG *g) {
    for (int i = 0; i < AG_SURFACES; i++) {
        Movie *m = g->gfx.surf[i].movie;
        if (m && m->modal && !m->ended) return false;
    }
    return true;
}

void movie_modal_skip(AG *g) {
    for (int i = 0; i < AG_SURFACES; i++) {
        Movie *m = g->gfx.surf[i].movie;
        if (m && m->modal && !m->ended) {
            m->ended = true;
            if (m->audio) { snd_movie(g, m->video, false); m->audio = false; }
        }
    }
}

static int op_modal(AG *g) {
    if (!movie_play(g, (u32)arg_i(g, 1), arg_i(g, 2), 0, 0, true)) return OP_NEXT;
    g->wait = W_MOVIE;
    return OP_WAIT;
}

static int op_play(AG *g) {
    movie_play(g, (u32)arg_i(g, 1), arg_i(g, 2), arg_i(g, 4), 0, false);
    return OP_NEXT;
}

static int op_play_at(AG *g) {
    movie_play(g, (u32)arg_i(g, 1), arg_i(g, 2), arg_i(g, 4), arg_i(g, 5), false);
    return OP_NEXT;
}

static Movie *movie_at(AG *g, i32 slot) { return slot >= 0 && slot < AG_SURFACES ? g->gfx.surf[slot].movie : NULL; }

static int op_active(AG *g) {
    Movie *m = movie_at(g, arg_i(g, 2));
    arg_set_i(g, 1, m && !m->ended);
    return OP_NEXT;
}

static u32 mpeg_duration_ms(Movie *m) {
    u8 buf[65536];
    u64 first = ~0ull, last = 0;
    for (int pass = 0; pass < 2; pass++) {
        u64 at = pass == 0 ? 0 : (m->size > sizeof buf ? m->size - sizeof buf : 0);
        int64_t n = read_movie(m, at, buf, sizeof buf);
        for (int64_t i = 0; i + 14 <= n; i++) {
            if (buf[i] || buf[i + 1] || buf[i + 2] != 1 || buf[i + 3] != 0xba) continue;
            const u8 *p = buf + i + 4;
            if ((p[0] & 0xf0) != 0x20) continue;
            u64 scr = (u64)(p[0] >> 1 & 7) << 30 | (u64)p[1] << 22 | (u64)(p[2] >> 1) << 15 | (u64)p[3] << 7 | (u64)(p[4] >> 1);
            if (scr < first) first = scr;
            if (scr > last) last = scr;
        }
    }
    if (first == ~0ull || last <= first) return 0;
    double fps = m->info.fps > 0 ? m->info.fps : 30;
    return (u32)((double)(last - first) / 90.0 + 1000.0 / fps);
}

static int op_stop_time(AG *g) {
    Movie *m = movie_at(g, arg_i(g, 2));
    i32 slot = arg_i(g, 2);
    if (!m) { arg_set_i(g, 1, slot >= 0 && slot < AG_SURFACES && g->gfx.surf[slot].im ? 0 : -1); return OP_NEXT; }
    arg_set_i(g, 1, (i32)mpeg_duration_ms(m));
    return OP_NEXT;
}

static int op_rate(AG *g) {
    Movie *m = movie_at(g, arg_i(g, 1));
    if (m && arg_i(g, 2) > 0) m->rate = arg_i(g, 2) / 100.0;
    return OP_NEXT;
}

void movie_debug(AG *g, FILE *o) {
    for (int i = 0; i < AG_SURFACES; i++) {
        Movie *m = g->gfx.surf[i].movie;
        if (!m) continue;
        fprintf(o, "  movie slot %d started=%d ended=%d clock=%.2f audio=%d atime=%.2f rate=%.2f size=%dx%d err=%s\n", i, m->started, m->ended, m->clock, m->audio, m->audio ? vnvideo_audio_time(m->video) : 0, m->rate, m->info.width, m->info.height, vnvideo_error(m->video) ? vnvideo_error(m->video) : "-");
    }
}

void movie_register(void) {
    vm_register_draw(0x20f, op_modal);
    vm_register_draw(0x236, op_play);
    vm_register_draw(0x241, op_play_at);
    vm_register(0x23a, op_active);
    vm_register(0x23f, op_stop_time);
    vm_register(0x246, op_rate);
}

#include "sa.h"

#include <pthread.h>

#include "sa_vorbis_names.h"
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "third_party/stb_vorbis.c"
#undef L
#undef C
#undef R

typedef struct Voice Voice;

struct Voice {
    u8 *data;
    size_t size;
    stb_vorbis *ogg;
    const u8 *pcm;
    u32 pcmFrames, pcmPos;
    int pcmChannels, pcmBits;
    int rate;
    double step, pos;
    int16_t cur[2], next[2];
    bool primed, loop, done;
    float gain, target, delta;
    bool stopAtZero;
    int ch;
    int16_t buf[4096];
    int bufLen, bufPos;
    u32 played, loopStart, loopEnd;
    Voice *then;
};

enum { SLOTS_BGM = 2, SLOTS_SE = 16, SLOTS_VOICE = 2, SLOTS = SLOTS_BGM + SLOTS_SE + SLOTS_VOICE };

struct Mixer {
    int rate;
    Vfs *vfs;
    void (*log)(void *ctx, const char *msg);
    void *logCtx;
    pthread_mutex_t lock;
    Voice *v[SLOTS];
    float chanVol[CH_COUNT];
};

static int slot_index(int ch, int slot) {
    int base = ch == CH_BGM ? 0 : ch == CH_SE ? SLOTS_BGM : SLOTS_BGM + SLOTS_SE;
    int n = ch == CH_BGM ? SLOTS_BGM : ch == CH_SE ? SLOTS_SE : SLOTS_VOICE;
    return slot >= 0 && slot < n ? base + slot : -1;
}

Mixer *sa_mixer_new(int rate, Vfs *vfs) {
    Mixer *m = sa_alloc(sizeof *m);
    m->rate = rate > 0 ? rate : 44100;
    m->vfs = vfs;
    for (int i = 0; i < CH_COUNT; i++) m->chanVol[i] = 1;
    pthread_mutex_init(&m->lock, NULL);
    return m;
}

static void voice_free(Voice *v) {
    while (v) {
        Voice *n = v->then;
        if (v->ogg) stb_vorbis_close(v->ogg);
        free(v->data);
        free(v);
        v = n;
    }
}

void sa_mixer_free(Mixer *m) {
    if (!m) return;
    for (int i = 0; i < SLOTS; i++) voice_free(m->v[i]);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

static bool refill(Voice *v) {
    v->bufPos = 0;
    v->bufLen = 0;
    for (int attempt = 0; attempt < 2; attempt++) {
        if (v->ogg) v->bufLen = stb_vorbis_get_samples_short_interleaved(v->ogg, 2, v->buf, 4096);
        else {
            int n = 0;
            while (n < 2048 && v->pcmPos < v->pcmFrames) {
                const u8 *s = v->pcm + (size_t)v->pcmPos * (size_t)(v->pcmChannels * v->pcmBits / 8);
                int16_t l, r;
                if (v->pcmBits == 16) {
                    l = (int16_t)rd16(s);
                    r = v->pcmChannels > 1 ? (int16_t)rd16(s + 2) : l;
                } else {
                    l = (int16_t)((s[0] - 128) << 8);
                    r = v->pcmChannels > 1 ? (int16_t)((s[1] - 128) << 8) : l;
                }
                v->buf[n * 2] = l;
                v->buf[n * 2 + 1] = r;
                n++;
                v->pcmPos++;
            }
            v->bufLen = n;
        }
        if (v->bufLen > 0) return true;
        if (!v->loop) return false;
        if (v->ogg) stb_vorbis_seek_start(v->ogg);
        else v->pcmPos = 0;
    }
    return false;
}

static bool read_frame(Voice *v, int16_t out[2]) {
    if (v->loopEnd && v->played >= v->loopEnd) {
        if (v->ogg) stb_vorbis_seek(v->ogg, v->loopStart);
        else v->pcmPos = v->loopStart;
        v->bufLen = v->bufPos = 0;
        v->played = v->loopStart;
    }
    if (v->bufPos >= v->bufLen && !refill(v)) return false;
    v->played++;
    out[0] = v->buf[v->bufPos * 2];
    out[1] = v->buf[v->bufPos * 2 + 1];
    v->bufPos++;
    return true;
}

void sa_mixer_mix(Mixer *m, float *out, int frames, bool paused) {
    memset(out, 0, sizeof(float) * (size_t)frames * 2);
    if (!m || paused) return;
    pthread_mutex_lock(&m->lock);
    for (int s = 0; s < SLOTS; s++) {
        int i = 0;
        while (i < frames) {
            Voice *v = m->v[s];
            if (!v) break;
            if (v->done) {
                if (v->then && !v->stopAtZero) {
                    m->v[s] = v->then;
                    v->then = NULL;
                    voice_free(v);
                    continue;
                }
                break;
            }
            if (!v->primed) {
                if (!read_frame(v, v->cur) || !read_frame(v, v->next)) {
                    v->done = true;
                    continue;
                }
                v->primed = true;
            }
            float vol = m->chanVol[v->ch];
            for (; i < frames; i++) {
                if (v->gain != v->target) {
                    v->gain += v->delta;
                    if ((v->delta > 0 && v->gain >= v->target) || (v->delta < 0 && v->gain <= v->target) || v->delta == 0) v->gain = v->target;
                    if (v->gain <= 0 && v->stopAtZero) {
                        v->done = true;
                        break;
                    }
                }
                float f = (float)v->pos, g = v->gain * vol / 32768.0f;
                out[i * 2] += ((float)v->cur[0] + ((float)v->next[0] - (float)v->cur[0]) * f) * g;
                out[i * 2 + 1] += ((float)v->cur[1] + ((float)v->next[1] - (float)v->cur[1]) * f) * g;
                v->pos += v->step;
                while (v->pos >= 1.0) {
                    v->pos -= 1.0;
                    v->cur[0] = v->next[0];
                    v->cur[1] = v->next[1];
                    if (!read_frame(v, v->next)) {
                        v->done = true;
                        break;
                    }
                }
                if (v->done) {
                    i++;
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&m->lock);
    for (int i = 0; i < frames * 2; i++) out[i] = out[i] > 1 ? 1 : out[i] < -1 ? -1 : out[i];
}

static bool parse_wav(Voice *v) {
    const u8 *d = v->data;
    size_t n = v->size;
    if (n < 12 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) return false;
    size_t p = 12;
    bool fmt = false;
    while (p + 8 <= n) {
        u32 len = rd32(d + p + 4);
        if (!memcmp(d + p, "fmt ", 4) && len >= 16 && p + 8 + len <= n) {
            if (rd16(d + p + 8) != 1) return false;
            v->pcmChannels = rd16(d + p + 10);
            v->rate = (int)rd32(d + p + 12);
            v->pcmBits = rd16(d + p + 22);
            fmt = v->pcmChannels >= 1 && v->pcmChannels <= 2 && (v->pcmBits == 8 || v->pcmBits == 16);
        } else if (!memcmp(d + p, "data", 4) && fmt) {
            size_t avail = n - (p + 8);
            if (len > avail) len = (u32)avail;
            v->pcm = d + p + 8;
            v->pcmFrames = len / (u32)(v->pcmChannels * v->pcmBits / 8);
            return true;
        }
        p += 8 + len + (len & 1);
    }
    return false;
}

static void mlog(Mixer *m, const char *what, const char *name) {
    if (!m->log) return;
    char *t = sa_fmt("%s %s", what, name);
    m->log(m->logCtx, t);
    free(t);
}

static Voice *open_voice(Mixer *m, int ch, const char *name, bool loop) {
    size_t n;
    u8 *d = sa_vfs_read(m->vfs, name, &n);
    if (!d) {
        mlog(m, "missing sound", name);
        return NULL;
    }
    size_t skip = 0;
    if (n >= 0x40 && !memcmp(d, "AoiOgg", 6)) {
        for (size_t k = 6; k + 4 <= 0x40; k++)
            if (!memcmp(d + k, "OggS", 4)) {
                skip = k;
                break;
            }
    }
    Voice *v = sa_alloc(sizeof *v);
    v->data = d;
    v->size = n;
    int err = 0;
    if (n >= skip + 4 && !memcmp(d + skip, "OggS", 4)) {
        v->ogg = stb_vorbis_open_memory(d + skip, (int)(n - skip), &err, NULL);
        if (v->ogg) v->rate = (int)stb_vorbis_get_info(v->ogg).sample_rate;
    }
    if (!v->ogg && !parse_wav(v)) {
        mlog(m, "can't decode sound", name);
        voice_free(v);
        return NULL;
    }
    v->loop = loop;
    v->step = (double)v->rate / (double)m->rate;
    v->gain = 1;
    v->target = 1;
    v->ch = ch;
    return v;
}

static void fade_out(Mixer *m, Voice *v, int ms) {
    if (!v) return;
    if (ms <= 0) {
        v->done = true;
        v->stopAtZero = true;
        return;
    }
    v->target = 0;
    v->stopAtZero = true;
    v->delta = -v->gain / ((float)ms * (float)m->rate / 1000.0f);
}

static void install(Mixer *m, int ch, int slot, Voice *v) {
    int s = slot_index(ch, slot);
    if (s < 0) {
        voice_free(v);
        return;
    }
    pthread_mutex_lock(&m->lock);
    Voice *gone = m->v[s];
    m->v[s] = v;
    pthread_mutex_unlock(&m->lock);
    voice_free(gone);
}

void sa_play(Mixer *m, int ch, int slot, const char *name, bool loop) {
    if (!m || !name || !*name) return;
    Voice *v = open_voice(m, ch, name, loop);
    if (v) install(m, ch, slot, v);
}

void sa_play_loop(Mixer *m, int ch, int slot, const char *name, double loopTo, double loopFrom) {
    if (!m || !name || !*name) return;
    Voice *v = open_voice(m, ch, name, true);
    if (!v) return;
    if (loopFrom > loopTo && loopFrom > 0) {
        v->loopStart = (u32)(loopTo * v->rate + 0.5);
        v->loopEnd = (u32)(loopFrom * v->rate + 0.5);
    }
    install(m, ch, slot, v);
}

void sa_stop(Mixer *m, int ch, int slot, int fadeMs) {
    int s = slot_index(ch, slot);
    if (!m || s < 0) return;
    pthread_mutex_lock(&m->lock);
    fade_out(m, m->v[s], fadeMs);
    pthread_mutex_unlock(&m->lock);
}

void sa_set_volume(Mixer *m, int ch, float v) {
    if (!m || ch < 0 || ch >= CH_COUNT) return;
    pthread_mutex_lock(&m->lock);
    m->chanVol[ch] = v < 0 ? 0 : v > 1 ? 1 : v;
    pthread_mutex_unlock(&m->lock);
}

bool sa_playing(Mixer *m, int ch, int slot) {
    int s = slot_index(ch, slot);
    if (!m || s < 0) return false;
    pthread_mutex_lock(&m->lock);
    Voice *v = m->v[s];
    bool r = v && (!v->done || v->then) && !v->stopAtZero;
    pthread_mutex_unlock(&m->lock);
    return r;
}

void sa_mixer_set_log(Mixer *m, void (*log)(void *ctx, const char *msg), void *ctx) {
    m->log = log;
    m->logCtx = ctx;
}

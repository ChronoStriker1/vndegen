#include "anim.h"

#include <pthread.h>

#include "an_vorbis_names.h"
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "third_party/stb_vorbis.c"
#undef L
#undef C
#undef R

typedef struct Voice {
    u8 *data;
    stb_vorbis *ogg;
    double step, pos;
    int16_t cur[2], next[2];
    bool primed, loop, done;
    float gain, target, delta;
    bool stopAtZero;
    int16_t buf[4096];
    int bufLen, bufPos;
} Voice;

struct Mixer {
    pthread_mutex_t lock;
    Voice *v[CH_COUNT];
    Voice *fading[CH_COUNT];
    float vol[CH_COUNT];
};

bool an_snd_init(AN *a) {
    struct Mixer *m = an_alloc(sizeof *m);
    pthread_mutex_init(&m->lock, NULL);
    for (int i = 0; i < CH_COUNT; i++) m->vol[i] = i == CH_BGM ? 0.6f : 0.85f;
    a->mix = m;
    return true;
}

static void voice_free(Voice *v) {
    if (!v) return;
    if (v->ogg) stb_vorbis_close(v->ogg);
    free(v->data);
    free(v);
}

void an_snd_free(AN *a) {
    struct Mixer *m = a->mix;
    if (!m) return;
    for (int i = 0; i < CH_COUNT; i++) {
        voice_free(m->v[i]);
        voice_free(m->fading[i]);
    }
    pthread_mutex_destroy(&m->lock);
    free(m);
    a->mix = NULL;
}

static bool refill(Voice *v) {
    v->bufPos = 0;
    v->bufLen = 0;
    for (int attempt = 0; attempt < 2; attempt++) {
        v->bufLen = stb_vorbis_get_samples_short_interleaved(v->ogg, 2, v->buf, 4096);
        if (v->bufLen > 0) return true;
        if (!v->loop) return false;
        stb_vorbis_seek_start(v->ogg);
    }
    return false;
}

static bool read_frame(Voice *v, int16_t out[2]) {
    if (v->bufPos >= v->bufLen && !refill(v)) return false;
    out[0] = v->buf[v->bufPos * 2];
    out[1] = v->buf[v->bufPos * 2 + 1];
    v->bufPos++;
    return true;
}

static void mix_voice(Voice *v, float vol, float *out, int frames) {
    if (!v || v->done) return;
    if (!v->primed) {
        if (!read_frame(v, v->cur) || !read_frame(v, v->next)) {
            v->done = true;
            return;
        }
        v->primed = true;
    }
    for (int i = 0; i < frames; i++) {
        if (v->gain != v->target) {
            v->gain += v->delta;
            if ((v->delta > 0 && v->gain >= v->target) || (v->delta < 0 && v->gain <= v->target) || v->delta == 0) v->gain = v->target;
            if (v->gain <= 0 && v->stopAtZero) {
                v->done = true;
                return;
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
                return;
            }
        }
    }
}

void an_snd_mix(AN *a, float *out, int frames) {
    memset(out, 0, sizeof(float) * (size_t)frames * 2);
    struct Mixer *m = a->mix;
    if (!m || a->paused) return;
    pthread_mutex_lock(&m->lock);
    for (int c = 0; c < CH_COUNT; c++) {
        mix_voice(m->v[c], m->vol[c], out, frames);
        mix_voice(m->fading[c], m->vol[c], out, frames);
    }
    pthread_mutex_unlock(&m->lock);
    for (int i = 0; i < frames * 2; i++) out[i] = out[i] > 1 ? 1 : out[i] < -1 ? -1 : out[i];
}

static u8 *sound_data(AN *a, const char *name, bool fromVoice, size_t *size) {
    u8 *d = NULL;
    if (fromVoice) {
        char key[96];
        snprintf(key, sizeof key, "%s.eog", name);
        const PackEntry *e = an_pack_find(&a->voice, key);
        if (e) {
            d = an_pack_read(&a->voice, e);
            *size = e->size;
        }
    }
    if (!d) {
        char *path = an_fmt("%s/bgm/%s.eog", a->dir, name);
        d = an_read_file(path, size);
        free(path);
    }
    if (!d) {
        const PackEntry *e = NULL;
        char key[96];
        snprintf(key, sizeof key, "%s.eog", name);
        if (!fromVoice) e = an_pack_find(&a->voice, key);
        if (e) {
            d = an_pack_read(&a->voice, e);
            *size = e->size;
        }
    }
    return d;
}

void an_snd_play(AN *a, int channel, const char *name, bool loop, bool fromVoice) {
    struct Mixer *m = a->mix;
    if (!m || channel < 0 || channel >= CH_COUNT || !name || !*name) return;
    size_t n = 0;
    u8 *d = sound_data(a, name, fromVoice, &n);
    if (!d) {
        an_log(a, "missing sound %s", name);
        return;
    }
    size_t skip = n >= 8 && !memcmp(d, "CRM", 4) ? 8 : 0;
    int err = 0;
    stb_vorbis *ogg = n > skip + 4 && !memcmp(d + skip, "OggS", 4) ? stb_vorbis_open_memory(d + skip, (int)(n - skip), &err, NULL) : NULL;
    if (!ogg) {
        an_log(a, "can't decode sound %s", name);
        free(d);
        return;
    }
    Voice *v = an_alloc(sizeof *v);
    v->data = d;
    v->ogg = ogg;
    v->loop = loop;
    v->step = (double)stb_vorbis_get_info(ogg).sample_rate / (double)a->rate;
    v->gain = v->target = 1;
    pthread_mutex_lock(&m->lock);
    Voice *gone = m->fading[channel];
    m->fading[channel] = NULL;
    Voice *old = m->v[channel];
    m->v[channel] = v;
    pthread_mutex_unlock(&m->lock);
    voice_free(gone);
    voice_free(old);
}

void an_snd_stop(AN *a, int channel) {
    struct Mixer *m = a->mix;
    if (!m || channel < 0 || channel >= CH_COUNT) return;
    pthread_mutex_lock(&m->lock);
    Voice *v = m->v[channel];
    if (v) {
        v->target = 0;
        v->stopAtZero = true;
        v->delta = -v->gain / (0.3f * (float)a->rate);
    }
    Voice *gone = m->fading[channel];
    m->fading[channel] = v;
    m->v[channel] = NULL;
    pthread_mutex_unlock(&m->lock);
    voice_free(gone);
}

bool an_snd_playing(AN *a, int channel) {
    struct Mixer *m = a->mix;
    if (!m || channel < 0 || channel >= CH_COUNT) return false;
    pthread_mutex_lock(&m->lock);
    bool r = m->v[channel] && !m->v[channel]->done;
    pthread_mutex_unlock(&m->lock);
    return r;
}

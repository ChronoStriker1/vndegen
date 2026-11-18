#include "th.h"

#include <math.h>
#include <pthread.h>

#include "stb_vorbis_names.h"
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "third_party/stb_vorbis.c"
#undef L
#undef C
#undef R

typedef struct Voice {
    int16_t *pcm;
    size_t frames;
    size_t pos;
    bool loop;
    float gain, target, step, scale;
    bool stopAtZero;
    bool done;
} Voice;

struct Mixer {
    int rate;
    pthread_mutex_t lock;
    Voice *v[CH_COUNT];
    float chanVol[CH_COUNT];
    Movie *movie;
};

Mixer *mixer_new(int rate) {
    Mixer *m = th_alloc(sizeof *m);
    m->rate = rate;
    for (int i = 0; i < CH_COUNT; i++) m->chanVol[i] = 1;
    pthread_mutex_init(&m->lock, NULL);
    return m;
}

static void voice_free(Voice *v) {
    if (!v) return;
    free(v->pcm);
    free(v);
}

void mixer_free(Mixer *m) {
    if (!m) return;
    for (int i = 0; i < CH_COUNT; i++) voice_free(m->v[i]);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

void mixer_mix(Mixer *m, float *out, int frames, bool paused) {
    memset(out, 0, sizeof(float) * (size_t)frames * 2);
    if (!m || paused) return;
    pthread_mutex_lock(&m->lock);
    for (int c = 0; c < CH_COUNT; c++) {
        Voice *v = m->v[c];
        if (!v || v->done || !v->frames) continue;
        float vol = m->chanVol[c];
        for (int i = 0; i < frames; i++) {
            if (v->pos >= v->frames) {
                if (v->loop) v->pos = 0;
                else {
                    v->done = true;
                    break;
                }
            }
            if (v->gain != v->target) {
                v->gain += v->step;
                if ((v->step > 0 && v->gain >= v->target) || (v->step < 0 && v->gain <= v->target) || v->step == 0) v->gain = v->target;
                if (v->gain <= 0 && v->stopAtZero) {
                    v->done = true;
                    break;
                }
            }
            float g = v->gain * v->scale * vol / 32768.0f;
            out[i * 2] += v->pcm[v->pos * 2] * g;
            out[i * 2 + 1] += v->pcm[v->pos * 2 + 1] * g;
            v->pos++;
        }
    }
    if (m->movie) movie_mix(m->movie, out, frames, m->chanVol[CH_BGM], m->rate);
    pthread_mutex_unlock(&m->lock);
    for (int i = 0; i < frames * 2; i++) out[i] = out[i] > 1 ? 1 : out[i] < -1 ? -1 : out[i];
}

void mixer_set_movie(Mixer *m, Movie *movie) {
    pthread_mutex_lock(&m->lock);
    m->movie = movie;
    pthread_mutex_unlock(&m->lock);
}

static int16_t *resample(const int16_t *src, size_t frames, int channels, int srcRate, int rate, size_t *outFrames) {
    if (!frames || channels < 1) return NULL;
    size_t n = srcRate == rate ? frames : (size_t)((double)frames * rate / srcRate);
    int16_t *out = th_alloc(n * 4 + 4);
    for (size_t i = 0; i < n; i++) {
        double p = srcRate == rate ? (double)i : (double)i * srcRate / rate;
        size_t k = (size_t)p;
        double f = p - (double)k;
        size_t k2 = k + 1 < frames ? k + 1 : k;
        for (int c = 0; c < 2; c++) {
            int sc = channels == 1 ? 0 : c;
            double a = src[k * channels + sc], b = src[k2 * channels + sc];
            out[i * 2 + c] = (int16_t)lround(a + (b - a) * f);
        }
    }
    *outFrames = n;
    return out;
}

static Voice *load_voice(TH *t, int kind, const char *name) {
    size_t size;
    u8 *d = store_load(&t->store, kind, name, &size);
    if (!d) {
        th_log(t, "missing sound %s", name);
        return NULL;
    }
    Voice *v = th_alloc(sizeof *v);
    if (size >= 9 && d[0] == 0x44 && rd32(d + 5) <= size - 9) {
        u32 len = rd32(d + 5);
        v->pcm = resample((const int16_t *)(d + 9), len / 4, 2, 44100, t->audioRate, &v->frames);
    } else if (size >= 4 && !memcmp(d, "OggS", 4)) {
        int ch = 0, rate = 0;
        short *pcm = NULL;
        int n = stb_vorbis_decode_memory(d, (int)size, &ch, &rate, &pcm);
        if (n > 0 && pcm) v->pcm = resample(pcm, (size_t)n, ch, rate, t->audioRate, &v->frames);
        free(pcm);
    } else {
        th_log(t, "unknown sound format %s", name);
    }
    free(d);
    if (!v->pcm) {
        free(v);
        return NULL;
    }
    return v;
}

static void install(TH *t, int ch, Voice *v) {
    Voice *old;
    pthread_mutex_lock(&t->mixer->lock);
    old = t->mixer->v[ch];
    t->mixer->v[ch] = v;
    pthread_mutex_unlock(&t->mixer->lock);
    voice_free(old);
}

static float channel_volume(TH *t, int ch) {
    int master = t->sysVol[VOL_MASTER];
    int part = ch == CH_BGM ? t->sysVol[VOL_BGM] : (ch == CH_VOICE || ch == CH_SYSVOICE || (ch >= CH_SEHV && ch <= CH_SEHV4)) ? t->sysVol[VOL_VOICE] : t->sysVol[VOL_SE];
    return (float)master / 100.0f * (float)part / 100.0f;
}

void snd_play(TH *t, int ch, int kind, const char *name, bool loop, int fadeMs) {
    Voice *v = load_voice(t, kind, name);
    if (!v) {
        snd_stop(t, ch, 0);
        return;
    }
    v->loop = loop;
    v->target = 1;
    v->scale = kind == PAC_VOICE ? voice_scale(t, name) : 1.0f;
    if (fadeMs > 0) {
        v->gain = 0;
        v->step = 1.0f / ((float)fadeMs * (float)t->audioRate / 1000.0f);
    } else {
        v->gain = 1;
    }
    pthread_mutex_lock(&t->mixer->lock);
    t->mixer->chanVol[ch] = channel_volume(t, ch);
    pthread_mutex_unlock(&t->mixer->lock);
    install(t, ch, v);
}

void snd_stop(TH *t, int ch, int fadeMs) {
    if (fadeMs <= 0) {
        install(t, ch, NULL);
        return;
    }
    pthread_mutex_lock(&t->mixer->lock);
    Voice *v = t->mixer->v[ch];
    if (v) {
        v->target = 0;
        v->stopAtZero = true;
        v->step = -(v->gain > 0 ? v->gain : 1) / ((float)fadeMs * (float)t->audioRate / 1000.0f);
    }
    pthread_mutex_unlock(&t->mixer->lock);
}

bool snd_playing(TH *t, int ch) {
    pthread_mutex_lock(&t->mixer->lock);
    Voice *v = t->mixer->v[ch];
    bool p = v && !v->done;
    pthread_mutex_unlock(&t->mixer->lock);
    return p;
}

void snd_update(TH *t) {
    pthread_mutex_lock(&t->mixer->lock);
    for (int c = 0; c < CH_COUNT; c++) t->mixer->chanVol[c] = channel_volume(t, c);
    pthread_mutex_unlock(&t->mixer->lock);
}

void snd_close(TH *t) {
    mixer_free(t->mixer);
    t->mixer = NULL;
}

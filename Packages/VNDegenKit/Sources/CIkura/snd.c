#include "ikura.h"

#include <math.h>
#include <pthread.h>

#include "ik_vorbis_names.h"
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "third_party/stb_vorbis.c"

typedef struct Sound {
    u8 *data;
    const int16_t *pcm;
    long frames;
    stb_vorbis *ogg;
    int channels, rate;
    long at;
    short buf[2048];
    int bufLen, bufPos;
    double step, pos;
    int16_t cur[2], next[2];
    bool primed, loop, done;
    float gain, delta;
} Sound;

struct Mixer {
    pthread_mutex_t lock;
    Movie *movie;
    Sound *ch[IK_CHANNELS];
    float vol[IK_CHANNELS];
};

static float volume_gain(int v) {
    if (v <= 0) return 0;
    if (v >= 255) return 1;
    return powf(10.0f, -1000.0f * log2f(255.0f / (float)v) / 2000.0f);
}

void snd_volume(IK *t, int ch, int v) {
    float g = volume_gain(v);
    pthread_mutex_lock(&t->mixer->lock);
    if (ch == CH_SE)
        for (int i = CH_SE; i < IK_CHANNELS; i++) t->mixer->vol[i] = g;
    else t->mixer->vol[ch] = g;
    pthread_mutex_unlock(&t->mixer->lock);
}

static void sound_free(Sound *s) {
    if (!s) return;
    if (s->ogg) stb_vorbis_close(s->ogg);
    free(s->data);
    free(s);
}

static bool pull(Sound *s, int16_t out[2]) {
    for (int tries = 0; tries < 2; tries++) {
        if (s->ogg) {
            if (s->bufPos >= s->bufLen) {
                int n = stb_vorbis_get_samples_short_interleaved(s->ogg, s->channels, s->buf, 2048);
                s->bufLen = n * s->channels;
                s->bufPos = 0;
            }
            if (s->bufPos < s->bufLen) {
                out[0] = s->buf[s->bufPos];
                out[1] = s->channels > 1 ? s->buf[s->bufPos + 1] : out[0];
                s->bufPos += s->channels;
                return true;
            }
            if (!s->loop || !stb_vorbis_seek_start(s->ogg)) return false;
        } else {
            if (s->at < s->frames) {
                out[0] = s->pcm[s->at * s->channels];
                out[1] = s->channels > 1 ? s->pcm[s->at * s->channels + 1] : out[0];
                s->at++;
                return true;
            }
            if (!s->loop || !s->frames) return false;
            s->at = 0;
        }
    }
    return false;
}

static Sound *sound_open(IK *t, u8 *data, size_t size, const char *name) {
    Sound *s = ik_alloc(sizeof *s);
    s->data = data;
    s->gain = 1;
    if (size >= 12 && !memcmp(data, "RIFF", 4) && !memcmp(data + 8, "WAVE", 4)) {
        size_t p = 12;
        int bits = 0;
        while (p + 8 <= size) {
            u32 len = rd32(data + p + 4);
            if (!memcmp(data + p, "fmt ", 4) && len >= 16 && p + 8 + 16 <= size) {
                s->channels = rd16(data + p + 10);
                s->rate = (int)rd32(data + p + 12);
                bits = rd16(data + p + 22);
            } else if (!memcmp(data + p, "data", 4)) {
                if (len > size - p - 8) len = (u32)(size - p - 8);
                s->pcm = (const int16_t *)(data + p + 8);
                if (s->channels > 0) s->frames = (long)(len / 2 / (u32)s->channels);
                break;
            }
            p += 8 + len + (len & 1);
        }
        if (bits != 16 || !s->pcm || s->channels < 1 || s->rate <= 0) {
            ik_log(t, "can't decode sound %s", name);
            sound_free(s);
            return NULL;
        }
    } else {
        int err;
        s->ogg = stb_vorbis_open_memory(data, (int)size, &err, NULL);
        if (!s->ogg) {
            ik_log(t, "can't decode sound %s", name);
            sound_free(s);
            return NULL;
        }
        stb_vorbis_info info = stb_vorbis_get_info(s->ogg);
        s->channels = info.channels;
        s->rate = (int)info.sample_rate;
    }
    s->step = (double)s->rate / (double)t->audioRate;
    return s;
}

void snd_init(IK *t) {
    t->mixer = ik_alloc(sizeof *t->mixer);
    pthread_mutex_init(&t->mixer->lock, NULL);
    for (int i = 0; i < IK_CHANNELS; i++) t->mixer->vol[i] = volume_gain(IK_DEFAULT_VOLUME);
}

void snd_close(IK *t) {
    if (!t->mixer) return;
    for (int i = 0; i < IK_CHANNELS; i++) sound_free(t->mixer->ch[i]);
    pthread_mutex_destroy(&t->mixer->lock);
    free(t->mixer);
    t->mixer = NULL;
}

static void install(IK *t, int ch, Sound *s) {
    pthread_mutex_lock(&t->mixer->lock);
    Sound *old = t->mixer->ch[ch];
    t->mixer->ch[ch] = s;
    pthread_mutex_unlock(&t->mixer->lock);
    sound_free(old);
}

static void play(IK *t, int ch, int kind, const char *name, bool loop) {
    size_t n;
    u8 *d = store_load(&t->store, kind, name, &n);
    if (!d) {
        ik_log(t, "missing sound %s", name);
        install(t, ch, NULL);
        return;
    }
    Sound *s = sound_open(t, d, n, name);
    if (s) s->loop = loop;
    if (t->trace) ik_log(t, "sound %d %s%s", ch, name, loop ? " loop" : "");
    install(t, ch, s);
}

void snd_se_load(IK *t, int slot, const char *name) {
    if (slot < 0 || slot >= IK_SLOTS) return;
    snprintf(t->slots[slot].name, sizeof t->slots[slot].name, "%s", name);
}

void snd_se_play(IK *t, int slot, bool loop) {
    if (slot < 0 || slot >= IK_SLOTS || !t->slots[slot].name[0]) return;
    play(t, CH_SE + slot, AR_SE, t->slots[slot].name, loop);
}

void snd_se_stop(IK *t, int slot) {
    if (slot >= 0 && slot < IK_SLOTS) install(t, CH_SE + slot, NULL);
}

void snd_voice(IK *t, const char *name) { play(t, CH_VOICE, AR_VOICE, name, false); }

void snd_voice_stop(IK *t) { install(t, CH_VOICE, NULL); }

bool snd_voice_playing(IK *t) {
    pthread_mutex_lock(&t->mixer->lock);
    Sound *s = t->mixer->ch[CH_VOICE];
    bool on = s && !s->done;
    pthread_mutex_unlock(&t->mixer->lock);
    return on;
}

void snd_bgm(IK *t, const char *name, bool loop) {
    bool named = !strcmp(store_name(&t->store, AR_BGM, t->bgmName), store_name(&t->store, AR_BGM, name));
    pthread_mutex_lock(&t->mixer->lock);
    Sound *s = t->mixer->ch[CH_BGM];
    bool same = s && !s->done && s->delta >= 0 && named;
    pthread_mutex_unlock(&t->mixer->lock);
    if (same) return;
    snprintf(t->bgmName, sizeof t->bgmName, "%s", name);
    t->bgmLoop = loop;
    play(t, CH_BGM, AR_BGM, name, loop);
}

void snd_bgm_fade(IK *t, int ms) {
    t->bgmName[0] = 0;
    if (ms <= 0) {
        install(t, CH_BGM, NULL);
        return;
    }
    pthread_mutex_lock(&t->mixer->lock);
    Sound *s = t->mixer->ch[CH_BGM];
    if (s) s->delta = -1.0f / ((float)ms / 1000.0f * (float)t->audioRate);
    pthread_mutex_unlock(&t->mixer->lock);
}

void snd_set_movie(IK *t, Movie *m) {
    pthread_mutex_lock(&t->mixer->lock);
    t->mixer->movie = m;
    pthread_mutex_unlock(&t->mixer->lock);
}

void snd_mix(IK *t, float *out, int frames) {
    memset(out, 0, sizeof(float) * (size_t)frames * 2);
    if (!t->mixer || t->paused) return;
    pthread_mutex_lock(&t->mixer->lock);
    for (int c = 0; c < IK_CHANNELS; c++) {
        Sound *s = t->mixer->ch[c];
        if (!s || s->done) continue;
        if (!s->primed) {
            if (!pull(s, s->cur) || !pull(s, s->next)) {
                s->done = true;
                continue;
            }
            s->primed = true;
        }
        for (int i = 0; i < frames && !s->done; i++) {
            if (s->delta) {
                s->gain += s->delta;
                if (s->gain <= 0) {
                    s->done = true;
                    break;
                }
            }
            float f = (float)s->pos, g = s->gain * t->mixer->vol[c] / 32768.0f;
            out[i * 2] += ((float)s->cur[0] + ((float)s->next[0] - (float)s->cur[0]) * f) * g;
            out[i * 2 + 1] += ((float)s->cur[1] + ((float)s->next[1] - (float)s->cur[1]) * f) * g;
            s->pos += s->step;
            while (s->pos >= 1.0) {
                s->pos -= 1.0;
                s->cur[0] = s->next[0];
                s->cur[1] = s->next[1];
                if (!pull(s, s->next)) {
                    s->done = true;
                    break;
                }
            }
        }
    }
    if (t->mixer->movie) movie_mix(t->mixer->movie, out, frames, t->mixer->vol[CH_BGM], t->audioRate);
    pthread_mutex_unlock(&t->mixer->lock);
    for (int i = 0; i < frames * 2; i++) out[i] = out[i] > 1 ? 1 : out[i] < -1 ? -1 : out[i];
}

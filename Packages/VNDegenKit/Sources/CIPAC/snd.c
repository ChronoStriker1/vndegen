#include "ipac.h"

#include <math.h>
#include <pthread.h>

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#define DRWAV_API static
#define DRWAV_PRIVATE static
#include "third_party/dr_wav.h"

typedef struct Voice {
    u8 *data;
    drwav wav;
    int channels;
    double step, pos;
    int16_t cur[2], next[2];
    bool primed;
    bool loop;
    float gain, target, delta;
    bool stopAtZero;
    bool done;
} Voice;

struct Mixer {
    int rate;
    pthread_mutex_t lock;
    Voice *v[CH_COUNT];
    float chanVol[CH_COUNT];
};

Mixer *mixer_new(int rate) {
    Mixer *m = ip_alloc(sizeof *m);
    m->rate = rate;
    for (int i = 0; i < CH_COUNT; i++) m->chanVol[i] = 1;
    pthread_mutex_init(&m->lock, NULL);
    return m;
}

static void voice_free(Voice *v) {
    if (!v) return;
    drwav_uninit(&v->wav);
    free(v->data);
    free(v);
}

void mixer_free(Mixer *m) {
    if (!m) return;
    for (int i = 0; i < CH_COUNT; i++) voice_free(m->v[i]);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

static bool read_frame(Voice *v, int16_t out[2]) {
    int16_t f[8];
    if (drwav_read_pcm_frames_s16(&v->wav, 1, f) != 1) {
        if (!v->loop || drwav_seek_to_pcm_frame(&v->wav, 0) != DRWAV_TRUE || drwav_read_pcm_frames_s16(&v->wav, 1, f) != 1) return false;
    }
    out[0] = f[0];
    out[1] = v->channels > 1 ? f[1] : f[0];
    return true;
}

void mixer_mix(Mixer *m, float *out, int frames, bool paused) {
    memset(out, 0, sizeof(float) * (size_t)frames * 2);
    if (!m || paused) return;
    pthread_mutex_lock(&m->lock);
    for (int c = 0; c < CH_COUNT; c++) {
        Voice *v = m->v[c];
        if (!v || v->done) continue;
        if (!v->primed) {
            if (!read_frame(v, v->cur) || !read_frame(v, v->next)) {
                v->done = true;
                continue;
            }
            v->primed = true;
        }
        float vol = m->chanVol[c];
        for (int i = 0; i < frames; i++) {
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
            if (v->done) break;
        }
    }
    pthread_mutex_unlock(&m->lock);
    for (int i = 0; i < frames * 2; i++) out[i] = out[i] > 1 ? 1 : out[i] < -1 ? -1 : out[i];
}

static u8 *wst_to_wav(const u8 *d, size_t n, size_t *outSize) {
    if (n < 40 || memcmp(d, "WST2", 4)) return NULL;
    size_t dataLen = n - 40;
    size_t total = 8 + 4 + 8 + 0x32 + 8 + dataLen;
    u8 *w = ip_alloc(total);
    u8 *p = w;
    memcpy(p, "RIFF", 4);
    wr32(p + 4, (u32)(total - 8));
    memcpy(p + 8, "WAVEfmt ", 8);
    wr32(p + 16, 0x32);
    p += 20;
    wr16(p, 2);
    wr16(p + 2, 2);
    wr32(p + 4, 44100);
    wr32(p + 8, 44100);
    wr16(p + 12, 0x800);
    wr16(p + 14, 4);
    wr16(p + 16, 0x20);
    wr16(p + 18, 0x7f4);
    wr16(p + 20, 7);
    memcpy(p + 22, d + 12, 0x1c);
    p += 0x32;
    memcpy(p, "data", 4);
    wr32(p + 4, (u32)dataLen);
    memcpy(p + 8, d + 40, dataLen);
    *outSize = total;
    return w;
}

static Voice *voice_open(IP *t, u8 *data, size_t size, const char *what) {
    Voice *v = ip_alloc(sizeof *v);
    v->data = data;
    if (!drwav_init_memory(&v->wav, data, size, NULL)) {
        char *u = ip_sjis_to_utf8(what);
        ip_log(t, "can't decode sound %s", u);
        free(u);
        free(data);
        free(v);
        return NULL;
    }
    v->channels = (int)v->wav.channels;
    v->step = (double)v->wav.sampleRate / (double)t->audioRate;
    v->gain = v->target = 1;
    return v;
}

static void install(IP *t, int ch, Voice *v) {
    pthread_mutex_lock(&t->mixer->lock);
    Voice *old = t->mixer->v[ch];
    t->mixer->v[ch] = v;
    pthread_mutex_unlock(&t->mixer->lock);
    voice_free(old);
}

static float fade_delta(IP *t, int speed) {
    if (speed <= 0) return 0;
    float frames = 256.0f / (float)speed / 60.0f * (float)t->audioRate;
    return 1.0f / (frames > 1 ? frames : 1);
}

void snd_volumes(IP *t) {
    if (!t->mixer) return;
    pthread_mutex_lock(&t->mixer->lock);
    t->mixer->chanVol[CH_BGM] = t->bgmOn ? (float)t->volBgm / 255.0f : 0;
    t->mixer->chanVol[CH_SE] = t->seOn ? (float)t->volSe / 255.0f : 0;
    t->mixer->chanVol[CH_SYS] = t->seOn ? (float)t->volSe / 255.0f : 0;
    t->mixer->chanVol[CH_VOICE] = t->voiceOn ? (float)t->volVoice / 255.0f : 0;
    pthread_mutex_unlock(&t->mixer->lock);
}

void snd_bgm(IP *t, int number, int speed) {
    int track = number >= 100 ? number - 100 : number;
    if (track == t->bgmNumber && t->mixer->v[CH_BGM] && !t->mixer->v[CH_BGM]->stopAtZero) return;
    char name[32];
    snprintf(name, sizeof name, "BGM%02d", track);
    size_t n;
    u8 *d = store_load(&t->store, PK_BGM, name, ".WST", &n);
    if (!d) {
        ip_log(t, "missing %s", name);
        snd_bgm_stop(t, 0);
        return;
    }
    size_t wn;
    u8 *w = wst_to_wav(d, n, &wn);
    if (w) free(d);
    else {
        w = d;
        wn = n;
    }
    Voice *v = voice_open(t, w, wn, name);
    if (!v) return;
    v->loop = true;
    if (speed > 0) {
        v->gain = 0;
        v->delta = fade_delta(t, speed);
    }
    t->bgmNumber = track;
    install(t, CH_BGM, v);
}

void snd_bgm_stop(IP *t, int speed) {
    t->bgmNumber = 0;
    if (speed <= 0) {
        install(t, CH_BGM, NULL);
        return;
    }
    pthread_mutex_lock(&t->mixer->lock);
    Voice *v = t->mixer->v[CH_BGM];
    if (v) {
        v->target = 0;
        v->stopAtZero = true;
        v->delta = -fade_delta(t, speed);
    }
    pthread_mutex_unlock(&t->mixer->lock);
}

static void play_wav(IP *t, int ch, int kind, const char *name) {
    size_t n;
    u8 *d = store_load(&t->store, kind, name, ".wav", &n);
    if (!d) {
        char *u = ip_sjis_to_utf8(name);
        ip_log(t, "missing sound %s", u);
        free(u);
        install(t, ch, NULL);
        return;
    }
    Voice *v = voice_open(t, d, n, name);
    install(t, ch, v);
}

void snd_se(IP *t, int number) {
    char name[16];
    snprintf(name, sizeof name, "SE%02d", number);
    play_wav(t, CH_SE, PK_SE, name);
}

void snd_se_stop(IP *t) { install(t, CH_SE, NULL); }

void snd_sys(IP *t, int number) {
    char name[16];
    snprintf(name, sizeof name, "SE%02d", number);
    play_wav(t, CH_SYS, PK_SE, name);
}

void snd_voice(IP *t, const char *name) { play_wav(t, CH_VOICE, PK_VOICE, name); }

void snd_voice_stop(IP *t) { install(t, CH_VOICE, NULL); }

bool snd_voice_playing(IP *t) {
    pthread_mutex_lock(&t->mixer->lock);
    Voice *v = t->mixer->v[CH_VOICE];
    bool p = v && !v->done;
    pthread_mutex_unlock(&t->mixer->lock);
    return p;
}

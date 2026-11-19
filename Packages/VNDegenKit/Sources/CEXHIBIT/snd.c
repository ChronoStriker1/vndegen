#include "ex.h"

#include <pthread.h>

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#define DRWAV_API static
#define DRWAV_PRIVATE static
#include "third_party/dr_wav.h"

#include "ex_vorbis_names.h"
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "third_party/stb_vorbis.c"
#undef L
#undef C
#undef R

typedef struct Voice {
    FILE *file;
    u8 *data;
    drwav wav;
    bool isWav;
    stb_vorbis *ogg;
    int channels, rate;
    double step, pos;
    int16_t cur[2], next[2];
    bool primed, loop, done;
    float gain, target, delta;
    bool stopAtZero;
    int16_t buf[4096];
    int bufLen, bufPos;
} Voice;

struct Mixer {
    int rate;
    pthread_mutex_t lock;
    Voice *v[EX_CHANNELS];
    float chanVol[EX_CHANNELS];
};

Mixer *mixer_new(int rate) {
    Mixer *m = ex_alloc(sizeof *m);
    m->rate = rate;
    for (int i = 0; i < EX_CHANNELS; i++) m->chanVol[i] = 1;
    pthread_mutex_init(&m->lock, NULL);
    return m;
}

static void voice_free(Voice *v) {
    if (!v) return;
    if (v->isWav) drwav_uninit(&v->wav);
    if (v->ogg) stb_vorbis_close(v->ogg);
    if (v->file) fclose(v->file);
    free(v->data);
    free(v);
}

void mixer_free(Mixer *m) {
    if (!m) return;
    for (int i = 0; i < EX_CHANNELS; i++) voice_free(m->v[i]);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

static bool refill(Voice *v) {
    v->bufPos = 0;
    v->bufLen = 0;
    for (int attempt = 0; attempt < 2; attempt++) {
        if (v->isWav) {
            int16_t tmp[4096];
            drwav_uint64 want = 2048 / (drwav_uint64)(v->channels > 0 ? v->channels : 1);
            if (want > 2048) want = 2048;
            drwav_uint64 got = drwav_read_pcm_frames_s16(&v->wav, want, tmp);
            for (drwav_uint64 i = 0; i < got; i++) {
                v->buf[i * 2] = tmp[i * (drwav_uint64)v->channels];
                v->buf[i * 2 + 1] = v->channels > 1 ? tmp[i * (drwav_uint64)v->channels + 1] : tmp[i * (drwav_uint64)v->channels];
            }
            v->bufLen = (int)got;
        } else if (v->ogg) {
            int got = stb_vorbis_get_samples_short_interleaved(v->ogg, 2, v->buf, 4096);
            v->bufLen = got;
        }
        if (v->bufLen > 0) return true;
        if (!v->loop) return false;
        if (v->isWav) drwav_seek_to_pcm_frame(&v->wav, 0);
        else if (v->ogg) stb_vorbis_seek_start(v->ogg);
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

void mixer_mix(Mixer *m, float *out, int frames, bool paused) {
    memset(out, 0, sizeof(float) * (size_t)frames * 2);
    if (!m || paused) return;
    pthread_mutex_lock(&m->lock);
    for (int c = 0; c < EX_CHANNELS; c++) {
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

static size_t file_read(void *user, void *buf, size_t n) { return fread(buf, 1, n, (FILE *)user); }

static drwav_bool32 file_seek(void *user, int offset, drwav_seek_origin origin) {
    int whence = origin == DRWAV_SEEK_SET ? SEEK_SET : origin == DRWAV_SEEK_END ? SEEK_END : SEEK_CUR;
    return fseek((FILE *)user, offset, whence) == 0;
}

static drwav_bool32 file_tell(void *user, drwav_int64 *pos) {
    long p = ftell((FILE *)user);
    if (p < 0) return DRWAV_FALSE;
    *pos = p;
    return DRWAV_TRUE;
}

static Voice *voice_wav_file(EX *e, const char *path) {
    char real[2048];
    if (ex_path_resolve(path, real, sizeof real) < 0) return NULL;
    Voice *v = ex_alloc(sizeof *v);
    v->file = fopen(real, "rb");
    if (!v->file || !drwav_init(&v->wav, file_read, file_seek, file_tell, v->file, NULL)) {
        ex_log(e, "can't open sound %s", path);
        if (v->file) fclose(v->file);
        free(v);
        return NULL;
    }
    v->isWav = true;
    v->channels = v->wav.channels;
    v->rate = (int)v->wav.sampleRate;
    return v;
}

static Voice *voice_memory(EX *e, u8 *data, size_t size, const char *name) {
    Voice *v = ex_alloc(sizeof *v);
    v->data = data;
    if (size >= 4 && !memcmp(data, "OggS", 4)) {
        int err = 0;
        v->ogg = stb_vorbis_open_memory(data, (int)size, &err, NULL);
        if (!v->ogg) {
            ex_log(e, "can't decode sound %s", name);
            voice_free(v);
            return NULL;
        }
        stb_vorbis_info info = stb_vorbis_get_info(v->ogg);
        v->channels = 2;
        v->rate = (int)info.sample_rate;
        return v;
    }
    if (!drwav_init_memory(&v->wav, data, size, NULL)) {
        ex_log(e, "can't decode sound %s", name);
        voice_free(v);
        return NULL;
    }
    v->isWav = true;
    v->channels = v->wav.channels;
    v->rate = (int)v->wav.sampleRate;
    return v;
}

static void ogg_unlock(u8 *d, size_t n) {
    static const int rot[8] = {2, 15, 7, 11, 1, 15, 23, 3};
    if (n < 512 || !memcmp(d, "OggS", 4)) return;
    u32 w0 = (u32)d[0] | (u32)d[1] << 8 | (u32)d[2] << 16 | (u32)d[3] << 24;
    u32 key = (w0 << 2 | w0 >> 30) ^ 0x5367674fu;
    for (int i = 0; i < 128; i++) {
        u8 *p = d + 4 * i;
        u32 w = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
        int r = rot[i & 7];
        w = (w << r | w >> (32 - r)) ^ key;
        p[0] = (u8)w;
        p[1] = (u8)(w >> 8);
        p[2] = (u8)(w >> 16);
        p[3] = (u8)(w >> 24);
    }
}

static Voice *voice_open(EX *e, const char *rel) {
    char *path = ex_game_path(e, rel);
    Voice *v = NULL;
    if (ex_file_exists(path)) {
        size_t n = strlen(path);
        if (n > 4 && ex_ieq(path + n - 4, ".wav")) v = voice_wav_file(e, path);
        else {
            size_t sz;
            u8 *d = ex_read_file(path, &sz);
            if (d) {
                ogg_unlock(d, sz);
                v = voice_memory(e, d, sz, rel);
            }
        }
    }
    free(path);
    return v;
}

static float channel_volume(EX *e, int ch) {
    if (e->muted) return 0;
    int v = ch == EX_CH_MUSIC ? e->volBgm : ch == EX_CH_VOICE || ch == EX_CH_ENV ? e->volVoice : e->volSe;
    return (float)v / 100.0f * (float)e->volMaster / 100.0f;
}

void snd_apply_volumes(EX *e) {
    Mixer *m = e->mixer;
    if (!m) return;
    pthread_mutex_lock(&m->lock);
    for (int ch = 0; ch < EX_CHANNELS; ch++) m->chanVol[ch] = channel_volume(e, ch);
    pthread_mutex_unlock(&m->lock);
}

static void fade_out(EX *e, Voice *v, int ms) {
    if (!v) return;
    if (ms <= 0) {
        v->done = true;
        return;
    }
    v->target = 0;
    v->stopAtZero = true;
    v->delta = -v->gain / ((float)ms * (float)e->audioRate / 1000.0f);
}

static void start(EX *e, Voice *v, int ch, bool loop, int fadeMs) {
    if (!e->mixer || ch < 0 || ch >= EX_CHANNELS) {
        voice_free(v);
        return;
    }
    v->loop = loop;
    v->step = (double)v->rate / (double)e->audioRate;
    v->gain = fadeMs > 0 ? 0 : 1;
    v->target = 1;
    v->delta = fadeMs > 0 ? 1.0f / ((float)fadeMs * (float)e->audioRate / 1000.0f) : 0;
    Mixer *m = e->mixer;
    pthread_mutex_lock(&m->lock);
    Voice *old = m->v[ch];
    m->v[ch] = v;
    m->chanVol[ch] = channel_volume(e, ch);
    pthread_mutex_unlock(&m->lock);
    voice_free(old);
}

void snd_play_file(EX *e, const char *rel, int ch) {
    Voice *v = voice_open(e, rel);
    if (!v) {
        ex_log(e, "missing sound %s", rel);
        return;
    }
    start(e, v, ch, false, 0);
}

void snd_play_id(EX *e, int id, int ch, bool loop, int fadeMs) {
    char rel[64];
    snprintf(rel, sizeof rel, "res/s/e/%03d.wav", id);
    Voice *v = voice_open(e, rel);
    if (!v) {
        snprintf(rel, sizeof rel, "res/s/v/%02d/%05d.v", id / 1000, id);
        v = voice_open(e, rel);
    }
    if (!v) {
        ex_log(e, "missing sound %d", id);
        return;
    }
    start(e, v, ch, loop, fadeMs);
}

void snd_play_voice(EX *e, int id) {
    char rel[64];
    snprintf(rel, sizeof rel, "res/s/v/%02d/%05d.v", id / 1000, id);
    char *path = ex_game_path(e, rel);
    bool ok = ex_file_exists(path);
    free(path);
    if (!ok) return;
    Voice *v = voice_open(e, rel);
    if (v) start(e, v, EX_CH_VOICE, false, 0);
}

void snd_play_env(EX *e, int id, int volume) {
    (void)volume;
    char rel[64];
    snprintf(rel, sizeof rel, "res/s/v/%02d/%05d.v", id / 1000, id);
    Voice *v = voice_open(e, rel);
    if (!v) {
        ex_log(e, "missing voice %d", id);
        return;
    }
    start(e, v, EX_CH_ENV, true, 0);
}

void snd_play_music(EX *e, int track, int fadeMs) {
    char rel[64];
    snprintf(rel, sizeof rel, "res/s/m/m%02d.ogg", track);
    Voice *v = voice_open(e, rel);
    if (!v) {
        ex_log(e, "missing music %d", track);
        return;
    }
    start(e, v, 0, true, fadeMs);
}

void snd_stop(EX *e, int ch, int fadeMs) {
    if (!e->mixer || ch < 0 || ch >= EX_CHANNELS) return;
    pthread_mutex_lock(&e->mixer->lock);
    fade_out(e, e->mixer->v[ch], fadeMs);
    pthread_mutex_unlock(&e->mixer->lock);
}

bool snd_playing(EX *e, int ch) {
    if (!e->mixer || ch < 0 || ch >= EX_CHANNELS) return false;
    pthread_mutex_lock(&e->mixer->lock);
    bool r = e->mixer->v[ch] && !e->mixer->v[ch]->done;
    pthread_mutex_unlock(&e->mixer->lock);
    return r;
}

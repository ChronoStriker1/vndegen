#include "bsx.h"

#include <pthread.h>

#include "bx_vorbis_names.h"
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "third_party/stb_vorbis.c"
#undef L
#undef C
#undef R

typedef struct Voice {
    u8 *data;
    stb_vorbis *ogg;
    int rate;
    double step, pos;
    int16_t cur[2], next[2];
    bool primed, loop, done;
    float gain, target, delta, level;
    bool stopAtZero;
    int ch;
    char key[32], name[32];
    int16_t buf[4096];
    int bufLen, bufPos;
} Voice;

#define LOOPS 16
#define SLOTS (CH_COUNT * 2 + LOOPS)

struct Mixer {
    int rate;
    pthread_mutex_t lock;
    Voice *v[SLOTS];
    float chanVol[CH_COUNT];
};

Mixer *bx_mixer_new(int rate) {
    Mixer *m = bx_alloc(sizeof *m);
    m->rate = rate;
    for (int i = 0; i < CH_COUNT; i++) m->chanVol[i] = 1;
    pthread_mutex_init(&m->lock, NULL);
    return m;
}

static void voice_free(Voice *v) {
    if (!v) return;
    if (v->ogg) stb_vorbis_close(v->ogg);
    free(v->data);
    free(v);
}

void bx_mixer_free(Mixer *m) {
    if (!m) return;
    for (int i = 0; i < SLOTS; i++) voice_free(m->v[i]);
    pthread_mutex_destroy(&m->lock);
    free(m);
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

void bx_mixer_mix(Mixer *m, float *out, int frames, bool paused) {
    memset(out, 0, sizeof(float) * (size_t)frames * 2);
    if (!m || paused) return;
    pthread_mutex_lock(&m->lock);
    for (int s = 0; s < SLOTS; s++) {
        Voice *v = m->v[s];
        if (!v || v->done) continue;
        if (!v->primed) {
            if (!read_frame(v, v->cur) || !read_frame(v, v->next)) {
                v->done = true;
                continue;
            }
            v->primed = true;
        }
        float vol = m->chanVol[v->ch];
        for (int i = 0; i < frames; i++) {
            if (v->gain != v->target) {
                v->gain += v->delta;
                if ((v->delta > 0 && v->gain >= v->target) || (v->delta < 0 && v->gain <= v->target) || v->delta == 0) v->gain = v->target;
                if (v->gain <= 0 && v->stopAtZero) {
                    v->done = true;
                    break;
                }
            }
            float f = (float)v->pos, g = v->gain * v->level * vol / 32768.0f;
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

static float cfg_volume(BX *b, const char *key, float def) {
    const char *v = bx_cfg_get(&b->cfg, "default volume", key);
    return v ? (float)atof(v) : def;
}

static float channel_volume(BX *b, int ch) {
    bool voice = ch == CH_VOICE || ch == CH_VOICE2 || ch == CH_BGV;
    int v = ch == CH_BGM ? b->volBgm : ch == CH_BGS ? b->volBgs : ch == CH_BGV ? b->volBgv : voice ? b->volVoice : b->volSe;
    float base = ch == CH_BGM ? cfg_volume(b, "bgm", 0.5f) : ch == CH_BGS ? cfg_volume(b, "bgs", 0.5f) : ch == CH_BGV ? cfg_volume(b, "bgv", 0.5f) : voice ? cfg_volume(b, "voice", 0.75f) : cfg_volume(b, "se", 0.75f);
    return base * (float)v / 100.0f * (float)b->volMaster / 100.0f;
}

void bx_snd_volumes(BX *b) {
    if (!b->mixer) return;
    pthread_mutex_lock(&b->mixer->lock);
    for (int c = 0; c < CH_COUNT; c++) b->mixer->chanVol[c] = channel_volume(b, c);
    pthread_mutex_unlock(&b->mixer->lock);
}

static void fade_out(BX *b, Voice *v, int ms) {
    if (!v) return;
    if (ms <= 0) {
        v->done = true;
        return;
    }
    v->target = 0;
    v->stopAtZero = true;
    v->delta = -v->gain / ((float)ms * (float)b->audioRate / 1000.0f);
}

static Voice *open_voice(BX *b, int ch, int ix, const char *name, bool loop, int fadeMs) {
    size_t n;
    u8 *d = bx_res_read(&b->arc, ix, name, &n);
    if (!d) {
        bx_log(b, "missing sound %s", name);
        return NULL;
    }
    Voice *v = bx_alloc(sizeof *v);
    v->data = d;
    int err = 0;
    v->ogg = n >= 4 && !memcmp(d, "OggS", 4) ? stb_vorbis_open_memory(d, (int)n, &err, NULL) : NULL;
    if (!v->ogg) {
        bx_log(b, "can't decode sound %s", name);
        voice_free(v);
        return NULL;
    }
    v->rate = (int)stb_vorbis_get_info(v->ogg).sample_rate;
    v->loop = loop;
    v->step = (double)v->rate / (double)b->audioRate;
    v->gain = fadeMs > 0 ? 0 : 1;
    v->target = 1;
    v->level = 1;
    v->delta = fadeMs > 0 ? 1.0f / ((float)fadeMs * (float)b->audioRate / 1000.0f) : 0;
    v->ch = ch;
    return v;
}

void bx_snd_play(BX *b, int ch, int ix, const char *name, bool loop, int fadeMs) {
    if (!b->mixer || ch < 0 || ch >= CH_COUNT || !name || !*name) return;
    Voice *v = open_voice(b, ch, ix, name, loop, fadeMs);
    if (!v) return;
    Mixer *m = b->mixer;
    pthread_mutex_lock(&m->lock);
    Voice *gone = m->v[ch + CH_COUNT];
    m->v[ch + CH_COUNT] = m->v[ch];
    fade_out(b, m->v[ch + CH_COUNT], fadeMs);
    m->v[ch] = v;
    m->chanVol[ch] = channel_volume(b, ch);
    pthread_mutex_unlock(&m->lock);
    voice_free(gone);
}

void bx_snd_stop(BX *b, int ch, int fadeMs) {
    if (!b->mixer || ch < 0 || ch >= CH_COUNT) return;
    pthread_mutex_lock(&b->mixer->lock);
    fade_out(b, b->mixer->v[ch], fadeMs);
    pthread_mutex_unlock(&b->mixer->lock);
}

bool bx_snd_playing(BX *b, int ch) {
    if (!b->mixer || ch < 0 || ch >= CH_COUNT) return false;
    pthread_mutex_lock(&b->mixer->lock);
    bool r = b->mixer->v[ch] && !b->mixer->v[ch]->done && !b->mixer->v[ch]->stopAtZero;
    pthread_mutex_unlock(&b->mixer->lock);
    return r;
}

void bx_snd_loop(BX *b, int ch, const char *key, int ix, const char *name) {
    if (!b->mixer || !name || !*name) return;
    Mixer *m = b->mixer;
    pthread_mutex_lock(&m->lock);
    int slot = -1;
    bool same = false;
    for (int s = CH_COUNT * 2; s < SLOTS; s++)
        if (m->v[s] && m->v[s]->ch == ch && !strcmp(m->v[s]->key, key)) {
            slot = s;
            same = !m->v[s]->done && !m->v[s]->stopAtZero && !strcmp(m->v[s]->name, name);
        }
    pthread_mutex_unlock(&m->lock);
    if (same) return;
    Voice *v = open_voice(b, ch, ix, name, true, 0);
    if (!v) return;
    snprintf(v->key, sizeof v->key, "%s", key);
    snprintf(v->name, sizeof v->name, "%s", name);
    Voice *gone = NULL;
    pthread_mutex_lock(&m->lock);
    for (int s = CH_COUNT * 2; s < SLOTS && slot < 0; s++)
        if (!m->v[s] || m->v[s]->done) slot = s;
    if (slot < 0) slot = CH_COUNT * 2;
    gone = m->v[slot];
    m->v[slot] = v;
    m->chanVol[ch] = channel_volume(b, ch);
    pthread_mutex_unlock(&m->lock);
    voice_free(gone);
}

void bx_snd_loop_stop(BX *b, int ch, const char *key, int fadeMs) {
    if (!b->mixer) return;
    pthread_mutex_lock(&b->mixer->lock);
    for (int s = CH_COUNT * 2; s < SLOTS; s++) {
        Voice *v = b->mixer->v[s];
        if (v && v->ch == ch && (!key || !strcmp(v->key, key))) fade_out(b, v, fadeMs);
    }
    pthread_mutex_unlock(&b->mixer->lock);
}

void bx_snd_loop_level(BX *b, int ch, const char *key, float level) {
    if (!b->mixer) return;
    pthread_mutex_lock(&b->mixer->lock);
    for (int s = CH_COUNT * 2; s < SLOTS; s++) {
        Voice *v = b->mixer->v[s];
        if (v && v->ch == ch && (!key || !strcmp(v->key, key))) v->level = level;
    }
    pthread_mutex_unlock(&b->mixer->lock);
}

void bx_mixer_lock(Mixer *m, bool lock) {
    if (!m) return;
    if (lock) pthread_mutex_lock(&m->lock);
    else pthread_mutex_unlock(&m->lock);
}

float bx_mixer_volume(Mixer *m, int ch) { return m ? m->chanVol[ch] : 0; }

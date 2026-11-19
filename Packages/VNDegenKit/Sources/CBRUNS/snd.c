#include "br.h"

#include <pthread.h>

#include "br_vorbis_names.h"
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
    float gain, target, delta;
    bool stopAtZero;
    int16_t buf[4096];
    int bufLen, bufPos;
} Voice;

struct Mixer {
    pthread_mutex_t lock;
    Voice *v[BR_CHANNELS];
    float chanVol[BR_CHANNELS];
};

Mixer *mixer_new(void) {
    Mixer *m = br_alloc(sizeof *m);
    for (int i = 0; i < BR_CHANNELS; i++) m->chanVol[i] = 1;
    pthread_mutex_init(&m->lock, NULL);
    return m;
}

static void voice_free(Voice *v) {
    if (!v) return;
    if (v->ogg) stb_vorbis_close(v->ogg);
    free(v->data);
    free(v);
}

void mixer_free(Mixer *m) {
    if (!m) return;
    for (int i = 0; i < BR_CHANNELS; i++) voice_free(m->v[i]);
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

void mixer_mix(Mixer *m, float *out, int frames, bool paused) {
    memset(out, 0, sizeof(float) * (size_t)frames * 2);
    if (!m || paused) return;
    pthread_mutex_lock(&m->lock);
    for (int c = 0; c < BR_CHANNELS; c++) {
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

static Voice *voice_open(BR *e, const char *rel) {
    char *path = br_game_path(e, rel);
    size_t n;
    u8 *d = br_read_file(path, &n);
    free(path);
    if (!d) return NULL;
    if (n >= 4 && memcmp(d, "OggS", 4))
        for (size_t i = 0; i < n && i < 0x800; i++) d[i] ^= 0xff;
    int err = 0;
    Voice *v = br_alloc(sizeof *v);
    v->data = d;
    v->ogg = stb_vorbis_open_memory(d, (int)n, &err, NULL);
    if (!v->ogg) {
        br_log(e, "can't decode sound %s", rel);
        voice_free(v);
        return NULL;
    }
    v->rate = (int)stb_vorbis_get_info(v->ogg).sample_rate;
    return v;
}

static float channel_volume(BR *e, int ch) {
    if (e->muted) return 0;
    int k = ch < CH_SE ? 0 : ch < CH_VOICE ? 1 : 2;
    return (float)e->vol[k] / 100.0f * (float)e->volMaster / 100.0f * e->chanGain[ch];
}

void snd_apply_volumes(BR *e) {
    Mixer *m = e->mixer;
    if (!m) return;
    pthread_mutex_lock(&m->lock);
    for (int ch = 0; ch < BR_CHANNELS; ch++) m->chanVol[ch] = channel_volume(e, ch);
    pthread_mutex_unlock(&m->lock);
}

static void fade_out(BR *e, Voice *v, int ms) {
    if (!v) return;
    if (ms <= 0) {
        v->done = true;
        return;
    }
    v->target = 0;
    v->stopAtZero = true;
    v->delta = -v->gain / ((float)ms * (float)e->audioRate / 1000.0f);
}

static void start(BR *e, Voice *v, int ch, bool loop, int fadeMs) {
    if (!e->mixer || ch < 0 || ch >= BR_CHANNELS) {
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

void snd_stop(BR *e, int ch, int fadeMs) {
    if (ch >= CH_MUSIC && ch < CH_SE) e->music[ch - CH_MUSIC] = -1;
    if (!e->mixer || ch < 0 || ch >= BR_CHANNELS) return;
    pthread_mutex_lock(&e->mixer->lock);
    fade_out(e, e->mixer->v[ch], fadeMs);
    pthread_mutex_unlock(&e->mixer->lock);
}

bool snd_playing(BR *e, int ch) {
    if (!e->mixer || ch < 0 || ch >= BR_CHANNELS) return false;
    pthread_mutex_lock(&e->mixer->lock);
    bool r = e->mixer->v[ch] && !e->mixer->v[ch]->done;
    pthread_mutex_unlock(&e->mixer->lock);
    return r;
}

void snd_music(BR *e, int slot, int track, int fadeMs) {
    if (slot < 0 || slot >= CH_SE - CH_MUSIC) return;
    if (e->music[slot] == track && snd_playing(e, CH_MUSIC + slot)) return;
    char rel[128];
    snprintf(rel, sizeof rel, "%s/%s%0*d.um3", e->cfg.musicDir, e->cfg.musicBase, e->cfg.musicDigits, track);
    Voice *v = voice_open(e, rel);
    if (!v) {
        br_log(e, "missing music %s", rel);
        return;
    }
    e->music[slot] = track;
    start(e, v, CH_MUSIC + slot, true, fadeMs);
}

void snd_se(BR *e, int slot, int num) {
    if (slot < 0 || slot >= CH_VOICE - CH_SE) slot = 0;
    char rel[128];
    snprintf(rel, sizeof rel, "%s/%s%0*d.um3", e->cfg.effectDir, e->cfg.effectBase, e->cfg.effectDigits, num);
    Voice *v = voice_open(e, rel);
    if (!v) {
        br_log(e, "missing sound %s", rel);
        return;
    }
    start(e, v, CH_SE + slot, false, 0);
}

void snd_voice(BR *e, int slot, int num) {
    if (slot < 0 || slot > 9) return;
    char rel[128];
    snprintf(rel, sizeof rel, "%s/%d/%d%0*d.um3", e->cfg.voiceDir, slot, slot, e->cfg.voiceDigits, num);
    Voice *v = voice_open(e, rel);
    if (!v) {
        br_log(e, "missing voice %s", rel);
        return;
    }
    start(e, v, CH_VOICE + slot, false, 0);
}

void snd_se_hover(BR *e) {}

void snd_se_click(BR *e) {}

void snd_volume(BR *e, int ch, int vol, int ms) {
    if (ch < 0 || ch >= BR_CHANNELS) return;
    e->chanGain[ch] = (float)vol / 100.0f;
    snd_apply_volumes(e);
}

void snd_stop_voices(BR *e) {
    for (int i = CH_VOICE; i < BR_CHANNELS; i++) snd_stop(e, i, 0);
}

#include "age.h"

#include <math.h>
#include <pthread.h>

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#define DRWAV_API static
#define DRWAV_PRIVATE static
#include "third_party/dr_wav.h"

#include "ag_vorbis_names.h"
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "third_party/stb_vorbis.c"
#undef L
#undef C
#undef R

#include "vnvideo.h"

typedef struct Voice {
    u8 *data;
    size_t size;
    drwav wav;
    bool isWav;
    stb_vorbis *ogg;
    int channels, rate;
    double step, pos;
    int16_t cur[2], next[2];
    bool primed, loop, done, playing;
    int16_t buf[4096];
    int bufLen, bufPos;
} Voice;

struct Mixer {
    int rate;
    pthread_mutex_t lock;
    Voice *v[AG_CHANNELS];
    float gain[AG_CHANNELS];
    vnvideo *movie[8];
    float movieGain;
};

Mixer *mixer_new(int rate) {
    Mixer *m = ag_alloc(sizeof *m);
    m->rate = rate;
    for (int i = 0; i < AG_CHANNELS; i++) m->gain[i] = 1;
    m->movieGain = 1;
    pthread_mutex_init(&m->lock, NULL);
    return m;
}

static void voice_free(Voice *v) {
    if (!v) return;
    if (v->isWav) drwav_uninit(&v->wav);
    if (v->ogg) stb_vorbis_close(v->ogg);
    free(v->data);
    free(v);
}

void mixer_free(Mixer *m) {
    if (!m) return;
    for (int i = 0; i < AG_CHANNELS; i++) voice_free(m->v[i]);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

static void voice_rewind(Voice *v) {
    if (v->isWav) drwav_seek_to_pcm_frame(&v->wav, 0);
    else if (v->ogg) stb_vorbis_seek_start(v->ogg);
    v->bufLen = v->bufPos = 0;
    v->primed = false;
    v->done = false;
    v->pos = 0;
}

static bool refill(Voice *v) {
    v->bufPos = 0;
    v->bufLen = 0;
    for (int attempt = 0; attempt < 2; attempt++) {
        if (v->isWav) {
            int16_t tmp[4096];
            int ch = v->channels > 0 ? v->channels : 1;
            drwav_uint64 want = (drwav_uint64)(4096 / ch);
            if (want > 2048) want = 2048;
            drwav_uint64 got = drwav_read_pcm_frames_s16(&v->wav, want, tmp);
            for (drwav_uint64 i = 0; i < got; i++) {
                v->buf[i * 2] = tmp[i * (drwav_uint64)ch];
                v->buf[i * 2 + 1] = ch > 1 ? tmp[i * (drwav_uint64)ch + 1] : tmp[i * (drwav_uint64)ch];
            }
            v->bufLen = (int)got;
        } else if (v->ogg) v->bufLen = stb_vorbis_get_samples_short_interleaved(v->ogg, 2, v->buf, 4096);
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
    for (int c = 0; c < AG_CHANNELS; c++) {
        Voice *v = m->v[c];
        if (!v || !v->playing || v->done) continue;
        if (!v->primed) {
            if (!read_frame(v, v->cur) || !read_frame(v, v->next)) { v->done = true; continue; }
            v->primed = true;
        }
        float g = m->gain[c] / 32768.0f;
        for (int i = 0; i < frames; i++) {
            float f = (float)v->pos;
            out[i * 2] += ((float)v->cur[0] + ((float)v->next[0] - (float)v->cur[0]) * f) * g;
            out[i * 2 + 1] += ((float)v->cur[1] + ((float)v->next[1] - (float)v->cur[1]) * f) * g;
            v->pos += v->step;
            while (v->pos >= 1.0) {
                v->pos -= 1.0;
                v->cur[0] = v->next[0];
                v->cur[1] = v->next[1];
                if (!read_frame(v, v->next)) { v->done = true; break; }
            }
            if (v->done) break;
        }
    }
    for (int mv = 0; mv < 8; mv++) {
        vnvideo *video = m->movie[mv];
        if (!video) continue;
        vnvideo_info info;
        vnvideo_get_info(video, &info);
        if (info.audio_rate <= 0) continue;
        float tmp[4096];
        int left = frames, at = 0;
        double step = (double)info.audio_rate / (double)m->rate;
        int ch = info.audio_channels > 1 ? 2 : 1;
        while (left > 0) {
            int n = left > 1024 ? 1024 : left;
            size_t want = (size_t)((double)n * step);
            if (want < 1) want = 1;
            if (want > (size_t)(4096 / ch)) want = (size_t)(4096 / ch);
            size_t got = vnvideo_read_audio(video, tmp, want);
            if (!got) break;
            for (int i = 0; i < n; i++) {
                size_t k = (size_t)((double)i * step);
                if (k >= got) break;
                float l = tmp[k * (size_t)ch], r = ch > 1 ? tmp[k * (size_t)ch + 1] : l;
                out[(at + i) * 2] += l * m->movieGain;
                out[(at + i) * 2 + 1] += r * m->movieGain;
            }
            left -= n;
            at += n;
        }
    }
    pthread_mutex_unlock(&m->lock);
    for (int i = 0; i < frames * 2; i++) out[i] = out[i] > 1 ? 1 : out[i] < -1 ? -1 : out[i];
}

static Voice *voice_memory(u8 *data, size_t size) {
    Voice *v = ag_alloc(sizeof *v);
    v->data = data;
    v->size = size;
    if (size >= 4 && !memcmp(data, "OggS", 4)) {
        int err = 0;
        v->ogg = stb_vorbis_open_memory(data, (int)size, &err, NULL);
        if (!v->ogg) { voice_free(v); return NULL; }
        stb_vorbis_info info = stb_vorbis_get_info(v->ogg);
        v->channels = 2;
        v->rate = (int)info.sample_rate;
        return v;
    }
    if (!drwav_init_memory(&v->wav, data, size, NULL)) { voice_free(v); return NULL; }
    v->isWav = true;
    v->channels = v->wav.channels;
    v->rate = (int)v->wav.sampleRate;
    return v;
}

static void apply_gains(AG *g);

static float bp_gain(i32 bp) {
    if (bp <= 0) return 0;
    if (bp >= 10000) return 1;
    return (float)bp / 10000.0f;
}

static int route_of(int ch) { return ch == 11 ? 1 : ch == 12 ? 3 : 2; }

void snd_apply(AG *g) { apply_gains(g); }

static void apply_gains(AG *g) {
    Sound *s = &g->snd;
    if (!s->mixer) return;
    float master = bp_gain(s->vol[0]);
    pthread_mutex_lock(&s->mixer->lock);
    for (int c = 0; c < AG_CHANNELS; c++) {
        int r = route_of(c);
        float v = s->route[r] ? bp_gain(s->vol[r]) * master : 0;
        if (c == 11) v *= s->bgmLevel;
        if (c == 12 && s->muteNext) v = 0;
        s->mixer->gain[c] = v;
    }
    s->mixer->movieGain = s->route[4] ? bp_gain(s->vol[4]) * master : 0;
    pthread_mutex_unlock(&s->mixer->lock);
}

void snd_init(AG *g, int rate) {
    Sound *s = &g->snd;
    s->rate = rate > 0 ? rate : 44100;
    s->mixer = mixer_new(s->rate);
    for (int i = 0; i < 5; i++) { s->vol[i] = 10000; s->route[i] = true; }
    s->bgmLevel = 1;
    for (int i = 0; i < 10; i++) s->sfxDelayCh[i] = -1;
    s->voiceDelay = -1;
    apply_gains(g);
}

void snd_close(AG *g) {
    mixer_free(g->snd.mixer);
    g->snd.mixer = NULL;
}

void snd_movie(AG *g, void *video, bool add) {
    Mixer *m = g->snd.mixer;
    if (!m) return;
    pthread_mutex_lock(&m->lock);
    for (int i = 0; i < 8; i++)
        if (m->movie[i] == video) m->movie[i] = NULL;
    if (add)
        for (int i = 0; i < 8; i++)
            if (!m->movie[i]) { m->movie[i] = video; break; }
    pthread_mutex_unlock(&m->lock);
}

static void set_voice(AG *g, int ch, Voice *v) {
    Mixer *m = g->snd.mixer;
    if (!m || ch < 0 || ch >= AG_CHANNELS) { voice_free(v); return; }
    if (v) v->step = (double)v->rate / (double)m->rate;
    pthread_mutex_lock(&m->lock);
    Voice *old = m->v[ch];
    m->v[ch] = v;
    pthread_mutex_unlock(&m->lock);
    voice_free(old);
}

static void start(AG *g, int ch, bool loop) {
    Mixer *m = g->snd.mixer;
    if (!m || ch < 0 || ch >= AG_CHANNELS) return;
    if (!g->snd.route[route_of(ch)]) return;
    pthread_mutex_lock(&m->lock);
    Voice *v = m->v[ch];
    if (v) {
        if (v->playing || v->done) voice_rewind(v);
        v->loop = loop;
        v->playing = true;
    }
    pthread_mutex_unlock(&m->lock);
}

static bool playing(AG *g, int ch) {
    Mixer *m = g->snd.mixer;
    if (!m || ch < 0 || ch >= AG_CHANNELS) return false;
    pthread_mutex_lock(&m->lock);
    bool r = m->v[ch] && m->v[ch]->playing && !m->v[ch]->done;
    pthread_mutex_unlock(&m->lock);
    return r;
}

static Voice *load_id(AG *g, u32 id) {
    size_t n = 0;
    u8 *d = store_read(&g->st, id, &n);
    if (!d) { ag_log(g, "sound %#x can't be read", id); return NULL; }
    Voice *v = voice_memory(d, n);
    if (!v) { ag_log(g, "sound %#x can't be decoded", id); free(d); }
    return v;
}

static void bgm_play(AG *g, i32 track, bool loop, bool force) {
    Sound *s = &g->snd;
    s->bgmFading = false;
    if (track == 0) track = s->bgm;
    if (track <= 0) { set_voice(g, 11, NULL); s->bgm = 0; return; }
    if (!force && track == s->bgm && playing(g, 11)) return;
    char name[32];
    snprintf(name, sizeof name, "BGM%03d.OGG", track);
    size_t n = 0;
    u8 *d = store_read_name(&g->st, name, &n);
    Voice *v = d ? voice_memory(d, n) : NULL;
    if (!v) { ag_log(g, "music %s can't be played", name); free(d); return; }
    s->bgm = track;
    s->bgmLevel = 1;
    apply_gains(g);
    set_voice(g, 11, v);
    start(g, 11, loop);
}

void snd_tick(AG *g) {
    Sound *s = &g->snd;
    if (s->bgmFading) {
        u32 el = g->now - s->bgmFadeStart;
        float t = s->bgmFadeDur ? (float)el / (float)s->bgmFadeDur : 1;
        if (t >= 1 || g->in.skip) {
            t = 1;
            s->bgmFading = false;
        }
        s->bgmLevel = s->bgmFrom + (s->bgmTo - s->bgmFrom) * t;
        if (!s->bgmFading && s->bgmTo <= 0) {
            set_voice(g, 11, NULL);
            s->bgm = 0;
        }
        apply_gains(g);
    }
    for (int i = 0; i < 10; i++)
        if (s->sfxDelayCh[i] >= 0 && (i32)(g->now - s->sfxDelayAt[i]) >= 0) {
            start(g, s->sfxDelayCh[i], s->sfxDelayMode[i] == 1);
            s->sfxDelayCh[i] = -1;
        }
    if (s->voiceDelay >= 0 && (i32)(g->now - s->voiceDelayAt) >= 0) {
        Voice *v = load_id(g, (u32)s->voiceDelay);
        s->voiceDelay = -1;
        if (v) { set_voice(g, 12, v); start(g, 12, false); }
    }
}

static int op_sfx_load(AG *g) {
    i32 ch = arg_i(g, 2);
    Voice *v = load_id(g, (u32)arg_i(g, 1));
    if (v) set_voice(g, ch, v);
    if (v && ch >= 0 && ch < 10) g->snd.sfxRes[ch] = arg_i(g, 1);
    return OP_NEXT;
}

static int op_sfx_start(AG *g) { start(g, arg_i(g, 1), false); return OP_NEXT; }
static int op_sfx_loop(AG *g) { start(g, arg_i(g, 1), true); return OP_NEXT; }
static int op_sfx_release(AG *g) {
    i32 ch = arg_i(g, 1);
    set_voice(g, ch, NULL);
    if (ch >= 0 && ch < 10) g->snd.sfxRes[ch] = 0;
    return OP_NEXT;
}

void snd_restore(AG *g, i32 bgm, const i32 *sfx) {
    for (int c = 0; c < AG_CHANNELS; c++) set_voice(g, c, NULL);
    g->snd.bgm = 0;
    if (bgm > 0) bgm_play(g, bgm, true, true);
    for (int c = 0; c < 10; c++) {
        g->snd.sfxRes[c] = 0;
        if (sfx[c] > 0) {
            Voice *v = load_id(g, (u32)sfx[c]);
            if (v) { set_voice(g, c, v); g->snd.sfxRes[c] = sfx[c]; }
        }
    }
}

static int op_sfx_schedule(AG *g) {
    i32 ch = arg_i(g, 1);
    if (ch < 0 || ch > 9) return OP_NEXT;
    g->snd.sfxDelayCh[ch] = ch;
    g->snd.sfxDelayMode[ch] = arg_i(g, 2);
    g->snd.sfxDelayAt[ch] = g->now + (u32)arg_i(g, 3);
    return OP_NEXT;
}

static int op_bgm(AG *g) { bgm_play(g, arg_i(g, 1), true, false); return OP_NEXT; }
static int op_bgm_restart_loop(AG *g) { bgm_play(g, arg_i(g, 1), true, true); return OP_NEXT; }
static int op_bgm_restart_once(AG *g) { bgm_play(g, arg_i(g, 1), false, true); return OP_NEXT; }

static int op_bgm_stop(AG *g) {
    g->snd.bgmFading = false;
    set_voice(g, 11, NULL);
    g->snd.bgm = 0;
    return OP_NEXT;
}

static int op_bgm_fade(AG *g) {
    Sound *s = &g->snd;
    float to = (float)arg_i(g, 1) / 10000.0f;
    i32 dur = arg_i(g, 2);
    s->bgmFrom = s->bgmLevel;
    s->bgmTo = to < 0 ? 0 : to > 1 ? 1 : to;
    s->bgmFadeStart = g->now;
    s->bgmFadeDur = dur > 0 ? (u32)dur : 0;
    s->bgmFading = true;
    snd_tick(g);
    return OP_NEXT;
}

static int op_bgm_current(AG *g) { arg_set_i(g, 1, g->snd.bgm); return OP_NEXT; }

static int op_voice(AG *g) {
    Sound *s = &g->snd;
    s->lastVoice = arg_i(g, 1);
    hist_voice(g, s->lastVoice, 0);
    if (g->in.skip) return OP_NEXT;
    Voice *v = load_id(g, (u32)s->lastVoice);
    apply_gains(g);
    if (v) { set_voice(g, 12, v); start(g, 12, false); }
    s->muteNext = false;
    return OP_NEXT;
}

static int op_voice_history(AG *g) {
    hist_voice(g, arg_i(g, 1), 1);
    Voice *v = load_id(g, (u32)arg_i(g, 1));
    if (v) { set_voice(g, 12, v); start(g, 12, false); }
    return OP_NEXT;
}

static int op_voice_schedule(AG *g) {
    g->snd.voiceDelay = arg_i(g, 1);
    g->snd.voiceDelayAt = g->now + (u32)arg_i(g, 3);
    return OP_NEXT;
}

static int op_mute_next(AG *g) { g->snd.muteNext = true; return OP_NEXT; }

static int op_voice_reset(AG *g) {
    set_voice(g, 12, NULL);
    g->snd.muteNext = false;
    apply_gains(g);
    return OP_NEXT;
}

static int op_duck(AG *g) { g->snd.duckFlags = (u32)arg_i(g, 1); return OP_NEXT; }
static int op_nop(AG *g) { (void)g; return OP_NEXT; }

static int op_vol_get(AG *g) {
    i32 c = arg_i(g, 1);
    arg_set_i(g, 2, c >= 0 && c < 5 ? g->snd.vol[c] : 0);
    return OP_NEXT;
}

static int op_vol_set(AG *g) {
    i32 c = arg_i(g, 1);
    if (c >= 0 && c < 5) { g->snd.vol[c] = arg_i(g, 2); g->snd.volSet[c] = true; }
    apply_gains(g);
    return OP_NEXT;
}

static int op_route_set(AG *g) {
    i32 c = arg_i(g, 1);
    if (c >= 1 && c < 5) g->snd.route[c] = arg_i(g, 2) != 0;
    apply_gains(g);
    return OP_NEXT;
}

static int op_route_get(AG *g) {
    i32 c = arg_i(g, 1);
    arg_set_i(g, 2, c >= 1 && c < 5 ? g->snd.route[c] : 0);
    return OP_NEXT;
}

static int op_playing(AG *g) { arg_set_i(g, 1, playing(g, arg_i(g, 2))); return OP_NEXT; }

void snd_register(void) {
    vm_register(0xb4, op_sfx_load);
    vm_register(0xb5, op_sfx_start);
    vm_register(0xb6, op_sfx_release);
    vm_register(0xb7, op_bgm_restart_loop);
    vm_register(0xb8, op_bgm_stop);
    vm_register(0xb9, op_bgm_restart_once);
    vm_register(0xba, op_sfx_loop);
    vm_register(0xbf, op_bgm);
    vm_register(0xc0, op_bgm_current);
    vm_register(0xc2, op_bgm_fade);
    vm_register(0xc4, op_voice);
    vm_register(0xc5, op_vol_get);
    vm_register(0xc6, op_vol_set);
    vm_register(0xc7, op_route_get);
    vm_register(0x1ba, op_route_set);
    vm_register(0x1bc, op_voice_reset);
    vm_register(0x1bd, op_voice_history);
    vm_register(0x1be, op_playing);
    vm_register(0x1bf, op_nop);
    vm_register(0x1cf, op_duck);
    vm_register(0x1d5, op_mute_next);
    vm_register(0x2bf, op_sfx_schedule);
    vm_register(0x2c0, op_voice_schedule);
}

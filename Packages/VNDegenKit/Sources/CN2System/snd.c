// Sound: Ogg Vorbis through stb_vorbis (public domain or MIT). The game thread decodes each playing sound a little
// ahead into its channel's ring of stereo frames at the output rate; the audio thread only mixes the rings, ramping
// each channel's gain toward the volume the game thread set.
#include "n2.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

// stb_vorbis's functions aren't static; rename them so another engine's copy in the same app doesn't clash.
#define stb_vorbis_close n2_stb_vorbis_close
#define stb_vorbis_decode_memory n2_stb_vorbis_decode_memory
#define stb_vorbis_get_error n2_stb_vorbis_get_error
#define stb_vorbis_get_file_offset n2_stb_vorbis_get_file_offset
#define stb_vorbis_get_frame_float n2_stb_vorbis_get_frame_float
#define stb_vorbis_get_frame_short n2_stb_vorbis_get_frame_short
#define stb_vorbis_get_frame_short_interleaved n2_stb_vorbis_get_frame_short_interleaved
#define stb_vorbis_get_info n2_stb_vorbis_get_info
#define stb_vorbis_get_comment n2_stb_vorbis_get_comment
#define stb_vorbis_get_sample_offset n2_stb_vorbis_get_sample_offset
#define stb_vorbis_get_samples_float n2_stb_vorbis_get_samples_float
#define stb_vorbis_get_samples_float_interleaved n2_stb_vorbis_get_samples_float_interleaved
#define stb_vorbis_get_samples_short n2_stb_vorbis_get_samples_short
#define stb_vorbis_get_samples_short_interleaved n2_stb_vorbis_get_samples_short_interleaved
#define stb_vorbis_open_memory n2_stb_vorbis_open_memory
#define stb_vorbis_seek n2_stb_vorbis_seek
#define stb_vorbis_seek_frame n2_stb_vorbis_seek_frame
#define stb_vorbis_seek_start n2_stb_vorbis_seek_start
#define stb_vorbis_stream_length_in_samples n2_stb_vorbis_stream_length_in_samples
#define stb_vorbis_stream_length_in_seconds n2_stb_vorbis_stream_length_in_seconds
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "third_party/stb_vorbis.c"
#undef L
#undef C
#undef R

#define RING 32768       // frames per channel ring, about 0.7 s at 48 kHz
#define AHEAD 12000      // frames decoded ahead each update
#define MAX_CHANNELS 48

typedef struct Channel {
    float ring[RING * 2];
    int head, tail;  // ring positions in frames; written by the game thread, read by the audio thread
    float gain, target;
    bool active;
    bool ended;      // the decoder reached the end and the ring drained
    u64 played;      // frames consumed, for positions
} Channel;

struct Mixer {
    int rate;
    pthread_mutex_t lock;
    Channel *ch[MAX_CHANNELS];
};

struct Sound {
    int kind;  // 0 BGM, 1 SE, 2 voice
    u8 *data;
    size_t size;
    stb_vorbis *dec;
    int srcRate, srcChannels;
    u32 lengthSamples;
    Channel *ch;
    bool playing, loop, decoderDone;
    u32 loopStart, loopEnd;  // source samples; loopEnd 0 is the end
    double vol;              // 0 to 1000, SetVolume's value
    double fadeFrom, fadeTo;
    u32 fadeStart, fadeDur;
    double srcPos;           // resampling position in source samples
    float lastL, lastR, curL, curR;
    u64 framesQueued;
};

struct Mixer *mixer_new(int rate) {
    struct Mixer *m = n2_alloc(sizeof *m);
    m->rate = rate;
    pthread_mutex_init(&m->lock, NULL);
    return m;
}

void mixer_free(struct Mixer *m) {
    pthread_mutex_lock(&m->lock);
    for (int i = 0; i < MAX_CHANNELS; i++) free(m->ch[i]);
    pthread_mutex_unlock(&m->lock);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

static Channel *channel_new(struct Mixer *m) {
    Channel *c = n2_alloc(sizeof *c);
    c->active = true;
    pthread_mutex_lock(&m->lock);
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (!m->ch[i]) {
            m->ch[i] = c;
            pthread_mutex_unlock(&m->lock);
            return c;
        }
    }
    pthread_mutex_unlock(&m->lock);
    free(c);
    return NULL;
}

static void channel_free(struct Mixer *m, Channel *c) {
    if (!c) return;
    pthread_mutex_lock(&m->lock);
    for (int i = 0; i < MAX_CHANNELS; i++)
        if (m->ch[i] == c) m->ch[i] = NULL;
    pthread_mutex_unlock(&m->lock);
    free(c);
}

static u8 *read_sound(N2 *n, const char *path, size_t *size) {
    u8 *d = fs_read(&n->fs, path, size);
    if (!d) {
        char *p = n2_fmt("%s.ogg", path);
        d = fs_read(&n->fs, p, size);
        free(p);
    }
    return d;
}

Sound *sound_load(N2 *n, const char *path, int kind) {
    Sound *s = n2_alloc(sizeof *s);
    s->kind = kind;
    s->vol = 1000;
    s->data = read_sound(n, path, &s->size);
    if (!s->data) {
        n2_log(n, "missing sound %s", path);
        return s;
    }
    int err;
    s->dec = stb_vorbis_open_memory(s->data, (int)s->size, &err, NULL);
    if (!s->dec) {
        n2_log(n, "can't decode sound %s", path);
        return s;
    }
    stb_vorbis_info info = stb_vorbis_get_info(s->dec);
    s->srcRate = (int)info.sample_rate;
    s->srcChannels = info.channels;
    s->lengthSamples = stb_vorbis_stream_length_in_samples(s->dec);
    return s;
}

void sound_free(N2 *n, Sound *s) {
    if (!s) return;
    channel_free(n->mixer, s->ch);
    if (s->dec) stb_vorbis_close(s->dec);
    free(s->data);
    free(s);
}

void sound_play(N2 *n, Sound *s) {
    if (!s->dec) return;
    if (!s->ch) s->ch = channel_new(n->mixer);
    if (!s->ch) return;
    pthread_mutex_lock(&n->mixer->lock);
    s->ch->head = s->ch->tail = 0;
    s->ch->ended = false;
    s->ch->played = 0;
    s->ch->gain = -1;  // jump straight to the first target
    pthread_mutex_unlock(&n->mixer->lock);
    stb_vorbis_seek_start(s->dec);
    s->srcPos = 0;
    s->curL = s->curR = s->lastL = s->lastR = 0;
    s->framesQueued = 0;
    s->decoderDone = false;
    s->playing = true;
}

void sound_stop(N2 *n, Sound *s) {
    s->playing = false;
    if (s->ch) {
        channel_free(n->mixer, s->ch);
        s->ch = NULL;
    }
}

void sound_set_volume(N2 *n, Sound *s, double volume, u32 durationMs) {
    double cur = s->vol;
    if (s->fadeDur && n->now - s->fadeStart < s->fadeDur)
        cur = s->fadeFrom + (s->fadeTo - s->fadeFrom) * (double)(n->now - s->fadeStart) / s->fadeDur;
    if (n2_skip_active(n)) durationMs = 0;
    s->fadeFrom = cur;
    s->fadeTo = volume;
    s->fadeStart = n->now;
    s->fadeDur = durationMs;
    if (!durationMs) s->vol = volume;
}

void sound_set_loop(N2 *n, Sound *s, bool loop) { s->loop = loop; }

void sound_set_loop_points(N2 *n, Sound *s, u32 startMs, u32 endMs) {
    s->loop = true;
    s->loopStart = (u32)((u64)startMs * (u32)s->srcRate / 1000);
    s->loopEnd = endMs ? (u32)((u64)endMs * (u32)s->srcRate / 1000) : 0;
}

bool sound_playing(Sound *s) { return s && s->playing; }

u32 sound_duration_ms(Sound *s) {
    return s && s->srcRate ? (u32)((u64)s->lengthSamples * 1000 / (u32)s->srcRate) : 0;
}

u32 sound_position_ms(Sound *s) {
    if (!s || !s->ch || !s->srcRate) return 0;
    return (u32)(s->ch->played * 1000 / 44100);
}

static double kind_volume(N2 *n, int kind) {
    static const char *vars[] = {"#SYSTEM_sound_volume_bgm", "#SYSTEM_sound_volume_se", "#SYSTEM_sound_volume_voice"};
    static const char *enables[] = {"#SYSTEM_sound_bgm", "#SYSTEM_sound_se", "#SYSTEM_sound_voice"};
    Value *en = var_get(n, enables[kind]);
    if (en && en->t != V_NULL && !v_truthy(en)) return 0;
    Value *v = var_get(n, vars[kind]);
    double x = v ? v_tonum(v) : 1000;
    return x < 0 ? 0 : x > 1000 ? 1 : x / 1000;
}

// Decodes source frames into the ring, resampled linearly to the output rate.
static void fill(N2 *n, Sound *s) {
    Channel *c = s->ch;
    int rate = n->mixer->rate;
    double step = (double)s->srcRate / rate;
    float tmp[AHEAD * 2];
    int produced = 0;
    pthread_mutex_lock(&n->mixer->lock);
    int used = (c->head - c->tail + RING) % RING;
    pthread_mutex_unlock(&n->mixer->lock);
    int want = AHEAD - used;
    if (want <= 0) return;
    float src[2];
    while (produced < want && !s->decoderDone) {
        while (s->srcPos >= 1) {
            s->lastL = s->curL;
            s->lastR = s->curR;
            float buf[2];
            int got = stb_vorbis_get_samples_float_interleaved(s->dec, s->srcChannels > 1 ? 2 : 1, buf, s->srcChannels > 1 ? 2 : 1);
            int pos = stb_vorbis_get_sample_offset(s->dec);
            if (got <= 0 || (s->loop && s->loopEnd && pos >= (int)s->loopEnd)) {
                if (s->loop) {
                    stb_vorbis_seek(s->dec, s->loopStart);
                    got = stb_vorbis_get_samples_float_interleaved(s->dec, s->srcChannels > 1 ? 2 : 1, buf, s->srcChannels > 1 ? 2 : 1);
                }
                if (got <= 0) {
                    s->decoderDone = true;
                    break;
                }
            }
            s->curL = buf[0];
            s->curR = s->srcChannels > 1 ? buf[1] : buf[0];
            s->srcPos -= 1;
        }
        if (s->decoderDone) break;
        float f = (float)s->srcPos;
        src[0] = s->lastL + (s->curL - s->lastL) * f;
        src[1] = s->lastR + (s->curR - s->lastR) * f;
        tmp[produced * 2] = src[0];
        tmp[produced * 2 + 1] = src[1];
        produced++;
        s->srcPos += step;
    }
    pthread_mutex_lock(&n->mixer->lock);
    for (int i = 0; i < produced; i++) {
        c->ring[c->head * 2] = tmp[i * 2];
        c->ring[c->head * 2 + 1] = tmp[i * 2 + 1];
        c->head = (c->head + 1) % RING;
    }
    pthread_mutex_unlock(&n->mixer->lock);
}

static void update_sound(N2 *n, Sound *s) {
    if (!s->playing || !s->ch) return;
    if (s->fadeDur) {
        u32 el = n->now - s->fadeStart;
        if (el >= s->fadeDur) {
            s->vol = s->fadeTo;
            s->fadeDur = 0;
        } else {
            s->vol = s->fadeFrom + (s->fadeTo - s->fadeFrom) * (double)el / s->fadeDur;
        }
    }
    fill(n, s);
    pthread_mutex_lock(&n->mixer->lock);
    s->ch->target = (float)(s->vol / 1000.0 * kind_volume(n, s->kind));
    bool drained = s->decoderDone && s->ch->head == s->ch->tail;
    pthread_mutex_unlock(&n->mixer->lock);
    if (drained) sound_stop(n, s);
}

// The voice and one-shot effects from text tags, outside the object tree.
static Sound *g_voice;
static Sound *g_se[4];

void voice_play(N2 *n, const char *path) {
    voice_stop(n);
    g_voice = sound_load(n, path, 2);
    sound_play(n, g_voice);
}

void voice_stop(N2 *n) {
    if (g_voice) {
        sound_free(n, g_voice);
        g_voice = NULL;
    }
}

bool voice_playing(N2 *n) { return g_voice && g_voice->playing; }

void voice_se(N2 *n, const char *path) {
    int slot = 0;
    for (int i = 0; i < 4; i++) {
        if (!g_se[i] || !g_se[i]->playing) {
            slot = i;
            break;
        }
    }
    if (g_se[slot]) sound_free(n, g_se[slot]);
    g_se[slot] = sound_load(n, path, 1);
    sound_play(n, g_se[slot]);
}

void sound_release_globals(N2 *n) {
    voice_stop(n);
    for (int i = 0; i < 4; i++) {
        if (g_se[i]) sound_free(n, g_se[i]);
        g_se[i] = NULL;
    }
}

static void update_tree(N2 *n, Obj *o) {
    for (int i = 0; i < o->childCount; i++) {
        Obj *c = o->children[i];
        if (c->sound) update_sound(n, c->sound);
        update_tree(n, c);
    }
}

void mixer_update(N2 *n) {
    update_tree(n, &n->root);
    if (g_voice) update_sound(n, g_voice);
    for (int i = 0; i < 4; i++)
        if (g_se[i]) update_sound(n, g_se[i]);
}

void mixer_render(struct Mixer *m, float *out, int frames) {
    memset(out, 0, sizeof(float) * 2 * (size_t)frames);
    pthread_mutex_lock(&m->lock);
    for (int k = 0; k < MAX_CHANNELS; k++) {
        Channel *c = m->ch[k];
        if (!c) continue;
        if (c->gain < 0) c->gain = c->target;
        for (int i = 0; i < frames && c->tail != c->head; i++) {
            c->gain += (c->target - c->gain) * 0.002f;
            out[i * 2] += c->ring[c->tail * 2] * c->gain;
            out[i * 2 + 1] += c->ring[c->tail * 2 + 1] * c->gain;
            c->tail = (c->tail + 1) % RING;
            c->played++;
        }
    }
    pthread_mutex_unlock(&m->lock);
    for (int i = 0; i < frames * 2; i++) out[i] = out[i] > 1 ? 1 : out[i] < -1 ? -1 : out[i];
}

#include "yr.h"
#include "vm.h"
#include <math.h>
#include <pthread.h>

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "stb_vorbis_names.h"
#include "third_party/stb_vorbis.c"

typedef struct Voice {
    char *id;
    char *file;
    u8 *data;
    size_t size;
    stb_vorbis *ogg;
    const u8 *wav;
    u32 wavFrames;
    int wavChannels, wavBits;
    int rate, channels;
    u32 total;
    double pos;
    float buf[4096 * 2];
    int bufFrames, bufAt;
    u32 bufStart;
    bool playing, paused, enabled;
    i64 loop, loops;
    i64 startMs, restartMs, endMs;
    i64 delayFrames;
    double vol, volTarget, volStep;
    int volume, pan, pitch;
    bool stopAfterFade;
} Voice;

struct YSnd {
    pthread_mutex_t lock;
    Voice **v;
    int n, cap;
};

static void voice_free(Voice *v) {
    if (v->ogg) stb_vorbis_close(v->ogg);
    free(v->data);
    free(v->file);
    free(v->id);
    free(v);
}

void yr_snd_open(YR *y) {
    YSnd *s = y->snd = yr_alloc(sizeof(YSnd));
    pthread_mutex_init(&s->lock, NULL);
}

void yr_snd_close(YR *y) {
    YSnd *s = y->snd;
    if (!s) return;
    for (int i = 0; i < s->n; i++) voice_free(s->v[i]);
    free(s->v);
    pthread_mutex_destroy(&s->lock);
    free(s);
    y->snd = NULL;
}

static Voice *find(YSnd *s, const char *id) {
    for (int i = 0; i < s->n; i++)
        if (!strcasecmp(s->v[i]->id, id)) return s->v[i];
    return NULL;
}

static Voice *add(YSnd *s, const char *id) {
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 16;
        s->v = yr_realloc(s->v, sizeof(Voice *) * (size_t)s->cap);
    }
    Voice *v = yr_alloc(sizeof(Voice));
    v->id = yr_strdup(id);
    v->enabled = true;
    v->volume = 256;
    v->vol = v->volTarget = 1;
    v->pan = 256;
    v->pitch = 256;
    v->startMs = v->restartMs = v->endMs = -99999;
    v->loop = 1;
    s->v[s->n++] = v;
    return v;
}

static void unload(Voice *v) {
    if (v->ogg) stb_vorbis_close(v->ogg);
    v->ogg = NULL;
    free(v->data);
    v->data = NULL;
    v->wav = NULL;
    v->total = 0;
    v->bufFrames = v->bufAt = 0;
}

static bool parse_wav(Voice *v) {
    const u8 *d = v->data;
    size_t n = v->size;
    if (n < 12 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) return false;
    size_t o = 12;
    int fmt = 0;
    while (o + 8 <= n) {
        u32 len = yr_le32(d + o + 4);
        if (!memcmp(d + o, "fmt ", 4) && len >= 16) {
            fmt = yr_le16(d + o + 8);
            v->channels = yr_le16(d + o + 10);
            v->rate = (int)yr_le32(d + o + 12);
            v->wavBits = yr_le16(d + o + 22);
        } else if (!memcmp(d + o, "data", 4)) {
            if (fmt != 1 || (v->wavBits != 8 && v->wavBits != 16) || v->channels < 1 || v->channels > 2) return false;
            size_t avail = n - o - 8 < len ? n - o - 8 : len;
            v->wav = d + o + 8;
            v->wavChannels = v->channels;
            v->wavFrames = (u32)(avail / (size_t)(v->channels * v->wavBits / 8));
            v->total = v->wavFrames;
            return true;
        }
        o += 8 + len + (len & 1);
    }
    return false;
}

static const char *const soundExts[] = { ".ogg", ".wav", NULL };

static bool load(YR *y, Voice *v, const char *file) {
    unload(v);
    free(v->file);
    v->file = yr_strdup(file);
    YFile f;
    if (!yr_fs_read(y, file, soundExts, &f)) {
        yr_log(y, "SOUND: file not found: %s\n", file);
        return false;
    }
    v->data = f.data;
    v->size = f.size;
    int err = 0;
    if (f.size >= 4 && !memcmp(f.data, "OggS", 4)) {
        v->ogg = stb_vorbis_open_memory(f.data, (int)f.size, &err, NULL);
        if (v->ogg) {
            stb_vorbis_info info = stb_vorbis_get_info(v->ogg);
            v->rate = (int)info.sample_rate;
            v->channels = info.channels;
            v->total = stb_vorbis_stream_length_in_samples(v->ogg);
            return true;
        }
    } else if (parse_wav(v)) return true;
    yr_log(y, "SOUND: cannot decode %s\n", file);
    unload(v);
    return false;
}

static u32 ms_to_frames(const Voice *v, i64 ms) { return ms <= 0 ? 0 : (u32)((double)ms * v->rate / 1000.0); }

static void seek(Voice *v, u32 frame) {
    if (frame > v->total) frame = v->total;
    v->pos = frame;
    v->bufFrames = v->bufAt = 0;
    v->bufStart = frame;
    if (v->ogg) stb_vorbis_seek(v->ogg, frame);
}

static void start(Voice *v, i64 startMs) {
    v->delayFrames = 0;
    if (startMs < 0 && startMs != -99999) {
        v->delayFrames = (i64)ms_to_frames(v, -startMs);
        seek(v, 0);
    } else seek(v, startMs == -99999 ? 0 : ms_to_frames(v, startMs));
}

static bool fill(Voice *v) {
    v->bufStart = (u32)v->pos;
    v->bufAt = 0;
    if (v->ogg) {
        int got = stb_vorbis_get_samples_float_interleaved(v->ogg, 2, v->buf, 4096 * 2);
        if (v->channels == 1)
            for (int i = 0; i < got; i++) v->buf[i * 2 + 1] = v->buf[i * 2];
        v->bufFrames = got;
        return got > 0;
    }
    u32 at = (u32)v->pos, n = v->wavFrames > at ? v->wavFrames - at : 0;
    if (n > 4096) n = 4096;
    int bps = v->wavBits / 8;
    for (u32 i = 0; i < n; i++)
        for (int ch = 0; ch < 2; ch++) {
            int c = ch < v->wavChannels ? ch : 0;
            const u8 *p = v->wav + ((size_t)(at + i) * (size_t)v->wavChannels + (size_t)c) * (size_t)bps;
            v->buf[i * 2 + ch] = bps == 2 ? (int16_t)(p[0] | p[1] << 8) / 32768.0f : (p[0] - 128) / 128.0f;
        }
    v->bufFrames = (int)n;
    return n > 0;
}

static void finish_or_loop(Voice *v) {
    v->loops++;
    if (v->loop == 0 || v->loops < v->loop) {
        i64 rs = v->restartMs != -99999 ? v->restartMs : v->startMs;
        start(v, rs);
    } else {
        v->playing = false;
    }
}

static void mix(YSnd *s, float *out, int frames, int rate) {
    memset(out, 0, sizeof(float) * 2 * (size_t)frames);
    for (int i = 0; i < s->n; i++) {
        Voice *v = s->v[i];
        if (!v->playing || v->paused || (!v->ogg && !v->wav) || v->rate <= 0) continue;
        double step = (double)v->rate / rate;
        double pl = v->pan <= 256 ? 1 : (512 - v->pan) / 256.0, pr = v->pan >= 256 ? 1 : v->pan / 256.0;
        for (int f = 0; f < frames && v->playing; f++) {
            if (v->delayFrames > 0) { v->delayFrames--; continue; }
            u32 end = v->endMs != -99999 && v->endMs > 0 ? ms_to_frames(v, v->endMs) : v->total;
            if (end > v->total) end = v->total;
            if ((u32)v->pos >= end) { finish_or_loop(v); continue; }
            if (v->bufAt >= v->bufFrames || (u32)v->pos < v->bufStart || (u32)v->pos >= v->bufStart + (u32)v->bufFrames) {
                if (!fill(v)) { finish_or_loop(v); continue; }
            }
            int k = (int)((u32)v->pos - v->bufStart);
            float l = v->buf[k * 2], r = v->buf[k * 2 + 1];
            if (v->volStep != 0) {
                v->vol += v->volStep;
                if ((v->volStep > 0 && v->vol >= v->volTarget) || (v->volStep < 0 && v->vol <= v->volTarget)) {
                    v->vol = v->volTarget;
                    v->volStep = 0;
                    if (v->stopAfterFade) { v->playing = false; v->stopAfterFade = false; }
                }
            }
            float g = v->enabled ? (float)(v->vol * v->vol) : 0;
            out[f * 2] += l * g * (float)pl;
            out[f * 2 + 1] += r * g * (float)pr;
            v->pos += step;
            v->bufAt = (int)((u32)v->pos - v->bufStart);
        }
    }
    for (int i = 0; i < frames * 2; i++) out[i] = out[i] > 1 ? 1 : out[i] < -1 ? -1 : out[i];
}

void yr_snd_mix(YR *y, float *out, int frames) {
    YSnd *s = y->snd;
    if (!s) { memset(out, 0, sizeof(float) * 2 * (size_t)frames); return; }
    pthread_mutex_lock(&s->lock);
    mix(s, out, frames, y->audioRate);
    pthread_mutex_unlock(&s->lock);
}

static void fade_to(YR *y, Voice *v, double target, i64 ms) {
    if (ms <= 0) {
        v->vol = target;
        v->volStep = 0;
        return;
    }
    v->volTarget = target;
    v->volStep = (target - v->vol) / ((double)ms * y->audioRate / 1000.0);
    if (v->volStep == 0) v->vol = target;
}

static double gain(int volume) { return volume <= 0 ? 0 : volume >= 256 ? 1 : volume / 256.0; }

static int sound(YCall *c) {
    YR *y = c->y;
    YSnd *s = y->snd;
    char *id = yc_str(c, "ID");
    if (!id) return YR_NEXT;
    pthread_mutex_lock(&s->lock);
    Voice *v = find(s, id);
    if (!v) v = add(s, id);
    free(id);
    char *file = yc_str(c, "FILE");
    if (file) {
        v->playing = false;
        v->loops = 0;
        load(y, v, file);
        free(file);
    }
    i64 x;
    if (yc_int(c, "E", &x)) v->enabled = x != 0;
    if (yc_int(c, "LOOP", &x)) v->loop = x;
    if (yc_int(c, "START", &x)) v->startMs = x;
    if (yc_int(c, "RESTART", &x)) v->restartMs = x;
    if (yc_int(c, "END", &x)) v->endMs = x;
    if (yc_int(c, "PAN", &x)) v->pan = x < 0 ? 0 : x > 512 ? 512 : (int)x;
    if (yc_int(c, "PITCH", &x)) v->pitch = (int)x;
    if (yc_int(c, "PAUSE", &x)) v->paused = x != 0;
    i64 fade = yc_int_or(c, "FADE", 0);
    bool hasVol = yc_int(c, "VOLUME", &x);
    if (hasVol) v->volume = x < 0 ? 0 : x > 256 ? 256 : (int)x;
    i64 play;
    if (yc_int(c, "PLAY", &play)) {
        if (play) {
            if (!v->playing) {
                v->loops = 0;
                v->paused = false;
                start(v, v->startMs);
                v->playing = v->ogg || v->wav;
                v->stopAfterFade = false;
                if (fade > 0) { v->vol = 0; fade_to(y, v, gain(v->volume), fade); }
                else fade_to(y, v, gain(v->volume), 0);
            } else {
                v->stopAfterFade = false;
                fade_to(y, v, gain(v->volume), fade);
            }
        } else if (v->playing) {
            if (fade > 0) {
                fade_to(y, v, 0, fade);
                v->stopAfterFade = true;
            } else v->playing = false;
        }
    } else if (hasVol) fade_to(y, v, gain(v->volume), fade);
    pthread_mutex_unlock(&s->lock);
    return YR_NEXT;
}

static int soundinfo(YCall *c) {
    YR *y = c->y;
    YSnd *s = y->snd;
    YRef out;
    if (!yc_ref(c, "LET", &out)) return YR_NEXT;
    char *id = yc_str(c, "ID");
    pthread_mutex_lock(&s->lock);
    Voice *v = id ? find(s, id) : NULL;
    free(id);
    YVal r = yv_int(0);
    if (yc_int_or(c, "EXIST", 0)) r = yv_int(v != NULL);
    else if (!v) r = yv_int(0);
    else if (yc_int_or(c, "FILE", 0)) r = yv_str(v->file ? v->file : "");
    else if (yc_int_or(c, "E", 0)) r = yv_int(v->enabled);
    else if (yc_int_or(c, "PLAY", 0)) r = yv_int(v->playing);
    else if (yc_int_or(c, "LOOPCOUNT", 0)) r = yv_int(v->loops);
    else if (yc_int_or(c, "LOOP", 0)) r = yv_int(v->loop);
    else if (yc_int_or(c, "PAUSE", 0)) r = yv_int(v->paused);
    else if (yc_int_or(c, "RESTART", 0)) r = yv_int(v->restartMs);
    else if (yc_int_or(c, "START", 0)) r = yv_int(v->startMs);
    else if (yc_int_or(c, "END", 0)) r = yv_int(v->endMs);
    else if (yc_int_or(c, "VOLUME", 0)) r = yv_int(v->volume);
    else if (yc_int_or(c, "PAN", 0)) r = yv_int(v->pan);
    else if (yc_int_or(c, "PITCH", 0)) r = yv_int(v->pitch);
    else if (yc_int_or(c, "TOTALTIME", 0)) r = yv_int(v->rate ? (i64)v->total * 1000 / v->rate : 0);
    else if (yc_int_or(c, "TIME", 0)) r = yv_int(v->rate ? (i64)(v->pos * 1000 / v->rate) : 0);
    else if (yc_int_or(c, "CHANNEL", 0)) r = yv_int(v->channels);
    else if (yc_int_or(c, "BIT", 0)) r = yv_int(v->wav ? v->wavBits : 16);
    else if (yc_int_or(c, "SAMPLE", 0)) r = yv_int(v->rate);
    else if (yc_int_or(c, "CODEC", 0)) r = yv_int(v->ogg ? 1 : 0);
    pthread_mutex_unlock(&s->lock);
    yr_ref_set(y, c->t, &out, &r);
    yv_free(&r);
    return YR_NEXT;
}

static int soundend(YCall *c) {
    YSnd *s = c->y->snd;
    char *id = yc_str(c, "ID");
    if (!id) return YR_NEXT;
    pthread_mutex_lock(&s->lock);
    for (int i = 0; i < s->n; i++)
        if (!strcasecmp(s->v[i]->id, id)) {
            voice_free(s->v[i]);
            memmove(&s->v[i], &s->v[i + 1], sizeof(Voice *) * (size_t)(s->n - i - 1));
            s->n--;
            break;
        }
    pthread_mutex_unlock(&s->lock);
    free(id);
    return YR_NEXT;
}

int yr_cmd_sound(YCall *c, int id) {
    switch (id) {
    case C_SOUND: return sound(c);
    case C_SOUNDINFO: return soundinfo(c);
    case C_SOUNDEND: return soundend(c);
    }
    YR *y = c->y;
    if (id >= 0 && id < C_COUNT && !y->unknownSeen[id]) {
        y->unknownSeen[id] = true;
        yc_unknown(c, "command");
    }
    YRef out;
    if (yc_ref(c, "LET", &out)) {
        YVal r = yv_int(0);
        yr_ref_set(y, c->t, &out, &r);
    }
    return YR_NEXT;
}

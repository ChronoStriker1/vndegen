// The mixer behind RGSS's Audio module. Ogg Vorbis (stb_vorbis), MP3 (dr_mp3) and WAV (dr_wav) decode as they play;
// MIDI plays through VNDegen's shared vnmidi. Pitch resamples, as RGSS's pitch changes speed and pitch together.
#include "audio.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rgss.h"
#include "vnmidi.h"

// The decoders are compiled into this file with internal linkage or an rgss_ prefix, since other engines in the app
// compile their own copies.
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define stb_vorbis_close rgss_stb_vorbis_close
#define stb_vorbis_decode_memory rgss_stb_vorbis_decode_memory
#define stb_vorbis_get_comment rgss_stb_vorbis_get_comment
#define stb_vorbis_get_error rgss_stb_vorbis_get_error
#define stb_vorbis_get_file_offset rgss_stb_vorbis_get_file_offset
#define stb_vorbis_get_frame_float rgss_stb_vorbis_get_frame_float
#define stb_vorbis_get_frame_short rgss_stb_vorbis_get_frame_short
#define stb_vorbis_get_frame_short_interleaved rgss_stb_vorbis_get_frame_short_interleaved
#define stb_vorbis_get_info rgss_stb_vorbis_get_info
#define stb_vorbis_get_sample_offset rgss_stb_vorbis_get_sample_offset
#define stb_vorbis_get_samples_float rgss_stb_vorbis_get_samples_float
#define stb_vorbis_get_samples_float_interleaved rgss_stb_vorbis_get_samples_float_interleaved
#define stb_vorbis_get_samples_short rgss_stb_vorbis_get_samples_short
#define stb_vorbis_get_samples_short_interleaved rgss_stb_vorbis_get_samples_short_interleaved
#define stb_vorbis_open_memory rgss_stb_vorbis_open_memory
#define stb_vorbis_seek rgss_stb_vorbis_seek
#define stb_vorbis_seek_frame rgss_stb_vorbis_seek_frame
#define stb_vorbis_seek_start rgss_stb_vorbis_seek_start
#define stb_vorbis_stream_length_in_samples rgss_stb_vorbis_stream_length_in_samples
#define stb_vorbis_stream_length_in_seconds rgss_stb_vorbis_stream_length_in_seconds
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../third_party/stb_vorbis.c"
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#define DRMP3_API static
#define DRMP3_PRIVATE static
#include "../third_party/dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#define DRWAV_API static
#define DRWAV_PRIVATE static
#include "../third_party/dr_wav.h"
#pragma GCC diagnostic pop

enum { MAX_SE = 16, CHUNK = 1024 };

typedef enum { KIND_OGG, KIND_MP3, KIND_WAV, KIND_MIDI } Kind;

// One sound being decoded.
typedef struct {
    Kind kind;
    uint8_t *data;
    size_t size;
    stb_vorbis *vorbis;
    drmp3 mp3;
    drwav wav;
    vnmidi_player *midi;
    int channels, rate;
    int64_t frames;                  // length in frames at the source rate, or -1 when unknown
    int64_t position;                // next frame to decode
    int64_t loopStart, loopEnd;      // RPG Maker's LOOPSTART and LOOPLENGTH tags, or the whole sound
} Stream;

// A stream playing on a track or as a sound effect, resampled to the output rate.
typedef struct {
    Stream *stream;
    bool loop, ended;
    double step;       // source frames per output frame
    double phase;      // between the two buffered frames
    float prev[2], next[2];
    float buffer[CHUNK * 2];
    int bufferCount, bufferIndex;
    float gain;        // from the volume
    float fade;        // 1 until a fade starts
    float fadeStep;    // subtracted per output frame while fading
} Voice;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static Voice *tracks[3];
static Voice *effects[MAX_SE];
static bool paused;
static vnmidi_font *soundFont;
// The BGM sits out while an ME plays and picks up where it was once the ME ends.
static bool bgmHeld;
// Graphics.play_movie's sound, resampled to the output rate as it is pulled from the decoder.
static struct {
    vnvideo *video;
    int channels;
    double step, phase;
    float prev[2], next[2];
    float buffer[CHUNK * 2];
    int bufferCount, bufferIndex;
    bool primed;
} movie;

static float gain_for(int volume) {
    if (volume <= 0) return 0;
    if (volume > 100) volume = 100;
    // DirectSound volumes are in decibels; RGSS's 0..100 sounds close to a squared curve.
    float v = volume / 100.0f;
    return v * v;
}

static void ensure_font(void);

// MARK: Streams

static bool has_prefix(const uint8_t *d, size_t n, const char *magic) {
    size_t m = strlen(magic);
    return n >= m && memcmp(d, magic, m) == 0;
}

static void stream_free(Stream *s) {
    if (!s) return;
    if (s->vorbis) stb_vorbis_close(s->vorbis);
    if (s->kind == KIND_MP3) drmp3_uninit(&s->mp3);
    if (s->kind == KIND_WAV) drwav_uninit(&s->wav);
    if (s->midi) vnmidi_player_close(s->midi);
    free(s->data);
    free(s);
}

static int64_t comment_value(stb_vorbis_comment *c, const char *key) {
    size_t k = strlen(key);
    for (int i = 0; i < c->comment_list_length; i++) {
        const char *entry = c->comment_list[i];
        if (strncasecmp(entry, key, k) == 0 && entry[k] == '=') return atoll(entry + k + 1);
    }
    return -1;
}

static Stream *stream_open(uint8_t *data, size_t size, const char *name) {
    Stream *s = calloc(1, sizeof *s);
    if (!s) {
        free(data);
        return NULL;
    }
    s->data = data;
    s->size = size;
    s->frames = -1;
    if (has_prefix(data, size, "OggS")) {
        int error;
        s->vorbis = stb_vorbis_open_memory(data, (int)size, &error, NULL);
        if (!s->vorbis) goto fail;
        stb_vorbis_info info = stb_vorbis_get_info(s->vorbis);
        s->kind = KIND_OGG;
        s->channels = info.channels;
        s->rate = (int)info.sample_rate;
        s->frames = stb_vorbis_stream_length_in_samples(s->vorbis);
        stb_vorbis_comment comments = stb_vorbis_get_comment(s->vorbis);
        int64_t start = comment_value(&comments, "LOOPSTART"), length = comment_value(&comments, "LOOPLENGTH");
        if (start >= 0) {
            s->loopStart = start;
            s->loopEnd = length > 0 ? start + length : s->frames;
        }
    } else if (has_prefix(data, size, "RIFF")) {
        if (!drwav_init_memory(&s->wav, data, size, NULL)) goto fail;
        s->kind = KIND_WAV;
        s->channels = s->wav.channels;
        s->rate = (int)s->wav.sampleRate;
        s->frames = (int64_t)s->wav.totalPCMFrameCount;
    } else if (has_prefix(data, size, "MThd")) {
        ensure_font();
        char error[256] = "";
        s->midi = vnmidi_player_open(soundFont, data, size, AUDIO_RATE, error, sizeof error);
        if (!s->midi) {
            rgss_log("audio: %s: %s", name, error);
            goto fail;
        }
        s->kind = KIND_MIDI;
        s->channels = 2;
        s->rate = AUDIO_RATE;
    } else {
        if (!drmp3_init_memory(&s->mp3, data, size, NULL)) goto fail;
        s->kind = KIND_MP3;
        s->channels = (int)s->mp3.channels;
        s->rate = (int)s->mp3.sampleRate;
    }
    if (s->channels < 1 || s->rate <= 0) goto fail;
    if (s->loopEnd <= s->loopStart) {
        s->loopStart = 0;
        s->loopEnd = s->frames;
    }
    return s;
fail:
    rgss_log("audio: could not decode %s", name);
    stream_free(s);
    return NULL;
}

static bool stream_seek(Stream *s, int64_t frame) {
    if (frame < 0) frame = 0;
    bool ok = false;
    switch (s->kind) {
    case KIND_OGG: ok = stb_vorbis_seek(s->vorbis, (unsigned)frame); break;
    case KIND_MP3: ok = drmp3_seek_to_pcm_frame(&s->mp3, (drmp3_uint64)frame); break;
    case KIND_WAV: ok = drwav_seek_to_pcm_frame(&s->wav, (drwav_uint64)frame); break;
    case KIND_MIDI:
        vnmidi_player_seek(s->midi, (int)(frame * 1000 / AUDIO_RATE));
        ok = true;
        break;
    }
    if (ok) s->position = frame;
    return ok;
}

// Decodes up to `frames` stereo frames at the source rate. Returns fewer at the end of the sound.
static int stream_read(Stream *s, float *out, int frames) {
    float temp[CHUNK * 8];
    int got = 0;
    if (s->kind == KIND_MIDI) return vnmidi_player_render(s->midi, out, frames);
    int channels = s->channels > 8 ? 8 : s->channels;
    if (frames > CHUNK) frames = CHUNK;
    switch (s->kind) {
    case KIND_OGG: got = stb_vorbis_get_samples_float_interleaved(s->vorbis, channels, temp, frames * channels); break;
    case KIND_MP3: got = (int)drmp3_read_pcm_frames_f32(&s->mp3, (drmp3_uint64)frames, temp); break;
    case KIND_WAV: got = (int)drwav_read_pcm_frames_f32(&s->wav, (drwav_uint64)frames, temp); break;
    default: break;
    }
    for (int i = 0; i < got; i++) {
        float l = temp[i * s->channels], r = s->channels > 1 ? temp[i * s->channels + 1] : l;
        out[i * 2] = l;
        out[i * 2 + 1] = r;
    }
    s->position += got;
    return got;
}

// MARK: Voices

static Voice *voice_new(Stream *s, bool loop, int volume, int pitch) {
    Voice *v = calloc(1, sizeof *v);
    if (!v) {
        stream_free(s);
        return NULL;
    }
    v->stream = s;
    v->loop = loop;
    v->gain = gain_for(volume);
    v->fade = 1;
    if (pitch <= 0) pitch = 100;
    if (s->kind == KIND_MIDI) {
        vnmidi_player_set_tempo(s->midi, pitch / 100.0f);
        vnmidi_player_set_loop(s->midi, loop, VNMIDI_LOOP_MARKER);
        v->step = 1;
    } else {
        v->step = (double)s->rate / AUDIO_RATE * pitch / 100.0;
    }
    return v;
}

static void voice_free(Voice *v) {
    if (!v) return;
    stream_free(v->stream);
    free(v);
}

// The next source frame, looping or ending as the voice says.
static bool voice_pull(Voice *v, float frame[2]) {
    Stream *s = v->stream;
    if (v->bufferIndex >= v->bufferCount) {
        int want = CHUNK;
        if (v->loop && s->loopEnd > 0 && s->position + want > s->loopEnd) want = (int)(s->loopEnd - s->position);
        int got = want > 0 ? stream_read(s, v->buffer, want) : 0;
        if (got <= 0) {
            if (!v->loop || s->kind == KIND_MIDI || !stream_seek(s, s->loopStart)) return false;
            got = stream_read(s, v->buffer, CHUNK);
            if (got <= 0) return false;
        }
        v->bufferCount = got;
        v->bufferIndex = 0;
    }
    frame[0] = v->buffer[v->bufferIndex * 2];
    frame[1] = v->buffer[v->bufferIndex * 2 + 1];
    v->bufferIndex++;
    return true;
}

// Reads the first two frames, so mixing can interpolate between them. Runs before the voice is shared.
static void voice_prime(Voice *v) {
    if (v->stream->kind == KIND_MIDI) return;
    if (!voice_pull(v, v->prev)) {
        v->ended = true;
        return;
    }
    if (!voice_pull(v, v->next)) {
        v->next[0] = v->prev[0];
        v->next[1] = v->prev[1];
    }
}

// Adds the voice's next frames to `out`. False once it has ended.
static bool voice_mix(Voice *v, float *out, int frames) {
    if (v->ended) return false;
    if (v->stream->kind == KIND_MIDI) {
        float temp[CHUNK * 2];
        int done = 0;
        while (done < frames) {
            int n = frames - done > CHUNK ? CHUNK : frames - done;
            int got = vnmidi_player_render(v->stream->midi, temp, n);
            for (int i = 0; i < got; i++) {
                float g = v->gain * v->fade;
                out[(done + i) * 2] += temp[i * 2] * g;
                out[(done + i) * 2 + 1] += temp[i * 2 + 1] * g;
                if (v->fadeStep > 0 && (v->fade -= v->fadeStep) <= 0) {
                    v->ended = true;
                    return false;
                }
            }
            if (got < n) {
                v->ended = true;
                return false;
            }
            done += n;
        }
        return true;
    }
    for (int i = 0; i < frames; i++) {
        float t = (float)v->phase, g = v->gain * v->fade;
        out[i * 2] += (v->prev[0] + (v->next[0] - v->prev[0]) * t) * g;
        out[i * 2 + 1] += (v->prev[1] + (v->next[1] - v->prev[1]) * t) * g;
        v->phase += v->step;
        while (v->phase >= 1) {
            v->prev[0] = v->next[0];
            v->prev[1] = v->next[1];
            if (!voice_pull(v, v->next)) {
                v->ended = true;
                return false;
            }
            v->phase -= 1;
        }
        if (v->fadeStep > 0 && (v->fade -= v->fadeStep) <= 0) {
            v->ended = true;
            return false;
        }
    }
    return true;
}

// MARK: Movies

// The movie's next source frame, or false while the decoder has none ready.
static bool movie_pull(float frame[2]) {
    if (movie.bufferIndex >= movie.bufferCount) {
        float temp[CHUNK * 2];
        int got = (int)vnvideo_read_audio(movie.video, temp, CHUNK);
        if (got <= 0) return false;
        for (int i = 0; i < got; i++) {
            movie.buffer[i * 2] = temp[i * movie.channels];
            movie.buffer[i * 2 + 1] = temp[i * movie.channels + movie.channels - 1];
        }
        movie.bufferCount = got;
        movie.bufferIndex = 0;
    }
    frame[0] = movie.buffer[movie.bufferIndex * 2];
    frame[1] = movie.buffer[movie.bufferIndex * 2 + 1];
    movie.bufferIndex++;
    return true;
}

// Adds the movie's sound to `out`. When the decoder falls behind, the rest of this block stays silent and the movie's
// clock, which counts the sound handed out, waits with it.
static void movie_mix(float *out, int frames) {
    if (!movie.primed) {
        if (!movie_pull(movie.prev)) return;
        movie.next[0] = movie.prev[0];
        movie.next[1] = movie.prev[1];
        movie.primed = true;
    }
    for (int i = 0; i < frames; i++) {
        float t = (float)movie.phase;
        out[i * 2] += movie.prev[0] + (movie.next[0] - movie.prev[0]) * t;
        out[i * 2 + 1] += movie.prev[1] + (movie.next[1] - movie.prev[1]) * t;
        movie.phase += movie.step;
        while (movie.phase >= 1) {
            float frame[2];
            if (!movie_pull(frame)) {
                movie.phase = 1;
                return;
            }
            movie.prev[0] = movie.next[0];
            movie.prev[1] = movie.next[1];
            movie.next[0] = frame[0];
            movie.next[1] = frame[1];
            movie.phase -= 1;
        }
    }
}

void audio_set_movie(vnvideo *video) {
    vnvideo_info info = {0};
    if (video) vnvideo_get_info(video, &info);
    pthread_mutex_lock(&lock);
    memset(&movie, 0, sizeof movie);
    if (video && info.audio_rate > 0 && info.audio_channels > 0) {
        movie.video = video;
        movie.channels = info.audio_channels;
        movie.step = (double)info.audio_rate / AUDIO_RATE;
    }
    pthread_mutex_unlock(&lock);
}

// MARK: The host's side

// The SoundFont the host asked for, loaded when the game first plays MIDI, on the game thread, since decoding it takes
// a moment. The player may have chosen another in Settings since the last game.
static char wantedFont[2048], wantedFallback[2048], loadedFonts[4100];

static void ensure_font(void) {
    char wanted[sizeof loadedFonts];
    snprintf(wanted, sizeof wanted, "%s|%s", wantedFont, wantedFallback);
    if (!strcmp(wanted, loadedFonts)) return;
    vnmidi_font_free(soundFont);
    soundFont = NULL;
    if (wantedFont[0] || wantedFallback[0]) {
        const char *path = wantedFont[0] ? wantedFont : wantedFallback;
        const char *fallback = wantedFont[0] && wantedFallback[0] ? wantedFallback : NULL;
        char error[512] = "";
        soundFont = vnmidi_font_load_fallback(path, fallback, error, sizeof error);
        if (error[0]) rgss_log("audio: SoundFont: %s", error);
    }
    snprintf(loadedFonts, sizeof loadedFonts, "%s", wanted);
}

void audio_start(const char *path, const char *fallback) {
    audio_stop();
    snprintf(wantedFont, sizeof wantedFont, "%s", path ? path : "");
    snprintf(wantedFallback, sizeof wantedFallback, "%s", fallback ? fallback : "");
    pthread_mutex_lock(&lock);
    paused = false;
    pthread_mutex_unlock(&lock);
}

void audio_stop(void) {
    Voice *old[3 + MAX_SE];
    pthread_mutex_lock(&lock);
    for (int i = 0; i < 3; i++) {
        old[i] = tracks[i];
        tracks[i] = NULL;
    }
    for (int i = 0; i < MAX_SE; i++) {
        old[3 + i] = effects[i];
        effects[i] = NULL;
    }
    bgmHeld = false;
    memset(&movie, 0, sizeof movie);
    pthread_mutex_unlock(&lock);
    for (int i = 0; i < 3 + MAX_SE; i++) voice_free(old[i]);
}

void audio_set_paused(bool p) {
    pthread_mutex_lock(&lock);
    paused = p;
    pthread_mutex_unlock(&lock);
}

void audio_render(float *out, int frames) {
    memset(out, 0, sizeof(float) * 2 * (size_t)frames);
    pthread_mutex_lock(&lock);
    if (!paused) {
        for (int t = 0; t < 3; t++) {
            Voice *v = tracks[t];
            if (!v || (t == TRACK_BGM && bgmHeld)) continue;
            if (!voice_mix(v, out, frames) && t == TRACK_ME) bgmHeld = false;
        }
        for (int i = 0; i < MAX_SE; i++) if (effects[i]) voice_mix(effects[i], out, frames);
        if (movie.video) movie_mix(out, frames);
    }
    pthread_mutex_unlock(&lock);
    for (int i = 0; i < frames * 2; i++) {
        float x = out[i];
        out[i] = x > 1 ? 1 : x < -1 ? -1 : x;
    }
}

// MARK: The game's side

// Swaps a voice in under the lock and frees what it replaced outside it, since freeing can take a moment.
static void put_track(AudioTrack track, Voice *v) {
    pthread_mutex_lock(&lock);
    Voice *old = tracks[track];
    tracks[track] = v;
    if (track == TRACK_ME) bgmHeld = v != NULL;
    pthread_mutex_unlock(&lock);
    voice_free(old);
}

bool audio_play(AudioTrack track, uint8_t *data, size_t size, const char *name, int volume, int pitch, double position) {
    Stream *s = stream_open(data, size, name);
    if (!s) return false;
    Voice *v = voice_new(s, track != TRACK_ME, volume, pitch);
    if (!v) return false;
    if (position > 0) stream_seek(s, (int64_t)position);
    voice_prime(v);
    put_track(track, v);
    return true;
}

void audio_stop_track(AudioTrack track) { put_track(track, NULL); }

void audio_fade_track(AudioTrack track, int ms) {
    pthread_mutex_lock(&lock);
    Voice *v = tracks[track];
    if (v) {
        int frames = ms > 0 ? (int)((int64_t)ms * AUDIO_RATE / 1000) : 1;
        v->fadeStep = v->fade / (float)frames;
    }
    pthread_mutex_unlock(&lock);
}

long audio_position(AudioTrack track) {
    pthread_mutex_lock(&lock);
    Voice *v = tracks[track];
    long position = v ? (long)v->stream->position : 0;
    pthread_mutex_unlock(&lock);
    return position;
}

bool audio_play_se(uint8_t *data, size_t size, const char *name, int volume, int pitch) {
    Stream *s = stream_open(data, size, name);
    if (!s) return false;
    Voice *v = voice_new(s, false, volume, pitch);
    if (!v) return false;
    voice_prime(v);
    Voice *old = NULL;
    pthread_mutex_lock(&lock);
    int slot = -1;
    for (int i = 0; i < MAX_SE && slot < 0; i++) if (!effects[i] || effects[i]->ended) slot = i;
    // With every slot busy, the oldest effect gives way.
    if (slot < 0) slot = 0;
    old = effects[slot];
    effects[slot] = v;
    pthread_mutex_unlock(&lock);
    voice_free(old);
    return true;
}

void audio_stop_se(void) {
    Voice *old[MAX_SE];
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_SE; i++) {
        old[i] = effects[i];
        effects[i] = NULL;
    }
    pthread_mutex_unlock(&lock);
    for (int i = 0; i < MAX_SE; i++) voice_free(old[i]);
}

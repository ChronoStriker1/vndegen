// Sound: CD music (the MP3 files _inmm.ini lists), WAV effects from the WA archive, and MIDI music from the MA
// archive through the shared vnmidi player. vns3_audio mixes them on the audio thread; a mutex keeps the
// interpreter's changes out of a mix in progress.
#include "s3.h"
#include "vnmidi.h"
#include <math.h>
#include <pthread.h>

#define DR_MP3_IMPLEMENTATION
#define DRMP3_API static
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "third_party/dr_mp3.h"
#pragma GCC diagnostic pop

#define WAV_CHANNELS 8
#define CHUNK 1024

typedef struct Wav {
    float *pcm;          // interleaved stereo at the source rate
    size_t frames;
    double pos, step;    // source frames per output frame
    int loops;           // 0 forever
    bool on;
} Wav;

typedef struct Music {
    u8 *file;
    drmp3 *mp3;          // on the heap: dr_mp3's memory reader points into the decoder, so it must never move
    bool open, on;
    int loops;           // 0 forever, otherwise plays this many times
    double step, frac;
    float cur[2], next[2];
    u64 played;          // output frames played, for the position
} Music;

typedef struct S3Audio {
    pthread_mutex_t lock;
    int rate;
    bool paused;
    Wav wav[WAV_CHANNELS];
    Music music;
    vnmidi_font *font;
    vnmidi_player *midi;
    float scratch[CHUNK * 2];
    char **tracks;       // _inmm.ini: tracks[k] is the file for CD track k + 2
    int trackCount;
} S3Audio;

static void read_inmm(S3 *s, S3Audio *a) {
    char *path = s3_find_file(s->gameDir, "_inmm.ini");
    size_t n;
    char *d = path ? (char *)s3_read_file(path, &n) : NULL;
    free(path);
    if (!d) return;
    for (char *save = NULL, *line = strtok_r(d, "\r\n", &save); line; line = strtok_r(NULL, "\r\n", &save)) {
        while (*line == ' ' || *line == '\t') line++;
        if (!*line || *line == ';') continue;
        a->tracks = realloc(a->tracks, sizeof(char *) * (size_t)(a->trackCount + 1));
        a->tracks[a->trackCount++] = s3_strdup(line);
    }
    free(d);
}

void s3_audio_init(S3 *s) {
    S3Audio *a = s3_alloc(sizeof *a);
    pthread_mutex_init(&a->lock, NULL);
    a->rate = s->cfg.audioRate > 0 ? s->cfg.audioRate : 44100;
    if (s->cfg.soundFontPath) {
        char err[256] = "";
        a->font = vnmidi_font_load_fallback(s->cfg.soundFontPath, s->cfg.fallbackSoundFontPath, err, sizeof err);
        if (err[0]) s3_log(s, "SoundFont: %s", err);
    }
    read_inmm(s, a);
    s->audio = a;
}

static void wav_free(Wav *w) { free(w->pcm); memset(w, 0, sizeof *w); }

static void music_close(Music *m) {
    if (m->open) drmp3_uninit(m->mp3);
    free(m->mp3);
    free(m->file);
    memset(m, 0, sizeof *m);
}

void s3_audio_free(S3 *s) {
    S3Audio *a = s->audio;
    if (!a) return;
    for (int i = 0; i < WAV_CHANNELS; i++) wav_free(&a->wav[i]);
    music_close(&a->music);
    if (a->midi) vnmidi_player_close(a->midi);
    if (a->font) vnmidi_font_free(a->font);
    for (int i = 0; i < a->trackCount; i++) free(a->tracks[i]);
    free(a->tracks);
    pthread_mutex_destroy(&a->lock);
    free(a);
    s->audio = NULL;
}

// ---- WAV ----

static bool decode_wav(const u8 *d, size_t n, Wav *w, int outRate) {
    if (n < 12 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) return false;
    int fmt = 0, ch = 0, rate = 0, bits = 0;
    const u8 *data = NULL;
    size_t dataLen = 0;
    for (size_t p = 12; p + 8 <= n;) {
        u32 len = s3_le32(d + p + 4);
        const u8 *c = d + p + 8;
        if (p + 8 + len > n) len = (u32)(n - p - 8);
        if (!memcmp(d + p, "fmt ", 4) && len >= 16) { fmt = s3_le16(c); ch = s3_le16(c + 2); rate = (int)s3_le32(c + 4); bits = s3_le16(c + 14); }
        else if (!memcmp(d + p, "data", 4)) { data = c; dataLen = len; }
        p += 8 + len + (len & 1);
    }
    if (fmt != 1 || !data || (ch != 1 && ch != 2) || (bits != 8 && bits != 16) || rate <= 0) return false;
    size_t frames = dataLen / (size_t)(ch * bits / 8);
    w->pcm = malloc(frames * 2 * sizeof(float) + 8);
    if (!w->pcm) return false;
    for (size_t i = 0; i < frames; i++) {
        for (int c = 0; c < 2; c++) {
            int src = ch == 1 ? 0 : c;
            float v = bits == 8 ? (data[i * ch + src] - 128) / 128.0f : (int16_t)s3_le16(data + (i * ch + src) * 2) / 32768.0f;
            w->pcm[i * 2 + c] = v;
        }
    }
    w->frames = frames;
    w->step = (double)rate / outRate;
    w->pos = 0;
    return frames > 0;
}

void s3_wav_play(S3 *s, int channel, const u8 *data, size_t size, int loops) {
    S3Audio *a = s->audio;
    if (!a || channel < 0 || channel >= WAV_CHANNELS) return;
    Wav w = {0};
    if (!decode_wav(data, size, &w, a->rate)) { s3_log(s, "unplayable WAV"); return; }
    w.loops = loops;
    w.on = true;
    pthread_mutex_lock(&a->lock);
    Wav old = a->wav[channel];
    a->wav[channel] = w;
    pthread_mutex_unlock(&a->lock);
    free(old.pcm);
}

void s3_wav_stop(S3 *s, int channel) {
    S3Audio *a = s->audio;
    if (!a || channel < 0 || channel >= WAV_CHANNELS) return;
    pthread_mutex_lock(&a->lock);
    a->wav[channel].on = false;
    pthread_mutex_unlock(&a->lock);
}

bool s3_wav_playing(S3 *s, int channel) {
    S3Audio *a = s->audio;
    if (!a || channel < 0 || channel >= WAV_CHANNELS) return false;
    pthread_mutex_lock(&a->lock);
    bool on = a->wav[channel].on;
    pthread_mutex_unlock(&a->lock);
    return on;
}

// ---- CD music ----

void s3_music_play(S3 *s, int track, int loops) {
    S3Audio *a = s->audio;
    if (!a) return;
    s3_music_stop(s);
    s->cdTrack = track;
    if (track < 2 || track - 2 >= a->trackCount) { s3_log(s, "CD track %d has no file", track); return; }
    char *path = s3_find_file(s->gameDir, a->tracks[track - 2]);
    size_t n;
    u8 *d = path ? s3_read_file(path, &n) : NULL;
    free(path);
    if (!d) { s3_log(s, "CD track %d file missing: %s", track, a->tracks[track - 2]); return; }
    Music m = {0};
    m.file = d;
    m.mp3 = s3_alloc(sizeof *m.mp3);
    if (!drmp3_init_memory(m.mp3, d, n, NULL)) { free(m.mp3); free(d); s3_log(s, "CD track %d is not MP3", track); return; }
    m.open = true;
    m.on = true;
    m.loops = loops;
    m.step = (double)m.mp3->sampleRate / a->rate;
    pthread_mutex_lock(&a->lock);
    a->music = m;
    pthread_mutex_unlock(&a->lock);
}

void s3_music_stop(S3 *s) {
    S3Audio *a = s->audio;
    if (!a) return;
    pthread_mutex_lock(&a->lock);
    Music old = a->music;
    memset(&a->music, 0, sizeof a->music);
    pthread_mutex_unlock(&a->lock);
    music_close(&old);
}

int s3_music_position(S3 *s) {
    S3Audio *a = s->audio;
    if (!a) return -1;
    pthread_mutex_lock(&a->lock);
    int ms = a->music.on ? (int)(a->music.played * 1000 / (u64)a->rate) : -1;
    pthread_mutex_unlock(&a->lock);
    return ms;
}

// Reads one source frame, looping or ending at the end of the file.
static bool music_frame(Music *m, float out[2]) {
    float f[2];
    for (int tries = 0; tries < 2; tries++) {
        if (drmp3_read_pcm_frames_f32(m->mp3, 1, f) == 1) {
            if (m->mp3->channels == 1) f[1] = f[0];
            out[0] = f[0]; out[1] = f[1];
            return true;
        }
        if (m->loops == 1) return false;
        if (m->loops > 1) m->loops--;
        drmp3_seek_to_pcm_frame(m->mp3, 0);
    }
    return false;
}

// ---- MIDI ----

void s3_midi_play(S3 *s, int n, int loops) {
    S3Audio *a = s->audio;
    if (!a) return;
    s3_midi_stop(s);
    S3Blob b;
    if (!s3_archive_get(&s->ma, (u32)n, &b, NULL)) { s3_log(s, "MIDI %d missing", n); return; }
    char err[256] = "";
    vnmidi_player *p = vnmidi_player_open(a->font, b.data, b.size, a->rate, err, sizeof err);
    if (!p) { s3_log(s, "MIDI %d: %s", n, err); return; }
    vnmidi_player_set_loop(p, loops != 1, 0);
    pthread_mutex_lock(&a->lock);
    a->midi = p;
    pthread_mutex_unlock(&a->lock);
    s->midiPlaying = n;
}

// The MIDI number playing, or 0 once a one-shot piece has ended or nothing plays.
int s3_midi_playing(S3 *s) {
    S3Audio *a = s->audio;
    if (!a || !s->midiPlaying) return 0;
    pthread_mutex_lock(&a->lock);
    bool on = a->midi && vnmidi_player_playing(a->midi);
    pthread_mutex_unlock(&a->lock);
    return on ? s->midiPlaying : 0;
}

void s3_midi_stop(S3 *s) {
    S3Audio *a = s->audio;
    if (!a) return;
    pthread_mutex_lock(&a->lock);
    vnmidi_player *p = a->midi;
    a->midi = NULL;
    pthread_mutex_unlock(&a->lock);
    if (p) vnmidi_player_close(p);
    s->midiPlaying = 0;
}

// ---- mixing ----

static void mix(S3Audio *a, float *out, int frames) {
    memset(out, 0, sizeof(float) * 2 * (size_t)frames);
    for (int c = 0; c < WAV_CHANNELS; c++) {
        Wav *w = &a->wav[c];
        if (!w->on) continue;
        for (int i = 0; i < frames; i++) {
            size_t k = (size_t)w->pos;
            if (k + 1 >= w->frames) {
                if (w->loops == 1) { w->on = false; break; }
                if (w->loops > 1) w->loops--;
                w->pos -= (double)(w->frames - 1);
                if (w->pos < 0) w->pos = 0;
                k = (size_t)w->pos;
                if (k + 1 >= w->frames) { w->on = false; break; }
            }
            float t = (float)(w->pos - (double)k);
            out[i * 2] += w->pcm[k * 2] + (w->pcm[k * 2 + 2] - w->pcm[k * 2]) * t;
            out[i * 2 + 1] += w->pcm[k * 2 + 1] + (w->pcm[k * 2 + 3] - w->pcm[k * 2 + 1]) * t;
            w->pos += w->step;
        }
    }
    Music *m = &a->music;
    if (m->on) {
        if (m->played == 0 && m->frac == 0) { if (!music_frame(m, m->cur) || !music_frame(m, m->next)) m->on = false; }
        for (int i = 0; i < frames && m->on; i++) {
            float t = (float)m->frac;
            out[i * 2] += m->cur[0] + (m->next[0] - m->cur[0]) * t;
            out[i * 2 + 1] += m->cur[1] + (m->next[1] - m->cur[1]) * t;
            m->played++;
            m->frac += m->step;
            while (m->frac >= 1.0 && m->on) {
                m->frac -= 1.0;
                m->cur[0] = m->next[0]; m->cur[1] = m->next[1];
                if (!music_frame(m, m->next)) m->on = false;
            }
        }
    }
    if (a->midi) {
        for (int done = 0; done < frames;) {
            int n = frames - done < CHUNK ? frames - done : CHUNK;
            int got = vnmidi_player_render(a->midi, a->scratch, n);
            for (int i = 0; i < got * 2; i++) out[done * 2 + i] += a->scratch[i];
            done += n;
        }
    }
    for (int i = 0; i < frames * 2; i++) {
        float v = out[i];
        out[i] = v > 1 ? 1 : v < -1 ? -1 : v;
    }
}

void vns3_audio(VNS3 *s3, float *out, int frames) {
    S3 *s = (S3 *)s3;
    S3Audio *a = s->audio;
    if (!a || frames <= 0) { if (frames > 0) memset(out, 0, sizeof(float) * 2 * (size_t)frames); return; }
    pthread_mutex_lock(&a->lock);
    if (a->paused) memset(out, 0, sizeof(float) * 2 * (size_t)frames);
    else mix(a, out, frames);
    pthread_mutex_unlock(&a->lock);
}

void s3_audio_pause(S3 *s, bool paused) {
    S3Audio *a = s->audio;
    if (!a) return;
    pthread_mutex_lock(&a->lock);
    a->paused = paused;
    pthread_mutex_unlock(&a->lock);
}

#include "vnmidi.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/vnmidi_libs.h"

enum {
    CHANNELS = 16,
    DRUMS = 9,
    MAX_VOICES = 128,
    // A finished song ends once its last notes have faded below QUIET_LEVEL for QUIET_MS, or after TAIL_LIMIT_MS.
    // TinySoundFont keeps a releasing voice until its envelope ends, which can be seconds after it is inaudible.
    QUIET_MS = 200,
    TAIL_LIMIT_MS = 10000,
    REQUEST_NONE = -1,
    REQUEST_STOP = -2,
};

// At 0 dB a General MIDI song through TinySoundFont is about four times as loud as typical game music in Ogg or MP3,
// and loud passages clip.
static const float output_gain_db = -12.0f;
static const float quiet_level = 1e-4f;  // -80 dBFS

// TinySoundFont's count of the players sharing a font's samples isn't atomic, so copying and closing take this lock.
static pthread_mutex_t share_lock = PTHREAD_MUTEX_INITIALIZER;

struct vnmidi_font {
    tsf *base;
};

enum state { PLAYING, ENDING, STOPPED, DONE };

struct vnmidi_player {
    tsf *synth;  // NULL without a font
    tml_message *song;
    int rate, length_ms, marker_ms;
    char error[160];

    // Set from any thread, read by render.
    _Atomic float volume, tempo;
    atomic_int transpose, loop, loop_start_ms;
    atomic_int request;  // a seek target in ms, REQUEST_STOP, or REQUEST_NONE

    // Written by render, read from any thread.
    atomic_int position_ms, playing;

    // Render's own.
    enum state state;
    const tml_message *next;
    double ms;           // song time
    double tail_ms;      // how long the song has been ending
    double quiet_ms;     // how long its output has stayed below quiet_level since then
    float gain;          // the volume the last render call ended at
    unsigned char sounding[CHANNELS][128];  // the key each sounding note started on, plus 1, by its key in the file
};

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || !size) return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, size, format, args);
    va_end(args);
}

// TinySoundFont's and TinyMidiLoader's own file and memory loaders call their readers through function pointers of
// another type, which is undefined behavior that UBSan reports. These readers have the type the loaders call.
static int file_read(void *file, void *ptr, unsigned size) { return (int)fread(ptr, 1, size, file); }
static int file_skip(void *file, unsigned count) { return !fseek(file, count, SEEK_CUR); }

struct memory {
    const unsigned char *data;
    size_t size, pos;
};

static int memory_read(void *stream, void *ptr, unsigned size) {
    struct memory *m = stream;
    if (size > m->size - m->pos) size = (unsigned)(m->size - m->pos);
    if (size) memcpy(ptr, m->data + m->pos, size);  // TinyMidiLoader reads an empty track into a NULL buffer
    m->pos += size;
    return (int)size;
}

vnmidi_font *vnmidi_font_load(const char *path, char *error, size_t error_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        set_error(error, error_size, "can't open SoundFont %s: %s", path, strerror(errno));
        return NULL;
    }
    struct tsf_stream stream = {file, file_read, file_skip};
    tsf *base = tsf_load(&stream);
    fclose(file);
    if (!base) {
        set_error(error, error_size, "%s isn't a SoundFont vnmidi can read", path);
        return NULL;
    }
    vnmidi_font *font = calloc(1, sizeof *font);
    if (!font) {
        tsf_close(base);
        set_error(error, error_size, "out of memory loading SoundFont %s", path);
        return NULL;
    }
    font->base = base;
    return font;
}

void vnmidi_font_free(vnmidi_font *font) {
    if (!font) return;
    pthread_mutex_lock(&share_lock);
    tsf_close(font->base);
    pthread_mutex_unlock(&share_lock);
    free(font);
}

// Ends every sound within about 10 ms: a quick release rather than a cut, which would click.
static void silence(vnmidi_player *p) {
    for (int c = 0; c < CHANNELS; c++) {
        tsf_channel_set_sustain(p->synth, c, 0);
        tsf_channel_sounds_off_all(p->synth, c);
    }
    memset(p->sounding, 0, sizeof p->sounding);
}

// Programs, controllers, pitch wheels, and the sustain pedal back to their defaults, for a start from silence.
// Allocates nothing, since open created every channel.
static void reset_channels(vnmidi_player *p) {
    for (int c = 0; c < CHANNELS; c++) {
        tsf_channel_set_sustain(p->synth, c, 0);
        tsf_channel_midi_control(p->synth, c, 121, 0);  // reset all controllers
        tsf_channel_set_pitchwheel(p->synth, c, 8192);
        tsf_channel_set_presetnumber(p->synth, c, 0, c == DRUMS);
    }
}

static void note_off(vnmidi_player *p, int channel, int key) {
    int played = p->sounding[channel][key];
    if (!played) return;
    p->sounding[channel][key] = 0;
    tsf_channel_note_off(p->synth, channel, played - 1);
}

// Plays one event. Seeking passes `notes` 0 to set up the channels without sounding the notes before the target.
static void apply(vnmidi_player *p, const tml_message *m, int notes) {
    if (!p->synth) return;
    int channel = m->channel & 15, key = m->key & 127, value = m->velocity & 127;
    switch (m->type) {
    case TML_NOTE_ON:
        if (!notes) break;
        if (!value) {
            note_off(p, channel, key);
            break;
        }
        note_off(p, channel, key);
        {
            int played = key;
            if (channel != DRUMS) {
                played += atomic_load_explicit(&p->transpose, memory_order_relaxed);
                played = played < 0 ? 0 : played > 127 ? 127 : played;
            }
            p->sounding[channel][key] = (unsigned char)(played + 1);
            tsf_channel_note_on(p->synth, channel, played, value / 127.0f);
        }
        break;
    case TML_NOTE_OFF:
        if (notes) note_off(p, channel, key);
        break;
    case TML_PROGRAM_CHANGE:
        tsf_channel_set_presetnumber(p->synth, channel, m->program & 127, channel == DRUMS);
        break;
    case TML_CONTROL_CHANGE:
        tsf_channel_midi_control(p->synth, channel, m->control & 127, value);
        break;
    case TML_PITCH_BEND:
        tsf_channel_set_pitchwheel(p->synth, channel, m->pitch_bend & 0x3fff);
        break;
    }
}

// Plays on from `target`. Sounding notes are released, so their tails carry on, and the channels are set up as they
// are at `target` by playing the events before it without their notes. Channels keep what the song set after that
// point, as sequencers do across a loop: resetting them would change the level and pitch of the tails at once, which
// clicks. A start from silence resets them first.
static void jump(vnmidi_player *p, double target) {
    int fresh = p->state == STOPPED || p->state == DONE;
    if (p->synth) {
        if (fresh) {
            reset_channels(p);
        } else {
            for (int c = 0; c < CHANNELS; c++) tsf_channel_set_sustain(p->synth, c, 0);
            tsf_note_off_all(p->synth);
        }
        memset(p->sounding, 0, sizeof p->sounding);
    }
    if (fresh || target < p->ms) p->next = p->song;
    for (; p->next && p->next->time < target; p->next = p->next->next) apply(p, p->next, 0);
    p->ms = target;
    p->tail_ms = p->quiet_ms = 0;
    p->state = PLAYING;
}

vnmidi_player *vnmidi_player_open(vnmidi_font *font, const void *data, size_t size, int sample_rate, char *error,
                                  size_t error_size) {
    if (sample_rate <= 0) {
        set_error(error, error_size, "invalid sample rate %d", sample_rate);
        return NULL;
    }
    if (size < 14 || size > 0x7fffffff || memcmp(data, "MThd", 4) != 0) {
        set_error(error, error_size, "not a Standard MIDI File");
        return NULL;
    }
    vnmidi_player *p = calloc(1, sizeof *p);
    if (!p) {
        set_error(error, error_size, "out of memory opening a MIDI file");
        return NULL;
    }
    // TinyMidiLoader drops a track that runs past the end of the data, so a truncated file would lose its last track.
    // A copy whose last chunk length stops at the end of the data keeps its events. A length of 2 GB or more overflows
    // the int TinyMidiLoader reads it into, which is undefined behavior, and makes it stop at that chunk, so the data
    // ends before such a chunk instead.
    const unsigned char *bytes = data;
    unsigned char *fixed = NULL;
    for (size_t at = 0; at + 8 <= size;) {
        size_t length = (size_t)bytes[at + 4] << 24 | (size_t)bytes[at + 5] << 16 | bytes[at + 6] << 8 | bytes[at + 7];
        if (length & 0x80000000u) {
            size = at;
            break;
        }
        if (length > size - at - 8) {
            if (!(fixed = malloc(size))) {
                free(p);
                set_error(error, error_size, "out of memory opening a MIDI file");
                return NULL;
            }
            memcpy(fixed, data, size);
            length = size - at - 8;
            fixed[at + 4] = (unsigned char)(length >> 24), fixed[at + 5] = (unsigned char)(length >> 16);
            fixed[at + 6] = (unsigned char)(length >> 8), fixed[at + 7] = (unsigned char)length;
            data = fixed;
            break;
        }
        at += 8 + length;
    }
    struct memory memory = {data, size, 0};
    struct tml_stream stream = {&memory, memory_read};
    p->song = tml_load(&stream);
    free(fixed);
    if (!p->song) {
        free(p);
        set_error(error, error_size, "can't read the MIDI file, or it has no events");
        return NULL;
    }
    p->rate = sample_rate;
    p->marker_ms = -1;
    for (const tml_message *m = p->song; m; m = m->next) {
        p->length_ms = (int)m->time;
        if (m->type == TML_CONTROL_CHANGE && (m->control & 127) == 111 && p->marker_ms < 0) p->marker_ms = (int)m->time;
    }

    if (font) {
        pthread_mutex_lock(&share_lock);
        p->synth = tsf_copy(font->base);
        pthread_mutex_unlock(&share_lock);
        // Creating every channel and voice now keeps render from allocating.
        if (!p->synth || !tsf_set_max_voices(p->synth, MAX_VOICES) ||
            !tsf_channel_set_presetnumber(p->synth, CHANNELS - 1, 0, 0)) {
            vnmidi_player_close(p);
            set_error(error, error_size, "out of memory opening a MIDI file");
            return NULL;
        }
        tsf_set_output(p->synth, TSF_STEREO_INTERLEAVED, sample_rate, output_gain_db);
    } else {
        snprintf(p->error, sizeof p->error, "no SoundFont loaded, so MIDI music is silent");
    }

    atomic_init(&p->volume, 1.0f);
    atomic_init(&p->tempo, 1.0f);
    atomic_init(&p->transpose, 0);
    atomic_init(&p->loop, 0);
    atomic_init(&p->loop_start_ms, VNMIDI_LOOP_MARKER);
    atomic_init(&p->request, REQUEST_NONE);
    atomic_init(&p->position_ms, 0);
    atomic_init(&p->playing, 1);
    p->gain = 1.0f;
    p->state = STOPPED;
    jump(p, 0);
    return p;
}

void vnmidi_player_close(vnmidi_player *player) {
    if (!player) return;
    if (player->synth) {
        pthread_mutex_lock(&share_lock);
        tsf_close(player->synth);
        pthread_mutex_unlock(&share_lock);
    }
    tml_free(player->song);
    free(player);
}

const char *vnmidi_player_error(const vnmidi_player *player) { return player->error[0] ? player->error : NULL; }

static int loop_target(vnmidi_player *p) {
    int start = atomic_load_explicit(&p->loop_start_ms, memory_order_relaxed);
    if (start == VNMIDI_LOOP_MARKER) start = p->marker_ms;
    return start > 0 && start < p->length_ms ? start : 0;
}

static int voices(vnmidi_player *p) { return p->synth ? tsf_active_voice_count(p->synth) : 0; }

int vnmidi_player_render(vnmidi_player *p, float *out, int frames) {
    int request = atomic_exchange_explicit(&p->request, REQUEST_NONE, memory_order_acquire);
    if (request == REQUEST_STOP) {
        if (p->synth) silence(p);
        p->state = STOPPED;
        p->ms = 0;
    } else if (request >= 0) {
        jump(p, request);
    }

    float tempo = atomic_load_explicit(&p->tempo, memory_order_relaxed);
    if (!(tempo > 0.01f)) tempo = 0.01f;
    double ms_per_frame = 1000.0 * tempo / p->rate;
    int played = 0, done = 0;
    while (done < frames) {
        int n = frames - done;
        if (p->state == PLAYING) {
            // Events at the very end would only start notes that the end releases at once.
            for (; p->next && p->next->time <= p->ms && p->next->time < (unsigned)p->length_ms; p->next = p->next->next)
                apply(p, p->next, 1);
            if (p->ms >= p->length_ms) {
                if (atomic_load_explicit(&p->loop, memory_order_relaxed) && p->length_ms > 0) {
                    jump(p, loop_target(p));
                    continue;
                }
                if (p->synth) tsf_note_off_all(p->synth);
                p->state = ENDING;
                continue;
            }
            // Up to the next event, so events land on the frame they belong to.
            double until = p->next && p->next->time < (unsigned)p->length_ms ? (double)p->next->time : p->length_ms;
            double to_event = ceil((until - p->ms) / ms_per_frame);
            if (to_event < n) n = to_event < 1 ? 1 : (int)to_event;
            p->ms += n * ms_per_frame;
            played = done + n;
        } else if (p->state == ENDING) {
            if (!voices(p) || p->quiet_ms >= QUIET_MS || p->tail_ms >= TAIL_LIMIT_MS) {
                if (p->synth) silence(p);
                p->state = DONE;
                p->ms = p->length_ms;
                continue;
            }
            p->tail_ms += n * 1000.0 / p->rate;
            played = done + n;
        } else {
            // Stopped or done: whatever still fades out, then silence.
            if (!voices(p)) {
                memset(out + done * 2, 0, (size_t)n * 2 * sizeof(float));
                break;
            }
        }
        if (p->synth) tsf_render_float(p->synth, out + done * 2, n, 0);
        else memset(out + done * 2, 0, (size_t)n * 2 * sizeof(float));
        if (p->state == ENDING) {
            float peak = 0;
            for (int i = done * 2; i < (done + n) * 2; i++) peak = fmaxf(peak, fabsf(out[i]));
            p->quiet_ms = peak < quiet_level ? p->quiet_ms + n * 1000.0 / p->rate : 0;
        }
        done += n;
    }

    // The volume ramps across the call.
    float target = atomic_load_explicit(&p->volume, memory_order_relaxed);
    if (target != 1.0f || p->gain != 1.0f) {
        float step = (target - p->gain) / (float)(frames > 0 ? frames : 1);
        for (int i = 0; i < frames; i++) {
            float g = p->gain + step * (float)(i + 1);
            out[2 * i] *= g;
            out[2 * i + 1] *= g;
        }
    }
    p->gain = target;

    atomic_store_explicit(&p->position_ms, (int)p->ms, memory_order_relaxed);
    atomic_store_explicit(&p->playing, p->state == PLAYING || p->state == ENDING, memory_order_relaxed);
    return played;
}

int vnmidi_player_length_ms(const vnmidi_player *player) { return player->length_ms; }

int vnmidi_player_position_ms(const vnmidi_player *player) {
    return atomic_load_explicit(&player->position_ms, memory_order_relaxed);
}

int vnmidi_player_playing(const vnmidi_player *player) {
    return atomic_load_explicit(&player->playing, memory_order_relaxed);
}

int vnmidi_player_marker_ms(const vnmidi_player *player) { return player->marker_ms; }

void vnmidi_player_set_loop(vnmidi_player *player, int loop, int loop_start_ms) {
    atomic_store_explicit(&player->loop_start_ms, loop_start_ms, memory_order_relaxed);
    atomic_store_explicit(&player->loop, loop != 0, memory_order_relaxed);
}

void vnmidi_player_set_volume(vnmidi_player *player, float gain) {
    atomic_store_explicit(&player->volume, gain > 0 ? gain : 0, memory_order_relaxed);
}

void vnmidi_player_set_tempo(vnmidi_player *player, float factor) {
    atomic_store_explicit(&player->tempo, factor, memory_order_relaxed);
}

void vnmidi_player_set_transpose(vnmidi_player *player, int semitones) {
    atomic_store_explicit(&player->transpose, semitones, memory_order_relaxed);
}

void vnmidi_player_seek(vnmidi_player *player, int ms) {
    if (ms < 0) ms = 0;
    atomic_store_explicit(&player->position_ms, ms, memory_order_relaxed);
    atomic_store_explicit(&player->playing, 1, memory_order_relaxed);
    atomic_store_explicit(&player->request, ms, memory_order_release);
}

void vnmidi_player_stop(vnmidi_player *player) {
    atomic_store_explicit(&player->position_ms, 0, memory_order_relaxed);
    atomic_store_explicit(&player->playing, 0, memory_order_relaxed);
    atomic_store_explicit(&player->request, REQUEST_STOP, memory_order_release);
}

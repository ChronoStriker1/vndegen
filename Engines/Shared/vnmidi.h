// vnmidi: General MIDI music for VNDegen's native engines. It plays Standard MIDI Files (formats 0 and 1) through a
// SoundFont (.sf2, or .sf3 with Ogg Vorbis samples) with TinySoundFont, and renders interleaved stereo float. The app
// bundles a General MIDI SoundFont and gives its path to engines; see docs/midi.md.
//
// Threads. A font may be shared by any number of players on any threads. Each player has one rendering thread, which
// may be a real-time audio thread: vnmidi_player_render doesn't allocate, lock, or touch files. Every other player
// call may come from any thread while it renders. Settings, seeks, and stops take effect at the start of the next
// render call. Close a player only once its render calls have stopped. Opening and closing players, and loading and
// freeing fonts, allocate and take a short lock, so keep them off the real-time thread.
#ifndef VNMIDI_H
#define VNMIDI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vnmidi_font vnmidi_font;
typedef struct vnmidi_player vnmidi_player;

// Loads a SoundFont and decodes all of its samples, which takes a moment for an .sf3 file. Returns NULL on failure and
// writes a one-line reason to `error` when it isn't NULL.
vnmidi_font *vnmidi_font_load(const char *path, char *error, size_t error_size);
// Releases the font. Players opened from it keep its samples until they close, so this may come before their close.
void vnmidi_font_free(vnmidi_font *font);

// Opens a player for the MIDI file in `data`, which may be freed after the call. The player starts at the beginning
// and plays when rendered, once, without looping. `font` may be NULL: the player then renders silence but keeps
// time, so position, length, and the end of the song work, and vnmidi_player_error says why it is silent. Returns
// NULL only when `data` isn't a MIDI file it can read, `sample_rate` isn't positive, or memory runs out, and writes a
// one-line reason to `error` when it isn't NULL.
vnmidi_player *vnmidi_player_open(vnmidi_font *font, const void *data, size_t size, int sample_rate, char *error,
                                  size_t error_size);
void vnmidi_player_close(vnmidi_player *player);
// Why the player is silent, in one line, or NULL when it has a font.
const char *vnmidi_player_error(const vnmidi_player *player);

// Writes `frames` frames of interleaved stereo float to `out`. Returns `frames` while the song plays, and fewer once it
// ends or is stopped within the call: the frames after that are silent, and later calls return 0. A song that doesn't
// loop ends when its last event has passed and its last notes have faded below -80 dBFS, or 10 s after that.
int vnmidi_player_render(vnmidi_player *player, float *out, int frames);

// Song time in ms, which tempo doesn't change, as a file's own clock counts it.
int vnmidi_player_length_ms(const vnmidi_player *player);
// Where the song is, in ms of song time. Follows loops, and is 0 after a stop.
int vnmidi_player_position_ms(const vnmidi_player *player);
// 1 until the song ends or is stopped.
int vnmidi_player_playing(const vnmidi_player *player);
// The time of the file's first CC#111 event, RPG Maker's loop start marker, or -1 when it has none.
int vnmidi_player_marker_ms(const vnmidi_player *player);

// The loop start that means the file's CC#111 marker when it has one, else the start of the song.
#define VNMIDI_LOOP_MARKER (-1)

// Turns looping on or off. A looping song jumps from its end to `loop_start_ms`, or to VNMIDI_LOOP_MARKER's start.
// Notes sounding at the end are released, so their tails carry over the jump, and the channels take the programs and
// controllers they have at the loop start. A loop start at or past the end loops from the start of the song.
void vnmidi_player_set_loop(vnmidi_player *player, int loop, int loop_start_ms);
// Linear gain on the output, 1 by default. Changes ramp over one render call, so they don't click.
void vnmidi_player_set_volume(vnmidi_player *player, float gain);
// Plays faster above 1 and slower below, without changing pitch. 1 by default.
void vnmidi_player_set_tempo(vnmidi_player *player, float factor);
// Shifts every channel but the drums (channel 10) by `semitones`. Applies to notes that start after the change.
void vnmidi_player_set_transpose(vnmidi_player *player, int semitones);
// Jumps to `ms` of song time and plays from there, also after a stop or the end. Sounding notes are released, and the
// channels take the programs and controllers they have at that point.
void vnmidi_player_seek(vnmidi_player *player, int ms);
// Silences the player within a few ms, without a click, and rewinds it. It stays silent until a seek.
void vnmidi_player_stop(vnmidi_player *player);

#ifdef __cplusplus
}
#endif

#endif

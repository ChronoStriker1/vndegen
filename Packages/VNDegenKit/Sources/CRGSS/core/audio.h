// RGSS's Audio module: BGM, BGS, ME and SE, mixed to stereo float at 44100 Hz. The game thread starts and stops
// sounds; the host's audio thread pulls the mix through audio_render.
#ifndef RGSS_AUDIO_H
#define RGSS_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { AUDIO_RATE = 44100 };

typedef enum { TRACK_BGM, TRACK_BGS, TRACK_ME } AudioTrack;

// Called by the host.
void audio_start(const char *soundFont, const char *fallbackSoundFont);
void audio_stop(void);
void audio_set_paused(bool paused);
void audio_render(float *out, int frames);

// Called by the game thread. `data` is the whole file, which the audio side takes and frees. `extension` is the
// file's, lower case, which picks the decoder when the data doesn't say. Volume is RGSS's 0..100, pitch its percent.
bool audio_play(AudioTrack track, uint8_t *data, size_t size, const char *name, int volume, int pitch, double position);
void audio_stop_track(AudioTrack track);
void audio_fade_track(AudioTrack track, int ms);
// Seconds into the track's sound, for VX Ace's bgm_pos and bgs_pos, as the sample count RGSS reports.
long audio_position(AudioTrack track);
bool audio_play_se(uint8_t *data, size_t size, const char *name, int volume, int pitch);
void audio_stop_se(void);

#endif

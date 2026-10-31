// The RGSS runtime's C interface, the only part of it VNDegen calls. The runtime is VNDegen's own (Engines/RGSSRuntime):
// it runs the game's Ruby scripts on a thread of its own, composes each frame on the CPU, and mixes its sound, which
// the app presents and plays through FrameHost.
// Every function runs on the main thread except vnrgss_audio_render. One game runs at a time; after vnrgss_stop,
// vnrgss_start can start another in the same process.
#ifndef VNRGSS_H
#define VNRGSS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNRGSSLaunch {
    const char *gameIni;          // the game's Game.ini; the game is the folder it is in
    const char *const *rtps;      // the installed RTP folders the game names, in Game.ini's order
    int rtpCount;
    const char *saveDir;          // where the game writes. It reads this folder and its own folder as one, this one first
    const char *logPath;          // file the runtime's log is written to; NULL logs to stderr
    const char *soundFont;        // the SoundFont for MIDI music; NULL plays MIDI silently
    const char *fallbackSoundFont; // used when soundFont can't be loaded; may be NULL
} VNRGSSLaunch;

// Loads the game and starts its scripts. Returns 0 on success. Call vnrgss_stop afterwards even when it fails.
int vnrgss_start(const VNRGSSLaunch *launch);

// Lets the game run for `seconds` of time, the game drawing as many frames as its frame rate gives. Returns nonzero
// once the game has ended, by its own quit or by an error that vnrgss_problem describes. Does nothing while paused.
int vnrgss_step(double seconds);

// The latest frame, RGBA8 with rows packed, and its size. `changed` says whether it differs from the frame the last
// call returned. The pixels stay valid until the next vnrgss_step or vnrgss_stop. NULL before the first frame.
const uint8_t *vnrgss_frame(int *width, int *height, bool *changed);

// The frame rate the game runs at, from Graphics.frame_rate.
int vnrgss_frame_rate(void);

// Mixes `frames` frames of interleaved stereo float at 44100 Hz into `out`. Safe on the real-time audio thread.
void vnrgss_audio_render(float *out, int frames);

// A key going down or up, as a libretro key code (RETROK_*): lowercase ASCII for letters, digits and punctuation,
// 13 Return, 27 Escape, 273 to 276 the arrows (up, down, right, left), 304 left Shift, 306 left Control. RPG Maker's
// keys also press RGSS's buttons, as on PC: Z, Return, and Space are C, X and Escape are B, left Shift is A.
void vnrgss_key(unsigned retrokCode, bool down);

// The mouse: a position normalized 0..1 over the frame, and button 0 left, 1 right, 2 middle.
void vnrgss_mouse_move(float x, float y);
void vnrgss_mouse_button(int button, bool down);
// Wheel notches, positive turned away from the player.
void vnrgss_wheel(float notches);

// Stops the game's clock and sound. vnrgss_step does nothing while paused.
void vnrgss_set_paused(bool paused);

// Runs the game `speed` times as fast, 1 being its own speed: each step lets it draw `speed` times as many frames, and
// only the frames that will be shown are composited.
void vnrgss_set_speed(int speed);

// Queues Ruby to run on the game thread while the game waits for its next frame, between two frames, whether or not
// it is paused. Returns a request number for vnrgss_ruby_result, or 0 when no game is running.
long vnrgss_run_ruby(const char *ruby);

// What a request's Ruby gave, as its last value's to_s, or with `failed` set, why it failed: its exception's message, or
// the game ending before it ran. The caller frees it. NULL while the request waits, and for a request already read.
char *vnrgss_ruby_result(long request, bool *failed);

// Ends the game and waits for its thread to unwind. RGSS games save only when the player saves, so nothing is lost.
void vnrgss_stop(void);

// A sentence for the player describing why the game ended with an error, such as the Ruby exception it raised, or
// NULL when there is none. It stays readable after vnrgss_stop and clears when the next game starts.
const char *vnrgss_problem(void);

#ifdef __cplusplus
}
#endif

#endif

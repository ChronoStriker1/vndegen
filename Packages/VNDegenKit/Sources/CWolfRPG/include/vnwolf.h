// VNDegen's Wolf RPG Editor runtime: its C interface, the only part of the runtime the app calls.
//
// The runtime draws each frame on the CPU into an RGBA buffer and mixes its sound into 44.1 kHz stereo float
// samples. The host presents the frame and plays the samples; it owns the window, the clock and the input.
// Call every function on one thread (the host's main thread), except vnwolf_audio_render, which the audio thread
// calls. One engine runs one game.
#ifndef VNWOLF_H
#define VNWOLF_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNWolfEngine VNWolfEngine;

typedef struct VNWolfLaunch {
    const char *gameRoot;       // folder holding Game.exe and Data or Data.wolf
    const char *logPath;        // file the runtime's log is appended to; NULL logs to stderr
    const char *soundFontPath;  // General MIDI SoundFont for MIDI music; NULL plays MIDI silently
    const char *fallbackSoundFontPath;  // loaded when soundFontPath can't be; may be NULL
} VNWolfLaunch;

// Starts a game. Returns NULL when it can't, with the reason in vnwolf_start_error().
VNWolfEngine *vnwolf_start(const VNWolfLaunch *launch);
const char *vnwolf_start_error(void);

// Advances the game by ms milliseconds, at the game's own frame rate. Returns false once the game has ended.
bool vnwolf_step(VNWolfEngine *e, int ms);

// The current frame: RGBA8, width * height pixels, rows packed. *changed is set when the game advanced since the
// last call. The pointer stays valid until the next vnwolf_step.
const uint8_t *vnwolf_frame(VNWolfEngine *e, int *width, int *height, bool *changed);

// The game's title from Game.dat, UTF-8.
const char *vnwolf_caption(VNWolfEngine *e);

// Mixes frames stereo frames of 44.1 kHz float audio into out (interleaved). Safe on the audio thread.
void vnwolf_audio_render(VNWolfEngine *e, float *out, int frames);

// Keyboard input, in the key codes below. Wolf's confirm is Z, Return or Space, cancel X or Escape, and sub C or Shift.
void vnwolf_key(VNWolfEngine *e, int key, bool down);

// Key codes: printable keys are their uppercase ASCII codes ('A'..'Z', '0'..'9'), others these. The same values
// as VNDegen's NScripter runtime, so hosts share their key tables.
enum {
    VNWOLF_KEY_BACKSPACE = 8,
    VNWOLF_KEY_TAB = 9,
    VNWOLF_KEY_RETURN = 13,
    VNWOLF_KEY_ESCAPE = 27,
    VNWOLF_KEY_SPACE = 32,
    VNWOLF_KEY_UP = 256,
    VNWOLF_KEY_DOWN,
    VNWOLF_KEY_LEFT,
    VNWOLF_KEY_RIGHT,
    VNWOLF_KEY_PAGEUP,
    VNWOLF_KEY_PAGEDOWN,
    VNWOLF_KEY_INSERT,
    VNWOLF_KEY_CTRL,
    VNWOLF_KEY_SHIFT,
    VNWOLF_KEY_F1 = 300,  // F1..F12 are VNWOLF_KEY_F1 + 0..11
};

// Stops the clock and sound. vnwolf_step does nothing while paused.
void vnwolf_set_paused(VNWolfEngine *e, bool paused);

// Frees the engine.
void vnwolf_stop(VNWolfEngine *e);

// For logs and test hosts: the map and what the game waits for.
const char *vnwolf_debug_status(VNWolfEngine *e);

// A sentence for the player about the most recent problem, such as a missing file, or NULL.
const char *vnwolf_problem(VNWolfEngine *e);

#ifdef __cplusplus
}
#endif

#endif

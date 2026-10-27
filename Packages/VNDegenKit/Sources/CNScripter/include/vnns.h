// VNDegen's NScripter runtime: its C interface, the only part of the runtime the app calls.
//
// The runtime draws each frame on the CPU into an RGBA buffer and mixes its sound into 44.1 kHz stereo float
// samples. The host presents the frame and plays the samples; it owns the window, the clock and the input.
// Call every function on one thread (the host's main thread), except vnns_audio_render, which the audio thread
// calls. One engine runs one game; several can exist at once.
#ifndef VNNS_H
#define VNNS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNNSEngine VNNSEngine;

typedef struct VNNSLaunch {
    const char *gameRoot;   // folder holding the script (0.txt, 00.txt or nscript.dat) and archives
    const char *logPath;    // file the runtime's log is appended to; NULL logs to stderr
    // Break English text between words, as some translation patches' engines do. The runtime also turns this on
    // itself when the game's exe carries such a patch (a PE section named ".patch").
    bool englishWordWrap;
} VNNSLaunch;

// Starts a game. Returns NULL when it can't, with the reason in vnns_start_error().
VNNSEngine *vnns_start(const VNNSLaunch *launch);
const char *vnns_start_error(void);

// Advances the game by ms milliseconds and runs its script. Returns false once the game has ended.
bool vnns_step(VNNSEngine *e, int ms);

// The current frame: RGBA8, width * height pixels, rows packed. *changed is set when it differs from the last
// call's frame. The pointer stays valid until the next vnns_step.
const uint8_t *vnns_frame(VNNSEngine *e, int *width, int *height, bool *changed);

// The game's window title, UTF-8.
const char *vnns_caption(VNNSEngine *e);

// Mixes frames stereo frames of 44.1 kHz float audio into out (interleaved). Safe on the audio thread.
void vnns_audio_render(VNNSEngine *e, float *out, int frames);

// Input. Coordinates are normalized 0..1 over the frame. Buttons: 1 left, 2 middle, 3 right.
void vnns_mouse_move(VNNSEngine *e, float x, float y);
void vnns_mouse_button(VNNSEngine *e, int button, bool down);
// Wheel notches, positive turned away from the player (up), which NScripter games use for the message history.
void vnns_wheel(VNNSEngine *e, int notches);
void vnns_key(VNNSEngine *e, int key, bool down);
// Characters typed, UTF-8, for text input fields.
void vnns_text(VNNSEngine *e, const char *utf8);

// Key codes: printable keys are their uppercase ASCII codes ('A'..'Z', '0'..'9'), others these.
enum {
    VNNS_KEY_BACKSPACE = 8,
    VNNS_KEY_TAB = 9,
    VNNS_KEY_RETURN = 13,
    VNNS_KEY_ESCAPE = 27,
    VNNS_KEY_SPACE = 32,
    VNNS_KEY_UP = 256,
    VNNS_KEY_DOWN,
    VNNS_KEY_LEFT,
    VNNS_KEY_RIGHT,
    VNNS_KEY_PAGEUP,
    VNNS_KEY_PAGEDOWN,
    VNNS_KEY_INSERT,
    VNNS_KEY_CTRL,
    VNNS_KEY_SHIFT,
    VNNS_KEY_F1 = 300,  // F1..F12 are VNNS_KEY_F1 + 0..11
};

// Stops the clock, timers and sound. vnns_step does nothing while paused.
void vnns_set_paused(VNNSEngine *e, bool paused);

// Ends the game the way closing its window does: system data (global variables, read text, logs) is written.
void vnns_request_quit(VNNSEngine *e);
// Writes the system data now, without ending the game.
void vnns_save_system_data(VNNSEngine *e);
// Frees the engine.
void vnns_stop(VNNSEngine *e);

// A sentence for the player about the most recent problem, such as a missing file, or NULL.
const char *vnns_problem(VNNSEngine *e);

// Glyph rendering. On Apple systems the runtime renders text itself. Elsewhere the host renders each character
// into a cellWidth x cellHeight 8-bit coverage buffer and returns nonzero. Set before vnns_start.
typedef int (*VNNSGlyphFunction)(void *ctx, uint32_t codepoint, int cellWidth, int cellHeight, int bold,
                                 uint8_t *coverage);
void vnns_set_glyph_function(VNNSGlyphFunction fn, void *ctx);

#ifdef __cplusplus
}
#endif

#endif

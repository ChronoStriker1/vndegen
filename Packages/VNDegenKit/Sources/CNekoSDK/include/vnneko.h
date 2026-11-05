// VNDegen's NekoSDK runtime: its C interface, the only part of the runtime the app calls.
//
// The runtime draws each frame on the CPU into an RGBA buffer and mixes its sound into 44.1 kHz stereo float samples.
// The host presents the frame and plays the samples; it owns the window, the clock and the input. Call every function
// on one thread (the host's main thread), except vnneko_audio_render, which the audio thread calls. One engine runs one
// game.
#ifndef VNNEKO_H
#define VNNEKO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNNekoEngine VNNekoEngine;

typedef struct VNNekoLaunch {
    const char *gameRoot;  // folder holding the game's .pak archives
    const char *logPath;   // file the runtime's log is appended to; NULL logs to stderr
    const char *fontDir;   // folder holding the Windows font pack's fonts.tsv
} VNNekoLaunch;

// Starts a game. Returns NULL when it can't, with the reason in vnneko_start_error().
VNNekoEngine *vnneko_start(const VNNekoLaunch *launch);
const char *vnneko_start_error(void);

// Advances the game by ms milliseconds. Returns false once the game has ended.
bool vnneko_step(VNNekoEngine *e, int ms);

// The current frame: RGBA8, width * height pixels, rows packed. *changed is set when it differs from the last call's.
// The pointer stays valid until the next vnneko_step.
const uint8_t *vnneko_frame(VNNekoEngine *e, int *width, int *height, bool *changed);

// Mixes frames stereo frames of 44.1 kHz float audio into out (interleaved). Safe on the audio thread.
void vnneko_audio_render(VNNekoEngine *e, float *out, int frames);

// The player's input. x and y are normalized over the frame, 0 to 1. Buttons are 0 left, 1 right, 2 middle.
void vnneko_mouse_move(VNNekoEngine *e, float x, float y);
void vnneko_mouse_button(VNNekoEngine *e, int button, bool down);
// Runs through text and waits for as long as it's on, as holding Ctrl does.
void vnneko_set_skip(VNNekoEngine *e, bool on);

// Stops the clock and sound. vnneko_step does nothing while paused.
void vnneko_set_paused(VNNekoEngine *e, bool paused);

// Frees the engine.
void vnneko_stop(VNNekoEngine *e);

// A sentence for the player about the most recent problem, such as a missing file, or NULL.
const char *vnneko_problem(VNNekoEngine *e);

// For logs and test hosts: the script, the node, and what the game waits for.
const char *vnneko_debug_status(VNNekoEngine *e);
// For test hosts: whether the game waits for a click now, and how many text boxes it has shown.
bool vnneko_debug_waiting_for_click(VNNekoEngine *e);
int vnneko_debug_text_count(VNNekoEngine *e);
// For test hosts: presses a button of the menu on screen by its action, such as "start".
bool vnneko_debug_press(VNNekoEngine *e, const char *action);
// For test hosts: the options of the choice on screen, or -1, and picking one.
int vnneko_debug_choice_count(VNNekoEngine *e);
void vnneko_debug_choose(VNNekoEngine *e, int index);
// For test hosts: starts this scene script and runs it without waiting up to the node. False when it's not there.
bool vnneko_debug_jump(VNNekoEngine *e, const char *script, int node);

#ifdef __cplusplus
}
#endif

#endif

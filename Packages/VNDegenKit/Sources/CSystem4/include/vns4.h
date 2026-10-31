// VNDegen's AliceSoft System 4 runtime: its C interface, the only part of the runtime the app calls.
//
// The runtime runs the game's bytecode on a thread of its own, draws each frame on the CPU into an RGBA buffer and
// mixes its sound into 44.1 kHz stereo float samples. The host presents the frame and plays the samples; it owns
// the window, the clock and the input. Call every function on one thread (the host's main thread), except
// vns4_audio_render, which the audio thread calls. One game runs at a time.
#ifndef VNS4_H
#define VNS4_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNS4Engine VNS4Engine;

typedef struct VNS4Launch {
    const char *gameRoot;  // folder holding the game's .ain and its ini (AliceStart.ini or System40.ini)
    const char *saveDir;   // where saves go: the game's SaveData folder, as on Windows
    const char *logPath;   // file the runtime's log is appended to; NULL logs to stderr
    const char *fontsDir;  // VNDegen's Windows font pack (the folder holding fonts.tsv); NULL uses the system's fonts
} VNS4Launch;

// Why VNDegen can't run the System 4 game in this folder yet, as a sentence for the player, or NULL when it can (or
// when the folder's code file can't be read, which vns4_start reports). Reads the code file; valid until the next
// call on the same thread.
const char *vns4_unsupported(const char *gameRoot);

// Starts a game. Returns NULL when it can't, with a sentence for the player in vns4_start_error().
VNS4Engine *vns4_start(const VNS4Launch *launch);
const char *vns4_start_error(void);

// Runs the game for ms milliseconds of game time: until it shows its next frame or polls for input. Returns false
// once the game has ended.
bool vns4_step(VNS4Engine *e, int ms);

// The current frame: RGBA8, width * height pixels, rows packed. The pointer stays valid until the next vns4_frame.
const uint8_t *vns4_frame(VNS4Engine *e, int *width, int *height);

// The game's name from its ini, UTF-8.
const char *vns4_caption(VNS4Engine *e);

// Mixes frames stereo frames of 44.1 kHz float audio into out (interleaved). Safe on the audio thread.
void vns4_audio_render(VNS4Engine *e, float *out, int frames);

// Input. Keys are Windows virtual-key codes (VK_RETURN 0x0D, VK_SPACE 0x20, VK_ESCAPE 0x1B, arrows 0x25..0x28,
// letters and digits their uppercase ASCII codes); the mouse buttons are VK_LBUTTON 1, VK_RBUTTON 2 and VK_MBUTTON 4.
// Mouse coordinates are in the game's pixels.
void vns4_key(VNS4Engine *e, int vk, bool down);
void vns4_mouse_move(VNS4Engine *e, int x, int y);
void vns4_wheel(VNS4Engine *e, int notches);  // positive away from the player

// Stops the clock and sound. vns4_step does nothing while paused.
void vns4_set_paused(VNS4Engine *e, bool paused);

// Stops the game and frees the engine.
void vns4_stop(VNS4Engine *e);

// Debugging: logs every sprite when the next frame is drawn.
void vns4_debug_dump_sprites(VNS4Engine *e);
// Debugging: calls the game function with this name (UTF-8) when the game next shows a frame.
void vns4_debug_call(VNS4Engine *e, const char *utf8Name);

// A sentence for the player about the most recent problem, such as a part of the game VNDegen can't run yet, or NULL.
const char *vns4_problem(VNS4Engine *e);

#ifdef __cplusplus
}
#endif

#endif

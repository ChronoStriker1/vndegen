// VNDegen's N2System runtime: Nitroplus games on "NitroplusSystem2.0", such as The Song of Saya.
// A clean-room implementation; docs/n2system-cleanroom.md lists its sources.
//
// The runtime has no threads and no platform code. The host calls vnn2_frame about 60 times a
// second, shows the frame it returns, sends input, and pulls audio. All calls for one VNN2 must
// come from one thread, except vnn2_audio, which may run on the audio thread.
#ifndef VNN2_H
#define VNN2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNN2 VNN2;

typedef struct VNN2Config {
    const char *gameDir;   // the folder holding the .npa archives
    const char *saveDir;   // the PC engine's save folder: %APPDATA%\Nitroplus\<name>\<version>, made if missing
    const char *fontPath;  // font file for all text; NULL asks the shared font pack (vnfont) for the game's font
    int fontIndex;         // face index inside a font collection, with fontPath
    const char *logPath;   // log file, appended to; NULL logs nothing
    int audioRate;         // output sample rate for vnn2_audio, such as 44100
    long long startTime;   // the game clock's start in Unix seconds; 0 starts it now. Tests pin it.
} VNN2Config;

// Opens a game. Returns NULL and fills err (if given) when the folder holds no game it can run.
VNN2 *vnn2_open(const VNN2Config *config, char *err, size_t errSize);
void vnn2_close(VNN2 *n2);

// Reads system.ini's product name and version from a game folder, for the save folder name, without opening the
// game. Returns false when the folder has no system.ini. Both strings are UTF-8.
bool vnn2_product(const char *gameDir, char *name, size_t nameSize, char *version, size_t versionSize);

// Runs the game for elapsedMs milliseconds of wall time. Returns false once the game has quit.
bool vnn2_frame(VNN2 *n2, uint32_t elapsedMs);

// The visible screen as RGBA8 pixels (bytes R, G, B, A), rows top to bottom. Valid until the next vnn2_frame.
const uint32_t *vnn2_pixels(VNN2 *n2, int *width, int *height);
// Bumped whenever the screen changed, so the host can skip unchanged frames.
uint32_t vnn2_frame_serial(VNN2 *n2);

// Input. Mouse coordinates are in screen pixels. Keys use Windows virtual-key codes (VK_RETURN 0x0d,
// VK_SPACE 0x20, VK_ESCAPE 0x1b, VK_CONTROL 0x11, arrows 0x25 to 0x28, letters 0x41 to 0x5a).
enum { VNN2_MOUSE_LEFT = 1, VNN2_MOUSE_RIGHT = 2 };
void vnn2_mouse_move(VNN2 *n2, int x, int y);
void vnn2_mouse_button(VNN2 *n2, int button, bool down);
void vnn2_key(VNN2 *n2, int virtualKey, bool down);
void vnn2_wheel(VNN2 *n2, int notches);

// Fills frames of interleaved stereo float audio at the configured rate. Safe to call on the audio thread.
void vnn2_audio(VNN2 *n2, float *out, int frames);

// Stops the game clock and its sound while paused.
void vnn2_set_paused(VNN2 *n2, bool paused);

// Asks the game to end; the next vnn2_frame returns false.
void vnn2_request_quit(VNN2 *n2);

// The game's window title, in UTF-8.
const char *vnn2_title(VNN2 *n2);

// A sentence describing the last problem that stopped the game, or NULL.
const char *vnn2_problem(VNN2 *n2);

#ifdef __cplusplus
}
#endif

#endif

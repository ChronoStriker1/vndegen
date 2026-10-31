// VNDegen's YU-RIS runtime. A clean-room implementation; docs/yuris-cleanroom.md lists its sources.
//
// The runtime has no threads and no platform code. The host calls vny_frame about 60 times a second, shows the frame
// it returns, sends input, and pulls audio. All calls for one VNY must come from one thread, except vny_audio, which
// may run on the audio thread. Text uses the shared Windows font pack (vnfont.h), which the host initializes.
#ifndef VNYURIS_H
#define VNYURIS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNY VNY;

typedef struct VNYConfig {
    const char *gameDir;   // the folder holding the game's .exe, yscfg.dat and .ypf archives
    const char *saveDir;   // where save files are read and written; the game's savedata folder on PC
    const char *logPath;   // log file, appended to; NULL logs nothing
    int audioRate;         // output sample rate for vny_audio, such as 44100
    long long startTime;   // the game clock's start in Unix seconds; 0 starts it now. Tests pin it, since games read it.
} VNYConfig;

// Opens a game. Returns NULL and fills err (if given) when the folder holds no game it can run.
VNY *vny_open(const VNYConfig *config, char *err, size_t errSize);
void vny_close(VNY *y);

// Runs the game for elapsedMs milliseconds of wall time. Returns false once the game has quit.
bool vny_frame(VNY *y, uint32_t elapsedMs);

// The visible screen as RGBA8 pixels (bytes R, G, B, A), rows top to bottom. Valid until the next vny_frame.
const uint32_t *vny_pixels(VNY *y, int *width, int *height);
// Bumped whenever the screen changed, so the host can skip unchanged frames.
uint32_t vny_frame_serial(VNY *y);

// Input. Mouse coordinates are in screen pixels. Keys use Windows virtual-key codes (VK_RETURN 0x0d, VK_SPACE 0x20,
// VK_ESCAPE 0x1b, VK_CONTROL 0x11, arrows 0x25 to 0x28).
enum { VNY_MOUSE_LEFT = 1, VNY_MOUSE_RIGHT = 2, VNY_MOUSE_MIDDLE = 3 };
void vny_mouse_move(VNY *y, int x, int y_);
void vny_mouse_button(VNY *y, int button, bool down);
void vny_key(VNY *y, int virtualKey, bool down);
void vny_wheel(VNY *y, int notches);

// Fills frames of interleaved stereo float audio at the configured rate. Safe to call on the audio thread.
void vny_audio(VNY *y, float *out, int frames);

// Stops the game clock and its sound while paused.
void vny_set_paused(VNY *y, bool paused);

// Asks the game to end; the next vny_frame returns false.
void vny_request_quit(VNY *y);

// The game's window title, in UTF-8.
const char *vny_title(VNY *y);

// A sentence describing the last problem that stopped the game, or NULL.
const char *vny_problem(VNY *y);

#ifdef __cplusplus
}
#endif

#endif

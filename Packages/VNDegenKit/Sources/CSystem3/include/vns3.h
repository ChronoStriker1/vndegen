// VNDegen's System 3 runtime: AliceSoft System 3.5 to 3.9 games (System39.exe) and the older
// SYSTEM3.EXE games. A clean-room implementation; docs/system3-cleanroom.md lists its sources.
//
// The runtime has no threads and no platform code. The host calls vns3_frame about 60 times a
// second, shows the frame it returns, sends input, and pulls audio. All calls for one VNS3 must
// come from one thread, except vns3_audio, which may run on the audio thread.
#ifndef VNS3_H
#define VNS3_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNS3 VNS3;

typedef struct VNS3Config {
    const char *gameDir;   // the folder holding System39.ain and the ALD files, or SYSTEM3.EXE and ADISK.DAT
    const char *saveDir;   // where save files are read and written; the game folder on PC
    const char *fontPath;  // font file for all text; NULL asks the shared font pack (vnfont) for MS Gothic
    int fontIndex;         // face index inside a font collection, with fontPath
    const char *logPath;   // log file, appended to; NULL logs nothing
    int audioRate;         // output sample rate for vns3_audio, such as 44100
    const char *soundFontPath;         // General MIDI SoundFont for MIDI music; NULL plays MIDI silently
    const char *fallbackSoundFontPath; // used when soundFontPath can't be read; may be NULL
    long long startTime;   // the game clock's start in Unix seconds; 0 starts it now. Tests pin it, since games read it.
} VNS3Config;

// Opens a game. Returns NULL and fills err (if given) when the folder holds no game it can run.
VNS3 *vns3_open(const VNS3Config *config, char *err, size_t errSize);
void vns3_close(VNS3 *s3);

// Runs the game for elapsedMs milliseconds of wall time. Returns false once the game has quit.
bool vns3_frame(VNS3 *s3, uint32_t elapsedMs);

// The visible screen as RGBA8 pixels (bytes R, G, B, A; 0xAABBGGRR read as a little-endian word), rows top to bottom.
// Valid until the next vns3_frame.
const uint32_t *vns3_pixels(VNS3 *s3, int *width, int *height);
// Bumped whenever the screen changed, so the host can skip unchanged frames.
uint32_t vns3_frame_serial(VNS3 *s3);

// Input. Mouse coordinates are in screen pixels. Keys use Windows virtual-key codes, which is
// what the games test for (VK_RETURN 0x0d, VK_SPACE 0x20, VK_ESCAPE 0x1b, arrows 0x25 to 0x28).
enum { VNS3_MOUSE_LEFT = 1, VNS3_MOUSE_RIGHT = 2 };
void vns3_mouse_move(VNS3 *s3, int x, int y);
void vns3_mouse_button(VNS3 *s3, int button, bool down);
void vns3_key(VNS3 *s3, int virtualKey, bool down);
void vns3_wheel(VNS3 *s3, int notches);

// Fills frames of interleaved stereo float audio at the configured rate. Safe to call on the audio thread.
void vns3_audio(VNS3 *s3, float *out, int frames);

// Stops the game clock and its sound while paused.
void vns3_set_paused(VNS3 *s3, bool paused);

// Asks the game to end; the next vns3_frame returns false.
void vns3_request_quit(VNS3 *s3);

// The game's window title, in UTF-8.
const char *vns3_title(VNS3 *s3);

// A sentence describing the last problem that stopped the game, or NULL.
const char *vns3_problem(VNS3 *s3);

// Cheats, for VNDegen's Cheats page. They read and write the game's own variables, which its saves keep.
// How many variables the game has: the ones System39.ain names, or the 512 a SYSTEM3.EXE game saves.
int vns3_variable_count(VNS3 *s3);
// A variable's name from System39.ain in UTF-8, such as "VAR0012", or NULL when the game names none. Valid until the
// next call.
const char *vns3_variable_name(VNS3 *s3, int index);
// A variable's value as the game reads it, 0 to 65535.
int vns3_variable(VNS3 *s3, int index);
// Sets a variable, held to 0 to 65535. False when the game has no such variable.
bool vns3_set_variable(VNS3 *s3, int index, int value);
// Runs the game's clock 1 to 8 times as fast, so its waits, fades and timers pass that much sooner. 1 when the game
// opens.
void vns3_set_speed(VNS3 *s3, int speed);


#ifdef __cplusplus
}
#endif

#endif

// The RGSS engine's C interface, the only part of the engine VNDegen calls. The engine is mkxp-z's libretro core,
// rendering through ANGLE on Metal, inside the RGSS framework.
// Every function runs on the main thread. One game runs at a time; after vnrgss_stop, vnrgss_start can start another
// in the same process.
#ifndef VNRGSS_H
#define VNRGSS_H

#include <stdbool.h>
#include <stdint.h>

// The engine builds with hidden visibility, so the C API opts back in.
#define VNRGSS_API __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNRGSSLaunch {
    const char *gameIni;   // the game's Game.ini; the engine reads the game from its folder
    const char *systemDir; // the engine's own folder. RTPs are looked up in its mkxp-z/RTP/<name> folders
    const char *saveDir;   // where the game writes. It reads this folder and its own folder as one, this one first
    const char *logPath;   // file the engine's log is written to; NULL logs to stderr
    void *layer;           // the CALayer the game draws in, filling it, letterboxed to the game's shape
} VNRGSSLaunch;

// Loads the game and starts its audio. Returns 0 on success. Call vnrgss_stop afterwards even when it fails.
VNRGSS_API int vnrgss_start(const VNRGSSLaunch *launch);

// Runs one game frame and draws it, `seconds` after the previous call. Returns nonzero once the game has ended, by
// its own quit or by an error that vnrgss_problem describes. Does nothing while paused.
VNRGSS_API int vnrgss_frame(double seconds);

// A key going down or up, as a libretro key code (RETROK_*): lowercase ASCII for letters, digits and punctuation,
// 13 Return, 27 Escape, 273 to 276 the arrows (up, down, right, left), 304 left Shift, 306 left Control. RPG Maker's
// keys also press RGSS's buttons, as on PC: Z, Return, and Space are C, X and Escape are B, left Shift is A.
VNRGSS_API void vnrgss_key(unsigned retrokCode, bool down);

// The mouse: a position normalized 0..1 over the layer, and button 0 left, 1 right, 2 middle.
VNRGSS_API void vnrgss_mouse_move(float x, float y);
VNRGSS_API void vnrgss_mouse_button(int button, bool down);
// Wheel notches, positive turned away from the player.
VNRGSS_API void vnrgss_wheel(float notches);

// Stops the game's clock and audio. vnrgss_frame does nothing while paused.
VNRGSS_API void vnrgss_set_paused(bool paused);

// Ends the game and tears the engine down. RGSS games save only when the player saves, so nothing is lost.
VNRGSS_API void vnrgss_stop(void);

// A sentence for the player describing why the game ended with an error, such as the Ruby exception it raised, or
// NULL when there is none. It stays readable after vnrgss_stop and clears when the next game starts.
VNRGSS_API const char *vnrgss_problem(void);

#ifdef __cplusplus
}
#endif

#endif

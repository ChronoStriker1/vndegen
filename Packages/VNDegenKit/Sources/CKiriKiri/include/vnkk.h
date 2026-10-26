// The KiriKiri engine's C interface, the only part of the engine VNDegen calls.
// Every function runs on the main thread. One game runs at a time; after vnkk_stop, vnkk_start can
// start another in the same process.
#ifndef VNKK_H
#define VNKK_H

#include <stdbool.h>

// The engine builds with hidden visibility, so the C API opts back in.
#define VNKK_API __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNKKLaunch {
    const char *gameRoot;    // folder that holds the game's .exe; System.exePath and the base for savedata/
    const char *projectData; // data.xp3, a data/ folder, or a file with an embedded archive
    const char *logPath;     // file the engine's log is appended to; NULL logs to stderr
    const char *appDataDir;  // System.appDataPath, the Windows AppData/Roaming folder; NULL uses gameRoot
    const char *personalDir; // System.personalPath, the Windows Documents folder; NULL uses gameRoot
    void *hostView;          // macOS: the NSView the game draws in, filling it. Unused on iOS, see vnkk_view
} VNKKLaunch;

// Creates the SDL window and starts the game. Returns 0 on success. Call vnkk_stop afterwards
// even when it fails.
VNKK_API int vnkk_start(const VNKKLaunch *launch);

// Runs one frame. Returns nonzero once the game has quit. A call made while another is running,
// as happens when a modal dialog spins the run loop, returns 0 and does nothing. On iOS, and on
// macOS with a host view, a frame leaves the host app's events to the host's run loop.
VNKK_API int vnkk_iterate(void);

// Input, delivered as SDL events. Only these reach the game: SDL's own keyboard and, on macOS, its
// own mouse are ignored, so the host decides what the game receives. Touches on iOS still reach
// the game directly. Coordinates are normalized 0..1 over the view the game draws in. Mouse
// buttons use SDL's numbering: 1 left, 2 middle, 3 right.
VNKK_API void vnkk_key(int sdlScancode, bool down);
VNKK_API void vnkk_mouse_button(int button, bool down, float x, float y);
VNKK_API void vnkk_mouse_move(float x, float y);
// A mouse button pressed wherever the pointer or the last touch was, the middle of the view
// before either.
VNKK_API void vnkk_click(int button, bool down);
// Wheel notches, positive turned away from the player, which in Kirikiri opens the message history.
VNKK_API void vnkk_wheel(float notches);

// Stops and resumes the engine clock, its timers, and its audio. vnkk_iterate does nothing while
// paused.
VNKK_API void vnkk_set_paused(bool paused);

// Closes the game without its quit confirmation, so it runs its normal close path and writes its
// system save files. Keep calling vnkk_iterate until it returns nonzero.
VNKK_API void vnkk_request_quit(void);

// Tears the engine down after the game has quit or after vnkk_request_quit.
VNKK_API void vnkk_stop(void);

// A sentence for the player describing the most recent problem the engine recorded, such as an
// archive encryption it cannot read, or NULL when there is none. It stays readable after the game
// quits and clears when the next game starts.
VNKK_API const char *vnkk_problem(void);

// The UIView SDL renders into, for the host to place in its own view hierarchy. NULL on macOS,
// where the engine places its view in VNKKLaunch.hostView itself.
VNKK_API void *vnkk_view(void);

#ifdef __cplusplus
}
#endif

#endif

// The Godot engine's C interface, the only part of the engine VNDegen calls. VNDegen builds two Godot runtimes,
// Godot 3 and Godot 4, into two frameworks that export these same functions, and opens the one a game needs.
// Every function runs on the main thread. One game runs at a time; after vngd_stop, vngd_start can start another
// in the same process.
#ifndef VNGD_H
#define VNGD_H

#include <stdbool.h>
#include <stdint.h>

// The engine builds with hidden visibility, so the C API opts back in.
#define VNGD_API __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNGDLaunch {
    const char *pack;            // the game's .pck, or the .exe that holds it
    const char *executable;      // what OS.get_executable_path() returns: the game's .exe in the game's folder
    const char *dataDir;         // stands in for the Windows %APPDATA% folder. user:// is
                                 // <dataDir>/Godot/app_userdata/<project name>, or the project's custom user folder in it
    const char *logPath;         // file the engine's log is written to; NULL logs to stderr
    const char *renderingMethod; // Godot 4: "forward_plus", "mobile", or "gl_compatibility"; NULL keeps the game's.
                                 // Godot 3 ignores it
    void *layer;                 // the CALayer the game draws in, filling it. Godot 4 needs a CAMetalLayer. In the
                                 // iOS Simulator it draws through OpenGL ES, in a layer of its own inside this one
    float width, height;         // the layer's size in points
    float scale;                 // pixels per point
    // Called when the game wants text typed, such as when a text field takes focus, and when it no longer does.
    void (*keyboard)(bool show, void *context);
    void *context;
    // The Windows font substitutes of Engines/Shared/vnfont.h: the table and the folders its fonts are in, as
    // vnfont_init takes them. A game that asks for a Windows font by name gets its substitute. NULL for none.
    const char *fontTable;
    const char *const *fontFolders;
    int fontFolderCount;
} VNGDLaunch;

// Loads the game and starts it. Returns 0 on success. Call vngd_stop afterwards even when it fails.
VNGD_API int vngd_start(const VNGDLaunch *launch);

// Runs one frame and draws it. Returns nonzero once the game has quit. Does nothing while paused.
VNGD_API int vngd_iterate(void);

// The layer changed size, in points, or moved to a screen with another scale.
VNGD_API void vngd_resize(float width, float height, float scale);

// A key going down or up, as its USB HID keyboard usage (UIKey.keyCode on iOS), such as 4 for A, 40 Return, 41 Escape,
// 44 Space, and 79 to 82 the arrows (right, left, down, up). `character` is the Unicode character a key going down
// types, which a focused text field inserts, or 0. Godot's keyboard drivers put it on the same event.
VNGD_API void vngd_key(int hidUsage, bool down, uint32_t character);
// Characters typed, in UTF-8, as text the game's focused text field receives.
VNGD_API void vngd_text(const char *utf8);

// The mouse: a position normalized 0..1 over the layer, and button 1 left, 2 right, 3 middle.
VNGD_API void vngd_mouse_move(float x, float y);
VNGD_API void vngd_mouse_button(int button, bool down);
// Wheel notches, positive turned away from the player.
VNGD_API void vngd_wheel(float notches);
// A finger on the screen: index tells fingers apart, phase 0 down, 1 moved, 2 up, 3 cancelled. Godot games receive
// touches as touches, and as the mouse unless the game turned that off.
VNGD_API void vngd_touch(int index, int phase, float x, float y);

// The one controller VNDegen presents, an Xbox controller. Buttons use these numbers, axes run -1..1 (sticks, down and
// right positive) and 0..1 (triggers).
enum {
    VNGD_JOY_A, VNGD_JOY_B, VNGD_JOY_X, VNGD_JOY_Y, VNGD_JOY_BACK, VNGD_JOY_GUIDE, VNGD_JOY_START,
    VNGD_JOY_LEFT_STICK, VNGD_JOY_RIGHT_STICK, VNGD_JOY_LEFT_SHOULDER, VNGD_JOY_RIGHT_SHOULDER,
    VNGD_JOY_DPAD_UP, VNGD_JOY_DPAD_DOWN, VNGD_JOY_DPAD_LEFT, VNGD_JOY_DPAD_RIGHT,
};
enum { VNGD_AXIS_LEFT_X, VNGD_AXIS_LEFT_Y, VNGD_AXIS_RIGHT_X, VNGD_AXIS_RIGHT_Y, VNGD_AXIS_TRIGGER_LEFT, VNGD_AXIS_TRIGGER_RIGHT };
VNGD_API void vngd_joy_button(int button, bool down);
VNGD_API void vngd_joy_axis(int axis, float value);

// Stops the game's clock and audio. vngd_iterate does nothing while paused.
VNGD_API void vngd_set_paused(bool paused);

// Asks the game to close, as the window's close button would. A game that handles the request itself, such as with
// a confirmation, keeps running. Keep calling vngd_iterate until it returns nonzero.
VNGD_API void vngd_request_quit(void);

// Tears the engine down after the game has quit or after vngd_request_quit.
VNGD_API void vngd_stop(void);

// A sentence for the player describing why the game did not start or stopped, or NULL when there is none. It stays
// readable after vngd_stop and clears when the next game starts.
VNGD_API const char *vngd_problem(void);

#ifdef __cplusplus
}
#endif

#endif

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

// The Windows font substitutes, as the FontPack module lists them: the table and the folders its fonts are in, the
// bundled folder first. A font the game names and doesn't ship resolves through them, and the default font is the
// substitute for ＭＳ Ｐゴシック, the Windows engine's default, unless the game ships its own default.ttf. Call it before
// vnkk_start. A game started without it gets no substitutes.
VNKK_API void vnkk_set_fonts(const char *table, const char *const *fontDirs, int count);

// Creates the SDL window and starts the game. Returns 0 on success. Call vnkk_stop afterwards
// even when it fails.
VNKK_API int vnkk_start(const VNKKLaunch *launch);

// Runs one frame. Returns nonzero once the game has quit. A call made while another is running,
// as happens when a modal dialog spins the run loop, returns 0 and does nothing. On iOS, and on
// macOS with a host view, a frame leaves the host app's events to the host's run loop, except
// while the game shows a modal window of its own, such as a Yes/No dialog: that window's loop
// runs the host's events, so the host's input can answer it.
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
// Characters typed, in UTF-8, as the key presses (WM_CHAR) that follow a key going down on Windows. Kirikiri's
// text fields read these. Control characters such as "\r" and "\b" go through as Windows sends them. They reach
// the game in order with the input sent before them, so a key goes down before its character.
VNKK_API void vnkk_text(const char *utf8);

// Stops and resumes the engine clock, its timers, and its audio. vnkk_iterate does nothing while
// paused.
VNKK_API void vnkk_set_paused(bool paused);

// Closes the game without its quit confirmation, so it runs its normal close path and writes its
// system save files. A modal window the game shows closes as its close button would. Keep calling
// vnkk_iterate until it returns nonzero.
VNKK_API void vnkk_request_quit(void);

// Writes the game's system save files now, for a host that has to give up on a game that cannot
// close because the host's own code holds a frame open above it, such as an app quitting while
// the game's modal window runs the host's events. Call vnkk_stop once that frame has returned.
VNKK_API void vnkk_save_system_data(void);

// Tears the engine down after the game has quit or after vnkk_request_quit.
VNKK_API void vnkk_stop(void);

// A sentence for the player describing the most recent problem the engine recorded, such as an
// archive encryption it cannot read, or NULL when there is none. It stays readable after the game
// quits and clears when the next game starts.
VNKK_API const char *vnkk_problem(void);

// The game's window menu bar, which the engine never draws. A game that asks whether it is visible is told one of
// these. Automatic tells it hidden when one of its scripts tests for a hidden menu bar, since such a game puts its
// menu commands on screen then, and otherwise what the game set, so the game builds its menu for vnkk_menu to list.
enum { VNKK_MENU_BAR_AUTOMATIC = 0, VNKK_MENU_BAR_HIDDEN = 1, VNKK_MENU_BAR_SHOWN = 2 };
// Call before vnkk_start. The choice holds for later games until the next call.
VNKK_API void vnkk_set_menu_bar(int mode);
// -1 until the game first asked, then 1 when it was told the menu bar shows and 0 when it was told it is hidden.
VNKK_API int vnkk_menu_bar_shown(void);

// The main window's menu bar as JSON, or NULL when the game has none: an array of items, each
// {"id": "0.3", "title": "Save(&S)", "checked": false, "radio": false, "enabled": true, "items": [...]},
// with the game's captions as it set them, "-" for a separator. Hidden items are left out, and so are the ones
// VNDegen's own menu covers or that can't work here: Exit, Windowed, Full Screen, Help and its items, the font
// dialog, and the debug menu. "enabled" is false when the item or a menu above it is disabled. The text stays valid
// until the next call.
VNKK_API const char *vnkk_menu(void);
// Clicks the item with the given id, as the player choosing it in the menu bar would. The game handles the click in
// its next frame. Returns false when no item has the id.
VNKK_API bool vnkk_menu_click(const char *id);

// The UIView SDL renders into, for the host to place in its own view hierarchy. NULL on macOS,
// where the engine places its view in VNKKLaunch.hostView itself.
VNKK_API void *vnkk_view(void);

#ifdef __cplusplus
}
#endif

#endif

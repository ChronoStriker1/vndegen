#pragma once

// VNDegen's host for Ren'Py on iPhone and iPad. The app links Ren'Py's static libraries and runs every game in one
// Python interpreter on the main thread, because iOS has no helper processes and CPython can't be restarted in one
// process. These functions do nothing on other platforms.

/// Starts Python the first time, with the standard library and Ren'Py from the runtime folder `home`, then runs the
/// Python source `code` and returns once it returns, which for a game is when the game quits. Returns 0 when the code
/// finished and -1 when it raised. Call it on the main thread from a run loop callback, never from a main-queue block:
/// the game's event loop runs the main run loop itself, and a main-queue block would hold up the main queue for as long
/// as the game runs.
int vndegen_renpy_run(const char *home, const char *code);

/// What the hub can ask of the running game. The game handles requests the next time it checks, a few times a second.
enum {
    VNDEGEN_RENPY_OPEN_MENU = 1,
    /// Quit as Ren'Py's Quit button does once the player confirms.
    VNDEGEN_RENPY_QUIT = 2,
};
void vndegen_renpy_request(int request);

/// While paused, the game stops drawing and reading input, and the main run loop keeps running for the hub.
void vndegen_renpy_set_paused(int paused);

/// Called on the main thread with SDL's UIWindow once Ren'Py's window and its view exist, and again whenever Ren'Py
/// sets its display mode.
void vndegen_renpy_set_window_handler(void (*handler)(void *window));

/// App life cycle changes, which SDL otherwise learns from its own app delegate. Ren'Py saves and stops drawing in the
/// background.
enum {
    VNDEGEN_RENPY_WILL_RESIGN_ACTIVE,
    VNDEGEN_RENPY_DID_ENTER_BACKGROUND,
    VNDEGEN_RENPY_WILL_ENTER_FOREGROUND,
    VNDEGEN_RENPY_DID_BECOME_ACTIVE,
};
void vndegen_renpy_app_state(int state);

// Called by VNDegen's launcher inside the game, through ctypes.

/// The requests made since the last call, as a bit set.
int vndegen_renpy_take_requests(void);
int vndegen_renpy_is_paused(void);
/// Runs the main run loop until the game is resumed.
void vndegen_renpy_wait_while_paused(void);
/// Runs the main run loop briefly, for waits inside the game.
void vndegen_renpy_pump(void);
void vndegen_renpy_window_ready(void);

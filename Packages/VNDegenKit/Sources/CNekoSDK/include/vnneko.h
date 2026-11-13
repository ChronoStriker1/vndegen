// VNDegen's NekoSDK runtime: its C interface, the only part of the runtime the app calls.
//
// The runtime draws each frame on the CPU into an RGBA buffer and mixes its sound into 44.1 kHz stereo float samples.
// The host presents the frame and plays the samples; it owns the window, the clock and the input. Call every function
// on one thread (the host's main thread), except vnneko_audio_render, which the audio thread calls. One engine runs one
// game.
#ifndef VNNEKO_H
#define VNNEKO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNNekoEngine VNNekoEngine;

// A movie decoder the host lends the runtime, for the games' WMV movies: the app's is vnffmovie in the KiriKiri
// framework, found with dlsym, and the Linux host's the same code built against the system's FFmpeg. These are its
// five functions, with the same layouts and meanings as `Engines/Shared/vnffmovie.h`.
typedef struct VNNekoMovieInfo {
    int width, height;
    double fps;
    double duration;
    int audioRate, audioChannels;
} VNNekoMovieInfo;

typedef int64_t (*VNNekoMovieRead)(void *user, uint64_t offset, void *buffer, size_t length);

typedef struct VNNekoMovies {
    void *(*open)(VNNekoMovieRead read, void *user, uint64_t size, int loop, VNNekoMovieInfo *info, char *error,
                  size_t errorSize);
    const uint8_t *(*frame)(void *movie, double time, int *changed, int *ended);
    size_t (*audio)(void *movie, float *out, size_t frames);
    void (*seek)(void *movie, double time);
    void (*close)(void *movie);
} VNNekoMovies;

typedef struct VNNekoLaunch {
    const char *gameRoot;  // folder holding the game's .pak archives
    const char *dataDir;   // the game's data folder, where saves go, under AppData/Roaming as on Windows
    const char *logPath;   // file the runtime's log is appended to; NULL logs to stderr
    const char *fontDir;   // folder holding the Windows font pack's fonts.tsv
    // The movie decoder, copied at start. NULL when the host has none: each movie is then logged and left out, and
    // the game goes on without it.
    const VNNekoMovies *movies;
} VNNekoLaunch;

// The app's movie decoder, vnffmovie, found with dlsym among the libraries the process has loaded: the KiriKiri
// framework exports it. NULL, with the missing function's name in vnneko_start_error(), when it isn't there.
const VNNekoMovies *vnneko_find_movies(void);

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
// A notch of the wheel is positive away from the player.
void vnneko_mouse_wheel(VNNekoEngine *e, int notches);
// Moves the pointer to the nearest control in that direction, for a controller or arrow keys. False when this screen has no
// controls to move between, so the host can treat the press as something else.
bool vnneko_move_focus(VNNekoEngine *e, int dx, int dy);
bool vnneko_user_data_directory(const char *gameRoot, char *out, size_t size);
// Whether VNDegen has this game's menu table yet.
bool vnneko_is_supported(const char *gameRoot);
// Runs through text and waits for as long as it's on, as holding Ctrl does.
void vnneko_set_skip(VNNekoEngine *e, bool on);

// A click on a save card's comment asks the host for one. Returns the card's slot, with its comment now in `utf8`, once
// per click, or 0 when nothing is asked. The host shows a text field and answers with vnneko_set_comment, which writes
// the comment into that save, cut to the 36 bytes of CP932 the PC's box takes. Cancelling just doesn't answer.
int vnneko_take_comment_request(VNNekoEngine *e, char *utf8, size_t size);
bool vnneko_set_comment(VNNekoEngine *e, int slot, const char *utf8);
// Something the title asks the host to show, once per click: kind "alert" with its text (the PC's Version info box),
// "manual" with the full path of the game's own manual page, to show in a web view that loads only that folder, or
// "url" with a web page the PC opens in a browser, to open only after asking the player. False when nothing's asked.
// A press through vnneko_debug_press never asks for the web page.
bool vnneko_take_host_request(VNNekoEngine *e, char *kind, size_t kindSize, char *payload, size_t payloadSize);
// Stops the clock and sound. vnneko_step does nothing while paused.
void vnneko_set_paused(VNNekoEngine *e, bool paused);

// The host window, for Config's screen mode: whether it can go full screen, as a Mac's window can and an iPad's can't,
// and whether it is. Call it each frame; the screen mode buttons show it.
void vnneko_set_window_state(VNNekoEngine *e, bool canFullScreen, bool fullScreen);
// The size the host's view gives the game, in any unit: a game with a 4:3 layout (init.ini's MsgWinSquare) draws in it
// when the view is nearer 4:3 than 16:9 and Config leaves the shape automatic. Call it each frame; vnneko_frame's size
// follows.
void vnneko_set_view_size(VNNekoEngine *e, float width, float height);
// The screen mode Config's buttons asked for since the last call: 0 a window, 1 full screen, -1 none.
int vnneko_take_window_request(VNNekoEngine *e);

// Whether Config asks for the game to stop while the app isn't active. The host pauses it with vnneko_set_paused.
bool vnneko_pauses_when_inactive(VNNekoEngine *e);
// Config's confirm key: 0 for Space, 1 for Enter. The other one acts as the right button.
int vnneko_confirm_key(VNNekoEngine *e);

// Frees the engine.
void vnneko_stop(VNNekoEngine *e);

// A sentence for the player about the most recent problem, such as a missing file, or NULL.
const char *vnneko_problem(VNNekoEngine *e);

// For logs and test hosts: the script, the node, and what the game waits for.
const char *vnneko_debug_status(VNNekoEngine *e);
// For test hosts: whether the game waits for a click now, and how many text boxes it has shown.
bool vnneko_debug_waiting_for_click(VNNekoEngine *e);
int vnneko_debug_text_count(VNNekoEngine *e);
int vnneko_debug_text_advance(VNNekoEngine *e, uint32_t codepoint);
// For test hosts: presses a button of the menu on screen by its action, such as "start".
bool vnneko_debug_press(VNNekoEngine *e, const char *action);
// For test hosts: the options of the choice on screen, or -1, and picking one.
int vnneko_debug_choice_count(VNNekoEngine *e);
void vnneko_debug_choose(VNNekoEngine *e, int index);
// For test hosts: starts this scene script and runs it without waiting up to the node, or a flow script (under `flow\`)
// from that node, or from its start for 0 or less. False when it is not there. A scene started so in a game with flow
// scripts has no flow behind it, and a save made after it names none, which the PC game refuses to load; a story
// table's game goes on from its table, as a load does.
bool vnneko_debug_jump(VNNekoEngine *e, const char *script, int node);
// For tests: the stored size of the file the game's archives and patches give for this path, or -1 when none has it.
int64_t vnneko_debug_file_size(const char *gameRoot, const char *path);
// For tests: the name a save gives that file, which for a file a patch provides is the patch's name for it.
bool vnneko_debug_saved_name(const char *gameRoot, const char *path, char *out, size_t size);
// For measuring against the PC: table lines read ahead of each game's own, so their settings win. Call before vnneko_start.
void vnneko_debug_table_prefix(const char *text);
void vnneko_debug_host_taps(VNNekoEngine *e, bool on);
int vnneko_debug_choice_result(VNNekoEngine *e);
bool vnneko_debug_choice_kept(VNNekoEngine *e);
bool vnneko_debug_choice_line(VNNekoEngine *e);
// For tests: saves to a slot, loads one, or turns a card's lock on or off, as the save and load screens do.
bool vnneko_debug_save(VNNekoEngine *e, int slot);
bool vnneko_debug_load(VNNekoEngine *e, int slot);
void vnneko_debug_toggle_lock(VNNekoEngine *e, int slot);
// For tests: the value of the global (scope 0) or scenario variable (1) with this number.
int vnneko_debug_variable(VNNekoEngine *e, int scope, int number);
// For tests: whether a sound still plays in this channel of music (kind 0), effects (1) or voices (2).
bool vnneko_debug_sound_playing(VNNekoEngine *e, int kind, int channel);
// For tests: a save's plain bytes read with the layout of the game whose UserDataDirectory this is, lower case, and
// built again into `out`. Returns the built save's size, which may be more than `outSize` holds, or 0 when the save
// can't be read or the folder has no layout.
size_t vnneko_debug_rebuild_save(const char *userDataDirectory, const uint8_t *plain, size_t size, uint8_t *out, size_t outSize);
// For tests: the number of the variable a script names by its place in macrolist.ini's list, global (scope 0) or
// scenario (1), or -1 when there's no such place.
int vnneko_debug_macro_number(const char *gameRoot, int scope, int index);
// For tests: the first pixel, as RGBA, of the picture at this path read with no art set, then with the patch `artSet`,
// then with none again, from one picture cache. False when it can't be read.
bool vnneko_debug_art_pixels(const char *gameRoot, const char *path, const char *artSet, uint32_t out[3]);

#ifdef __cplusplus
}
#endif

#endif

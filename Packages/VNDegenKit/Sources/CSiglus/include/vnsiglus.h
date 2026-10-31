// VNDegen's C interface to siglus_rs, which runs SiglusEngine and RealLive games. The VNSiglus framework exports one
// symbol, vnsiglus_get_api_v1, which returns this table. Engines/Siglus/vnsiglus implements it.
//
// Every function runs on the thread that created the game, the main thread in VNDegen. Strings are UTF-8. A string the
// engine returns through `error` is freed with `string_free`.
#ifndef VNSIGLUS_H
#define VNSIGLUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNSiglusGame VNSiglusGame;
typedef struct VNRealLiveGame VNRealLiveGame;

// A message box the game shows, which the host answers with `siglus_answer` once the player picks a button. `kind` is
// 0 OK, 1 OK and Cancel, 2 Yes and No, 3 Yes, No and Cancel. The strings are valid only during the call.
typedef void (*VNSiglusMessageBoxFn)(void *context, uint64_t request, int32_t kind, const char *title,
                                     const char *message);

// The view kinds `siglus_create` takes.
enum { VNSIGLUS_VIEW_UIKIT = 1, VNSIGLUS_VIEW_APPKIT = 2 };

// The buttons `siglus_button` and `reallive_button` take.
enum { VNSIGLUS_BUTTON_LEFT = 0, VNSIGLUS_BUTTON_RIGHT = 1, VNSIGLUS_BUTTON_MIDDLE = 2 };

// What `siglus_step` and `reallive_step` return.
enum { VNSIGLUS_RUNNING = 0, VNSIGLUS_ENDED = 1, VNSIGLUS_FAILED = 2 };

typedef struct VNSiglusApiV1 {
    // sizeof(VNSiglusApiV1) in the engine, so a host built against an older table can tell what it has.
    uint32_t struct_size;

    void (*string_free)(char *string);
    // Lines the engines log, appended to the file at `path`, or to nowhere for NULL.
    void (*set_log_file)(const char *path);
    // The font used for MS PGothic and for any face no game or system font provides, read from `path`. Call it once,
    // before the first game. Returns 1 when the font loaded.
    int32_t (*set_default_font)(const char *path);

    // SiglusEngine. `view` is a UIView whose layer is a CAMetalLayer, or an NSView backed by one, `width` and `height`
    // its size in pixels and `scale` its pixels per point. The game reads and writes `savedata/` in `root`, as on a PC.
    // Returns NULL and an error message on failure.
    VNSiglusGame *(*siglus_create)(int32_t view_kind, void *view, uint32_t width, uint32_t height, double scale,
                                   const char *root, char **error);
    // Runs the game for one display frame and draws it.
    int32_t (*siglus_step)(VNSiglusGame *game, uint32_t elapsed_ms);
    // The view's new size in pixels. The game is drawn at its own aspect ratio in the middle, with bars beside or
    // above it.
    void (*siglus_resize)(VNSiglusGame *game, uint32_t width, uint32_t height, double scale);
    // The game's screen size, which pointer positions are in.
    void (*siglus_screen_size)(const VNSiglusGame *game, uint32_t *width, uint32_t *height);
    void (*siglus_pointer)(VNSiglusGame *game, double x, double y);
    void (*siglus_button)(VNSiglusGame *game, int32_t button, int32_t down);
    // Wheel notches, positive away from the player.
    void (*siglus_wheel)(VNSiglusGame *game, int32_t notches);
    // A Windows virtual-key code going down or up, and whether it is a key repeat.
    void (*siglus_key)(VNSiglusGame *game, int32_t virtual_key, int32_t down, int32_t repeat);
    void (*siglus_text)(VNSiglusGame *game, const char *text);
    // 1 while a text field of the game takes typed text.
    int32_t (*siglus_wants_text)(const VNSiglusGame *game);
    // Silences the game while the host's menu is open. The host stops calling `siglus_step` meanwhile.
    void (*siglus_set_paused)(VNSiglusGame *game, int32_t paused);
    void (*siglus_set_message_box)(VNSiglusGame *game, VNSiglusMessageBoxFn show, void *context);
    // The button picked, by its place in the order `kind` names them: 0 OK or Yes, 1 Cancel or No, 2 the Cancel after
    // Yes and No.
    void (*siglus_answer)(VNSiglusGame *game, uint64_t request, int64_t button);
    // Saves the game's global data and ends it.
    void (*siglus_destroy)(VNSiglusGame *game);

    // RealLive. The game reads `root` and keeps its saves in `saves`. Returns NULL and an error message on failure.
    VNRealLiveGame *(*reallive_open)(const char *root, const char *saves, char **error);
    int32_t (*reallive_step)(VNRealLiveGame *game, uint32_t elapsed_ms);
    // The frame `reallive_step` drew, RGBA with 4 bytes a pixel and no padding, valid until the next step.
    const uint8_t *(*reallive_frame)(const VNRealLiveGame *game, uint32_t *width, uint32_t *height);
    void (*reallive_pointer)(VNRealLiveGame *game, int32_t x, int32_t y);
    void (*reallive_button)(VNRealLiveGame *game, int32_t button, int32_t down);
    void (*reallive_wheel)(VNRealLiveGame *game, int32_t notches);
    void (*reallive_key)(VNRealLiveGame *game, int32_t virtual_key, int32_t down);
    void (*reallive_text)(VNRealLiveGame *game, const char *text);
    // 1 when the game draws no pointer of its own and wants the system's.
    int32_t (*reallive_cursor_visible)(const VNRealLiveGame *game);
    void (*reallive_set_paused)(VNRealLiveGame *game, int32_t paused);
    // Saves the game's global data and ends it.
    void (*reallive_close)(VNRealLiveGame *game);
} VNSiglusApiV1;

typedef const VNSiglusApiV1 *(*VNSiglusGetApiV1Fn)(void);

#ifdef __cplusplus
}
#endif

#endif

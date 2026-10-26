#include "vndegen_renpy.h"

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

// From Ren'Py's static libraries, which the app target links. librenpy registers Ren'Py's compiled modules with Python.
extern void init_librenpy(void);
extern int Py_IsInitialized(void);
extern void Py_InitializeEx(int install_signal_handlers);
extern int PyRun_SimpleString(const char *code);
extern void SDL_SetMainReady(void);
extern void SDL_iPhoneSetEventPump(int enabled);
extern void SDL_OnApplicationWillResignActive(void);
extern void SDL_OnApplicationDidEnterBackground(void);
extern void SDL_OnApplicationWillEnterForeground(void);
extern void SDL_OnApplicationDidBecomeActive(void);
extern void *SDL_GL_GetCurrentWindow(void);

// SDL_SysWMinfo as SDL 2 lays it out for UIKit. The app has no SDL headers, only Ren'Py's prebuilt libraries.
typedef struct {
    struct { unsigned char major, minor, patch; } version;
    int subsystem;
    union {
        struct { void *window; unsigned framebuffer, colorbuffer, resolveFramebuffer; } uikit;
        unsigned char dummy[64];
    } info;
} WindowInfo;
extern int SDL_GetWindowWMInfo(void *window, WindowInfo *info);

// Python reaches these through ctypes, so they must stay in the app's exported symbols.
#define EXPORTED __attribute__((visibility("default"), used))

// Ren'Py's iOS Python sends standard output and error, one line at a time, to this class, which renios's own Xcode
// project defines. Here the lines go to the standard error, which VNDegen points at the game's log file.
@interface Log : NSObject
+ (void)log:(char *)line;
@end

@implementation Log
+ (void)log:(char *)line {
    fprintf(stderr, "%s\n", line);
}
@end

// Ren'Py's in-app purchase support looks up this class on iOS, whichever store a game uses. renios's own version talks
// to the App Store. PC games bought elsewhere have nothing to buy there, so this one reports that payments are off.
@interface IAPHelper : NSObject
@property NSArray *productIdentifiers;
@property NSString *dialogTitle;
@property int finished;
@end

@implementation IAPHelper
- (BOOL)canMakePayments { return NO; }
- (void)validateProductIdentifiers { self.finished = 1; }
- (void)beginPurchase:(NSString *)identifier { self.finished = 1; }
- (void)restorePurchases { self.finished = 1; }
- (BOOL)hasPurchased:(NSString *)identifier { return NO; }
- (BOOL)hasPurchasedConsumable:(NSString *)identifier { return NO; }
- (BOOL)isDeferred:(NSString *)identifier { return NO; }
- (NSString *)formatPrice:(NSString *)identifier { return nil; }
- (void)requestReview {}
@end

static volatile int requests;
static volatile int paused;
static void (*window_handler)(void *window);

int vndegen_renpy_run(const char *home, const char *code) {
    if (!Py_IsInitialized()) {
        // Ren'Py's own iOS launcher sets RENPY_PLATFORM, which is how Ren'Py knows it runs on iOS.
        setenv("RENPY_PLATFORM", TARGET_OS_SIMULATOR ? "ios-sim-arm64" : "ios-arm64", 1);
        setenv("PYTHONHOME", home, 1);
        setenv("PYTHONNOUSERSITE", "1", 1);
        setenv("PYTHONDONTWRITEBYTECODE", "1", 1);
        setenv("PYTHONUNBUFFERED", "1", 1);
        setenv("PYTHONUTF8", "1", 1);
        unsetenv("PYTHONPATH");
        init_librenpy();
        SDL_SetMainReady();
        Py_InitializeEx(0);
    }
    // SDL runs the main run loop each time Ren'Py reads events, which keeps UIKit and the hub responsive.
    SDL_iPhoneSetEventPump(1);
    int status = PyRun_SimpleString(code);
    SDL_iPhoneSetEventPump(0);
    return status;
}

void vndegen_renpy_request(int request) { requests |= request; }

void vndegen_renpy_set_paused(int value) { paused = value; }

void vndegen_renpy_set_window_handler(void (*handler)(void *window)) { window_handler = handler; }

void vndegen_renpy_app_state(int state) {
    switch (state) {
        case VNDEGEN_RENPY_WILL_RESIGN_ACTIVE: SDL_OnApplicationWillResignActive(); break;
        case VNDEGEN_RENPY_DID_ENTER_BACKGROUND: SDL_OnApplicationDidEnterBackground(); break;
        case VNDEGEN_RENPY_WILL_ENTER_FOREGROUND: SDL_OnApplicationWillEnterForeground(); break;
        case VNDEGEN_RENPY_DID_BECOME_ACTIVE: SDL_OnApplicationDidBecomeActive(); break;
    }
}

EXPORTED int vndegen_renpy_take_requests(void) {
    int taken = requests;
    requests = 0;
    return taken;
}

EXPORTED int vndegen_renpy_is_paused(void) { return paused; }

EXPORTED void vndegen_renpy_wait_while_paused(void) {
    while (paused) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
}

EXPORTED void vndegen_renpy_window_ready(void) {
    void *window = SDL_GL_GetCurrentWindow();
    WindowInfo info = {{2, 0, 22}};
    if (window && window_handler && SDL_GetWindowWMInfo(window, &info)) window_handler(info.info.uikit.window);
}

#else

int vndegen_renpy_run(const char *home, const char *code) { return -1; }
void vndegen_renpy_request(int request) {}
void vndegen_renpy_set_paused(int paused) {}
void vndegen_renpy_set_window_handler(void (*handler)(void *window)) {}
void vndegen_renpy_app_state(int state) {}
int vndegen_renpy_take_requests(void) { return 0; }
int vndegen_renpy_is_paused(void) { return 0; }
void vndegen_renpy_wait_while_paused(void) {}
void vndegen_renpy_window_ready(void) {}

#endif

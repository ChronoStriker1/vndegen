#include "vnkk.h"

#define SDL_MAIN_USE_CALLBACKS
#define SDL_MAIN_NOIMPL
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include "tjsCommHead.h"
#include "Platform.h"
#include "PlatformAudio.h"
#include "PlatformView.h"
#include "TVPCompositor.h"
#include "TVPScript.h"
#include "TVPSystem.h"
#include "TVPWindow.h"
#include "WindowManager.h"
#include "ciphers.h"

#import <Foundation/Foundation.h>
#if TARGET_OS_IOS
#import <GameController/GameController.h>
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

#include <atomic>
#include <cstdio>
#include <string>

static bool started;
static bool running;
static bool paused;
static bool quitRequested;
static Uint64 quitSentAt; // SDL ticks when the close was sent, 0 before
static bool terminated;
static bool finished;
static FILE *logFile;
// Archives can open on the engine's loader threads.
static std::atomic<const char *> problem;

static void writeLog(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    fprintf((FILE *)userdata, "%s\n", message);
    fflush((FILE *)userdata);
}

// Marks the input events vnkk sends, so the filter below can tell them from SDL's own.
static const Uint32 hostDevice = 0x564e4b4b;
// Where the pointer or the last touch was, in window coordinates. Negative before either.
static float pointerX = -1, pointerY = -1;

static void notePointer(float x, float y)
{
    pointerX = x;
    pointerY = y;
}

// The host delivers the keyboard, and on macOS the mouse, from its own views, the way it does for its other engines.
// SDL would deliver some of the same presses a second time, since on macOS it reads whichever AppKit events its event
// pump happens to take off the queue.
static bool SDLCALL filterInput(void *, SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        return event->key.which == hostDevice;
#if TARGET_OS_OSX
    case SDL_EVENT_MOUSE_WHEEL:
        return event->wheel.which == hostDevice;
    case SDL_EVENT_MOUSE_MOTION:
        if (event->motion.which != hostDevice)
            return false;
        notePointer(event->motion.x, event->motion.y);
        return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.which != hostDevice)
            return false;
        notePointer(event->button.x, event->button.y);
        return true;
#else
    case SDL_EVENT_MOUSE_MOTION:
        notePointer(event->motion.x, event->motion.y);
        return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        notePointer(event->button.x, event->button.y);
        return true;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION: {
        int w = 0, h = 0;
        TVPGetWindowSize(&w, &h);
        notePointer(event->tfinger.x * w, event->tfinger.y * h);
        return true;
    }
#endif
    default:
        return true;
    }
}

#if TARGET_OS_OSX
// SDL draws into a window of its own and sizes the game from it. The game belongs in VNDegen's window, so SDL wraps
// an empty window that nobody sees, and the view SDL draws in moves into the host view once SDL has set it up. SDL
// only presents to a window it believes is on screen, so that window stays ordered in, fully transparent, and closed
// to the mouse and to focus.
@interface VNKKSDLWindow : NSWindow
@end

@implementation VNKKSDLWindow
- (BOOL)canBecomeKeyWindow
{
    return NO;
}

- (BOOL)canBecomeMainWindow
{
    return NO;
}
@end

// The view SDL draws in. Its window follows its size, since SDL takes the game's size from the window.
@interface VNKKEngineView : NSView
@property (nonatomic, weak) NSWindow *sdlWindow;
@end

@implementation VNKKEngineView
- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    if (size.width >= 1 && size.height >= 1)
        [self.sdlWindow setContentSize:size];
}

// The host view takes the mouse.
- (NSView *)hitTest:(NSPoint)point
{
    return nil;
}
@end

static VNKKSDLWindow *sdlWindow;
static VNKKEngineView *engineView;

static void prepareHostWindow(NSView *host)
{
    NSRect frame = NSMakeRect(0, 0, MAX(host.bounds.size.width, 1), MAX(host.bounds.size.height, 1));
    sdlWindow = [[VNKKSDLWindow alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskBorderless
                                                   backing:NSBackingStoreBuffered defer:NO];
    sdlWindow.releasedWhenClosed = NO;
    sdlWindow.alphaValue = 0;
    sdlWindow.ignoresMouseEvents = YES;
    sdlWindow.hasShadow = NO;
    sdlWindow.excludedFromWindowsMenu = YES;
    sdlWindow.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorTransient |
                                   NSWindowCollectionBehaviorIgnoresCycle | NSWindowCollectionBehaviorFullScreenAuxiliary;
    engineView = [[VNKKEngineView alloc] initWithFrame:frame];
    sdlWindow.contentView = engineView;
    [sdlWindow orderWindow:NSWindowBelow relativeTo:host.window.windowNumber];
    TVPSetHostWindow((__bridge void *)sdlWindow, (__bridge void *)engineView);
    // Showing its window must not take focus from VNDegen's.
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
}

// SDL put its Metal view in the engine view while creating the renderer, so the engine view can move now.
static void moveIntoHostView(NSView *host)
{
    sdlWindow.contentView = [[NSView alloc] initWithFrame:engineView.frame];
    engineView.sdlWindow = sdlWindow;
    engineView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    engineView.frame = host.bounds;
    [host addSubview:engineView];
}

static void releaseHostWindow()
{
    TVPSetHostWindow(nullptr, nullptr);
    [engineView removeFromSuperview];
    engineView = nil;
    [sdlWindow orderOut:nil];
    [sdlWindow close];
    sdlWindow = nil;
}
#else
// SDL claims the hardware keyboard's GameController handler when it starts and clears it when it quits, and it moves
// the handler to a background queue. VNDegen reads the keyboard through that handler on the main queue, so both are
// put back each time.
static GCKeyboardValueChangedHandler hostKeyHandler;
static dispatch_queue_t hostKeyQueue;

static void saveKeyboardHandler()
{
    GCKeyboard *keyboard = GCKeyboard.coalescedKeyboard;
    hostKeyHandler = keyboard.keyboardInput.keyChangedHandler;
    hostKeyQueue = keyboard.handlerQueue;
}

static void restoreKeyboardHandler()
{
    GCKeyboard *keyboard = GCKeyboard.coalescedKeyboard;
    keyboard.handlerQueue = hostKeyQueue ?: dispatch_get_main_queue();
    keyboard.keyboardInput.keyChangedHandler = hostKeyHandler;
}
#endif

int vnkk_start(const VNKKLaunch *launch)
{
    problem = nullptr;
    if (launch->logPath) {
        logFile = fopen(launch->logPath, "a");
        if (logFile)
            SDL_SetLogOutputFunction(writeLog, logFile);
    }
    std::string exe = NSBundle.mainBundle.executablePath.UTF8String;
    std::string root = std::string("-gameroot=") + launch->gameRoot;
    std::string appData = std::string("-appdatadir=") + (launch->appDataDir ? launch->appDataDir : launch->gameRoot);
    std::string personal = std::string("-personaldir=") + (launch->personalDir ? launch->personalDir : launch->gameRoot);
    const char *argv[] = {exe.c_str(), launch->projectData, root.c_str(), appData.c_str(), personal.c_str(),
                          "-render=software", nullptr};
    // VNDegen owns the process's entry point, so SDL never runs its own main.
    SDL_SetMainReady();
    started = true;
    pointerX = pointerY = -1;
#if TARGET_OS_OSX
    NSView *host = (__bridge NSView *)launch->hostView;
    if (host)
        prepareHostWindow(host);
#else
    saveKeyboardHandler();
#endif
    // An exception the engine leaves uncaught would end the host app along with the game.
    SDL_AppResult result = SDL_APP_FAILURE;
    try {
        result = SDL_AppInit(nullptr, 6, (char **)argv);
    } catch (...) {
        SDL_Log("vnkk: the game stopped on an error while starting");
    }
#if TARGET_OS_OSX
    if (host && engineView)
        moveIntoHostView(host);
#else
    restoreKeyboardHandler();
#endif
    if (result != SDL_APP_CONTINUE)
        return -1;
#if TARGET_OS_OSX
    // Without a host view, as in KiriKiriHelper, SDL's own window and input are all there is.
    if (host)
#endif
        SDL_SetEventFilter(filterInput, nullptr);
    return 0;
}

static const Uint64 forcedQuitDelayMS = 3000;

// KAG and the frameworks built on it ask "quit?" before closing unless askOnClose is off.
static void closeWithoutConfirmation()
{
    try {
        TVPExecuteScript(TJS_N("if (typeof global.kag == 'Object' && isvalid global.kag && "
                               "typeof global.kag.askOnClose != 'undefined') global.kag.askOnClose = false;"));
    } catch (...) {
        SDL_Log("vnkk: could not turn off the quit confirmation");
    }
    // A script error while the game closes stops its teardown partway, with its window half
    // invalidated, so further events would only raise more errors. The game is ended at once.
    try {
        if (TVPMainWindow)
            TVPMainWindow->Close();
    } catch (const eTJS &e) {
        SDL_Log("vnkk: closing the game raised %s, terminating it", e.GetMessage().AsStdString().c_str());
        terminated = true;
        TVPTerminateAsync();
    }
}

// Invalidates the main window without its close query, as terminating does on Windows. Its finalize
// runs, where games that are not built on KAG save their system data. Engine shutdown deletes the
// windows without it.
static void closeMainWindowAnyway()
{
    try {
        TVPExecuteScript(TJS_N("if (Window.mainWindow !== null && isvalid Window.mainWindow) invalidate Window.mainWindow;"));
    } catch (const eTJS &e) {
        SDL_Log("vnkk: closing the game's window raised %s", e.GetMessage().AsStdString().c_str());
    }
}

int vnkk_iterate(void)
{
    if (finished)
        return 1;
    if (running || paused)
        return 0;
    running = true;
    if (quitRequested && !quitSentAt) {
        quitSentAt = SDL_GetTicks() | 1;
        closeWithoutConfirmation();
    } else if (quitSentAt && !terminated && SDL_GetTicks() - quitSentAt > forcedQuitDelayMS) {
        // A game with its own quit dialog ignores askOnClose and is still waiting on it.
        terminated = true;
        SDL_Log("vnkk: the game did not close, terminating it");
        TVPDoSaveSystemVariables();
        closeMainWindowAnyway();
        TVPTerminateAsync();
    }
    try {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            SDL_AppEvent(nullptr, &event);
        finished = SDL_AppIterate(nullptr) != SDL_APP_CONTINUE;
    } catch (...) {
        SDL_Log("vnkk: the game stopped on an error");
        finished = true;
    }
    running = false;
    return finished;
}

static Uint32 windowID()
{
    SDL_Window *window = TVPGetSDLWindow();
    return window ? SDL_GetWindowID(window) : 0;
}

// Maps a point normalized over the view the game draws in, which is the whole SDL window, to window coordinates.
static void toWindow(float x, float y, float *wx, float *wy)
{
    int w = 0, h = 0;
    TVPGetWindowSize(&w, &h);
    *wx = x * w;
    *wy = y * h;
}

void vnkk_key(int sdlScancode, bool down)
{
    SDL_Event event = {};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.timestamp = SDL_GetTicksNS();
    event.key.windowID = windowID();
    event.key.which = hostDevice;
    event.key.scancode = (SDL_Scancode)sdlScancode;
    event.key.key = SDL_GetKeyFromScancode((SDL_Scancode)sdlScancode, SDL_KMOD_NONE, false);
    event.key.down = down;
    SDL_PushEvent(&event);
}

void vnkk_mouse_button(int button, bool down, float x, float y)
{
    SDL_Event event = {};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.timestamp = SDL_GetTicksNS();
    event.button.windowID = windowID();
    event.button.which = hostDevice;
    event.button.button = (Uint8)button;
    event.button.down = down;
    event.button.clicks = 1;
    toWindow(x, y, &event.button.x, &event.button.y);
    SDL_PushEvent(&event);
}

void vnkk_mouse_move(float x, float y)
{
    SDL_Event event = {};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.timestamp = SDL_GetTicksNS();
    event.motion.windowID = windowID();
    event.motion.which = hostDevice;
    toWindow(x, y, &event.motion.x, &event.motion.y);
    SDL_PushEvent(&event);
}

void vnkk_click(int button, bool down)
{
    int w = 0, h = 0;
    TVPGetWindowSize(&w, &h);
    float x = pointerX < 0 || !w ? 0.5f : pointerX / w;
    float y = pointerY < 0 || !h ? 0.5f : pointerY / h;
    vnkk_mouse_button(button, down, x, y);
}

void vnkk_wheel(float notches)
{
    SDL_Event event = {};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.timestamp = SDL_GetTicksNS();
    event.wheel.windowID = windowID();
    event.wheel.which = hostDevice;
    // krkrsdl3 hands Kirikiri the opposite sign of SDL's wheel, so a wheel turned away arrives as SDL's toward.
    event.wheel.y = -notches;
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    SDL_PushEvent(&event);
}

void vnkk_set_paused(bool value)
{
    if (paused == value)
        return;
    paused = value;
    TVPSetClockPaused(value);
    TVPSetSoundPaused(value);
}

void vnkk_request_quit(void)
{
    quitRequested = true;
    vnkk_set_paused(false);
}

void vnkk_stop(void)
{
    // The engine clock and audio would stay paused into the next game.
    vnkk_set_paused(false);
    if (started)
        SDL_AppQuit(nullptr, finished ? SDL_APP_SUCCESS : SDL_APP_FAILURE);
    started = running = quitRequested = terminated = finished = false;
    quitSentAt = 0;
#if TARGET_OS_OSX
    releaseHostWindow();
#else
    restoreKeyboardHandler();
#endif
    if (logFile) {
        SDL_SetLogOutputFunction(SDL_GetDefaultLogOutputFunction(), nullptr);
        fclose(logFile);
        logFile = nullptr;
    }
}

void *vnkk_view(void)
{
#if TARGET_OS_IOS
    SDL_Window *window = TVPGetSDLWindow();
    if (!window)
        return nullptr;
    UIWindow *uiWindow = (__bridge UIWindow *)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
    // SDL's own full-screen window would cover the host's views once its content moves there.
    uiWindow.hidden = YES;
    return (__bridge void *)uiWindow.rootViewController.view;
#else
    return nullptr;
#endif
}

void vnkk_record_problem(const char *sentence)
{
    problem = sentence;
}

const char *vnkk_problem(void)
{
    return problem;
}

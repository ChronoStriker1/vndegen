#include "vnkk.h"

#define SDL_MAIN_USE_CALLBACKS
#define SDL_MAIN_NOIMPL
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_events.h>
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
#import <UIKit/UIKit.h>
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
    const char *argv[] = {exe.c_str(), launch->projectData, root.c_str(), "-render=software", nullptr};
    // VNDegen owns the process's entry point, so SDL never runs its own main.
    SDL_SetMainReady();
    started = true;
    if (SDL_AppInit(nullptr, 4, (char **)argv) != SDL_APP_CONTINUE)
        return -1;
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
        TVPTerminateAsync();
    }
    SDL_Event event;
    while (SDL_PollEvent(&event))
        SDL_AppEvent(nullptr, &event);
    finished = SDL_AppIterate(nullptr) != SDL_APP_CONTINUE;
    running = false;
    return finished;
}

static Uint32 windowID()
{
    SDL_Window *window = TVPGetSDLWindow();
    return window ? SDL_GetWindowID(window) : 0;
}

// Maps a point normalized over the game's drawn area to window coordinates.
static void toWindow(float x, float y, float *wx, float *wy)
{
    if (TVPSprite *sprite = krkrsdl3::KRKR_Get_Current_Sprite()) {
        *wx = sprite->xPos + x * sprite->scale * sprite->width;
        *wy = sprite->yPos + y * sprite->scale * sprite->height;
        return;
    }
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
    toWindow(x, y, &event.motion.x, &event.motion.y);
    SDL_PushEvent(&event);
}

void vnkk_wheel(float dy)
{
    SDL_Event event = {};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.timestamp = SDL_GetTicksNS();
    event.wheel.windowID = windowID();
    event.wheel.y = dy;
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

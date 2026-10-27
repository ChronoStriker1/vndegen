// KiriKiriRestart: runs several games one after another in one process, the way VNDegen's app
// does when the player quits a game and starts another, and reports what each game leaves behind.
//   KiriKiriRestart <log folder> <game root> <data> <title marker> [<game root> <data> <title marker>]...
// Each game runs until a line of its log ends with the title marker or titleTimeout passes, then
// quits through vnkk_request_quit. A game still short of its title after clickAfter gets a left
// click in the middle of the screen every clickInterval, as a player skipping its intro would give
// it. After each game's vnkk_stop, stdout gets the resident size, the resident size once malloc has
// returned its free pages to the system, and the number of open files. Each game is paused for
// pauseTime at its title, and the next game's engine clock must still start level with SDL's.
// At its title each game then opens a modal window through script, as KAG's Yes/No dialog is opened,
// and a timer of the window's own closes it. The window's loop must draw and return within
// modalTimeout. Its timer can only fire once a pass of the loop has run the engine's events. The
// first game opens its window late in its engine time and a later one must open it earlier in its
// own, so a timestamp an earlier game left behind would hold the window open.
// The games draw in a window's view and the run loop turns between frames, as in VNDegen's app,
// and every frame runs with an event of the app's queued, which the frame must leave to the app.
// A title marker of "movie" quits the game midway through its first movie instead: movieTime after
// the log shows the movie opening, while its decoder is still reading.
// Exits nonzero if a game missed its title, did not quit, started with a clock behind SDL's, had a
// frame take the app's events, or held its modal window open past modalTimeout, and also if no later
// game opened its window early enough for that check to mean anything.
#include "tjsCommHead.h"
#include "Platform.h"
#include "TVPScript.h"
#include "vnkk.h"

#include <SDL3/SDL_timer.h>

#import <AppKit/AppKit.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <malloc/malloc.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

using Clock = std::chrono::steady_clock;

static const auto frameTime = std::chrono::microseconds(16667);
static const auto titleTimeout = std::chrono::seconds(30);
static const auto quitTimeout = std::chrono::seconds(15);
static const auto clickAfter = std::chrono::seconds(10);
static const auto clickInterval = std::chrono::seconds(2);
static const auto pauseTime = std::chrono::milliseconds(500);
static const auto movieTime = std::chrono::milliseconds(1500);
static const auto modalTimeout = std::chrono::seconds(2);
// A window still open this long ends the run, so a loop that never returns fails instead of hanging.
static const auto modalWatchdog = std::chrono::seconds(20);
// The first game opens its window this late in its own engine time, and a later game must open one at
// least modalGap earlier in its own, so a timestamp left over from the earlier game would hold it open.
static const tjs_uint64 firstModalTick = 20000;
static const tjs_uint64 modalGap = 2500;

static std::string readLog(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    std::stringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

// Whether a line of the log ends with the marker.
static bool logHasLineEnding(const std::string &path, const std::string &marker)
{
    return readLog(path).find(marker + "\n") != std::string::npos;
}

static double residentMB()
{
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) != KERN_SUCCESS)
        return -1;
    return info.resident_size / 1048576.0;
}

static int openFiles()
{
    int count = 0;
    for (int fd = 0, limit = getdtablesize(); fd < limit; fd++)
        count += fcntl(fd, F_GETFD) != -1;
    return count;
}

// Handles the app's events until `until`, as VNDegen's run loop does between frames.
static void runHost(Clock::time_point until)
{
    for (;;) {
        double seconds = std::max(std::chrono::duration<double>(until - Clock::now()).count(), 0.0);
        NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate dateWithTimeIntervalSinceNow:seconds]
                                               inMode:NSDefaultRunLoopMode dequeue:YES];
        if (!event)
            return;
        [NSApp sendEvent:event];
    }
}

static bool appEventsTaken;

// Runs a frame with an event of the app's queued and checks that the frame left it there.
static int iterate()
{
    [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint modifierFlags:0
                                       timestamp:0 windowNumber:0 context:nil subtype:0 data1:0 data2:0]
             atStart:NO];
    int quit = vnkk_iterate();
    appEventsTaken |= ![NSApp nextEventMatchingMask:NSEventMaskApplicationDefined untilDate:NSDate.distantPast
                                             inMode:NSDefaultRunLoopMode dequeue:YES];
    return quit;
}

// Opens a modal window that its own timer closes after 100 ms. Returns the seconds the window's
// loop held the frame, or a negative number if the script failed.
static double runModalWindow()
{
    Clock::time_point start = Clock::now();
    try {
        TVPExecuteScript(TJS_N("global.vnkkModalCheck = %[];"
                               "with (global.vnkkModalCheck) {"
                               "  .window = new Window();"
                               "  .timer = new Timer((function { this.timer.enabled = false; this.window.close(); } incontextof global.vnkkModalCheck), '');"
                               "  .timer.interval = 100;"
                               "  .timer.enabled = true;"
                               "  .window.showModal();"
                               "  invalidate .timer;"
                               "  invalidate .window;"
                               "}"
                               "delete global.vnkkModalCheck;"));
    } catch (const eTJS &e) {
        fprintf(stderr, "modal window: %s\n", e.GetMessage().AsStdString().c_str());
        return -1;
    }
    return std::chrono::duration<double>(Clock::now() - start).count();
}

// Runs frames until done() or the deadline. Returns whether done() became true.
template <typename Done> static bool runUntil(Clock::duration timeout, Done done)
{
    Clock::time_point deadline = Clock::now() + timeout;
    while (Clock::now() < deadline) {
        Clock::time_point frameStart = Clock::now();
        @autoreleasepool {
            if (done())
                return true;
        }
        runHost(frameStart + frameTime);
    }
    return false;
}

int main(int argc, char *argv[])
{
    if (argc < 5 || (argc - 2) % 3) {
        fprintf(stderr, "usage: %s <log folder> <game root> <data> <title marker> [...]\n", argv[0]);
        return 2;
    }
    std::string logFolder = argv[1];
    [NSApplication sharedApplication];
    NSApp.activationPolicy = NSApplicationActivationPolicyAccessory;
    [NSApp finishLaunching];
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1280, 720) styleMask:NSWindowStyleMaskTitled
                                                     backing:NSBackingStoreBuffered defer:NO];
    [window orderFrontRegardless];
    int failures = 0;
    tjs_uint64 lastModalTick = 0;
    bool modalAfterGap = false;
    for (int i = 2, n = 1; i < argc; i += 3, n++) {
        const char *root = argv[i], *data = argv[i + 1], *marker = argv[i + 2];
        bool midMovie = !strcmp(marker, "movie");
        std::string log = logFolder + "/" + std::to_string(n) + ".log";
        remove(log.c_str());
        printf("game %d: %s\n", n, root);
        fflush(stdout);

        VNKKLaunch launch = {};
        launch.gameRoot = root;
        launch.projectData = data;
        launch.logPath = log.c_str();
        launch.hostView = (__bridge void *)window.contentView;
        appEventsTaken = false;
        Clock::time_point start = Clock::now();
        bool started = vnkk_start(&launch) == 0;
        // The engine clock leaves out paused time, and this game has not been paused yet.
        tjs_uint64 engineTicks = TVPGetRoughTickCount();
        int64_t clockBehind = started ? (int64_t)(SDL_GetTicks() - engineTicks) : 0;
        bool quitEarly = false;
        Clock::time_point nextClick = start + clickAfter;
        Clock::time_point movieOpened = Clock::time_point::max();
        bool atTitle = started && runUntil(titleTimeout, [&] {
            if (iterate()) {
                quitEarly = true;
                return true;
            }
            if (midMovie) {
                if (movieOpened == Clock::time_point::max() && readLog(log).find("movie: opened ") != std::string::npos)
                    movieOpened = Clock::now();
                return movieOpened != Clock::time_point::max() && Clock::now() >= movieOpened + movieTime;
            }
            if (logHasLineEnding(log, marker))
                return true;
            if (Clock::now() >= nextClick) {
                nextClick += clickInterval;
                vnkk_mouse_move(0.5f, 0.5f);
                vnkk_mouse_button(1, true, 0.5f, 0.5f);
                vnkk_mouse_button(1, false, 0.5f, 0.5f);
            }
            return false;
        }) && !quitEarly;
        double titleSeconds = std::chrono::duration<double>(Clock::now() - start).count();

        if (atTitle && !midMovie) {
            vnkk_set_paused(true);
            std::this_thread::sleep_for(pauseTime);
            vnkk_set_paused(false);
        }

        bool modalClosed = true;
        char modalReport[96] = "no modal window";
        if (atTitle && !midMovie) {
            if (!lastModalTick)
                runUntil(std::chrono::milliseconds(firstModalTick + 5000),
                         [] { return iterate() != 0 || TVPGetRoughTickCount() >= firstModalTick; });
            tjs_uint64 tick = TVPGetRoughTickCount();
            if (lastModalTick >= tick + modalGap)
                modalAfterGap = true;
            std::mutex watchdogMutex;
            std::condition_variable watchdogWake;
            bool windowReturned = false;
            std::thread watchdog([&] {
                std::unique_lock<std::mutex> lock(watchdogMutex);
                if (!watchdogWake.wait_for(lock, modalWatchdog, [&] { return windowReturned; })) {
                    fprintf(stderr, "game %d: modal window still open after %lld s\n", n, (long long)modalWatchdog.count());
                    _exit(3);
                }
            });
            double seconds = runModalWindow();
            {
                std::lock_guard<std::mutex> lock(watchdogMutex);
                windowReturned = true;
            }
            watchdogWake.notify_one();
            watchdog.join();
            lastModalTick = TVPGetRoughTickCount();
            modalClosed = seconds >= 0 && seconds < std::chrono::duration<double>(modalTimeout).count();
            snprintf(modalReport, sizeof modalReport, "modal window at engine tick %llu %s after %.1f s",
                     (unsigned long long)tick, seconds < 0 ? "failed" : "closed", seconds);
        }

        bool quit = !started || quitEarly;
        if (!quit) {
            vnkk_request_quit();
            quit = runUntil(quitTimeout, [] { return iterate() != 0; });
        }
        vnkk_stop();

        double resident = residentMB();
        malloc_zone_pressure_relief(nullptr, 0);
        bool clockKept = clockBehind >= 0 && clockBehind < 100;
        printf("game %d: %s after %.1f s, %s, clock %lld ms behind SDL's at start, resident %.1f MB, %.1f MB after relief, "
               "%d open files, %s, %s\n",
               n, atTitle ? (midMovie ? "mid-movie" : "title") : started ? "no title" : "did not start", titleSeconds,
               quit ? "quit" : "did not quit", (long long)clockBehind, resident, residentMB(), openFiles(),
               appEventsTaken ? "a frame took the app's events" : "frames left the app's events",
               modalReport);
        fflush(stdout);
        failures += !atTitle || !quit || !clockKept || appEventsTaken || !modalClosed;
    }
    printf("%d of %d games failed\n", failures, (argc - 2) / 3);
    if (lastModalTick && !modalAfterGap) {
        printf("no game opened its modal window %llu ms earlier in its engine time than the game before it, so the "
               "check for a timestamp left over from an earlier game did not run\n", (unsigned long long)modalGap);
        return 1;
    }
    return failures ? 1 : 0;
}

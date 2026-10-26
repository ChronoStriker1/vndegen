// KiriKiriRestart: runs several games one after another in one process, the way VNDegen's app
// does when the player quits a game and starts another, and reports what each game leaves behind.
//   KiriKiriRestart <log folder> <game root> <data> <title marker> [<game root> <data> <title marker>]...
// Each game runs until a line of its log ends with the title marker or titleTimeout passes, then
// quits through vnkk_request_quit. A game still short of its title after clickAfter gets a left
// click in the middle of the screen every clickInterval, as a player skipping its intro would give
// it. After each game's vnkk_stop, stdout gets the resident size, the resident size once malloc has
// returned its free pages to the system, and the number of open files. Each game is paused for
// pauseTime at its title, and the next game's engine clock must still start level with SDL's.
// A title marker of "movie" quits the game midway through its first movie instead: movieTime after
// the log shows the movie opening, while its decoder is still reading.
// Exits nonzero if a game missed its title, did not quit, or started with a clock behind SDL's.
#include "tjsCommHead.h"
#include "Platform.h"
#include "vnkk.h"

#include <SDL3/SDL_timer.h>

#import <Foundation/Foundation.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <malloc/malloc.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
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
        std::this_thread::sleep_until(frameStart + frameTime);
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
    int failures = 0;
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
        Clock::time_point start = Clock::now();
        bool started = vnkk_start(&launch) == 0;
        // The engine clock leaves out paused time, and this game has not been paused yet.
        tjs_uint64 engineTicks = TVPGetRoughTickCount();
        int64_t clockBehind = started ? (int64_t)(SDL_GetTicks() - engineTicks) : 0;
        bool quitEarly = false;
        Clock::time_point nextClick = start + clickAfter;
        Clock::time_point movieOpened = Clock::time_point::max();
        bool atTitle = started && runUntil(titleTimeout, [&] {
            if (vnkk_iterate()) {
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

        bool quit = !started || quitEarly;
        if (!quit) {
            vnkk_request_quit();
            quit = runUntil(quitTimeout, [] { return vnkk_iterate() != 0; });
        }
        vnkk_stop();

        double resident = residentMB();
        malloc_zone_pressure_relief(nullptr, 0);
        bool clockKept = clockBehind >= 0 && clockBehind < 100;
        printf("game %d: %s after %.1f s, %s, clock %lld ms behind SDL's at start, resident %.1f MB, %.1f MB after relief, "
               "%d open files\n",
               n, atTitle ? (midMovie ? "mid-movie" : "title") : started ? "no title" : "did not start", titleSeconds,
               quit ? "quit" : "did not quit", (long long)clockBehind, resident, residentMB(), openFiles());
        fflush(stdout);
        failures += !atTitle || !quit || !clockKept;
    }
    printf("%d of %d games failed\n", failures, (argc - 2) / 3);
    return failures ? 1 : 0;
}

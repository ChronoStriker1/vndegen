// KiriKiriHelper: the process VNDegen's macOS app runs a KiriKiri game in.
//   KiriKiriHelper --game-root <dir> --data <path> [--log <file>]
// SIGTERM closes the game through its normal close path, so it saves its system data, and the
// helper exits once the game has quit or after quitTimeout. SIGUSR1 pauses the game and SIGUSR2
// resumes it, through vnkk_set_paused as the app's pause does, for tests.
#include "vnkk.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <thread>

using Clock = std::chrono::steady_clock;

static volatile std::sig_atomic_t terminateRequested = 0;
// 1 to pause, 0 to resume, -1 once handled.
static volatile std::sig_atomic_t pauseRequested = -1;
static const auto frameTime = std::chrono::microseconds(16667);
static const auto quitTimeout = std::chrono::seconds(8);

int main(int argc, char *argv[])
{
    VNKKLaunch launch = {};
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--game-root"))
            launch.gameRoot = argv[i + 1];
        else if (!strcmp(argv[i], "--data"))
            launch.projectData = argv[i + 1];
        else if (!strcmp(argv[i], "--log"))
            launch.logPath = argv[i + 1];
    }
    if (!launch.gameRoot || !launch.projectData) {
        fprintf(stderr, "usage: %s --game-root <dir> --data <path> [--log <file>]\n", argv[0]);
        return 2;
    }
    std::signal(SIGTERM, [](int) { terminateRequested = 1; });
    std::signal(SIGUSR1, [](int) { pauseRequested = 1; });
    std::signal(SIGUSR2, [](int) { pauseRequested = 0; });

    if (vnkk_start(&launch) != 0) {
        vnkk_stop();
        return 1;
    }
    Clock::time_point quitDeadline = Clock::time_point::max();
    for (;;) {
        Clock::time_point frameStart = Clock::now();
        if (terminateRequested && quitDeadline == Clock::time_point::max()) {
            vnkk_request_quit();
            quitDeadline = frameStart + quitTimeout;
        }
        if (pauseRequested >= 0) {
            vnkk_set_paused(pauseRequested == 1);
            pauseRequested = -1;
        }
        if (vnkk_iterate() || frameStart > quitDeadline)
            break;
        std::this_thread::sleep_until(frameStart + frameTime);
    }
    vnkk_stop();
    return 0;
}

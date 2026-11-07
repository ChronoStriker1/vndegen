// Checks how the runtime turns display refreshes into the game's frames (core/framepace.h). The steps come as the
// app's MillisecondClock makes them: whole milliseconds since the last refresh, carrying the fraction.
#include "framepace.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

// The frames each of `ticks` refreshes at `hertz` runs, with the first refresh at `phaseMs` and each one off its true
// time by up to `jitterMs`. The display link's timestamps start far from zero, as here.
std::vector<int> run(double hertz, double fps, int ticks, double phaseMs, double jitterMs = 0, int speed = 1) {
    wolf::FramePace pace;
    const double start = 86400123.0 + phaseMs;
    double last = start, carry = 0;
    std::vector<int> frames;
    for (int n = 1; n <= ticks; n++) {
        double now = start + n * 1000.0 / hertz + (n % 2 ? jitterMs : -jitterMs);
        double elapsed = std::max(now - last, 0.0) + carry;
        double ms = std::floor(elapsed);
        carry = elapsed - ms;
        last = now;
        frames.push_back(pace.advance(int(ms), 1000.0 / fps, speed, 4 * speed));
    }
    return frames;
}

int total(const std::vector<int> &frames) {
    int sum = 0;
    for (int f : frames) sum += f;
    return sum;
}

// Every refresh at the game's rate runs one frame, wherever the frame boundary falls between refreshes.
void oneFramePerRefreshAt60Hz() {
    for (double phase = 0; phase < 17; phase += 0.05) {
        for (double jitter : {0.0, 0.05}) {
            auto frames = run(60, 60, 600, phase, jitter);
            for (int f : frames) assert(f == 1);
        }
    }
}

// Twice the game's rate runs a frame every second refresh, and a 30 fps game at 60 Hz the same.
void everySecondRefreshAt120Hz() {
    for (double phase = 0; phase < 17; phase += 0.05) {
        for (auto [hertz, fps] : {std::pair{120.0, 60.0}, std::pair{60.0, 30.0}}) {
            auto frames = run(hertz, fps, 1200, phase, 0.05);
            for (size_t i = 0; i + 1 < frames.size(); i += 2) assert(frames[i] + frames[i + 1] == 1);
        }
    }
}

// A 30 fps game at 120 Hz runs a frame every fourth refresh.
void everyFourthRefreshFor30FpsAt120Hz() {
    for (double phase = 0; phase < 34; phase += 0.1) {
        auto frames = run(120, 30, 1200, phase, 0.05);
        for (size_t i = 0; i + 3 < frames.size(); i += 4) assert(frames[i] + frames[i + 1] + frames[i + 2] + frames[i + 3] == 1);
    }
}

// A display that isn't a multiple of the game's rate still runs the game at its own rate, including those near a
// multiple, whose steps match a frame or half a frame most of the time, and with uneven timestamps.
void otherRatesKeepTheGamesTime() {
    for (double fps : {60.0, 30.0}) {
        for (double hertz : {240.0, 180.0, 144.0, 125.0, 119.88, 115.0, 110.0, 100.0, 90.0, 75.0, 65.0, 59.94, 55.0, 50.0}) {
            for (double jitter : {0.0, 0.4}) {
                for (int speed : {1, 3}) {
                    int frames = total(run(hertz, fps, int(hertz * 60), 3.3, jitter, speed));
                    double expected = int(hertz * 60) / hertz * fps * speed;
                    assert(std::abs(frames - expected) <= 1 + speed);
                }
            }
        }
    }
}

// Fast-forward runs `speed` frames a refresh at the game's rate.
void fastForward() {
    auto frames = run(60, 60, 600, 7, 0.05, 3);
    for (int f : frames) assert(f == 3);
}

// A stall runs at most four frames and drops the rest, rather than racing to catch up.
void stallsDropTheExcess() {
    wolf::FramePace pace;
    assert(pace.advance(17, 1000.0 / 60, 1, 4) == 1);
    assert(pace.advance(500, 1000.0 / 60, 1, 4) == 4);
    assert(pace.elapsedMs == 0);
    assert(pace.advance(16, 1000.0 / 60, 1, 4) == 1);
}

}  // namespace

int main() {
    oneFramePerRefreshAt60Hz();
    everySecondRefreshAt120Hz();
    everyFourthRefreshFor30FpsAt120Hz();
    otherRatesKeepTheGamesTime();
    fastForward();
    stallsDropTheExcess();
    std::puts("framepace: all passed");
    return 0;
}

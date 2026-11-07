#pragma once

#include <cmath>

namespace wolf {

// Turns display refresh steps into the game's own frames. The host passes each refresh as whole milliseconds that add
// up to real time, so a 60 Hz refresh comes as 16 or 17 and a 120 Hz one as 8 or 9. Counted as they come, a frame
// boundary that sits near a refresh falls on one side of it on one step and the other side on the next: one refresh
// runs no frame, the next runs two, and the picture stutters. So a step within a millisecond of the frame time, or of
// a half, third or quarter of it, counts as exactly that. A display at a whole multiple of the game's rate then runs
// one frame every one to four refreshes, as the PC engine does on its own display. Only those of 6 ms or more are
// matched, which keeps them at least 2 ms apart, so a step never lies within a millisecond of two. Other steps, and
// any on displays above about 170 Hz, count as they come.
struct FramePace {
    double elapsedMs = 0;

    // How many frames to run for a step of `ms` at `speed` times the game's rate: at most `most`, and the rest of a
    // longer stall is dropped rather than caught up.
    int advance(int ms, double frameMs, int speed, int most) {
        double step = ms;
        for (int part = 1; part <= 4 && frameMs / part >= 6; part++) {
            if (std::abs(ms - frameMs / part) < 1) {
                step = frameMs / part;
                break;
            }
        }
        elapsedMs += step * speed;
        // The tolerance absorbs rounding in sums of a third of a frame, which can fall a hair short of a whole one.
        const double due = frameMs - 1e-6;
        int frames = 0;
        while (elapsedMs >= due && frames < most) {
            elapsedMs -= frameMs;
            frames++;
        }
        if (elapsedMs > frameMs * most) elapsedMs = 0;
        return frames;
    }
};

}  // namespace wolf

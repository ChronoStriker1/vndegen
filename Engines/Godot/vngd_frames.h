// Frame timing for VNDegen's frame rate checks, shared by the Godot 3 and Godot 4 modules. With VNGD_FRAME_LOG set to a
// path, each second's frame count and slowest frame are appended there, and a summary: how long the game has run, its
// average frame rate, and its 1% low, the average rate of its slowest 1% of frames. The summary is written every ten
// seconds and when the game stops, so the last one holds even when the app is killed, as a simulator relaunch does.
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

class VNGDFrames {
	using Clock = std::chrono::steady_clock;

	FILE *file = nullptr;
	double target = 60;
	int seconds = 0;
	Clock::time_point last;
	Clock::time_point window_start;
	int window_frames = 0;
	double window_worst = 0;
	std::vector<double> intervals;

	void write_summary(const char *p_label) {
		double total = 0;
		for (double interval : intervals) {
			total += interval;
		}
		std::vector<double> slowest = intervals;
		std::sort(slowest.begin(), slowest.end(), [](double a, double b) { return a > b; });
		const size_t count = std::max<size_t>(1, slowest.size() / 100);
		double slow_total = 0;
		for (size_t i = 0; i < count && i < slowest.size(); i++) {
			slow_total += slowest[i];
		}
		fprintf(file, "%s seconds=%.1f frames=%zu average_fps=%.1f low1_fps=%.1f target_fps=%.0f\n", p_label, total,
				intervals.size(), total > 0 ? intervals.size() / total : 0, slow_total > 0 ? count / slow_total : 0, target);
		fflush(file);
	}

public:
	// The rate the game aims for: its own cap, which the display's refresh rate limits.
	void set_target(double p_target_fps) { target = p_target_fps; }

	void start() {
		const char *path = getenv("VNGD_FRAME_LOG");
		file = path ? fopen(path, "a") : nullptr;
		intervals.clear();
		seconds = 0;
		window_frames = 0;
		window_worst = 0;
		last = window_start = Clock::now();
	}

	// After each frame the engine ran. Time spent paused goes uncounted, through `skip`.
	void frame() {
		if (!file) {
			return;
		}
		const Clock::time_point now = Clock::now();
		const double interval = std::chrono::duration<double>(now - last).count();
		last = now;
		intervals.push_back(interval);
		window_frames++;
		window_worst = std::max(window_worst, interval);
		if (now - window_start >= std::chrono::seconds(1)) {
			fprintf(file, "second fps=%d worst=%.1fms\n", window_frames, window_worst * 1000);
			fflush(file);
			if (++seconds % 10 == 0) {
				write_summary("running");
			}
			window_frames = 0;
			window_worst = 0;
			window_start = now;
		}
	}

	void skip() { last = window_start = Clock::now(); }

	void stop() {
		if (!file) {
			return;
		}
		write_summary("summary");
		fclose(file);
		file = nullptr;
	}
};

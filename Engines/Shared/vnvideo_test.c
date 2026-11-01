// Tests vnvideo.
//   vnvideo_test <testdata dir>      built-in checks on the small movies in Engines/Shared/testdata
//   vnvideo_test -play <movie>...    decodes each movie as fast as it can and prints its format, frames, sound and speed
#define _POSIX_C_SOURCE 200809L
#include "vnvideo.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    FILE *file;
} source;

static int64_t read_file(void *user, uint64_t offset, void *buffer, size_t length) {
    FILE *file = ((source *)user)->file;
    if (fseeko(file, (off_t)offset, SEEK_SET) != 0) return -1;
    return (int64_t)fread(buffer, 1, length, file);
}

static int failures;

static void check(int ok, const char *what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        failures++;
    }
}

typedef struct {
    int frames, width, height;
    double seconds, sound;
    uint8_t first[4];
} result;

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

// Plays a movie on a simulated clock that steps one frame at a time, pulling its sound as an output would, until it
// ends or `limit` seconds pass. Returns 0 when it can't open.
static int play(const char *path, int loop, double limit, result *out, vnvideo_info *info) {
    source src = {fopen(path, "rb")};
    if (!src.file) return 0;
    fseeko(src.file, 0, SEEK_END);
    uint64_t size = (uint64_t)ftello(src.file);
    char error[160];
    vnvideo *video = vnvideo_open(read_file, &src, size, loop, error, sizeof error);
    if (!video) {
        printf("%s: %s\n", path, error);
        fclose(src.file);
        return 0;
    }
    vnvideo_get_info(video, info);
    memset(out, 0, sizeof *out);
    float *sound = malloc(48000 * 2 * sizeof(float));
    double step = 1.0 / info->fps, time = 0;
    struct timespec pause = {0, 200000};
    int idle = 0;
    while (time < limit && !vnvideo_ended(video, time)) {
        int changed = 0;
        const uint8_t *frame = vnvideo_frame(video, time, &changed);
        if (frame && changed) {
            if (!out->frames) memcpy(out->first, frame + (size_t)(info->height / 2 * info->width + info->width / 2) * 4, 4);
            out->frames++;
        }
        // The decoder runs on its own thread, so the clock waits for each frame, which measures decoding speed. After
        // 20 ms with no new frame the movie is holding a picture, or has finished or stalled, and the clock moves on.
        if (!changed) {
            nanosleep(&pause, NULL);
            if (++idle < 100) continue;
        }
        idle = 0;
        if (info->audio_rate) {
            size_t want = (size_t)((time + step) * info->audio_rate) - (size_t)(vnvideo_audio_time(video) * info->audio_rate);
            size_t got = vnvideo_read_audio(video, sound, want > 48000 ? 48000 : want);
            for (size_t i = 0; i < got * (size_t)info->audio_channels; i++)
                out->sound = fmax(out->sound, fabs(sound[i]));
        }
        time += step;
        if (vnvideo_error(video)) break;
    }
    out->seconds = time;
    out->width = info->width;
    out->height = info->height;
    if (vnvideo_error(video)) printf("%s: %s\n", path, vnvideo_error(video));
    vnvideo_close(video);
    free(sound);
    fclose(src.file);
    return 1;
}

static int near(int value, int expected) {
    return abs(value - expected) <= 8;
}

static void builtin(const char *dir) {
    char path[1024];
    result r;
    vnvideo_info info;

    snprintf(path, sizeof path, "%s/red-tone.mpg", dir);
    check(play(path, 0, 10, &r, &info), "MPEG-1 opens");
    check(info.width == 96 && info.height == 64 && fabs(info.fps - 30) < 0.01, "MPEG-1 size and rate");
    check(info.audio_rate == 44100 && info.audio_channels == 2, "MP2 sound format");
    check(r.frames >= 28 && r.frames <= 30, "MPEG-1 frame count");
    check(near(r.first[0], 192) && near(r.first[1], 32) && near(r.first[2], 64) && r.first[3] == 255, "MPEG-1 colors");
    check(r.sound > 0.08, "MP2 sound decoded");
    check(r.seconds < 1.2, "MPEG-1 ends after its last frame");

    snprintf(path, sizeof path, "%s/red-tone.ogv", dir);
    check(play(path, 0, 10, &r, &info), "Theora with Opus opens");
    check(info.width == 96 && info.height == 64 && fabs(info.fps - 30) < 0.01, "Theora size and rate");
    check(info.audio_rate == 48000 && info.audio_channels == 2, "Opus sound format");
    check(r.frames >= 29 && r.frames <= 31, "Theora frame count");
    check(near(r.first[0], 192) && near(r.first[1], 32) && near(r.first[2], 64), "Theora colors");
    check(r.sound > 0.08, "Opus sound decoded");

    // A picture region smaller than the coded frame, and no sound.
    snprintf(path, sizeof path, "%s/green.ogv", dir);
    check(play(path, 0, 10, &r, &info), "silent Theora opens");
    check(info.width == 98 && info.height == 66 && info.audio_rate == 0, "picture region and no sound");
    check(r.frames == 12, "silent Theora frame count");
    check(near(r.first[0], 32) && near(r.first[1], 192) && near(r.first[2], 64), "picture region colors");

    // A looping movie keeps going past its end.
    check(play(path, 1, 2, &r, &info), "looping Theora opens");
    check(r.frames >= 47 && r.seconds >= 2, "looping movie runs on");
    snprintf(path, sizeof path, "%s/red-tone.mpg", dir);
    check(play(path, 1, 2.5, &r, &info), "looping MPEG-1 opens");
    check(r.frames >= 60, "looping MPEG-1 runs on");

    // Sound that ends long before the picture: the movie still plays to its last frame and ends.
    snprintf(path, sizeof path, "%s/short-sound.ogv", dir);
    check(play(path, 0, 10, &r, &info), "short sound opens");
    check(r.frames >= 58 && r.seconds >= 1.95 && r.seconds < 2.5, "a movie whose sound ends early plays to its end");

    // One picture and 8 s of sound: playback starts though the frame ring never fills, and lasts as long as the sound.
    snprintf(path, sizeof path, "%s/long-sound.ogv", dir);
    check(play(path, 0, 20, &r, &info), "long sound opens");
    check(r.frames == 1 && r.seconds >= 7.9 && r.seconds < 8.5, "a movie whose sound outlasts its picture plays it all");

    // Repeated frames at the end still count toward the movie's length.
    snprintf(path, sizeof path, "%s/held-end.ogv", dir);
    check(play(path, 0, 10, &r, &info), "held ending opens");
    check(r.seconds >= 0.99 && r.seconds < 1.2, "repeated last frames count toward the movie's length");

    char garbage[] = "/dev/null";
    check(!play(garbage, 0, 1, &r, &info), "an empty file is refused");
    printf(failures ? "%d failures\n" : "all vnvideo checks passed\n", failures);
}

int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "-play")) {
        for (int i = 2; i < argc; i++) {
            result r;
            vnvideo_info info;
            double start = now();
            if (!play(argv[i], 0, 1e9, &r, &info)) {
                failures++;
                continue;
            }
            double spent = now() - start;
            printf("%s: %dx%d %.3f fps, sound %d Hz x%d, %d frames over %.2f s, peak %.3f, decoded at %.0f fps\n",
                   argv[i], info.width, info.height, info.fps, info.audio_rate, info.audio_channels, r.frames,
                   r.seconds, r.sound, r.frames / spent);
        }
        return failures != 0;
    }
    if (argc != 2) {
        fprintf(stderr, "usage: vnvideo_test <testdata dir> | -play <movie>...\n");
        return 2;
    }
    builtin(argv[1]);
    return failures != 0;
}

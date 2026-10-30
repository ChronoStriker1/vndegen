// Tests vnmidi.
//   vnmidi_test <soundfont>                              built-in checks on generated MIDI files
//   vnmidi_test <soundfont> <out dir> <file.mid>...       renders each file to <out dir>/<name>.wav, and the seam of
//                                                        its loop to <name>-loop.wav, and prints levels and timing
#define _POSIX_C_SOURCE 200809L
#include "vnmidi.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { RATE = 44100, BLOCK = 512 };

static int failures;
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            failures++;                                   \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);   \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

static double cpu_seconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

// A growable buffer of rendered frames.
typedef struct {
    float *data;
    size_t frames, capacity;
} audio;

static float *audio_reserve(audio *a, size_t frames) {
    if (a->frames + frames > a->capacity) {
        a->capacity = (a->frames + frames) * 2;
        a->data = realloc(a->data, a->capacity * 2 * sizeof(float));
        if (!a->data) abort();
    }
    return a->data + a->frames * 2;
}

// Renders until the player finishes or `max_frames` have been rendered. Returns the frames the song played.
static size_t render_all(vnmidi_player *p, audio *a, size_t max_frames) {
    size_t played = 0;
    while (a->frames < max_frames) {
        int n = vnmidi_player_render(p, audio_reserve(a, BLOCK), BLOCK);
        a->frames += BLOCK;
        played += (size_t)n;
        if (n < BLOCK) break;
    }
    return played;
}

static float peak(const float *x, size_t frames) {
    float m = 0;
    for (size_t i = 0; i < frames * 2; i++) m = fmaxf(m, fabsf(x[i]));
    return m;
}

static double rms(const float *x, size_t frames) {
    double s = 0;
    for (size_t i = 0; i < frames * 2; i++) s += (double)x[i] * x[i];
    return frames ? sqrt(s / (frames * 2)) : 0;
}

static double db(double v) { return v > 0 ? 20 * log10(v) : -999; }

static int cmp_float(const void *a, const void *b) {
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

// The largest step between neighbouring samples in [from, to), and the 99.9th percentile of steps over the whole
// buffer. A click at a loop seam is a step far above the song's own.
static void steps(const audio *a, size_t from, size_t to, float *seam, float *typical) {
    *seam = 0;
    for (size_t i = from > 0 ? from : 1; i < to && i < a->frames; i++)
        for (int c = 0; c < 2; c++) *seam = fmaxf(*seam, fabsf(a->data[i * 2 + c] - a->data[(i - 1) * 2 + c]));
    size_t n = a->frames > 1 ? a->frames - 1 : 0;
    float *d = malloc((n + 1) * sizeof(float));
    for (size_t i = 0; i < n; i++)
        d[i] = fmaxf(fabsf(a->data[(i + 1) * 2] - a->data[i * 2]),
                     fabsf(a->data[(i + 1) * 2 + 1] - a->data[i * 2 + 1]));
    qsort(d, n, sizeof(float), cmp_float);
    *typical = n ? d[(size_t)(n * 0.999)] : 0;
    free(d);
}

static void put16(FILE *f, unsigned v) { fputc(v & 255, f), fputc(v >> 8 & 255, f); }
static void put32(FILE *f, unsigned v) { put16(f, v & 0xffff), put16(f, v >> 16); }

static void write_wav(const char *path, const float *x, size_t frames) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }
    unsigned bytes = (unsigned)(frames * 4);
    fwrite("RIFF", 1, 4, f), put32(f, 36 + bytes), fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16), put16(f, 1), put16(f, 2), put32(f, RATE), put32(f, RATE * 4), put16(f, 4), put16(f, 16);
    fwrite("data", 1, 4, f), put32(f, bytes);
    for (size_t i = 0; i < frames * 2; i++) {
        float v = fmaxf(-1, fminf(1, x[i]));
        put16(f, (unsigned)(short)lrintf(v * 32767));
    }
    fclose(f);
}

// A generated song: 120 bpm, 480 ticks a beat. Piano plays a note on each of 8 beats over a held string pad, with the
// drums on beats, a CC#111 marker at beat 2, and the end of track at beat 8, which is 4000 ms.
typedef struct {
    unsigned char *d;
    size_t n, cap;
} bytes;

static void b_put(bytes *b, const void *p, size_t n) {
    if (b->n + n > b->cap) b->d = realloc(b->d, b->cap = (b->n + n) * 2 + 64);
    memcpy(b->d + b->n, p, n);
    b->n += n;
}
static void b_byte(bytes *b, int v) {
    unsigned char c = (unsigned char)v;
    b_put(b, &c, 1);
}
static void b_var(bytes *b, unsigned v) {
    unsigned char tmp[5];
    int n = 0;
    tmp[n++] = v & 0x7f;
    while (v >>= 7) tmp[n++] = 0x80 | (v & 0x7f);
    while (n--) b_byte(b, tmp[n]);
}
static void b_ev(bytes *b, unsigned delta, int status, int a1, int a2) {
    b_var(b, delta), b_byte(b, status), b_byte(b, a1);
    if (a2 >= 0) b_byte(b, a2);
}
static void b_track(bytes *out, const bytes *t) {
    b_put(out, "MTrk", 4);
    unsigned char len[4] = {(unsigned char)(t->n >> 24), (unsigned char)(t->n >> 16), (unsigned char)(t->n >> 8),
                            (unsigned char)t->n};
    b_put(out, len, 4);
    b_put(out, t->d, t->n);
}

static void song_events(bytes *t, int with_tempo) {
    if (with_tempo) b_put(t, "\x00\xff\x51\x03\x07\xa1\x20", 7);  // 500000 us a beat
    b_ev(t, 0, 0xc0, 0, -1);                                        // piano
    b_ev(t, 0, 0xc1, 48, -1);                                       // strings
    b_ev(t, 0, 0xb0, 7, 100);
    b_ev(t, 0, 0xb1, 7, 70);
    b_ev(t, 0, 0x91, 48, 80);  // pad held through the song
    for (int beat = 0; beat < 8; beat++) {
        b_ev(t, beat == 0 ? 0 : 240, 0x90, 60 + (beat % 4) * 2, 100);
        b_ev(t, 0, 0x99, beat % 2 ? 38 : 36, 100);
        if (beat == 2) b_ev(t, 0, 0xb0, 111, 0);
        b_ev(t, 240, 0x80, 60 + (beat % 4) * 2, 0);
        b_ev(t, 0, 0x89, beat % 2 ? 38 : 36, 0);
    }
    b_put(t, "\x81\x70\xff\x2f\x00", 5);  // 240 ticks after the last note off
}

static bytes make_song(int format) {
    bytes out = {0}, t1 = {0}, t2 = {0};
    unsigned char header[14] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, (unsigned char)format, 0, format ? 2 : 1, 1, 0xe0};
    b_put(&out, header, 14);
    if (format == 0) {
        song_events(&t1, 1);
        b_track(&out, &t1);
    } else {
        b_put(&t1, "\x00\xff\x51\x03\x07\xa1\x20", 7);
        b_put(&t1, "\x83\x60\xff\x2f\x00", 5);  // tempo track ends at 480 ticks; the song track is longer
        song_events(&t2, 0);
        b_track(&out, &t1);
        b_track(&out, &t2);
    }
    free(t1.d), free(t2.d);
    return out;
}

// Starts the song again from silence, so earlier notes' tails don't carry into it, and lets a volume change ramp.
static void restart(vnmidi_player *p) {
    float buf[BLOCK * 2];
    vnmidi_player_stop(p);
    for (int i = 0; i < 10; i++) vnmidi_player_render(p, buf, BLOCK);
    vnmidi_player_seek(p, 0);
}

// The song again, after a tempo track whose first event is a time signature at beat 1, followed by a change to 60 bpm
// at the same tick: 500 ms, then 7 beats of 1000 ms. TinyMidiLoader lost that track's events before VNDegen's change.
static bytes make_late_tempo_song(void) {
    bytes out = {0}, t1 = {0}, t2 = {0};
    unsigned char header[14] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 2, 1, 0xe0};
    b_put(&out, header, 14);
    b_put(&t1, "\x83\x60\xff\x58\x04\x04\x02\x18\x08", 9);
    b_put(&t1, "\x00\xff\x51\x03\x0f\x42\x40", 7);
    b_put(&t1, "\x00\xff\x2f\x00", 4);
    song_events(&t2, 0);
    b_track(&out, &t1);
    b_track(&out, &t2);
    free(t1.d), free(t2.d);
    return out;
}

typedef struct {
    vnmidi_font *font;
    const bytes *song;
    int ok;
} thread_job;

static void *player_thread(void *arg) {
    thread_job *job = arg;
    float buf[BLOCK * 2];
    job->ok = 1;
    for (int round = 0; round < 20; round++) {
        vnmidi_player *p = vnmidi_player_open(job->font, job->song->d, job->song->n, RATE, NULL, 0);
        if (!p) {
            job->ok = 0;
            break;
        }
        for (int i = 0; i < 20; i++) vnmidi_player_render(p, buf, BLOCK);
        vnmidi_player_close(p);
    }
    return NULL;
}

static void builtin(const char *font_path) {
    char error[200] = "";
    CHECK(!vnmidi_font_load("/nonexistent/x.sf2", error, sizeof error), "loaded a missing font");
    CHECK(error[0] && !strchr(error, '\n'), "missing font error: '%s'", error);
    printf("missing font: %s\n", error);

    double t0 = cpu_seconds();
    vnmidi_font *font = vnmidi_font_load(font_path, error, sizeof error);
    CHECK(font, "font: %s", error);
    if (!font) return;
    printf("font loaded in %.2f s CPU\n", cpu_seconds() - t0);

    for (int format = 0; format <= 1; format++) {
        bytes song = make_song(format);
        vnmidi_player *p = vnmidi_player_open(font, song.d, song.n, RATE, error, sizeof error);
        CHECK(p, "format %d: %s", format, error);
        if (!p) continue;
        CHECK(!vnmidi_player_error(p), "error with a font");
        CHECK(vnmidi_player_length_ms(p) == 4000, "format %d length %d", format, vnmidi_player_length_ms(p));
        CHECK(vnmidi_player_marker_ms(p) == 1000, "format %d marker %d", format, vnmidi_player_marker_ms(p));

        // Once through: plays its 4 s, then the tail fades, then render reports the end.
        audio a = {0};
        size_t played = render_all(p, &a, RATE * 20);
        float pk = peak(a.data, a.frames);
        printf("format %d once: %.3f s played, peak %.3f, rms %.1f dBFS\n", format, (double)played / RATE, pk,
               db(rms(a.data, a.frames)));
        CHECK(played >= RATE * 4 && played < RATE * 9, "played %zu frames", played);
        CHECK(pk > 0.05f && pk < 1.0f, "peak %f", pk);
        CHECK(!vnmidi_player_playing(p), "still playing at the end");
        CHECK(vnmidi_player_render(p, audio_reserve(&a, BLOCK), BLOCK) == 0, "render after the end");
        CHECK(peak(a.data + a.frames * 2, BLOCK) == 0, "sound after the end");

        // Looping from the marker: the position wraps from 4000 to 1000 ms.
        vnmidi_player_set_loop(p, 1, VNMIDI_LOOP_MARKER);
        vnmidi_player_seek(p, 0);
        audio l = {0};
        size_t seam = 0;
        int last = 0, wraps = 0;
        while (l.frames < (size_t)RATE * 11) {
            int n = vnmidi_player_render(p, audio_reserve(&l, 64), 64);
            CHECK(n == 64, "looping song ended");
            l.frames += 64;
            int pos = vnmidi_player_position_ms(p);
            if (pos < last) {
                wraps++;
                if (!seam) seam = l.frames;
                CHECK(pos >= 1000 && pos < 1010, "wrapped to %d", pos);
            }
            last = pos;
        }
        CHECK(wraps == 3, "wrapped %d times in 11 s of a 4 s song looping from 1 s", wraps);
        CHECK(seam > RATE * 4 - 128 && seam < RATE * 4 + 128, "first seam at frame %zu", seam);
        float jump, typical;
        steps(&l, seam - 256, seam + 256, &jump, &typical);
        printf("format %d loop: seam step %.4f, song's 99.9%% step %.4f\n", format, jump, typical);
        CHECK(jump <= typical * 2, "seam step %f against %f", jump, typical);
        // The tail of the pad carries over the seam instead of stopping.
        CHECK(rms(l.data + (seam - 64) * 2, 64) > 0.001 && rms(l.data + seam * 2, 32) > 0.001, "silent seam");

        // An explicit loop start in ms.
        vnmidi_player_set_loop(p, 1, 2500);
        vnmidi_player_seek(p, 3900);
        float buf[BLOCK * 2];
        for (int i = 0; i < 20; i++) vnmidi_player_render(p, buf, BLOCK);
        int pos = vnmidi_player_position_ms(p);
        CHECK(pos > 2500 && pos < 2500 + 300, "loop from 2500 at %d", pos);

        // Stop fades within a few ms and rewinds; a seek plays again.
        vnmidi_player_seek(p, 500);
        for (int i = 0; i < 20; i++) vnmidi_player_render(p, buf, BLOCK);
        vnmidi_player_stop(p);
        CHECK(!vnmidi_player_playing(p) && vnmidi_player_position_ms(p) == 0, "stop state");
        audio s = {0};
        int n = vnmidi_player_render(p, audio_reserve(&s, RATE / 10), RATE / 10);
        s.frames = RATE / 10;
        CHECK(n == 0, "render after stop returned %d", n);
        CHECK(peak(s.data + (RATE / 40) * 2, RATE / 10 - RATE / 40) == 0, "sound 25 ms after stop");
        float fade_jump, fade_typical;
        steps(&s, 0, 64, &fade_jump, &fade_typical);
        CHECK(fade_jump < 0.05f, "stop clicks: step %f", fade_jump);
        vnmidi_player_seek(p, 0);
        CHECK(vnmidi_player_render(p, buf, BLOCK) == BLOCK && vnmidi_player_playing(p), "seek after stop");

        // Volume scales the output; tempo doesn't change song time but halves real time.
        vnmidi_player_set_loop(p, 0, 0);
        audio full = {0}, half = {0};
        restart(p);
        vnmidi_player_render(p, audio_reserve(&full, RATE), RATE);
        full.frames = RATE;
        vnmidi_player_set_volume(p, 0.5f);
        restart(p);
        vnmidi_player_render(p, audio_reserve(&half, RATE), RATE);
        half.frames = RATE;
        double ratio = rms(half.data, RATE) / rms(full.data, RATE);
        CHECK(fabs(ratio - 0.5) < 0.01, "volume 0.5 gives %f", ratio);
        vnmidi_player_set_volume(p, 1);
        vnmidi_player_set_tempo(p, 2);
        vnmidi_player_seek(p, 0);
        audio fast = {0};
        played = render_all(p, &fast, RATE * 20);
        CHECK(vnmidi_player_length_ms(p) == 4000, "length at tempo 2");
        CHECK(played >= RATE * 2 && played < RATE * 7, "tempo 2 played %zu frames", played);
        vnmidi_player_set_tempo(p, 1);

        // Transposed notes end even when the transposition changes while they sound.
        vnmidi_player_set_transpose(p, 5);
        vnmidi_player_seek(p, 0);
        for (int i = 0; i < 40; i++) vnmidi_player_render(p, buf, BLOCK);
        vnmidi_player_set_transpose(p, -7);
        audio rest = {0};
        render_all(p, &rest, RATE * 20);
        CHECK(!vnmidi_player_playing(p), "a transposed note hangs");
        vnmidi_player_set_transpose(p, 0);

        vnmidi_player_close(p);
        free(a.data), free(l.data), free(s.data), free(full.data), free(half.data), free(fast.data), free(rest.data);

        // Without a font: silence, an error line, and the same timing.
        p = vnmidi_player_open(NULL, song.d, song.n, RATE, error, sizeof error);
        CHECK(p && vnmidi_player_error(p), "no-font player");
        if (p) {
            printf("no font: %s\n", vnmidi_player_error(p));
            audio q = {0};
            played = render_all(p, &q, RATE * 20);
            CHECK(peak(q.data, q.frames) == 0, "sound without a font");
            CHECK(played >= RATE * 4 && played < RATE * 4 + BLOCK * 2, "no-font song played %zu frames", played);
            vnmidi_player_close(p);
            free(q.data);
        }

        // Every truncation of the file either fails to open or plays without trouble.
        int opened = 0;
        for (size_t cut = 0; cut < song.n; cut++) {
            unsigned char *copy = malloc(cut ? cut : 1);
            memcpy(copy, song.d, cut);
            p = vnmidi_player_open(font, copy, cut, RATE, error, sizeof error);
            free(copy);
            if (!p) continue;
            opened++;
            vnmidi_player_set_loop(p, 1, VNMIDI_LOOP_MARKER);
            for (int i = 0; i < 50; i++) vnmidi_player_render(p, buf, BLOCK);
            vnmidi_player_close(p);
        }
        printf("format %d: %d of %zu truncations opened\n", format, opened, song.n);

        // Players on several threads share the font.
        pthread_t threads[4];
        thread_job jobs[4];
        for (int i = 0; i < 4; i++) {
            jobs[i] = (thread_job){font, &song, 0};
            pthread_create(&threads[i], NULL, player_thread, &jobs[i]);
        }
        for (int i = 0; i < 4; i++) {
            pthread_join(threads[i], NULL);
            CHECK(jobs[i].ok, "thread %d", i);
        }
        free(song.d);
    }

    bytes late = make_late_tempo_song();
    vnmidi_player *lp = vnmidi_player_open(NULL, late.d, late.n, RATE, error, sizeof error);
    CHECK(lp, "late tempo change: %s", error);
    if (lp) {
        CHECK(vnmidi_player_length_ms(lp) == 7500, "late tempo change: length %d", vnmidi_player_length_ms(lp));
        CHECK(vnmidi_player_marker_ms(lp) == 1500, "late tempo change: marker %d", vnmidi_player_marker_ms(lp));
        vnmidi_player_close(lp);
    }
    free(late.d);

    CHECK(!vnmidi_player_open(font, "MThd", 4, RATE, error, sizeof error) && error[0], "short file opened");
    CHECK(!vnmidi_player_open(font, "RIFF0000WAVEfmt ", 16, RATE, error, sizeof error), "non-MIDI opened");

    // A player outlives the font it came from.
    bytes song = make_song(1);
    vnmidi_player *p = vnmidi_player_open(font, song.d, song.n, RATE, NULL, 0);
    vnmidi_font_free(font);
    audio a = {0};
    render_all(p, &a, RATE * 2);
    CHECK(peak(a.data, a.frames) > 0.05f, "player after its font was freed");
    vnmidi_player_close(p);
    free(a.data), free(song.d);
}

static unsigned char *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *d = malloc(n > 0 ? (size_t)n : 1);
    *size = fread(d, 1, (size_t)n, f);
    fclose(f);
    return d;
}

static void render_files(const char *font_path, const char *out_dir, int count, char **files) {
    char error[200];
    double t0 = cpu_seconds();
    vnmidi_font *font = vnmidi_font_load(font_path, error, sizeof error);
    if (!font) {
        printf("FAIL font: %s\n", error);
        failures++;
        return;
    }
    printf("font loaded in %.2f s CPU\n", cpu_seconds() - t0);
    printf("file\tlength_s\tplayed_s\ttail_s\tmarker_ms\tpeak\trms_dBFS\tcpu_per_audio_s\tseam_step\tsong_step_p999\n");
    double total_audio = 0, total_cpu = 0, worst_cpu = 0;
    for (int i = 0; i < count; i++) {
        size_t size;
        unsigned char *data = read_file(files[i], &size);
        vnmidi_player *p = data ? vnmidi_player_open(font, data, size, RATE, error, sizeof error) : NULL;
        free(data);
        const char *name = strrchr(files[i], '/') ? strrchr(files[i], '/') + 1 : files[i];
        if (!p) {
            printf("FAIL %s: %s\n", name, data ? error : "can't read");
            failures++;
            continue;
        }
        int length = vnmidi_player_length_ms(p), marker = vnmidi_player_marker_ms(p);
        audio a = {0};
        double c0 = cpu_seconds();
        size_t played = render_all(p, &a, (size_t)(length / 1000.0 + 15) * RATE);
        double cpu = cpu_seconds() - c0, secs = (double)a.frames / RATE;
        total_audio += secs, total_cpu += cpu;
        if (cpu / secs > worst_cpu) worst_cpu = cpu / secs;
        float pk = peak(a.data, a.frames);
        double level = db(rms(a.data, played));
        char path[4096];
        snprintf(path, sizeof path, "%s/%s.wav", out_dir, name);
        write_wav(path, a.data, a.frames);

        // The loop: 3 s before the seam to 3 s after, from the marker or the start. The seam is where the song time
        // reaches the end. Tails carry over it and the loop start's notes begin after it, so a click would be a step
        // right at that frame, above the steps the song makes anyway.
        int from = length > 3000 ? length - 3000 : 0;
        size_t seam = (size_t)ceil((length - from) * (RATE / 1000.0)), wrapped = 0;
        vnmidi_player_set_loop(p, 1, VNMIDI_LOOP_MARKER);
        vnmidi_player_seek(p, from);
        audio l = {0};
        int last = from;
        while (l.frames < seam + RATE * 3) {
            vnmidi_player_render(p, audio_reserve(&l, 64), 64);
            l.frames += 64;
            int pos = vnmidi_player_position_ms(p);
            if (pos < last && !wrapped) wrapped = l.frames;
            last = pos;
        }
        float jump = 0, typical = 0;
        steps(&l, seam - 2, seam + 3, &jump, &typical);
        snprintf(path, sizeof path, "%s/%s-loop.wav", out_dir, name);
        write_wav(path, l.data, l.frames);

        double played_s = (double)played / RATE;
        printf("%s\t%.2f\t%.2f\t%.2f\t%d\t%.3f\t%.1f\t%.4f\t%.4f\t%.4f\n", name, length / 1000.0, played_s,
               played_s - length / 1000.0, marker, pk, level, cpu / secs, jump, typical);
        CHECK(wrapped >= seam && wrapped < seam + 64 + 2, "%s: wrapped at frame %zu, not %zu", name, wrapped, seam);
        CHECK(jump <= typical, "%s: seam step %f against the song's %f", name, jump, typical);
        CHECK(played_s >= length / 1000.0 - 0.05 && played_s < length / 1000.0 + 11, "%s: played %.2f s of %.2f", name,
              played_s, length / 1000.0);
        CHECK(pk < 1.0f, "%s: clips at %f", name, pk);
        CHECK(pk > 0.01f, "%s: nearly silent, peak %f", name, pk);
        vnmidi_player_close(p);
        free(a.data), free(l.data);
    }
    printf("rendered %.1f s of audio in %.2f s CPU: %.4f s CPU per second, worst file %.4f\n", total_audio, total_cpu,
           total_cpu / total_audio, worst_cpu);
    vnmidi_font_free(font);
}

int main(int argc, char **argv) {
    if (argc < 2 || argc == 3) {
        fprintf(stderr, "usage: vnmidi_test <soundfont> [<out dir> <file.mid>...]\n");
        return 2;
    }
    if (argc == 2) builtin(argv[1]);
    else render_files(argv[1], argv[2], argc - 3, argv + 3);
    printf(failures ? "%d failures\n" : "all passed\n", failures);
    return failures != 0;
}

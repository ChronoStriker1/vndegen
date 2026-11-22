#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "png.h"
#include "vnfont.h"
#include "vnsystemaoi.h"
#ifdef SA_HOST_MOVIES
#include "vnffmovie.h"
#endif

static float audio[4096];

static int run(VNSystemAoi *s, int ms) {
    for (int t = 0; t < ms; t += 16) {
        if (!vnsystemaoi_frame(s, 16)) return 0;
        vnsystemaoi_audio(s, audio, 705);
    }
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "sarun <gameDir> <outDir> <clicks> [shotEvery] [click=x,y ...]\n");
        return 2;
    }
    const char *dirs[] = {SA_FONTS_DIR};
    vnfont_init(SA_FONTS_DIR "/fonts.tsv", dirs, 1);
    char *log = malloc(strlen(argv[2]) + 16);
    sprintf(log, "%s/sarun.log", argv[2]);
    VNSystemAoiConfig cfg = {.gameDir = argv[1], .saveDir = argv[2], .logPath = log, .audioRate = 44100};
#ifdef SA_HOST_MOVIES
    static VNSystemAoiMovies movies;
    movies.open = (void *(*)(VNSystemAoiMovieRead, void *, uint64_t, int, VNSystemAoiMovieInfo *, char *, size_t))vnffmovie_open;
    movies.frame = (const uint8_t *(*)(void *, double, int *, int *))vnffmovie_frame;
    movies.audio = (size_t(*)(void *, float *, size_t))vnffmovie_audio;
    movies.seek = (void (*)(void *, double))vnffmovie_seek;
    movies.close = (void (*)(void *))vnffmovie_close;
    cfg.movies = &movies;
#endif
    char err[256] = "";
    VNSystemAoi *s = vnsystemaoi_open(&cfg, err, sizeof err);
    if (!s) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    int clicks = atoi(argv[3]), every = argc > 4 ? atoi(argv[4]) : 0;
    const char *fast = getenv("SARUN_FAST");
    int alive = run(s, 500);
    if (getenv("SARUN_SKIP")) {
        int ms = atoi(getenv("SARUN_SKIP"));
        vnsystemaoi_set_skip(s, 1);
        alive = run(s, ms);
        vnsystemaoi_set_skip(s, 0);
    }
    if (getenv("SARUN_REALTIME")) {
        int ms = atoi(getenv("SARUN_REALTIME"));
        char path[1024];
        if (getenv("SARUN_RTSKIP")) vnsystemaoi_set_skip(s, 1);
        for (int t = 0; alive && t < ms; t += 16) {
            alive = vnsystemaoi_frame(s, 16);
            vnsystemaoi_audio(s, audio, 705);
            float peak = 0;
            for (int i = 0; i < 1410; i++) peak = audio[i] > peak ? audio[i] : -audio[i] > peak ? -audio[i] : peak;
            if (t % 10000 < 16) printf("t=%d peak=%.3f\n", t, peak);
            usleep(16000);
            if (t % 10000 < 16) {
                int w, h;
                const uint32_t *px = vnsystemaoi_pixels(s, &w, &h);
                snprintf(path, sizeof path, "%s/rt%06d.png", argv[2], t);
                write_png(path, px, w, h, 2, 0);
            }
        }
    }
    for (int a = 5; a < argc; a++) {
        if (!strcmp(argv[a], "clicks")) {
            for (int i = 0; alive && i < clicks; i++) {
                vnsystemaoi_key(s, 0x0d, 1);
                alive = run(s, 64);
                vnsystemaoi_key(s, 0x0d, 1);
                alive = alive && run(s, 400);
            }
            clicks = 0;
            continue;
        }
        int x, y;
        int n;
        if (sscanf(argv[a], "wheel=%d", &n) == 1) {
            vnsystemaoi_wheel(s, n);
            alive = alive && run(s, 300);
        }
        if (sscanf(argv[a], "click=%d,%d", &x, &y) == 2) {
            vnsystemaoi_mouse_move(s, x, y);
            alive = alive && run(s, 100);
            vnsystemaoi_mouse_button(s, VNSYSTEMAOI_MOUSE_LEFT, 1);
            vnsystemaoi_mouse_button(s, VNSYSTEMAOI_MOUSE_LEFT, 0);
            alive = alive && run(s, 1000);
        }
    }
    for (int i = 0; alive && i < clicks; i++) {
        vnsystemaoi_key(s, 0x0d, 1);
        vnsystemaoi_key(s, 0x0d, 0);
        alive = run(s, fast ? 16 : 64);
        vnsystemaoi_key(s, 0x0d, 1);
        alive = alive && run(s, fast ? 48 : 400);
        if (every && i % every == 0) {
            int w, h;
            const uint32_t *px = vnsystemaoi_pixels(s, &w, &h);
            char path[1024];
            snprintf(path, sizeof path, "%s/shot%05d.png", argv[2], i);
            write_png(path, px, w, h, 1, 0);
        }
    }
    alive = alive && run(s, 3000);
    int w, h;
    const uint32_t *px = vnsystemaoi_pixels(s, &w, &h);
    char path[1024];
    snprintf(path, sizeof path, "%s/last.png", argv[2]);
    write_png(path, px, w, h, 1, 0);
    printf("alive=%d serial=%u\n", alive, vnsystemaoi_frame_serial(s));
    vnsystemaoi_close(s);
    free(log);
    return 0;
}

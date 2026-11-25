#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "png.h"
#include "vnfont.h"
#include "vnsystemaoi.h"
#include "sa.h"
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
        if (sscanf(argv[a], "skip=%d", &n) == 1) {
            vnsystemaoi_set_skip(s, 1);
            alive = alive && run(s, n);
            vnsystemaoi_set_skip(s, 0);
        }
        if (sscanf(argv[a], "wait=%d", &n) == 1) alive = alive && run(s, n);
        if (sscanf(argv[a], "shot=%d", &n) == 1) {
            int w, h;
            const uint32_t *px = vnsystemaoi_pixels(s, &w, &h);
            char path[1024];
            snprintf(path, sizeof path, "%s/act%02d.png", argv[2], n);
            write_png(path, px, w, h, 1, 0);
        }
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
        if (!strncmp(argv[a], "dungeon=", 8)) {
            char map[64];
            int dx, dy, dd;
            if (sscanf(argv[a] + 8, "%63[^,],%d,%d,%d", map, &dx, &dy, &dd) == 4) sa_debug_dungeon(s, map, dx, dy, dd);
            alive = alive && run(s, 100);
        }
        if (sscanf(argv[a], "expedition=%d,%d", &x, &y) == 2) {
            sa_debug_expedition(s, x, y);
            alive = alive && run(s, 100);
        }
        if (sscanf(argv[a], "key=%d", &n) == 1) {
            vnsystemaoi_key(s, n, 1);
            vnsystemaoi_key(s, n, 0);
            alive = alive && run(s, 300);
        }
        int dd;
        if (sscanf(argv[a], "place=%d,%d,%d", &x, &y, &dd) == 3) {
            sa_debug_place(s, x, y, dd);
            alive = alive && run(s, 100);
        }
        if (!strncmp(argv[a], "join=", 5)) sa_debug_join(s, argv[a] + 5);
        if (sscanf(argv[a], "unbench=%d,%d", &x, &y) == 2) sa_debug_unbench(s, x, y);
        if (sscanf(argv[a], "save=%d", &n) == 1) sa_debug_save(s, n, false);
        if (!strncmp(argv[a], "pcsave=", 7)) sa_debug_pcsave(s, argv[a] + 7);
        if (!strncmp(argv[a], "pcload=", 7)) {
            sa_debug_pcload(s, argv[a] + 7);
            alive = alive && run(s, 100);
        }
        if (sscanf(argv[a], "load=%d", &n) == 1) {
            sa_debug_save(s, n, true);
            alive = alive && run(s, 100);
        }
        if (sscanf(argv[a], "setf=%d,%d", &x, &y) == 2) sa_debug_set(s, true, x, y);
        if (sscanf(argv[a], "setc=%d,%d", &x, &y) == 2) sa_debug_set(s, false, x, y);
        if (sscanf(argv[a], "getc=%d", &n) == 1) printf("C%03d=%d\n", n, sa_debug_get(s, false, n));
        if (sscanf(argv[a], "getf=%d", &n) == 1) printf("F%04d=%d\n", n, sa_debug_get(s, true, n));
        if (sscanf(argv[a], "mazesim=%d", &n) == 1) sa_debug_maze(s, n);
        if (sscanf(argv[a], "bench=%d", &n) == 1) printf("dungeon frame %.2f ms\n", sa_debug_bench(s, n));
        if (!strcmp(argv[a], "finds")) {
            int f[4];
            sa_debug_finds(s, f);
            printf("finds residents %d food %d money %d resources %d\n", f[0], f[1], f[2], f[3]);
        }
        if (!strcmp(argv[a], "newgame")) {
            sa_debug_new_game(s);
            alive = alive && run(s, 100);
        }
        if (!strncmp(argv[a], "battle=", 7)) {
            sa_debug_battle(s, argv[a] + 7, "bg021a0.agf");
            alive = alive && run(s, 100);
        }
        if (sscanf(argv[a], "move=%d,%d", &x, &y) == 2) {
            vnsystemaoi_mouse_move(s, x, y);
            alive = alive && run(s, 100);
        }
        if (sscanf(argv[a], "rpress=%d,%d", &x, &y) == 2) {
            vnsystemaoi_mouse_move(s, x, y);
            alive = alive && run(s, 16);
            vnsystemaoi_mouse_button(s, VNSYSTEMAOI_MOUSE_RIGHT, 1);
            vnsystemaoi_mouse_button(s, VNSYSTEMAOI_MOUSE_RIGHT, 0);
        }
        if (sscanf(argv[a], "press=%d,%d", &x, &y) == 2) {
            vnsystemaoi_mouse_move(s, x, y);
            alive = alive && run(s, 16);
            vnsystemaoi_mouse_button(s, VNSYSTEMAOI_MOUSE_LEFT, 1);
            vnsystemaoi_mouse_button(s, VNSYSTEMAOI_MOUSE_LEFT, 0);
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

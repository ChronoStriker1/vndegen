// Runs a YU-RIS game headless for testing: replays scripted input and writes frames as PPM files.
//   yrrun <gameDir> [--ms N] [--save DIR] [--log FILE] [--time UNIX] [--final FILE.ppm] [events...]
// Events, by time in ms: T:key:VK, T:click:X,Y, T:rclick:X,Y, T:move:X,Y, T:wheel:N, T:shot:FILE.ppm
// The game clock starts at --time, 2026-01-01 by default, so a run replays the same way.
#include "vnyuris.h"
#include "vnfont.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Ev { int t; char kind[16]; int a, b; char file[512]; } Ev;

static void shot(VNY *g, const char *file) {
    int w, h;
    const uint32_t *px = vny_pixels(g, &w, &h);
    FILE *f = fopen(file, "wb");
    if (!f || !px) { fprintf(stderr, "cannot write %s\n", file); if (f) fclose(f); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        unsigned char c[3] = { (unsigned char)px[i], (unsigned char)(px[i] >> 8), (unsigned char)(px[i] >> 16) };
        fwrite(c, 1, 3, f);
    }
    fclose(f);
    printf("shot %s (%dx%d)\n", file, w, h);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: yrrun <gameDir> [--ms N] [events]\n"); return 2; }
    VNYConfig cfg = { 0 };
    cfg.gameDir = argv[1];
    cfg.audioRate = 44100;
    cfg.startTime = 1767225600;   // 2026-01-01
    int ms = 5000;
    const char *final = NULL;
    Ev ev[256];
    int nev = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--ms") && i + 1 < argc) ms = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) cfg.saveDir = argv[++i];
        else if (!strcmp(argv[i], "--log") && i + 1 < argc) cfg.logPath = argv[++i];
        else if (!strcmp(argv[i], "--time") && i + 1 < argc) cfg.startTime = atoll(argv[++i]);
        else if (!strcmp(argv[i], "--final") && i + 1 < argc) final = argv[++i];
        else if (nev < 256) {
            Ev *e = &ev[nev];
            memset(e, 0, sizeof *e);
            char rest[600];
            if (sscanf(argv[i], "%d:%15[^:]:%599s", &e->t, e->kind, rest) != 3) { fprintf(stderr, "bad event %s\n", argv[i]); return 2; }
            if (!strcmp(e->kind, "shot")) snprintf(e->file, sizeof e->file, "%s", rest);
            else sscanf(rest, "%d,%d", &e->a, &e->b);
            nev++;
        }
    }
    const char *fontDirs[] = { YR_FONTS_DIR };
    vnfont_init(YR_FONTS_DIR "/fonts.tsv", fontDirs, 1);
    char err[512] = "";
    VNY *g = vny_open(&cfg, err, sizeof err);
    if (!g) { fprintf(stderr, "open failed: %s\n", err); return 1; }
    int t = 0, frame = 16;
    bool alive = true;
    while (t < ms && alive) {
        for (int i = 0; i < nev; i++) {
            Ev *e = &ev[i];
            if (e->t < t || e->t >= t + frame) continue;
            if (!strcmp(e->kind, "key")) { vny_key(g, e->a, true); }
            else if (!strcmp(e->kind, "keyup")) { vny_key(g, e->a, false); }
            else if (!strcmp(e->kind, "click") || !strcmp(e->kind, "rclick")) {
                vny_mouse_move(g, e->a, e->b);
                vny_mouse_button(g, e->kind[0] == 'r' ? VNY_MOUSE_RIGHT : VNY_MOUSE_LEFT, true);
            } else if (!strcmp(e->kind, "move")) vny_mouse_move(g, e->a, e->b);
            else if (!strcmp(e->kind, "wheel")) vny_wheel(g, e->a);
            else if (!strcmp(e->kind, "shot")) shot(g, e->file);
        }
        alive = vny_frame(g, (uint32_t)frame);
        // Clicks and key presses last two frames, as a quick press does.
        for (int i = 0; i < nev; i++) {
            Ev *e = &ev[i];
            if (e->t + 2 * frame < t || e->t + 2 * frame >= t + frame) continue;
            if (!strcmp(e->kind, "key")) vny_key(g, e->a, false);
            else if (!strcmp(e->kind, "click")) vny_mouse_button(g, VNY_MOUSE_LEFT, false);
            else if (!strcmp(e->kind, "rclick")) vny_mouse_button(g, VNY_MOUSE_RIGHT, false);
        }
        t += frame;
    }
    if (final) shot(g, final);
    const char *p = vny_problem(g);
    printf("ran %d ms, %s\n", t, alive ? "running" : "ended");
    if (p) printf("problem: %s\n", p);
    vny_close(g);
    return p ? 1 : 0;
}

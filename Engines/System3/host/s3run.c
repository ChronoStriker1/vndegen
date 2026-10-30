// Runs a System 3 game headless for testing: replays scripted input and writes frames as PPM files.
//   s3run <gameDir> [--ms N] [--save DIR] [--log FILE] [--final FILE.ppm] [events...]
// Events, by time in ms: T:key:VK, T:click:X,Y, T:rclick:X,Y, T:move:X,Y, T:shot:FILE.ppm
// It prints the game's problem, if one stopped it, and exits 1 then.
#include "vns3.h"
#include "vnfont.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Ev { int t; char kind[16]; int a, b; char file[512]; } Ev;

static void shot(VNS3 *g, const char *file) {
    int w, h;
    const uint32_t *px = vns3_pixels(g, &w, &h);
    FILE *f = fopen(file, "wb");
    if (!f || !px) { fprintf(stderr, "cannot write %s\n", file); if (f) fclose(f); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        unsigned char c[3] = { (unsigned char)(px[i] >> 16), (unsigned char)(px[i] >> 8), (unsigned char)px[i] };
        fwrite(c, 1, 3, f);
    }
    fclose(f);
    printf("shot %s (%dx%d)\n", file, w, h);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: s3run gameDir [--ms N] [--save DIR] [--log FILE] [events]\n"); return 2; }
    const char *dirs[] = { S3_FONTS_DIR };
    vnfont_init(S3_FONTS_DIR "/fonts.tsv", dirs, 1);
    VNS3Config cfg = { .gameDir = argv[1], .audioRate = 44100, .soundFontPath = S3_SOUNDFONT };
    int total = 10000;
    const char *final = NULL;
    Ev ev[256];
    int n = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--ms") && i + 1 < argc) total = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) cfg.saveDir = argv[++i];
        else if (!strcmp(argv[i], "--log") && i + 1 < argc) cfg.logPath = argv[++i];
        else if (!strcmp(argv[i], "--final") && i + 1 < argc) final = argv[++i];
        else if (n < 256) {
            Ev *e = &ev[n];
            memset(e, 0, sizeof *e);
            char rest[600] = "";
            if (sscanf(argv[i], "%d:%15[^:]:%599s", &e->t, e->kind, rest) < 2) { fprintf(stderr, "bad event %s\n", argv[i]); return 2; }
            if (!strcmp(e->kind, "shot")) snprintf(e->file, sizeof e->file, "%s", rest);
            else sscanf(rest, "%i,%i", &e->a, &e->b);
            n++;
        }
    }
    char err[512] = "";
    VNS3 *g = vns3_open(&cfg, err, sizeof err);
    if (!g) { fprintf(stderr, "open failed: %s\n", err); return 1; }
    float audio[1470 * 2];
    int ok = 1;
    for (int t = 0; t < total; t += 16) {
        for (int i = 0; i < n; i++) {
            Ev *e = &ev[i];
            if (e->t < t || e->t >= t + 16) continue;
            if (!strcmp(e->kind, "key")) { vns3_key(g, e->a, true); vns3_frame(g, 0); vns3_key(g, e->a, false); }
            else if (!strcmp(e->kind, "move")) vns3_mouse_move(g, e->a, e->b);
            else if (!strcmp(e->kind, "click") || !strcmp(e->kind, "rclick")) {
                int b = e->kind[0] == 'r' ? VNS3_MOUSE_RIGHT : VNS3_MOUSE_LEFT;
                vns3_mouse_move(g, e->a, e->b);
                vns3_mouse_button(g, b, true);
                vns3_frame(g, 0);
                vns3_mouse_button(g, b, false);
            } else if (!strcmp(e->kind, "shot")) shot(g, e->file);
        }
        if (!vns3_frame(g, 16)) { ok = !vns3_problem(g); break; }
        vns3_audio(g, audio, 705); // keep the mixer moving as a device would
    }
    if (final) shot(g, final);
    if (vns3_problem(g)) printf("problem: %s\n", vns3_problem(g));
    printf("title: %s\n", vns3_title(g));
    vns3_close(g);
    return ok ? 0 : 1;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "png.h"
#include "vnsmilegame.h"

static int step(VNSmileGame *g, int ms) {
    int alive = 1;
    for (int t = 0; t < ms && alive; t += 16) alive = vnsmilegame_frame(g, 16);
    return alive;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: sgrun <game> <out> [wait:ms] [key:vk] [hold:vk,ms] [shot:name] ...\n");
        return 2;
    }
    char log[1024];
    snprintf(log, sizeof log, "%s/sgrun.log", argv[2]);
    VNSmileGameConfig cfg = {.gameDir = argv[1], .saveDir = argv[2], .logPath = log, .audioRate = 44100, .fallbackFont = getenv("SG_FALLBACK_FONT")};
    char err[512];
    VNSmileGame *g = vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) {
        fprintf(stderr, "open failed: %s\n", err);
        return 1;
    }
    int alive = step(g, 16);
    for (int i = 3; i < argc && alive; i++) {
        const char *c = argv[i];
        int k, ms;
        char name[256];
        if (sscanf(c, "wait:%d", &ms) == 1) {
            alive = step(g, ms);
        } else if (sscanf(c, "hold:%i,%d", &k, &ms) == 2) {
            vnsmilegame_key(g, k, true);
            alive = step(g, ms);
            vnsmilegame_key(g, k, false);
            alive = alive && step(g, 32);
        } else if (sscanf(c, "key:%i", &k) == 1) {
            vnsmilegame_key(g, k, true);
            alive = step(g, 48);
            vnsmilegame_key(g, k, false);
            alive = alive && step(g, 32);
        } else if (sscanf(c, "okuntil:%d", &k) == 1) {
            for (int n = 0; n < k && alive && strcmp(vnsmilegame_scene(g), "field") != 0; n++) {
                vnsmilegame_key(g, 0x0d, true);
                alive = step(g, 48);
                vnsmilegame_key(g, 0x0d, false);
                alive = alive && step(g, 400);
            }
            printf("okuntil scene %s\n", vnsmilegame_scene(g));
        } else if (sscanf(c, "save:%d", &k) == 1) {
            printf("save %d %s\n", k, vnsmilegame_save(g, k) ? "ok" : "failed");
        } else if (sscanf(c, "load:%d", &k) == 1) {
            printf("load %d %s\n", k, vnsmilegame_load(g, k) ? "ok" : "failed");
            alive = step(g, 16);
        } else if (sscanf(c, "shot:%255s", name) == 1) {
            int w, h;
            const uint32_t *px = vnsmilegame_pixels(g, &w, &h);
            char path[1400];
            snprintf(path, sizeof path, "%s/%s.png", argv[2], name);
            png_write(path, px, w, h);
            printf("shot %s scene %s\n", path, vnsmilegame_scene(g));
        } else {
            fprintf(stderr, "unknown command %s\n", c);
        }
    }
    printf("scene %s\n", vnsmilegame_scene(g));
    vnsmilegame_close(g);
    return 0;
}

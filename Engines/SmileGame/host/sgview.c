#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "png.h"
#include "sg.h"

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

int main(int argc, char **argv) {
    if (argc < 7) {
        fprintf(stderr, "usage: sgview <game> <out.png> <map> <x> <z> <dir> [frames] [pose:pitch,yaw,dist,fov] [cal:dist,fov,lift] [hold:key,ms]\n");
        return 2;
    }
    char log[1200];
    snprintf(log, sizeof log, "%s.log", argv[2]);
    VNSmileGameConfig cfg = {.gameDir = argv[1], .saveDir = NULL, .logPath = log, .audioRate = 44100};
    char err[512];
    VNSmileGame *vg = vnsmilegame_open(&cfg, err, sizeof err);
    if (!vg) {
        fprintf(stderr, "open failed: %s\n", err);
        return 1;
    }
    for (int i = 8; i < argc; i++) {
        float a, b;
        if (sscanf(argv[i], "light:%f,%f", &a, &b) == 2) sg_debug_pose(vg, -2000, a, b, 0);
    }
    if (!sg_debug_warp(vg, argv[3], atoi(argv[4]), atoi(argv[5]), atoi(argv[6]))) {
        fprintf(stderr, "map %s not found\n", argv[3]);
        return 1;
    }
    int frames = argc > 7 ? atoi(argv[7]) : 1;
    for (int i = 8; i < argc; i++) {
        float a, b, c, d, e, f, h;
        int k, ms;
        char name[128];
        if (sscanf(argv[i], "cam3:%f,%f,%f,%f,%f,%f,%f", &a, &b, &c, &d, &e, &f, &h) == 7) {
            ViewPose p;
            sg_camera_get((SG *)vg, &p);
            p.target = (Vec3){a, b, c};
            p.pitch = d, p.yaw = e, p.distance = f, p.fov = h, p.follow = false;
            sg_camera_to((SG *)vg, &p, 0);
        } else if (sscanf(argv[i], "hero:%127s", name) == 1) sg_debug_player(vg, name);
        else if (sscanf(argv[i], "pose:%f,%f,%f,%f", &a, &b, &c, &d) == 4) sg_debug_pose(vg, a, b, c, d);
        else if (sscanf(argv[i], "cal:%f,%f,%f", &a, &b, &c) == 3) sg_debug_pose(vg, -1000, a, b, c);
        else if (sscanf(argv[i], "light:%f,%f", &a, &b) == 2) sg_debug_pose(vg, -2000, a, b, 0);
        else if (sscanf(argv[i], "hold:%i,%d", &k, &ms) == 2) {
            vnsmilegame_key(vg, k, true);
            for (int t = 0; t < ms; t += 16) vnsmilegame_frame(vg, 16);
            vnsmilegame_key(vg, k, false);
        }
    }
    double total = 0, worst = 0;
    double *times = calloc((size_t)frames + 1, sizeof(double));
    for (int i = 0; i < frames; i++) {
        double t0 = now_ms();
        vnsmilegame_frame(vg, 16);
        double dt = now_ms() - t0;
        times[i] = dt;
        total += dt;
        if (dt > worst) worst = dt;
    }
    const FrameStats *st = sg_debug_stats(vg);
    printf("frames %d avg %.2f ms worst %.2f ms draw %.2f ms tris %d chunks %d/%d\n", frames, total / frames, worst, st->drawMs, st->tris, st->chunksDrawn,
           st->chunksTotal);
    free(times);
    int w, h;
    const uint32_t *px = vnsmilegame_pixels(vg, &w, &h);
    png_write(argv[2], px, w, h);
    vnsmilegame_close(vg);
    return 0;
}

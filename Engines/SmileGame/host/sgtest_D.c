#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sg.h"

static int failures, checks;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        checks++;                                                                                                      \
        if (!(cond)) {                                                                                                 \
            failures++;                                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                 \
        }                                                                                                              \
    } while (0)

static bool near(float a, float b, float tol) { return fabsf(a - b) <= tol; }

static int count_color(const u32 *fb, int n, u32 color) {
    int c = 0;
    for (int i = 0; i < n; i++) c += fb[i] == color;
    return c;
}

static void test_raster(void) {
    R3 r;
    r3_init(&r, 16, 16);
    u32 fb[256];
    Cam cam = {.target = {8, 0, 8}, .pitch = 90, .yaw = 0, .distance = 10, .fov = 90};
    cam_update(&cam, 16, 16);
    float half = 10.0f;
    Vert a = {8 - half, 0, 8 - half, 0, 0, 1}, b = {8 + half, 0, 8 - half, 0, 0, 1}, c = {8 + half, 0, 8 + half, 0, 0, 1}, d = {8 - half, 0, 8 + half, 0, 0, 1};
    r3_begin(&r, fb, 0xff000000u);
    r3_tri(&r, &cam, &(Tri){.v = {a, b, c}});
    r3_tri(&r, &cam, &(Tri){.v = {a, c, d}});
    r3_flush(&r);
    CHECK(count_color(fb, 256, 0xffffffffu) == 256);
    r3_begin(&r, fb, 0xff000000u);
    r3_tri(&r, &cam, &(Tri){.v = {a, b, c}});
    r3_flush(&r);
    int first = count_color(fb, 256, 0xffffffffu);
    r3_begin(&r, fb, 0xff000000u);
    r3_tri(&r, &cam, &(Tri){.v = {a, c, d}});
    r3_flush(&r);
    int second = count_color(fb, 256, 0xffffffffu);
    CHECK(first + second == 256);
    CHECK(first == 120 || first == 136);
    r3_begin(&r, fb, 0xff000000u);
    Vert e = {8 - 5, 0, 8 - 5, 0, 0, 1}, f = {8 + 5, 0, 8 - 5, 0, 0, 1}, gg = {8 + 5, 0, 8 + 5, 0, 0, 1}, h = {8 - 5, 0, 8 + 5, 0, 0, 1};
    r3_tri(&r, &cam, &(Tri){.v = {e, f, gg}});
    r3_tri(&r, &cam, &(Tri){.v = {e, gg, h}});
    r3_flush(&r);
    CHECK(count_color(fb, 256, 0xffffffffu) == 64);
    r3_free(&r);
}

static const SgrRec *map_record(SG *g, const char *file) {
    Game *d = &g->data;
    int mi = 0;
    for (int i = 0; i < d->pack.count && mi < d->mapCount; i++) {
        const char *key = d->pack.entries[i].key;
        size_t n = strlen(key);
        if (strncmp(key, "map/", 4) != 0 || n <= 4 || strcmp(key + n - 4, ".sgr") != 0) continue;
        Sgr *s = &d->maps[mi++];
        if (strcasecmp(key + 4, file) != 0) continue;
        for (int k = 0; k < s->count; k++)
            if (s->recs[k].type == SGR_MAP) return &s->recs[k];
    }
    return NULL;
}

static SG *open_game(const char *dir) {
    char err[256];
    VNSmileGameConfig cfg = {.gameDir = dir};
    VNSmileGame *vg = vnsmilegame_open(&cfg, err, sizeof err);
    if (!vg) fprintf(stderr, "open %s: %s\n", dir, err);
    return (SG *)vg;
}

static void test_colette(const char *dir) {
    SG *g = open_game(dir);
    CHECK(g != NULL);
    if (!g) return;
    MapLook l;
    const SgrRec *rec = map_record(g, "map_069.sgr");
    CHECK(rec && maplook_parse(&l, rec));
    if (rec) {
        CHECK(l.slopeCount == 1);
        CHECK(l.slopes[0].index == 1);
        CHECK(l.sky == 2);
        CHECK(l.sun);
        CHECK(near(l.sunPitch, -1.2f, 1e-4f));
        CHECK(near(l.sunYaw, -0.7f, 1e-4f));
        CHECK(l.sunColor[0] == 255 && l.sunColor[1] == 255 && l.sunColor[2] == 255);
        CHECK(!l.hasCamera);
        const Res *out = res_find(&g->res, l.outsideChip);
        CHECK(out && strcmp(out->name, "021_Black") == 0);
        maplook_free(&l);
    }
    rec = map_record(g, "map_073.sgr");
    CHECK(rec && maplook_parse(&l, rec));
    if (rec) {
        CHECK(l.hasCamera && l.cameraKind == 1);
        CHECK(near(l.camera[0], 27.5f, 1e-4f) && near(l.camera[1], 1.7f, 1e-4f));
        CHECK(!l.sun);
        maplook_free(&l);
    }
    CHECK(sg_debug_warp((VNSmileGame *)g, "Map_069", 14, 11, 2));
    ViewPose p;
    sg_camera_get(g, &p);
    CHECK(near(p.pitch, -49.0f, 1e-4f) && near(p.yaw, 0.0f, 1e-4f) && near(p.distance, 3.0f, 1e-4f) && near(p.fov, 70.0f, 1e-4f));
    View *v = sg_view(g);
    CHECK(v->slopeDir[9 * g->field.map.w + 14] == 1);
    CHECK(v->slopeDir[9 * g->field.map.w + 13] == 1);
    vnsmilegame_frame((VNSmileGame *)g, 16);
    const Cam *c = &g->field.cam;
    float sx[3];
    const float wx[3] = {13.0f, 16.0f, 14.5f};
    for (int i = 0; i < 3; i++) {
        const float *m = c->m;
        float px = wx[i], py = 2.0f, pz = 11.5f;
        float x = m[0] * px + m[1] * py + m[2] * pz + m[3], y = m[4] * px + m[5] * py + m[6] * pz + m[7], z = m[8] * px + m[9] * py + m[10] * pz + m[11];
        sx[i] = c->cx + x * c->focal / z;
        (void)y;
    }
    CHECK(near(sx[0], 370.0f, 4.0f));
    CHECK(near(sx[1], 589.0f, 4.0f));
    CHECK(near(sx[2], 480.0f, 1.0f));
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_sacred(const char *dir) {
    SG *g = open_game(dir);
    CHECK(g != NULL);
    if (!g) return;
    MapLook l;
    const SgrRec *rec = map_record(g, "map_007.sgr");
    CHECK(rec && maplook_parse(&l, rec));
    if (rec) {
        static const float want[8] = {-3, 5, 0, 70, 3.5f, -1, 0, 0};
        CHECK(l.hasCamera && l.cameraKind == 0);
        for (int i = 0; i < 8; i++) CHECK(near(l.camera[i], want[i], 1e-4f));
        CHECK(l.sky == 0);
        CHECK(l.slopeCount == 3);
        maplook_free(&l);
    }
    rec = map_record(g, "map_002.sgr");
    CHECK(rec && maplook_parse(&l, rec));
    if (rec) {
        CHECK(l.slopeCount == 7);
        CHECK(l.sky == 1);
        CHECK(!l.hasCamera);
        const Res *out = res_find(&g->res, l.outsideChip);
        CHECK(out && strcmp(out->name, "010_Sea") == 0);
        maplook_free(&l);
    }
    CHECK(sg_debug_warp((VNSmileGame *)g, "Map_002", 110, 85, 2));
    ViewPose p;
    sg_camera_get(g, &p);
    CHECK(near(p.pitch, -48.0f, 1e-4f) && near(p.distance, 3.5f, 1e-4f) && near(p.fov, 70.0f, 1e-4f));
    CHECK(sg_debug_player((VNSmileGame *)g, "c_023a"));
    Model *m = g->field.player.model;
    CHECK(m && m->mesh.count == 218 && m->rig && m->rig->boneCount == 10);
    CHECK(m && model_has_motion(m, "walk") && model_has_motion(m, "wait") && model_has_motion(m, "KO") && model_has_motion(m, "disappointed"));
    CHECK(m && !model_has_motion(m, "fly"));
    if (m) {
        bool moved = false;
        for (int i = 0; i < m->mesh.count && !moved; i++)
            for (int k = 0; k < 3; k++) {
                model_pose(m, "walk", 0.0f);
                float y0 = model_mesh(m)->tris[i].v[k].y;
                model_pose(m, "walk", 0.25f);
                if (fabsf(model_mesh(m)->tris[i].v[k].y - y0) > 1e-3f) moved = true;
            }
        CHECK(moved);
    }
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    test_raster();
    const char *colette = getenv("SG_COLETTE"), *sacred = getenv("SG_SACRED");
    if (colette) test_colette(colette);
    if (sacred) test_sacred(sacred);
    printf("%d checks, %d failures%s\n", checks, failures, colette && sacred ? "" : " (game checks skipped without SG_COLETTE and SG_SACRED)");
    return failures ? 1 : 0;
}

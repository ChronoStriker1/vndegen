#ifndef SMILEGAME_D_H
#define SMILEGAME_D_H

enum {
    TRI_OPAQUE = 0,
    TRI_MODULATE = 1,
    TRI_ADD = 2,
    TRI_BLEND = 3,
};

enum {
    TF_LIT = 1 << 0,
    TF_SHADOW = 1 << 1,
    TF_ANIM = 1 << 2,
    TF_NOZWRITE = 1 << 3,
    TF_NOZTEST = 1 << 4,
    TF_CAST = 1 << 5,
    TF_CULL = 1 << 6,
    TF_XRAY = 1 << 7,
};

typedef struct ViewPose {
    Vec3 target;
    float pitch, yaw, distance, fov, lift;
    bool follow;
} ViewPose;

typedef struct SlopeChip {
    i32 index;
    Guid id;
} SlopeChip;

typedef struct MapLook {
    SlopeChip *slopes;
    int slopeCount;
    Guid outsideChip;
    i32 outsideHeight;
    bool sun;
    float sunPitch, sunYaw;
    i32 sunColor[3];
    i32 sky;
    i32 cameraKind;
    bool hasCamera;
    float camera[8];
} MapLook;

bool maplook_parse(MapLook *l, const SgrRec *rec);
void maplook_free(MapLook *l);

typedef struct Chunk {
    int first, count, afirst, acount;
    float min[3], max[3];
} Chunk;

typedef struct ShadowMap {
    int w, h;
    float *depth, *base;
    int dirty[4];
    float axis[3][3];
    float origin[3];
    float scale;
} ShadowMap;

typedef struct FrameStats {
    double drawMs, submitMs;
    int tris, chunksDrawn, chunksTotal;
} FrameStats;

typedef struct Pool Pool;

typedef struct R3x {
    Pool *pool;
    u8 *mark;
    u8 markSerial;
    ShadowMap shadow;
    bool shadowOn;
    float amb[3], sun[3];
    float sunDir[3];
    float animU;
    int threads;
} R3x;

typedef struct Clip {
    char name[32];
    double start, end;
    bool loop;
    void *scene;
    void *anim;
    void **map;
} Clip;

typedef struct Rig {
    SG *g;
    void *scene;
    char *dir;
    int vertCount;
    float *pos, *nrm, *out, *outN;
    u8 (*bone)[4];
    float (*weight)[4];
    int *vertNode;
    int *corner;
    int boneCount;
    void **boneNodes;
    float (*bind)[16];
    float (*world)[16];
    Mesh posed;
    Clip *clips;
    int clipCount;
    bool clipsScanned;
    char motion[32];
    int clip;
    bool animated;
} Rig;

typedef struct View {
    MapLook look;
    Chunk *chunks;
    int chunkCount, chunkW, chunkH, border;
    ViewPose pose, from, to, base;
    u32 moveMs, moveTotal;
    Model *sky[3];
    Image *blob;
    Mesh alpha;
    Image **slopeImages;
    u8 *chipAttr;
    i16 *slopeDir;
    u8 *slopeIdx;
    u32 waterMs;
    FrameStats stats;
    bool ready;
    bool eye;
} View;

void view_load(SG *g);
void view_free(View *v);
void view_unload(SG *g);
void view_follow(SG *g, float x, float y, float z, u32 ms);
View *sg_view(SG *g);

void sg_camera_get(SG *g, ViewPose *out);
void sg_camera_to(SG *g, const ViewPose *pose, u32 ms);
void sg_camera_reset(SG *g, u32 ms);
bool sg_camera_moving(SG *g);
void sg_camera_command(SG *g, const Cmd *c);

void sg_actor_motion(SG *g, Actor *a, const char *motion);

void r3_light(R3 *r, const float amb[3], const float sun[3], const float dir[3]);
void r3_shadow_begin(R3 *r, const float dir[3], const float min[3], const float max[3], int texelsPerUnit);
void r3_shadow_tri(R3 *r, const Tri *t);
void r3_shadow_free(R3 *r);
void r3_shadow_seal(R3 *r);
void r3_shadow_restore(R3 *r);
void r3_tri_ex(R3 *r, const Cam *c, const Tri *t, u8 mode, u8 flags);
void r3_set_anim(R3 *r, float u);
void r3_threads(R3 *r, int n);
void r3_flush(R3 *r);
bool r3_box_visible(const Cam *c, int w, int h, const float min[3], const float max[3]);

void model_pose(Model *m, const char *motion, float seconds);
bool model_has_motion(const Model *m, const char *motion);
const char *model_rest_clip(Model *m);
const Mesh *model_mesh(const Model *m);
const float *model_normals(const Model *m);

bool sg_debug_warp(VNSmileGame *vg, const char *mapName, int x, int z, int dir);
void sg_debug_pose(VNSmileGame *vg, float pitch, float yaw, float distance, float fov);
const FrameStats *sg_debug_stats(VNSmileGame *vg);
bool sg_debug_player(VNSmileGame *vg, const char *resName);

#endif

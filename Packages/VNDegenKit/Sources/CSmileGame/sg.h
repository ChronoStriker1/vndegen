#ifndef SMILEGAME_INTERNAL_H
#define SMILEGAME_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "sg_F.h"

#include "vnsmilegame.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef struct SG SG;

typedef struct Guid {
    u8 b[16];
} Guid;

void *sg_alloc(size_t n);
void *sg_realloc(void *p, size_t n);
char *sg_strdup(const char *s);
char *sg_fmt(const char *fmt, ...);
u8 *sg_read_file(const char *path, size_t *size);
void sg_log(SG *g, const char *fmt, ...);
bool guid_zero(Guid g);
bool guid_eq(Guid a, Guid b);
void guid_str(Guid g, char out[37]);
bool guid_parse(const char *s, Guid *out);
int utf8_decode(const char *s, u32 *cp);

typedef struct Rd {
    const u8 *p, *end;
    bool bad;
} Rd;

Rd rd_make(const u8 *p, size_t n);
bool rd_need(Rd *r, size_t n);
u8 rd_u8(Rd *r);
i16 rd_i16(Rd *r);
i32 rd_i32(Rd *r);
i64 rd_i64(Rd *r);
float rd_f32(Rd *r);
Guid rd_guid(Rd *r);
char *rd_str(Rd *r);
void rd_skip(Rd *r, size_t n);

typedef struct PackEntry {
    char *key;
    u32 offset, csize, usize;
    u16 method;
} PackEntry;

typedef struct Pack {
    FILE *f;
    PackEntry *entries;
    int count;
} Pack;

bool pack_open(Pack *p, const char *path, char *err, size_t errSize);
void pack_close(Pack *p);
const PackEntry *pack_find(const Pack *p, const char *name);
u8 *pack_read(Pack *p, const char *name, size_t *size);
void pack_key(const char *name, char *out, size_t size);

typedef struct SgrRec {
    u8 sub, type;
    const u8 *body;
    u32 size;
} SgrRec;

typedef struct Sgr {
    u8 *data;
    size_t size;
    i32 version;
    SgrRec *recs;
    int count;
} Sgr;

bool sgr_parse(Sgr *s, u8 *data, size_t size);
void sgr_free(Sgr *s);

enum {
    SGR_SETTINGS = 1,
    SGR_HERO = 2,
    SGR_MAP = 3,
    SGR_ITEM = 4,
    SGR_SKILL = 5,
    SGR_EVENT = 7,
    SGR_MONSTER = 8,
    SGR_RESOURCE = 17,
};

enum {
    RES_BGM = 1,
    RES_SE = 2,
    RES_CHIPSET = 3,
    RES_CHARACTER = 4,
    RES_ICON = 5,
    RES_MONSTER = 6,
    RES_IMAGE = 7,
    RES_WINDOW = 8,
    RES_EFFECT = 9,
    RES_MAPOBJECT = 17,
    RES_FACE = 19,
    RES_BGS = 20,
    RES_BATTLEBG = 21,
    RES_CHIP = 22,
    RES_MODEL = 23,
};

typedef struct Res {
    Guid id;
    u8 kind;
    char *name;
    char *path;
    i32 frames, frameMs, flagA, divisions;
    i32 border[4];
} Res;

typedef struct ResTable {
    Res *items;
    int count;
} ResTable;

bool res_load(SG *g);
void res_free(ResTable *t);
const Res *res_find(const ResTable *t, Guid id);

typedef struct Image {
    int w, h;
    u32 *px;
} Image;

Image *img_decode(const u8 *data, size_t size);
void img_free(Image *im);
Image *sg_image(SG *g, Guid id);

typedef struct Settings {
    char *name;
    Guid party[4];
    int partyCount;
    i32 startX, startZ;
    Guid startMap;
    char **switchNames;
    int switchCount;
    char **varNames;
    int varCount;
    Guid windowImage;
    Guid titleImage, titleOverlay, titleBgm, titleCursor, logo;
    char *titleItems[3];
    i32 titleLayout;
    u8 titlePlace[4];
    u32 titleColor;
    i32 screenW, screenH;
} Settings;

bool settings_parse(Settings *s, const SgrRec *rec);
void settings_free(Settings *s);
u32 color_parse(const char *hex);

typedef struct MapEventRef {
    i32 x, z;
    Guid id;
} MapEventRef;

typedef struct MapObject {
    i32 x, z, dir;
    Guid id;
} MapObject;

typedef struct MapStair {
    i32 x, z, dir, kind;
} MapStair;

typedef struct MapChip {
    i32 index;
    Guid id;
} MapChip;

typedef struct Map {
    Guid id;
    char *name;
    MapEventRef *events;
    int eventCount;
    i32 encounterRate;
    i32 w, h;
    i16 *height;
    i16 *chip;
    MapObject *objects;
    int objectCount;
    MapStair *stairs;
    int stairCount;
    i32 colors[8];
    Guid env, bgm;
    MapChip *chips;
    int chipCount;
} Map;

bool map_parse(Map *m, const SgrRec *rec);
void map_free(Map *m);
Guid map_chip_guid(const Map *m, int index);

typedef struct CmdArg {
    u8 kind;
    i32 i;
    Guid g;
    char *s;
} CmdArg;

typedef struct Cmd {
    i32 type, nest;
    CmdArg *args;
    int argCount;
} Cmd;

typedef struct Script {
    Guid id;
    u8 flag;
    i32 trigger;
    Cmd *cmds;
    int count;
    u8 tail[2];
} Script;

enum { ARG_INT = 1, ARG_GUID = 2, ARG_STR = 3 };

bool script_parse(Script *s, const SgrRec *rec);
void script_free(Script *s);
i32 cmd_int(const Cmd *c, int i);
Guid cmd_guid(const Cmd *c, int i);
const char *cmd_str(const Cmd *c, int i);

typedef struct EventCond {
    i32 kind, op, index, value;
} EventCond;

typedef struct EventSheet {
    EventCond conds[8];
    int condCount;
    Guid graphic;
    i32 direction;
    Guid script;
    i32 trigger;
} EventSheet;

typedef struct EventDef {
    Guid id;
    char *name;
    EventSheet *sheets;
    int sheetCount;
} EventDef;

bool event_parse(EventDef *e, const SgrRec *rec);
void event_free(EventDef *e);

typedef struct Game {
    Pack pack;
    Sgr settingsFile, resFile, heroFile;
    Sgr *packRes;
    char **packDirs;
    Guid *packGuids;
    int packCount;
    Sgr *maps;
    int mapCount;
    Settings settings;
} Game;

bool game_load(SG *g, char *err, size_t errSize);
void game_free(Game *d);
const SgrRec *game_record(Game *d, u8 type, u8 sub, Guid id, Sgr **file);
const SgrRec *game_hero(Game *d, Guid id);

typedef struct Font Font;
Font *font_load(const u8 *data, size_t size);
void font_free(Font *f);
void font_set_fallback(Font *f, Font *fallback);
int font_draw(Font *f, u32 *fb, int fw, int fh, int x, int y, int px, const char *utf8, u32 color);
int font_width(Font *f, int px, const char *utf8);

void blit(u32 *fb, int fw, int fh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, int alpha);
void fill_rect(u32 *fb, int fw, int fh, int x, int y, int w, int h, u32 color);

typedef struct Vec3 {
    float x, y, z;
} Vec3;

typedef struct Vert {
    float x, y, z, u, v, shade;
} Vert;

typedef struct Tri {
    Vert v[3];
    const Image *tex;
    u8 alphaTest;
    u8 modulate;
    u8 mode;
    u8 flags;
    u8 model;
} Tri;

typedef struct Cam {
    float m[16];
    float focal, cx, cy;
    float pitch, yaw, distance, fov;
    Vec3 target;
} Cam;

typedef struct R3 {
    int w, h;
    u32 *fb;
    float *zbuf;
    int tint[3];
    struct R3x *x;
    struct View *view;
} R3;

void r3_init(R3 *r, int w, int h);
void r3_free(R3 *r);
void r3_begin(R3 *r, u32 *fb, u32 clear);
void cam_update(Cam *c, int w, int h);
void r3_tri(R3 *r, const Cam *c, const Tri *t);

typedef struct Mesh {
    Tri *tris;
    int count, cap;
} Mesh;

void mesh_add(Mesh *m, const Tri *t);
void mesh_quad(Mesh *m, const Image *tex, Vec3 a, Vec3 b, Vec3 c, Vec3 d, float u0, float v0, float u1, float v1,
               float shade, bool alphaTest);
void mesh_free(Mesh *m);

typedef struct Model {
    Mesh mesh;
    Image **textures;
    u8 *modulate;
    int textureCount;
    struct Rig *rig;
} Model;

Model *model_load(SG *g, const char *path);
void model_free(Model *m);

typedef struct Actor {
    float x, z, y;
    int dir;
    int frame;
    u32 animMs;
    const Res *graphic;
    Image *sheet;
    Model *model;
    Guid eventId;
    bool visible;
    bool moving;
    float tx, tz;
} Actor;

typedef struct Field {
    Map map;
    bool loaded;
    Mesh terrain;
    Image **chipImages;
    int chipImageCount;
    Model **objectModels;
    Actor player;
    Actor *npcs;
    int npcCount;
    EventDef *events;
    int eventCount;
    Cam cam;
} Field;

bool field_load(SG *g, Guid mapId, int x, int z, int dir);
int event_sheet(SG *g, const EventDef *e);
void field_free(Field *f);
void field_update(SG *g, u32 ms);
void field_draw(SG *g);

#include "sg_A.h"
#include "sg_B.h"
#include "sg_E.h"
#include "sg_I.h"

typedef struct Interp {
    Script script;
    bool active;
    int pc;
    u32 waitMs;
    int state;
    char **pages;
    int pageCount, page;
    const char *scrollText;
    float scroll;
    u32 shownMs;
    SgaState a;
} Interp;

void interp_start(SG *g, const Script *s);
void interp_update(SG *g, u32 ms);
void interp_draw(SG *g);
void interp_free(Interp *ip);

enum { SCENE_TITLE, SCENE_FIELD };

enum {
    KEY_UP = 1 << 0,
    KEY_DOWN = 1 << 1,
    KEY_LEFT = 1 << 2,
    KEY_RIGHT = 1 << 3,
    KEY_OK = 1 << 4,
    KEY_CANCEL = 1 << 5,
    KEY_DASH = 1 << 6,
};

struct SG {
    VNSmileGameConfig cfg;
    char *gameDir;
    char *saveDir;
    FILE *log;
    char problem[512];
    Game data;
    ResTable res;
    struct {
        Guid id;
        Image *im;
    } *imageCache;
    int imageCacheCount;
    Font *font;
    int w, h;
    u32 *fb;
    u32 serial;
    R3 r3;
    int scene;
    u32 keys, pressed;
    bool pointerDown, pointerUsed;
    u32 pointerMs;
    int titleCursor;
    int titleSaves;
    u32 titleMs;
    Field field;
    Interp interp;
    u32 clockMs;
    i32 vars[1024];
    u8 switches[1024];
    struct {
        Guid event;
        u8 on[4];
    } *locals;
    int localCount;
    float fadeLevel, fadeFrom, fadeTo;
    u32 fadeMs, fadeTotal;
    Guid pendingMap;
    int pendingX, pendingZ, pendingDir;
    bool pendingTransfer;
    SgB b;
    SgE e;
    SgI fi;
};

bool sg_pressed(SG *g, u32 key);
bool sg_local_switch(SG *g, Guid event, int index);
void text_outlined(SG *g, int x, int y, int px, const char *s, u32 color);

#include "sg_D.h"
#include "sg_C.h"
#include "sg_K.h"

#endif

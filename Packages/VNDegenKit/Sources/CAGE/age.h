#ifndef AGE_H
#define AGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vnage.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef struct VNAge AG;

void *ag_alloc(size_t n);
void *ag_realloc(void *p, size_t n);
char *ag_strdup(const char *s);
char *ag_fmt(const char *fmt, ...);
u8 *ag_read_file(const char *path, size_t *size);
bool ag_write_file(const char *path, const void *data, size_t size);
bool ag_file_exists(const char *path);
bool ag_mkdirs(const char *dir);
int ag_path_resolve(const char *path, char *out, size_t size);
void ag_log(AG *e, const char *fmt, ...);
int ag_sjis_decode(const u8 *p, u32 *cp);
int ag_sjis_len(const u8 *p);
char *ag_sjis_to_utf8(const char *s);
char *ag_utf8_to_sjis(const char *s);
bool ag_ieq(const char *a, const char *b);
static inline u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static inline u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static inline void wr16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static inline void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

size_t ag_lzss(const u8 *src, size_t srcLen, u8 *dst, size_t dstLen);

typedef struct CatEntry {
    char name[64];
    i32 arc;
    u32 offset, size;
} CatEntry;

typedef struct Catalog {
    char **arcs;
    FILE **files;
    int arcCount;
    CatEntry *e;
    int count;
} Catalog;

typedef struct Setting {
    char *key, *value;
} Setting;

typedef struct Store {
    char *dir;
    Catalog cat[256];
    u32 bankSaved[6], bankFull[6];
    Setting *settings;
    int settingCount;
    char **patchNames;
    int patchCount;
    char **looseNames;
    int looseCount;
} Store;

bool store_open(Store *s, const char *dir, char *err, size_t errSize);
void store_close(Store *s);
const CatEntry *store_entry(Store *s, u32 id);
u8 *store_read(Store *s, u32 id, size_t *size);
u8 *store_read_name(Store *s, const char *name, size_t *size);
const char *store_setting(Store *s, const char *key);
i32 store_find(Store *s, const char *name);

typedef struct Image {
    int w, h;
    u32 *px;
} Image;

Image *image_new(int w, int h);
void image_free(Image *im);
Image *agf_decode(const u8 *data, size_t size);

typedef struct Script {
    u32 id;
    char name[64];
    u32 hdr[13];
    u32 *w;
    u32 count;
    u32 codeEnd;
    u32 *t1;
    u32 t1Count;
} Script;

Script *script_load(Store *st, u32 id, char *err, size_t errSize);
void script_free(Script *s);
const char *script_string(const Script *s, u32 off);

enum { BANK_GINT, BANK_LINT, BANK_GSTR, BANK_LSTR };

typedef struct Ref {
    u8 bank;
    i16 frame;
    i32 index;
} Ref;

typedef struct Str {
    char *s;
    u32 len;
} Str;

#define AG_FRAMES 40
#define AG_RET 256

typedef struct Frame {
    Script *sc;
    u32 pc;
    i32 *li;
    Str *ls;
    Ref *lir, *lsr;
    u32 nli, nls, nlir, nlsr;
    u32 ret[AG_RET];
    int rsp;
    bool active;
    u32 handlerA, handlerB;
    int launchReturn;
} Frame;

enum { OP_NEXT, OP_JUMP, OP_WAIT, OP_HALT, OP_STAY };

typedef int (*OpFn)(AG *g);

enum {
    W_NONE,
    W_SLEEP,
    W_YIELD,
    W_FRAME,
} ;

typedef struct Switch {
    char **keys;
    u32 *pcs;
    int count;
} Switch;

typedef struct Queue {
    i32 *v;
    int head, count, cap;
} Queue;

void vm_register(int op, OpFn fn);
void vm_register_all(void);
void ops_core_register(void);

i32 arg_i(AG *g, int n);
u32 arg_type(AG *g, int n);
u32 arg_raw(AG *g, int n);
char *arg_s(AG *g, int n);
void arg_set_i(AG *g, int n, i32 v);
void arg_set_s(AG *g, int n, const char *s);
bool arg_addr(AG *g, int n, Ref *r);
i32 *ref_int(AG *g, Ref r);
Str *ref_str(AG *g, Ref r);
void str_set(Str *s, const char *v);
void vm_problem(AG *g, const char *fmt, ...);
bool vm_call_script(AG *g, u32 id);
void vm_jump(AG *g, u32 pc);
void vm_call_local(AG *g, u32 pc);
void vm_callback(AG *g, u32 pc);
int vm_frame_exit(AG *g);
bool vm_init(AG *g);
void vm_close(AG *g);
void vm_run(AG *g, int budget);
void vm_reset_root(AG *g);
u32 vm_now(AG *g);

typedef struct M4 {
    float m[4][4];
} M4;

typedef struct Vec3 {
    float x, y, z;
} Vec3;

#define AG_SURFACES 1100

typedef struct Surface {
    Image *im;
    i32 res;
    bool created;
    u32 flags;
    void *movie;
} Surface;

typedef struct Chan {
    i32 delay, dur;
    bool on;
} Chan;

typedef struct Obj {
    i32 handle;
    bool visible;
    i32 slot;
    i32 sl, st, sr, sb;
    Vec3 pos, anchor;
    int mode;
    u32 color, colorFrom, colorTo;
    bool live;
    u32 start;
    bool started;
    Chan cColor, cScale, cRot, cTrans, cCell;
    Vec3 scale, scaleFrom, scaleTo;
    Vec3 rotAxis, rotAxisTo;
    float rotAngle, rotFrom, rotTo;
    Vec3 trans, transFrom, transTo;
    i32 cellFrames, cellCols, cellFlags;
    u32 cycStart[5];
    bool cycStarted[5];
    i32 cycPeriod[5];
    u32 cycColor;
    Vec3 cycScale, cycAxis, cycTrans;
    i32 loopFrames, loopCols;
    u32 detached;
    i32 cellX, cellY;
    bool cellHidden;
} Obj;

typedef struct Gfx {
    Surface surf[AG_SURFACES];
    Obj **obj;
    int nobj, cap;
    i32 target;
    u32 background;
    i32 rangeFirst, rangeCount;
    Obj range;
    bool dirty;
    u32 frameNow, framePrev;
    u32 animStart, animDur;
    bool animStarted;
    u32 serviceFlags;
    bool forceComplete;
    i32 tileEdge;
} Gfx;

void gfx_init(AG *g);
void gfx_close(AG *g);
void gfx_reset(AG *g);
void gfx_register(void);
Obj *gfx_obj(AG *g, i32 handle, bool create);
void gfx_render(AG *g, i32 target, i32 first, i32 count);
void gfx_tick(AG *g);
bool gfx_busy(AG *g);
void gfx_complete(AG *g);
Image *gfx_surface(AG *g, i32 slot);
void gfx_set_surface(AG *g, i32 slot, Image *im, i32 res, bool created);

typedef struct Hotspot {
    i32 x, y, w, h;
    u32 enter, leave, click;
    int action;
    bool inside;
    int owner;
} Hotspot;

typedef struct Input {
    int mx, my;
    u32 buttons;
    u32 buttonsPrev;
    int wheel;
    u8 keys[256];
    u8 keyAction[256];
    u8 mouseSlot[8];
    i32 joyButton[32];
    int actionCount;
    u32 mask;
    int scan;
    bool scanEmptyDone;
    u32 joy[32];
    u32 mouseTarget;
    i32 mouseInterval;
    u32 mouseLast;
    int mouseOwner;
    Hotspot hot[100];
    int nhot;
    bool hotArmed;
    u32 clicks;
    bool skip, autoMode, readSkip;
} Input;

void input_init(AG *g);
void input_register(void);
u32 input_mask(AG *g);

struct VNAge {
    Input in;
    Gfx gfx;
    FILE *log;
    Store st;
    char *gameDir, *saveDir;
    char *problem;
    char title[256];
    i32 *gi;
    Str *gs;
    u32 ngi, ngs;
    Frame fr[AG_FRAMES];
    int top;
    u32 *ins;
    u32 insOp;
    u32 insLen;
    u32 launch[256];
    int launchCount, launchHead;
    int wait;
    u32 waitUntil;
    u32 now;
    u64 steps;
    bool halted, quit;
    bool initialRun;
    int blockDepth;
    Switch sw;
    Queue q[11];
    Queue stk[11];
    u32 opSeen[0x400];
    u32 opMissing[0x400];
    u32 frameSerial;
    int width, height;
    u32 *screen;
    u32 rng;
};

#endif

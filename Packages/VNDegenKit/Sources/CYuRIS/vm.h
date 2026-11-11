// The runtime's state: the project, variables, tasks, input, and the graphics and sound parts.
#ifndef YR_VM_H
#define YR_VM_H

#include "yr.h"

typedef struct YFS YFS;
typedef struct YGfx YGfx;
typedef struct YSnd YSnd;
typedef struct YText YText;

enum { YR_PARAMS = 17 };   // GOSUB and RETURN values 1 to 16; slot 0 unused

// The values one GOSUB level received and the ones its last callee returned to it.
typedef struct YValues {
    i64 i[YR_PARAMS];
    double f[YR_PARAMS];
    char *s[YR_PARAMS];
    u8 flag[YR_PARAMS];
} YValues;

typedef struct YFrame {
    u16 retScript;
    u32 retIp;
    int loopBase;     // loop depth when the level began
    YValues params, results;
} YFrame;

typedef struct YLoop {
    u16 script;
    u32 start, end;   // the LOOP and its LOOPEND
    i64 count;        // -1 repeats until a break
    i64 lc;           // @_LC, from 1
} YLoop;

typedef struct YTask {
    char *id;
    int z;
    bool alive;
    u16 script;
    u32 ip;
    YFrame *frames;
    int depth, frameCap;
    YLoop *loops;
    int loopDepth, loopCap;
    u64 waitUntil;    // game clock in ms
    int waitFrames;
    u64 order;        // creation order, for tasks of equal priority
    bool enabled;     // TASK[E=0] stops a task without ending it
    YValues tvals;    // TINT, TFLT and TSTR given at creation, read as @_TINT(n) and the like
    bool clearText;   // rested at a RETURNCODE: $_TEXT and $_TEXT_FLAG empty when it runs again
} YTask;

// Results of one instruction for the scheduler.
enum { YR_NEXT, YR_JUMPED, YR_YIELD, YR_STOP };

typedef struct YRef {
    u32 vid;
    YVar *var;
    long index;       // element, or -1 for the whole array
    YSysVar sys;
    i64 sub[8];
    int nsub;
} YRef;

struct YR {
    char *gameDir, *saveDir;
    FILE *log;
    char problem[512];
    char title[256];
    YFS *fs;
    YCmdInfo *cmds;
    u32 cmdCount;
    YScript *scripts;
    u32 scriptCount;
    YLabel *labels;
    u32 labelCount, labelCap;
    int *labelIndex;
    u32 key, scriptVersion;
    int width, height;
    int enc;                    // YR_SJIS or YR_UTF8, for strings in scripts
    YVar *vars;
    u32 varCount;
    YSysVar sys[1000];

    YTask **tasks;
    int taskCount;
    u64 taskOrder;
    YTask *cur;

    u64 clock;                  // game time in ms
    u64 frameNo;
    i64 startUnix;
    bool paused, quit;
    int fps;

    // Input: per button code, frames held (positive) or released (negative), and press counts.
    int held[YR_CODE_COUNT];
    int pressCount[YR_CODE_COUNT];
    bool down[YR_CODE_COUNT];
    bool pressedThisFrame[YR_CODE_COUNT];
    int wheel;
    int mouseX, mouseY;
    int lastKey, lastMouse;

    // System variable storage that isn't per task.
    char *text, *textFlag;      // $_TEXT and $_TEXT_FLAG
    i64 menuCode;
    i64 sysInts[1000];
    char *sysStrs[1000];

    YGfx *gfx;
    YSnd *snd;
    YText *txt;
    int audioRate;
    u32 serial;
    long watchVid;              // logs stores to this variable id, from YR_WATCH; -1 for none
    bool traceGosub;            // logs every GOSUB, from the YR_TRACE_GOSUB environment variable
    uint32_t *screen;           // RGBA, width x height
};

// ---- vm.c ----
bool yr_vm_start(YR *y);
void yr_vm_step(YR *y);          // runs every task once, for one frame
YTask *yr_task_new(YR *y, const char *id, int z, u16 script, u32 ip);
void yr_task_free(YTask *t);
YTask *yr_task_find(YR *y, const char *id);
bool yr_gosub(YR *y, YTask *t, const char *label, YScript *from, u32 retIp);
void yr_fail(YR *y, const char *fmt, ...);

// ---- expr.c ----
YVal yr_eval(YR *y, YTask *t, const u8 *p, u32 n);
bool yr_lvalue(YR *y, YTask *t, const u8 *p, u32 n, YRef *out);
YVal yr_ref_get(YR *y, YTask *t, const YRef *r);
void yr_ref_set(YR *y, YTask *t, const YRef *r, const YVal *v);
u8 yr_ref_type(YR *y, const YRef *r);

// ---- arguments of the running instruction (args.c) ----
typedef struct YCall {
    YR *y;
    YTask *t;
    YScript *s;
    const YIns *in;
} YCall;
const YArg *yc_arg(const YCall *c, const char *key);
bool yc_has(const YCall *c, const char *key);
YVal yc_val(const YCall *c, const char *key);
bool yc_int(const YCall *c, const char *key, i64 *out);
i64 yc_int_or(const YCall *c, const char *key, i64 def);
bool yc_flt(const YCall *c, const char *key, double *out);
char *yc_str(const YCall *c, const char *key);  // malloc'd, or NULL when absent
bool yc_ref(const YCall *c, const char *key, YRef *out);
void yc_unknown(const YCall *c, const char *what);

// ---- sysvar.c ----
YVal yr_sys_get(YR *y, YTask *t, const YSysVar *s, int n, const i64 *sub);
void yr_sys_set(YR *y, YTask *t, const YSysVar *s, int n, const i64 *sub, const YVal *v);

// ---- commands (cmd_*.c). Each returns YR_NEXT, YR_JUMPED, YR_YIELD or YR_STOP. ----
int yr_cmd_vars(YCall *c, int id);      // INT, FLT, STR, S_*, F_*, G_*, LET, VARACT, VARINFO, MATH
int yr_cmd_system(YCall *c, int id);    // WAIT, END, INPUT, MOUSE, FPS, SYSTEM, WINDOW, FILE*, SAVE, LOAD, ...
int yr_cmd_text(YCall *c, int id);      // _, WORD, RETURNCODE, TEXT, TEXTINFO, FONT, FONTINFO
int yr_cmd_cg(YCall *c, int id);        // CG, CGACT, CGEND, CGINFO
int yr_cmd_sound(YCall *c, int id);     // SOUND, SOUNDEND, SOUNDINFO, MOVIE...
int yr_cmd_task(YCall *c, int id);      // TASK, TASKEND, TASKINFO

// ---- save.c ----
// Whether a file the game wrote (in the save folder) exists, by the game's name for it.
bool yr_save_exists(YR *y, const char *name);
char *yr_save_path(YR *y, const char *name);   // malloc'd host path inside the save folder, or NULL

// ---- input.c ----
void yr_input_frame(YR *y);             // advances the held counters once per frame

#endif

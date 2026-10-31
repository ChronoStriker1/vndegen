// Internal declarations of VNDegen's N2System runtime. See include/vnn2.h for the host interface and
// docs/n2system.md for the design.
#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "vnn2.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;

#define N2_W 800
#define N2_H 600

// ---- utilities (util.c) ----

void *n2_alloc(size_t size);  // zeroed; aborts when memory runs out
void *n2_grow(void *p, size_t size);
char *n2_strdup(const char *s);
char *n2_strndup(const char *s, size_t n);
char *n2_fmt(const char *fmt, ...);
bool n2_streq_ci(const char *a, const char *b);  // ASCII case-insensitive
bool n2_glob(const char *pattern, const char *name);  // '*' wildcards, ASCII case-insensitive
u32 n2_hash(const char *s);  // ASCII case-insensitive FNV-1a
char *n2_sjis_to_utf8(const u8 *s, size_t n, size_t *outLen);
char *n2_utf8_to_sjis(const char *s, size_t *outLen);
u32 n2_utf8_next(const char **p);  // decodes one code point and advances; 0 at the end

typedef struct Buf {
    u8 *data;
    size_t len, cap;
} Buf;
void buf_put(Buf *b, const void *p, size_t n);
void buf_u8(Buf *b, u8 v);
void buf_u32(Buf *b, u32 v);
void buf_str(Buf *b, const char *s);  // without the terminator
void buf_free(Buf *b);

// A name to pointer map that ignores ASCII case.
typedef struct MapEntry {
    char *key;
    void *value;
    u32 hash;
} MapEntry;
typedef struct Map {
    MapEntry *entries;
    int cap, count;
} Map;
void *map_get(const Map *m, const char *key);
void map_set(Map *m, const char *key, void *value);  // a NULL value removes the key
void map_free(Map *m, void (*freeValue)(void *));

// ---- archives and files (npa.c, fs.c) ----

typedef struct NpaEntry {
    char *name;  // UTF-8, '/' separated, as stored (case kept)
    u32 offset, size, unpackedSize;
} NpaEntry;

typedef struct Npa {
    char *path;
    FILE *file;
    bool compressed;
    NpaEntry *entries;
    int count;
    Map index;  // lowercased name to entry
} Npa;

Npa *npa_open(const char *path, char *err, size_t errSize);
void npa_close(Npa *a);
const NpaEntry *npa_find(Npa *a, const char *name);
u8 *npa_read(Npa *a, const NpaEntry *e, size_t *size);

typedef struct N2Fs {
    char *gameDir;
    Npa *archives[16];
    char *archiveNames[16];  // "cg", "nss", ...: the first path component they serve
    int archiveCount;
} N2Fs;

bool fs_init(N2Fs *fs, const char *gameDir, char *err, size_t errSize);
void fs_free(N2Fs *fs);
// Reads a game file by its script path, such as "cg/bg/bgba01me0.jpg" or "nss\\boot.nss": loose files first, then
// the archive named by the first component. Case and slash direction are ignored.
u8 *fs_read(N2Fs *fs, const char *path, size_t *size);
bool fs_exists(N2Fs *fs, const char *path);

// ---- values ----

typedef struct Str {
    int rc;
    int len;
    char s[];
} Str;

enum { V_NULL, V_NUM, V_STR, V_BOOL };

typedef struct Value {
    u8 t;
    u8 delta;  // an '@' coordinate, relative to the current value
    double n;  // V_NUM and V_BOOL
    Str *s;    // V_STR
} Value;

Str *str_new(const char *s, int len);
Str *str_ref(Str *s);
void str_unref(Str *s);
Value v_null(void);
Value v_num(double n);
Value v_bool(bool b);
Value v_str(const char *s);  // copies
Value v_strn(const char *s, int len);
Value v_copy(const Value *v);
void v_free(Value *v);
bool v_truthy(const Value *v);
double v_tonum(const Value *v);
const char *v_tostr(const Value *v, char *tmp, size_t tmpSize);  // tmp holds number conversions
bool v_equal(const Value *a, const Value *b);

// ---- scripts (script.c: lexer, parser, compiler) ----

enum {
    OP_NOP,
    OP_CONST,    // push consts[a]
    OP_NULL,
    OP_TRUE,
    OP_FALSE,
    OP_GETVAR,   // push variable named consts[a]
    OP_SETVAR,   // pop into variable consts[a]
    OP_GETPARAM, // push parameter a
    OP_SETPARAM,
    OP_GETIDX,   // pop index, push element of array consts[a]
    OP_SETIDX,   // pop value, pop index, store into array consts[a]
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_NOT, OP_NEG, OP_DELTA,
    OP_POP,
    OP_JMP,      // pc = a
    OP_JZ,       // pop, jump to a when false
    OP_JNZ,
    OP_CALL,     // call function consts[a & 0xffff] with a >> 16 arguments, push the result
    OP_RET,      // return the top of the stack
    OP_CALL_CHAPTER,  // pop a path and run that file's chapter main
    OP_CALL_SCENE,    // pop "file->scene", "@->scene" or "scene" and run it
    OP_SELECT_BEGIN,
    OP_ISPRESSED,     // push whether button consts[a] was pressed this pass
    OP_SELECT_END,    // yield one frame; loop to a unless a case ran
    OP_PRE,           // activate dialogue block a of this file
    OP_TEXT,          // text block: show markup consts[a]
    OP_TEXT_END,      // text block: the end of the block
    OP_DUP,
};

typedef struct Chunk Chunk;
typedef struct Script Script;

enum { CHUNK_FUNCTION, CHUNK_CHAPTER, CHUNK_SCENE, CHUNK_TEXT };

struct Chunk {
    int kind;
    char *name;
    Script *script;
    char **params;
    int paramCount;
    u32 *code;
    i32 *srcpos;  // source offset of each instruction
    int codeLen, codeCap;
    Value *consts;
    int constCount, constCap;
};

typedef struct TextBlock {
    char *name;  // "text0010"
    char *box;   // "@box09"
    Chunk *chunk;
    i32 srcpos;
} TextBlock;

struct Script {
    char *path;     // as loaded, lowercased with '/' separators, such as "nss/boot.nss"
    char *source;   // UTF-8
    size_t sourceLen;
    u8 *raw;        // the Shift-JIS bytes as read, which saves keep
    size_t rawLen;
    Chunk **chunks;
    int chunkCount;
    TextBlock *blocks;
    int blockCount;
    char **includes;
    int includeCount;
};

// Parses and compiles a file. Returns NULL and logs when it can't.
Script *script_compile(const char *path, const u8 *raw, size_t rawLen, char *err, size_t errSize);
void script_free(Script *s);
Chunk *script_find(Script *s, int kind, const char *name);
TextBlock *script_block(Script *s, const char *name);

// ---- graphics (gfx.c) ----

typedef struct Image {
    int w, h;
    u32 *px;  // premultiplied RGBA8, bytes R, G, B, A
    int rc;
} Image;

Image *img_new(int w, int h);
Image *img_ref(Image *i);
void img_unref(Image *i);
Image *img_decode(const u8 *data, size_t size);

// ---- objects (obj.c) ----

enum {
    OBJ_NAME,       // CreateName and implicit parents: draws nothing
    OBJ_TEXTURE,
    OBJ_COLOR,
    OBJ_WINDOW,
    OBJ_TEXT,
    OBJ_CHOICE,
    OBJ_SOUND,
    OBJ_PROCESS,
    OBJ_SCROLLBAR,
    OBJ_IMAGE,      // LoadImage: an image kept by name, not drawn
    OBJ_EFFECT,
    OBJ_BACKLOG,
};

enum { ANIM_FADE, ANIM_MOVE, ANIM_ZOOM, ANIM_ROTATE, ANIM_TRANSITION, ANIM_SHAKE, ANIM_VOLUME, ANIM_COUNT };

typedef struct Anim {
    bool active;
    double from[3], to[3];
    u32 start, duration;  // milliseconds of game time
    int ease;
    char *mask;           // DrawTransition
    Image *maskImage;
    double feather;
} Anim;

typedef struct Thread Thread;
typedef struct TextObj TextObj;
typedef struct Sound Sound;

typedef struct Obj Obj;
struct Obj {
    int kind;
    char *name;
    char *alias;
    Obj *parent;
    Obj **children;
    int childCount, childCap;
    u32 serial;   // creation order, ties in priority
    char *createCall;  // the creating call as text, which frames.npf keeps
    int prio;
    double x, y;
    bool centerX, centerY;  // "Center" and "Middle" coordinates
    int w, h;
    double opacity;   // 0 to 1000
    double zoomX, zoomY;  // 1000 is 100%
    double rotX, rotY, rotZ;
    double transition;  // DrawTransition progress, 0 to 1000; < 0 when none
    double shakeX, shakeY;
    bool locked, erased, hideable, disused, smoothing, unclickable, pushText, noLog;
    int blend;  // 0 normal, 1 add, 2 subtract, 3 multiply
    Image *image;
    u32 color;  // OBJ_COLOR, 0xRRGGBB
    Anim anims[ANIM_COUNT];
    TextObj *text;
    Sound *sound;
    Thread *thread;   // OBJ_PROCESS
    char *procTarget;
    // OBJ_CHOICE
    int mouseState, lastMouseState;
    bool pressed;
    Obj *focusNext[4];
    // OBJ_SCROLLBAR
    int sbX1, sbY1, sbX2, sbY2;
    double sbValue;
    bool sbVertical, sbDragging;
};

// ---- the VM (vm.c) ----

enum {
    WAIT_NONE,
    WAIT_TIME,     // until waitUntil
    WAIT_KEY,      // until a click or key, or waitUntil when > 0
    WAIT_TEXT,     // until the text object waitText finishes
    WAIT_ANIM,     // until the animation waitAnim of every object matching waitQuery ends
    WAIT_FRAME,    // one frame
    WAIT_TEXTSEG,  // text thread: until the typewriter shows its segment
    WAIT_SOUND,
    WAIT_DONE,     // the thread has ended
};

typedef struct Frame {
    Chunk *chunk;
    int pc;
    Value *params;
    int paramCount;
    int stackBase;
    bool selectPressed;
    bool pushes;  // OP_CALL frames leave their result; chapters, scenes and thread entries don't
} Frame;

struct Thread {
    char *name;
    Value *stack;
    int sp, stackCap;
    Frame *frames;
    int fp, frameCap;
    int wait;
    u32 waitUntil;
    char *waitQuery;
    int waitAnim;
    Obj *waitObj;
    bool paused;
    bool isMain;
    TextObj *textOwner;  // a text block's thread
    Obj *owner;          // the process or text object that keeps this thread
    Obj *ctx;            // the object tree it creates and finds objects in
};

// ---- the runtime (n2.c) ----

typedef struct VNN2 N2;

// Variables: '$' and '#' names in one map; arrays as their own map.
typedef struct Var {
    Value v;
    u32 order;  // creation order, which the save files keep
} Var;

typedef struct Arr {
    Value *items;
    int count;
} Arr;

struct VNN2 {
    VNN2Config cfg;
    char *saveDir;
    char *title;
    char *productName, *productVersion;
    FILE *log;
    char problem[512];
    N2Fs fs;
    Map ini;          // "section/key" to value, UTF-8
    Map scripts;      // path to Script
    Map functions;    // name to Chunk, from every loaded file
    Map vars;         // name to Var
    Map arrays;       // name to Arr
    Map images;       // LoadImage name to Image cache
    Map imageCache;   // path to Image
    Obj root;
    u32 objSerial;
    Thread **threads;
    int threadCount, threadCap;
    Thread *main;
    Thread *system;   // a system script started by a key (menu, shortcut, backlog) while main waits
    struct {
        int vk;
        char *script;
    } shortcuts[32];
    int shortcutCount;
    Script *mainScript;   // the file of the running chapter or scene, for "@->"
    TextBlock *activeBlock;
    Script *activeBlockScript;
    u32 now;          // game time in milliseconds
    u32 frameSerial;
    bool dirty;
    bool quit;
    bool paused;
    u32 *screen;
    // input
    int mouseX, mouseY;
    bool mouseDown[3], mousePressed[3], mouseReleased[3];
    bool keyDown[256], keyPressed[256];
    int wheel;
    bool anyPress;     // a click or key that advances text this frame
    bool clickOnButton;  // this frame's click landed on a button
    // text
    struct Font *font;
    char fontFace[64];
    int fontSize;
    u32 fontColor, fontOutline;
    int fontWeight;
    char *outlineMode;
    // sound
    struct Mixer *mixer;
    long long startTime;
    int rngState;
    Map pressedButtons;  // button name to (void *)1 for the current select pass
    int pendingLoad;     // Load(n) asked for this slot; -1 when none
    bool pendingReset;
    struct Backlog *backlog;
    Map conquest;        // "path|block" of every text block shown, for skip
    u32 varOrder;
    Obj sysRoot;         // objects of system scripts, apart from the game's
    Obj *curRoot;        // the object tree of the running thread
};

void n2_log(N2 *n, const char *fmt, ...);
void n2_fail(N2 *n, const char *fmt, ...);
const char *n2_ini(N2 *n, const char *section, const char *key);

// Variables
Value *var_get(N2 *n, const char *name);  // NULL when unset
void var_set(N2 *n, const char *name, const Value *v);  // copies
Arr *arr_get(N2 *n, const char *name, bool create);

// Scripts and threads
Script *n2_load_script(N2 *n, const char *path);
Chunk *n2_find_function(N2 *n, const char *name);
Thread *thread_new(N2 *n, const char *name);
void thread_free(Thread *t);
bool thread_call(N2 *n, Thread *t, Chunk *c, Value *args, int argc);
void thread_run(N2 *n, Thread *t);  // runs until the thread waits or ends
void thread_kill(N2 *n, Thread *t);
void vm_push(Thread *t, Value v);  // takes ownership

// Built-ins (builtins.c). Returns true when the call made the thread wait.
typedef bool (*BuiltinFn)(N2 *n, Thread *t, Value *args, int argc, Value *ret);
BuiltinFn builtin_find(const char *name);

// Objects (obj.c)
Obj *obj_create(N2 *n, const char *path, int kind, int prio);  // replaces an object of that path
void obj_destroy(N2 *n, Obj *o);
typedef void (*ObjVisit)(N2 *n, Obj *o, void *ctx);
int obj_query(N2 *n, const char *query, ObjVisit visit, void *ctx);  // returns the match count
Obj *obj_find(N2 *n, const char *query);  // the first match
void obj_set_alias(N2 *n, Obj *o, const char *alias);
void obj_update_anims(N2 *n, Obj *o);
bool obj_anim_running(Obj *o, int anim);
double ease_apply(int ease, double t);
int ease_from_name(const char *name);
void obj_start_anim(N2 *n, Obj *o, int which, u32 duration, const double *to, int ease);
void obj_screen_pos(Obj *o, double *x, double *y);

// Buttons (obj.c): hover and press states of CreateChoice objects, run each frame.
void buttons_update(N2 *n);
bool obj_state_visible(Obj *o);
void n2_forget_obj(N2 *n, Obj *o);
bool n2_skip_active(N2 *n);

// Rendering (draw.c)
void n2_render(N2 *n);

// Text (text.c)
struct Font *font_open(const char *path, int index);
void font_close(struct Font *f);
TextObj *text_new(N2 *n, Obj *o, const char *markup, int w, int h, bool autoSize);
void text_free(TextObj *t);
void text_update(N2 *n, Obj *o);   // typewriter
bool text_segment_done(TextObj *t);
bool text_finished(TextObj *t);
void text_draw(N2 *n, Obj *o, u32 *dst, int dx, int dy, double opacity);
void text_start_block(N2 *n, Obj *o, TextBlock *b, Script *s, int w, int h, int letterSpacing, int lineSpacing);
void text_append(N2 *n, TextObj *t, const char *markup);
void text_deliver_input(N2 *n, bool advance);  // a click or key for the text waiting on one
int text_width(N2 *n, const char *markup);

// Sound (snd.c)
struct Mixer *mixer_new(int rate);
void mixer_free(struct Mixer *m);
Sound *sound_load(N2 *n, const char *path, int kind);
void sound_free(N2 *n, Sound *s);
void sound_play(N2 *n, Sound *s);
void sound_stop(N2 *n, Sound *s);
void sound_set_volume(N2 *n, Sound *s, double volume, u32 durationMs);
void sound_set_loop(N2 *n, Sound *s, bool loop);
void sound_set_loop_points(N2 *n, Sound *s, u32 startMs, u32 endMs);
bool sound_playing(Sound *s);
u32 sound_duration_ms(Sound *s);
u32 sound_position_ms(Sound *s);
void voice_play(N2 *n, const char *path);
void voice_stop(N2 *n);
bool voice_playing(N2 *n);
void mixer_update(N2 *n);
void mixer_render(struct Mixer *m, float *out, int frames);

Image *img_blur(Image *src, int radius);
Image *n2_load_image(N2 *n, const char *src);
bool n2_obj_alive(N2 *n, Obj *o);
void text_enter(N2 *n, Obj *o);

// Backlog (backlog.c)
void backlog_attach(N2 *n, Obj *o);
void backlog_set_scrollbar(N2 *n, Obj *bar, Obj *log);
void backlog_add(N2 *n, const char *text, const char *voice);
void backlog_clear(N2 *n);
int backlog_count(N2 *n);

// Saves (save.c)
Image *save_read_thumbnail(N2 *n, const char *path);
u8 *save_read_file(N2 *n, const char *path, size_t *size);
void save_write_file(N2 *n, const char *path, const u8 *data, size_t size);
bool conquest_read(N2 *n, const char *file, const char *scene);
void conquest_mark(N2 *n, Script *s, TextBlock *b);
Buf slot_state_write(N2 *n);
bool slot_state_read(N2 *n, const u8 *state, size_t size, const u8 *vars, size_t vsize);
void slot_read_vars(N2 *n, const u8 *d, size_t size);
void n2_reset(N2 *n, bool restartMain);

// Drawing helpers (draw.c)
void n2_blit(N2 *n, Obj *o, const Image *img, double x, double y, u32 alpha);
void n2_render_to(N2 *n, u32 *dst);
void backlog_draw(N2 *n, Obj *o);
void backlog_input(N2 *n);
void backlog_bar_request(N2 *n, Obj *bar, const char *act);
void voice_se(N2 *n, const char *path);
void sound_release_globals(N2 *n);
struct Font *font_open_named(const char *face);
bool npsf_write(const char *path, const u8 *data, size_t size);
u8 *npsf_read(const char *path, size_t *size);
void save_globals(N2 *n);
void load_globals(N2 *n);
bool save_slot(N2 *n, int slot);
bool load_slot(N2 *n, int slot);
bool save_exists(N2 *n, int slot);
void save_delete(N2 *n, int slot);

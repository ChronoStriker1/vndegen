#ifndef PJADV_INTERNAL_H
#define PJADV_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vnpjadv.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef int64_t i64;

typedef struct PJ PJ;

#define PJ_W 1920
#define PJ_H 1080

void *pj_alloc(size_t n);
void *pj_realloc(void *p, size_t n);
char *pj_strdup(const char *s);
char *pj_fmt(const char *fmt, ...);
u8 *pj_read_file(const char *path, size_t *size);
bool pj_write_file(const char *path, const void *data, size_t size);
bool pj_mkdirs(const char *dir);
int pj_path_resolve(const char *path, char *out, size_t size);
void pj_log(PJ *p, const char *fmt, ...);

int pj_utf8_decode(const char *s, u32 *cp);
bool pj_ieq(const char *a, const char *b);
static inline u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static inline u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static inline void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

typedef struct PacEntry {
    char name[33];
    int arc;
    u32 offset, size;
} PacEntry;

#define PJ_ARCS 8

typedef struct Pac {
    FILE *f[PJ_ARCS];
    int arcs;
    PacEntry *e;
    int count, cap;
    int *hash;
    int hashSize;
} Pac;

bool pj_pac_open(Pac *a, const char *dir, PJ *log);
void pj_pac_close(Pac *a);
const PacEntry *pj_pac_find(const Pac *a, const char *name);
u8 *pj_pac_read(Pac *a, const char *name, size_t *size);
const PacEntry *pj_pac_find_ext(const Pac *a, const char *base, const char *const *exts, char *found, size_t foundSize);

typedef struct Image {
    int w, h;
    u32 *px;
} Image;

Image *pj_image_new(int w, int h);
void pj_image_free(Image *im);
Image *pj_epa_decode(const u8 *data, size_t size);

typedef struct Ini {
    char **sec, **key, **val;
    int count, cap;
} Ini;

void pj_ini_parse(Ini *c, const u8 *data, size_t size);
void pj_ini_free(Ini *c);
const char *pj_ini_get(const Ini *c, const char *sec, const char *key);
int pj_ini_ints(const char *v, int *out, int max);

typedef struct Def {
    char name[40];
    char type[24];
    char src[3][40];
    int x, y;
    bool hasRect;
    int rx, ry, rw, rh;
    char anime[4][40];
    int animeCount;
    char child[4][40];
    int childCount;
    int alpha, scale, cx, cy;
    bool show;
    int xsize, ysize;
    bool filter;
} Def;

bool pj_def_parse(Def *d, const char *name, const u8 *data, size_t size);

enum { ANM_NONE, ANM_ONE, ANM_TWO };

typedef struct Anm {
    int kind;
    int type;
    u8 *data;
    size_t size;
    u32 start, end;
    u32 state[6];
    char sound[33];
} Anm;

bool pj_anm_parse(Anm *a, u8 *data, size_t size);
void pj_anm_free(Anm *a);

typedef struct Script {
    u8 *code;
    size_t codeSize;
    char **names;
    int nameCount;
    u8 *text;
    size_t textSize;
} Script;

bool pj_script_load(Script *s, Pac *a, char *err, size_t errSize);
void pj_script_free(Script *s);
const char *pj_script_name(const Script *s, int index);
char *pj_script_text(const Script *s, u32 offset);

#define PJ_TRACKS 4

typedef struct Track {
    bool live;
    u32 pc, end;
    int wait;
    int op, t, dur;
    int a0, a1;
    int done0, done1;
} Track;

#define PJ_LAYERS 64
#define PJ_SUBS 16
#define PJ_SUB_BASE 0x800

typedef struct Layer {
    bool used, shown;
    int id;
    char name[40];
    Def def;
    bool hasDef;
    Image *img;
    int sx, sy, sw, sh;
    int x, y;
    int ox, oy;
    int alpha;
    int scale;
    int parent;
    Anm anm[2];
    int anmCount;
    Track tr[PJ_TRACKS];
    bool animating;
    int btnState;
    int children[4];
    int childCount;
    bool isChild;
    u32 z;
} Layer;

enum { TW_ALPHA, TW_MOVE, TW_SCALE };

typedef struct Tween {
    int layer, kind;
    int d0, d1;
    int done0, done1;
} Tween;

#define PJ_TWEENS 32

typedef struct Button {
    bool used;
    int group, id;
    int x1, y1, x2, y2;
    Layer spr;
    u32 handler;
    bool hovered, pressed;
} Button;

#define PJ_BUTTONS 48

typedef struct Slider {
    bool used, shown;
    int id;
    int args[22];
    u32 var;
    int value;
    int dx, dy;
    Layer knob;
} Slider;

#define PJ_SLIDERS 8

typedef struct MsgWin {
    int x, y, w, h;
    int cw, ch, pitch;
    u32 color, edge;
    int font, indent;
} MsgWin;

enum { W_NONE, W_TIME, W_GROUP, W_TRANS, W_TEXT, W_FRAME };

#define PJ_STACK 64
#define PJ_GLOBALS 4096
#define PJ_SAVEVARS 256

typedef struct Mixer Mixer;
typedef struct Game Game;

struct PJ {
    VNPJADVConfig cfg;
    char *gameDir, *saveDir;
    FILE *log;
    char problem[256];
    char title[128];
    int lang;
    u32 rng;

    Pac pac;
    Script sc;
    Ini bgmIni;

    struct {
        char name[40];
        Image *img;
        u32 used;
    } cache[128];
    u32 cacheTick;

    Layer layer[PJ_LAYERS + PJ_SUBS];
    u32 zNext;
    Tween tween[PJ_TWEENS];
    int tweenCount;
    bool grouping;
    int groupMs, groupT;

    u32 *screen;
    u32 *shown;
    u32 *from;
    bool frozen;
    int transMs, transT;
    u32 serial;
    bool dirty;

    u32 pc;
    u32 stack[PJ_STACK];
    int sp;
    i32 reg[16];
    i32 saved[PJ_STACK][8];
    int savedCount;
    i32 R[PJ_SAVEVARS];
    i32 g[PJ_GLOBALS];
    i32 result;
    int wait;
    int waitMs;
    int activeGroup;
    bool halted;
    u32 keyHandler[16];
    u32 cancelHandler;

    Button btn[PJ_BUTTONS];
    int hover;
    int pressBtn;
    u32 pendingHandler;
    int callDepthAtHandler;
    Slider slider[PJ_SLIDERS];
    int dragSlider;

    MsgWin win[2];
    int msgFrameFile;
    int msgFrameLoaded;
    Layer msgFrame;
    bool msgShown;
    char *msgName, *msgText;
    int msgShownChars, msgTotalChars;
    int msgT;
    int textSpeed;

    int mx, my;
    bool mdown, rdown;
    bool click, rclick;
    int wheel;

    Mixer *mix;
    Game *game;
    u8 *sys, *recs;
    size_t sysSize, recsSize;

    bool quit;
    bool paused;
};

void pj_log(PJ *p, const char *fmt, ...);
void pj_fail(PJ *p, const char *fmt, ...);
u32 pj_rand(PJ *p);

Image *pj_image(PJ *p, const char *name);
void pj_layer_clear(PJ *p, int id);
Layer *pj_layer(PJ *p, int id);
bool pj_layer_load(PJ *p, int id, const char *name);
bool pj_sprite_load(PJ *p, Layer *l, const char *name);
void pj_sprite_free(Layer *l);
void pj_layer_set_state(PJ *p, Layer *l, int state);
void pj_layers_tick(PJ *p, int ms);
void pj_slider_place(PJ *p, Slider *s);
void pj_track_start(Layer *l, int t, u32 pc, u32 end);
void pj_track_tick(Layer *l, const Anm *a, int ti, int ms, u32 *rng);
void pj_anm1_start(Layer *l, const Anm *a);
bool pj_sprite_tick(PJ *p, Layer *l, int ms);
void pj_compose(PJ *p, u32 *dst);
void pj_freeze(PJ *p);
void pj_commit(PJ *p, int ms);
void pj_frame_tick(PJ *p, int ms);
void pj_blit(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int alpha, int scale, int cx, int cy);

void pj_vm_run(PJ *p, int ms);
void pj_call_handler(PJ *p, u32 target);
i32 *pj_var(PJ *p, u32 operand);

void pj_msg_init(PJ *p);
void pj_msg_draw(PJ *p, u32 *dst);
int pj_text_draw(u32 *dst, int dw, int dh, const char *s, int n, int x, int y, int size, u32 color, u32 edge);
int pj_text_width(const char *s, int n, int size);
int pj_char_width(u32 cp, int size);
void pj_font_init(PJ *p);
void pj_msg_tick(PJ *p, int ms);

Mixer *pj_mixer_new(int rate);
void pj_mixer_free(Mixer *m);
void pj_mixer_mix(Mixer *m, float *out, int frames, bool paused);
enum { CH_BGM, CH_SE, CH_VOICE, CH_COUNT };
void pj_play(PJ *p, int ch, int slot, const char *name, bool loop);
void pj_play_intro(PJ *p, int ch, int slot, const char *intro, const char *body);
void pj_stop(PJ *p, int ch, int slot, int fadeMs);
void pj_set_volume(PJ *p, int ch, float v);
bool pj_playing(PJ *p, int ch, int slot);

Game *pj_game_new(PJ *p, int kind, int chapter, int stage, i32 resultVar);
void pj_game_free(Game *g);
void pj_game_mouse(PJ *p, Game *g, int button, bool down);
void pj_game_wheel(PJ *p, Game *g, int notches);
bool pj_game_tick(PJ *p, Game *g, int ms);
void pj_game_draw(PJ *p, Game *g, u32 *dst);
int pj_game_result(Game *g);
int pj_game_icons(const Game *g, int *xs, int *ys, int *tools, int max);
int pj_game_tool(const Game *g);
int pj_game_phase(const Game *g);
bool pj_game_uses(const Game *g, const Image *im);
bool pj_stage_cleared(PJ *p, int chapter, int stage);
int pj_stage_best(PJ *p, int chapter, int stage);
void pj_stage_record(PJ *p, int chapter, int stage, int seconds);

bool pj_save_load(PJ *p);
bool pj_save_store(PJ *p);

#endif

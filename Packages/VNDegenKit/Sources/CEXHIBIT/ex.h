#pragma once

#include "ex_names.h"
#include "vnexhibit.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;

#define EX_W 1920
#define EX_H 1080

typedef struct Image {
    int w, h;
    u32 *px;
    bool alpha;
} Image;

typedef struct Op {
    u16 op;
    u8 flags, nv, ns;
    i32 *v;
    char **s;
} Op;

typedef struct Label {
    int id, pc;
} Label;

typedef struct Script {
    char *name;
    u8 *data;
    Op *ops;
    int count;
    char *library;
    Label *labels;
    int labelCount;
    Label *funcs;
    int funcCount;
} Script;

typedef struct Frame {
    Script *sc;
    int pc;
} Frame;

enum { W_NONE, W_TIME, W_CLICK, W_VIEW, W_TEXT, W_FADE, W_END };

typedef struct Anim {
    bool active;
    float from, to;
    u32 start, dur;
    int ease;
} Anim;

typedef struct Billboard {
    bool used;
    int id, kind;
    Image *img;
    bool ownImg;
    u32 color;
    int x, y, w, h;
    Anim alpha;
    float a;
    bool removing;
    Image *mask;
} Billboard;

typedef struct Item {
    int id, layer;
    Image *img;
    int states;
    bool enabled;
    int action[6];
    char *sound;
    char *disabledIf;
} Item;

typedef struct Layer {
    int id;
    int x, y, w, h;
    Anim mx, my, fade;
    float a;
    Item *items;
    int itemCount;
} Layer;

typedef struct View {
    bool active, shown;
    Layer *layers;
    int layerCount;
    int hover;
    int result;
    int resultReg;
} View;

typedef struct Mixer Mixer;

#define EX_CHANNELS 8
#define EX_BILLBOARDS 64

typedef struct EX {
    char *gameDir, *saveDir, *cacheDir;
    FILE *log;
    int audioRate;
    u32 now;
    u32 tickAcc;
    int wheel;
    char *title;
    u32 serial;
    u32 *screen;
    Image *cg;
    u32 cgColor;
    Image *cgNext;
    Anim cgFade;
    Billboard bb[EX_BILLBOARDS];
    View view;
    Script **scripts;
    int scriptCount;
    u32 keys[8];
    int keyCount;
    Frame stack[64];
    int sp;
    Script *sc;
    int pc;
    int wait;
    u32 waitUntil;
    i32 *R, *S;
    int nR, nS;
    int mouseX, mouseY;
    bool lDown, lPress, rPress;
    bool quit, paused, ended;
    char problem[256];
    Mixer *mixer;
    int volMaster, volBgm, volSe, volVoice;
    bool trace;
    char *unknownSeen;
} EX;

void *ex_alloc(size_t n);
void *ex_realloc(void *p, size_t n);
char *ex_strdup(const char *s);
char *ex_fmt(const char *fmt, ...);
int ex_path_resolve(const char *path, char *out, size_t size);
u8 *ex_read_file(const char *path, size_t *size);
bool ex_write_file(const char *path, const void *data, size_t size);
bool ex_mkdirs(const char *dir);
bool ex_file_exists(const char *path);
bool ex_ieq(const char *a, const char *b);
int ex_sjis_decode(const u8 *p, u32 *cp);
int ex_sjis_len(const u8 *p);
char *ex_sjis_to_utf8(const char *s);
void ex_log(EX *e, const char *fmt, ...);
char *ex_game_path(EX *e, const char *rel);

typedef struct MT {
    u32 mt[624];
    int i;
} MT;
void mt_seed(MT *m, u32 seed);
u32 mt_next(MT *m);

Image *image_new(int w, int h);
void image_free(Image *im);
Image *gyu_decode(const u8 *d, size_t n);
Image *image_load(EX *e, const char *rel);

bool rld_init(EX *e);
Script *rld_get(EX *e, const char *name);
void rld_free_all(EX *e);
int script_label(Script *sc, int id);
int script_func(Script *sc, int id);

int split_csv(char *s, char **out, int max);
int field_int(const char *s);

void vm_run(EX *e);
void vm_goto_script(EX *e, const char *name);
bool vm_cond(EX *e, const char *cond);
i32 *reg_ref(EX *e, char kind, int n);

float anim_value(EX *e, Anim *a, float cur);
void anim_start(EX *e, Anim *a, float from, float to, int ms, int ease);

void gfx_frame(EX *e);
void gfx_draw_cg(EX *e, int id, int ms);
int gfx_billboard(EX *e, char *args);
int gfx_remove_billboard(EX *e, char *args);
bool gfx_busy(EX *e);

void view_prepare(EX *e, const char *spec);
void view_show(EX *e, char *args);
void view_select(EX *e, char *args);
void view_destroy(EX *e);
void view_move(EX *e, char *args);
void view_fade(EX *e, char *args, bool enter);
void view_draw(EX *e, u32 *dst);
void view_input(EX *e);

Mixer *mixer_new(int rate);
void mixer_free(Mixer *m);
void mixer_mix(Mixer *m, float *out, int frames, bool paused);
void snd_play_id(EX *e, int id, int ch, bool loop, int fadeMs);
void snd_play_music(EX *e, int track, int fadeMs);
void snd_stop(EX *e, int ch, int fadeMs);
bool snd_playing(EX *e, int ch);
void snd_play_file(EX *e, const char *rel, int ch);

void blit(u32 *dst, int dw, int dh, const Image *src, int x, int y, int alpha);
void blit_part(u32 *dst, int dw, int dh, const Image *src, int sx, int sy, int w, int h, int dx, int dy, int alpha);
void fill(u32 *dst, int dw, int dh, int x, int y, int w, int h, u32 color, int alpha);

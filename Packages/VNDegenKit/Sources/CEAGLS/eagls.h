#ifndef EAGLS_H
#define EAGLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eg_names.h"
#include "vneagls.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef struct EG EG;

void *eg_alloc(size_t n);
void *eg_realloc(void *p, size_t n);
char *eg_strdup(const char *s);
char *eg_fmt(const char *fmt, ...);
u8 *eg_read_file(const char *path, size_t *size);
bool eg_write_file(const char *path, const void *data, size_t size);
bool eg_file_exists(const char *path);
bool eg_mkdirs(const char *dir);
int eg_path_resolve(const char *path, char *out, size_t size);
void eg_log(EG *e, const char *fmt, ...);
int eg_sjis_decode(const u8 *p, u32 *cp);
int eg_sjis_len(const u8 *p);
char *eg_sjis_to_utf8(const char *s);
char *eg_utf8_to_sjis(const char *s);
bool eg_ieq(const char *a, const char *b);
static inline u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static inline u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static inline void wr16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static inline void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

typedef struct PakEntry {
    char *name;
    u64 offset;
    u32 size;
} PakEntry;

typedef struct Pak {
    FILE *f;
    PakEntry *e;
    int count;
    int *hash;
    int hashSize;
} Pak;

enum { PK_CG, PK_SC, PK_WAVE, PK_COUNT };

bool pak_open(Pak *p, const char *dir, const char *base);
void pak_close(Pak *p);
const PakEntry *pak_find(Pak *p, const char *name);
u8 *pak_read(Pak *p, const PakEntry *e, size_t *size);
void eg_rand_seed(u32 *s, i32 seed);
int eg_rand(u32 *s);

typedef struct Ini {
    char **keys;
    char **values;
    int count;
} Ini;

bool ini_parse(Ini *ini, const u8 *data, size_t size);
void ini_free(Ini *ini);
const char *ini_get(const Ini *ini, const char *key);
int ini_ints(const Ini *ini, const char *key, int *out, int max);

typedef struct Image {
    int w, h;
    u32 *px;
    bool alpha;
} Image;

Image *image_new(int w, int h);
Image *image_copy(const Image *im);
void image_free(Image *im);
Image *bmp_decode(const u8 *data, size_t size);
Image *eg_image_load(EG *e, const char *name);

enum { A_NUM, A_STR, A_VAR, A_WORD, A_OP };

typedef struct Arg {
    u8 kind;
    char op;
    i32 num;
    char *s;
    struct Arg *index;
} Arg;

enum { I_CALL, I_TEXT, I_NAME, I_ASSIGN, I_IF, I_JMP, I_LOOP, I_NOP };

typedef struct Ins {
    u8 op;
    int line;
    int off;
    int n;
    int target;
    Arg *args;
    int argc;
    Arg var;
    char *s, *s2;
} Ins;

typedef struct Label {
    char *name;
    int ins;
} Label;

typedef struct Script {
    char *name;
    Ins *ins;
    int count, cap;
    Label *labels;
    int labelCount;
} Script;

Script *script_parse(const char *name, const u8 *data, size_t size);
void script_free(Script *s);
int script_label(const Script *s, const char *label);
int script_at_offset(const Script *s, int off);
u8 *script_decrypt(const u8 *data, size_t size, size_t *textSize);

typedef struct Val {
    i32 i;
    char *s;
} Val;

typedef struct Var {
    char *name;
    Val *v;
    int n;
    bool str;
} Var;

typedef struct VarTable {
    Var *vars;
    int count, cap;
    int *hash;
    int hashSize;
} VarTable;

Var *var_find(VarTable *t, const char *name);
Var *var_get(VarTable *t, const char *name, bool create);
void var_remove(VarTable *t, const char *name);
void var_clear(VarTable *t);
Val *var_at(Var *v, int index);

enum {
    W_NONE,
    W_FRAMES,
    W_TIME,
    W_TRANS,
    W_TEXT,
    W_CHOICE,
    W_UI,
    W_INPUT,
};

#define EG_STACK 64
#define EG_THREADS 8

typedef struct Frame {
    Script *sc;
    int pc;
} Frame;

typedef struct Thread {
    bool active;
    Script *sc;
    int pc;
    Frame stack[EG_STACK];
    int sp;
    int wait;
    int frames;
    u32 until;
    int msgOff;
    bool waited, backYield;
} Thread;

#define EG_PLANES 32

typedef struct Layer {
    bool vis;
    int plane;
    int dx, dy, dw, dh;
    int sx, sy, sw, sh;
    int alpha;
    int movie;
} Layer;

typedef struct Movie Movie;

typedef struct Mixer Mixer;
Mixer *mixer_new(int rate);
void mixer_free(Mixer *m);
void mixer_mix(Mixer *m, float *out, int frames, bool paused);
void mixer_set_movie(Mixer *m, Movie *mv, int channel);

#define EG_CHANNELS 8

void snd_play(EG *e, const char *name, int mode, int channel, int fadeMs);
void snd_stop_all(EG *e);
bool snd_playing(EG *e, int channel);
void snd_volumes(EG *e);

bool movie_open(EG *e, int channel, const char *file, int plane);
void movie_play(EG *e, int channel, bool loop);
void movie_stop(EG *e, int channel);
int movie_state(EG *e, int channel);
void movie_size(EG *e, int channel, int *w, int *h);
void movie_update(EG *e);
void movie_mix(Movie *m, float *out, int frames, float vol, int rate);
void movie_close_all(EG *e);
bool movie_any(EG *e);

typedef struct Font {
    int em;
    int size;
    bool bold;
    int spacing;
    int lineGap;
} Font;

typedef struct LogEntry {
    char *name;
    char *text;
    char *voice;
} LogEntry;

typedef struct ChoiceBox {
    bool used;
    int plane, hplane;
    int x, y, hx, hy;
    int sx, sy, hsx, hsy;
    int w, h;
    int tx, ty, htx, hty;
    char *text;
} ChoiceBox;

enum { UI_NONE, UI_SAVE, UI_LOAD, UI_CONFIG, UI_LOG, UI_MSGBOX, UI_ANIM, UI_BRA, UI_CG, UI_SCENE, UI_SOUND };

typedef struct SlotInfo {
    bool used;
    int year, month, day, hour, minute, second;
    long long stamp;
    char text[256];
} SlotInfo;

typedef struct UI UI;
typedef struct Gallery Gallery;

#define EG_SLOTS 80

struct EG {
    char *gameDir, *dataDir, *saveDir;
    FILE *log;
    char problem[512];
    char *title;
    long long startTime;
    Pak pak[PK_COUNT];
    Ini ini;
    int W, H;
    u32 *screen;
    u32 *shown;
    u32 *from;
    u32 *work;
    u32 serial;
    u32 now;
    double clockMs, tickAcc;
    bool paused, quit;
    bool trace;

    Script **scripts;
    int scriptCount;
    Thread th[EG_THREADS];
    Thread *cur;
    VarTable vars;
    u32 rng;

    Image *plane[EG_PLANES];
    char planeName[EG_PLANES][64];
    Layer layer[EG_PLANES];
    bool presentPending;

    bool transActive;
    int transEffect;
    Image *transMask;
    u32 transStart, transDur;
    bool transReverse;

    Image *mesImg[8];
    char mesName[8][64];
    int winRect[12];
    int nameRect[16];
    int mesMode, mesMode2;
    int mesAlpha;
    bool winHidden;
    Font fonts[16];
    char *nameFmt[4];
    char *speaker;
    char *voice;
    char *text;
    int textId;
    int textShown;
    u32 textStart;
    bool textDone;
    int textSpeed;
    bool autoMode, skipMode;
    u32 autoAt;
    LogEntry *logs;
    int logCount;
    Image *cursor;
    int cursorRect[9];

    ChoiceBox choice[8];
    char *choiceText[8];
    int choiceHover;
    int choiceCount;
    int choiceX, choiceY;
    char choiceVar[64];
    int choiceVarIndex;

    u32 timer[8];
    bool clicked, rclicked;
    int mx, my;
    bool lDown, rDown;
    bool click, rclick;
    int wheel;
    bool keyDown[256], keyPress[256];

    Mixer *mixer;
    int audioRate;
    Movie *movies[4];
    int volMaster, volBgm, volSe, volVoice, volMovie;
    int textSpeedSlider, autoSpeed, textWhite, mute, skipUnread, autoWaitVoice;
    Image *thumb;
    u8 *prevBlock;
    char bgmName[2][64];
    int bgmLoop[2];
    bool scriptUi, scriptUiDone;
    bool lPress;
    bool editing;
    char editBuf[64];
    int editMax;
    int editRect[4];

    int uiMode;
    UI *ui;
    bool nameFresh;
    Gallery *gallery;
    int menuHover;
    int winHover;
    char *pendingTitle;

    u8 *global;
    size_t globalSize;
    bool globalDirty;
};

void gfx_init(EG *e);
void gfx_close(EG *e);
void gfx_load(EG *e, const char *name, int plane);
void gfx_free_plane(EG *e, int plane);
void gfx_layer_reset(EG *e, int layer);
void gfx_set_layer(EG *e, int layer, int plane, const int *r, int alpha);
void gfx_copy(EG *e, int dst, int dx, int dy, int w, int h, int src, int sx, int sy);
void gfx_copy_mask(EG *e, int dst, int x, int y, int w, int h, int src, int sx, int sy, int maskPlane, int color);
int gfx_color_at(EG *e, int plane, int x, int y);
void gfx_present(EG *e);
void gfx_transition(EG *e, int effect, const char *mask, int ms, bool reverse);
bool gfx_transition_done(EG *e);
void gfx_frame(EG *e);
void gfx_compose(EG *e, u32 *dst);
void blit(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, int alpha);
void blit_part(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int w, int h, int dx, int dy, int alpha);
void blit_scaled(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int ddw, int ddh, int alpha);
void fill_rect(u32 *dst, int dw, int dh, int x, int y, int w, int h, u32 rgba);

void text_init(EG *e);
int text_advance(u32 cp, int size, int spacing);
int text_draw(EG *e, u32 *dst, int dw, int dh, const char *sjis, int x, int y, int size, int spacing, u32 color, bool outline);
int text_width(const char *sjis, int size, int spacing);

void msg_show(EG *e, int id, const char *text);
void msg_name(EG *e, const char *name, const char *voice);
int msg_tag(EG *e, const u8 *p, const Font **f, u32 *color);
const Font *msg_font(EG *e, int i);
void msg_clear(EG *e);
void msg_update(EG *e);
void msg_draw(EG *e, u32 *dst);
bool msg_complete(EG *e);
void msg_reveal(EG *e);
int msg_chars(const char *sjis);
void msg_window_parts(EG *e, int index, const char *name);

void choice_begin(EG *e);
void choice_update(EG *e);
void choice_draw(EG *e, u32 *dst);

void vm_init(EG *e);
void vm_goto(EG *e, Thread *t, const char *file, const char *label);
bool vm_call(EG *e, Thread *t, const char *file, const char *label);
void gallery_open(EG *e, int kind);
void gallery_update(EG *e);
void gallery_draw(EG *e, u32 *dst);
void gallery_free(EG *e);
bool gallery_scene_end(EG *e, Thread *t);
void vm_run(EG *e);
Script *vm_script(EG *e, const char *file);
i32 vm_int(EG *e, const Arg *a);
const char *vm_str(EG *e, const Arg *a);
Val *vm_ref(EG *e, const Arg *a, bool create);
void vm_set_int(EG *e, const Arg *a, i32 v);
void vm_set_str_name(EG *e, const char *name, const char *s);
const char *vm_name(EG *e, const Arg *a);
void cmd_exec(EG *e, Thread *t, const Ins *in);

void ui_init(EG *e);
void ui_free(EG *e);
void ui_open(EG *e, int mode);
void ui_update(EG *e);
void ui_draw(EG *e, u32 *dst);
void win_update(EG *e);
void win_draw(EG *e, u32 *dst);

bool save_slot(EG *e, int slot);
bool load_slot(EG *e, int slot);
bool save_exists(EG *e, int slot);
bool save_info(EG *e, int slot, SlotInfo *info, Image **thumb);
void save_delete(EG *e, int slot);
void save_capture_thumb(EG *e);
void global_load(EG *e);
void global_save(EG *e);
void config_load(EG *e);
void config_save(EG *e);
void game_to_title(EG *e);
void ui_open_title_load(EG *e);
void ui_open_sound(EG *e);
void ui_message_box(EG *e, int kind);
void ui_slots_changed(EG *e);
void ui_notice(EG *e, const char *text);
u8 *lzss_pack(const u8 *src, size_t n, size_t *outLen);
u8 *lzss_unpack(const u8 *src, size_t n, size_t outLen);

#endif

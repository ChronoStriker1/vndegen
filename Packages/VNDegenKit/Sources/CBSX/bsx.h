#ifndef BSX_H
#define BSX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vnbsx.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef struct BX BX;

void *bx_alloc(size_t n);
void *bx_realloc(void *p, size_t n);
char *bx_strdup(const char *s);
char *bx_fmt(const char *fmt, ...);
u8 *bx_read_file(const char *path, size_t *size);
bool bx_write_file(const char *path, const void *data, size_t size);
bool bx_file_exists(const char *path);
bool bx_mkdirs(const char *dir);
int bx_path_resolve(const char *path, char *out, size_t size);
void bx_log(BX *b, const char *fmt, ...);
char *bx_utf16_to_utf8(const u8 *p, size_t units);
char *bx_sjis_to_utf8(const char *s);
int bx_utf8_decode(const char *s, u32 *cp);
bool bx_ieq(const char *a, const char *b);
u32 bx_hash(const char *s);
static inline u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static inline u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static inline void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

typedef struct BsaEntry {
    char *name;
    int arc;
    u32 offset, size;
} BsaEntry;

typedef struct ResIndex {
    BsaEntry *e;
    int count, cap;
    int *hash;
    int hashSize;
} ResIndex;

enum { RX_GFX, RX_VOICE, RX_BGM, RX_BGS, RX_SE, RX_SYS, RX_DEF, RX_COUNT };

#define BX_ARCS 48

typedef struct Archives {
    FILE *f[BX_ARCS];
    char *path[BX_ARCS];
    int count;
    ResIndex ix[RX_COUNT];
} Archives;

bool bx_archives_open(Archives *a, const char *dir);
void bx_archives_close(Archives *a);
const BsaEntry *bx_res_find(Archives *a, int ix, const char *name);
u8 *bx_res_read(Archives *a, int ix, const char *name, size_t *size);

typedef struct Image {
    int w, h;
    int x, y;
    u32 *px;
} Image;

Image *bx_image_new(int w, int h);
void bx_image_free(Image *im);
Image *bx_bsg_decode(const u8 *data, size_t size);
Image *bx_bsg_part(const u8 *data, size_t size, int index);
Image *bx_image_load(BX *b, const char *name);
void bx_image_cache_clear(BX *b);

typedef struct Script {
    u8 *data;
    size_t size;
    char version[16];
    int minor;
    const u8 *code;
    u32 codeSize;
    u32 *labels;
    char **labelNames;
    int labelCount;
    char **strs;
    int strCount;
    char **vars;
    int varCount;
    char **chars;
    int charCount;
    char **msgs;
    int msgCount;
    u32 (*sysvars)[2];
    int sysvarCount;
    u32 hdr[6];
} Script;

Script *bx_script_load(const char *path, char *err, size_t errSize);
void bx_script_free(Script *s);
int bx_script_label(const Script *s, const char *name);
int bx_script_var(const Script *s, const char *name);

typedef struct Cfg {
    char **sec, **key, **val;
    int count, cap;
} Cfg;

bool bx_cfg_parse(Cfg *c, const u8 *data, size_t size);
void bx_cfg_free(Cfg *c);
const char *bx_cfg_get(const Cfg *c, const char *sec, const char *key);
int bx_cfg_ints(const Cfg *c, const char *sec, const char *key, int *out, int max);
int bx_cfg_int(const Cfg *c, const char *sec, const char *key, int def);
u32 bx_cfg_color(const Cfg *c, const char *sec, const char *key, u32 def);
int bx_cfg_field(const char *v, int index, char *out, size_t size);
int bx_eval(const char *expr, const char **end);

typedef struct Val {
    u8 str;
    i32 v;
} Val;

enum { VT_LOCAL = 2, VT_GLOBAL = 3, VT_ARG = 4, VT_TEMP = 5 };

enum {
    W_NONE,
    W_TIME,
    W_TRANS,
    W_TEXT,
    W_CHOICE,
    W_PHASE,
    W_CLICK,
    W_SOUND,
    W_ANIM,
};

#define BX_STACK 128
#define BX_LOCALS 1024
#define BX_ARGS 32
#define BX_TEMPS 32

typedef struct Thread {
    u32 pc;
    u32 stack[BX_STACK];
    u32 frameLabel[BX_STACK];
    int sp;
    int curLabel;
    int wait;
    u32 until;
    int skipLabel;
    bool halted;
} Thread;

enum { OK_BG = 1, OK_OVERLAY = 2, OK_CHAR = 3, OK_OBJ = 160 };

#define BX_OBJS 64

typedef struct KeyFrame {
    int no, x, y, opacity;
} KeyFrame;

typedef struct Anim {
    KeyFrame *frames;
    int count;
    int fps, begin, end;
    bool keep;
} Anim;

typedef struct Obj {
    bool used;
    int kind, id, charNo;
    char name[64];
    char face[64];
    int x, y;
    int seq;
    int anim;
    int ax0, ay0, ax1, ay1;
    u32 animStart, animDur;
    int alpha;
    int blend;
    Anim *key;
    int keyDelay;
    int kx, ky;
} Obj;

typedef struct Scene {
    Obj obj[BX_OBJS];
    int seq;
} Scene;

typedef struct Mixer Mixer;
Mixer *bx_mixer_new(int rate);
void bx_mixer_free(Mixer *m);
void bx_mixer_mix(Mixer *m, float *out, int frames, bool paused);

enum { CH_BGM, CH_BGS, CH_SE, CH_SE2, CH_VOICE, CH_SYS, CH_COUNT };

void bx_snd_play(BX *b, int ch, int ix, const char *name, bool loop, int fadeMs);
void bx_snd_stop(BX *b, int ch, int fadeMs);
bool bx_snd_playing(BX *b, int ch);
void bx_snd_volumes(BX *b);

typedef struct LogEntry {
    char *name;
    char *text;
    char *voice;
} LogEntry;

typedef struct Choice {
    int label;
    int msg;
    int design;
    int x, y, w, h;
} Choice;

typedef struct Button {
    char key[32];
    Image *img;
    int frames;
    int x, y;
    int group;
} Button;

enum { PH_NONE, PH_TITLE, PH_MAP, PH_CUSTOM_MAP, PH_DATA, PH_TITLE_LOAD, PH_CONFIG, PH_LOG };

typedef struct Phase Phase;
typedef struct MapPhase MapPhase;
typedef struct DataPhase DataPhase;
typedef struct LogPhase LogPhase;
typedef struct ConfigPhase ConfigPhase;

typedef struct Settings {
    int textSpeed, autoSpeed;
    int color[4];
    int attenuation;
    int skipUnread, ceaseSkip, noEffects;
    int voiceMask, voiceMode, autoSelect;
} Settings;

struct BX {
    char *gameDir, *saveDir;
    FILE *log;
    char problem[512];
    char *title;
    char saveId[33];
    long long startTime;
    bool trace;

    Archives arc;
    Script *sc;
    Cfg cfg;
    int W, H;

    u32 *screen;
    u32 *front;
    u32 *from;
    u32 serial;
    u32 now;
    double clockMs, tickAcc;
    bool paused, quit;

    Thread th;
    Val *globals;
    Val locals[BX_LOCALS];
    Val args[BX_ARGS];
    Val temps[BX_TEMPS];
    u32 rng;

    Scene scene, shown;
    int nextBlend;
    int objHover;
    bool shownDirty;
    u32 *to;
    int transType;
    u32 transStart, transDur;
    Image *transMask;
    bool transActive;
    u32 flashColor;
    u32 flashStart, flashDur;

    struct {
        char *key;
        Image *img;
    } cache[24];
    struct {
        char *name;
        Anim anim;
    } anims[16];
    int animCount;
    int cacheNext;

    Image *msgBase, *msgFrame, *msgName;
    int msgX, msgY, msgH;
    int nameRect[4], textRect[4];
    u32 nameColor, textColor, bgColor;
    int fontSize, fontW, fontH, lines, edge, shadow;
    int msgSpeed;
    bool winShown, winHidden;
    char *speaker;
    char *text;
    char *voice;
    int textLen;
    int textShown;
    u32 textStart;
    bool textDone;
    bool skipMode, autoMode;
    u32 autoAt;
    Image *unagashi;
    int unaX, unaY, unaW, unaH, unaFrames, unaSpeed;
    LogEntry *logs;
    int logCount;

    u32 msgPc;
    u32 hist[50];
    int histCount;
    int msgsShown;
    int bgsIndex;
    int bgmIndex;
    Image *thumb;

    Choice choice[16];
    int choiceCount;
    int choiceHover;
    Image *choiceItem[3];
    int choiceDatum;

    int phase;
    Phase *ph;
    MapPhase *map;
    DataPhase *data;
    LogPhase *logp;
    ConfigPhase *config;
    Settings set;
    int phaseBack;
    int phaseResult;

    Image *menuBase;
    int menuX, menuY;
    int menuShow[4];
    bool menuVisible;
    Button buttons[24];
    int buttonCount;
    int buttonHover;
    bool dressEnabled, viewEnabled;

    int mx, my;
    bool lDown, rDown;
    bool click, rclick, lPress;
    int wheel;
    bool keyDown[256], keyPress[256];

    Mixer *mixer;
    int audioRate;
    int volMaster, volBgm, volSe, volVoice, volBgs;
    char bgmName[64];
};

void bx_vm_init(BX *b);
void bx_vm_goto(BX *b, int label);
void bx_vm_run(BX *b);
Val bx_arg(BX *b, int i);
i32 bx_arg_int(BX *b, int i);
const char *bx_arg_str(BX *b, int i);
Val *bx_var_ref(BX *b, int type, i32 index);
int bx_var_by_name(BX *b, const char *name);
void bx_cmd(BX *b, int op);

void bx_gfx_init(BX *b);
void bx_gfx_close(BX *b);
Obj *bx_obj_find(BX *b, int kind, int id);
Obj *bx_obj_by_id(BX *b, int id);
void bx_obj_keyframes(BX *b, int id, const char *spec);
bool bx_anim_busy(BX *b);
Obj *bx_obj_put(BX *b, int kind, int id, const char *name, int x, int y);
void bx_obj_remove(BX *b, Obj *o);
void bx_obj_face(BX *b, Obj *o, const char *name);
void bx_transition(BX *b, int type, int ticks, const char *mask);
void bx_flash(BX *b, int r, int g, int bl, int ticks);
void bx_gfx_frame(BX *b);
void bx_compose(BX *b, const Scene *s, u32 *dst);
void bx_anim_update(BX *b);
bool bx_obj_move(BX *b, int id, int x, int y, int ms);
void bx_blit(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, int alpha);
void bx_blit_multiply(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, int alpha);
void bx_blit_part(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int w, int h, int dx, int dy, int alpha);
void bx_fill(u32 *dst, int dw, int dh, int x, int y, int w, int h, u32 rgba);

void bx_text_init(BX *b);
int bx_text_width(const char *utf8, int size, int width);
int bx_text_draw(BX *b, u32 *dst, int dw, int dh, const char *utf8, int x, int y, int size, int width, u32 color, int edge, int shadow);

void bx_msg_init(BX *b);
void bx_msg_show(BX *b, const char *name, const char *text, const char *voice);
void bx_msg_update(BX *b);
void bx_msg_draw(BX *b, u32 *dst);
bool bx_msg_complete(BX *b);
void bx_msg_reveal(BX *b);
void bx_msg_clear(BX *b);

void bx_choice_begin(BX *b);
void bx_choice_update(BX *b);
void bx_choice_draw(BX *b, u32 *dst);

void bx_ui_init(BX *b);
void bx_ui_free(BX *b);
void bx_ui_update(BX *b);
void bx_ui_draw(BX *b, u32 *dst);
void bx_phase_start(BX *b, int phase);
void bx_phase_update(BX *b);
void bx_phase_draw(BX *b, u32 *dst);
void bx_phase_end(BX *b);
void bx_phase_reset(BX *b);
void bx_system(BX *b, const char *cmd);

bool bx_save_slot(BX *b, int slot);
void bx_autosave(BX *b);
bool bx_load_slot(BX *b, int slot);
bool bx_save_exists(BX *b, int slot);
void bx_save_capture_thumb(BX *b);
char *bx_save_path(BX *b, int slot);

bool bx_map_start(BX *b);
void bx_map_update(BX *b);
void bx_map_draw(BX *b, u32 *dst);
void bx_map_free(BX *b);
int bx_map_hover(BX *b);

void bx_data_open(BX *b, bool save);
void bx_data_update(BX *b);
void bx_data_draw(BX *b, u32 *dst);
void bx_data_free(BX *b);
void bx_open_data(BX *b, bool save);

bool bx_log_open(BX *b);
void bx_log_update(BX *b);
void bx_log_draw(BX *b, u32 *dst);
void bx_log_free(BX *b);

bool bx_config_open(BX *b);
void bx_config_update(BX *b);
void bx_config_draw(BX *b, u32 *dst);
void bx_config_free(BX *b);
void bx_settings_load(BX *b);
void bx_settings_save(BX *b);
void bx_settings_apply(BX *b);
void bx_settings_defaults(BX *b);

#endif

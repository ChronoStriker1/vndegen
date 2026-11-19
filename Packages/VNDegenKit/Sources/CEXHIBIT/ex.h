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
typedef int16_t i16;
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
    int id, kind, srcId;
    char *srcFile;
    Image *img;
    bool ownImg;
    u32 color;
    int x, y, w, h;
    Anim alpha, mx, my;
    float a;
    bool removing, masked;
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

#define EX_SLOTS 110
#define EX_HISTORY 512

typedef struct SlotInfo {
    bool used, thumbLoaded;
    long long time;
    char *memo;
    Image *thumb;
} SlotInfo;

typedef struct SavePoint {
    Script *sc;
    int pc, sp;
    Frame stack[64];
} SavePoint;

typedef struct HistEntry {
    char *name, *text;
    int voice;
} HistEntry;

typedef struct Ui Ui;
typedef struct Extra Extra;
#define EX_SEEN_CG 10000
#define EX_SEEN_SCENES 64

#define EX_CFG_FLAGS 24

typedef struct Config {
    int windowT, textSpeed, autoSpeed;
    int vol[6], charVol[5];
    bool mute[6], charMute[5];
    int flagId[EX_CFG_FLAGS];
    bool flag[EX_CFG_FLAGS];
} Config;

#define EX_CHANNELS 8
#define EX_BILLBOARDS 64
#define EX_FRAMES 16
#define EX_CTRLS 24
#define EX_EMBEDDED 16
#define EX_ACTORS 16
#define EX_CHARDEFS 256
#define EX_PEOPLE 64

enum { EX_CH_MUSIC, EX_CH_VOICE, EX_CH_SE, EX_CH_UI, EX_CH_LOOP, EX_CH_ENV };

typedef struct TextStyle {
    int pitch, size;
    u32 color, shadow;
    int shx, shy;
    bool shadowOn;
} TextStyle;

typedef struct MsgFrame {
    bool used;
    Image *img;
    int x, y;
    int tx, ty, tw, th;
    int nx, ny, nw, nh;
    TextStyle text, name;
} MsgFrame;

typedef struct PlacedChar {
    u32 cp;
    i16 x, line;
    bool italic;
} PlacedChar;

typedef struct Embedded {
    int op, at, nv;
    i32 v[16];
    char *str;
} Embedded;

typedef struct Ctrl {
    int id, x, y, states;
    Image *img, *knob, *tip;
    int tipX, tipY;
    int action, action2;
    bool slider, on;
    int value;
} Ctrl;

#define EX_BUBBLES 8

typedef struct Bubble {
    Image *mask;
    int x, y, alpha;
    u32 color, edge;
    char *text;
} Bubble;

typedef struct Msg {
    MsgFrame frames[EX_FRAMES];
    int frame;
    bool shown, tempHidden, userHidden, ctrlShown;
    float a, tempA;
    Anim fade, tempFade;
    PlacedChar *chars;
    int count, visible;
    u32 *name;
    int nameLen;
    u32 start;
    bool active;
    int textId, speaker;
    Embedded *emb;
    int embCount, embNext;
    Image *waitImg;
    int waitX, waitY, waitFrames;
    Ctrl ctrls[EX_CTRLS];
    int ctrlCount, ctrlHover;
    struct Ctrl *dragging;
    u32 doneAt;
    Bubble bubbles[EX_BUBBLES];
    int bubbleCount;
    bool bubbleMode;
} Msg;

typedef struct CharaDef {
    int chara, pose, size, ox, oy, faces, fx, fy;
} CharaDef;

typedef struct Person {
    int id;
    bool voiced;
    char *name;
} Person;

typedef struct Actor {
    bool used;
    int chara, pose, face, order;
    Image *img;
    float x, y, a;
    Anim mx, my, fade;
    bool leaving;
} Actor;

typedef struct Portrait {
    bool used;
    int chara, pose, face, x, y;
    Image *img;
} Portrait;

#define EX_TRACKS 8
#define EX_STEPS 32

typedef struct Step {
    int ms, dx, dy, ease;
} Step;

typedef struct Track {
    bool used, running;
    int id, count, next;
    Step steps[EX_STEPS];
} Track;

typedef struct Camera {
    float x, y, w, h;
    Anim ax, ay, aw, ah;
} Camera;

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
    Msg msg;
    Camera cam;
    Track tracks[EX_TRACKS];
    CharaDef *defs;
    int defCount;
    Person people[EX_PEOPLE];
    int peopleCount;
    Actor actors[EX_ACTORS];
    int actorOrder;
    Portrait portraits[4];
    int portraitZ[4];
    void *fonts;
    char *fontFile;
    int textMs;
    SavePoint savePoint;
    int cgId, musicTrack, loopId, envId;
    u32 *saveShot;
    char *memo;
    SlotInfo slots[EX_SLOTS];
    HistEntry history[EX_HISTORY];
    int histCount, histHead;
    Ui *ui;
    Extra *extra;
    u8 seenCg[EX_SEEN_CG / 8];
    u8 seenScene[EX_SEEN_SCENES];
    Config cfg;
    int volLoop, volEnv, volUi, voiceChara;
    char *sysmenuSpec;
    int lastPage;
    bool skipping, autoMode, muted, ctrlHeld;
    int pendingAction;
    int autoMs;
    int windowAlpha;
    char *bootEntry;
    Image *cgPrev, *cgBlur;
    const Image *cgBlurSrc;
    int focus, cgBlurLevel;
    i32 *blockR;
    int blockCg, blockPc;
    Script *blockSc;
    bool replaying;
    int replayTarget, replayBlock;
    bool fadeoutPending;
    u32 cgPrevColor;
    Camera camPrev;
    i32 Q[16], L[128], F[64];
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
u32 colorref(u32 c);
int field_int(const char *s);

void vm_run(EX *e);
void vm_goto_script(EX *e, const char *name);
bool vm_cond(EX *e, const char *cond);
i32 *reg_ref(EX *e, char kind, int n);

float anim_value(EX *e, Anim *a, float cur);
void anim_start(EX *e, Anim *a, float from, float to, int ms, int ease);

void gfx_frame(EX *e);
void gfx_focus(EX *e, char *args);
int gfx_fadeout(EX *e, char *args);
void fadeout_finish(EX *e);
void gfx_camera(EX *e, float x, float y, float w, float h, int ms, int ease);
void gfx_move(EX *e, char *args);
void gfx_fade(EX *e, char *args);
int gfx_faceup(EX *e, char *args);
void chara_move(EX *e, int chara, int pos, int ms, int ease);
void snd_play_env(EX *e, int id, int volume);
long vm_eval(EX *e, const char *s);
void gfx_draw_cg(EX *e, int id, int ms);
int gfx_billboard(EX *e, char *args);
int gfx_remove_billboard(EX *e, char *args);
bool gfx_busy(EX *e);

void view_prepare(EX *e, const char *spec);
void view_show(EX *e, char *args);
void view_select(EX *e, char *args);
void view_destroy(EX *e);
Layer *view_layer(EX *e, int id);
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

void text_free(EX *e);
int text_advance(EX *e, u32 cp, int size);
int text_ascent(EX *e, int size);
int text_descent(EX *e, int size);
int text_width(EX *e, const u32 *cps, int n, int size);
void text_glyph(EX *e, u32 *dst, int dw, int dh, u32 cp, int x, int baseline, int size, u32 color, int alpha, bool italic);

void msg_define(EX *e, char *args);
void msg_wait_mark(EX *e, char *args);
void msg_ctrl_define(EX *e, char *args);
void msg_free(EX *e);
void msg_set_frame(EX *e, int id);
void msg_show(EX *e, bool on, int ms);
void msg_temp_hide(EX *e, bool hide, int ms);
void msg_clear(EX *e);
void msg_start(EX *e, Op *op);
bool msg_typing(EX *e);
void msg_finish(EX *e);
void msg_tick(EX *e);
void msg_flush_embedded(EX *e);
void msg_draw(EX *e, u32 *dst);
void msg_clear_bubbles(EX *e);
int text_block_width(EX *e, const char *sjis, int size);
bool msg_input(EX *e);
void snd_apply_volumes(EX *e);
void vm_exec_embedded(EX *e, Embedded *b);

void chara_create(EX *e, char *args);
void chara_define(EX *e, char *args);
const char *chara_name(EX *e, int id);
bool chara_voiced(EX *e, int id);
void chara_enter(EX *e, Op *op);
void chara_leave(EX *e, int id, int ms);
void chara_leave_all(EX *e, int ms);
Actor *chara_actor(EX *e, int layer);
void chara_draw(EX *e, u32 *dst);
void chara_update(EX *e);
bool chara_busy(EX *e);
void chara_free(EX *e);
void portrait_def(EX *e, int slot, int z);
void portrait_draw_cmd(EX *e, char *args);
void portrait_clear(EX *e, char *args);
void portrait_draw(EX *e, u32 *dst, int z);
bool snd_play_voice(EX *e, int id);

char *save_state(EX *e, size_t *len);
bool pc_save_write(EX *e, int slot, long long when, const u32 *shot);
bool pc_save_read(EX *e, int slot, char *script, size_t scriptSize, i32 **regs, int *count);
long long pc_slot_time(EX *e, int slot);
Image *pc_slot_thumb(EX *e, int slot);
bool replay_to(EX *e, const char *script, int target, const i32 *regs, int count);
bool load_state(EX *e, const char *data, size_t len);
u8 *gyu_encode(const u32 *px, int w, int h, size_t *outLen);
bool save_slot(EX *e, int slot);
bool load_slot(EX *e, int slot);
Image *slot_thumb(EX *e, int slot);
void slots_write_index(EX *e);
void slots_read_index(EX *e);
void slots_free(EX *e);
void gfx_set_cg(EX *e, int id, u32 color);
void gfx_restore_billboard(EX *e, const int *v, const char *file);
void chara_restore(EX *e, const int *v);
void history_add(EX *e, const char *name, const char *text, int voice);
void history_free(EX *e);
void state_reset(EX *e);
void reset_for_title(EX *e);

bool ui_open(EX *e, int action);
void extra_open(EX *e, int tab);
bool extra_active(EX *e);
void extra_free(EX *e);
void extra_input(EX *e);
void extra_draw(EX *e, u32 *dst);
void extra_load(EX *e);
void extra_mark_cg(EX *e, int id);
void extra_mark_scene(EX *e, int n);
void cfg_apply(EX *e);
void cfg_defaults(EX *e);
void cfg_load(EX *e);
void cfg_save(EX *e);
bool ui_active(EX *e);
void ui_input(EX *e);
void ui_draw(EX *e, u32 *dst);
void ui_close(EX *e);
void text_draw_sjis(EX *e, u32 *dst, const char *sjis, int x, int baseline, int size, u32 color, u32 shadow, int alpha);
int text_wrap_sjis(EX *e, u32 *dst, const char *sjis, int x, int top, int width, int lineH, int size, u32 color, u32 shadow, int alpha, int maxLines);

void blit_scaled(u32 *dst, int dw, int dh, const Image *src, float sx, float sy, float sw, float sh, float dx, float dy, float dwid, float dhei, int alpha);
void blit(u32 *dst, int dw, int dh, const Image *src, int x, int y, int alpha);
void blit_part(u32 *dst, int dw, int dh, const Image *src, int sx, int sy, int w, int h, int dx, int dy, int alpha);
void fill(u32 *dst, int dw, int dh, int x, int y, int w, int h, u32 color, int alpha);

#pragma once

#include "vnbruns.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;
typedef int64_t i64;

#define BR_W 1280
#define BR_H 720
#define BR_WORDS 262144
#define BR_FLAGS 4096
#define BR_STACK 64
#define BR_LOCALS 64
#define BR_STRINGS 16384
#define BR_SCRIPTS 10000
#define QUICK_SLOT 100

enum { N_INT, N_STR, N_SYM, N_LIST };

enum {
    A_NONE,
    A_DEFINE_SET,
    A_DEFINE_SUB,
    A_CALL_SUB,
    A_CALL_FAR,
    A_JUMP,
    A_RETURN,
    A_LABEL,
    A_GOTO,
    A_IF,
    A_EVAL,
    A_EVAL_NOEFFECT,
    A_SWITCH,
    A_SEND_STRING,
    A_CALL_USER_FUNC,
    A_ASSIGN_WORD,
    A_ASSIGN_LOCAL,
    A_ASSIGN_ACC,
    A_ASSIGN_REFLAG,
    A_IDREF_WORD,
    A_IDREF_LOCAL,
    A_IDREF_ACC,
    A_IDREF_REFLAG,
    A_PRE_INCR_WORD,
    A_PRE_INCR_LOCAL,
    A_PRE_INCR_ACC,
    A_PRE_DECR_WORD,
    A_PRE_DECR_LOCAL,
    A_PRE_DECR_ACC,
    A_ADD,
    A_SUB,
    A_MUL,
    A_DIV,
    A_MOD,
    A_EQ,
    A_NE,
    A_LT,
    A_LE,
    A_GT,
    A_GE,
    A_LAND,
    A_LOR,
    A_BAND,
    A_BOR,
    A_BXOR,
    A_SHL,
    A_SHR,
    A_NOT,
    A_NEG,
    A_THREE_OP,
    A_COUNT
};

typedef struct Node {
    u8 t, op;
    int n;
    i32 i;
    char *s;
    struct Node **k;
} Node;

typedef struct Unit {
    int id, locals;
    Node **st;
    int count;
    int *labelId, *labelPc;
    int labels;
} Unit;

typedef struct Script {
    int num;
    Node *root;
    Unit set;
    Unit *subs;
    int subCount;
} Script;

typedef struct Frame {
    Script *sc;
    Unit *u;
    int pc;
    i32 local[BR_LOCALS];
} Frame;

typedef struct Image {
    int w, h;
    u32 *px;
    int refs;
    bool boxed;
    int bx0, by0, bx1, by1;
} Image;

typedef struct FontSheet {
    bool tried;
    int w, h;
    u8 *px;
} FontSheet;

enum { P_IMAGE, P_BUTTON, P_TEXT, P_ANIM, P_TOUCHMAP };

#define BR_PARTS 20000
#define BR_OBJS 10000
#define BR_ANIM_FRAMES 64

typedef struct Parts {
    int id, type;
    Node *def;
    Node *pic[4];
    int fontSize;
    u32 color;
    char *text;
    Node *frame[BR_ANIM_FRAMES];
    int frameMs[BR_ANIM_FRAMES];
    int frames;
    Node *map;
} Parts;

enum { OBJ_IMG, OBJ_BTN, OBJ_KINDS };

typedef struct Obj {
    int kind, id, parts, layer, x, y, z, alpha, flag;
    int event, order;
    int hoverEvent;
    bool disabled, hidden, hover, pressed, hasColor;
    u32 color;
    int fontSize, w, h;
    char *text;
} Obj;

typedef struct Stream Stream;
typedef struct Mixer Mixer;

#define BR_CHANNELS 24
enum { CH_MUSIC = 0, CH_SE = 4, CH_VOICE = 12 };

#define BR_MSGWIN 64
#define BR_MSG_GLYPHS 1024

typedef struct MsgCfg {
    bool loaded;
    int x, y, w, h, tx, ty, tw, th, size, pitch, ruby, cpl;
    Node *bg;
    u32 edge;
    bool edgeOn;
    int kwx, kwy, kwIcon, kwIconAuto;
    int speed[3];
} MsgCfg;

typedef struct Glyph {
    u8 c[2];
    int x, y, size;
    u32 color;
} Glyph;

typedef struct Msg {
    int win;
    bool visible, instant;
    Glyph g[BR_MSG_GLYPHS];
    int count, shown, drawn;
    int cx, line;
    u32 color;
    u32 lastReveal, waitStart;
    Image *layer;
    char *raw;
    bool unread;
    int rubyX, rubyLine;
} Msg;

typedef struct Choice {
    bool active;
    char *item[8];
    int index[8];
    int count, hover;
} Choice;

typedef struct Menu {
    bool show, autohide, enable, pinned, open;
    int hover;
    bool off[32];
} Menu;

enum {
    LP_SWITCH = 8, LP_IX, LP_IY, LP_IW, LP_IH, LP_TX, LP_TY, LP_TW, LP_TH,
    LP_MSYS, LP_MSWITCH, LP_MIX, LP_MIY, LP_MIW, LP_MIH, LP_MTX, LP_MTY, LP_MTW, LP_MTH,
    LP_R, LP_G, LP_B, LP_A, LP_COUNT
};

typedef struct Layer {
    int no, order, mode, group, pic;
    bool visible;
    i32 v[LP_COUNT], from[LP_COUNT];
    u32 start, dur;
    int ease, easeDir;
    u32 shakeStart, shakeDur;
    int shakeX, shakeY;
    int tmParts, tmRegion, tmCursorMode;
    int tmBase[8];
    bool tmHas[8];
    char *tmCursor;
    int amMode, amEvent, amPos;
} Layer;

typedef struct Pic {
    int no, layer, url, type, numsel;
    char *path;
    Image *img;
    Stream *stream;
} Pic;

typedef struct Anim {
    float from, to;
    u32 start, dur;
} Anim;

typedef struct Cfg {
    Node *root;
    char scriptDir[64], scriptBase[32], scriptExt[16];
    int scriptDigits;
    char imageDir[64], partsDir[64], partsBase[32], musicDir[64], musicBase[32], effectDir[64], effectBase[32], voiceDir[64];
    int partsDigits, musicDigits, effectDigits, voiceDigits, voiceSceneDigits;
    int initialScript, titleScript;
} Cfg;

enum { W_NONE, W_TIME, W_CLICK, W_EVENT, W_FADE, W_TEXT, W_DIALOG, W_END, W_CHOICE };

typedef struct BR {
    char *gameDir, *saveDir;
    FILE *log;
    bool trace;
    Cfg cfg;
    Script *scripts[BR_SCRIPTS];
    bool scriptMissing[BR_SCRIPTS];
    i32 *word;
    i32 flag[BR_FLAGS];
    i32 acc, reflag, cond;
    char *str[BR_STRINGS];
    Frame stack[BR_STACK];
    int sp;
    bool systemMode, nextSystem;
    int nextScript;
    int wait;
    u32 waitUntil;
    int waitArg;
    u32 now;
    u32 tickAcc;
    u32 serial;
    bool paused, quit, ended;
    char problem[512];
    char title[256];
    u32 *screen;
    int mouseX, mouseY;
    bool lDown, lPress, rPress, ctrlHeld;
    int wheel;
    int audioRate;
    u8 *unknownSeen;
    FontSheet font[2][4];
    Parts *parts[BR_PARTS];
    bool partsMissing[BR_PARTS];
    Obj *obj[OBJ_KINDS][BR_OBJS];
    int objOrder;
    Image *bg, *bgPrev;
    int bgNum;
    Anim bgFade;
    Anim overlay;
    u32 overlayColor;
    int event;
    int eventMode, eventTimeout, eventFlag;
    Layer **layers;
    int layerCount, layerCap;
    Pic **pics;
    int picCount, picCap;
    Image *transFrom, *frozenImg;
    u32 transStart, transDur;
    bool frozen;
    Msg msg;
    MsgCfg msgCfg[BR_MSGWIN];
    bool skipping, skipMode, autoMode, wheelDown, msgHidden;
    int textSpeed, autoMs;
    Mixer *mixer;
    int music[4];
    bool muted;
    int vol[3], volMaster;
    float chanGain[BR_CHANNELS];
    int voiceNum[10];
    Node *sysRoot, *gameSnap, *loadedState;
    bool sysDirty, slotShotDirty;
    u32 sysSavedAt;
    Menu menu;
    int savedWait;
    int keyEvent;
    Choice choice;
    struct History *hist;
    int voiceCh, voiceId;
    u32 rng;
    u32 eventStart;
} BR;

void *br_alloc(size_t n);
void *br_realloc(void *p, size_t n);
char *br_strdup(const char *s);
char *br_fmt(const char *fmt, ...);
u8 *br_read_file(const char *path, size_t *size);
bool br_write_file(const char *path, const void *data, size_t size);
bool br_mkdirs(const char *dir);
bool br_file_exists(const char *path);
int br_sjis_decode(const u8 *p, u32 *cp);
int br_sjis_len(const u8 *p);
char *br_sjis_to_utf8(const char *s);
char *br_utf8_to_sjis(const char *s);
void br_log(BR *e, const char *fmt, ...);
char *br_game_path(BR *e, const char *rel);

u8 *br_eenc_decode(const u8 *d, size_t n, size_t *outLen);
u8 *br_eenc_encode(const u8 *d, size_t n, bool compress, size_t *outLen);
u8 *br_read_game(BR *e, const char *rel, size_t *size);

Node *sx_parse(const char *text, size_t len);
void sx_free(Node *n);
Node *sx_find(Node *list, const char *key);
Node *sx_assoc(Node *list, i32 key);
Node *sx_cdr_value(Node *pair);
bool sx_sym(const Node *n, const char *name);
int sx_int(const Node *n, int dflt);
const char *sx_str(const Node *n);

Script *script_get(BR *e, int num);
void script_free_all(BR *e);
Unit *script_sub(Script *sc, int id);
int unit_label(Unit *u, int id);

void vm_start(BR *e, int script);
void vm_jump(BR *e, int script);
void vm_run(BR *e);
i32 vm_eval(BR *e, Node *n);
i32 *vm_word(BR *e, i32 idx);

bool uf_call(BR *e, int id, Node *call);
void uf_string(BR *e, Node *call);
void uf_tick(BR *e);
void gfx_frame(BR *e);

Image *image_new(int w, int h);
void image_free(Image *im);
Image *image_decode(const u8 *d, size_t n);
u8 *image_decode_gray(const u8 *d, size_t n, int *w, int *h);
Image *image_load(BR *e, const char *rel);
void image_cache_clear(void);
Image *image_storage(BR *e, int num);
Image *image_parts(BR *e, int num);
Image *image_url(BR *e, Node *url);
void blit(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int alpha);
void blit_scaled(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int w, int h, int alpha);
void fill_rect(u32 *dst, int dw, int dh, int x, int y, int w, int h, u32 color, int alpha);

int font_char_width(const u8 *p, int size);
int font_text_width(const char *sjis, int size);
void font_draw_char(BR *e, u32 *dst, int dw, int dh, const u8 *p, int x, int y, int size, u32 color, u32 edge, int edgeW, int alpha);
void font_draw(BR *e, u32 *dst, int dw, int dh, const char *sjis, int x, int y, int size, u32 color, u32 edge, int edgeW, int alpha);
void font_free(BR *e);

Parts *parts_get(BR *e, int id);
void parts_free_all(BR *e);
Image *parts_image(BR *e, Parts *p, int state, u32 now);

float anim_value(BR *e, Anim *a);
void anim_set(BR *e, Anim *a, float from, float to, int ms);
bool anim_busy(BR *e, Anim *a);
Obj *obj_get(BR *e, int kind, int id);
Obj *obj_create(BR *e, int kind, int id, int parts, int layer, int x, int y, int z, int alpha, int flag);
void obj_delete(BR *e, int kind, int id);
void obj_clear(BR *e, int kind);
bool obj_hit(BR *e, Obj *o, int mx, int my);
int obj_sorted(BR *e, Obj **out, int max);
void bg_set(BR *e, int num, int ms);
bool bg_busy(BR *e);
void overlay_set(BR *e, u32 rgb, int alpha, int ms);
bool overlay_busy(BR *e);
void gfx_free(BR *e);
void layers_draw(BR *e, u32 *dst);
void msg_draw(BR *e, u32 *dst);
const char *str_get(BR *e, int id);
void str_set(BR *e, int id, const char *s);
bool layers_busy(BR *e);
void msg_send(BR *e, Node *call);
void msg_tick(BR *e);
void snd_se_hover(BR *e);
void blit_tinted(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int w, int h, int alpha, u32 tint);
Layer *layer_find(BR *e, int no);
Pic *pic_find(BR *e, int no);
void layers_sort(BR *e);
Layer *layer_new(BR *e, int no, int mode);
void layers_clear_pics(BR *e, int layer);
void pic_reload(BR *e, Pic *p);
Pic *pic_new(BR *e, int layer, int no, int url, const char *path, int type, int numsel);
void layers_reset(BR *e);
void layers_free(BR *e);
int layers_load(BR *e, int parent, int child, Node *src, int mode, int loop);
void layers_visible(BR *e, int id, bool on);
void layers_set(BR *e, int id, const i32 *a, int count);
void layers_freeze(BR *e);
void layers_transition(BR *e, int type, int ms);
Stream *stream_open(BR *e, int num, int loop, u32 now);
Image *stream_frame(BR *e, Stream *s, u32 now);
void stream_free(Stream *s);
void font_draw_layer(BR *e, Image *L, const u8 *p, int x, int y, int size, u32 color, u32 edge, int edgeW);
void msg_select(BR *e, int id);
void msg_show(BR *e, bool on);
void msg_append(BR *e, const char *s);
bool msg_typing(BR *e);
void msg_update(BR *e);
void msg_free(BR *e);
void history_add(BR *e, const char *s);
void history_page(BR *e);
Mixer *mixer_new(void);
void mixer_free(Mixer *m);
void mixer_mix(Mixer *m, float *out, int frames, bool paused);
void snd_apply_volumes(BR *e);
void snd_stop(BR *e, int ch, int fadeMs);
bool snd_playing(BR *e, int ch);
void snd_music(BR *e, int slot, int track, int fadeMs);
void snd_se(BR *e, int slot, int num);
void snd_voice(BR *e, int slot, int num);
void snd_volume(BR *e, int ch, int vol, int ms);
char *sx_print(Node *n, size_t *len);
Node *sx_copy(const Node *n);
void sys_load(BR *e);
bool sys_save(BR *e);
void sys_mark_dirty(BR *e);
Node *state_capture(BR *e, bool globals);
bool state_apply(BR *e, Node *st, bool globals);
bool save_slot(BR *e, int slot, const char *comment);
Node *slot_info(BR *e, int slot);
Node *slot_state(BR *e, int slot);
void slot_state_release(Node *st);
bool slot_comment(BR *e, int slot, const char *comment);
void seen_text(BR *e, int script, int line);
bool seen_text_has(BR *e, int script, int line);
void sys_call(BR *e, int script);
void sys_return(BR *e);
void sys_game_start(BR *e, int script, int sub);
bool sys_info(BR *e, int slot, int wA, int wB, int sC);
bool sys_load_slot(BR *e, int slot);
void sys_format_time(BR *e, int dst, i32 when);
void sys_title(BR *e);
void snd_stop_voices(BR *e);
bool menu_allowed(BR *e);
void menu_event(BR *e, const char *ev);
bool menu_input(BR *e);
void menu_draw(BR *e, u32 *dst);
void menu_builtin(BR *e, int id, int a, int b);
void snd_se_click(BR *e);
void image_bbox(Image *im, int *x0, int *y0, int *x1, int *y1);
u32 color_code(BR *e, int idx);
int history_height(BR *e);
int history_layout(BR *e, int scroll, int view, int textBase, int voiceBase, int count, int textX, int topY, int voiceX);
int history_voice(BR *e, int idx, int *ch);
void history_free(BR *e);
void history_voice_mark(BR *e, int ch, int num);
void choice_start(BR *e, Node *call);
void choice_tick(BR *e);
void choice_draw(BR *e, u32 *dst);
void choice_free(BR *e);
void cfg_apply(BR *e);
bool stream_done(Stream *s, u32 now);
void stream_restart(Stream *s, u32 now);
void pics_activate(BR *e, int a, int b);
void pics_delete(BR *e, int a, int b);
void pic_stream_restart(BR *e, int no);
void layer_order(BR *e, int no, int order);
void layer_shake(BR *e, int no, int ms, int ax, int ay);
void touch_attach(BR *e, int no, int parts);
void touch_base(BR *e, int no, int mode, int base);
void touch_cursor(BR *e, int no, int mode, const char *path);
int touch_poll(BR *e, int no);
int touch_position(BR *e);
bool touch_event(BR *e, int *out);
void touch_auto(BR *e, int no, int mode);
int touch_take(BR *e, int no);
int touch_last_pos(BR *e, int no);
void touch_tick(BR *e);

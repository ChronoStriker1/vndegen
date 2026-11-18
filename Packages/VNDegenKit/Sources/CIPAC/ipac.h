#ifndef IPAC_H
#define IPAC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ip_names.h"
#include "vnipac.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef struct IP IP;

void *ip_alloc(size_t n);
void *ip_realloc(void *p, size_t n);
char *ip_strdup(const char *s);
char *ip_fmt(const char *fmt, ...);
u8 *ip_read_file(const char *path, size_t *size);
bool ip_write_file(const char *path, const void *data, size_t size);
bool ip_file_exists(const char *path);
bool ip_mkdirs(const char *dir);
void ip_log(IP *t, const char *fmt, ...);
int ip_sjis_decode(const u8 *p, u32 *cp);
int ip_sjis_len(const u8 *p);
char *ip_sjis_to_utf8(const char *s);
char *ip_utf8_to_sjis(const char *s);
bool ip_ieq(const char *a, const char *b);
void ip_lower(char *s);
static inline u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static inline u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static inline void wr16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static inline void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

enum { PK_BG, PK_CHR, PK_PARTS, PK_SCR, PK_SE, PK_VOICE, PK_BGM, PK_KINDS };

typedef struct PakEntry {
    char *key;
    u32 offset, size;
    int file;
} PakEntry;

typedef struct PakGroup {
    PakEntry *entries;
    int count, cap;
    int *hash;
    int hashSize;
} PakGroup;

typedef struct Store {
    char *dataDir;
    FILE **files;
    int fileCount;
    PakGroup groups[PK_KINDS];
} Store;

bool store_open(Store *s, const char *gameDir, char *err, size_t errSize);
void store_close(Store *s);
u8 *store_load(Store *s, int kind, const char *name, const char *ext, size_t *size);
bool store_has(Store *s, int kind, const char *name, const char *ext);
u8 *lzss_decode(const u8 *src, size_t n, size_t outLen);
u8 *lzss_encode(const u8 *src, size_t n, size_t *outLen);

typedef struct Image {
    int w, h;
    u32 *px;
    bool opaque;
} Image;

Image *image_new(int w, int h);
void image_free(Image *im);
Image *ies_decode(const u8 *data, size_t size);
Image *image_load(IP *t, int kind, const char *name);
Image *image_get(IP *t, int kind, const char *name);
void image_cache_trim(IP *t);

enum {
    S_BLANK,
    S_CMD,
    S_OPEN,
    S_CLOSE,
    S_TEXT,
    S_NAME,
    S_VOICE,
    S_LABEL,
    S_BG,
    S_SPRITE,
    S_JUMP,
    S_CHOICE,
    S_CASE,
};

typedef struct Stmt {
    u8 kind;
    int line;
    int off;
    char *s;
} Stmt;

typedef struct Script {
    char *name;
    Stmt *st;
    int count, cap;
    int parseOff;
} Script;

Script *script_parse(const char *name, const u8 *data, size_t size);
Script *script_load(IP *t, const char *name);
void script_free(Script *s);
int script_label(const Script *s, const char *label);
int script_block_end(const Script *s, int open);

typedef struct Mixer Mixer;
Mixer *mixer_new(int rate);
void mixer_free(Mixer *m);
void mixer_mix(Mixer *m, float *out, int frames, bool paused);

enum { CH_BGM, CH_SE, CH_VOICE, CH_SYS, CH_COUNT };

void snd_bgm(IP *t, int number, int fadeFrames);
void snd_bgm_stop(IP *t, int fadeFrames);
void snd_se(IP *t, int number);
void snd_se_stop(IP *t);
void snd_voice(IP *t, const char *name);
void snd_voice_stop(IP *t);
bool snd_voice_playing(IP *t);
void snd_sys(IP *t, int number);
void snd_volumes(IP *t);

#define IP_ANIM 64

typedef struct Obj {
    Image *img;
    int frame;
    int animFrames;
    i16 animX[IP_ANIM], animY[IP_ANIM];
    char name[96];
    int x, y, alpha;
    bool vis;
    int mx, my, malpha;
    bool moving;
    int sx, sy, salpha;
    u32 mstart, mdur;
} Obj;

enum { SP_L, SP_C, SP_R, SP_COUNT };

typedef struct Sprite {
    Image *img;
    char name[96];
    int x, fromX, toX;
    u32 moveStart;
    bool moving;
} Sprite;

enum {
    W_NONE,
    W_FRAME,
    W_TIME,
    W_TEXT,
    W_TRANS,
    W_MOVE,
    W_CHOICE,
    W_QUIT,
};

enum { UI_NONE, UI_SAVE, UI_LOAD, UI_CONFIG, UI_MENU, UI_LOG, UI_EXIT, UI_TITLE_CONFIRM };

#define IP_VARS 1000
#define IP_OBJS 16
#define IP_CHOICES 8

typedef struct Choice {
    char *text;
} Choice;

typedef struct LogEntry {
    char *name;
    char *text;
    char voice[32];
} LogEntry;

typedef struct UI UI;

typedef struct VListItem {
    int page, slot;
    char *img;
} VListItem;

typedef struct ConfigItem {
    char *menu, *item;
    int x, y, w, h, bx, by;
} ConfigItem;

struct IP {
    char *gameDir, *saveDir;
    FILE *log;
    char problem[512];
    char *title;
    char *brand;
    long long startTime;
    Store store;
    int W, H;
    u32 *screen;
    u32 *scene;
    u32 *prev;
    u32 *shakeBuf;
    bool snapped;
    u32 serial;
    bool dirty;
    u32 now;
    double clockMs, tickAcc;
    bool paused, quit;
    bool trace;

    struct CacheEntry { int kind; char *name; Image *img; u32 used; } *cache;
    int cacheCount;
    u32 cacheTick;
    Script **scripts;
    int scriptCount;

    Script *sc;
    int pc;
    int wait;
    u32 waitUntil;
    bool waitSkippable;
    int depth;

    i32 vf[IP_VARS], vw[IP_VARS];
    u32 clk;
    int sel;

    char bgName[96];
    Image *bg;
    int bgX, bgY;
    Sprite spr[SP_COUNT];
    Obj obj[IP_OBJS];
    Image *clickMap;
    char clickName[96];
    int layerMode;
    Image *work;
    Image *workBg;
    Image *viewer;
    char viewerBase[96];
    int viewerIndex;
    u8 *sys;
    bool sysDirty;

    char ruleName[96];
    int speed;
    bool transActive;
    u32 transStart, transDur;
    Image *rule;

    int shakeX, shakeY;
    u32 shakeStart, shakeDur;

    bool winOpen;
    bool winHidden;
    char *msgName;
    u32 *msgText;
    int msgLen;
    char *msgRaw;
    int msgShown;
    u32 msgStart;
    char voice[32];
    int msgOff, msgLine;
    char msgVoice[32];
    int pendOff, pendLine;
    char pendingName[96];
    char *pendingText;
    bool autoMode, skipMode;
    u32 autoAt;
    int fontSize, pitch;
    int winAlpha;
    bool menuEnabled;

    Choice choices[IP_CHOICES];
    int choiceCount;
    int choiceHover;

    LogEntry *logs;
    int logCount;

    int uiMode;
    UI *ui;
    int textSpeed, autoSpeed;
    int speedSlider, autoSlider;
    bool skipUnread, voiceWait, inGame;
    int volBgm, volSe, volVoice;
    bool bgmOn, seOn, voiceOn;
    char sceneName[128];
    VListItem vlist[256];
    int vlistCount;
    ConfigItem config[96];
    int configCount;

    int mx, my;
    bool lDown, rDown;
    bool click, rclick;
    int wheel;
    bool keyDown[256], keyPress[256];

    Mixer *mixer;
    int audioRate;
    int bgmNumber;
    u32 rng;
};

void gfx_init(IP *t);
void gfx_close(IP *t);
void gfx_compose(IP *t);
void gfx_begin_transition(IP *t);
bool gfx_transition_done(IP *t);
void gfx_finish_transition(IP *t);
void gfx_set_bg(IP *t, const char *name);
void gfx_set_sprite(IP *t, int pos, const char *name);
void gfx_apply_now(IP *t);
bool gfx_move_sprite(IP *t, int from, int toward);
bool gfx_moving(IP *t);
int gfx_slot_x(int slot);
void gfx_update(IP *t);
u32 gfx_click_at(IP *t, int x, int y);
void blit(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, int alpha);
void blit_part(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int w, int h, int dx, int dy, int alpha);
void fill_rect(u32 *dst, int dw, int dh, int x, int y, int w, int h, u32 rgba);

void text_init(IP *t);
void text_close(void);
void text_glyph(u32 *dst, int dw, int dh, u32 cp, int x, int y, int size, u32 color, u32 shadow);
bool text_wide(u32 cp);
int text_width(const u32 *cps, int n, int pitch);
void text_line(u32 *dst, int dw, int dh, const u32 *cps, int n, int x, int y, int size, int pitch, u32 color, u32 shadow);
u32 *text_decode(const char *sjis, int *len);

void vm_start(IP *t, const char *script, const char *label);
void vm_run(IP *t);

void msg_show(IP *t);
void msg_draw(IP *t);
bool msg_complete(IP *t);
void msg_clear(IP *t);
void msg_update(IP *t);
void msg_reveal(IP *t);
int msg_button_at(IP *t, int x, int y);
void msg_reset(void);
void choice_update(IP *t);
void choice_draw(IP *t);

void ui_init(IP *t);
void ui_free(IP *t);
void ui_update(IP *t);
void ui_draw(IP *t);
void ui_open(IP *t, int mode);
bool ui_window_update(IP *t);
void ui_window_draw(IP *t);

typedef struct SlotInfo {
    char scene[128];
    int month, day, hour, minute;
} SlotInfo;

bool save_info(IP *t, int slot, SlotInfo *info);
void game_to_title(IP *t);
bool save_slot(IP *t, int slot);
bool load_slot(IP *t, int slot);
bool save_exists(IP *t, int slot);
void save_system(IP *t);
bool cg_seen(IP *t, int page, int slot);
void cg_mark(IP *t, const char *name);
bool scene_seen(IP *t, int n);
void scene_mark(IP *t, int n);
bool frame_rect(Image *im, int frames, int k, int *x, int *y, int *w, int *h);
bool anim_pos(IP *t, const Obj *o, int *x, int *y);
void viewer_open(IP *t, int page, int slot);
void viewer_next(IP *t);
void load_system(IP *t);

#endif

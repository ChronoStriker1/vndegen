#ifndef TH_H
#define TH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "th_names.h"
#include "vntmrhiro.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef struct TH TH;

void *th_alloc(size_t n);
void *th_realloc(void *p, size_t n);
char *th_strdup(const char *s);
char *th_fmt(const char *fmt, ...);
u8 *th_read_file(const char *path, size_t *size);
bool th_write_file(const char *path, const void *data, size_t size);
void th_log(TH *t, const char *fmt, ...);
int th_sjis_decode(const u8 *p, u32 *cp);
char *th_sjis_to_utf8(const char *s);
char *th_utf8_to_sjis(const char *s);
bool th_ieq(const char *a, const char *b);
int th_path_resolve(const char *path, char *out, size_t size);
bool th_mkdirs(const char *dir);
static inline u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static inline u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static inline u16 rdbe16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
static inline void wr16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static inline void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

typedef struct PacFile {
    char *path;
    FILE *f;
} PacFile;

typedef struct PacEntry {
    char *name;
    u64 offset;
    u32 size;
    int file;
} PacEntry;

typedef struct PacGroup {
    PacEntry *entries;
    int count, cap;
    int *hash;
    int hashSize;
} PacGroup;

enum { PAC_IMAGE, PAC_SCRIPT, PAC_BGM, PAC_SE, PAC_SSE, PAC_VOICE, PAC_MOVIE, PAC_KINDS };

typedef struct Store {
    PacFile *files;
    int fileCount;
    PacGroup groups[PAC_KINDS];
} Store;

bool store_open(Store *s, const char *dir, char *err, size_t errSize);
void store_close(Store *s);
const PacEntry *store_find(Store *s, int kind, const char *name);
u8 *store_read(Store *s, const PacEntry *e);
u8 *store_load(Store *s, int kind, const char *name, size_t *size);

typedef struct Image {
    int cw, ch;
    int x, y, w, h;
    u32 *px;
    bool opaque;
} Image;

Image *image_new(int cw, int ch, int x, int y, int w, int h);
void image_free(Image *im);
Image *image_copy(const Image *im);
Image *grd_decode(const u8 *data, size_t size);
Image *image_load(TH *t, const char *name);
Image *image_get(TH *t, const char *name);
Image *image_try(TH *t, const char *name);
void image_cache_trim(TH *t);

typedef struct IniLine {
    char *key;
    char *value;
} IniLine;

typedef struct IniSection {
    char *name;
    IniLine *lines;
    int count;
} IniSection;

typedef struct Ini {
    IniSection *sections;
    int count;
} Ini;

bool ini_load(Ini *ini, const u8 *data, size_t size);
void ini_free(Ini *ini);
const char *ini_get(const Ini *ini, const char *section, const char *key);
int ini_int(const Ini *ini, const char *section, const char *key, int def);
int ini_ints(const Ini *ini, const char *section, const char *key, int *out, int max);
const IniSection *ini_section(const Ini *ini, const char *section);

typedef struct Rec {
    u16 cls, op;
    u8 *body;
    int len;
} Rec;

typedef struct Scene {
    char *name;
    Rec *recs;
    int count;
} Scene;

Scene *scene_load(TH *t, const char *name);
int scene_label(const Scene *s, const char *label, int from);
void scene_free(Scene *s);

typedef struct Mixer Mixer;
typedef struct Movie Movie;
Mixer *mixer_new(int rate);
void mixer_free(Mixer *m);
void mixer_mix(Mixer *m, float *out, int frames, bool paused);
void mixer_set_movie(Mixer *m, Movie *movie);

enum { CH_BGM, CH_SE, CH_SEH1, CH_SEH2, CH_SEH3, CH_SEHV, CH_SEHV2, CH_SEHV3, CH_SEHV4, CH_VOICE, CH_SYSVOICE, CH_COUNT };
enum { VOL_MASTER, VOL_BGM, VOL_SE, VOL_VOICE };

void snd_play(TH *t, int ch, int kind, const char *name, bool loop, int fadeMs);
void snd_stop(TH *t, int ch, int fadeMs);
bool snd_playing(TH *t, int ch);
void snd_update(TH *t);
void snd_close(TH *t);
bool movie_play(TH *t, const char *name);
void movie_stop(TH *t);
void movie_mix(Movie *m, float *out, int frames, float vol, int rate);
void movie_update(TH *t, u32 elapsedMs);
bool movie_draw(TH *t);

typedef struct Sprite {
    char name[96];
    Image *img;
    int layer;
    float x, y;
    float sx, sy;
} Sprite;

typedef struct Overlay {
    int layer;
    Image *img;
    int x0, y0, x1, y1;
    int s0x, s0y, s1x, s1y;
    int a0, a1;
    u32 start, dur;
    char name[48];
} Overlay;

typedef struct Button {
    int x, y, w, h;
    char label[64];
    int sx, sy;
    int index;
} Button;

typedef struct Choice {
    char *text;
    char label[64];
} Choice;

enum { MAP_PLAIN, MAP_SLOT, MAP_ITEM, MAP_STATIC, MAP_GAUGE };

typedef struct MapButton {
    int x, y, w, h;
    int sx, sy, hx, hy;
    char voice[64];
    char label[64];
    int kind;
    int form;
    int index;
    char pic[64];
    int picX;
    char hoverSe[64], clickSe[64];
    int bx, by, bsx, bsy, bw, bh;
    char var[16];
} MapButton;

typedef struct MiniButton {
    bool defined, shown;
    int x, y, sx, sy, w, h;
} MiniButton;

typedef struct GalItem {
    int type, x1, y1, x2, y2;
    char *list;
} GalItem;

typedef struct BacklogEntry {
    char *name;
    char *text;
    char voice[48];
} BacklogEntry;

enum {
    W_NONE,
    W_TIME,
    W_TEXT,
    W_TRANS,
    W_BUTTONS,
    W_CHOICE,
    W_MAP,
    W_QUIT,
    W_FLASH,
    W_GALLERY,
    W_CGVIEW,
    W_MOVIE,
    W_ROULETTE,
};

enum { UI_NONE, UI_SAVE, UI_LOAD, UI_SYSTEM, UI_BACKLOG, UI_CONFIRM, UI_GALLERY };

#define TH_MAX_SPRITES 8
#define TH_STACK 64
#define TH_VARS 4096

typedef struct Frame {
    char scene[64];
    int pc;
} Frame;

typedef struct UI UI;

struct TH {
    char *gameDir, *saveDir;
    FILE *log;
    char problem[512];
    char *title;
    long long startTime;
    Store store;
    Ini sys, fileMacro, charaMacro, varIni, charaView, rouletteIni;
    int W, H;
    u32 *screen;
    u32 *sceneBuf;
    u32 *prevBuf;
    u32 serial;
    bool dirty, sceneDirty;
    u32 now;
    bool paused, quit;

    struct CacheEntry { char *name; Image *img; u32 used; } *cache;
    int cacheCount;
    u32 cacheTick;
    Scene **scenes;
    int sceneCount;

    char scene[64];
    Scene *sc;
    int pc;
    Frame stack[TH_STACK];
    int sp;
    int savePc;
    char bgmName[64];
    i32 vars[6][TH_VARS];
    int sceneId;
    char sceneTitle[256];
    int wait;
    u32 waitUntil;
    bool waitSkippable;

    char bgName[96];
    Image *bg;
    Sprite spr[TH_MAX_SPRITES];
    int sprCount;
    Overlay overlay[8];
    int overlayCount;
    int bgEffect;
    struct { char prefix[16]; bool on; int vol; } chars[16];
    int charCount;
    struct { bool on; int x0, y0, x1, y1, s0x, s0y, s1x, s1y; u32 start, dur; } bgMove;
    Image *navi;
    char naviName[64];
    bool transActive;
    u32 transStart, transDur;
    int transKind;
    u32 flashColor;
    u32 flashStart, flashDur;
    int flashCount;
    int shakeAmp;
    u32 shakeStart, shakeDur;
    struct { bool active; Image *img; int fromY, toY; u32 start, dur; } pan;

    bool winVisible, winHidden, winShown;
    char *msgName;
    char *msgText;
    int msgKind;
    u32 msgStart;
    int msgChars, msgShown;
    bool msgRead;
    char msgVoice[48];
    bool autoMode, skipMode;
    u32 autoAt;

    Button buttons[32];
    int buttonCount;
    char buttonImg[64], buttonImgOn[64];
    int hover;
    Choice choices[8];
    int choiceCount;
    MapButton maps[72];
    int mapCount;
    int mapRecs;
    char mapCancel[64];
    char hudName[64];
    bool hudOn, gaugeOn;
    MiniButton mini[2];
    int miniHover;
    struct { int id; Image *img; } caps[6];
    int capCount;
    struct { bool shown; int phase; float angle, from; u32 at, wait; char needle[64], wheel[64], startSe[32], loopSe[32], stopSe[32]; int nx, ny; } roulette;
    char mapBg[64], mapBase[64], mapSheet[64];
    int mapMode;
    bool mapBuilding, jumped, mapLastActive;
    struct { char name[64]; int x0, y0, x1, y1; u32 start, dur; int kind; } mapAnim[8];
    int mapAnimCount;

    GalItem gal[48];
    int galCount;
    char galNodata[64], galIncomplete[64], galBase[64], galOn[64];
    int galLoopPc;
    char viewList[64][32];
    int viewCount, viewIndex;
    Image *viewImg;
    bool galleryAll;
    u8 *galleryMarks;
    int replayDepth;
    char replayScene[64];
    int replayPc;

    BacklogEntry *backlog;
    int backlogCount;

    int uiMode;
    UI *ui;
    bool menuBlocked, skipBlocked, saveBlocked;
    int sysVol[4];
    int textSpeed, autoSpeed;
    u32 winStat;
    bool winBaseDraw;
    bool readColor;

    int mx, my;
    bool mouseMoved;
    bool lDown, rDown;
    bool click, rclick;
    int wheel;
    bool keyDown[256], keyPress[256];

    Mixer *mixer;
    Movie *movie;
    int audioRate;

    u32 rng;
    u8 *system000;
    size_t system000Size;
    bool systemDirty;
    u8 *readMarks;
};

void gfx_init(TH *t);
void gfx_close(TH *t);
void gfx_compose(TH *t);
void gfx_set_bg(TH *t, const char *name, int trans);
void gfx_set_sprites(TH *t, const char *list, int trans);
void gfx_set_overlay(TH *t, const u8 *b, const char *name);
void gfx_clear_overlay(TH *t, int layer);
bool gfx_overlays_moving(TH *t);
void gfx_move_bg(TH *t, const u8 *b, const char *name);
void gfx_clear_sprites(TH *t, int trans);
void gfx_begin_transition(TH *t, int kind);
bool gfx_transition_done(TH *t);
void gfx_finish_transition(TH *t);
void blit(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, int alpha);
void blit_part(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int w, int h, int dx, int dy, int alpha);
void blit_scaled(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, float sx, float sy, int alpha);
void fill_rect(u32 *dst, int dw, int dh, int x, int y, int w, int h, u32 rgba);
Image *compose_named(TH *t, const char *spec, bool chara);

void text_init(TH *t);
void text_close(TH *t);
int text_draw(TH *t, u32 *dst, int dw, int dh, const char *sjis, int x, int y, int size, u32 color, bool shadow, int maxChars);
int text_width(TH *t, const char *sjis, int size);
int text_count(const char *sjis);

void vm_start(TH *t, const char *scene, const char *label);
void vm_run(TH *t);
bool vm_call(TH *t, const char *scene, const char *label);
void vm_goto(TH *t, const char *scene, const char *label);

void msg_show(TH *t, int kind, const char *body);
void msg_draw(TH *t);
void msg_advance(TH *t);
bool msg_complete(TH *t);

void ui_update(TH *t);
void ui_draw(TH *t);
void ui_open(TH *t, int mode);
void ui_close(TH *t);
void ui_free(TH *t);
bool ui_bar_update(TH *t);
void ui_bar_draw(TH *t);
void buttons_update(TH *t);
void buttons_draw(TH *t);
bool map_place(TH *t, const MapButton *m, int *x, int *y);
bool mini_update(TH *t);
void mini_draw(TH *t, u32 *dst);
void roulette_start(TH *t, const char *section);
void roulette_update(TH *t);
void roulette_draw(TH *t);
void gfx_capture(TH *t, int id);
void map_draw(TH *t, u32 *dst, bool card);
void blit_rotated(u32 *dst, int dw, int dh, const Image *im, int cx, int cy, float degrees);

void gallery_page(TH *t, const u8 *b, int len);
void gallery_item(TH *t, const u8 *b, int len);
void gallery_update(TH *t);
void gallery_draw(TH *t);
void gallery_free(TH *t);
void gallery_mark_cg(TH *t, const char *name);
void gallery_mark_scene(TH *t, const char *name);

bool save_slot(TH *t, int slot);
void save_auto(TH *t);
bool load_slot(TH *t, int slot);
bool save_exists(TH *t, int slot);
void save_system(TH *t);
void load_system(TH *t);
void system_settings_changed(TH *t);
float voice_scale(TH *t, const char *name);
bool read_flag(TH *t, int sceneId, int rec, bool mark);

#endif

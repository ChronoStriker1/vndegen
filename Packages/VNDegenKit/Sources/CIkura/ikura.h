#ifndef IKURA_H
#define IKURA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ik_names.h"
#include "vnikura.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;
typedef int16_t i16;
typedef int64_t i64;

typedef struct IK IK;

void *ik_alloc(size_t n);
void *ik_realloc(void *p, size_t n);
char *ik_strdup(const char *s);
char *ik_fmt(const char *fmt, ...);
u8 *ik_read_file(const char *path, size_t *size);
bool ik_write_file(const char *path, const void *data, size_t size);
bool ik_file_exists(const char *path);
bool ik_mkdirs(const char *dir);
void ik_log(IK *t, const char *fmt, ...);
int ik_sjis_decode(const u8 *p, u32 *cp);
int ik_sjis_len(const u8 *p);
char *ik_sjis_to_utf8(const char *s);
bool ik_ieq(const char *a, const char *b);
void ik_lower(char *s);
static inline u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static inline u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static inline void wr16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static inline void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

enum { AR_GRP, AR_SNR, AR_SE, AR_BGM, AR_DATA, AR_VOICE, AR_KINDS };

typedef struct MpxEntry {
    char name[16];
    u32 offset, size;
    int file;
} MpxEntry;

typedef struct Store {
    FILE **files;
    int fileCount;
    MpxEntry *entries[AR_KINDS];
    int counts[AR_KINDS];
} Store;

bool store_open(Store *s, const char *gameDir, char *err, size_t errSize);
void store_close(Store *s);
u8 *store_load(Store *s, int kind, const char *name, size_t *size);

typedef struct Cell {
    int dx, dy, sx, sy, w, h;
} Cell;

typedef struct Image {
    int w, h;
    u32 *px;
    Cell *cells;
    int cellCount;
} Image;

Image *image_new(int w, int h);
void image_free(Image *im);
Image *image_decode(const u8 *data, size_t size);

typedef struct Script {
    char name[16];
    u8 *data;
    size_t size;
    u32 *labels;
    int labelCount;
    i16 *labelAt;
    const u8 *code;
    u32 codeSize;
    int readSlot;
} Script;

Script *script_load(IK *t, const char *name);
void script_free(Script *s);

typedef struct Anim {
    bool on;
    u32 start, dur;
    int from[4], to[4];
} Anim;

typedef struct Layer {
    bool made;
    int w, h, x, y;
    int img, sx, sy;
    int argb[4];
    bool shown, ready, fill, textured, quad, kept;
    int tw, th;
    u32 fillArgb;
    Image *surf;
    float rot;
    Anim fade, move;
} Layer;

typedef struct ButtonState {
    int img, sx, sy;
} ButtonState;

typedef struct ButtonTip {
    bool on;
    int layer, x, y, w, h, img, sx, sy;
} ButtonTip;

typedef struct Scroll {
    int img, page;
    int cells[9];
    bool dragging;
} Scroll;

typedef struct Button {
    bool made, shown, enabled;
    int type, layer, x, y, w, h;
    ButtonState state[4];
    u8 stateFlag[4];
    u8 mode;
    int var, max, value;
    u32 raw[7];
    ButtonTip tip;
    int label;
    bool bound, call;
    Script *sc;
    Scroll scroll;
} Button;

#define IK_BUTTONS 512
#define IK_CANCEL 0x7fffffff

typedef struct Frame {
    Script *sc;
    u32 pc;
    bool resumeMsg;
} Frame;

typedef struct Glyph Glyph;

typedef struct Font {
    bool ok, embolden;
    int size, shadow, ascent;
    float scale;
    u8 *data;
    void *info;
    Glyph *glyphs;
    int count, cap;
} Font;

#define IK_FONTS 4

typedef struct MsgChar {
    u32 cp;
    int x, y;
} MsgChar;

typedef struct MsgWin {
    int layer, x, y, right, bottom, pitch;
    int nameLayer, nameX, nameY, nameW, nameH, nameImg, nameCount;
    int nameSrc[32][2];
    int curLayer, curX, curY, curW, curH, curImg, curFrames;
    int curSrc[16][2];
    int font;
    u32 colour, shadow;
    bool shown;
    MsgChar *chars;
    int count, cap, revealed;
    int penX, penY;
    int name;
    const u8 *tags;
    int tagLen, tagPos;
    u32 revealAt, cursorAt;
    bool cursor, unread;
    int msPerChar;
    char *raw;
    int rawLen, rawCap;
    char voice[16];
} MsgWin;

#define IK_LOG 100
#define IK_LOG_SLOTS 64

typedef struct LogEntry {
    char *text;
    int name;
    char voice[16];
} LogEntry;

#define IK_STACK 32

enum { W_NONE, W_TIME, W_ANIM, W_INPUT, W_MSG, W_VOICE, W_MOVIE, W_QUIT };

#define IK_SLOTS 16
#define IK_DEFAULT_VOLUME 128
enum { CH_BGM, CH_VOICE, CH_SE, IK_CHANNELS = CH_SE + IK_SLOTS };

typedef struct Mixer Mixer;
typedef struct Movie Movie;
typedef struct Gan Gan;

typedef struct SaveFile {
    char *path;
    int slots;
    u8 *snap;
    size_t snapSize;
    bool haveSnap, globalLoaded;
    u8 *read;
    int readUnits;
    bool readDirty;
} SaveFile;

#define IK_MAX_W 1024

typedef struct Column {
    float wz, lat2, u, dx, dz;
    int kind, top, bot;
} Column;

typedef struct Dungeon {
    u8 map[81 * 81];
    bool loaded, on, animating, lutReady;
    Image *tex[4];
    int x, y, dir;
    float fromYaw, toYaw, fromX, fromZ;
    u32 animStart;
    int compass, marker, mapX, mapY, cellW, cellH;
    u16 light[4096];
    u32 *view;
    Column cols[IK_MAX_W];
    bool stale;
    float viewYaw, viewX, viewZ;
} Dungeon;

typedef struct Automap {
    int w, h, n;
    size_t size;
    u8 *bits;
} Automap;

typedef struct SeSlot {
    char name[16];
} SeSlot;

struct IK {
    char *gameDir, *saveDir;
    FILE *log;
    char problem[512];
    bool trace;
    Store store;
    int W, H;
    u32 *screen;
    u32 serial;
    bool dirty;
    u32 now;
    double clockMs, tickAcc;
    bool paused, quit;

    Script **scripts;
    int scriptCount;
    Script *sc;
    u32 pc;
    Frame stack[IK_STACK];
    int sp;
    int wait;
    u32 waitUntil, inputUntil, inputPc;
    bool inputTimed;

    i32 *vars;
    int varCount;
    Image **images;
    int imageCount;
    Layer *layers;
    int layerCount;
    int *order;
    Button *buttons;
    int hover, pressed;
    int cancelLabel;
    bool cancelBound, cancelCall;
    Script *cancelSc;
    Script *wheelSc;
    int wheelLabel, wheelVar;
    bool wheelCall, wheelBound;
    Script *resumeSc;
    int resumeLabel;
    int msgMode;
    i32 settings[64];
    char **texts;
    int textCount;
    char (*imageNames)[16];
    SaveFile save;
    u8 *savedEnabled[4];
    struct {
        bool bound, call;
        Script *sc;
        int label;
        int keys[256];
    } savedInput[4];
    int keyButton[256];
    int skipMode, autoMode, modeButton[2], lastToggle;
    u32 autoAt;
    u8 *flags;
    int flagCount, flagBits;
    Font fonts[IK_FONTS];
    MsgWin msg;
    LogEntry backlog[IK_LOG];
    int logCount, logHead;
    char logVoice[IK_LOG_SLOTS][16];
    char **names;
    int nameCount;
    bool namesLoaded;
    Mixer *mixer;
    SeSlot slots[IK_SLOTS];
    char bgmName[16];
    bool bgmLoop;
    int buttonCount;
    u32 rng;
    Dungeon dun;
    Movie *movie;
    Gan *gan;
    Automap automap;

    int mx, my;
    bool lDown, rDown, click, rclick;
    int wheel;
    bool keyDown[256], keyPress[256];
    int audioRate;
};

u32 ik_value(IK *t, u32 raw);
void vm_start(IK *t, const char *script);
void vm_run(IK *t);
void gfx_init(IK *t);
void gfx_close(IK *t);
void gfx_update(IK *t);
bool gfx_animating(IK *t);
void gfx_compose(IK *t);
bool layer_drawn(const IK *t, int i);
bool buttons_live(const IK *t);
void image_blit(Image *dst, const Image *src, int sx, int sy, int w, int h, int x0, int y0, const int argb[4], u32 minAlpha,
                bool copy);
Image *layer_surface(IK *t, Layer *l);
void layer_restore(IK *t, Layer *l, int x, int y, int w, int h);
void gfx_blit(IK *t, const int argb[4], int img, int sx, int sy, int w, int h, int x0, int y0);
void layer_move(IK *t, Layer *l, int ms, int x, int y);
void gfx_blit_min(IK *t, const int argb[4], int img, int sx, int sy, int w, int h, int x0, int y0, u32 minAlpha);
int button_at(IK *t, int x, int y);
void buttons_input(IK *t);
void vm_branch(IK *t, Script *sc, int label, bool call);
void text_close(IK *t);
void font_set(IK *t, int slot, const char *name, int size, int shadow, bool bold);
int text_advance(IK *t, int slot, u32 cp);
void text_draw(IK *t, int slot, u32 cp, int x, int y, u32 colour, u32 shadow, const int clip[4]);
bool msg_exec(IK *t, u16 op, const u8 *a, int n);
void msg_tick(IK *t);
void msg_draw(IK *t, int layer);
void msg_close(IK *t);
void msg_stop_modes(IK *t);
void msg_log_voice(IK *t, int slot);
void scrollbar_exec(IK *t, u16 op, const u8 *a, int n);
void snd_init(IK *t);
void snd_close(IK *t);
void snd_se_load(IK *t, int slot, const char *name);
void snd_se_play(IK *t, int slot, bool loop);
void snd_se_stop(IK *t, int slot);
void snd_voice(IK *t, const char *name);
void snd_voice_stop(IK *t);
bool snd_voice_playing(IK *t);
void snd_bgm(IK *t, const char *name, bool loop);
void snd_bgm_fade(IK *t, int ms);
void snd_mix(IK *t, float *out, int frames);
void snd_volume(IK *t, int ch, int v);
bool save_exec(IK *t, u16 op, const u8 *a, int n);
bool text_exec(IK *t, u16 op, const u8 *a, int n);
void save_close(IK *t);
bool save_read_mark(IK *t, int slot, u32 n);
void settings_apply(IK *t);
void text_draw_on(IK *t, Image *dst, int slot, u32 cp, int x, int y, u32 colour, u32 shadow, const int clip[4]);
void vm_set_text(IK *t, int slot, const char *s, int len);
void image_load(IK *t, int slot, const char *name);
bool dun_exec(IK *t, u16 op, const u8 *a, int n);
void dun_render(IK *t);
void dun_update(IK *t);
void dun_close(IK *t);
void automap_exec(IK *t, u16 op, const u8 *a, int n);
bool movie_play(IK *t, const char *name, int x, int y, int w, int h, bool skippable);
void movie_stop(IK *t);
void movie_update(IK *t, double elapsedMs);
void movie_draw(IK *t);
void movie_mix(Movie *m, float *out, int frames, float vol, int rate);
void snd_set_movie(IK *t, Movie *m);
bool gan_exec(IK *t, u16 op, const u8 *a, int n);
bool equip_exec(IK *t, u16 op, const u8 *a, int n);
void gan_update(IK *t);
void gan_draw(IK *t, int layer);
bool gan_drawn(const IK *t, int layer);
void gan_free(IK *t);
void gan_save(IK *t, u8 out[32]);
void gan_restore(IK *t, const u8 in[32]);

#endif

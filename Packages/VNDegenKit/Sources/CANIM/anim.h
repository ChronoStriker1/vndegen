#ifndef ANIM_H
#define ANIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vnanim.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef struct AN AN;

void *an_alloc(size_t n);
void *an_realloc(void *p, size_t n);
char *an_strdup(const char *s);
char *an_fmt(const char *fmt, ...);
u8 *an_read_file(const char *path, size_t *size);
bool an_write_file(const char *path, const void *data, size_t size);
bool an_file_exists(const char *path);
bool an_mkdirs(const char *dir);
int an_path_resolve(const char *path, char *out, size_t size);
void an_log(AN *a, const char *fmt, ...);
char *an_sjis_to_utf8(const char *s);
char *an_utf8_to_sjis(const char *s);
int an_utf8_decode(const char *s, u32 *cp);
bool an_ieq(const char *a, const char *b);
u32 an_hash(const char *s);
static inline u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static inline u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static inline void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void an_gax_crypt(u8 key[16], u8 *data, size_t size, bool encrypt);
u8 *an_gax_open(const u8 *file, size_t size, size_t *outSize);

typedef struct Image {
    int w, h;
    int x0, y0, cw, ch;
    u32 *px;
} Image;

Image *an_image_new(int w, int h);
void an_image_free(Image *im);
Image *an_image_png(const u8 *data, size_t size);
Image *an_image_load(AN *a, const char *name);
void an_image_cache_clear(AN *a);
static inline u32 an_image_at(const Image *im, int x, int y) {
    x -= im->x0;
    y -= im->y0;
    if (x < 0 || y < 0 || x >= im->cw || y >= im->ch) return 0;
    return im->px[(size_t)y * (size_t)im->cw + (size_t)x];
}

typedef struct Script {
    u8 *data;
    size_t size;
    u32 codeEnd, tableEnd;
    u32 *labelAddr;
    char **labelName;
    int labelCount;
    char **strs;
    int strCount;
} Script;

Script *an_script_load(const char *path, char *err, size_t errSize);
void an_script_free(Script *s);
int an_script_label(const Script *s, const char *name);
const char *an_script_label_at(const Script *s, u32 pc);

typedef struct SpriteName {
    char *name, *file;
} SpriteName;

typedef struct EmoteVar {
    char *name;
    float value;
} EmoteVar;

typedef struct EmotePreset {
    char *name;
    EmoteVar *vars;
    int count;
} EmotePreset;

typedef struct IdleVar {
    char *name;
    float lo, hi;
} IdleVar;

typedef struct IdleMotion {
    char *name;
    IdleVar *vars;
    int count;
} IdleMotion;

typedef struct Define {
    u8 *data;
    size_t size;
    SpriteName *sprites;
    int spriteCount;
    EmotePreset *presets;
    int presetCount;
    IdleMotion *idles;
    int idleCount;
    char charName[16][16];
    float charScale[16];
    int charCount;
    char *sample;
} Define;

bool an_define_load(Define *d, const char *path);
void an_define_free(Define *d);
const char *an_define_sprite(const Define *d, const char *name);
const EmotePreset *an_define_preset(const Define *d, const char *name);
const IdleMotion *an_define_idle(const Define *d, const char *name);
float an_define_char_scale(const Define *d, const char *model);

typedef struct PackEntry {
    char *name;
    u32 offset, size;
} PackEntry;

typedef struct Pack {
    FILE *f;
    PackEntry *e;
    int count;
    int *hash;
    int hashSize;
} Pack;

bool an_pack_open(Pack *p, const char *path, bool wide);
void an_pack_close(Pack *p);
const PackEntry *an_pack_find(const Pack *p, const char *name);
u8 *an_pack_read(Pack *p, const PackEntry *e);

enum { W_NONE, W_FRAME, W_TIME, W_TEXT, W_TRANS, W_CHOICE, W_MOVIE, W_END };

#define AN_STACK 64
#define AN_VARS 1024

typedef struct Thread {
    u32 pc;
    u32 stack[AN_STACK];
    int sp;
    int wait;
    u32 waitUntil;
} Thread;

enum { LK_NONE, LK_IMAGE, LK_EMOTE };

#define AN_LAYERS 16

typedef struct Layer {
    int kind;
    char name[64];
    int x, y;
    int fromX, fromY, toX, toY;
    u32 moveStart, moveTime;
    int alpha, fromAlpha, toAlpha;
    u32 fadeStart, fadeTime;
    int scale, fromScale;
    u32 scaleStart, scaleTime;
    char motion[64];
    u32 motionTime;
    struct LayerVar {
        char name[32];
        float value;
    } vars[48];
    int varCount;
    u32 varSerial;
    bool fresh;
    char idle[32];
} Layer;

typedef struct Scene {
    char bg[64];
    int bgX, bgY, bgFromX, bgFromY;
    u32 bgMoveStart, bgMoveTime;
    int bgScale;
    bool camera;
    Layer layer[AN_LAYERS];
    bool window;
} Scene;

typedef struct Choice {
    int value;
    char *text;
    int x, y, w, h;
} Choice;

typedef struct LogEntry {
    char *name, *text, *voice;
} LogEntry;

#define AN_SE 16

enum { PH_TITLE, PH_GAME };
enum { SCR_NONE, SCR_HIDDEN, SCR_LOG, SCR_SAVE, SCR_LOAD, SCR_FLOW, SCR_VOLUME, SCR_SETTINGS };
enum { MSG_TITLE, MSG_QUIT, MSG_OVERWRITE, MSG_DELETE, MSG_JUMP, MSG_RESET_FLOW, MSG_RESET_SAVES };
enum { ACT_SAVE, ACT_LOAD, ACT_TITLE, ACT_QUIT, ACT_JUMP, ACT_DELETE, ACT_RESET_SAVES };
enum { BAR_QSAVE, BAR_QLOAD, BAR_SAVE, BAR_LOAD, BAR_FLOW, BAR_SET, BAR_VOL, BAR_SKIP, BAR_AUTO, BAR_HIST, BAR_TITLE };

typedef struct Font Font;

typedef struct FlowNode {
    int kind, id, x, y;
    const char *text;
    int links[4];
    int linkCount;
} FlowNode;

typedef struct SlotInfo {
    int year, month, day, hour, minute, second;
    char comment[128];
} SlotInfo;

enum { AN_VOICE_TABS = 9 };
enum { GAME_DDD, GAME_MGSW, GAME_COUNT };

typedef struct UILayout {
    int textSize, nameX, nameRow, textX, textRow, textRowNoName, linePitch, textW;
    int barX, barCount, barGap[3], barAction[11];
    int stripX, stripW, lockX, lockAX, lockAY, lockW, lockH;
} UILayout;

struct AN {
    char *dir, *saveDir, *base;
    int game;
    char page[1024];
    FILE *log;
    char problem[512];
    char title[256];
    int W, H;
    u32 *fb, *prev, *trans;
    u32 serial;
    u32 now;
    u32 lastInput;
    int rate;
    bool quit, paused;

    Script *sc;
    Define def;
    Pack voice, psb;

    struct ImageCache *images;

    Thread th;
    i32 L[AN_VARS], G[AN_VARS];
    char movieName[64];
    char missingSound[64];
    int soundTag;

    int phase;
    Scene scene;
    u32 transStart, transTime;
    int transKind;
    u32 transColor;

    const char *name, *text, *voiceName;
    bool textComplete, textRead;
    u32 textStart;
    LogEntry *logs;
    int logCount, logCap;

    Choice choice[16];
    int choiceCount, choiceHover;
    bool choiceOff[16];
    int choiceResult;

    int screen, logTop, wheel;
    int slotPage;
    int volume[5], voiceVolume[AN_VOICE_TABS], bgvVolume[AN_VOICE_TABS], talkVolume[AN_VOICE_TABS], volumeChar, dragSlider, infoRow;
    int option[11], windowOpacity, textSpeed, autoSpeed;
    u32 sampleStart;
    FlowNode *flow;
    int flowCount, flowScroll, flowZoom;
    bool flowFromScript;
    bool confirmOpen, shortcutsOpen, fractionalText;
    int confirmMsg, confirmAction, confirmArg;
    Image *thumbs[180];
    u32 thumbsSerial[180], saveSerial;
    int mx, my;
    bool down, clicked, rclicked, ctrl;
    int hover;

    bool skipMode, autoMode;

    u8 *saves, *sys;
    size_t sysSize;
    u8 saveKey[16], sysKey[16];
    int lastSlot;
    u32 textPc, opPc;
    int sceneKind;
    u32 *shot;
    struct EmoteCache *emotes;
    struct Pool *pool;
    float emoteScale;
    u8 *stencil;
    int slotX[AN_LAYERS], slotY[AN_LAYERS];
    struct Mixer *mix;
    struct Movie *movie;
    Font *font;
};

bool an_vm_step(AN *a);
void an_vm_start(AN *a, u32 pc);

void an_gfx_render(AN *a);
void an_gfx_snapshot(AN *a);
void an_gfx_begin_transition(AN *a, int kind, u32 ms, u32 color);
bool an_gfx_transition_done(AN *a);
void an_emote_draw(AN *a, int layer, Layer *l, int x, int y, int alpha);
void an_layer_set_var(Layer *l, const char *name, float value);
void an_layer_apply_preset(AN *a, Layer *l, const char *preset, u32 ms);
void an_emote_free(AN *a);
void an_emote_prepare(AN *a);
void an_parallel(AN *a, int jobs, void (*fn)(void *, int), void *ctx);
int an_pool_size(AN *a);
void an_pool_free(AN *a);
void an_window_render(AN *a);
void an_gfx_fill(u32 *dst, int w, int h, u32 color);
void an_gfx_blit(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int alpha);
void an_layer_pos(const AN *a, const Layer *l, int *x, int *y);
int an_layer_scale(const AN *a, const Layer *l);
void an_gfx_blit_fit(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int w, int h);

bool an_text_init(AN *a);
void an_text_free(AN *a);
int an_text_draw(AN *a, int x, int y, const char *s, int size, u32 color, u32 shadow, int maxWidth, int lineHeight);
int an_text_width(AN *a, const char *s, int size);

bool an_snd_init(AN *a);
void an_snd_free(AN *a);
void an_snd_play(AN *a, int channel, const char *name, bool loop, bool fromVoice);
void an_snd_stop(AN *a, int channel);
bool an_snd_playing(AN *a, int channel);
float an_snd_progress(AN *a, int channel);
typedef struct VoiceTabs {
    int count;
    const char *letters;
    u32 voiceAt, bgvAt, talkAt;
} VoiceTabs;
const VoiceTabs *an_voice_tabs(const AN *a);
int an_voice_tab(const AN *a, char letter);
void an_snd_mix(AN *a, float *out, int frames);
void an_snd_lock(AN *a, bool lock);
void an_snd_set_volume(AN *a, int channel, float volume);
float an_channel_volume(AN *a, int channel, const char *name, int tag);

bool an_movie_exists(AN *a, const char *name);
bool an_movie_play(AN *a, const char *name, bool loop, u32 fadeMs);
void an_movie_stop(AN *a);
bool an_movie_looping(AN *a);
bool an_movie_update(AN *a);
void an_movie_draw(AN *a, u32 *dst);
void an_movie_mix(AN *a, float *out, int frames);

enum { CH_BGM = 0, CH_VOICE = 1, CH_SE0 = 2, CH_BGV0 = CH_SE0 + AN_SE, CH_COUNT = CH_BGV0 + 4 };

bool an_slot_info(AN *a, int slot, SlotInfo *info);
bool an_save_slot(AN *a, int slot);
bool an_load_slot(AN *a, int slot);
bool an_delete_slot(AN *a, int slot);
bool an_reset_saves(AN *a);
bool an_quick_save(AN *a);
bool an_quick_load(AN *a);
void an_confirm(AN *a, int message, int action, int arg);
void an_do_action(AN *a, int action, int arg);
void an_slots_open(AN *a, bool saving);
void an_volume_open(AN *a);
void an_volume_defaults(AN *a);
void an_settings_open(AN *a);
void an_settings_defaults(AN *a);
enum { OPT_SCREEN, OPT_EFFECTS, OPT_SKIP_UNREAD, OPT_KEEP_SKIP, OPT_EVENT_MOVIES, OPT_HAIR, OPT_SKIP_VOICE, OPT_HISTORY, OPT_CONFIRM, OPT_VOICE_BAR, OPT_AUTO_BAR };
void an_screens_render(AN *a);
bool an_screens_input(AN *a);
void an_go_title(AN *a);
void an_flow_load(AN *a);
void an_flow_mark(AN *a, int index);
void an_flow_open(AN *a, bool fromScript);
void an_flow_jump(AN *a, int id);
void an_flow_render(AN *a);
bool an_flow_input(AN *a);
void an_capture_shot(AN *a);
void an_system_load(AN *a);
void an_system_save(AN *a);
void an_mark_read(AN *a, int id);
bool an_is_read(AN *a, int id);
void an_reset_scene(AN *a);
bool an_png_write(const char *path, const u32 *px, int w, int h);
void an_ui_render(AN *a);
bool an_ui_input(AN *a);
int an_bar_hit(AN *a);
const UILayout *an_ui(const AN *a);
void an_bar_draw(AN *a, Image *ui);
void an_title_render(AN *a);
void an_title_input(AN *a);
void an_game_input(AN *a);

#endif

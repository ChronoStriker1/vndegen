// Internal declarations shared by the System 3 runtime's source files.
#ifndef S3_H
#define S3_H

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "vns3.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// ---- util.c ----
void *s3_alloc(size_t n);                      // zeroed, aborts on failure
char *s3_strdup(const char *s);
// Reads a whole file. Returns NULL when missing.
u8 *s3_read_file(const char *path, size_t *size);
// Writes a whole file through a temporary beside it, so a failed write leaves the old file as it was.
bool s3_write_file(const char *path, const void *data, size_t size);
// Finds dir/name whatever the case of name, as Windows does. Returns a malloc'd path or NULL.
char *s3_find_file(const char *dir, const char *name);
char *s3_path(const char *dir, const char *name); // joins without checking
// Decodes one Shift-JIS (CP932) character at p. Returns its byte length (1 or 2) and the code
// point in *cp; unmapped bytes give U+FFFD.
int s3_sjis_decode(const u8 *p, u32 *cp);
bool s3_sjis_lead(u8 c);
// CP932 string to UTF-8 (malloc'd).
char *s3_sjis_to_utf8(const char *s);
u16 s3_le16(const u8 *p);
u32 s3_le32(const u8 *p);

// ---- ald.c: AliceSoft link archives (*SA.ALD, *GA.ALD, ...) ----
typedef struct S3Blob { const u8 *data; size_t size; } S3Blob;
typedef struct S3Archive {
    int disks;                 // number of volumes found (A, B, ...)
    u8 *vol[26];               // whole volume files
    size_t volSize[26];
    u32 count;                 // link numbers 1..count (index 0 unused)
    u8 *mapDisk;               // per link number: volume (1-based, 0 = missing)
    u16 *mapIndex;             // per link number: entry within the volume (1-based)
} S3Archive;
// Loads <prefix><kind><A..Z>.ALD from dir. kind is 'S', 'G', 'W', 'M', 'D', 'R'. Returns false
// when no volume exists.
bool s3_archive_open(S3Archive *a, const char *dir, const char *prefix, char kind);
void s3_archive_close(S3Archive *a);
// Entry for link number n (1-based). name receives the stored file name if given.
bool s3_archive_get(const S3Archive *a, u32 n, S3Blob *out, const char **name);

// System 3.0 DAT archives
typedef struct S3Dat {
    u8 *vol[4];
    size_t volSize[4];
    const u8 *map;             // file 1 of the first volume: (volume, index) per link number
    u8 *mapData;
    u32 count;
} S3Dat;
bool s3_dat_open(S3Dat *a, const char *dir, const char *const *names, int count);
bool s3_dat_file(const S3Dat *a, int vol, u32 file, S3Blob *out);
bool s3_dat_get(const S3Dat *a, u32 n, S3Blob *out);
void s3_dat_close(S3Dat *a);

// ---- ain.c: System39.ain ----
typedef struct S3Ain {
    u8 *data;
    u32 varCount;
    char **varNames;
    u32 msgCount;
    const char **msgs;          // CP932, NUL-terminated, point into data
    u32 funcCount;
} S3Ain;
bool s3_ain_open(S3Ain *ain, const char *path);
void s3_ain_close(S3Ain *ain);

// ---- cg.c: CG decoding ----
typedef struct S3Image {
    int x, y, w, h;           // default placement and size
    int bpp;                  // 8 (indexed) or 16
    u8 *pixels8;              // w*h palette indexes when bpp == 8
    u16 *pixels16;            // w*h RGB565 when bpp == 16
    u8 *alpha;                // optional w*h alpha for 16-bit images
    u8 pal[256][3];           // palette (RGB) for indexed images
    u16 palMask;              // PMS: bit n set loads palette entries 16n..16n+15
    int palBank;              // VSP: the 16-entry bank the palette belongs to
    bool vsp;
    bool hasPal;
    int transparent;          // colour key from the file, or -1
} S3Image;
bool s3_cg_decode(const u8 *data, size_t size, S3Image *img);
void s3_image_free(S3Image *img);

// ---- gfx.c: the drawing surface (DIB) and presentation ----
typedef struct S3Surface {
    int w, h, depth;          // depth 8 = indexed, otherwise 24-bit RGB stored as 0x00RRGGBB
    u8 *p8;
    u32 *p32;
} S3Surface;

typedef struct S3 S3;

// ---- the runtime ----
typedef enum {
    S3_WAIT_NONE, S3_WAIT_KEY, S3_WAIT_MENU, S3_WAIT_TIME, S3_WAIT_FADE, S3_WAIT_INPUT_POLL,
    S3_WAIT_EFFECT, S3_WAIT_QUIT,
} S3WaitKind;

typedef enum { S3_FRAME_LABEL, S3_FRAME_PAGE, S3_FRAME_PUSH, S3_FRAME_TEXT } S3FrameKind;
typedef struct S3Frame {
    u8 kind;
    u16 page;                 // return page (LABEL/PAGE)
    u32 pc;                   // return address
    u16 var;                  // pushed variable (PUSH)
    u16 value;                // pushed value (PUSH)
} S3Frame;

typedef struct S3ArrayPage { u16 *v; u32 n; bool save; } S3ArrayPage;
typedef struct S3Bind { u8 page; u16 base; u16 pointer; bool on; } S3Bind;

typedef struct S3Win { int x, y, w, h, save; } S3Win;

typedef struct S3MenuItem { u16 page; u32 addr; char text[256]; int zh; } S3MenuItem; // zh: the ZH mode its text came in

#define S3_MAX_STRINGS 5001
#define S3_MAX_FRAMES 4096
#define S3_MAX_MENU 64

struct S3 {
    VNS3Config cfg;
    char *gameDir, *saveDir;
    char problem[512];
    char title[256];
    FILE *log;
    bool quit, paused;

    // game data
    char prefix[64];           // ALD name prefix, such as "Rance"
    S3Archive sa, ga, wa, ma, da;
    S3Ain ain;
    bool hasAin;
    bool sys30;                // a SYSTEM3.EXE game (vm30.c)
    S3Dat adisk, cg30;
    struct S3State30 *s30;

    // VM state
    u16 *vars;                 // page 0, 65536 entries
    S3ArrayPage arr[256];
    S3Bind *bind;              // per variable
    u16 page;
    const u8 *code;
    u32 codeSize;
    u32 pc, cmdStart;
    bool vmError, forFirst;
    bool trace;                // S3_TRACE set: log every command (Linux test host)
    S3Frame stack[S3_MAX_FRAMES];
    int sp;
    u16 retValue;
    char *str[S3_MAX_STRINGS];
    int strLen;                // maximum string length (MZ)
    int strCount;
    // data tables (# and F commands)
    u16 dataPage; u32 dataPtr;

    // text and windows
    int zh;                    // 0 full width, 1 half width, 2 no conversion
    int fontSize, menuFontSize, lineSpace, fontWeight, decoration, decorationColor;
    int msgColor, menuColor, menuFrame, menuBg, msgFrame, msgBg, hitColor, menuCursor, menuSelColor;
    int msgBgAlpha, menuBgAlpha, menuDefault;
    int palBank;
    S3Win msgWin[128], selWin[128];  // the PC save keeps 128 of each
    u8 *msgUnder[101];      // what B4 covered with a message window whose B3 asked to keep it, for B4 ... ,1
    int curMsg, curSel;
    int msgFrameOn, selFrameOn;
    int tx, ty;                // next text position
    bool msgClearOnSelect;
    int keyMode[8];
    S3MenuItem menu[S3_MAX_MENU];
    int menuCount;
    int menuBuild;             // index of the menu item being built, or -1
    int menuCursorPos;
    u8 *menuSave; int menuSaveX, menuSaveY, menuSaveW, menuSaveH;
    int textStack[16][3]; int textSp; // TPC/TPP/TPS

    // graphics
    S3Surface dib;
    int viewX, viewY, viewW, viewH;
    u8 pal[256][3];            // the palette being drawn with
    u8 shownPal[256][3];       // the palette on screen (fades change it)
    int pcMode;                // PC command flags, default 7
    int pdLevel;               // PD brightness
    int cgX, cgY; bool cgPos; int cgPosMode;
    bool flip;                 // WZ: copy to screen
    u32 *screen; int screenW, screenH;
    u32 serial;
    bool dirty;
    int fadeLevel;             // 0..255 black (or white) amount on presentation
    bool fadeWhite;
    S3Win clr[101]; int clrColor[101];
    int lastCg, noPalCg;
    S3Bind cgCountVar;          // ZG: on, pointer = the array variable

    // input
    int mouseX, mouseY;
    bool mouseL, mouseR, keyDown[256];
    int exprDepth;          // nesting of index expressions being read
    u32 clickLatch;         // buttons pressed since IM last read them (16 left, 32 right), so a quick tap still counts
    // A tap moves the pointer and presses in one go. Games take their hover from the position they last polled, so a
    // press that comes with a move waits until the game has run a frame at the new position, as a mouse moves first.
    bool movedThisFrame;
    u8 buttonQueue[8];      // button << 1 | down, oldest first
    int buttonsQueued;
    u32 pressedMask;           // keys pressed since the last poll, IK bit layout
    int wheel;
    u32 cursorMoveTime;

    // timing
    u64 clockMs;               // runtime clock
    long long startTime;       // wall-clock seconds when the game opened; ZT0 counts from here with clockMs
    u64 timerBase[8];
    int polls;                 // input polls this frame, to yield busy loops

    // waits
    S3WaitKind wait;
    u64 waitUntil;
    int waitArg[8];

    // fonts and audio (opaque here)
    struct S3Font *font;
    struct S3Gaiji *gaiji;     // GAIJI.DAT's characters, for System 3.0 games that ship one
    int gaijiCount;
    struct S3UnitMap *unitMap;
    struct S3Sprites *sprites;
    struct S3Audio *audio;
    u32 rand;
    int cdLoops, cdTrack, midiLoops, midiPlaying;
    bool y3Cancel;
};



void s3_log(S3 *s, const char *fmt, ...);
void s3_fail(S3 *s, const char *fmt, ...);

// vm.c
bool s3_vm_start(S3 *s);
void s3_vm_run(S3 *s, int budget);
u16 s3_var_get(S3 *s, u16 v);
void s3_var_set(S3 *s, u16 v, u16 value);

// gfx.c
void s3_gfx_init(S3 *s, int w, int h, int depth);
void s3_gfx_present(S3 *s);
u32 s3_rgb(S3 *s, int color);                   // palette index or 24-bit value to 0xRRGGBB
void s3_fill(S3 *s, int x, int y, int w, int h, int color);
void s3_copy(S3 *s, int sx, int sy, int w, int h, int dx, int dy);
void s3_copy_key(S3 *s, int sx, int sy, int w, int h, int dx, int dy, int key);
void s3_draw_image(S3 *s, const S3Image *img, int x, int y, int key);
void s3_mark_dirty(S3 *s);
bool s3_clip(S3 *s, int *x, int *y, int *w, int *h);

// text.c
bool s3_font_open(S3 *s, const char *path, int index);
void s3_font_close(S3 *s);
// Draws one character at (x, y) with the cell height size; returns the advance.
int s3_draw_char(S3 *s, u32 cp, bool half, int x, int y, int size, int color);
// Code points from S3_GAIJI on stand for GAIJI.DAT's characters, by their index there.
#define S3_GAIJI 0xf0000u
void s3_gaiji_load(S3 *s);
int s3_gaiji_find(S3 *s, const u8 *sjis);

// msg.c
void s3_msg_text(S3 *s, const char *sjis);
void s3_msg_newline(S3 *s);
void s3_msg_clear(S3 *s);
// CP932 text as the window shows it (hiragana for half-width kana, full-width ASCII under ZH 0), in UTF-8.
char *s3_display_utf8(S3 *s, const char *sjis);
void s3_msg_select_window(S3 *s, int n, int frame, int noClear, int restore);
void s3_menu_open(S3 *s);
bool s3_menu_update(S3 *s);   // true when a choice was made or cancelled
void s3_menu_close(S3 *s, int choice);

// save.c: save files in the PC engine's format
bool s3_save_write(S3 *s, int slot);
bool s3_save_load(S3 *s, int slot);
bool s3_save_copy(S3 *s, int to, int from);
struct S3LV;
bool s3_save_write_part(S3 *s, int slot, const struct S3LV *from, int count);
bool s3_save_load_part(S3 *s, int slot, const struct S3LV *to, int count);
bool s3_save_time(S3 *s, int slot, u16 out[7]);

// snd.c
void s3_audio_init(S3 *s);
void s3_audio_free(S3 *s);
void s3_wav_play(S3 *s, int channel, const u8 *data, size_t size, int loops);
void s3_wav_stop(S3 *s, int channel);
bool s3_wav_playing(S3 *s, int channel);
void s3_music_play(S3 *s, int track, int loops);
void s3_music_stop(S3 *s);
int s3_music_position(S3 *s);
void s3_midi_play(S3 *s, int n, int loops);
void s3_midi_stop(S3 *s);
void s3_audio_pause(S3 *s, bool paused);

#endif

#ifndef SYSTEMAOI_INTERNAL_H
#define SYSTEMAOI_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vnsystemaoi.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef int64_t i64;

typedef struct SA SA;

#define SA_W 1280
#define SA_H 720

void *sa_alloc(size_t n);
void *sa_realloc(void *p, size_t n);
char *sa_strdup(const char *s);
char *sa_fmt(const char *fmt, ...);
u8 *sa_read_file(const char *path, size_t *size);
bool sa_write_file(const char *path, const void *data, size_t size);
bool sa_mkdirs(const char *dir);
int sa_path_resolve(const char *path, char *out, size_t size);
void sa_log(SA *s, const char *fmt, ...);

bool sa_ieq(const char *a, const char *b);
int sa_utf8_put(char *out, u32 cp);
int sa_utf8_decode(const char *s, u32 *cp);
char *sa_utf16_to_utf8(const u8 *p, int chars);

static inline u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static inline u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static inline u16 rd16be(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
static inline u32 rd32be(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | (u32)p[3]; }
static inline void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

typedef struct VfsEntry {
    char *name;
    int arc;
    u32 offset, size;
} VfsEntry;

#define SA_ARCS 48

typedef struct Vfs {
    FILE *f[SA_ARCS];
    char *arcName[SA_ARCS];
    int arcs;
    VfsEntry *e;
    int count, cap;
    int *hash;
    int hashSize;
} Vfs;

bool sa_vfs_open(Vfs *v, const char *dir);
void sa_vfs_close(Vfs *v);
const VfsEntry *sa_vfs_find(const Vfs *v, const char *name);
u8 *sa_vfs_read(Vfs *v, const char *name, size_t *size);

typedef struct BoxEntry {
    char name[32];
    u8 *data;
    u32 size;
} BoxEntry;

typedef struct Box {
    BoxEntry *e;
    int count;
} Box;

bool sa_box_parse(Box *b, const u8 *data, size_t size);
void sa_box_free(Box *b);
u8 sa_box_key(u32 offset);

typedef struct Image {
    int w, h;
    u32 *px;
} Image;

Image *sa_image_new(int w, int h);
void sa_image_free(Image *im);
Image *sa_agf_decode(const u8 *data, size_t size);

void sa_blit(u32 *dst, const Image *im, int x, int y);
void sa_blit_alpha(u32 *dst, const Image *im, int x, int y, u32 alpha);
void sa_blit_rect(u32 *dst, const Image *im, int sx, int sy, int w, int h, int x, int y);
void sa_fill(u32 *dst, int x, int y, int w, int h, u32 rgba);

enum {
    OPD_NONE,
    OPD_INT,
    OPD_BYTE,
    OPD_STR,
    OPD_ADDR,
    OPD_VAR,
    OPD_IVAR,
    OPD_COND,
};

#define SA_MAX_OPDS 24

typedef struct Operand {
    u8 kind;
    u8 bank;
    u16 index;
    i32 value;
    u32 strOffset;
    u16 strLen;
    u8 condKind;
} Operand;

typedef struct Insn {
    u32 at, next;
    u8 op;
    int count;
    Operand o[SA_MAX_OPDS];
} Insn;

bool sa_insn_decode(const u8 *code, u32 size, u32 at, Insn *in);
char *sa_operand_text(const u8 *code, const Operand *o);
char *sa_insn_text(const u8 *code, const Insn *in);

#define SA_C_VARS 4096
#define SA_F_VARS 8192
#define SA_L_VARS 64
#define SA_G_VARS 2048
#define SA_STACK 32
#define SA_CHOICES 16
#define SA_BOXES 64

typedef struct LoadedBox {
    char file[32];
    Box box;
} LoadedBox;

typedef struct Frame {
    int box, entry;
    u32 pc;
    u32 loopHeader;
    bool loopActive;
    u32 ret[SA_STACK];
    int sp;
    i32 cl[SA_L_VARS];
    u8 fl[SA_L_VARS];
} Frame;

typedef struct Choice {
    char *label;
    u32 target;
} Choice;

enum { VM_RUN, VM_WAIT_CLICK, VM_WAIT_CHOICE, VM_WAIT_TIME, VM_DONE };

typedef struct VM {
    LoadedBox boxes[SA_BOXES];
    int boxCount;
    char *boxSlot[256];
    Frame frames[SA_STACK];
    int depth;
    i32 c[SA_C_VARS];
    u8 f[SA_F_VARS];
    u8 g[SA_G_VARS];
    Choice choice[SA_CHOICES];
    int choiceCount;
    int state;
    u32 waitMs;
    u32 unknownSeen[8];
    u32 rng;
} VM;

typedef struct VMHost {
    void *ctx;
    void (*text)(void *ctx, const char *utf8, bool newLine);
    void (*speaker)(void *ctx, const char *name, const char *voice, const char *face);
    void (*page)(void *ctx);
    void (*command)(void *ctx, const u8 *code, const Insn *in, VM *vm);
    void (*log)(void *ctx, const char *msg);
} VMHost;

bool sa_vm_start(VM *vm, Vfs *vfs, const char *boxFile, int entry, const VMHost *host);
int sa_vm_run(VM *vm, Vfs *vfs, const VMHost *host, int budget);
void sa_vm_choose(VM *vm, int index);
void sa_vm_click(VM *vm);
void sa_vm_free(VM *vm);
i32 sa_vm_get(VM *vm, const u8 *code, const Operand *o);
void sa_vm_set(VM *vm, const Operand *o, i32 v);
bool sa_vm_cond(VM *vm, const char *expr);
u32 sa_vm_random(VM *vm);

typedef struct PvmVertex {
    float pos[3], normal[3], uv[2];
} PvmVertex;

typedef struct PvmLayer {
    char name[48];
    int parent;
    int sw;
    u32 diffuse, ambient, specular, emissive;
    float power;
    char texture[48];
    int blend;
    float translate[3];
    int range[3];
    bool hasTranslate, hasRange;
    PvmVertex *v;
    int vcount;
    u16 *tri;
    int tcount;
} PvmLayer;

typedef struct Pvm {
    PvmLayer *layer;
    int count;
    bool backFace;
} Pvm;

bool sa_pvm_parse(Pvm *m, const u8 *data, size_t size);
void sa_pvm_free(Pvm *m);
int sa_pvm_find(const Pvm *m, const char *name);

typedef struct Mixer Mixer;
enum { CH_BGM, CH_SE, CH_VOICE, CH_COUNT };
Mixer *sa_mixer_new(int rate, Vfs *vfs);
void sa_mixer_free(Mixer *m);
void sa_mixer_mix(Mixer *m, float *out, int frames, bool paused);
void sa_mixer_set_log(Mixer *m, void (*log)(void *ctx, const char *msg), void *ctx);
void sa_play(Mixer *m, int ch, int slot, const char *name, bool loop);
void sa_play_loop(Mixer *m, int ch, int slot, const char *name, double loopTo, double loopFrom);
void sa_stop(Mixer *m, int ch, int slot, int fadeMs);
void sa_set_volume(Mixer *m, int ch, float v);
bool sa_playing(Mixer *m, int ch, int slot);

bool sa_font_init(const char *const *names, char *used, size_t usedSize);
void sa_font_free(void);
void sa_font_set_spacing(int px);
int sa_char_width(u32 cp, int size);
int sa_text_width(const char *s, int n, int size);
int sa_text_draw(u32 *dst, int dw, int dh, const char *s, int n, int x, int y, int size, u32 color, u32 edge);

#define LOG_ENTRIES 300

typedef struct LogEntry {
    char *name;
    char voice[40];
    char **lines;
    int lineCount;
} LogEntry;

enum { LOG_ART_PANEL, LOG_ART_BACK, LOG_ART_PLAY, LOG_ART_TRACK, LOG_ART_THUMB, LOG_ART_UP, LOG_ART_DOWN, LOG_ART_FAST, LOG_ART_AUTO, LOG_ART_COUNT };
enum { LOG_ACT_NONE, LOG_ACT_CLOSE, LOG_ACT_VOICE, LOG_ACT_FAST, LOG_ACT_AUTO };

typedef struct Backlog {
    LogEntry e[LOG_ENTRIES];
    int count, first;
    bool open;
    int scroll;
    int hover;
    Image *art[LOG_ART_COUNT];
} Backlog;

void sa_log_init(Backlog *b, Vfs *vfs);
void sa_log_free(Backlog *b);
void sa_log_add(Backlog *b, const char *name, const char *voice, char **lines, int lineCount);
void sa_log_open(Backlog *b);
void sa_log_scroll(Backlog *b, int lines);
int sa_log_click(Backlog *b, int x, int y, char *voice, size_t voiceSize);
bool sa_log_hover(Backlog *b, int x, int y);
void sa_log_draw(Backlog *b, u32 *fb, bool fast, bool autoMode);

typedef struct Table {
    int rows, cols;
    i32 *cell;
    u8 *isText;
    char **text;
} Table;

bool sa_table_load(Table *t, Vfs *vfs, const char *name);
void sa_table_free(Table *t);
i32 sa_table_int(const Table *t, int row, int col);
const char *sa_table_text(const Table *t, int row, int col);

#define TOWN_W 10
#define TOWN_H 9
#define TOWN_CELLS (TOWN_W * TOWN_H)

enum {
    TOWN_NONE,
    TOWN_CMD_EXPEDITION,
    TOWN_CMD_POLICY,
    TOWN_CMD_REST,
    TOWN_CMD_BUILD,
    TOWN_CMD_DEPLOY,
    TOWN_CMD_EQUIP,
    TOWN_CMD_TRAIN,
    TOWN_CMD_SHOP,
    TOWN_CMD_SUBMIT,
    TOWN_BTN_LOG,
    TOWN_BTN_SAVE,
    TOWN_BTN_LOAD,
    TOWN_BTN_OPTION,
    TOWN_BTN_HIDE,
    TOWN_ACTIONS,
};

typedef struct Facility {
    int type;
    int level;
    int exp;
} Facility;

typedef struct Town {
    Table build, mass, lvup;
    Facility cell[TOWN_CELLS];
    int stage;
    Image *bg[8];
    Image **tile;
    Image *ui[40];
    int hover;
    bool hidden, mapOnly;
} Town;

bool sa_town_init(Town *t, Vfs *vfs);
void sa_town_free(Town *t);
void sa_town_new_game(Town *t);
int sa_town_type_row(const Town *t, int id);
int sa_town_staffed(const Town *t, const VM *vm, int cell);
void sa_town_draw(Town *t, Vfs *vfs, const VM *vm, u32 *fb);
int sa_town_hit(const Town *t, const VM *vm, int x, int y);
bool sa_town_hover(Town *t, const VM *vm, int x, int y);
void sa_town_draw_info(Town *t, const VM *vm, u32 *fb);
void sa_draw_number(u32 *fb, const Image *font, const char *s, int right, int y, int row, int cw, int ch);
void sa_draw_int(u32 *fb, const Image *font, int v, bool sign, int right, int y, int row, int cw, int ch);
Image *sa_load_image(Vfs *vfs, const char *name);

#define BUILD_PENDING 32

typedef struct BuildScreen {
    bool open;
    int held;
    int listHover, cellHover, buttonHover;
    int pendingCell[BUILD_PENDING], pendingType[BUILD_PENDING];
    int pendingCount;
    Image *art[16];
    Image *icon[40];
} BuildScreen;

enum { BUILD_STAY, BUILD_CLOSED };

void sa_build_init(BuildScreen *b, Vfs *vfs);
void sa_build_free(BuildScreen *b);
void sa_build_open(BuildScreen *b);
int sa_build_click(BuildScreen *b, Town *t, VM *vm, int x, int y, bool right);
bool sa_build_hover(BuildScreen *b, Town *t, VM *vm, int x, int y);
void sa_build_draw(BuildScreen *b, Town *t, Vfs *vfs, VM *vm, u32 *fb);

enum { C_TURN = 0x3c, C_RESIDENTS = 0xc8, C_FOOD = 0xc9, C_MONEY = 0xca, C_RESOURCES = 0xcb, C_RANK_MAX = 0x28 };

#endif

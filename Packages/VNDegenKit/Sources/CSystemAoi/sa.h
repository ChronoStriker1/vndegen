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

#endif

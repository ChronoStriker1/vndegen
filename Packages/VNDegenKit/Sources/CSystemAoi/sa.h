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

#endif

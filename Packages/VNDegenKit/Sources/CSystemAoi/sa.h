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
Image *sa_image_scale(const Image *im, int w, int h);
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

enum { VM_RUN, VM_WAIT_CLICK, VM_WAIT_CHOICE, VM_WAIT_TIME, VM_WAIT_DIALOG, VM_DONE };

typedef struct VM {
    LoadedBox boxes[SA_BOXES];
    int boxCount;
    char *boxSlot[256];
    Frame frames[SA_STACK];
    int depth;
    i32 c[SA_C_VARS];
    u8 f[SA_F_VARS];
    u8 g[SA_G_VARS];
    bool gDirty;
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
void sa_vm_register_hooks(VM *vm, Vfs *vfs, const char *boxFile);
char *sa_vm_expand(VM *vm, const char *text);

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

typedef struct R3Vert {
    float x, y, z, u, v, shade;
} R3Vert;

typedef struct R3 {
    int w, h;
    float *zbuf;
    u32 *fb;
    float focal, fogDensity;
    u32 fogColor;
    int cull;
    float blend;
} R3;

void r3_init(R3 *r, int w, int h);
void r3_free(R3 *r);
void r3_begin(R3 *r, u32 *fb, float fovDeg);
void r3_tri(R3 *r, const R3Vert *v, const Image *tex);
Image *sa_bmp_decode(const u8 *d, size_t n);

#define DUN_SIDE 20
#define DUN_CELLS (DUN_SIDE * DUN_SIDE)
#define BATTLE_CELLS 15
#define DUN_FIELDS 26

enum { DIR_N, DIR_E, DIR_S, DIR_W };
enum { PASS_EAST = 1, PASS_SOUTH = 2 };
#define DUN_TEX 192

struct Party;

enum { DUN_ART_PANEL, DUN_ART_REST, DUN_ART_PAD0, DUN_ART_COMPASS = DUN_ART_PAD0 + 6, DUN_ART_NEEDLE, DUN_ART_HOME, DUN_ART_DOWN, DUN_ART_UP, DUN_ART_OPEN, DUN_ART_DIGITS, DUN_ART_FOLD, DUN_ART_PLATFORM, DUN_ART };
enum { DUN_ICON_DOOR, DUN_ICON_CHEST, DUN_ICON_GROUP, DUN_ICON_PARTY, DUN_ICONS };
#define DUN_MARKS 8
#define DUN_WALKS 6
#define DUN_LINKS 48
#define DUN_STATUS 4
#define DUN_TRAP_ICONS 20
enum { DUN_ACT_NONE, DUN_ACT_HOME, DUN_ACT_DOWN, DUN_ACT_UP, DUN_ACT_OPEN };

typedef struct PartSet {
    Pvm pvm;
    int *last, *layerTex;
} PartSet;

typedef struct DunLink {
    int kind, x, y, target, tx, ty, dir;
} DunLink;

#define DUN_ROUTES 8
#define DUN_ROUTE_POINTS 12
#define DUN_CRYSTALS 8

typedef struct DunRoute {
    int count;
    int point[DUN_ROUTE_POINTS][4];
} DunRoute;

typedef struct DunStatus {
    char label[24];
    int steps;
} DunStatus;

typedef struct Dungeon {
    PartSet set, common;
    Image *tex[DUN_TEX];
    char texName[DUN_TEX][48];
    int texCount;
    i32 cell[DUN_CELLS][DUN_FIELDS];
    bool own[DUN_CELLS], *visited;
    u8 ownPassage[DUN_CELLS], *passage;
    bool loaded;
    int w, h;
    float ambient;
    int x, y, dir;
    float fov, camX, camZ, yaw;
    float fromX, fromZ, fromYaw, toX, toZ, toYaw;
    u32 animMs, animDur, clock;
    int hover, steps;
    bool bumped;
    Image *art[DUN_ART];
    Image *needle[4];
    Image *icon[DUN_ICONS], *arrow[4], *big[48], *walk[DUN_WALKS];
    const Image *bigFrom[48];
    int bigCount;
    int markCount, markX[DUN_MARKS], markY[DUN_MARKS];
    DunLink link[DUN_LINKS];
    int linkCount;
    DunRoute route[DUN_ROUTES];
    int routeCount;
    int crystal[DUN_CRYSTALS][2];
    int crystalCount;
    bool seen[DUN_CELLS], blind;
    Image *trapIcon[DUN_TRAP_ICONS], *plates;
    DunStatus status[DUN_STATUS];
    int statusCount;
    R3 r3;
    int fogTex;
    float fogSpeed[2], fogAlpha[2], fogHeight;
    int passCell, passSide;
} Dungeon;

enum { DUN_STAY, DUN_REST, DUN_MOVED, DUN_ACTION, DUN_TURNED };

bool sa_dungeon_load(Dungeon *d, Vfs *vfs, const char *set, const char *map);
void sa_dungeon_free(Dungeon *d);
void sa_dungeon_art(Dungeon *d, Vfs *vfs, const char *platform);
void sa_dungeon_place(Dungeon *d, int x, int y, int dir);
bool sa_dungeon_find_entrance(const Dungeon *d, int *x, int *y, int *dir);
bool sa_dungeon_tick(Dungeon *d, u32 ms);
bool sa_dungeon_hover(Dungeon *d, int x, int y);
int sa_dungeon_click(Dungeon *d, int x, int y);
int sa_dungeon_key(Dungeon *d, int key);
void sa_dungeon_draw(Dungeon *d, u32 *fb);
void sa_dungeon_draw_map(Dungeon *d, u32 *fb, int x, int y, int scale);
const Image *sa_dungeon_platform(const Dungeon *d);
enum { DUN_LINK_STAIRS = 1, DUN_LINK_TELEPORT = 2 };
bool sa_dungeon_link(const Dungeon *d, int kind, DunLink *out);
void sa_dungeon_reveal(Dungeon *d);
bool sa_dungeon_in_dark(const Dungeon *d);
void sa_dungeon_draw_frame(Dungeon *d, u32 *fb, struct Party *party, int rankLimit);
void sa_dungeon_draw_hud(Dungeon *d, u32 *fb, struct Party *party, int rankLimit);
int sa_dungeon_get(const Dungeon *d, const char *name, int x, int y, int dir);
void sa_dungeon_set(Dungeon *d, const char *name, int value, int x, int y, int mode, int dir);
bool sa_dungeon_read_map(Dungeon *d, Vfs *vfs, const char *map);
void sa_dungeon_open_doors(Dungeon *d);
bool sa_dungeon_find_mark(const Dungeon *d, int mark, int *x, int *y);
void sa_dungeon_sense(bool on);
int sa_dungeon_neighbour(const Dungeon *d, int x, int y, int side);
int sa_dungeon_group_step(const Dungeon *d, int x, int y, int side);
bool sa_dungeon_cell_is_passage(const Dungeon *d, int x, int y);
int sa_dungeon_passage_side(const Dungeon *d);
bool sa_dungeon_passage_cell(const Dungeon *d, int side, int along, int *x, int *y);
bool sa_dungeon_has_passage(const Dungeon *d, int side, int along);
bool sa_dungeon_arrive(Dungeon *d, int side, int along);
int sa_dungeon_action(const Dungeon *d);
int sa_dungeon_object(const Dungeon *d);
int sa_dungeon_crystal_index(const Dungeon *d);
void sa_dungeon_clear_object(Dungeon *d);
void sa_debug_dungeon(VNSystemAoi *sa, const char *map, int x, int y, int dir);

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
void sa_font_set_plain(bool on);
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
    TOWN_POLICY_DONE,
    TOWN_BTN_LOG,
    TOWN_BTN_SAVE,
    TOWN_BTN_LOAD,
    TOWN_BTN_OPTION,
    TOWN_BTN_HIDE,
    TOWN_CMD_MAZE,
    TOWN_TAB0,
    TOWN_WINBTN0 = TOWN_TAB0 + 6,
    TOWN_SECTION0 = TOWN_WINBTN0 + 6 * 3,
    TOWN_ACTIONS = TOWN_SECTION0 + 6 * 4,
};

enum { TW_FAC1, TW_FAC2, TW_INFO, TW_MAZE, TW_ROLES, TW_MAZE_ROLES, TOWN_WINDOWS };
enum { TOWN_SECTIONS = 4, TOWN_UI = 96 };

typedef struct Maze {
    int floors;
    int guards, passages, traps, chests;
    int marks[4];
    int food, money, resources;
    int fame, intruders;
    int depthPoints, renown;
    int townGuard, townTrap, townChest;
    int event[7][7][6];
} Maze;

typedef struct Facility {
    int type;
    int level;
    int exp;
    int built;
} Facility;

enum { ORDER_FREE, ORDER_ATTACK, ORDER_SUPPORT, ORDER_DEFEND, ORDER_SWAP, ORDER_INTRUDE, ORDER_FLEE, ORDER_SKILL };
enum {
    AIM_FREE,
    AIM_ALL,
    AIM_ENEMY,
    AIM_ALLY,
    AIM_FRONT,
    AIM_MIDDLE,
    AIM_BACK,
    AIM_RIGHT,
    AIM_LEFT,
    AIM_CENTRE,
    AIM_RIGHTMOST,
    AIM_LEFTMOST,
    AIM_COUNT
};

#define SPRITE_FRAMES 32

typedef struct SpriteFrame {
    int x, y, w, h;
} SpriteFrame;

typedef struct Order {
    int mode, skill, aim;
} Order;

#define PARTY_SLOTS 6
#define PARTY_PRESETS 3
#define PARTY_GOVERN_MAX 10
#define PARTY_ROSTER 64
#define PARTY_ITEMS 256
#define PARTY_FRONT 3
#define PARTY_CELL_MS 100

typedef struct Bench {
    int unit, hp, mp, level, exp, equip, sp, trained[10];
    int tile;
} Bench;

#define PARTY_RESERVE 5
#define PARTY_POSTS 30

#define PARTY_BENCH 16

typedef struct Party {
    Table units, abilities;
    Bench bench[PARTY_BENCH];
    int benchCount;
    int unit[PARTY_SLOTS];
    int hp[PARTY_SLOTS], mp[PARTY_SLOTS];
    int level[PARTY_SLOTS], exp[PARTY_SLOTS];
    Order order[PARTY_SLOTS], preset[PARTY_PRESETS][PARTY_SLOTS];
    int itemId[PARTY_ITEMS], itemCount[PARTY_ITEMS], items;
    int equip[PARTY_SLOTS];
    int sp[PARTY_SLOTS], trained[PARTY_SLOTS][10];
    int roster[PARTY_ROSTER], rosterCount;
    bool presetSaved[PARTY_PRESETS];
    Image *sprite[PARTY_SLOTS];
    int cells[PARTY_SLOTS];
    u32 ms;
    Image *platform, *tile, *smallTile, *gaugeFrame, *hpGauge, *mpGauge, *label, *digits;
    Vfs *vfs;
    const Table *lvup;
    Table positions;
    int post[PARTY_POSTS];
    Image *reserveSprite[PARTY_RESERVE];
    int reserveUnit[PARTY_RESERVE];
    Image *benchSprite[PARTY_BENCH], *sgpLabel;
    int benchSpriteUnit[PARTY_BENCH];
    int sgp;
} Party;

typedef struct Member {
    int unit, slot, bench;
    int *hp, *mp, *level, *exp, *equip, *sp, *trained;
} Member;

int sa_party_member_list(Party *p, Member *out);
void sa_party_draw_board(Party *p, u32 *fb, const Image *platform, int ox, int oy, const int *frames);
int sa_party_member_value(const Party *p, const Member *m, int index);
const Image *sa_party_member_sprite(Party *p, const Member *m);
int sa_party_member_badge(const Party *p, const Member *m);
enum { BADGE_FIELD = 98, BADGE_RESERVE = 99 };

bool sa_party_init(Party *p, Vfs *vfs);
void sa_party_free(Party *p);
void sa_party_new_game(Party *p, Vfs *vfs);
void sa_party_place(Party *p, Vfs *vfs, int slot, int id);
int sa_party_unit_row(const Party *p, int id);
bool sa_party_unit_knows(const Party *p, int row, int level, int k);
int sa_party_rank(const Party *p);
void sa_party_add_item(Party *p, int id, int n);
int sa_party_item_count(const Party *p, int id);
int sa_party_count_units(const Party *p, const char *key);
bool sa_party_join(Party *p, Vfs *vfs, const char *key);
void sa_party_swap(Party *p, int a, int b);
bool sa_party_bench(Party *p, int slot);
void sa_party_set_reserve(Party *p, int index, int tile);
int sa_party_reserve_at(const Party *p, int tile);
int sa_party_post_of(const Party *p, int unit);
int sa_party_post_icon(const Party *p, int unit, int post);
bool sa_party_post_stats(const Party *p, int unit, int post, int *gov, int *lead);
int sa_party_govern_cap(int v);
int sa_party_post_effect(const Party *p, int unit, int post);
int sa_party_post_index(const Party *p, const char *name);
void sa_party_set_post(Party *p, int post, int unit);
void sa_party_unit_stats(const Party *p, int row, int level, int *out);
void sa_party_unbench(Party *p, Vfs *vfs, int index, int slot);
int sa_party_members(const Party *p);
int sa_party_max_hp(const Party *p, int slot);
int sa_party_max_mp(const Party *p, int slot);
int sa_party_stat(const Party *p, int slot, int stat);
bool sa_party_tick(Party *p, u32 ms);
void sa_party_draw_diorama(Party *p, u32 *fb, int rankLimit, const Image *platform);
void sa_party_tile_center(int slot, int boardX, int boardY, int *x, int *y);
void sa_party_draw_tile(Party *p, u32 *fb, int cx, int cy, int frame);
void sa_party_draw_tiles(Party *p, u32 *fb, int boardX, int boardY, const int *frames);
void sa_party_draw_bars(Party *p, u32 *fb, int x, int y, int hp, int maxHp, int mp, int maxMp);
void sa_sprite_layout(const Image *im, int *cols, int *rows);
int sa_sprite_filled_cells(const Image *im);
int sa_sprite_split(const Image *im, SpriteFrame *out, int max);

enum { STAT_ATTACK, STAT_MAGIC, STAT_DEFENCE, STAT_RESIST, STAT_SPEED, STAT_EVASION, STAT_GOVERN, STAT_COMMAND };
enum { BATTLE_RUNNING, BATTLE_WON, BATTLE_LOST, BATTLE_FLED };
enum { PICK_NONE, PICK_ACTION, PICK_TARGET };

#define BATTLE_FIGHTERS 24
#define BATTLE_LOG 64
#define BATTLE_ART 64
#define BATTLE_PICKS 16
#define POP_GREEN "\x01"
#define POP_YELLOW "\x02"
#define POP_WHITE "\x03"

typedef struct Fighter {
    int id, row, level, slot, gridRow, gridCell;
    bool enemy, guard;
    int hp, mp, maxHp, maxMp;
    int stat[8];
    Image *sprite;
    SpriteFrame frame[SPRITE_FRAMES];
    int frames;
} Fighter;

typedef struct BattleLine {
    char *text;
    bool enemy, indent;
} BattleLine;

typedef struct Battle {
    bool open, acting, autoBattle;
    int result, round, hover, group, boardSize, boardSet, gaugeMode;
    char dungeonName[48], floorName[48];
    Party *party;
    Table groups, skills, lvup, items;
    Fighter f[BATTLE_FIGHTERS];
    int count;
    int order[BATTLE_FIGHTERS], orderCount, next;
    BattleLine log[BATTLE_LOG];
    int logCount;
    int picker, pickSlot, pickCount;
    Order pick[BATTLE_PICKS];
    int gainResources, gainExp;
    char gainItem[64];
    Image *art[BATTLE_ART], *board[4], *back, *pop[4];
    u32 ms, animMs, rng;
} Battle;

bool sa_battle_init(Battle *b, Vfs *vfs, Party *party);
void sa_battle_free(Battle *b);
bool sa_battle_start(Battle *b, Vfs *vfs, const char *group, const char *backdrop, u32 seed);
bool sa_battle_begin(Battle *b, Vfs *vfs, const char *group, const int *cells, Image *backdrop, int boardSet, u32 seed);
bool sa_battle_roll(Battle *b, const char *group, u32 seed, int *cells);
int sa_battle_board_size(const Battle *b, int party);
void sa_battle_unit_stats(const Battle *b, int row, int level, int *out);
void sa_battle_position_bonus(const Battle *b, int n, const int *unitRow, const int *gridRow, const int *gridCol, int (*delta)[10]);
void sa_battle_set_names(Battle *b, const char *dungeon, const char *floor);
void sa_battle_shade(u32 *px, int n);
bool sa_battle_tick(Battle *b, u32 ms);
int sa_battle_click(Battle *b, int x, int y);
bool sa_battle_hover(Battle *b, int x, int y);
void sa_battle_cancel(Battle *b);
void sa_battle_draw(Battle *b, u32 *fb);
void sa_debug_battle(VNSystemAoi *sa, const char *group, const char *backdrop);
void sa_debug_new_game(VNSystemAoi *sa);
void sa_debug_expedition(VNSystemAoi *sa, int dungeon, int floor);
void sa_debug_place(VNSystemAoi *sa, int x, int y, int dir);
void sa_debug_group(VNSystemAoi *sa, const char *name, int x, int y);
void sa_debug_battle_end(VNSystemAoi *sa, int result);
void sa_debug_calm(VNSystemAoi *sa, bool on);
double sa_debug_bench(VNSystemAoi *sa, int n);
void sa_debug_maze(VNSystemAoi *sa, int turns);
void sa_debug_set(VNSystemAoi *sa, bool flag, int index, int value);
int sa_debug_get(VNSystemAoi *sa, bool flag, int index);
void sa_debug_join(VNSystemAoi *sa, const char *key);
void sa_debug_unbench(VNSystemAoi *sa, int index, int slot);
void sa_debug_save(VNSystemAoi *sa, int slot, bool load);
void sa_debug_pcload(VNSystemAoi *sa, const char *path);
void sa_debug_pcsave(VNSystemAoi *sa, const char *path);
void sa_debug_finds(VNSystemAoi *sa, int *out);
void sa_debug_item(VNSystemAoi *sa, int id, int n);
int sa_battle_exp_needed(const Battle *b, int unitRow, int level);
int sa_item_pick(const Table *items, const char *table, u32 roll);

void sa_popup_load(Image **frame, Vfs *vfs);
void sa_dim(u32 *fb);
void sa_popup_draw(Image *const *frame, u32 *fb, const char *const *lines, int count);
void sa_popup_box(Image *const *frame, u32 *fb, const char *const *lines, int count, int x, int y, int w, int h);

typedef struct Town {
    Table build, mass, lvup;
    Facility cell[TOWN_CELLS];
    int stage;
    int condition;
    Party *party;
    Image *bg[8];
    Image **tile;
    Image *ui[TOWN_UI];
    int hover;
    bool hidden, mapOnly;
    bool shut[TOWN_WINDOWS], folded[TOWN_WINDOWS], narrow[TOWN_WINDOWS], sectionFolded[TOWN_WINDOWS][TOWN_SECTIONS];
    const Table *missions;
    const Maze *maze;
    Vfs *vfs;
    Image *burst;
    int burstCell[TOWN_CELLS], burstCount;
    u32 burstMs;
} Town;

#define SA_TOWN_BURST_MS 1000

bool sa_town_init(Town *t, Vfs *vfs);
void sa_town_free(Town *t);
void sa_town_burst(Town *t, VM *vm, int count);
bool sa_town_burst_tick(Town *t, u32 elapsedMs);
void sa_town_new_game(Town *t);
int sa_town_type_row(const Town *t, int id);
int sa_town_staffed(const Town *t, const VM *vm, int cell);
int sa_town_value(const Town *t, int r, int level, int col);
void sa_town_add_exp(Town *t, int cell, int amount);
void sa_town_grow_all(Town *t, const VM *vm, int amount);
void sa_town_level_up(Town *t);
void sa_town_draw_map(Town *t, Vfs *vfs, const VM *vm, u32 *fb);
void sa_town_draw(Town *t, Vfs *vfs, const VM *vm, u32 *fb);
int sa_town_hit(const Town *t, const VM *vm, int x, int y);
bool sa_town_hover(Town *t, const VM *vm, int x, int y);
bool sa_town_toggle(Town *t, int action);
int sa_town_role_window_hit(const Town *t, const VM *vm, int x, int y);
void sa_town_draw_role_window(Town *t, const VM *vm, u32 *fb);
bool sa_maze_post_open(const Maze *m, const VM *vm, int post);
void sa_town_draw_info(Town *t, const VM *vm, u32 *fb);
void sa_town_draw_status(Town *t, const VM *vm, u32 *fb);
void sa_town_draw_status_goal(Town *t, const VM *vm, u32 *fb);
void sa_draw_number(u32 *fb, const Image *font, const char *s, int right, int y, int row, int cw, int ch);
void sa_draw_int(u32 *fb, const Image *font, int v, bool sign, int right, int y, int row, int cw, int ch);
Image *sa_load_image(Vfs *vfs, const char *name);

#define DIALOG_ART 7
#define DIALOG_UNITS 4
#define DIALOG_LINES 8

typedef struct Dialog {
    bool open, table, hover;
    char *lines[DIALOG_LINES];
    int lineCount;
    int before[4], after[4];
    u32 ms;
    Image *art[DIALOG_ART];
    int unitCount;
    Image *unitSprite[DIALOG_UNITS];
    char unitName[DIALOG_UNITS][48];
} Dialog;

void sa_dialog_init(Dialog *d, Vfs *vfs);
void sa_dialog_free(Dialog *d);
void sa_dialog_open(Dialog *d, const char *text);
void sa_dialog_open_turn(Dialog *d, const int before[4], const int after[4], const char *text);
void sa_dialog_add_unit(Dialog *d, Image *sprite, const char *name);
bool sa_dialog_tick(Dialog *d, u32 ms);
bool sa_dialog_hover(Dialog *d, int x, int y);
bool sa_dialog_click(Dialog *d, int x, int y);
void sa_dialog_draw(Dialog *d, u32 *fb);

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

enum { BUILD_STAY, BUILD_CLOSED, BUILD_DECIDED };

void sa_build_init(BuildScreen *b, Vfs *vfs);
void sa_build_free(BuildScreen *b);
void sa_build_open(BuildScreen *b);
int sa_build_click(BuildScreen *b, Town *t, VM *vm, int x, int y, bool right);
bool sa_build_hover(BuildScreen *b, Town *t, VM *vm, int x, int y);
void sa_build_draw(BuildScreen *b, Town *t, Vfs *vfs, VM *vm, u32 *fb);

enum { F_MAZE = 38, F_POSTS = 44 };
enum { C_MAZE_FLOORS = 240, C_MAZE_OPENED = 248, C_MAZE_INTRUDERS = 249, C_MAZE_WIDTH = 251, C_MAZE_SIZE = 261, C_MAZE_FOOD = 271, C_MAZE_REPELLED = 278, C_MAZE_THROUGH = 279 };
enum { C_TURN = 0x3c, C_RESIDENTS = 0xc8, C_FOOD = 0xc9, C_MONEY = 0xca, C_RESOURCES = 0xcb, C_RANK_MAX = 0x28, C_SGP = 42, C_FINDS = 0xd2, C_TOWN_STAGE = 490, C_PLACE_STEPS = 0x48, C_MISSION_ID = 55, C_MISSION_DUE = 57 };

#endif

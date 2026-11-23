#ifndef SA_PRIVATE_H
#define SA_PRIVATE_H

#include "sa.h"
#include <pthread.h>

#define STD_SLOTS 9
#define EXP_HOOKS 8
#define EXP_DUNGEONS 12
#define EXP_FLOORS 24

#define POLICY_CATEGORIES 3
#define POLICY_ROWS 16

typedef struct Policy {
    Table events;
    Image *bg, *art[13], **pictures;
    int category, hover, pick;
} Policy;

typedef struct Expedition {
    bool active;
    int dungeon, floor, row;
    int stepsLeft;
    int hooks[EXP_HOOKS], hookCount, after;
    Table floors, dungeons;
    Image *hall, *plate[12], *areaPlate, *back, *badge, *seen, *platform;
    int hover, picked;
    bool (*explored)[EXP_FLOORS][DUN_CELLS];
} Expedition;
#define MSG_LINES 4

enum { UI_CLOSE, UI_LEAF, UI_SKIP, UI_LOG, UI_REPEAT, UI_OPTION, UI_AUTO, UI_FEATHER, UI_CHOICE_TOP, UI_CHOICE_ROW, UI_CHOICE_BOTTOM, UI_COUNT };
enum { MODE_TITLE, MODE_STORY, MODE_TOWN, MODE_BATTLE, MODE_DUNGEON, MODE_SELECT, MODE_POLICY };
enum { TITLE_PROLOGUE, TITLE_START, TITLE_LOAD, TITLE_OPTION, TITLE_EXTRA, TITLE_END, TITLE_ITEMS };

typedef struct BgmDef {
    int id;
    char file[40];
    double loopTo, loopFrom;
} BgmDef;

typedef struct Picture {
    char name[40];
    Image *img;
    int xslot, yslot;
} Picture;

struct SA {
    VNSystemAoiConfig cfg;
    char *gameDir, *saveDir;
    FILE *logFile;
    Vfs vfs;
    VM vm;
    VMHost host;
    Mixer *mix;
    u32 *fb;
    u32 serial;
    bool dirty, quit, paused;
    char title[128];
    char problem[256];
    Picture bg;
    int bgX, bgY, panFromX, panFromY, panToX, panToY;
    u32 panMs, panDur;
    bool panWait;
    Picture std[STD_SLOTS];
    Image *msgFrame;
    Image *ui[UI_COUNT];
    int hoverButton;
    bool autoMode, skipMode, windowHidden;
    u32 featherMs, autoMs;
    bool forceSkip, rightClicked;
    int mode, titleHover;
    VNSystemAoiMovies movies;
    bool haveMovies;
    pthread_mutex_t movieLock;
    void *movie;
    FILE *movieFile;
    double movieTime;
    int movieW, movieH;
    char *faceName[64], *facePrefix[64];
    int faceCount;
    char faceExpr[64][4];
    Picture face;
    char nextBox[32];
    int nextEntry;
    Image *titleBg, *titleItem[TITLE_ITEMS];
    char lastVoice[40];
    char pageVoice[40];
    Backlog log;
    Town town;
    Party party;
    Battle battle;
    BuildScreen build;
    Dialog dialog;
    Dungeon dungeon;
    Expedition exp;
    Policy policy;
    bool turnPending;
    bool inGame;
    int afterScript;
    char name[96];
    char *lines[MSG_LINES];
    int lineCount;
    int shown, total;
    u32 textMs;
    char voice[32];
    int mouseX, mouseY;
    bool clicked;
    int hover;
    u32 waitMs;
    bool waitClick;
    BgmDef *bgm;
    int bgmCount;
    Image *msgBand;
    u32 *from;
    u32 transMs, transDur;
};

bool sa_start_script(SA *s, const char *box, int entry);
void sa_policy_init(Policy *p, Vfs *vfs);
void sa_policy_free(Policy *p);
void sa_policy_open(SA *s);
bool sa_policy_frame(SA *s);
void sa_exp_init(SA *s);
void sa_exp_free(SA *s);
void sa_exp_open_select(SA *s);
bool sa_exp_frame(SA *s, u32 ms);
bool sa_exp_script_done(SA *s);
void sa_exp_key(SA *s, int key);
void sa_exp_battle_over(SA *s, int result);
void sa_exp_gain(SA *s, const int gain[4]);
void sa_exp_enter(SA *s, int dungeon, int floor);
void sa_exp_force_battle(SA *s);
int sa_exp_floors_reached(const SA *s, int dungeon);
void sa_exp_draw_view(SA *s, u32 *fb);
void sa_exp_debug_place(SA *s, int x, int y, int dir);
void sa_reset_scene(SA *s);
void sa_end_turn(SA *s);
void sa_show_frame_as_bg(SA *s, const u32 *px, const char *name);

#endif

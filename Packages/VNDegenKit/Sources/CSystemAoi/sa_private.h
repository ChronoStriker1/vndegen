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
    Image *bg, *art[19], **pictures;
    int category, hover, pick;
} Policy;

#define EQUIP_ICONS 100

typedef struct Equip {
    Table items;
    Image *bg, *art[20], *icon[EQUIP_ICONS], *label[21];
    int unit, item, hover;
} Equip;

#define SHOP_ICONS 100
#define SHOP_ROWS 256
#define SHOP_ROWS_MAX 256

typedef struct Shop {
    Image *art[22], *icon[SHOP_ICONS], *bigIcon[SHOP_ICONS];
    int trade[SHOP_ROWS_MAX];
    int hover, focus, scroll;
} Shop;

#define CARD_ICONS 13

#define MISSION_LOG 64

typedef struct Missions {
    Table table;
    Image *art[8];
    int log[MISSION_LOG][3], logCount;
    int hover;
    bool confirm;
} Missions;

enum { MAZE_FLOORS = 7, MAZE_SIZES = 5 };

typedef struct Recent {
    int parties, deepest;
    int fights[2], chests[2], traps[2], lost[2];
    int repelled, opened, broken;
    int flow[3][4];
} Recent;

typedef struct MazeView {
    Image *art[8];
    Image *piece[MAZE_FLOORS][MAZE_SIZES][4];
    Table rules, areas;
    Image *icon[13];
    int size[MAZE_FLOORS];
    int hover;
    bool hideRecent;
    Image *holder[11];
    int holderX[11], holderFloor[11], holderCols[11], holderRows[11], holderFrames[11];
    u32 ms;
} MazeView;

typedef struct Camp {
    Image *art[32], *backdrop;
    Table sgp;
    bool active, sgpOpen, skillPage;
    int hover, confirm, skillPick, floatSlot, floatValue, floatMs;
} Camp;

typedef struct Card {
    Image *frame, *digits, *label[22], *desc, *detail, *toggle, *icon[CARD_ICONS];
    bool showDetail, toggleHover;
} Card;

typedef struct Train {
    Table costs;
    Image *art[16];
    int member, pick, hover;
} Train;

typedef struct Deploy {
    Image *art[64], *benchSprite[PARTY_BENCH], *postRow[20];
    int benchUnit[PARTY_BENCH];
    int hover, picked, heldX, heldY, tab;
    bool overRank;
} Deploy;

typedef struct SaveSummary {
    bool used;
    u32 time;
    int turn, town[4], level, count, play, steps;
    char place[2][48], weapon[48];
} SaveSummary;

#define SAVE_SLOTS 100
#define SAVE_AUTOS 10
#define SAVESCREEN_ART 52

typedef struct SaveScreen {
    Image *art[SAVESCREEN_ART];
    SaveSummary sum[SAVE_SLOTS + SAVE_AUTOS];
    bool saving;
    int page, hover, confirm, ask, picked, latest, autoLatest, back;
    SaveSummary now;
} SaveScreen;

#define EXP_GROUPS 12
#define EXP_TARGETS 5

typedef struct ExpGroup {
    bool alive;
    int party, number, x, y, dir, flags, cooldown, fought, extra;
    bool stay;
    int cells[BATTLE_CELLS];
    int target[EXP_TARGETS][2];
} ExpGroup;

#define EXP_SPECIALS 12

typedef struct Expedition {
    bool active;
    int dungeon, floor, row;
    int steps, areaStart, fighting, stepsLeft;
    int arriveSide, arriveAlong, resumeX, resumeY, resumeDir;
    int last[4], entry[4];
    bool keepGroups, resuming, stepHook;
    ExpGroup group[EXP_GROUPS];
    int groupCount;
    int hooks[EXP_HOOKS], hookCount, after;
    Table floors, dungeons;
    Image *hall, *plate[12], *areaPlate[11], *back, *badge, *seen, *platform;
    int hover, picked;
    bool (*explored)[EXP_FLOORS][DUN_CELLS];
    u8 (*passages)[EXP_FLOORS][DUN_CELLS];
    int (*floorInfo)[EXP_FLOORS][4];
    int special[EXP_SPECIALS], specialValue[EXP_SPECIALS];
} Expedition;
#define MSG_LINES 4

enum { UI_CLOSE, UI_LEAF, UI_SKIP, UI_LOG, UI_REPEAT, UI_OPTION, UI_AUTO, UI_FEATHER, UI_CHOICE_TOP, UI_CHOICE_ROW, UI_CHOICE_BOTTOM, UI_COUNT };
enum { MODE_TITLE, MODE_STORY, MODE_TOWN, MODE_BATTLE, MODE_DUNGEON, MODE_SELECT, MODE_POLICY, MODE_EQUIP, MODE_SHOP, MODE_TRAIN, MODE_DEPLOY, MODE_SAVES, MODE_SUBMIT, MODE_MAZE, MODE_CAMP };
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
    bool endTurnAfterDialog;
    bool turnAdvance;
    bool gameOver;
    char joined[DIALOG_UNITS][48];
    int joinedCount;
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
    Equip equip;
    Shop shop;
    Missions missions;
    Maze maze;
    MazeView mazeView;
    Camp camp;
    Card card;
    Train train;
    Deploy deploy;
    SaveScreen saves;
    bool turnPending, autosaveTurn;
    u32 playMs;
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
void sa_equip_init(Equip *e, Vfs *vfs);
void sa_equip_free(Equip *e);
void sa_equip_open(SA *s);
bool sa_equip_frame(SA *s);
void sa_shop_init(Shop *p, Vfs *vfs);
void sa_shop_free(Shop *p);
void sa_shop_open(SA *s);
bool sa_shop_frame(SA *s);
void sa_shop_wheel(SA *s, int notches);
void sa_card_init(Card *c, Vfs *vfs);
void sa_card_free(Card *c);
void sa_card_draw(SA *s, u32 *fb, const Member *m, int x, int y);
void sa_card_input(SA *s);
void sa_train_init(Train *t, Vfs *vfs);
void sa_train_free(Train *t);
void sa_train_open(SA *s);
bool sa_train_frame(SA *s);
void sa_deploy_init(Deploy *d, Vfs *vfs);
void sa_deploy_free(Deploy *d);
void sa_deploy_open(SA *s);
bool sa_deploy_frame(SA *s, u32 ms);
bool sa_save_write(SA *s, int slot, int count);
void sa_save_current(SA *s, SaveSummary *out);
bool sa_save_read(SA *s, int slot);
void sa_save_delete(SA *s, int slot);
bool sa_save_summary(SA *s, int slot, SaveSummary *out);
void sa_load_into_town(SA *s, int slot);
void sa_enter_loaded(SA *s);
bool sa_pcsave_read(SA *s, const char *path);
bool sa_pcsave_write(SA *s, const char *path);
bool sa_pcsave_summary(SA *s, const char *path, SaveSummary *out);
char *sa_pcsave_path(const SA *s, int slot);
void sa_savescreen_init(SaveScreen *v, Vfs *vfs);
void sa_savescreen_free(SaveScreen *v);
void sa_savescreen_open(SA *s, bool saving);
bool sa_savescreen_frame(SA *s);
void sa_autosave(SA *s);
void sa_mission_init(SA *s);
void sa_mission_free(SA *s);
void sa_mission_reset(SA *s);
int sa_mission_available(SA *s);
int sa_mission_start(SA *s);
bool sa_mission_target(SA *s, int *out);
void sa_mission_check(SA *s);
void sa_mission_end(SA *s, int result);
void sa_mission_open(SA *s);
void sa_mission_draw_confirm(SA *s, u32 *fb);
bool sa_mission_confirm_input(SA *s);
bool sa_mission_frame(SA *s);
void sa_camp_init(SA *s);
void sa_camp_free(SA *s);
void sa_camp_open(SA *s);
void sa_camp_return(SA *s);
bool sa_camp_frame(SA *s);
int sa_camp_points(const SA *s);
void sa_camp_draw_backdrop(SA *s, u32 *fb);
int sa_back_mode(const SA *s);
bool sa_exp_special(SA *s, int id, int a, int b);
bool sa_exp_special_active(const SA *s, int id);
int sa_exp_special_value(const SA *s, int id);
void sa_maze_init(SA *s);
void sa_maze_free(SA *s);
void sa_maze_reset(SA *s);
void sa_maze_open(SA *s);
void sa_maze_update(SA *s);
bool sa_maze_turn(SA *s, int net[3]);
void sa_maze_summary(const SA *s, Recent *r);
void sa_maze_draw_scene(SA *s, u32 *fb);
bool sa_maze_frame(SA *s, u32 elapsedMs);
void sa_exp_init(SA *s);
void sa_exp_free(SA *s);
void sa_exp_open_select(SA *s);
bool sa_exp_frame(SA *s, u32 ms);
bool sa_exp_script_done(SA *s);
void sa_exp_key(SA *s, int key);
void sa_exp_battle_over(SA *s, int result);
void sa_exp_gain(SA *s, const int gain[4]);
void sa_exp_enter(SA *s, int dungeon, int floor);
void sa_exp_resume(SA *s, int dungeon, int floor, int x, int y, int dir, bool keepGroups);
void sa_exp_add_group(SA *s, const char *name);
void sa_exp_add_group_at(SA *s, const char *name, int x, int y);
int sa_exp_area_registered(SA *s);
int sa_exp_place_code(SA *s);
void sa_exp_place_names(SA *s, const char **dungeon, const char **floor);
const char *sa_exp_floor_map(SA *s, int dungeon, int floor);
void sa_exp_names(SA *s, int dungeon, int floor, const char **dungeonName, const char **floorName);
void sa_exp_force_battle(SA *s);
int sa_exp_floors_reached(const SA *s, int dungeon);
void sa_exp_draw_view(SA *s, u32 *fb);
void sa_exp_debug_place(SA *s, int x, int y, int dir);
void sa_reset_scene(SA *s);
void sa_end_turn(SA *s);
void sa_show_frame_as_bg(SA *s, const u32 *px, const char *name);

#endif

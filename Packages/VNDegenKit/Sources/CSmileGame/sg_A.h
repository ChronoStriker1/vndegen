#ifndef SMILEGAME_A_H
#define SMILEGAME_A_H

enum { SGA_PICTURES = 40, SGA_CONDS = 8 };

enum {
    CMD_WAIT = 9,
    CMD_TURN_EVENT = 10,
    CMD_MOVE_EVENT = 11,
    CMD_EVENT_GRAPHIC = 14,
    CMD_TURN_PLAYER = 16,
    CMD_MOVE_PLAYER = 17,
    CMD_TRANSFER = 20,
    CMD_BGM = 21,
    CMD_SE = 22,
    CMD_ERASE_PICTURE = 23,
    CMD_MOVE_PICTURE = 24,
    CMD_SHOW_PICTURE = 25,
    CMD_EFFECT = 27,
    CMD_MESSAGE = 29,
    CMD_CHOICES = 30,
    CMD_VARIABLE = 31,
    CMD_SWITCH = 32,
    CMD_ITEM = 33,
    CMD_MONEY = 34,
    CMD_SHOP = 37,
    CMD_INN = 38,
    CMD_PARTY = 42,
    CMD_FACE_MESSAGE = 43,
    CMD_SCROLL_TEXT = 44,
    CMD_BALLOON = 45,
    CMD_IF_SWITCH = 46,
    CMD_IF_VARIABLE = 47,
    CMD_IF_ITEM = 48,
    CMD_IF_PARTY = 50,
    CMD_SCREEN = 51,
    CMD_ERASE_EVENT = 52,
    CMD_CAMERA = 53,
    CMD_RECOVER = 59,
    CMD_FADE = 60,
    CMD_TINT = 61,
    CMD_SHAKE = 62,
    CMD_FLASH = 63,
    CMD_ME = 64,
    CMD_BATTLE = 65,
    CMD_HERO_GRAPHIC = 69,
    CMD_ELSE = 71,
    CMD_END = 72,
    CMD_CASE = 74,
};

enum { TRIG_AUTO = 1, TRIG_TALK = 2, TRIG_TOUCH = 3, TRIG_EVENT_TOUCH = 4, TRIG_REPEAT = 5, TRIG_PARALLEL = 6 };

enum { COND_SWITCH = 0, COND_VARIABLE = 1, COND_MONEY = 2, COND_ITEM = 3, COND_HERO = 4, COND_ITEM_ALL = 5 };

typedef struct SgaCond {
    i32 kind, op, index, value;
    Guid id;
} SgaCond;

typedef struct SgaSheet {
    SgaCond conds[SGA_CONDS];
    int condCount;
    Guid graphic, script;
    i32 dir, fixDir, moveType, moveSpeed, moveFreq;
    u8 solid, still;
    char *motion;
    i32 trigger;
} SgaSheet;

typedef struct SgaEvent {
    SgaSheet *sheets;
    int sheetCount;
    int sheet;
    int autoSheet;
    bool erased;
    bool graphicOverride;
    Guid graphic;
    char *motion;
    int moveDir, moveLeft;
    float moveSpeed;
    bool moveThrough;
    int savedDir;
    bool turned;
    int wanderDir, wanderLeft;
    u32 wanderWait;
    bool wanderStarted;
    u32 spinMs;
    int spinBase;
} SgaEvent;

typedef struct SgaPicture {
    bool used;
    bool kept;
    Guid id;
    Image *im;
    float alpha, alphaFrom, alphaTo;
    float x, y, xFrom, yFrom, xTo, yTo, zoom, zoomFrom, zoomTo;
    u32 color, colorFrom, colorTo;
    u32 ms, total;
    bool eraseAfter;
} SgaPicture;

typedef struct SgaFaceImage {
    char *key;
    Image *im;
} SgaFaceImage;

typedef struct SgaState {
    bool started;
    int eventIndex;
    Guid eventId;
    i32 trigger;
    int msgCmd, msgPos, speaker;
    Guid faceL, faceR;
    int exprL, exprR;
    bool windowOpen;
    char *lastPage;
    int carryWindow;
    u32 textMs;
    char **choices;
    int choiceCount, choiceCursor, choiceCancel, choiceHeader;
    float scrollSpeed;
    bool scrollStatic;
    u32 captionFadeMs;
    Image *scrollImage;
    SgaPicture pics[SGA_PICTURES];
    float fade, fadeFrom, fadeTo;
    u32 fadeMs, fadeTotal;
    bool fadeWait;
    u32 tint, tintFrom, tintTo;
    u32 tintMs, tintTotal;
    bool tintWait;
    u32 flashColor, flashMs, flashTotal;
    u32 shakeMs, shakeTotal;
    int shakePower;
    SgaEvent *ev;
    int evCount;
    Guid mapId;
    i16 *floor;
    i16 *under;
    int floorW, floorH;
    bool mapPending;
    bool dirty;
    int lastX, lastZ;
    int playerMoveDir, playerMoveLeft;
    float playerMoveSpeed;
    bool waitMoves;
    int screenHeader;
    Image *balloon;
    int balloonTarget;
    u32 balloonMs, balloonTotal;
    bool camWait;
    SgaFaceImage *faces;
    int faceCount;
    bool menuOff68, menuOff77;
} SgaState;

void sga_new_game(SG *g);
void sga_free(SG *g);
void sga_frame(SG *g, u32 ms);
void sga_draw_under(SG *g);
void sga_draw_over(SG *g);
bool sga_busy(SG *g);
void sga_map_loaded(SG *g);
void sga_refresh_events(SG *g);
int sga_event_sheet(SG *g, int npc);
bool sga_cond_holds(SG *g, Guid event, const SgaCond *c);
bool sga_field_input(SG *g);
void sga_start_event(SG *g, int npc, int sheet);
void sga_event_ended(SG *g);
bool sga_exec(SG *g, const Cmd *c);
void sga_actor_graphic(SG *g, Actor *a, Guid graphic);
void sga_move_actor(SG *g, Actor *a, int *dir, int *left, float speed, bool through, u32 ms);
int sga_dir_from_code(int code);
int sga_floor(SG *g, int x, int z);
void sga_step_moves(SG *g, u32 ms);
bool sga_moves_pending(SG *g);
void sga_move_command(SG *g, const Cmd *c, bool player);

i32 sga_item_count(SG *g, Guid id);
void sga_item_add(SG *g, Guid id, i32 n);
bool sga_in_party(SG *g, Guid hero);

void sga_window(SG *g, Image *im, const Res *r, int x, int y, int w, int h);
Image *sga_default_window(SG *g, const Res **res);
Image *sga_selected_window(SG *g, const Res **res);
Image *sga_selectable_window(SG *g, const Res **res);
Image *sga_face(SG *g, Guid folder, int expr);
Image *sga_path_image(SG *g, const char *path);

#endif

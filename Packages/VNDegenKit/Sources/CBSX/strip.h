#ifndef BX_STRIP_H
#define BX_STRIP_H

#include "bsx.h"

#define STRIP_CHARS 8
#define STRIP_ROWS 6
#define STRIP_PAGES 16
#define STRIP_THUMBS 32
#define STRIP_SLOTS 24
#define STRIP_OUTPUTS 4

enum { TAB_BG, TAB_BGV, TAB_MSG, TAB_DATA, TAB_BU, TAB_COUNT };
enum { STRIP_POSE, STRIP_VIEW, STRIP_NGNG, STRIP_WEAR, STRIP_UNDER, STRIP_FACE };
enum { DRAG_POS = 1, DRAG_SIGHT };
enum { STRIP_CUTIN = 0x08, STRIP_HAIR_OFF = 0x20 };

typedef struct BuInfo {
    MBtn icon;
    int wear[16], wearCount, under;
    int face[2][16], faceCount[2];
} BuInfo;

typedef struct BuState {
    int no, hidden;
    float pos;
    int six, pose, view, ngng, wear, under, face;
} BuState;

struct Strip {
    LNode *layout;
    int tab;
    MBtn tabs[TAB_COUNT], back;
    Image *notice[TAB_COUNT];
    int noticeOn;
    unsigned noticeSeen;
    bool hasBgv;
    BgvPanel *bgv;

    Image *buBase;
    MBtn orderUp, orderDown, onOff, pointer, switchBtn, hairBtn, cutinBtn, steamBtn, aheBtn;
    bool steamOff;
    int posRegion[4];
    int rowPos[STRIP_ROWS][2], rowSpace[STRIP_ROWS][2], rowRepeat[STRIP_ROWS];
    char poseSig[8], viewSig[8];
    int ngng[8], ngngCount;
    BuInfo info[STRIP_CHARS];
    int charCount;
    MBtn faceBtn, bustBtn, hipBtn, knob;
    int meter[4];
    int drag;

    Image *bgBase;
    MBtn barBase, bar;
    int barInterval;
    MBtn pages[2][STRIP_PAGES];
    int pageCount[2];
    int thumbPos[STRIP_THUMBS][2], thumbCount;
    Image *thumbs[STRIP_THUMBS];
    int thumbsMade;
    char **bgs;
    int bgCount;
    int viewGroup, viewPage;

    int bgGroup, bgPage, bgIndex;
    BuState st[STRIP_CHARS];
    int sel;
    u8 flags, head[3];
    float sight;
    int level;
    u8 *mesgwnd, *freemsg, *wearTable;
    size_t mesgwndSize, freemsgSize, wearTableSize;

    Image *dsBase, *komaban, *cmdBase;
    MBtn cmdSave, cmdLoad, cmdSlot, output, clear;
    int slotThumbPos[STRIP_SLOTS][2], slotCount;
    int outPos[STRIP_OUTPUTS][2], outCount;
    Image *slotThumbs[STRIP_SLOTS];
    bool slotUsed[STRIP_SLOTS];
    int outputs[STRIP_OUTPUTS];
    int menuSlot;

    u32 *scenePx;
    bool dirty;
    u32 sceneSerial;
};

Strip *bx_strip_open(BX *b);
void bx_strip_free(Strip *s);
bool bx_strip_update(BX *b, Strip *s);
void bx_strip_draw(BX *b, Strip *s, u32 *dst);
void bx_strip_defaults(BX *b, Strip *s);
void bx_strip_set_tab(BX *b, Strip *s, int tab);
void bx_strip_compose(BX *b, Strip *s, u32 *dst);
BuState *bx_strip_char(Strip *s, int no);
int *bx_strip_field(BuState *c, int row);
void bx_strip_name(const Strip *s, const BuState *c, int ngng, int wear, int under, char *out, size_t size);

void bx_strip_data_open(BX *b, Strip *s);
void bx_strip_data_close(Strip *s);
void bx_strip_data_enter(BX *b, Strip *s);
void bx_strip_data_update(BX *b, Strip *s);
void bx_strip_data_draw(BX *b, Strip *s, u32 *dst);
u8 *bx_strip_serialize(BX *b, Strip *s, size_t *size);
bool bx_strip_deserialize(BX *b, Strip *s, const u8 *d, size_t n);

#endif

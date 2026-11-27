#ifndef SMILEGAME_E_H
#define SMILEGAME_E_H

#include <time.h>

enum { SAVE_SLOTS = 40, SAVE_SWITCHES = 1000, SAVE_VARS = 1000 };

typedef struct SaveHero {
    Guid id;
    i32 level, maxHp, maxMp, hp, mp, power, vitality, magic, speed;
    Guid equip[6];
    i32 exp, state;
    Guid learned[16];
    int learnedCount;
    u8 *body;
    i32 size;
} SaveHero;

typedef struct SaveItem {
    Guid id;
    i32 count;
} SaveItem;

typedef struct SaveLook {
    Guid hero, face, graphic;
} SaveLook;

typedef struct SaveLocal {
    Guid event;
    i32 index;
    u8 on;
} SaveLocal;

typedef struct SaveEvent {
    Guid id, graphic;
    char *motion;
    i32 dir;
    float x, y, z;
    u8 flag;
} SaveEvent;

typedef struct SavePicture {
    u8 *body;
    i32 size;
} SavePicture;

typedef struct SaveData {
    i32 head;
    SaveHero *party, *reserve;
    int partyCount, reserveCount;
    SaveItem *items, *seen;
    int itemCount, seenCount;
    i32 money;
    SaveLook *looks;
    int lookCount;
    u8 switches[SAVE_SWITCHES];
    i32 vars[SAVE_VARS];
    u8 config[22];
    SaveLocal *locals;
    int localCount;
    float playSeconds;
    u8 *flagsTail;
    int flagsTailSize;
    Guid map;
    i32 x, z, dir;
    SaveEvent *events;
    int eventCount;
    float playerY;
    i32 cameraMode;
    float cameraRoll, cameraPitch, cameraYaw, cameraDistance, cameraFov, cameraScale;
    float cameraTarget[3];
    u8 cameraFlag;
    SavePicture *pictures;
    int pictureCount;
    u8 *mapTail;
    int mapTailSize;
} SaveData;

typedef struct SlotHero {
    Guid id;
    i32 level, hp, maxHp, mp, maxMp;
} SlotHero;

typedef struct SaveSlot {
    bool used;
    time_t written;
    Guid map;
    SlotHero party[4];
    int partyCount;
} SaveSlot;

bool save_parse(SaveData *s, const u8 *data, size_t size);
u8 *save_serialize(const SaveData *s, size_t *size);
void save_free(SaveData *s);

char *sg_save_path(SG *g, int slot);
int sg_save_slots(SG *g, SaveSlot *out, int max);
bool sg_save_write(SG *g, int slot);
bool sg_save_read(SG *g, int slot);
void sg_save_new_game(SG *g);
SaveData *sg_save_state(SG *g);
float sg_play_seconds(SG *g);
char *sg_settings_path(SG *g);

enum { SND_BGM, SND_BGS, SND_ME, SND_SE };

typedef struct Mixer Mixer;

typedef struct SgE {
    Mixer *mix;
    SaveData save;
    bool haveSave;
    int heroSize;
    Guid curMap;
    int curScene;
} SgE;

SgE *sg_e(SG *g);
void sg_e_open(SG *g);
void sg_e_close(SG *g);
void sg_e_frame(SG *g);
void snd_mix(SG *g, float *out, int frames);
void snd_bgm(SG *g, Guid id, int volume, int tempo);
void snd_bgm_stop(SG *g, int fadeMs);
void snd_bgs(SG *g, Guid id, int volume, int tempo);
void snd_bgs_stop(SG *g, int fadeMs);
void snd_me(SG *g, Guid id, int volume, int tempo);
void snd_se(SG *g, Guid id, int volume, int tempo);
void snd_se_stop(SG *g);
void snd_set_master(SG *g, int bgmPercent, int sePercent);
bool snd_bgm_playing(SG *g, Guid *id);
void snd_enable(SG *g, bool on);

#endif

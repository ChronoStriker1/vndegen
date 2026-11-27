#ifndef SMILEGAME_I_H
#define SMILEGAME_I_H

enum { MOVE_STILL = 0, MOVE_RANDOM = 1, MOVE_CHASE = 2 };

enum { SGI_FX = 8 };

typedef struct SgiFx {
    Image *im;
    int cols, rows;
    u32 ms, total;
    int target;
    bool balloon;
    int icon;
} SgiFx;

typedef struct SgI {
    u32 rng;
    u32 playerSpinMs;
    int playerSpinBase;
    SgiFx fx[SGI_FX];
} SgI;

u32 sgi_rand(SG *g, u32 n);
float sgi_step_speed(i32 speed);
u32 sgi_step_pause(i32 freq);
bool sgi_cell_free(SG *g, int fromX, int fromZ, int x, int z, int self);
void sgi_wander(SG *g, u32 ms);
int sgi_turn(SG *g, int code, const Actor *self, const Actor *other);
bool sgi_move_one(SG *g, const Cmd *c);
void sgi_turn_command(SG *g, const Cmd *c, bool player);
bool sgi_exec(SG *g, const Cmd *c, bool *busy);
bool sgi_encounters_on(SG *g);
void sgi_spins(SG *g, u32 ms);
u32 sgi_fx_start(SG *g, Guid effect, int target, bool balloon);
void sgi_fx_frame(SG *g, u32 ms);
void sgi_fx_draw(SG *g);
void sgi_fx_clear(SG *g);

#endif

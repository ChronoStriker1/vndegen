#ifndef SMILEGAME_K_H
#define SMILEGAME_K_H

enum { SG_NO_DECK = -32768 };

float sg_ground(SG *g, int x, int z);
int sg_deck(SG *g, int x, int z);
int sg_level_into(SG *g, int x, int z, int from);
float sg_ground_at(SG *g, int x, int z, float y);
bool sg_step_free(SG *g, int fromX, int fromZ, int h0, int x, int z, int self);
int sg_stair_kind(SG *g, int x, int z);
void sg_r3_light_models(R3 *r, const float amb[3], const float sun[3]);

#endif

#ifndef SMILEGAME_K_H
#define SMILEGAME_K_H

float sg_ground(SG *g, int x, int z);
int sg_stair_kind(SG *g, int x, int z);
void sg_r3_light_models(R3 *r, const float amb[3], const float sun[3]);

#endif

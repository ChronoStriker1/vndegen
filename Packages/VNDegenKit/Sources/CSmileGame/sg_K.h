#ifndef SMILEGAME_K_H
#define SMILEGAME_K_H

float sg_save_height(SG *g, int x, int z, float y);
bool sg_step_free(SG *g, int fromX, int fromZ, int h0, int x, int z, int self);
void sg_r3_light_models(R3 *r, const float amb[3], const float sun[3]);

#endif

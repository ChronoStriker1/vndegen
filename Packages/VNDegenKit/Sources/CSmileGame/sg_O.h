#ifndef SMILEGAME_O_H
#define SMILEGAME_O_H

bool sg_cell_water(SG *g, int x, int z);
Ptcl *sg_ptcl_load(SG *g, const char *path);
void sg_ptcl_free(Ptcl *p);
void sg_ptcl_update(Ptcl *p, u32 ms);
void sg_battle_list_draw(SG *g, bool skills, const void *const *list, int count, int cursor, int mp);
void sg_ptcl_draw(SG *g, Ptcl *p, const Cam *cam, float x, float y, float z);

#endif

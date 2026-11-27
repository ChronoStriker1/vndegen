#ifndef SMILEGAME_L_H
#define SMILEGAME_L_H

void sg_draw_choice_box(SG *g, char *const *items, int n, int cursor);
void sg_draw_text_box(SG *g, const char *text, int pos);
u32 sg_me_file(SG *g, const char *path, int volume);
int sg_floor_level(SG *g, int x, int z, int from);

#endif

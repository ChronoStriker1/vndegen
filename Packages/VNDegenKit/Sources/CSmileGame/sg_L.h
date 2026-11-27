#ifndef SMILEGAME_L_H
#define SMILEGAME_L_H

enum { CMD_STOP_SE = 76 };

void sg_se_stop_id(SG *g, Guid id);
void sg_draw_choice_box(SG *g, char *const *items, int n, int cursor);
void sg_draw_text_box(SG *g, const char *text, int pos);
u32 sg_me_file(SG *g, const char *path, int volume);
int sg_floor_level(SG *g, int x, int z, int from);

#endif

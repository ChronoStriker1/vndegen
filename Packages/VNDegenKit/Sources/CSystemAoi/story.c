#include "sa_private.h"

enum {
    F_STORY = 7,
    STORY_KIND = 1,
    C_STORY_KIND = 95,
    C_STORY_SCENE = 96,
    D4_DUNGEON = 0,
    D4_FLOOR = 1,
    D4_X = 4,
    D4_Y = 5,
    D4_DIR = 6,
    D4_OPERANDS = 7,
};

int sa_story_scene(const SA *s) {
    const Dungeon *d = &s->dungeon;
    if (!s->exp.active || !d->loaded || d->x < 0 || d->y < 0 || d->x >= d->w || d->y >= d->h) return 0;
    int n = d->cell[d->y * DUN_SIDE + d->x][F_STORY];
    return n > 0 ? n : 0;
}

bool sa_story_arm(SA *s) {
    int n = sa_story_scene(s);
    if (!n) return false;
    s->vm.c[C_STORY_KIND] = STORY_KIND;
    s->vm.c[C_STORY_SCENE] = n;
    sa_log(s, "story cell %d,%d: scene %d", s->dungeon.x, s->dungeon.y, n);
    return true;
}

bool sa_story_collapse(const int *v, int count, StoryWarp *to) {
    if (count < D4_OPERANDS) return false;
    *to = (StoryWarp){v[D4_DUNGEON], v[D4_FLOOR], v[D4_X], v[D4_Y], v[D4_DIR] & 3};
    return to->dungeon > 0 && to->floor > 0;
}

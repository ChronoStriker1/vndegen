#include "pj.h"

bool pj_def_parse(Def *d, const char *name, const u8 *data, size_t size) {
    memset(d, 0, sizeof *d);
    snprintf(d->name, sizeof d->name, "%s", name);
    d->alpha = 255;
    d->scale = 100;
    d->show = true;
    Ini c = {0};
    pj_ini_parse(&c, data, size);
    for (int i = 0; i < c.count; i++) {
        const char *k = c.key[i], *v = c.val[i];
        int n[8];
        if (pj_ieq(k, "type")) snprintf(d->type, sizeof d->type, "%s", v);
        else if (pj_ieq(k, "srce")) snprintf(d->src[0], sizeof d->src[0], "%s", v);
        else if (pj_ieq(k, "srcj")) snprintf(d->src[1], sizeof d->src[1], "%s", v);
        else if (pj_ieq(k, "srcc")) snprintf(d->src[2], sizeof d->src[2], "%s", v);
        else if (pj_ieq(k, "xpos")) d->x = atoi(v);
        else if (pj_ieq(k, "ypos")) d->y = atoi(v);
        else if (pj_ieq(k, "filepos") && pj_ini_ints(v, n, 4) == 4) {
            d->hasRect = true;
            d->rx = n[0];
            d->ry = n[1];
            d->rw = n[2];
            d->rh = n[3];
        } else if (pj_ieq(k, "anime") && d->animeCount < 4)
            snprintf(d->anime[d->animeCount++], sizeof d->anime[0], "%s", v);
        else if (pj_ieq(k, "child") && d->childCount < 4) {
            snprintf(d->child[d->childCount], sizeof d->child[0], "%s", v);
            char *cm = strchr(d->child[d->childCount], ',');
            if (cm) *cm = 0;
            d->childCount++;
        } else if (pj_ieq(k, "alpha")) d->alpha = atoi(v);
        else if (pj_ieq(k, "scale")) d->scale = atoi(v);
        else if (pj_ieq(k, "xcenter")) d->cx = atoi(v);
        else if (pj_ieq(k, "ycenter")) d->cy = atoi(v);
        else if (pj_ieq(k, "show")) d->show = atoi(v) != 0;
        else if (pj_ieq(k, "xsize")) d->xsize = atoi(v);
        else if (pj_ieq(k, "ysize")) d->ysize = atoi(v);
        else if (pj_ieq(k, "layer")) d->filter = true;
    }
    pj_ini_free(&c);
    return d->src[0][0] || d->src[1][0] || d->xsize > 0;
}

bool pj_anm_parse(Anm *a, u8 *data, size_t size) {
    memset(a, 0, sizeof *a);
    if (size < 16) return false;
    if (!memcmp(data, "ANM1", 4)) {
        a->kind = ANM_ONE;
        a->start = rd32(data + 8);
        a->end = rd32(data + 12);
        if (a->start > size || a->end > size || a->start > a->end) return false;
    } else if (!memcmp(data, "ANM2", 4)) {
        if (size < 32) return false;
        a->kind = ANM_TWO;
        a->type = (int)rd32(data + 4);
        for (int i = 0; i < 6; i++) {
            a->state[i] = rd32(data + 8 + 4 * i);
            if (a->state[i] >= size) return false;
        }
        if (a->type == 1 && size >= 0x40) {
            memcpy(a->sound, data + 0x20, 32);
            a->sound[32] = 0;
        }
        a->end = (u32)size;
    } else
        return false;
    a->data = data;
    a->size = size;
    return true;
}

void pj_anm_free(Anm *a) {
    free(a->data);
    memset(a, 0, sizeof *a);
}

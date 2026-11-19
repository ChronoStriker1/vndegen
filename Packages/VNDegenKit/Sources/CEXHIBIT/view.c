#include "ex.h"

static Layer *layer_get(View *v, int id, bool create) {
    for (int i = 0; i < v->layerCount; i++)
        if (v->layers[i].id == id) return &v->layers[i];
    if (!create) return NULL;
    v->layers = ex_realloc(v->layers, sizeof(Layer) * (size_t)(v->layerCount + 1));
    Layer *l = &v->layers[v->layerCount++];
    memset(l, 0, sizeof *l);
    l->id = id;
    l->a = 255;
    return l;
}

static Item *item_get(Layer *l, int id) {
    for (int i = 0; i < l->itemCount; i++)
        if (l->items[i].id == id) return &l->items[i];
    l->items = ex_realloc(l->items, sizeof(Item) * (size_t)(l->itemCount + 1));
    Item *it = &l->items[l->itemCount++];
    memset(it, 0, sizeof *it);
    it->id = id;
    it->layer = l->id;
    it->enabled = true;
    return it;
}

void view_destroy(EX *e) {
    View *v = &e->view;
    for (int i = 0; i < v->layerCount; i++) {
        Layer *l = &v->layers[i];
        for (int j = 0; j < l->itemCount; j++) {
            image_free(l->items[j].img);
            free(l->items[j].sound);
            free(l->items[j].disabledIf);
        }
        free(l->items);
    }
    free(v->layers);
    memset(v, 0, sizeof *v);
}

void view_prepare(EX *e, const char *spec) {
    view_destroy(e);
    View *v = &e->view;
    char *text = ex_strdup(spec);
    for (char *line = strtok(text, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        char *eq = strchr(line, '=');
        if (!eq || strncmp(line, "layer.", 6)) continue;
        *eq = 0;
        char *val = eq + 1;
        char *key = line + 6;
        int lid = (int)strtol(key, &key, 10);
        Layer *l = layer_get(v, lid, true);
        if (!strcmp(key, ".create")) {
            char *f[12];
            int n = split_csv(val, f, 12);
            if (n >= 5) {
                l->x = field_int(f[1]);
                l->y = field_int(f[2]);
                l->w = field_int(f[3]);
                l->h = field_int(f[4]);
            }
        } else if (!strncmp(key, ".item.", 6)) {
            char *rest = key + 6;
            int iid = (int)strtol(rest, &rest, 10);
            Item *it = item_get(l, iid);
            if (!strcmp(rest, ".create")) {
                char *f[16];
                int n = split_csv(val, f, 16);
                if (n >= 7) {
                    it->disabledIf = ex_strdup(f[2]);
                    it->img = image_load(e, f[6]);
                    it->states = it->img && l->w > 0 ? it->img->w / l->w : 1;
                    if (it->states < 1) it->states = 1;
                }
            } else if (!strcmp(rest, ".action")) {
                char *f[6];
                int n = split_csv(val, f, 6);
                for (int k = 0; k < n && k < 6; k++) it->action[k] = field_int(f[k]);
            } else if (!strcmp(rest, ".sound.enter")) {
                char *f[4];
                int n = split_csv(val, f, 4);
                if (n >= 3) it->sound = ex_strdup(f[2]);
            }
        }
    }
    free(text);
    v->hover = -1;
    v->result = -2;
}

void view_show(EX *e, char *args) {
    (void)args;
    e->view.shown = true;
}

void view_select(EX *e, char *args) {
    char *f[8];
    int n = split_csv(args, f, 8);
    e->view.resultReg = n > 3 ? field_int(f[3]) : 1300;
    e->view.result = -2;
    e->view.active = true;
    e->lPress = e->rPress = false;
}

void view_move(EX *e, char *args) {
    char *f[40];
    int n = split_csv(args, f, 40);
    if (n < 4) return;
    int ms = field_int(f[1]);
    for (int i = 4; i + 3 < n; i += 4) {
        int id = field_int(f[i]);
        if (id < 0) continue;
        Layer *l = layer_get(&e->view, id, false);
        if (!l) continue;
        int x = field_int(f[i + 1]), y = field_int(f[i + 2]), kind = field_int(f[i + 3]);
        if (ms <= 0) {
            l->x = x;
            l->y = y;
            l->mx.active = l->my.active = false;
        } else {
            anim_start(e, &l->mx, (float)l->x, (float)x, ms, kind);
            anim_start(e, &l->my, (float)l->y, (float)y, ms, kind);
        }
    }
}

void view_fade(EX *e, char *args, bool enter) {
    (void)enter;
    char *f[40];
    int n = split_csv(args, f, 40);
    for (int i = 2; i + 3 < n; i += 4) {
        int id = field_int(f[i]);
        if (id < 0) continue;
        int to = field_int(f[i + 1]), ms = field_int(f[i + 2]);
        for (int k = 0; k < EX_BILLBOARDS; k++) {
            Billboard *b = &e->bb[k];
            if (!b->used || b->id != id) continue;
            anim_start(e, &b->alpha, b->a, (float)to, ms, 0);
            if (ms <= 0) b->a = (float)to;
        }
        Layer *l = layer_get(&e->view, id, false);
        if (l) {
            anim_start(e, &l->fade, l->a, (float)to, ms, 0);
            if (ms <= 0) l->a = (float)to;
        }
    }
}

static bool item_enabled(EX *e, Item *it) { return !it->disabledIf || !vm_cond(e, it->disabledIf) || !strcmp(it->disabledIf, "false"); }

static void layer_pos(EX *e, Layer *l) {
    if (l->mx.active) l->x = (int)anim_value(e, &l->mx, (float)l->x);
    if (l->my.active) l->y = (int)anim_value(e, &l->my, (float)l->y);
    if (l->fade.active) l->a = anim_value(e, &l->fade, l->a);
}

void view_draw(EX *e, u32 *dst) {
    View *v = &e->view;
    if (!v->shown) return;
    for (int i = 0; i < v->layerCount; i++) {
        Layer *l = &v->layers[i];
        layer_pos(e, l);
        for (int j = 0; j < l->itemCount; j++) {
            Item *it = &l->items[j];
            if (!it->img) continue;
            int state = !item_enabled(e, it) ? 2 : v->hover == it->id ? 1 : 0;
            if (state >= it->states) state = 0;
            int w = it->img->w / it->states;
            blit_part(dst, EX_W, EX_H, it->img, state * w, 0, w, it->img->h, l->x, l->y, (int)l->a);
        }
    }
}

void view_input(EX *e) {
    View *v = &e->view;
    if (!v->shown) return;
    int hover = -1;
    Item *hit = NULL;
    for (int i = 0; i < v->layerCount; i++) {
        Layer *l = &v->layers[i];
        for (int j = 0; j < l->itemCount; j++) {
            Item *it = &l->items[j];
            if (!it->img || !item_enabled(e, it)) continue;
            int w = it->img->w / it->states;
            if (e->mouseX >= l->x && e->mouseX < l->x + w && e->mouseY >= l->y && e->mouseY < l->y + it->img->h) {
                hover = it->id;
                hit = it;
            }
        }
    }
    if (hover != v->hover) {
        v->hover = hover;
        if (hit && hit->sound) snd_play_file(e, hit->sound, 3);
    }
    if (!v->active) return;
    if (e->lPress && hit) {
        e->lPress = false;
        if (hit->action[0] == 51) v->result = hit->id;
        else {
            ex_log(e, "menu action %d", hit->action[0]);
            v->result = hit->id;
        }
        *reg_ref(e, 'R', v->resultReg) = v->result;
        v->active = false;
    } else if (e->rPress) {
        e->rPress = false;
        v->result = -1;
        *reg_ref(e, 'R', v->resultReg) = -1;
        v->active = false;
    }
}

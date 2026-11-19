#include "br.h"

static Node *pair_value(Node *list, const char *key) { return sx_cdr_value(sx_find(list, key)); }

static Node *first_url(Node *pic) {
    if (!pic || pic->t != N_LIST) return NULL;
    Node *u = sx_find(pic, "url");
    if (u) return sx_cdr_value(u);
    for (int i = 0; i < pic->n; i++)
        if (pic->k[i]->t == N_LIST) {
            Node *v = sx_cdr_value(sx_find(pic->k[i], "url"));
            if (v) return v;
        }
    return NULL;
}

static u32 rgb(Node *c, u32 dflt) {
    if (!c || c->t != N_LIST || c->n < 3) return dflt;
    return (u32)sx_int(c->k[0], 0) | (u32)sx_int(c->k[1], 0) << 8 | (u32)sx_int(c->k[2], 0) << 16 | 0xff000000u;
}

Parts *parts_get(BR *e, int id) {
    if (id < 0 || id >= BR_PARTS) return NULL;
    if (e->parts[id]) return e->parts[id];
    if (e->partsMissing[id]) return NULL;
    Node *def = sx_assoc(sx_find(e->cfg.root, ":parts"), id);
    if (!def) {
        e->partsMissing[id] = true;
        br_log(e, "parts %d is not defined", id);
        return NULL;
    }
    Parts *p = br_alloc(sizeof *p);
    p->id = id;
    p->def = def;
    Node *type = pair_value(def, "type");
    p->type = sx_sym(type, "button") ? P_BUTTON : sx_sym(type, "text") ? P_TEXT : sx_sym(type, "animation") ? P_ANIM : sx_sym(type, "touchmap") ? P_TOUCHMAP : P_IMAGE;
    if (p->type == P_BUTTON) {
        for (int i = 0; i < 4; i++) {
            char k[8];
            snprintf(k, sizeof k, "pic%d", i);
            p->pic[i] = first_url(pair_value(def, k));
        }
    } else if (p->type == P_IMAGE) p->pic[0] = pair_value(def, "url");
    else if (p->type == P_TEXT) {
        p->fontSize = sx_int(pair_value(def, "fontsize"), 24);
        p->color = rgb(pair_value(def, "fontcolor"), 0xffffffffu);
        const char *s = sx_str(pair_value(def, "string"));
        p->text = br_strdup(s ? s : "");
    } else if (p->type == P_ANIM) {
        Node *list = pair_value(def, "piclist");
        int wait = 100;
        for (int i = 0; list && list->t == N_LIST && i < list->n && p->frames < BR_ANIM_FRAMES; i++) {
            Node *f = list->k[i];
            p->frame[p->frames] = sx_cdr_value(sx_find(f, "url"));
            Node *w = sx_cdr_value(sx_find(f, "waittime"));
            if (w) wait = sx_int(w, wait);
            p->frameMs[p->frames++] = wait;
        }
    } else if (p->type == P_TOUCHMAP) p->map = pair_value(def, "map");
    e->parts[id] = p;
    return p;
}

void parts_free_all(BR *e) {
    for (int i = 0; i < BR_PARTS; i++) {
        if (!e->parts[i]) continue;
        free(e->parts[i]->text);
        free(e->parts[i]);
        e->parts[i] = NULL;
    }
}

Image *parts_image(BR *e, Parts *p, int state, u32 now) {
    if (!p) return NULL;
    if (p->type == P_BUTTON) {
        Node *u = p->pic[state & 3];
        if (!u) u = p->pic[0];
        return image_url(e, u);
    }
    if (p->type == P_IMAGE) return image_url(e, p->pic[0]);
    if (p->type == P_ANIM && p->frames) {
        u32 total = 0;
        for (int i = 0; i < p->frames; i++) total += (u32)p->frameMs[i];
        u32 t = total ? now % total : 0;
        int i = 0;
        while (i < p->frames - 1 && t >= (u32)p->frameMs[i]) t -= (u32)p->frameMs[i++];
        return image_url(e, p->frame[i]);
    }
    return NULL;
}

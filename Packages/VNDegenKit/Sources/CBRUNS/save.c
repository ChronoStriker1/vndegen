#include "br.h"

#include <time.h>

static Node *nl(void) {
    Node *n = br_alloc(sizeof *n);
    n->t = N_LIST;
    return n;
}

static Node *ni(i32 v) {
    Node *n = br_alloc(sizeof *n);
    n->t = N_INT;
    n->i = v;
    return n;
}

static Node *ns(const char *s) {
    Node *n = br_alloc(sizeof *n);
    n->t = N_STR;
    n->s = br_strdup(s ? s : "");
    return n;
}

static Node *nsym(const char *s) {
    Node *n = br_alloc(sizeof *n);
    n->t = N_SYM;
    n->s = br_strdup(s);
    return n;
}

static Node *add(Node *list, Node *k) {
    list->k = br_realloc(list->k, sizeof(Node *) * (size_t)(list->n + 1));
    list->k[list->n++] = k;
    return k;
}

static Node *pair(Node *list, Node *key, Node *v) {
    Node *p = add(list, nl());
    add(p, key);
    add(p, nsym("."));
    add(p, v);
    return p;
}

static Node *kv(Node *list, const char *key, Node *v) { return pair(list, nsym(key), v); }

static Node *kvi(Node *list, const char *key, i32 v) { return kv(list, key, ni(v)); }

static Node *kvb(Node *list, const char *key, bool v) { return kv(list, key, nsym(v ? "#t" : "#f")); }

static Node *sub(Node *list, const char *key) {
    Node *p = add(list, nl());
    add(p, nsym(key));
    return p;
}

static Node *get(Node *list, const char *key) { return sx_cdr_value(sx_find(list, key)); }

static int geti(Node *list, const char *key, int dflt) { return sx_int(get(list, key), dflt); }

static bool getb(Node *list, const char *key, bool dflt) {
    Node *v = get(list, key);
    if (sx_sym(v, "#t")) return true;
    if (sx_sym(v, "#f")) return false;
    return dflt;
}

typedef struct Buf {
    char *p;
    size_t n, cap;
} Buf;

static void put(Buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        b->cap = (b->n + n + 1) * 2;
        b->p = br_realloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}

static void print(Buf *b, Node *n) {
    char tmp[32];
    switch (n->t) {
    case N_INT:
        snprintf(tmp, sizeof tmp, "%d", n->i);
        put(b, tmp, strlen(tmp));
        break;
    case N_SYM: put(b, n->s, strlen(n->s)); break;
    case N_STR:
        put(b, "\"", 1);
        for (const u8 *p = (const u8 *)n->s; *p;) {
            int l = br_sjis_len(p);
            if (l == 1 && (*p == '"' || *p == '\\')) put(b, "\\", 1);
            put(b, (const char *)p, (size_t)l);
            p += l;
        }
        put(b, "\"", 1);
        break;
    default:
        put(b, "(", 1);
        for (int i = 0; i < n->n; i++) {
            if (i) put(b, " ", 1);
            print(b, n->k[i]);
        }
        put(b, ")", 1);
    }
}

char *sx_print(Node *n, size_t *len) {
    Buf b = {0};
    print(&b, n);
    if (len) *len = b.n;
    return b.p;
}

static char *sys_path(BR *e) { return br_fmt("%s/bruns-system.dat", e->saveDir); }

static void flags_from(BR *e, Node *root) {
    Node *ints = sx_find(sx_find(root, "sysflag"), "sysflag-int");
    for (int i = 1; ints && i < ints->n; i++) {
        Node *p = ints->k[i];
        if (p->t != N_LIST || !p->n) continue;
        int k = sx_int(p->k[0], -1);
        if (k >= 0 && k < BR_FLAGS) e->flag[k] = sx_int(sx_cdr_value(p), 0);
    }
}

void sys_load(BR *e) {
    char rel[128];
    snprintf(rel, sizeof rel, "save/bruns-system0.dat");
    size_t n;
    u8 *d = br_read_game(e, rel, &n);
    if (d) {
        Node *def = sx_parse((const char *)d, n);
        if (def->n) flags_from(e, def->k[0]);
        sx_free(def);
        free(d);
    }
    char *path = sys_path(e);
    u8 *raw = br_read_file(path, &n);
    free(path);
    if (raw) {
        size_t m;
        u8 *t = br_eenc_decode(raw, n, &m);
        free(raw);
        if (t) {
            Node *top = sx_parse((const char *)t, m);
            free(t);
            if (top->n && top->k[0]->t == N_LIST) {
                e->sysRoot = top->k[0];
                top->k[0] = NULL;
                top->n = 0;
            }
            sx_free(top);
        }
    }
    if (!e->sysRoot) {
        const char *init = "((version . 1) (sysflag (sysflag-int) (sysflag-string)) (flowmodestate (routePerSPMap) (routeMap) (stateMap) (saveData)) (resource-view-flag (1) (2)))";
        Node *top = sx_parse(init, strlen(init));
        e->sysRoot = top->k[0];
        top->n = 0;
        sx_free(top);
    } else flags_from(e, e->sysRoot);
}

static Node *ensure(Node *list, const char *key) {
    Node *n = sx_find(list, key);
    return n ? n : sub(list, key);
}

static void flags_to(BR *e) {
    Node *sf = ensure(e->sysRoot, "sysflag");
    Node *ints = sx_find(sf, "sysflag-int");
    if (ints) {
        for (int i = 1; i < ints->n; i++) sx_free(ints->k[i]);
        ints->n = 1;
    } else ints = sub(sf, "sysflag-int");
    for (int i = 0; i < BR_FLAGS; i++)
        if (e->flag[i]) pair(ints, ni(i), ni(e->flag[i]));
}

bool sys_save(BR *e) {
    flags_to(e);
    size_t len;
    char *text = sx_print(e->sysRoot, &len);
    size_t n;
    u8 *enc = br_eenc_encode((const u8 *)text, len, true, &n);
    free(text);
    br_mkdirs(e->saveDir);
    char *path = sys_path(e);
    bool ok = br_write_file(path, enc, n);
    free(path);
    free(enc);
    e->sysDirty = false;
    return ok;
}

void sys_mark_dirty(BR *e) { e->sysDirty = true; }

static Node *layer_node(BR *e, Layer *l) {
    Node *n = nl();
    add(n, ni(l->no));
    sub(n, "hitTestAMMap");
    kvi(n, "hitTestAMMode", 0);
    sub(n, "hitTestAutoCursorMap");
    sub(n, "hitTestEventNo");
    kvi(n, "hitTestParts", -1);
    kvb(n, "maskSALoop", true);
    kvi(n, "maskSACtrl", -1);
    kvi(n, "maskPictureEdge", 1);
    kvi(n, "maskPictureNo", -1);
    kvi(n, "maskOp", 1);
    kvb(n, "mainSALoop", true);
    kvi(n, "mainSACtrl", l->pic >= 0 ? -1 : 2);
    kvi(n, "mainPictureNo", l->pic);
    Node *m = sub(n, "motionParams");
    static const struct {
        const char *k;
        int i;
    } mp[] = {{"MC-a", LP_A}, {"MC-b", LP_B}, {"MC-g", LP_G}, {"MC-r", LP_R}, {"MT-h", LP_MTH}, {"MT-w", LP_MTW}, {"MT-y", LP_MTY}, {"MT-x", LP_MTX},
              {"MI-h", LP_MIH}, {"MI-w", LP_MIW}, {"MI-y", LP_MIY}, {"MI-x", LP_MIX}, {"mask-switch", LP_MSWITCH}, {"mask-system", LP_MSYS},
              {"T-h", LP_TH}, {"T-w", LP_TW}, {"T-y", LP_TY}, {"T-x", LP_TX}, {"I-h", LP_IH}, {"I-w", LP_IW}, {"I-y", LP_IY}, {"I-x", LP_IX}, {"main-switch", LP_SWITCH}};
    for (size_t i = 0; i < sizeof mp / sizeof *mp; i++) kvi(m, mp[i].k, l->v[mp[i].i]);
    kvi(n, "compositeOp", 1);
    kvb(n, "visible", l->visible);
    kvi(n, "order", l->order);
    kvi(n, "vram-height", 0);
    kvi(n, "vram-width", 0);
    kvi(n, "mode", l->mode);
    kvi(n, "groupNo", l->group);
    kvi(n, "no", l->no);
    return n;
}

static void layer_from(BR *e, Node *n) {
    int no = sx_int(n->k[0], -1);
    Layer *l = layer_new(e, no, geti(n, "mode", 1));
    l->order = geti(n, "order", no);
    l->group = geti(n, "groupNo", -1);
    l->visible = getb(n, "visible", true);
    l->pic = geti(n, "mainPictureNo", -1);
    Node *m = sx_find(n, "motionParams");
    static const struct {
        const char *k;
        int i;
    } mp[] = {{"MC-a", LP_A}, {"MC-b", LP_B}, {"MC-g", LP_G}, {"MC-r", LP_R}, {"MT-h", LP_MTH}, {"MT-w", LP_MTW}, {"MT-y", LP_MTY}, {"MT-x", LP_MTX},
              {"MI-h", LP_MIH}, {"MI-w", LP_MIW}, {"MI-y", LP_MIY}, {"MI-x", LP_MIX}, {"mask-switch", LP_MSWITCH}, {"mask-system", LP_MSYS},
              {"T-h", LP_TH}, {"T-w", LP_TW}, {"T-y", LP_TY}, {"T-x", LP_TX}, {"I-h", LP_IH}, {"I-w", LP_IW}, {"I-y", LP_IY}, {"I-x", LP_IX}, {"main-switch", LP_SWITCH}};
    for (size_t i = 0; i < sizeof mp / sizeof *mp; i++) l->v[mp[i].i] = geti(m, mp[i].k, l->v[mp[i].i]);
    memcpy(l->from, l->v, sizeof l->v);
    l->dur = 0;
}

static Node *obj_node(Obj *o) {
    Node *n = nl();
    add(n, ni(o->kind * 100000 + o->id));
    kvi(n, "kind", o->kind);
    kvi(n, "id", o->id);
    kvi(n, "parts", o->parts);
    kvi(n, "layer", o->layer);
    kvi(n, "x", o->x);
    kvi(n, "y", o->y);
    kvi(n, "z", o->z);
    kvi(n, "alpha", o->alpha);
    kvi(n, "flag", o->flag);
    kvi(n, "event", o->event);
    kvi(n, "hoverEvent", o->hoverEvent);
    kvb(n, "disabled", o->disabled);
    if (o->text) kv(n, "text", ns(o->text));
    return n;
}

Node *state_capture(BR *e, bool globals) {
    Node *st = nl();
    Node *dlk = sub(st, "dlkstate");
    Node *pls = sub(dlk, "pls");
    for (int i = 0; i < BR_STRINGS; i++)
        if (e->str[i] && e->str[i][0]) pair(pls, ni(i), ns(e->str[i]));
    sub(dlk, "plf");
    Node *lay = sub(dlk, "layer");
    sub(lay, "group");
    Node *ll = sub(lay, "layer");
    for (int i = 0; i < e->layerCount; i++) add(ll, layer_node(e, e->layers[i]));
    Node *pl = sub(lay, "picture");
    for (int i = 0; i < e->picCount; i++) {
        Pic *p = e->pics[i];
        Node *n = add(pl, nl());
        add(n, ni(p->no));
        kvi(n, "layer", p->layer);
        kvi(n, "numsel", p->numsel);
        if (p->path) kv(n, "url", ns(p->path));
        else kvi(n, "url", p->url);
        kvi(n, "no", p->no);
        kvi(n, "type", p->type);
    }
    sub(sub(dlk, "voice"), "voice-volume");
    kvi(sub(dlk, "sound"), "volume", 100);
    Node *mus = sub(dlk, "music");
    for (int i = 0; i < 4; i++) {
        Node *m = add(mus, nl());
        kvi(m, "loopnum", 0);
        kvi(m, "loopend", 0);
        kvi(m, "loopstart", 0);
        kvi(m, "loopsetting", 0);
        kvi(m, "startposition", 0);
        kvi(m, "volume", (int)(e->chanGain[CH_MUSIC + i] * 100 + 0.5f));
        if (e->music[i] >= 0) kvi(m, "music", e->music[i]);
        else kvb(m, "music", false);
    }
    Node *mouse = sub(dlk, "mouse");
    kvi(mouse, "showflag", 2);
    kvi(mouse, "cursor", -1);
    Node *fader = sub(dlk, "fader");
    int fa = (int)anim_value(e, &e->overlay);
    kvi(fader, "a", fa > 0 ? fa : -1);
    kvi(fader, "b", fa > 0 ? (int)(e->overlayColor >> 16 & 0xff) : -1);
    kvi(fader, "g", fa > 0 ? (int)(e->overlayColor >> 8 & 0xff) : -1);
    kvi(fader, "r", fa > 0 ? (int)(e->overlayColor & 0xff) : -1);
    Node *parts = sub(dlk, "parts");
    for (int k = 0; k < OBJ_KINDS; k++)
        for (int i = 0; i < BR_OBJS; i++)
            if (e->obj[k][i]) add(parts, obj_node(e->obj[k][i]));
    sub(dlk, "cell");
    Node *pic = sub(dlk, "picture");
    kvi(pic, "th", 0);
    kvi(pic, "tw", 0);
    kvi(pic, "ty", 0);
    kvi(pic, "tx", -1);
    if (e->bgNum >= 0 && e->bg) kvi(pic, "picno", e->bgNum);
    else kvb(pic, "picno", false);
    Node *mw = sub(dlk, "msgwin");
    kvi(mw, "posY", 0);
    kvi(mw, "posX", 0);
    kvi(mw, "posIndex", 0);
    kvi(mw, "keywait", e->wait == W_TEXT ? 1 : 0);
    kv(mw, "text", ns(e->msg.raw ? e->msg.raw : ""));
    kvb(mw, "visible", e->msg.visible);
    kvi(mw, "cfgno", e->msg.win);
    Node *menu = sub(dlk, "menu");
    kvb(menu, "show", e->menu.show);
    kvb(menu, "autohide", e->menu.autohide);
    kvb(menu, "enable", e->menu.enable);
    kvi(menu, "cfgno", 0);
    Node *us = sub(st, "userstack");
    kvi(us, "acc", e->acc);
    kvi(us, "reflag", e->reflag);
    kvi(us, "next-prg", -1);
    Node *stack = sub(us, "stack");
    for (int i = 0; i < e->sp; i++) {
        Frame *f = &e->stack[i];
        Node *fr = add(stack, nl());
        kvi(fr, "sce", f->sc->num);
        bool isSet = f->u == &f->sc->set;
        kvi(fr, "set", isSet ? 0 : -1);
        kvi(fr, "sub", isSet ? -1 : f->u->id);
        kvi(fr, "scriptFunc", -1);
        int pc = f->pc;
        if (i == e->sp - 1 && e->wait != W_NONE && pc > 0) pc--;
        kvi(fr, "pc", pc);
        Node *loc = sub(fr, "local");
        for (int k = 0; k < f->u->locals && k < BR_LOCALS; k++) add(loc, ni(f->local[k]));
        sub(fr, "args");
    }
    Node *w = sub(st, "word");
    if (globals)
        for (int i = 0; i < BR_WORDS; i++)
            if (e->word[i]) pair(w, ni(i), ni(e->word[i]));
    sub(st, "flag");
    sub(st, "bit");
    kvi(st, "lastSPID", -1);
    return st;
}

static void obj_from(BR *e, Node *n) {
    Obj *o = obj_create(e, geti(n, "kind", OBJ_BTN), geti(n, "id", 0), geti(n, "parts", 0), geti(n, "layer", 1), geti(n, "x", 0), geti(n, "y", 0), geti(n, "z", 0), geti(n, "alpha", 255), geti(n, "flag", 0));
    if (!o) return;
    o->event = geti(n, "event", -1);
    o->hoverEvent = geti(n, "hoverEvent", -1);
    o->disabled = getb(n, "disabled", false);
    const char *t = sx_str(get(n, "text"));
    if (t) o->text = br_strdup(t);
}

bool state_apply(BR *e, Node *st, bool globals) {
    Node *us = sx_find(st, "userstack");
    Node *stack = sx_find(us, "stack");
    if (!stack) return false;
    Frame frames[BR_STACK];
    int sp = 0;
    for (int i = 1; i < stack->n && sp < BR_STACK; i++) {
        Node *fr = stack->k[i];
        Script *sc = script_get(e, geti(fr, "sce", -1));
        if (!sc) return false;
        Unit *u = geti(fr, "set", -1) >= 0 ? &sc->set : script_sub(sc, geti(fr, "sub", -1));
        if (!u) return false;
        Frame *f = &frames[sp++];
        memset(f, 0, sizeof *f);
        f->sc = sc;
        f->u = u;
        f->pc = geti(fr, "pc", 0);
        Node *loc = sx_find(fr, "local");
        for (int k = 1; loc && k < loc->n && k - 1 < BR_LOCALS; k++) f->local[k - 1] = sx_int(loc->k[k], 0);
    }
    memcpy(e->stack, frames, sizeof(Frame) * (size_t)sp);
    e->sp = sp;
    e->acc = geti(us, "acc", 0);
    e->reflag = geti(us, "reflag", 0);
    e->wait = W_NONE;
    Node *dlk = sx_find(st, "dlkstate");
    if (globals) {
        for (int i = 0; i < BR_STRINGS; i++) {
            free(e->str[i]);
            e->str[i] = NULL;
        }
        Node *pls = sx_find(dlk, "pls");
        for (int i = 1; pls && i < pls->n; i++) str_set(e, sx_int(pls->k[i]->k[0], -1), sx_str(sx_cdr_value(pls->k[i])));
        memset(e->word, 0, sizeof(i32) * BR_WORDS);
        Node *w = sx_find(st, "word");
        for (int i = 1; w && i < w->n; i++) *vm_word(e, sx_int(w->k[i]->k[0], -1)) = sx_int(sx_cdr_value(w->k[i]), 0);
    }
    layers_reset(e);
    image_free(e->transFrom);
    image_free(e->frozenImg);
    e->transFrom = e->frozenImg = NULL;
    e->frozen = false;
    Node *lay = sx_find(dlk, "layer");
    Node *pl = sx_find(lay, "picture");
    for (int i = 1; pl && i < pl->n; i++) {
        Node *p = pl->k[i];
        Node *url = get(p, "url");
        pic_new(e, geti(p, "layer", -1), sx_int(p->k[0], -1), url && url->t == N_INT ? url->i : -1, url && url->t == N_STR ? url->s : NULL, geti(p, "type", 1), geti(p, "numsel", 1));
    }
    Node *ll = sx_find(lay, "layer");
    for (int i = 1; ll && i < ll->n; i++) layer_from(e, ll->k[i]);
    layers_sort(e);
    for (int k = 0; k < OBJ_KINDS; k++) obj_clear(e, k);
    Node *parts = sx_find(dlk, "parts");
    for (int i = 1; parts && i < parts->n; i++) obj_from(e, parts->k[i]);
    Node *pic = sx_find(dlk, "picture");
    Node *pn = get(pic, "picno");
    bg_set(e, pn && pn->t == N_INT ? pn->i : -1, 0);
    Node *fader = sx_find(dlk, "fader");
    int fa = geti(fader, "a", -1);
    if (fa > 0) overlay_set(e, (u32)geti(fader, "r", 0) | (u32)geti(fader, "g", 0) << 8 | (u32)geti(fader, "b", 0) << 16, fa, 0);
    else overlay_set(e, 0, 0, 0);
    Node *mus = sx_find(dlk, "music");
    for (int i = 0; i < 4; i++) {
        Node *m = mus && i + 1 < mus->n ? mus->k[i + 1] : NULL;
        Node *t = get(m, "music");
        e->chanGain[CH_MUSIC + i] = (float)geti(m, "volume", 100) / 100.0f;
        if (t && t->t == N_INT) snd_music(e, i, t->i, 0);
        else snd_stop(e, CH_MUSIC + i, 0);
    }
    snd_apply_volumes(e);
    Node *mw = sx_find(dlk, "msgwin");
    msg_select(e, geti(mw, "cfgno", 0));
    msg_show(e, getb(mw, "visible", false));
    const char *text = sx_str(get(mw, "text"));
    if (text && *text) {
        msg_append(e, text);
        e->msg.shown = e->msg.count;
    }
    Node *menu = sx_find(dlk, "menu");
    e->menu.show = getb(menu, "show", true);
    e->menu.autohide = getb(menu, "autohide", true);
    e->menu.enable = getb(menu, "enable", false);
    return true;
}

static Node *slot_find(Node *map, int slot) { return sx_assoc(map, slot); }

static Node *fms(BR *e) { return ensure(e->sysRoot, "flowmodestate"); }

Node *sx_copy(const Node *n) {
    Node *c = br_alloc(sizeof *c);
    c->t = n->t;
    c->op = n->op;
    c->i = n->i;
    c->s = n->s ? br_strdup(n->s) : NULL;
    if (n->n) {
        c->k = br_alloc(sizeof(Node *) * (size_t)n->n);
        for (int i = 0; i < n->n; i++) c->k[i] = sx_copy(n->k[i]);
        c->n = n->n;
    }
    return c;
}

static void replace(Node *list, const char *key, Node *with) {
    for (int i = 0; i < list->n; i++) {
        Node *k = list->k[i];
        if (k->t == N_LIST && k->n && sx_sym(k->k[0], key)) {
            sx_free(k);
            list->k[i] = with;
            return;
        }
    }
    add(list, with);
}

bool save_slot(BR *e, int slot, const char *comment) {
    Node *fresh = state_capture(e, true);
    if (e->gameSnap) {
        Node *dlk = sx_copy(sx_find(e->gameSnap, "dlkstate"));
        replace(dlk, "pls", sx_copy(sx_find(sx_find(fresh, "dlkstate"), "pls")));
        replace(fresh, "dlkstate", dlk);
        replace(fresh, "userstack", sx_copy(sx_find(e->gameSnap, "userstack")));
    }
    Node *fm = fms(e);
    Node *map = ensure(fm, "stateMap");
    Node *info = ensure(fm, "saveData");
    for (Node *m = map;; m = info) {
        for (int i = 1; i < m->n; i++)
            if (m->k[i]->t == N_LIST && m->k[i]->n && sx_int(m->k[i]->k[0], -1) == slot) {
                sx_free(m->k[i]);
                memmove(&m->k[i], &m->k[i + 1], sizeof(Node *) * (size_t)(m->n - i - 1));
                m->n--;
                break;
            }
        if (m == info) break;
    }
    Node *entry = add(map, nl());
    add(entry, ni(slot));
    for (int i = 0; i < fresh->n; i++) add(entry, fresh->k[i]);
    fresh->n = 0;
    sx_free(fresh);
    Node *si = add(info, nl());
    add(si, ni(slot));
    kvi(si, "routeID", -1);
    kvi(si, "stateID", slot);
    kv(si, "comment", ns(comment && *comment ? comment : "\x81\x69\x83\x52\x83\x81\x83\x93\x83\x67\x81\x6a"));
    kvi(si, "savetime", (i32)time(NULL));
    kvi(si, "sceneno", 0);
    e->slotShotDirty = true;
    return sys_save(e);
}

Node *slot_info(BR *e, int slot) { return slot_find(sx_find(sx_find(e->sysRoot, "flowmodestate"), "saveData"), slot); }

Node *slot_state(BR *e, int slot) {
    Node *s = slot_find(sx_find(sx_find(e->sysRoot, "flowmodestate"), "stateMap"), slot);
    if (!s) return NULL;
    Node *st = nl();
    for (int i = 1; i < s->n; i++) add(st, s->k[i]);
    return st;
}

void slot_state_release(Node *st) {
    if (!st) return;
    free(st->k);
    free(st);
}

bool slot_comment(BR *e, int slot, const char *comment) {
    Node *si = slot_info(e, slot);
    if (!si) return false;
    Node *c = sx_find(si, "comment");
    if (c && c->n >= 3) {
        free(c->k[2]->s);
        c->k[2]->s = br_strdup(comment);
    } else kv(si, "comment", ns(comment));
    return sys_save(e);
}

void seen_text(BR *e, int script, int line) {
    Node *rv = ensure(e->sysRoot, "resource-view-flag");
    int key = 10000 + script;
    Node *row = sx_assoc(rv, key);
    if (!row) {
        row = add(rv, nl());
        add(row, ni(key));
    }
    for (int i = 1; i < row->n; i++)
        if (row->k[i]->i == line) return;
    add(row, ni(line));
    e->sysDirty = true;
}

bool seen_text_has(BR *e, int script, int line) {
    Node *row = sx_assoc(sx_find(e->sysRoot, "resource-view-flag"), 10000 + script);
    for (int i = 1; row && i < row->n; i++)
        if (row->k[i]->i == line) return true;
    return false;
}

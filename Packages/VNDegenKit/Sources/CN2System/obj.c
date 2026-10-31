// The object tree. Scripts name objects by path ("box09img", "@選択肢１/MouseUsual/画像"): '/' separates a parent
// from its children, '@' looks a name up by alias, and '*' is a wildcard. A child's position is relative to its
// parent's; opacity is not inherited.
#include "n2.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void add_child(Obj *parent, Obj *o) {
    if (parent->childCount == parent->childCap) {
        parent->childCap = parent->childCap ? parent->childCap * 2 : 8;
        parent->children = n2_grow(parent->children, sizeof(Obj *) * (size_t)parent->childCap);
    }
    parent->children[parent->childCount++] = o;
    o->parent = parent;
}

static void remove_child(Obj *parent, Obj *o) {
    for (int i = 0; i < parent->childCount; i++) {
        if (parent->children[i] == o) {
            memmove(&parent->children[i], &parent->children[i + 1],
                    sizeof(Obj *) * (size_t)(parent->childCount - i - 1));
            parent->childCount--;
            return;
        }
    }
}

static bool part_matches(Obj *o, const char *part, bool byAlias) {
    if (byAlias) return o->alias ? n2_glob(part, o->alias) : n2_glob(part, o->name);
    return n2_glob(part, o->name) || (o->alias && n2_glob(part, o->alias));
}

static Obj *find_alias(Obj *o, const char *alias) {
    for (int i = 0; i < o->childCount; i++) {
        Obj *c = o->children[i];
        if ((c->alias && n2_streq_ci(c->alias, alias)) || n2_streq_ci(c->name, alias)) return c;
    }
    for (int i = 0; i < o->childCount; i++) {
        Obj *r = find_alias(o->children[i], alias);
        if (r) return r;
    }
    return NULL;
}

static Obj *find_child(Obj *parent, const char *name) {
    for (int i = 0; i < parent->childCount; i++)
        if (n2_streq_ci(parent->children[i]->name, name)) return parent->children[i];
    return NULL;
}

static Obj *new_obj(N2 *n, Obj *parent, const char *name, int kind, int prio) {
    Obj *o = n2_alloc(sizeof *o);
    o->kind = kind;
    o->name = n2_strdup(name);
    o->prio = prio;
    o->opacity = 1000;
    o->zoomX = o->zoomY = 1000;
    o->transition = -1;
    o->serial = ++n->objSerial;
    add_child(parent, o);
    return o;
}

Obj *obj_create(N2 *n, const char *path, int kind, int prio) {
    char *copy = n2_strdup(path);
    Obj *parent = n->curRoot;
    char *part = copy;
    for (;;) {
        char *slash = strchr(part, '/');
        if (!slash) break;
        *slash = 0;
        Obj *next;
        if (part[0] == '@') next = find_alias(parent, part + 1);
        else next = find_child(parent, part);
        if (!next && part[0] != '@') next = find_alias(parent, part);
        if (!next) next = new_obj(n, parent, part[0] == '@' ? part + 1 : part, OBJ_NAME, prio);
        parent = next;
        part = slash + 1;
    }
    const char *name = part[0] == '@' ? part + 1 : part;
    Obj *old = find_child(parent, name);
    if (old) obj_destroy(n, old);
    Obj *o = new_obj(n, parent, name, kind, prio);
    free(copy);
    n->dirty = true;
    return o;
}

void obj_destroy(N2 *n, Obj *o) {
    while (o->childCount) obj_destroy(n, o->children[o->childCount - 1]);
    if (o->parent) remove_child(o->parent, o);
    n2_forget_obj(n, o);
    if (o->thread) {
        thread_kill(n, o->thread);
        o->thread->owner = NULL;
        o->thread->textOwner = NULL;
    }
    if (o->text) text_free(o->text);
    if (o->sound) sound_free(n, o->sound);
    img_unref(o->image);
    for (int i = 0; i < ANIM_COUNT; i++) {
        free(o->anims[i].mask);
        img_unref(o->anims[i].maskImage);
    }
    free(o->children);
    free(o->name);
    free(o->alias);
    free(o->createCall);
    free(o->procTarget);
    free(o);
    n->dirty = true;
}

void obj_set_alias(N2 *n, Obj *o, const char *alias) {
    free(o->alias);
    o->alias = n2_strdup(alias[0] == '@' ? alias + 1 : alias);
}

// Matches one path part against the children of each object in `in`.
typedef struct Set {
    Obj **items;
    int count, cap;
} Set;

static void set_add(Set *s, Obj *o) {
    for (int i = 0; i < s->count; i++)
        if (s->items[i] == o) return;
    if (s->count == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 16;
        s->items = n2_grow(s->items, sizeof(Obj *) * (size_t)s->cap);
    }
    s->items[s->count++] = o;
}

static void collect_alias(Obj *o, const char *pattern, Set *out) {
    for (int i = 0; i < o->childCount; i++) {
        Obj *c = o->children[i];
        if (part_matches(c, pattern, true)) set_add(out, c);
        collect_alias(c, pattern, out);
    }
}

static Set resolve(N2 *n, const char *query) {
    Set cur = {0};
    Obj *root = n->curRoot;
    set_add(&cur, root);
    char *copy = n2_strdup(query);
    char *part = copy;
    bool first = true;
    while (part) {
        char *slash = strchr(part, '/');
        if (slash) *slash = 0;
        Set next = {0};
        const char *pat = part;
        bool byAlias = false;
        if (pat[0] == '<') pat++;  // '<' scopes a name to the main thread; there is one object tree
        if (pat[0] == '@') {
            pat++;
            byAlias = true;
        }
        size_t pl = strlen(pat);
        char *p2 = n2_strndup(pat, pl && pat[pl - 1] == '>' ? pl - 1 : pl);
        if (first && byAlias) {
            collect_alias(root, p2, &next);
        } else {
            for (int i = 0; i < cur.count; i++) {
                Obj *o = cur.items[i];
                for (int k = 0; k < o->childCount; k++)
                    if (part_matches(o->children[k], p2, byAlias)) set_add(&next, o->children[k]);
            }
            if (first && next.count == 0 && !strchr(p2, '*')) collect_alias(root, p2, &next);
        }
        free(p2);
        free(cur.items);
        cur = next;
        first = false;
        part = slash ? slash + 1 : NULL;
    }
    free(copy);
    return cur;
}

int obj_query(N2 *n, const char *query, ObjVisit visit, void *ctx) {
    if (!query || !query[0]) return 0;
    Set s = resolve(n, query);
    // Visiting may destroy objects, so check each is still in the tree first.
    int count = s.count;
    for (int i = 0; i < s.count; i++) visit(n, s.items[i], ctx);
    free(s.items);
    return count;
}

Obj *obj_find(N2 *n, const char *query) {
    if (!query || !query[0]) return NULL;
    Set s = resolve(n, query);
    Obj *o = s.count ? s.items[0] : NULL;
    free(s.items);
    return o;
}

void obj_screen_pos(Obj *o, double *x, double *y) {
    double ax = 0, ay = 0;
    for (Obj *p = o; p && p->parent; p = p->parent) {
        ax += p->x + p->shakeX;
        ay += p->y + p->shakeY;
    }
    *x = ax;
    *y = ay;
}

// ---- animation ----

double ease_apply(int ease, double t) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    switch (ease) {
    case 1: return t * t;
    case 2: return t * t * t;
    case 3: return t * t * t * t;
    case 4: return 1 - (1 - t) * (1 - t);
    case 5: return 1 - pow(1 - t, 3);
    case 6: return 1 - pow(1 - t, 4);
    case 7: return 1 - cos(t * M_PI / 2);
    case 8: return sin(t * M_PI / 2);
    case 9: return (1 - cos(t * M_PI)) / 2;
    case 10: return t < 0.5 ? sin(t * M_PI) / 2 : 1 - sin(t * M_PI) / 2;
    default: return t;
    }
}

int ease_from_name(const char *name) {
    static const char *names[] = {"", "Axl1", "Axl2", "Axl3", "Dxl1", "Dxl2", "Dxl3", "AxlAuto", "DxlAuto", "AxlDxl", "DxlAxl"};
    if (!name) return 0;
    for (int i = 1; i < 11; i++)
        if (n2_streq_ci(name, names[i])) return i;
    return 0;
}

static void anim_values(Obj *o, int which, double *v) {
    switch (which) {
    case ANIM_FADE: v[0] = o->opacity; break;
    case ANIM_MOVE: v[0] = o->x; v[1] = o->y; break;
    case ANIM_ZOOM: v[0] = o->zoomX; v[1] = o->zoomY; break;
    case ANIM_ROTATE: v[0] = o->rotX; v[1] = o->rotY; v[2] = o->rotZ; break;
    case ANIM_TRANSITION: v[0] = o->transition < 0 ? 0 : o->transition; break;
    default: break;
    }
}

static void anim_apply(N2 *n, Obj *o, int which, const double *v) {
    switch (which) {
    case ANIM_FADE: o->opacity = v[0]; break;
    case ANIM_MOVE: o->x = v[0]; o->y = v[1]; break;
    case ANIM_ZOOM: o->zoomX = v[0]; o->zoomY = v[1]; break;
    case ANIM_ROTATE: o->rotX = v[0]; o->rotY = v[1]; o->rotZ = v[2]; break;
    case ANIM_TRANSITION: o->transition = v[0]; break;
    case ANIM_SHAKE: break;
    case ANIM_VOLUME: break;
    }
    n->dirty = true;
}

void obj_start_anim(N2 *n, Obj *o, int which, u32 duration, const double *to, int ease) {
    Anim *a = &o->anims[which];
    a->active = false;
    anim_values(o, which, a->from);
    for (int i = 0; i < 3; i++) a->to[i] = to[i];
    a->start = n->now;
    a->duration = duration;
    a->ease = ease;
    if (duration == 0) {
        anim_apply(n, o, which, a->to);
        if (which == ANIM_TRANSITION) o->transition = -1;
        return;
    }
    a->active = true;
}

bool obj_anim_running(Obj *o, int which) { return o->anims[which].active; }

void obj_update_anims(N2 *n, Obj *o) {
    for (int w = 0; w < ANIM_COUNT; w++) {
        Anim *a = &o->anims[w];
        if (!a->active) continue;
        double t = a->duration ? (double)(n->now - a->start) / a->duration : 1;
        if (w == ANIM_SHAKE) {
            if (t >= 1) {
                o->shakeX = o->shakeY = 0;
                a->active = false;
            } else {
                // Alternate between the start and end offsets a few times a second, as a screen shake.
                double phase = (double)(n->now - a->start) / (a->to[2] > 0 ? a->to[2] : 50);
                int k = (int)phase;
                o->shakeX = (k & 1) ? a->to[0] : a->from[0];
                o->shakeY = (k & 1) ? a->to[1] : a->from[1];
            }
            n->dirty = true;
            continue;
        }
        if (w == ANIM_VOLUME) {
            if (t >= 1) a->active = false;
            continue;
        }
        double e = ease_apply(a->ease, t), v[3];
        for (int i = 0; i < 3; i++) v[i] = a->from[i] + (a->to[i] - a->from[i]) * e;
        anim_apply(n, o, w, v);
        if (t >= 1) {
            a->active = false;
            if (w == ANIM_TRANSITION) {
                // A finished DrawTransition leaves the object fully shown or hidden by its end value.
                o->opacity = a->to[0] >= 1000 ? 1000 : (a->to[0] <= 0 ? 0 : o->opacity);
                o->transition = -1;
                free(a->mask);
                a->mask = NULL;
                img_unref(a->maskImage);
                a->maskImage = NULL;
            }
        }
    }
}

// ---- buttons ----

enum { MOUSE_USUAL, MOUSE_OVER, MOUSE_CLICK };

static void bounds_of(Obj *o, double *x0, double *y0, double *x1, double *y1, bool *any) {
    if ((o->kind == OBJ_TEXTURE || o->kind == OBJ_COLOR || o->kind == OBJ_TEXT) && o->w > 0 && o->h > 0) {
        double x, y;
        obj_screen_pos(o, &x, &y);
        if (o->centerX) x = (N2_W - o->w) / 2.0;
        if (o->centerY) y = (N2_H - o->h) / 2.0;
        if (!*any || x < *x0) *x0 = x;
        if (!*any || y < *y0) *y0 = y;
        if (!*any || x + o->w > *x1) *x1 = x + o->w;
        if (!*any || y + o->h > *y1) *y1 = y + o->h;
        *any = true;
    }
    for (int i = 0; i < o->childCount; i++) bounds_of(o->children[i], x0, y0, x1, y1, any);
}

static Obj *state_node(Obj *choice, const char *name) {
    for (int i = 0; i < choice->childCount; i++)
        if (n2_streq_ci(choice->children[i]->name, name)) return choice->children[i];
    return NULL;
}

static bool hit(Obj *choice, int mx, int my) {
    Obj *usual = state_node(choice, "MouseUsual");
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool any = false;
    if (usual) bounds_of(usual, &x0, &y0, &x1, &y1, &any);
    if (!any) bounds_of(choice, &x0, &y0, &x1, &y1, &any);
    return any && mx >= x0 && mx < x1 && my >= y0 && my < y1;
}

static void start_state_children(N2 *n, Obj *node) {
    if (!node) return;
    for (int i = 0; i < node->childCount; i++) {
        Obj *c = node->children[i];
        if (c->kind == OBJ_PROCESS && c->thread && c->procTarget) {
            Chunk *fn = n2_find_function(n, c->procTarget);
            if (fn) {
                thread_kill(n, c->thread);
                thread_call(n, c->thread, fn, NULL, 0);
            }
        } else if (c->kind == OBJ_SOUND && c->sound) {
            sound_play(n, c->sound);
        }
    }
}

static void update_choice(N2 *n, Obj *o, bool *consumed) {
    for (int i = 0; i < o->childCount; i++) update_choice(n, o->children[i], consumed);
    if (o->kind != OBJ_CHOICE || o->erased) return;
    bool over = !o->unclickable && hit(o, n->mouseX, n->mouseY);
    int state = o->mouseState;
    if (over) {
        if (n->mousePressed[VNN2_MOUSE_LEFT] && !*consumed) state = MOUSE_CLICK;
        else if (state != MOUSE_CLICK) state = MOUSE_OVER;
        if (state == MOUSE_CLICK && n->mouseReleased[VNN2_MOUSE_LEFT] && !*consumed) {
            map_set(&n->pressedButtons, o->name, (void *)1);
            if (o->alias) map_set(&n->pressedButtons, o->alias, (void *)1);
            *consumed = true;
            state = MOUSE_OVER;
        }
        if (n->mousePressed[VNN2_MOUSE_LEFT]) *consumed = true;
    } else {
        state = MOUSE_USUAL;
    }
    if (state != o->mouseState) {
        if (state == MOUSE_OVER && o->mouseState == MOUSE_USUAL) start_state_children(n, state_node(o, "MouseOver"));
        if (state == MOUSE_USUAL && o->mouseState != MOUSE_USUAL) start_state_children(n, state_node(o, "MouseLeave"));
        if (state == MOUSE_CLICK) start_state_children(n, state_node(o, "MouseClick"));
        o->mouseState = state;
        n->dirty = true;
    }
}

void buttons_update(N2 *n) {
    // Presses stay recorded until a select pass reads them or the next click, so a quick click is never lost.
    if (n->mousePressed[VNN2_MOUSE_LEFT]) map_free(&n->pressedButtons, NULL);
    bool consumed = false;
    // System script buttons sit above the game's and take the click first.
    update_choice(n, &n->sysRoot, &consumed);
    update_choice(n, &n->root, &consumed);
    n->clickOnButton = consumed;
}

bool obj_state_visible(Obj *o) {
    // Children of a button's MouseUsual, MouseOver and MouseClick nodes show only in that state; a missing state
    // shows the MouseUsual one.
    Obj *p = o->parent;
    if (!p || !p->parent || p->parent->kind != OBJ_CHOICE) return true;
    Obj *choice = p->parent;
    const char *want = choice->mouseState == MOUSE_CLICK ? "MouseClick" : choice->mouseState == MOUSE_OVER ? "MouseOver" : "MouseUsual";
    if (!state_node(choice, want)) want = choice->mouseState == MOUSE_CLICK && state_node(choice, "MouseOver") ? "MouseOver" : "MouseUsual";
    if (n2_streq_ci(p->name, "MouseLeave")) return false;
    return n2_streq_ci(p->name, want);
}

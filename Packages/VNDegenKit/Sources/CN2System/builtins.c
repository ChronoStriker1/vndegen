// The built-in functions the scripts call. Argument lists follow NitroSharp's dispatcher (MIT) and Saya's scripts.
// Only what the test games call is here; anything else logs once as an unknown function.
#include "n2.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// ---- arguments ----

static const Value *arg(Value *a, int argc, int i) {
    static Value none = {V_NULL};
    return i < argc ? &a[i] : &none;
}

static const char *astr(Value *a, int argc, int i) {
    static char tmp[8][64];
    static int k;
    k = (k + 1) & 7;
    return v_tostr(arg(a, argc, i), tmp[k], sizeof tmp[k]);
}

static double anum(Value *a, int argc, int i) { return v_tonum(arg(a, argc, i)); }

static bool abool(Value *a, int argc, int i) { return v_truthy(arg(a, argc, i)); }

static bool is_null(Value *a, int argc, int i) { return arg(a, argc, i)->t == V_NULL; }

static bool aconst(Value *a, int argc, int i, const char *name) {
    const Value *v = arg(a, argc, i);
    return v->t == V_STR && n2_streq_ci(v->s->s, name);
}

// A coordinate: a number, an '@' delta from `cur`, or Center and Middle, which set *center.
static double acoord(Value *a, int argc, int i, double cur, bool *center) {
    const Value *v = arg(a, argc, i);
    if (center) *center = false;
    if (v->t == V_STR) {
        const char *s = v->s->s;
        if (n2_streq_ci(s, "center") || n2_streq_ci(s, "middle") || n2_streq_ci(s, "inherit")) {
            if (center) *center = true;
            return 0;
        }
        if (s[0] == '@') return cur + atof(s + 1);
    }
    double x = v_tonum(v);
    return v->delta ? cur + x : x;
}

// A color: a number (0xRRGGBB), "#RRGGBB", or a name.
static u32 acolor(Value *a, int argc, int i) {
    const Value *v = arg(a, argc, i);
    if (v->t == V_STR) {
        const char *s = v->s->s;
        static const struct {
            const char *name;
            u32 rgb;
        } names[] = {{"black", 0}, {"white", 0xffffff}, {"gray", 0x808080}, {"red", 0xff0000},
                     {"green", 0x00ff00}, {"blue", 0x0000ff}, {"yellow", 0xffff00}};
        for (size_t k = 0; k < sizeof names / sizeof names[0]; k++)
            if (n2_streq_ci(s, names[k].name)) return names[k].rgb;
    }
    return (u32)v_tonum(v) & 0xffffff;
}

// The wait argument of Fade and friends: true or 1 waits for the whole animation, a larger number waits that long.
static u32 await_ms(Value *a, int argc, int i, u32 duration, bool *wait) {
    const Value *v = arg(a, argc, i);
    *wait = false;
    if (v->t == V_NULL) return 0;
    if (v->t == V_BOOL || (v->t == V_STR && (n2_streq_ci(v->s->s, "true") || n2_streq_ci(v->s->s, "false")))) {
        *wait = v_truthy(v);
        return duration;
    }
    double x = v_tonum(v);
    if (x == 1) {
        *wait = true;
        return duration;
    }
    if (x > 1) {
        *wait = true;
        return (u32)x;
    }
    return 0;
}

static int aease(Value *a, int argc, int i) {
    const Value *v = arg(a, argc, i);
    return v->t == V_STR ? ease_from_name(v->s->s) : 0;
}

static void set_var_num(N2 *n, const char *name, double x) {
    Value v = v_num(x);
    var_set(n, name, &v);
}

static bool wait_anim(N2 *n, Thread *t, const char *query, int anim, u32 timeout) {
    free(t->waitQuery);
    t->waitQuery = n2_strdup(query);
    t->waitAnim = anim;
    t->waitUntil = timeout ? n->now + timeout : 0;
    t->wait = WAIT_ANIM;
    return true;
}

// Text a call creates an object with, which frames.npf keeps.
static void remember_call(Obj *o, const char *fn, Value *a, int argc) {
    Buf b = {0};
    buf_str(&b, fn);
    buf_u8(&b, '(');
    for (int i = 0; i < argc; i++) {
        char tmp[64];
        if (i) buf_u8(&b, ',');
        if (a[i].t == V_STR) {
            buf_u8(&b, '"');
            buf_str(&b, a[i].s->s);
            buf_u8(&b, '"');
        } else if (a[i].t == V_NULL) {
            buf_str(&b, "null");
        } else {
            buf_str(&b, v_tostr(&a[i], tmp, sizeof tmp));
        }
    }
    buf_str(&b, ");");
    buf_u8(&b, 0);
    free(o->createCall);
    o->createCall = (char *)b.data;
}

// ---- images ----

static Image *capture_screen(N2 *n) {
    n2_render(n);
    Image *img = img_new(N2_W, N2_H);
    memcpy(img->px, n->screen, sizeof(u32) * N2_W * N2_H);
    return img;
}

Image *n2_load_image(N2 *n, const char *src) {
    Image *named = map_get(&n->images, src);
    if (named) return img_ref(named);
    Image *cached = map_get(&n->imageCache, src);
    if (cached) return img_ref(cached);
    size_t size = 0;
    u8 *data = NULL;
    size_t sl = strlen(src);
    if (sl > 4 && n2_streq_ci(src + sl - 4, ".npf")) {
        // A save thumbnail, by its path under #SYSTEM_save_path.
        Image *img = save_read_thumbnail(n, src);
        return img;
    }
    data = fs_read(&n->fs, src, &size);
    if (!data) {
        static const char *exts[] = {".png", ".jpg", ".bmp"};
        for (int i = 0; i < 3 && !data; i++) {
            char *p = n2_fmt("%s%s", src, exts[i]);
            data = fs_read(&n->fs, p, &size);
            free(p);
        }
    }
    if (!data) {
        n2_log(n, "missing image %s", src);
        return NULL;
    }
    Image *img = img_decode(data, size);
    free(data);
    if (!img) {
        n2_log(n, "can't decode %s", src);
        return NULL;
    }
    // Keep the system images; story CG is large and rarely reused within a scene.
    if (strncasecmp(src, "cg/sys/", 7) == 0 && n->imageCache.count < 400) map_set(&n->imageCache, src, img_ref(img));
    return img;
}

// ---- object creation ----

static bool bi_CreateTexture(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    const char *src = astr(a, argc, 4);
    Image *img;
    if (n2_streq_ci(src, "VIDEO") || n2_streq_ci(src, "SCREEN")) img = capture_screen(n);
    else img = n2_load_image(n, src);
    if (!img) {
        Obj *other = obj_find(n, src);
        if (other && other->image) img = img_ref(other->image);
    }
    Obj *o = obj_create(n, astr(a, argc, 0), OBJ_TEXTURE, (int)anum(a, argc, 1));
    remember_call(o, "CreateTexture", a, argc);
    o->x = acoord(a, argc, 2, 0, &o->centerX);
    o->y = acoord(a, argc, 3, 0, &o->centerY);
    o->image = img;
    if (img) {
        o->w = img->w;
        o->h = img->h;
    }
    return false;
}

static bool bi_CreateColor(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_create(n, astr(a, argc, 0), OBJ_COLOR, (int)anum(a, argc, 1));
    remember_call(o, "CreateColor", a, argc);
    o->x = acoord(a, argc, 2, 0, &o->centerX);
    o->y = acoord(a, argc, 3, 0, &o->centerY);
    o->w = (int)anum(a, argc, 4);
    o->h = (int)anum(a, argc, 5);
    o->color = acolor(a, argc, argc == 8 ? 7 : 6);
    return false;
}

static bool bi_CreateWindow(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_create(n, astr(a, argc, 0), OBJ_WINDOW, (int)anum(a, argc, 1));
    remember_call(o, "CreateWindow", a, argc);
    o->x = acoord(a, argc, 2, 0, &o->centerX);
    o->y = acoord(a, argc, 3, 0, &o->centerY);
    o->w = (int)anum(a, argc, 4);
    o->h = (int)anum(a, argc, 5);
    return false;
}

static bool bi_CreateText(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_create(n, astr(a, argc, 0), OBJ_TEXT, (int)anum(a, argc, 1));
    remember_call(o, "CreateText", a, argc);
    o->x = acoord(a, argc, 2, 0, &o->centerX);
    o->y = acoord(a, argc, 3, 0, &o->centerY);
    bool autoW = aconst(a, argc, 4, "auto"), autoH = aconst(a, argc, 5, "auto");
    int w = autoW ? 0 : (int)anum(a, argc, 4), h = autoH ? 0 : (int)anum(a, argc, 5);
    o->text = text_new(n, o, astr(a, argc, 6), w, h, autoW);
    return false;
}

static bool bi_LoadText(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    const char *boxName = astr(a, argc, 1), *textName = astr(a, argc, 2);
    TextBlock *b = NULL;
    Script *s = n->activeBlockScript;
    if (s) b = script_block(s, textName);
    if (!b) {
        n2_log(n, "LoadText: no block %s", textName);
        return false;
    }
    Obj *box = obj_find(n, boxName);
    char *path = n2_fmt("%s/%s", boxName, b->name);
    Obj *o = obj_create(n, path, OBJ_TEXT, box ? box->prio : 0);
    free(path);
    remember_call(o, "LoadText", a, argc);
    obj_set_alias(n, o, b->name);
    text_start_block(n, o, b, s, (int)anum(a, argc, 3), (int)anum(a, argc, 4), (int)anum(a, argc, 5),
                     (int)anum(a, argc, 6));
    return false;
}

static bool bi_CreateChoice(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_create(n, astr(a, argc, 0), OBJ_CHOICE, 0);
    remember_call(o, "CreateChoice", a, argc);
    return false;
}

static bool bi_CreateSound(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    const char *kind = astr(a, argc, 1);
    int k = n2_streq_ci(kind, "BGM") ? 0 : n2_streq_ci(kind, "Voice") ? 2 : 1;
    Obj *o = obj_create(n, astr(a, argc, 0), OBJ_SOUND, 0);
    remember_call(o, "CreateSound", a, argc);
    o->sound = sound_load(n, astr(a, argc, 2), k);
    return false;
}

static bool bi_CreateProcess(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_create(n, astr(a, argc, 0), OBJ_PROCESS, (int)anum(a, argc, 1));
    remember_call(o, "CreateProcess", a, argc);
    o->procTarget = n2_strdup(astr(a, argc, 4));
    o->thread = thread_new(n, o->name);
    o->thread->owner = o;
    o->thread->wait = WAIT_DONE;  // until Request(Start) or a button state starts it
    return false;
}

static bool bi_CreateScrollbar(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_create(n, astr(a, argc, 0), OBJ_SCROLLBAR, (int)anum(a, argc, 1));
    remember_call(o, "CreateScrollbar", a, argc);
    o->sbX1 = (int)anum(a, argc, 2);
    o->sbY1 = (int)anum(a, argc, 3);
    o->sbX2 = (int)anum(a, argc, 4);
    o->sbY2 = (int)anum(a, argc, 5);
    o->sbValue = anum(a, argc, 6);
    if (o->sbValue > 1) o->sbValue /= 1000;
    o->sbVertical = aconst(a, argc, 7, "VERTICAL");
    o->image = n2_load_image(n, astr(a, argc, 8));
    if (o->image) {
        o->w = o->image->w;
        o->h = o->image->h;
    }
    return false;
}

static bool bi_CreateBacklog(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_create(n, astr(a, argc, 0), OBJ_BACKLOG, (int)anum(a, argc, 1));
    remember_call(o, "CreateBacklog", a, argc);
    backlog_attach(n, o);
    return false;
}

static bool bi_CreateName(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    obj_create(n, astr(a, argc, 0), OBJ_NAME, 0);
    return false;
}

static bool bi_CreateEffect(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    // Effect layers ("Plain") composite their children; drawing them as a plain group is what the test game needs.
    Obj *o = obj_create(n, astr(a, argc, 0), OBJ_NAME, (int)anum(a, argc, 1));
    remember_call(o, "CreateEffect", a, argc);
    return false;
}

static bool bi_LoadImage(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    const char *name = astr(a, argc, 0);
    Image *img = n2_load_image(n, astr(a, argc, 1));
    Image *old = map_get(&n->images, name);
    if (old) img_unref(old);
    map_set(&n->images, name, img);
    return false;
}

// ---- object control ----

static void del_visit(N2 *n, Obj *o, void *ctx) {
    if (o->locked) return;
    obj_destroy(n, o);
}

typedef struct Collect {
    Obj **list;
    int *count;
} Collect;

static void collect(N2 *n, Obj *o, void *ctx) {
    Collect *c = ctx;
    if (*c->count < 1024) c->list[(*c->count)++] = o;
}

static bool bi_Delete(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    const char *q = astr(a, argc, 0);
    // Images loaded by name go too ("select_img*").
    for (int i = 0; i < n->images.cap; i++) {
        MapEntry *e = &n->images.entries[i];
        if (e->key && e->value && n2_glob(q[0] == '@' ? q + 1 : q, e->key)) {
            img_unref(e->value);
            e->value = NULL;
        }
    }
    // Destroying a parent frees its children, so collect matches first and skip ones that vanished.
    Obj *list[1024];
    int count = 0;
    Collect c = {list, &count};
    obj_query(n, q, collect, &c);
    for (int i = 0; i < count; i++) {
        if (n2_obj_alive(n, list[i])) del_visit(n, list[i], NULL);
    }
    return false;
}

static void alias_visit(N2 *n, Obj *o, void *ctx) { obj_set_alias(n, o, ctx); }

static bool bi_SetAlias(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    obj_query(n, astr(a, argc, 0), alias_visit, (void *)astr(a, argc, 1));
    return false;
}

static void request_visit(N2 *n, Obj *o, void *ctx) {
    const char *act = ctx;
    if (n2_streq_ci(act, "Lock")) o->locked = true;
    else if (n2_streq_ci(act, "UnLock")) o->locked = false;
    else if (n2_streq_ci(act, "Hideable")) o->hideable = true;
    else if (n2_streq_ci(act, "Erase")) o->erased = true;
    else if (n2_streq_ci(act, "Enter")) {
        o->erased = false;
        if (o->text) text_enter(n, o);
    } else if (n2_streq_ci(act, "Disused")) {
        if (!o->locked) {
            if (o->sound && sound_playing(o->sound)) o->disused = true;  // freed when it stops
            else obj_destroy(n, o);
        }
    } else if (n2_streq_ci(act, "Start")) {
        if (o->kind == OBJ_PROCESS && o->thread && o->procTarget) {
            Chunk *fn = n2_find_function(n, o->procTarget);
            if (fn) {
                thread_kill(n, o->thread);
                o->thread->paused = false;
                thread_call(n, o->thread, fn, NULL, 0);
            } else {
                n2_log(n, "process %s: no function %s", o->name, o->procTarget);
            }
        }
    } else if (n2_streq_ci(act, "Stop")) {
        if (o->thread) thread_kill(n, o->thread);
        if (o->sound) sound_stop(n, o->sound);
    } else if (n2_streq_ci(act, "Pause")) {
        if (o->thread) o->thread->paused = true;
    } else if (n2_streq_ci(act, "Resume")) {
        if (o->thread) o->thread->paused = false;
    } else if (n2_streq_ci(act, "Play")) {
        if (o->sound) sound_play(n, o->sound);
    } else if (n2_streq_ci(act, "Smoothing")) o->smoothing = true;
    else if (n2_streq_ci(act, "PushText")) o->pushText = true;
    else if (n2_streq_ci(act, "NoLog")) o->noLog = true;
    else if (n2_streq_ci(act, "UnClickable")) o->unclickable = true;
    else if (n2_streq_ci(act, "Clickable")) o->unclickable = false;
    else if (n2_streq_ci(act, "AddRender")) o->blend = 1;
    else if (n2_streq_ci(act, "SubRender")) o->blend = 2;
    else if (n2_streq_ci(act, "MulRender")) o->blend = 3;
    n->dirty = true;
}

static bool bi_Request(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    const char *q = astr(a, argc, 0);
    char act[64];
    snprintf(act, sizeof act, "%s", astr(a, argc, 1));
    if (n2_streq_ci(act, "Disused")) {
        Obj *list[1024];
        int count = 0;
        Collect c = {list, &count};
        obj_query(n, q, collect, &c);
        for (int i = 0; i < count; i++)
            if (n2_obj_alive(n, list[i])) request_visit(n, list[i], act);
        return false;
    }
    obj_query(n, q, request_visit, act);
    return false;
}

typedef struct AnimArgs {
    int which;
    u32 duration;
    Value *a;
    int argc;
    int ease;
} AnimArgs;

static void fade_visit(N2 *n, Obj *o, void *ctx) {
    AnimArgs *x = ctx;
    double to[3] = {anum(x->a, x->argc, x->argc == 3 ? 1 : 2), 0, 0};
    if (to[0] < 0) to[0] = 0;
    if (to[0] > 1000) to[0] = 1000;
    obj_start_anim(n, o, ANIM_FADE, x->duration, to, x->ease);
}

static bool bi_Fade(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    const char *q = astr(a, argc, 0);
    if (argc == 3) {
        AnimArgs x = {ANIM_FADE, 0, a, argc, 0};
        obj_query(n, q, fade_visit, &x);
        return false;
    }
    u32 dur = (u32)fmax(0, anum(a, argc, 1));
    if (n2_skip_active(n)) dur = 0;
    // The ease and wait arguments come in either order.
    int ease = 0;
    bool wait = false;
    u32 waitMs = 0;
    if (argc >= 5) {
        const Value *v3 = arg(a, argc, 3);
        if (v3->t == V_STR && ease_from_name(v3->s->s)) {
            ease = ease_from_name(v3->s->s);
            waitMs = await_ms(a, argc, 4, dur, &wait);
        } else if (v3->t == V_NULL) {
            waitMs = await_ms(a, argc, 4, dur, &wait);
        } else {
            waitMs = await_ms(a, argc, 3, dur, &wait);
            ease = aease(a, argc, 4);
        }
    } else if (argc == 4) {
        waitMs = await_ms(a, argc, 3, dur, &wait);
    }
    AnimArgs x = {ANIM_FADE, dur, a, argc, ease};
    obj_query(n, q, fade_visit, &x);
    if (wait && dur) return wait_anim(n, t, q, ANIM_FADE, waitMs == dur ? 0 : waitMs);
    return false;
}

static void move_visit(N2 *n, Obj *o, void *ctx) {
    AnimArgs *x = ctx;
    bool c;
    double to[3] = {acoord(x->a, x->argc, 2, o->x, &c), acoord(x->a, x->argc, 3, o->y, &c), 0};
    obj_start_anim(n, o, ANIM_MOVE, x->duration, to, x->ease);
}

static bool bi_Move(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    const char *q = astr(a, argc, 0);
    u32 dur = (u32)fmax(0, anum(a, argc, 1));
    if (n2_skip_active(n)) dur = 0;
    bool wait = false;
    u32 waitMs = argc >= 6 ? await_ms(a, argc, 5, dur, &wait) : 0;
    AnimArgs x = {ANIM_MOVE, dur, a, argc, aease(a, argc, 4)};
    obj_query(n, q, move_visit, &x);
    if (wait && dur) return wait_anim(n, t, q, ANIM_MOVE, waitMs == dur ? 0 : waitMs);
    return false;
}

static void zoom_visit(N2 *n, Obj *o, void *ctx) {
    AnimArgs *x = ctx;
    bool c;
    double to[3] = {acoord(x->a, x->argc, 2, o->zoomX, &c), acoord(x->a, x->argc, 3, o->zoomY, &c), 0};
    obj_start_anim(n, o, ANIM_ZOOM, x->duration, to, x->ease);
}

static bool bi_Zoom(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    const char *q = astr(a, argc, 0);
    u32 dur = (u32)fmax(0, anum(a, argc, 1));
    if (n2_skip_active(n)) dur = 0;
    bool wait = false;
    u32 waitMs = await_ms(a, argc, 5, dur, &wait);
    AnimArgs x = {ANIM_ZOOM, dur, a, argc, aease(a, argc, 4)};
    obj_query(n, q, zoom_visit, &x);
    if (wait && dur) return wait_anim(n, t, q, ANIM_ZOOM, waitMs == dur ? 0 : waitMs);
    return false;
}

static void rotate_visit(N2 *n, Obj *o, void *ctx) {
    AnimArgs *x = ctx;
    bool c;
    double to[3] = {acoord(x->a, x->argc, 2, o->rotX, &c), acoord(x->a, x->argc, 3, o->rotY, &c),
                    acoord(x->a, x->argc, 4, o->rotZ, &c)};
    obj_start_anim(n, o, ANIM_ROTATE, x->duration, to, x->ease);
}

static bool bi_Rotate(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    const char *q = astr(a, argc, 0);
    u32 dur = (u32)fmax(0, anum(a, argc, 1));
    if (n2_skip_active(n)) dur = 0;
    bool wait = false;
    u32 waitMs = await_ms(a, argc, 6, dur, &wait);
    AnimArgs x = {ANIM_ROTATE, dur, a, argc, aease(a, argc, 5)};
    obj_query(n, q, rotate_visit, &x);
    if (wait && dur) return wait_anim(n, t, q, ANIM_ROTATE, waitMs == dur ? 0 : waitMs);
    return false;
}

static void shake_visit(N2 *n, Obj *o, void *ctx) {
    AnimArgs *x = ctx;
    Anim *an = &o->anims[ANIM_SHAKE];
    double freq = anum(x->a, x->argc, 6);
    an->from[0] = anum(x->a, x->argc, 2);
    an->from[1] = anum(x->a, x->argc, 3);
    an->to[0] = -an->from[0];
    an->to[1] = -an->from[1];
    an->to[2] = freq > 0 ? 500.0 / freq : 50;  // half a period in milliseconds
    an->start = n->now;
    an->duration = x->duration;
    an->active = x->duration > 0;
}

static bool bi_Shake(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    const char *q = astr(a, argc, 0);
    u32 dur = (u32)fmax(0, anum(a, argc, 1));
    if (n2_skip_active(n)) dur = 0;
    bool wait = false;
    await_ms(a, argc, argc - 1, dur, &wait);
    AnimArgs x = {ANIM_SHAKE, dur, a, argc, 0};
    obj_query(n, q, shake_visit, &x);
    if (wait && dur) return wait_anim(n, t, q, ANIM_SHAKE, 0);
    return false;
}

static void transition_visit(N2 *n, Obj *o, void *ctx) {
    AnimArgs *x = ctx;
    Anim *an = &o->anims[ANIM_TRANSITION];
    free(an->mask);
    img_unref(an->maskImage);
    an->mask = n2_strdup(astr(x->a, x->argc, 6));
    an->maskImage = n2_load_image(n, an->mask);
    an->feather = anum(x->a, x->argc, 4);
    o->transition = anum(x->a, x->argc, 2);
    double to[3] = {anum(x->a, x->argc, 3), 0, 0};
    // The object shows through the mask; DrawTransition owns its opacity until it ends.
    o->opacity = 1000;
    obj_start_anim(n, o, ANIM_TRANSITION, x->duration, to, x->ease);
}

static bool bi_DrawTransition(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    const char *q = astr(a, argc, 0);
    u32 dur = (u32)fmax(0, anum(a, argc, 1));
    if (n2_skip_active(n)) dur = 0;
    bool wait = false;
    await_ms(a, argc, 7, dur, &wait);
    AnimArgs x = {ANIM_TRANSITION, dur, a, argc, aease(a, argc, 5)};
    obj_query(n, q, transition_visit, &x);
    if (wait && dur) return wait_anim(n, t, q, ANIM_TRANSITION, 0);
    return false;
}

static bool bi_WaitFade(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    return wait_anim(n, t, astr(a, argc, 0), ANIM_FADE, 0);
}

static bool bi_WaitMove(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    return wait_anim(n, t, astr(a, argc, 0), ANIM_MOVE, 0);
}

static bool bi_WaitAction(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    return wait_anim(n, t, astr(a, argc, 0), -1, is_null(a, argc, 1) ? 0 : (u32)anum(a, argc, 1));
}

static void shade_visit(N2 *n, Obj *o, void *ctx) {
    const char *level = ctx;
    if (!o->image) return;
    int radius = n2_streq_ci(level, "HEAVY") ? 8 : n2_streq_ci(level, "SEMIHEAVY") ? 6 :
                 n2_streq_ci(level, "MEDIUM") ? 4 : n2_streq_ci(level, "LIGHT") || n2_streq_ci(level, "SEMILIGHT") ? 2 : 0;
    if (!radius) return;
    Image *b = img_blur(o->image, radius);
    img_unref(o->image);
    o->image = b;
    n->dirty = true;
}

static bool bi_SetShade(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    obj_query(n, astr(a, argc, 0), shade_visit, (void *)astr(a, argc, 1));
    return false;
}

static bool bi_SetNextFocus(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    // Keyboard focus between buttons. VNDegen drives buttons with the pointer, which every input method moves.
    return false;
}

// ---- waits ----

static bool bi_Wait(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    u32 ms = (u32)fmax(0, anum(a, argc, 0));
    t->waitUntil = n->now + ms;
    t->wait = WAIT_TIME;
    return true;
}

static bool bi_WaitKey(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    t->waitUntil = argc > 0 && !is_null(a, argc, 0) ? n->now + (u32)fmax(1, anum(a, argc, 0)) : 0;
    t->wait = WAIT_KEY;
    return true;
}

static bool bi_WaitText(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_find(n, astr(a, argc, 0));
    if (!o || !o->text) return false;
    t->waitObj = o;
    t->wait = WAIT_TEXT;
    return true;
}

static bool bi_Draw(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    t->wait = WAIT_FRAME;
    return true;
}

// ---- text ----

static bool bi_SetFont(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    snprintf(n->fontFace, sizeof n->fontFace, "%s", astr(a, argc, 0));
    n->fontSize = (int)anum(a, argc, 1);
    n->fontColor = acolor(a, argc, 2);
    n->fontOutline = acolor(a, argc, 3);
    n->fontWeight = (int)anum(a, argc, 4);
    free(n->outlineMode);
    n->outlineMode = n2_strdup(astr(a, argc, 5));
    return false;
}

static bool bi_LoadFont(N2 *n, Thread *t, Value *a, int argc, Value *ret) { return false; }

// ---- sound ----

typedef struct VolArgs {
    u32 duration;
    double volume;
} VolArgs;

static void volume_visit(N2 *n, Obj *o, void *ctx) {
    VolArgs *v = ctx;
    if (o->sound) sound_set_volume(n, o->sound, v->volume, v->duration);
}

static bool bi_SetVolume(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    VolArgs v = {(u32)fmax(0, anum(a, argc, 1)), anum(a, argc, 2)};
    obj_query(n, astr(a, argc, 0), volume_visit, &v);
    return false;
}

static void loop_visit(N2 *n, Obj *o, void *ctx) {
    if (o->sound) sound_set_loop(n, o->sound, ctx != NULL);
}

static bool bi_SetLoop(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    obj_query(n, astr(a, argc, 0), loop_visit, abool(a, argc, 1) ? (void *)1 : NULL);
    return false;
}

static bool bi_SetLoopPoint(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_find(n, astr(a, argc, 0));
    if (o && o->sound) sound_set_loop_points(n, o->sound, (u32)anum(a, argc, 1), (u32)anum(a, argc, 2));
    return false;
}

static bool bi_RemainTime(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_find(n, astr(a, argc, 0));
    double r = 0;
    if (o && o->sound && sound_playing(o->sound)) {
        u32 d = sound_duration_ms(o->sound), p = sound_position_ms(o->sound);
        r = d > p ? d - p : 0;
    }
    *ret = v_num(r);
    return false;
}

static bool bi_DurationTime(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_find(n, astr(a, argc, 0));
    *ret = v_num(o && o->sound ? sound_duration_ms(o->sound) : 0);
    return false;
}

// ---- object queries ----

static bool bi_Position(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_find(n, astr(a, argc, 0));
    double x = 0, y = 0;
    if (o) {
        x = o->x;
        y = o->y;
        if (o->kind == OBJ_SCROLLBAR) {
            x = o->sbX1 + (o->sbX2 - o->sbX1) * o->sbValue;
            y = o->sbY1 + (o->sbY2 - o->sbY1) * o->sbValue;
        }
    }
    set_var_num(n, astr(a, argc, 1), x);
    set_var_num(n, astr(a, argc, 2), y);
    return false;
}

static bool bi_ImageHorizon(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_find(n, astr(a, argc, 0));
    Image *img = o ? o->image : map_get(&n->images, astr(a, argc, 0));
    *ret = v_num(o ? o->w : img ? img->w : 0);
    return false;
}

static bool bi_ImageVertical(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_find(n, astr(a, argc, 0));
    Image *img = o ? o->image : map_get(&n->images, astr(a, argc, 0));
    *ret = v_num(o ? o->h : img ? img->h : 0);
    return false;
}

static bool bi_ScrollbarValue(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Obj *o = obj_find(n, astr(a, argc, 0));
    *ret = v_num(o && o->kind == OBJ_SCROLLBAR ? o->sbValue : 0);
    return false;
}

static bool bi_SetScrollbar(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    backlog_set_scrollbar(n, obj_find(n, astr(a, argc, 0)), obj_find(n, astr(a, argc, 1)));
    return false;
}

static bool bi_noop(N2 *n, Thread *t, Value *a, int argc, Value *ret) { return false; }

static bool bi_true(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    *ret = v_bool(true);
    return false;
}

// ---- backlog ----

static bool bi_SetBacklog(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    backlog_add(n, astr(a, argc, 0), is_null(a, argc, 1) ? NULL : astr(a, argc, 1));
    return false;
}

static bool bi_ClearBacklog(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    backlog_clear(n);
    return false;
}

static bool bi_EnableBacklog(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    *ret = v_bool(backlog_count(n) > 0);
    return false;
}

// ---- saves and files ----

static bool bi_Save(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    save_slot(n, (int)anum(a, argc, 0));
    return false;
}

static bool bi_Load(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    // Loading replaces every thread, including this one, so it happens after the current pass.
    n->pendingLoad = (int)anum(a, argc, 0);
    t->wait = WAIT_FRAME;
    return true;
}

static bool bi_ExistSave(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    *ret = v_bool(save_exists(n, (int)anum(a, argc, 0)));
    return false;
}

static bool bi_DeleteSaveFile(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    save_delete(n, (int)anum(a, argc, 0));
    return false;
}

static bool bi_ReadFile(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    size_t size;
    u8 *data = save_read_file(n, astr(a, argc, 0), &size);
    if (data) {
        char *u = n2_sjis_to_utf8(data, size, NULL);
        *ret = v_str(u);
        free(u);
        free(data);
    } else {
        *ret = v_str("");
    }
    return false;
}

static bool bi_WriteFile(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    size_t len;
    char *sj = n2_utf8_to_sjis(astr(a, argc, 1), &len);
    save_write_file(n, astr(a, argc, 0), (u8 *)sj, len);
    free(sj);
    return false;
}

// ---- values ----

static bool bi_String(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    // printf-style with %d, %0Nd, %s and %%.
    const char *f = astr(a, argc, 0);
    Buf b = {0};
    int ai = 1;
    for (const char *p = f; *p; p++) {
        if (*p != '%') {
            buf_u8(&b, (u8)*p);
            continue;
        }
        if (p[1] == '%') {
            buf_u8(&b, '%');
            p++;
            continue;
        }
        char spec[16];
        int k = 0;
        spec[k++] = '%';
        p++;
        while (*p && strchr("0123456789-+. ", *p) && k < 12) spec[k++] = *p++;
        char conv = *p;
        if (!conv) break;
        char out[256];
        if (conv == 'd' || conv == 'i' || conv == 'x' || conv == 'X') {
            spec[k++] = 'l';
            spec[k++] = 'l';
            spec[k++] = conv;
            spec[k] = 0;
            snprintf(out, sizeof out, spec, (long long)anum(a, argc, ai++));
        } else {
            spec[k++] = 's';
            spec[k] = 0;
            snprintf(out, sizeof out, spec, astr(a, argc, ai++));
        }
        buf_str(&b, out);
    }
    buf_u8(&b, 0);
    *ret = v_str((char *)b.data);
    buf_free(&b);
    return false;
}

static bool bi_Integer(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    *ret = v_num(trunc(anum(a, argc, 0)));
    return false;
}

static bool bi_Random(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    int max = (int)anum(a, argc, 0);
    n->rngState = n->rngState * 1103515245 + 12345;
    *ret = v_num(max > 0 ? ((unsigned)n->rngState >> 8) % (unsigned)max : 0);
    return false;
}

static bool bi_Time(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    *ret = v_num((double)(n->startTime + n->now / 1000));
    return false;
}

static bool bi_DateTime(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    time_t now = (time_t)(n->startTime + n->now / 1000);
    struct tm tm;
    localtime_r(&now, &tm);
    int v[6] = {tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec};
    for (int i = 0; i < 6 && i < argc; i++)
        if (a[i].t == V_STR) set_var_num(n, a[i].s->s, v[i]);
    return false;
}

static bool bi_Array(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Arr *arr = arr_get(n, astr(a, argc, 0), true);
    for (int i = 0; i < arr->count; i++) v_free(&arr->items[i]);
    arr->count = argc - 1 > 0 ? argc - 1 : 0;
    arr->items = n2_grow(arr->items, sizeof(Value) * (size_t)(arr->count ? arr->count : 1));
    for (int i = 1; i < argc; i++) arr->items[i - 1] = v_copy(&a[i]);
    return false;
}

static bool bi_Count(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    Arr *arr = arr_get(n, astr(a, argc, 0), false);
    *ret = v_num(arr ? arr->count : 0);
    return false;
}

static bool bi_VariableValue(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    char *name = n2_fmt("%s%s", astr(a, argc, 0), astr(a, argc, 1));
    if (argc >= 3) {
        var_set(n, name, arg(a, argc, 2));
    } else {
        Value *v = var_get(n, name);
        *ret = v ? v_copy(v) : v_null();
    }
    free(name);
    return false;
}

static bool bi_ModuleFileName(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    // The file of the running chapter or scene, as "nss/name.nss".
    const char *path = "";
    for (int i = t->fp - 1; i >= 0; i--) {
        if (t->frames[i].chunk->kind != CHUNK_FUNCTION) {
            path = t->frames[i].chunk->script->path;
            break;
        }
    }
    const char *slash = strrchr(path, '/');
    *ret = v_str(slash ? slash + 1 : path);
    return false;
}

static bool bi_Conquest(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    *ret = v_bool(conquest_read(n, astr(a, argc, 0), is_null(a, argc, 1) ? NULL : astr(a, argc, 1)));
    return false;
}

static bool bi_ClearScore(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    // ClearScore(LOCAL) clears the '$' variables, which the start script then sets again.
    for (int i = 0; i < n->vars.cap; i++) {
        MapEntry *e = &n->vars.entries[i];
        if (e->key && e->value && e->key[0] == '$') {
            Var *v = e->value;
            v_free(&v->v);
            free(v);
            e->value = NULL;
        }
    }
    return false;
}

static bool bi_SetShortcut(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    const char *key = astr(a, argc, 0), *script = astr(a, argc, 1);
    int vk = toupper((u8)key[0]);
    for (int i = 0; i < n->shortcutCount; i++) {
        if (n->shortcuts[i].vk == vk) {
            free(n->shortcuts[i].script);
            n->shortcuts[i].script = n2_strdup(script);
            return false;
        }
    }
    if (n->shortcutCount < 32) {
        n->shortcuts[n->shortcutCount].vk = vk;
        n->shortcuts[n->shortcutCount].script = n2_strdup(script);
        n->shortcutCount++;
    }
    return false;
}

static bool bi_Reset(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    n->pendingReset = true;
    t->wait = WAIT_FRAME;
    return true;
}

static bool bi_Exit(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    save_globals(n);
    n->quit = true;
    t->wait = WAIT_FRAME;
    return true;
}

static bool bi_Message(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    n2_log(n, "message: %s: %s", astr(a, argc, 0), astr(a, argc, 1));
    *ret = v_str("OK");
    return false;
}

static bool bi_MoveCursor(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    n->mouseX = (int)anum(a, argc, 0);
    n->mouseY = (int)anum(a, argc, 1);
    return false;
}

static bool bi_CursorPosition(N2 *n, Thread *t, Value *a, int argc, Value *ret) {
    set_var_num(n, astr(a, argc, 0), n->mouseX);
    set_var_num(n, astr(a, argc, 1), n->mouseY);
    return false;
}

static const struct {
    const char *name;
    BuiltinFn fn;
} table[] = {
    {"CreateTexture", bi_CreateTexture}, {"CreateColor", bi_CreateColor}, {"CreateWindow", bi_CreateWindow},
    {"CreateText", bi_CreateText}, {"LoadText", bi_LoadText}, {"CreateChoice", bi_CreateChoice},
    {"CreateSound", bi_CreateSound}, {"CreateProcess", bi_CreateProcess}, {"CreateScrollbar", bi_CreateScrollbar},
    {"CreateBacklog", bi_CreateBacklog}, {"CreateName", bi_CreateName}, {"CreateEffect", bi_CreateEffect},
    {"CreateSurface", bi_noop}, {"SetSurface", bi_noop}, {"SetVertex", bi_noop}, {"LoadImage", bi_LoadImage},
    {"Delete", bi_Delete}, {"SetAlias", bi_SetAlias}, {"Request", bi_Request}, {"Fade", bi_Fade}, {"Move", bi_Move},
    {"Zoom", bi_Zoom}, {"Rotate", bi_Rotate}, {"Shake", bi_Shake}, {"DrawTransition", bi_DrawTransition},
    {"WaitFade", bi_WaitFade}, {"WaitMove", bi_WaitMove}, {"WaitAction", bi_WaitAction}, {"SetShade", bi_SetShade},
    {"SetNextFocus", bi_SetNextFocus}, {"Wait", bi_Wait}, {"WaitKey", bi_WaitKey}, {"WaitText", bi_WaitText},
    {"Draw", bi_Draw}, {"SetFont", bi_SetFont}, {"LoadFont", bi_LoadFont}, {"SetVolume", bi_SetVolume},
    {"SetLoop", bi_SetLoop}, {"SetLoopPoint", bi_SetLoopPoint}, {"RemainTime", bi_RemainTime},
    {"DurationTime", bi_DurationTime}, {"Position", bi_Position}, {"ImageHorizon", bi_ImageHorizon},
    {"ImageVertical", bi_ImageVertical}, {"ScrollbarValue", bi_ScrollbarValue}, {"SetScrollbar", bi_SetScrollbar},
    {"SetScrollSpeed", bi_noop}, {"SetBacklog", bi_SetBacklog}, {"ClearBacklog", bi_ClearBacklog},
    {"EnableBacklog", bi_EnableBacklog}, {"BacklogLine", bi_noop}, {"Save", bi_Save}, {"Load", bi_Load},
    {"ExistSave", bi_ExistSave}, {"DeleteSaveFile", bi_DeleteSaveFile}, {"ReadFile", bi_ReadFile},
    {"WriteFile", bi_WriteFile}, {"String", bi_String}, {"Integer", bi_Integer}, {"Random", bi_Random},
    {"Time", bi_Time}, {"DateTime", bi_DateTime}, {"Array", bi_Array}, {"Count", bi_Count},
    {"VariableValue", bi_VariableValue}, {"ModuleFileName", bi_ModuleFileName}, {"Conquest", bi_Conquest},
    {"ClearScore", bi_ClearScore}, {"SetShortcut", bi_SetShortcut}, {"Reset", bi_Reset}, {"Exit", bi_Exit},
    {"System", bi_noop}, {"Message", bi_Message}, {"EnableDirect3DHAL", bi_true}, {"MoveCursor", bi_MoveCursor},
    {"CursorPosition", bi_CursorPosition}, {"VoiceReset", bi_noop}, {"ShowRecChapterName", bi_noop},
};

BuiltinFn builtin_find(const char *name) {
    static Map index;
    if (!index.count) {
        for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) map_set(&index, table[i].name, (void *)table[i].fn);
    }
    return (BuiltinFn)map_get(&index, name);
}

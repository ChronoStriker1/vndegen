#include "ex.h"

#define KEEP -12345
#define CHARA_SCALE 1.5f

void chara_create(EX *e, char *args) {
    char *f[8];
    int n = split_csv(args, f, 8);
    if (n < 4 || e->peopleCount >= EX_PEOPLE) return;
    Person *p = &e->people[e->peopleCount++];
    p->id = field_int(f[0]);
    p->voiced = field_int(f[2]) != 0;
    p->name = ex_strdup(f[3]);
}

void chara_define(EX *e, char *args) {
    char *f[32];
    int n = split_csv(args, f, 32);
    if (n < 23) return;
    if (e->defCount % 64 == 0) e->defs = ex_realloc(e->defs, sizeof(CharaDef) * (size_t)(e->defCount + 64));
    CharaDef *d = &e->defs[e->defCount++];
    d->chara = field_int(f[0]);
    d->pose = field_int(f[1]);
    d->size = field_int(f[2]);
    d->ox = field_int(f[4]);
    d->oy = field_int(f[5]);
    d->faces = field_int(f[20]);
    d->fx = field_int(f[21]);
    d->fy = field_int(f[22]);
}

static Person *person(EX *e, int id) {
    for (int i = 0; i < e->peopleCount; i++)
        if (e->people[i].id == id) return &e->people[i];
    return NULL;
}

const char *chara_name(EX *e, int id) {
    Person *p = person(e, id);
    return p ? p->name : NULL;
}

bool chara_voiced(EX *e, int id) {
    Person *p = person(e, id);
    return p && p->voiced;
}

static CharaDef *def(EX *e, int chara, int pose) {
    for (int i = 0; i < e->defCount; i++)
        if (e->defs[i].chara == chara && e->defs[i].pose == pose) return &e->defs[i];
    return NULL;
}

static Image *compose(EX *e, int chara, int pose, int face) {
    char rel[96];
    int code = chara * 10000 + pose;
    snprintf(rel, sizeof rel, "res/g/ch/%08d/%08dP000.gyu", chara * 10000 + pose / 10 * 10, code);
    Image *body = image_load(e, rel);
    if (!body) return NULL;
    CharaDef *d = def(e, chara, pose);
    if (!d || d->faces <= 0 || face < 0) return body;
    snprintf(rel, sizeof rel, "res/g/ch/%08d/%08dF.gyu", chara * 10000 + pose / 10 * 10, code);
    Image *strip = image_load(e, rel);
    if (!strip) return body;
    int fw = strip->w / d->faces;
    if (face < d->faces) {
        for (int y = 0; y < strip->h; y++) {
            int by = d->fy + y;
            if (by < 0 || by >= body->h) continue;
            for (int x = 0; x < fw; x++) {
                int bx = d->fx + x;
                if (bx < 0 || bx >= body->w) continue;
                u32 *b = &body->px[(size_t)by * (size_t)body->w + (size_t)bx];
                u32 s = strip->px[(size_t)y * (size_t)strip->w + (size_t)(face * fw + x)];
                *b = (s & 0xffffff) | (*b & 0xff000000u);
            }
        }
    }
    image_free(strip);
    return body;
}

static Actor *actor(EX *e, int chara, bool create) {
    for (int i = 0; i < EX_ACTORS; i++)
        if (e->actors[i].used && e->actors[i].chara == chara) return &e->actors[i];
    if (!create) return NULL;
    for (int i = 0; i < EX_ACTORS; i++)
        if (!e->actors[i].used) {
            memset(&e->actors[i], 0, sizeof e->actors[i]);
            e->actors[i].used = true;
            e->actors[i].chara = chara;
            e->actors[i].pose = -1;
            e->actors[i].face = -1;
            e->actors[i].x = EX_W / 2;
            return &e->actors[i];
        }
    return NULL;
}

Actor *chara_actor(EX *e, int layer) { return actor(e, layer - 1000, false); }

static void actor_free(Actor *a) {
    image_free(a->img);
    memset(a, 0, sizeof *a);
}

void chara_enter(EX *e, Op *op) {
    if (op->nv < 5) return;
    int chara = op->v[0], pose = op->v[1], face = op->v[2], pos = op->v[3], mode = op->v[4];
    Actor *a = actor(e, chara, true);
    if (!a) return;
    bool fresh = a->pose < 0;
    if (pose == KEEP) pose = a->pose;
    if (face == KEEP) face = a->face;
    if (pose != a->pose || face != a->face || !a->img) {
        Image *im = compose(e, chara, pose, face);
        if (im) {
            image_free(a->img);
            a->img = im;
        }
        a->pose = pose;
        a->face = face;
    }
    if (pos != KEEP) {
        a->x = (float)(EX_W / 2 + (i16)(pos & 0xffff));
        a->mx.active = false;
    }
    a->y = 0;
    a->leaving = false;
    if (fresh) {
        a->order = ++e->actorOrder;
        a->a = (mode & 0xff) == 9 ? 0 : 255;
        if ((mode & 0xff) == 9) anim_start(e, &a->fade, 0, 255, 300, 0);
    }
}

void chara_move(EX *e, int chara, int pos, int ms, int kind) {
    Actor *a = actor(e, chara, false);
    if (!a || pos == KEEP) return;
    float to = (float)(EX_W / 2 + (i16)(pos & 0xffff));
    if (ms <= 0) {
        a->x = to;
        a->mx.active = false;
    } else anim_start(e, &a->mx, a->x, to, ms, kind);
}

void chara_leave(EX *e, int chara, int ms) {
    Actor *a = actor(e, chara, false);
    if (!a) return;
    if (ms <= 0) {
        actor_free(a);
        return;
    }
    a->leaving = true;
    anim_start(e, &a->fade, a->a, 0, ms, 0);
}

void chara_leave_all(EX *e, int ms) {
    for (int i = 0; i < EX_ACTORS; i++)
        if (e->actors[i].used) chara_leave(e, e->actors[i].chara, ms);
}

bool chara_busy(EX *e) {
    for (int i = 0; i < EX_ACTORS; i++) {
        Actor *a = &e->actors[i];
        if (a->used && (a->fade.active || a->mx.active || a->my.active)) return true;
    }
    return false;
}

void chara_draw(EX *e, u32 *dst) {
    int idx[EX_ACTORS], n = 0;
    for (int i = 0; i < EX_ACTORS; i++) {
        Actor *a = &e->actors[i];
        if (!a->used) continue;
        if (a->fade.active) a->a = anim_value(e, &a->fade, a->a);
        if (a->mx.active) a->x = anim_value(e, &a->mx, a->x);
        if (a->my.active) a->y = anim_value(e, &a->my, a->y);
        if (a->leaving && !a->fade.active) {
            actor_free(a);
            continue;
        }
        idx[n++] = i;
    }
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && e->actors[idx[j]].order < e->actors[idx[j - 1]].order; j--) {
            int t = idx[j];
            idx[j] = idx[j - 1];
            idx[j - 1] = t;
        }
    for (int k = 0; k < n; k++) {
        Actor *a = &e->actors[idx[k]];
        if (!a->img || a->a <= 0) continue;
        CharaDef *d = def(e, a->chara, a->pose);
        float s = CHARA_SCALE;
        float w = (float)a->img->w * s, h = (float)a->img->h * s;
        float left = a->x - w / 2 + (d ? (float)d->ox * s : 0);
        float top = (float)EX_H - (d ? (float)d->oy * s : h) + a->y;
        Camera *c = &e->cam;
        float k = c->w > 0 ? (float)EX_W / c->w : 1;
        blit_scaled(dst, EX_W, EX_H, a->img, 0, 0, (float)a->img->w, (float)a->img->h, (left - c->x) * k, (top - c->y) * k, w * k, h * k, (int)a->a);
    }
}

void chara_free(EX *e) {
    for (int i = 0; i < EX_ACTORS; i++)
        if (e->actors[i].used) actor_free(&e->actors[i]);
    for (int i = 0; i < 4; i++) image_free(e->portraits[i].img);
    for (int i = 0; i < e->peopleCount; i++) free(e->people[i].name);
    free(e->defs);
    e->defs = NULL;
    e->defCount = e->peopleCount = 0;
}

void portrait_def(EX *e, int slot, int z) {
    if (slot >= 0 && slot < 4) e->portraitZ[slot] = z;
}

void portrait_draw_cmd(EX *e, char *args) {
    char *f[12];
    int n = split_csv(args, f, 12);
    if (n < 7) return;
    int slot = field_int(f[0]);
    if (slot < 0 || slot >= 4) return;
    Portrait *p = &e->portraits[slot];
    int chara = field_int(f[1]), pose = field_int(f[2]), face = field_int(f[3]);
    if (!p->used || p->chara != chara || p->pose != pose || p->face != face) {
        image_free(p->img);
        p->img = compose(e, chara, pose, face);
    }
    p->used = true;
    p->chara = chara;
    p->pose = pose;
    p->face = face;
    p->x = field_int(f[5]);
    p->y = field_int(f[6]);
}

void portrait_clear(EX *e, char *args) {
    char *f[8];
    int n = split_csv(args, f, 8);
    if (n < 1) return;
    int slot = field_int(f[0]);
    if (slot < 0 || slot >= 4) return;
    image_free(e->portraits[slot].img);
    memset(&e->portraits[slot], 0, sizeof e->portraits[slot]);
}

void portrait_draw(EX *e, u32 *dst, int z) {
    for (int i = 0; i < 4; i++) {
        Portrait *p = &e->portraits[i];
        if (!p->used || !p->img || e->portraitZ[i] != z) continue;
        blit(dst, EX_W, EX_H, p->img, p->x, p->y, 255);
    }
}

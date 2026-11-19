#include "bsx.h"

typedef struct Slot {
    Image *img;
    int x, y;
    char var[64];
    char op[3];
    int value;
} Slot;

struct StatusPhase {
    Image *base, *inner, *back;
    int backStates, backX, backY;
    Slot slots[32];
    int count;
};

static Image *own(BX *b, const char *name) {
    Image *im = name && *name ? bx_image_load(b, name) : NULL;
    if (!im) return NULL;
    Image *c = bx_image_new(im->w, im->h);
    c->x = im->x;
    c->y = im->y;
    memcpy(c->px, im->px, sizeof(u32) * (size_t)im->w * (size_t)im->h);
    return c;
}

static bool quoted(const char *s, const char *key, char *out, size_t size, const char **after) {
    const char *p = strstr(s, key);
    if (!p) return false;
    p = strchr(p + strlen(key), '"');
    if (!p) return false;
    const char *e = strchr(p + 1, '"');
    if (!e) return false;
    size_t n = (size_t)(e - p - 1) < size - 1 ? (size_t)(e - p - 1) : size - 1;
    memcpy(out, p + 1, n);
    out[n] = 0;
    if (after) *after = e + 1;
    return true;
}

static bool pos(const char *s, int *x, int *y) {
    const char *p = strstr(s, "pos:");
    if (!p || !(p = strchr(p, '['))) return false;
    const char *e;
    *x = bx_eval(p + 1, &e);
    if (*e != ',') return false;
    *y = bx_eval(e + 1, NULL);
    return true;
}

void bx_status_free(BX *b) {
    StatusPhase *s = b->status;
    if (!s) return;
    bx_image_free(s->base);
    bx_image_free(s->inner);
    bx_image_free(s->back);
    for (int i = 0; i < s->count; i++) bx_image_free(s->slots[i].img);
    free(s);
    b->status = NULL;
}

bool bx_status_open(BX *b) {
    const char *layout = bx_cfg_get(&b->cfg, "status screen", "layout");
    size_t n;
    u8 *d = layout ? bx_res_read(&b->arc, RX_DEF, layout, &n) : NULL;
    if (!d) {
        bx_log(b, "no status layout");
        return false;
    }
    bx_status_free(b);
    StatusPhase *s = bx_alloc(sizeof *s);
    b->status = s;
    char *text = bx_alloc(n + 1);
    memcpy(text, d, n);
    free(d);
    char name[64];
    if (quoted(text, "base:", name, sizeof name, NULL)) s->base = own(b, name);
    const char *mb = strstr(text, "\"mode button\"");
    if (mb && quoted(mb, "name:", name, sizeof name, NULL)) {
        s->back = own(b, name);
        const char *st = strstr(mb, "max_state:");
        s->backStates = st ? atoi(st + 10) : 1;
        if (s->backStates < 1) s->backStates = 1;
        pos(mb, &s->backX, &s->backY);
    }
    const char *inv = strstr(text, "layout:");
    if (inv && quoted(inv, "base:", name, sizeof name, NULL)) s->inner = own(b, name);
    const char *slot = inv ? strstr(inv, "slot:") : NULL;
    for (const char *p = slot ? strchr(slot, '{') : NULL; p && s->count < 32; p = strchr(p + 1, '{')) {
        const char *end = strchr(p, '}');
        if (!end) break;
        char item[512];
        size_t len = (size_t)(end - p) < sizeof item - 1 ? (size_t)(end - p) : sizeof item - 1;
        memcpy(item, p, len);
        item[len] = 0;
        Slot *sl = &s->slots[s->count];
        char cond[128] = "";
        if (!quoted(item, "name:", name, sizeof name, NULL) || !pos(item, &sl->x, &sl->y)) continue;
        quoted(item, "condition:", cond, sizeof cond, NULL);
        sl->img = own(b, name);
        char op[3] = "";
        if (sscanf(cond, "%63s %2s %d", sl->var, op, &sl->value) == 3) snprintf(sl->op, sizeof sl->op, "%s", op);
        s->count++;
        p = end;
    }
    free(text);
    b->phaseBack = b->phase;
    b->phase = PH_STATUS;
    return true;
}

static bool met(BX *b, const Slot *s) {
    if (!s->var[0]) return true;
    int v = bx_var_by_name(b, s->var);
    i32 x = v >= 0 ? b->globals[v].v : 0;
    if (!strcmp(s->op, "!=")) return x != s->value;
    if (!strcmp(s->op, "==")) return x == s->value;
    if (!strcmp(s->op, ">=")) return x >= s->value;
    if (!strcmp(s->op, "<=")) return x <= s->value;
    if (!strcmp(s->op, ">")) return x > s->value;
    if (!strcmp(s->op, "<")) return x < s->value;
    return true;
}

static bool on_back(BX *b, StatusPhase *s) {
    if (!s->back) return false;
    int h = s->back->h / s->backStates;
    return b->mx >= s->backX && b->my >= s->backY && b->mx < s->backX + s->back->w && b->my < s->backY + h;
}

void bx_status_update(BX *b) {
    StatusPhase *s = b->status;
    if (!s) return;
    if (b->rclick || (b->click && on_back(b, s))) {
        b->rclick = b->click = false;
        bx_status_free(b);
        b->phase = b->phaseBack;
        b->phaseBack = PH_NONE;
    }
    b->click = false;
}

void bx_status_draw(BX *b, u32 *dst) {
    StatusPhase *s = b->status;
    if (!s) return;
    if (s->base) bx_blit(dst, b->W, b->H, s->base, s->base->x, s->base->y, 255);
    if (s->inner) bx_blit(dst, b->W, b->H, s->inner, s->inner->x, s->inner->y, 255);
    for (int i = 0; i < s->count; i++)
        if (s->slots[i].img && met(b, &s->slots[i])) bx_blit(dst, b->W, b->H, s->slots[i].img, s->slots[i].x, s->slots[i].y, 255);
    if (s->back) {
        int h = s->back->h / s->backStates;
        bx_blit_part(dst, b->W, b->H, s->back, 0, on_back(b, s) && s->backStates > 1 ? h : 0, s->back->w, h, s->backX, s->backY, 255);
    }
}

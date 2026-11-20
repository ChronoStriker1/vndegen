#include "bsx.h"

static void skip(const char **p) {
    for (;;) {
        while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n' || **p == ',') (*p)++;
        if (**p != ';') return;
        while (**p && **p != '\n') (*p)++;
    }
}

static char *word(const char **p) {
    const char *s = *p;
    if (*s == '"') {
        const char *e = strchr(s + 1, '"');
        if (!e) e = s + strlen(s);
        char *r = bx_alloc((size_t)(e - s));
        memcpy(r, s + 1, (size_t)(e - s - 1));
        *p = *e ? e + 1 : e;
        return r;
    }
    const char *e = s;
    while (*e && !strchr(" \t\r\n:,{}[];", *e)) e++;
    char *r = bx_alloc((size_t)(e - s) + 1);
    memcpy(r, s, (size_t)(e - s));
    *p = e;
    return r;
}

static void add(LNode *n, LNode *kid) {
    n->kids = bx_realloc(n->kids, sizeof(LNode) * (size_t)(n->count + 1));
    n->kids[n->count++] = *kid;
}

static void value(const char **p, LNode *out, int depth) {
    skip(p);
    if (depth > 32) {
        out->kind = LN_NUM;
        return;
    }
    if (**p == '{' || **p == '[') {
        char close = **p == '{' ? '}' : ']';
        out->kind = **p == '{' ? LN_OBJ : LN_ARR;
        (*p)++;
        for (;;) {
            skip(p);
            if (!**p) return;
            if (**p == close) {
                (*p)++;
                return;
            }
            LNode kid = {0};
            if (out->kind == LN_OBJ) {
                kid.key = word(p);
                skip(p);
                if (**p == ':') (*p)++;
            }
            value(p, &kid, depth + 1);
            add(out, &kid);
        }
    }
    if (**p == '"') {
        out->kind = LN_STR;
        out->str = word(p);
        return;
    }
    const char *e;
    int v = bx_eval(*p, &e);
    if (e == *p) {
        out->kind = LN_STR;
        out->str = word(p);
        if (!*out->str && **p) (*p)++;
        return;
    }
    out->kind = LN_NUM;
    out->num = v;
    *p = e;
}

static LNode *parse(const char *text) {
    LNode *root = bx_alloc(sizeof *root);
    const char *p = text;
    if ((u8)p[0] == 0xef && (u8)p[1] == 0xbb && (u8)p[2] == 0xbf) p += 3;
    value(&p, root, 0);
    return root;
}

LNode *bx_layout_load(BX *b, const char *name) {
    size_t n;
    u8 *d = name ? bx_res_read(&b->arc, RX_DEF, name, &n) : NULL;
    if (!d) return NULL;
    char *text = bx_alloc(n + 1);
    memcpy(text, d, n);
    free(d);
    LNode *root = parse(text);
    free(text);
    return root;
}

static void free_node(LNode *n) {
    for (int i = 0; i < n->count; i++) free_node(&n->kids[i]);
    free(n->kids);
    free(n->key);
    free(n->str);
}

void bx_layout_free(LNode *n) {
    if (!n) return;
    free_node(n);
    free(n);
}

const LNode *bx_layout_get(const LNode *n, const char *key) {
    if (!n || n->kind != LN_OBJ) return NULL;
    for (int i = 0; i < n->count; i++)
        if (n->kids[i].key && !strcmp(n->kids[i].key, key)) return &n->kids[i];
    return NULL;
}

int bx_layout_int(const LNode *n, const char *key, int def) {
    const LNode *v = key ? bx_layout_get(n, key) : n;
    return v && v->kind == LN_NUM ? v->num : def;
}

const char *bx_layout_str(const LNode *n, const char *key) {
    const LNode *v = key ? bx_layout_get(n, key) : n;
    return v && v->kind == LN_STR ? v->str : NULL;
}

int bx_layout_ints(const LNode *n, const char *key, int *out, int max) {
    const LNode *v = key ? bx_layout_get(n, key) : n;
    if (!v || v->kind != LN_ARR) return 0;
    int k = 0;
    for (; k < max && k < v->count; k++) out[k] = v->kids[k].kind == LN_NUM ? v->kids[k].num : 0;
    return k;
}

static bool hit_mask(const Image *im, int h) {
    u32 first = 0;
    size_t solid = 0;
    bool any = false;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < im->w; x++) {
            u32 p = im->px[(size_t)y * im->w + x];
            if (!(p >> 24)) continue;
            if (!any) first = p & 0xffffff;
            any = true;
            for (int s = 0; s < 24; s += 8) {
                int d = (int)(p >> s & 0xff) - (int)(first >> s & 0xff);
                if (d > 12 || d < -12) return false;
            }
            solid += p >> 24 == 0xff;
        }
    return any && solid * 2 >= (size_t)im->w * (size_t)h;
}

static bool mbtn_load(BX *b, MBtn *t, const char *name, int frames, int x, int y) {
    memset(t, 0, sizeof *t);
    Image *im = name && *name ? bx_image_load(b, name) : NULL;
    if (!im) return false;
    t->img = bx_image_new(im->w, im->h);
    memcpy(t->img->px, im->px, sizeof(u32) * (size_t)im->w * (size_t)im->h);
    t->frames = clampi(frames, 1, 16);
    t->skip = t->frames > 2 && hit_mask(t->img, t->img->h / t->frames) ? 1 : 0;
    t->x = x;
    t->y = y;
    return true;
}

bool bx_mbtn_cfg(BX *b, MBtn *t, const char *sec, const char *key, int dx, int dy) {
    memset(t, 0, sizeof *t);
    const char *v = bx_cfg_get(&b->cfg, sec, key);
    char name[64], f[64];
    if (!v || bx_cfg_field(v, 0, name, sizeof name) <= 0) return false;
    int frames = bx_cfg_field(v, 1, f, sizeof f) > 0 ? bx_eval(f, NULL) : 1;
    int x = bx_cfg_field(v, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    int y = bx_cfg_field(v, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    return mbtn_load(b, t, name, frames, x + dx, y + dy);
}

bool bx_mbtn_node(BX *b, MBtn *t, const LNode *n, int dx, int dy) {
    memset(t, 0, sizeof *t);
    int pos[2] = {0, 0};
    bx_layout_ints(n, "pos", pos, 2);
    return n && mbtn_load(b, t, bx_layout_str(n, "name"), bx_layout_int(n, "max_state", 1), pos[0] + dx, pos[1] + dy);
}

bool bx_mbtn_on(const BX *b, const MBtn *t) {
    if (!t->img) return false;
    int h = t->img->h / t->frames;
    return b->mx >= t->x && b->my >= t->y && b->mx < t->x + t->img->w && b->my < t->y + h;
}

void bx_mbtn_draw(BX *b, u32 *dst, const MBtn *t, int state) {
    if (!t->img) return;
    int h = t->img->h / t->frames;
    int f = clampi((state < 0 ? 0 : state) + t->skip, 0, t->frames - 1);
    bx_blit_part(dst, b->W, b->H, t->img, 0, f * h, t->img->w, h, t->x, t->y, state < 0 ? 110 : 255);
}

int bx_mbtn_state(const BX *b, const MBtn *t) { return bx_mbtn_on(b, t) ? 1 : 0; }

void bx_mbtn_free(MBtn *t) {
    bx_image_free(t->img);
    t->img = NULL;
}

void bx_mbar_set(MBar *r, Image *img, int x, int y, const int *hit) {
    r->img = img;
    r->x = x;
    r->y = y;
    if (hit) memcpy(r->hit, hit, sizeof r->hit);
    else {
        r->hit[0] = x;
        r->hit[1] = y;
        r->hit[2] = img ? img->w : 0;
        r->hit[3] = img ? img->h : 0;
    }
}

bool bx_mbar_on(const BX *b, const MBar *r) { return r->hit[2] > 0 && b->mx >= r->hit[0] && b->my >= r->hit[1] && b->mx < r->hit[0] + r->hit[2] && b->my < r->hit[1] + r->hit[3]; }

int bx_mbar_value(const BX *b, const MBar *r) { return r->hit[2] > 0 ? clampi((b->mx - r->hit[0]) * 100 / r->hit[2], 0, 100) : 0; }

void bx_mbar_draw(BX *b, u32 *dst, const MBar *r, int value) {
    if (!r->img) return;
    bx_blit_part(dst, b->W, b->H, r->img, 0, 0, r->img->w * clampi(value, 0, 100) / 100, r->img->h, r->x, r->y, 255);
}

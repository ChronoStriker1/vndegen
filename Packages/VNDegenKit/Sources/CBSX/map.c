#include "bsx.h"

typedef struct MapPlace {
    int id, x, y;
    int offs[15][2];
    int offCount;
} MapPlace;

typedef struct MapRow {
    i32 threshold;
    int count;
    u8 pairs[8][2];
} MapRow;

typedef struct MapEntry {
    char progress[64], flag[64];
    MapRow *rows;
    int rowCount;
} MapEntry;

typedef struct Sprite {
    Image *img;
    int frames;
    int x, y;
} Sprite;

typedef struct Icon {
    int entry;
    int place;
    char name[64];
    int x, y;
} Icon;

struct MapPhase {
    char timeVar[64];
    i32 times[8];
    int timeCount;
    char **names;
    int nameCount;
    MapPlace places[32];
    int placeCount;
    MapEntry *entries;
    int entryCount;
    char resultVar[64];

    Image *base, *over, *lock, *hit;
    Sprite frame, wno, wday, time, next, markers[32];
    Image *nextHit;
    int dayAdjust;
    char dateVar[64];
    Icon icons[64];
    int iconCount;
    int hoverPlace, lockedPlace, hoverIcon;
    bool hoverNext;
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

static bool sprite(BX *b, const char *sec, const char *key, Sprite *s) {
    const char *v = bx_cfg_get(&b->cfg, sec, key);
    memset(s, 0, sizeof *s);
    if (!v) return false;
    char name[64], f[64];
    bx_cfg_field(v, 0, name, sizeof name);
    s->img = own(b, name);
    s->frames = bx_cfg_field(v, 1, f, sizeof f) > 0 ? bx_eval(f, NULL) : 1;
    if (s->frames < 1) s->frames = 1;
    s->x = bx_cfg_field(v, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    s->y = bx_cfg_field(v, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    return s->img != NULL;
}

static void sprite_draw(BX *b, u32 *dst, const Sprite *s, int frame, int frames, int dx, int dy) {
    if (!s->img) return;
    int n = frames > 0 ? frames : s->frames;
    int h = s->img->h / n;
    frame = clampi(frame, 0, n - 1);
    bx_blit_part(dst, b->W, b->H, s->img, 0, frame * h, s->img->w, h, dx, dy, 255);
}

typedef struct Reader {
    const u8 *d;
    size_t n, p;
    bool bad;
} Reader;

static int rd_u8(Reader *r) {
    if (r->p + 1 > r->n) {
        r->bad = true;
        return 0;
    }
    return r->d[r->p++];
}

static int rd_u16(Reader *r) {
    if (r->p + 2 > r->n) {
        r->bad = true;
        return 0;
    }
    int v = rd16(r->d + r->p);
    r->p += 2;
    return v;
}

static i32 rd_i32(Reader *r) {
    if (r->p + 4 > r->n) {
        r->bad = true;
        return 0;
    }
    i32 v = (i32)rd32(r->d + r->p);
    r->p += 4;
    return v;
}

static void rd_str(Reader *r, char *out, size_t size) {
    size_t k = 0;
    while (r->p < r->n && r->d[r->p]) {
        if (k + 1 < size) out[k++] = (char)r->d[r->p];
        r->p++;
    }
    if (r->p >= r->n) r->bad = true;
    r->p++;
    out[k] = 0;
}

static bool load_map(BX *b, MapPhase *m, const char *file) {
    char path[2048];
    snprintf(path, sizeof path, "%s/%s", b->gameDir, file);
    size_t n;
    u8 *d = bx_read_file(path, &n);
    if (!d || n < 16 || memcmp(d, "BSX MAP INFO r3", 15)) {
        bx_log(b, "can't read %s", file);
        free(d);
        return false;
    }
    Reader r = {d, n, 16, false};
    rd_str(&r, m->timeVar, sizeof m->timeVar);
    m->timeCount = rd_u8(&r);
    for (int i = 0; i < m->timeCount; i++) {
        i32 v = rd_i32(&r);
        if (i < 8) m->times[i] = v;
    }
    if (m->timeCount > 8) m->timeCount = 8;
    m->nameCount = rd_u8(&r);
    m->names = bx_alloc(sizeof(char *) * (size_t)(m->nameCount ? m->nameCount : 1));
    for (int i = 0; i < m->nameCount; i++) {
        char s[128];
        rd_str(&r, s, sizeof s);
        m->names[i] = bx_strdup(s);
    }
    int places = rd_u8(&r);
    for (int i = 0; i < places && !r.bad; i++) {
        MapPlace *p = &m->places[i < 32 ? i : 31];
        p->id = rd_u8(&r);
        p->x = rd_u16(&r);
        p->y = rd_u16(&r);
        int c = rd_u8(&r);
        p->offCount = c < 15 ? c : 15;
        for (int k = 0; k < c; k++) {
            i32 x = rd_i32(&r), y = rd_i32(&r);
            if (k < 15) {
                p->offs[k][0] = x;
                p->offs[k][1] = y;
            }
        }
        if (i < 32) m->placeCount = i + 1;
    }
    m->entryCount = rd_u8(&r);
    m->entries = bx_alloc(sizeof(MapEntry) * (size_t)(m->entryCount ? m->entryCount : 1));
    for (int i = 0; i < m->entryCount && !r.bad; i++) {
        MapEntry *e = &m->entries[i];
        rd_str(&r, e->progress, sizeof e->progress);
        rd_str(&r, e->flag, sizeof e->flag);
        e->rowCount = rd_u8(&r);
        e->rows = bx_alloc(sizeof(MapRow) * (size_t)(e->rowCount ? e->rowCount : 1));
        for (int k = 0; k < e->rowCount; k++) {
            MapRow *row = &e->rows[k];
            row->threshold = rd_i32(&r);
            int c = rd_u8(&r);
            row->count = c < 8 ? c : 8;
            for (int j = 0; j < c; j++) {
                int pl = rd_u8(&r), ic = rd_u8(&r);
                if (j < 8) {
                    row->pairs[j][0] = (u8)pl;
                    row->pairs[j][1] = (u8)ic;
                }
            }
        }
    }
    int results = rd_u8(&r);
    if (results > 0) rd_str(&r, m->resultVar, sizeof m->resultVar);
    free(d);
    if (r.bad) bx_log(b, "%s ended early", file);
    return !r.bad;
}

static i32 var_value(BX *b, const char *name) {
    int v = bx_var_by_name(b, name);
    return v >= 0 ? b->globals[v].v : 0;
}

static void place_icons(BX *b, MapPhase *m) {
    m->iconCount = 0;
    i32 now = var_value(b, m->timeVar);
    int slot = -1;
    for (int i = 0; i < m->timeCount; i++)
        if (m->times[i] == now) slot = i;
    if (slot < 0) return;
    for (int i = 0; i < m->entryCount && m->iconCount < 64; i++) {
        MapEntry *e = &m->entries[i];
        i32 progress = var_value(b, e->progress);
        MapRow *row = NULL;
        for (int k = 0; k < e->rowCount; k++)
            if (e->rows[k].threshold <= progress && (!row || e->rows[k].threshold >= row->threshold)) row = &e->rows[k];
        if (!row || slot >= row->count) continue;
        int place = row->pairs[slot][0], icon = row->pairs[slot][1];
        if (!place || !icon || icon > m->nameCount) continue;
        Icon *ic = &m->icons[m->iconCount++];
        ic->entry = i + 1;
        ic->place = place;
        snprintf(ic->name, sizeof ic->name, "%s", m->names[icon - 1]);
    }
    for (int p = 0; p < m->placeCount; p++) {
        MapPlace *pl = &m->places[p];
        int count = 0;
        for (int i = 0; i < m->iconCount; i++) count += m->icons[i].place == pl->id;
        int base = count * (count - 1) / 2, k = 0;
        for (int i = 0; i < m->iconCount; i++) {
            if (m->icons[i].place != pl->id) continue;
            int o = base + k < pl->offCount ? base + k : pl->offCount - 1;
            m->icons[i].x = pl->x + (o >= 0 ? pl->offs[o][0] : 0);
            m->icons[i].y = pl->y + (o >= 0 ? pl->offs[o][1] : 0);
            k++;
        }
    }
}

static int hit_at(const Image *hit, int x, int y) {
    if (!hit || x < 0 || y < 0 || x >= hit->w || y >= hit->h) return 255;
    return (int)(hit->px[(size_t)y * hit->w + x] & 0xff);
}

void bx_map_free(BX *b) {
    MapPhase *m = b->map;
    if (!m) return;
    for (int i = 0; i < m->nameCount; i++) free(m->names[i]);
    free(m->names);
    for (int i = 0; i < m->entryCount; i++) free(m->entries[i].rows);
    free(m->entries);
    bx_image_free(m->base);
    bx_image_free(m->over);
    bx_image_free(m->lock);
    bx_image_free(m->hit);
    bx_image_free(m->nextHit);
    Sprite *ss[] = {&m->frame, &m->wno, &m->wday, &m->time, &m->next};
    for (int i = 0; i < 5; i++) bx_image_free(ss[i]->img);
    for (int i = 0; i < 32; i++) bx_image_free(m->markers[i].img);
    free(m);
    b->map = NULL;
}

bool bx_map_start(BX *b) {
    bx_map_free(b);
    MapPhase *m = bx_alloc(sizeof *m);
    b->map = m;
    const char *sec = "map phase";
    const char *file = bx_cfg_get(&b->cfg, sec, "outsource");
    if (!load_map(b, m, file ? file : "map.dat")) {
        bx_map_free(b);
        return false;
    }
    m->base = own(b, bx_cfg_get(&b->cfg, sec, "base"));
    m->over = own(b, bx_cfg_get(&b->cfg, sec, "base over"));
    m->lock = own(b, bx_cfg_get(&b->cfg, sec, "base on"));
    m->hit = own(b, bx_cfg_get(&b->cfg, sec, "hit table"));
    sprite(b, sec, "frame", &m->frame);
    sprite(b, sec, "week no", &m->wno);
    sprite(b, sec, "week day", &m->wday);
    sprite(b, sec, "time zone", &m->time);
    sprite(b, sec, "next time", &m->next);
    if (m->next.img) {
        const char *v = bx_cfg_get(&b->cfg, sec, "next time");
        char name[64], hit[80];
        bx_cfg_field(v, 0, name, sizeof name);
        snprintf(hit, sizeof hit, "%s_hit", name);
        m->nextHit = own(b, hit);
    }
    for (int i = 0; i < m->placeCount; i++) {
        char key[32];
        snprintf(key, sizeof key, "plase_%d", m->places[i].id);
        sprite(b, sec, key, &m->markers[i]);
    }
    const char *date = bx_cfg_get(&b->cfg, sec, "date");
    snprintf(m->dateVar, sizeof m->dateVar, "%s", date ? date : "#day");
    m->dayAdjust = bx_cfg_int(&b->cfg, sec, "day adjustment", 0);
    m->hoverPlace = m->lockedPlace = 0;
    m->hoverIcon = -1;
    place_icons(b, m);
    return true;
}

static void finish(BX *b, int value) {
    MapPhase *m = b->map;
    int v = bx_var_by_name(b, m->resultVar[0] ? m->resultVar : "#character");
    if (v >= 0) b->globals[v] = (Val){0, value};
    bx_snd_play(b, CH_SYS, RX_SYS, "done", false, 0);
    bx_map_free(b);
    bx_phase_end(b);
    bx_transition(b, 0, 0, NULL);
}

static bool place_has_icons(MapPhase *m, int place) {
    for (int i = 0; i < m->iconCount; i++)
        if (m->icons[i].place == place) return true;
    return false;
}

void bx_map_update(BX *b) {
    MapPhase *m = b->map;
    if (!m) return;
    m->hoverIcon = -1;
    m->hoverNext = false;
    if (m->next.img) {
        int lx = b->mx - m->next.x, ly = b->my - m->next.y;
        int h = m->next.img->h / m->next.frames;
        if (lx >= 0 && ly >= 0 && lx < m->next.img->w && ly < h) m->hoverNext = (m->next.img->px[(size_t)ly * m->next.img->w + lx] >> 24) != 0;
    }
    if (m->lockedPlace) {
        for (int i = 0; i < m->iconCount; i++) {
            Icon *ic = &m->icons[i];
            if (ic->place == m->lockedPlace && b->mx >= ic->x && b->mx < ic->x + 80 && b->my >= ic->y && b->my < ic->y + 80) m->hoverIcon = i;
        }
    } else {
        int h = m->hit ? hit_at(m->hit, b->mx, b->my) : 255;
        m->hoverPlace = h == 255 ? 0 : h + 1;
    }
    if (b->rclick && m->lockedPlace) {
        b->rclick = false;
        m->lockedPlace = 0;
        bx_snd_play(b, CH_SYS, RX_SYS, "cancel", false, 0);
    }
    if (!b->click) return;
    b->click = false;
    if (m->hoverNext) {
        finish(b, 0);
        return;
    }
    if (m->lockedPlace) {
        if (m->hoverIcon >= 0) finish(b, m->icons[m->hoverIcon].entry);
        return;
    }
    if (m->hoverPlace && place_has_icons(m, m->hoverPlace)) {
        m->lockedPlace = m->hoverPlace;
        bx_snd_play(b, CH_SYS, RX_SYS, "check", false, 0);
    }
}

static void masked(BX *b, u32 *dst, const Image *src, const Image *hit, int id) {
    if (!src || !hit) return;
    int w = src->w < hit->w ? src->w : hit->w, h = src->h < hit->h ? src->h : hit->h;
    for (int y = 0; y < h && y < b->H; y++)
        for (int x = 0; x < w && x < b->W; x++)
            if ((int)(hit->px[(size_t)y * hit->w + x] & 0xff) == id) dst[(size_t)y * b->W + x] = src->px[(size_t)y * src->w + x];
}

void bx_map_draw(BX *b, u32 *dst) {
    MapPhase *m = b->map;
    if (!m) return;
    if (m->base) bx_blit(dst, b->W, b->H, m->base, 0, 0, 255);
    int place = m->lockedPlace ? m->lockedPlace : m->hoverPlace;
    if (place) masked(b, dst, m->lockedPlace ? (m->lock ? m->lock : m->over) : m->over, m->hit, place - 1);
    if (place && place_has_icons(m, place)) {
        for (int i = 0; i < m->placeCount; i++)
            if (m->places[i].id == place && m->markers[i].img) {
                int count = 0;
                for (int k = 0; k < m->iconCount; k++) count += m->icons[k].place == place;
                sprite_draw(b, dst, &m->markers[i], count - 1, 0, m->places[i].x + m->markers[i].x, m->places[i].y + m->markers[i].y);
            }
        for (int i = 0; i < m->iconCount; i++) {
            Icon *ic = &m->icons[i];
            if (ic->place != place) continue;
            Image *im = bx_image_load(b, ic->name);
            if (!im) continue;
            int fh = im->h / 2;
            bx_blit_part(dst, b->W, b->H, im, 0, i == m->hoverIcon ? fh : 0, im->w, fh, ic->x, ic->y, 255);
        }
    }
    if (m->frame.img) bx_blit(dst, b->W, b->H, m->frame.img, m->frame.x, m->frame.y, 255);
    i32 day = var_value(b, m->dateVar) + m->dayAdjust;
    if (day < 0) day = 0;
    sprite_draw(b, dst, &m->wno, day / 7, 0, m->wno.x, m->wno.y);
    sprite_draw(b, dst, &m->wday, day % 7, 0, m->wday.x, m->wday.y);
    i32 now = var_value(b, m->timeVar);
    int slot = 0;
    for (int i = 0; i < m->timeCount; i++)
        if (m->times[i] == now) slot = i;
    sprite_draw(b, dst, &m->time, slot, m->timeCount, m->time.x, m->time.y);
    sprite_draw(b, dst, &m->next, m->hoverNext ? 1 : 0, 0, m->next.x, m->next.y);
}

int bx_map_hover(BX *b) { return b->map ? (b->map->lockedPlace ? b->map->lockedPlace : b->map->hoverPlace) : -1; }

bool bx_map_choose(BX *b, int index) {
    MapPhase *m = b->map;
    if (!m || index < 0 || index >= m->iconCount) return false;
    finish(b, m->icons[index].entry);
    return true;
}

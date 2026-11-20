#include "strip.h"

typedef struct Variant {
    int count;
    int *ids;
} Variant;

typedef struct Thumb {
    int count;
    Variant *v;
} Thumb;

typedef struct CgPage {
    int count;
    Thumb *t;
} CgPage;

typedef struct SceneEntry {
    int page, index;
    char label[64];
    int count;
    int ids[8];
} SceneEntry;

#define LISTS 10
#define LIST_MAX 64

struct Museum {
    CgPage *pages;
    int pageCount;
    char **names, **tags;
    int nameCount;
    int *hash;
    int hashSize;
    SceneEntry *scenes;
    int sceneCount;
    size_t cgFlags, sceneFlags, listOff;
    u32 state;
    int list[LISTS][LIST_MAX][3];
    int listLen[LISTS], listSel;
    int queue[LIST_MAX][3];
    int queueLen, queuePos;
};

enum { MM_BGM, MM_BGV, MM_CG, MM_SCENE, MM_STRIP, MM_COUNT };

typedef struct Viewer {
    bool open, panel, hairOff, cutOff, steamOff, dragVol;
    int thumb, variant;
    u32 *pixels;
    int px, py;
    Image *base, *digits;
    int digitFrames, now[3], all[3];
    MBtn prev, next, cut, hair, exit, detail, stop, steam, ahe;
    MBar vol;
} Viewer;

typedef struct Playlist {
    bool open;
    Image *base, *bar;
    int barX, barY;
    MBtn play, close, pages[LISTS];
    int thumb[4], cols, max;
    Image *thumbs[LIST_MAX];
    int made;
} Playlist;

struct MuseumPhase {
    int mode;
    Image *base;
    MBtn modes[MM_COUNT], back;
    int modeOf[MM_COUNT];
    u32 modeBits[MM_COUNT];
    Image *under, *over, *frames[32], *parts, *percent;
    int frameCount;
    MBtn pages[16];
    int pageCount, page;
    int thumbPos[32][2], thumbsPerPage;
    int partsX, partsY, partsSpace, percentX, percentY, percentSpace, partsFrames, percentFrames;
    Image *thumbs[32];
    int thumbsMade, hover;
    MBtn playNow, addList, openList;
    bool addMode;
    Playlist list;
    MBtn tracks[32], play, stop, prev, next, volMin, volMax;
    MBar vol;
    bool dragVol, playing;
    char trackNames[32][32];
    int trackCount, track;
    Viewer view;
    BgvPanel *bgv;
    bool bgvOverlay;
    Strip *strip;
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

static Image *cfg_image(BX *b, const char *sec, const char *key, int *x, int *y) {
    const char *v = bx_cfg_get(&b->cfg, sec, key);
    char name[64], f[32];
    if (!v || bx_cfg_field(v, 0, name, sizeof name) <= 0) return NULL;
    *x = bx_cfg_field(v, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    *y = bx_cfg_field(v, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    return own(b, name);
}

static int name_id(const Museum *m, const char *name) {
    if (!m || !m->hashSize) return -1;
    char low[64];
    size_t n = 0;
    for (; name[n] && n < sizeof low - 1; n++) low[n] = (char)(name[n] >= 'A' && name[n] <= 'Z' ? name[n] + 32 : name[n]);
    low[n] = 0;
    for (u32 h = bx_hash(low) & (u32)(m->hashSize - 1); m->hash[h] >= 0; h = (h + 1) & (u32)(m->hashSize - 1))
        if (bx_ieq(m->names[m->hash[h]], low)) return m->hash[h];
    return -1;
}

static bool load_cg(BX *b, Museum *m) {
    char path[2048];
    snprintf(path, sizeof path, "%s/cg.dat", b->gameDir);
    size_t n;
    u8 *d = bx_read_file(path, &n);
    if (!d || n < 2) {
        free(d);
        return false;
    }
    size_t p = 0;
    bool bad = false;
#define NEED(k)                                                                                                                  \
    if (p + (k) > n) {                                                                                                           \
        bad = true;                                                                                                              \
        break;                                                                                                                   \
    }
    m->pageCount = d[p++];
    m->pages = bx_alloc(sizeof(CgPage) * (size_t)m->pageCount);
    for (int pg = 0; pg < m->pageCount && !bad; pg++) {
        NEED(1);
        CgPage *page = &m->pages[pg];
        page->count = d[p++];
        page->t = bx_alloc(sizeof(Thumb) * (size_t)page->count);
        for (int t = 0; t < page->count && !bad; t++) {
            NEED(2);
            Thumb *th = &page->t[t];
            th->count = rd16(d + p);
            p += 2;
            th->v = bx_alloc(sizeof(Variant) * (size_t)(th->count ? th->count : 1));
            for (int v = 0; v < th->count; v++) {
                NEED(1);
                int k = d[p++];
                NEED((size_t)k * 4 + 1);
                th->v[v].count = k;
                th->v[v].ids = bx_alloc(sizeof(int) * (size_t)(k ? k : 1));
                for (int i = 0; i < k; i++, p += 4) th->v[v].ids[i] = (int)rd32(d + p);
                p += v == th->count - 1 ? 1 : 2;
            }
        }
    }
    if (!bad && p + 2 <= n) {
        m->nameCount = rd16(d + p);
        p += 2;
        m->names = bx_alloc(sizeof(char *) * (size_t)(m->nameCount ? m->nameCount : 1));
        m->tags = bx_alloc(sizeof(char *) * (size_t)(m->nameCount ? m->nameCount : 1));
        for (int i = 0; i < m->nameCount; i++) {
            size_t e = p;
            while (e + 1 < n && (d[e] || d[e + 1])) e += 2;
            char *s = bx_utf16_to_utf8(d + p, (e - p) / 2);
            for (char *c = s; *c; c++)
                if (*c >= 'A' && *c <= 'Z') *c = (char)(*c + 32);
            char *colon = strchr(s, ':');
            m->tags[i] = bx_strdup(colon ? colon + 1 : "");
            if (colon) *colon = 0;
            m->names[i] = s;
            p = e + 2;
        }
    } else bad = true;
#undef NEED
    free(d);
    if (bad) {
        bx_log(b, "cg.dat doesn't parse");
        return false;
    }
    m->hashSize = 1;
    while (m->hashSize < m->nameCount * 2 + 2) m->hashSize <<= 1;
    m->hash = bx_alloc(sizeof(int) * (size_t)m->hashSize);
    for (int i = 0; i < m->hashSize; i++) m->hash[i] = -1;
    for (int i = 0; i < m->nameCount; i++) {
        u32 h = bx_hash(m->names[i]) & (u32)(m->hashSize - 1);
        while (m->hash[h] >= 0) h = (h + 1) & (u32)(m->hashSize - 1);
        m->hash[h] = i;
    }
    return true;
}

static void load_scenes(BX *b, Museum *m) {
    char path[2048];
    snprintf(path, sizeof path, "%s/scene.dat", b->gameDir);
    size_t n;
    u8 *d = bx_read_file(path, &n);
    if (!d) return;
    char *text = bx_alloc(n + 1);
    memcpy(text, d, n);
    free(d);
    for (char *line = text; line && *line;) {
        char *next = strchr(line, '\n');
        if (next) *next++ = 0;
        char field[16][64];
        int nf = 0;
        for (char *s = line; nf < 16;) {
            char *e = strchr(s, ',');
            size_t len = e ? (size_t)(e - s) : strlen(s);
            char *f = field[nf++];
            size_t k = 0;
            for (size_t i = 0; i < len && k < 63; i++)
                if (s[i] != '"' && s[i] != '\r' && s[i] != ' ') f[k++] = s[i];
            f[k] = 0;
            if (!e) break;
            s = e + 1;
        }
        if (nf >= 5) {
            m->scenes = bx_realloc(m->scenes, sizeof(SceneEntry) * (size_t)(m->sceneCount + 1));
            SceneEntry *e = &m->scenes[m->sceneCount++];
            memset(e, 0, sizeof *e);
            e->page = atoi(field[0]);
            e->index = atoi(field[1]);
            snprintf(e->label, sizeof e->label, "%s", field[2]);
            for (int i = 4; i < nf && e->count < 8; i++)
                if (field[i][0]) e->ids[e->count++] = name_id(m, field[i]);
        }
        line = next;
    }
    free(text);
}

static bool lists_fit(const BX *b, size_t k) {
    if (k + 4 > b->commonSize || rd32(b->common + k) != LISTS) return false;
    size_t p = k + 4;
    for (int i = 0; i < LISTS; i++) {
        if (p + 4 > b->commonSize) return false;
        u32 c = rd32(b->common + p);
        if (c > 4096) return false;
        p += 4 + 12 * (size_t)c;
    }
    return p + 4 == b->commonSize;
}

static void find_tables(BX *b, Museum *m) {
    m->state = 0xffffffffu;
    if (!b->common) return;
    size_t start = b->readFlags + (size_t)b->sc->msgCount;
    if (start + 8 <= b->commonSize && rd32(b->common + start) >= 1 && rd32(b->common + start) < 64) m->state = rd32(b->common + start + 4);
    for (size_t k = start; k + 4 <= b->commonSize && k < start + 256; k++)
        if (rd32(b->common + k) == (u32)m->nameCount && k + 4 + (size_t)m->nameCount + 4 <= b->commonSize) {
            m->cgFlags = k + 4;
            size_t s = m->cgFlags + (size_t)m->nameCount;
            if (rd32(b->common + s) == (u32)m->sceneCount && s + 4 + (size_t)m->sceneCount <= b->commonSize) m->sceneFlags = s + 4;
            break;
        }
    if (!m->sceneFlags) return;
    for (size_t k = m->sceneFlags + (size_t)m->sceneCount; k + 48 <= b->commonSize; k++)
        if (lists_fit(b, k)) {
            m->listOff = k;
            break;
        }
    if (!m->listOff) return;
    size_t p = m->listOff + 4;
    for (int i = 0; i < LISTS; i++) {
        u32 c = rd32(b->common + p);
        p += 4;
        int n = 0;
        for (u32 j = 0; j < c; j++, p += 12)
            if (n < LIST_MAX) {
                for (int f = 0; f < 3; f++) m->list[i][n][f] = (int)rd32(b->common + p + 4 * (size_t)f);
                n++;
            }
        m->listLen[i] = n;
    }
    m->listSel = clampi((int)rd32(b->common + p), 0, LISTS - 1);
}

static void save_lists(BX *b) {
    Museum *m = b->museum;
    if (!m->listOff) return;
    size_t size = 8;
    for (int i = 0; i < LISTS; i++) size += 4 + 12 * (size_t)m->listLen[i];
    b->common = bx_realloc(b->common, m->listOff + size);
    u8 *p = b->common + m->listOff;
    wr32(p, LISTS);
    p += 4;
    for (int i = 0; i < LISTS; i++) {
        wr32(p, (u32)m->listLen[i]);
        p += 4;
        for (int j = 0; j < m->listLen[i]; j++)
            for (int f = 0; f < 3; f++, p += 4) wr32(p, (u32)m->list[i][j][f]);
    }
    wr32(p, (u32)m->listSel);
    b->commonSize = m->listOff + size;
    b->commonDirty = true;
}

void bx_museum_init(BX *b) {
    Museum *m = bx_alloc(sizeof *m);
    if (!load_cg(b, m)) {
        free(m);
        return;
    }
    load_scenes(b, m);
    find_tables(b, m);
    b->museum = m;
    if (!m->cgFlags) bx_log(b, "no museum records in common.dat");
}

void bx_museum_free(BX *b) {
    Museum *m = b->museum;
    if (!m) return;
    for (int pg = 0; pg < m->pageCount; pg++) {
        for (int t = 0; t < m->pages[pg].count; t++) {
            for (int v = 0; v < m->pages[pg].t[t].count; v++) free(m->pages[pg].t[t].v[v].ids);
            free(m->pages[pg].t[t].v);
        }
        free(m->pages[pg].t);
    }
    free(m->pages);
    for (int i = 0; i < m->nameCount; i++) {
        free(m->names[i]);
        free(m->tags[i]);
    }
    free(m->names);
    free(m->tags);
    free(m->hash);
    free(m->scenes);
    free(m);
    b->museum = NULL;
}

static bool seen(BX *b, int id) {
    Museum *m = b->museum;
    return m && m->cgFlags && id >= 0 && id < m->nameCount && b->common[m->cgFlags + (size_t)id];
}

static bool variant_seen(BX *b, const Variant *v) {
    for (int i = 0; i < v->count; i++)
        if (!seen(b, v->ids[i])) return false;
    return v->count > 0;
}

static bool scene_open(BX *b, int i) {
    Museum *m = b->museum;
    return m && m->sceneFlags && i >= 0 && i < m->sceneCount && b->common[m->sceneFlags + (size_t)i];
}

void bx_museum_seen(BX *b, const char *name) {
    Museum *m = b->museum;
    if (!m || !m->cgFlags || !name || !*name) return;
    int id = name_id(m, name);
    if (id < 0 || b->common[m->cgFlags + (size_t)id]) return;
    b->common[m->cgFlags + (size_t)id] = 1;
    b->commonDirty = true;
    if (!m->sceneFlags) return;
    for (int i = 0; i < m->sceneCount; i++) {
        SceneEntry *e = &m->scenes[i];
        if (b->common[m->sceneFlags + (size_t)i] || !e->count) continue;
        bool all = true, has = false;
        for (int k = 0; k < e->count; k++) {
            has |= e->ids[k] == id;
            all &= seen(b, e->ids[k]);
        }
        if (has && all) b->common[m->sceneFlags + (size_t)i] = 1;
    }
}

int bx_museum_rate(BX *b, const char *kind) {
    Museum *m = b->museum;
    if (!m) return 0;
    if (!strcmp(kind, "cg")) {
        int all = 0, got = 0;
        for (int pg = 0; pg < m->pageCount; pg++)
            for (int t = 0; t < m->pages[pg].count; t++)
                for (int v = 0; v < m->pages[pg].t[t].count; v++) {
                    all++;
                    got += variant_seen(b, &m->pages[pg].t[t].v[v]);
                }
        return all ? got * 100 / all : 0;
    }
    if (!strcmp(kind, "scene")) {
        int got = 0;
        for (int i = 0; i < m->sceneCount; i++) got += scene_open(b, i);
        return m->sceneCount ? got * 100 / m->sceneCount : 0;
    }
    if (!b->wearTable) return 0;
    int all = 0, got = 0;
    size_t k = b->wearTable + 4;
    for (u32 c = rd32(b->common + b->wearTable); c > 0 && k + 4 <= b->commonSize; c--) {
        u32 w = rd32(b->common + k);
        k += 4;
        for (u32 i = 0; i < w && k + i < b->commonSize; i++) got += b->common[k + i] != 0;
        all += (int)w;
        k += w;
    }
    return all ? got * 100 / all : 0;
}

void bx_museum_wear(BX *b, int ch, int wear) {
    if (!b->common || !b->wearTable || ch < 1 || wear < 0) return;
    u32 count = rd32(b->common + b->wearTable);
    size_t k = b->wearTable + 4;
    for (u32 c = 1; c <= count && k + 4 <= b->commonSize; c++) {
        u32 w = rd32(b->common + k);
        k += 4;
        if ((int)c == ch) {
            if ((u32)wear < w && !b->common[k + (size_t)wear]) {
                b->common[k + (size_t)wear] = 1;
                b->commonDirty = true;
            }
            return;
        }
        k += w;
    }
}

void bx_museum_find_wear(BX *b) {
    b->wearTable = 0;
    if (!b->common) return;
    int n = 0;
    u32 w[32];
    while (n < 32) {
        char key[32];
        snprintf(key, sizeof key, "ch%d max wear no", n + 1);
        if (!bx_cfg_get(&b->cfg, "bustup setting", key)) break;
        w[n++] = (u32)clampi(bx_cfg_int(&b->cfg, "bustup setting", key, 0), 0, 254) + 1;
    }
    if (!n) return;
    for (size_t k = 0x20; k + 8 < b->choiceFlags; k++) {
        if (rd32(b->common + k) != (u32)n) continue;
        size_t q = k + 4;
        int c = 0;
        for (; c < n && q + 4 <= b->choiceFlags && rd32(b->common + q) == w[c]; c++) q += 4 + w[c];
        if (c == n) {
            b->wearTable = k;
            return;
        }
    }
}

static const char *mode_section(int mode) {
    return mode == MM_BGM ? "museum phase - bgm" : mode == MM_CG ? "museum phase - cg" : mode == MM_SCENE ? "museum phase - scene" : NULL;
}

static void free_images(Image **list, int n) {
    for (int i = 0; i < n; i++) {
        bx_image_free(list[i]);
        list[i] = NULL;
    }
}

static void close_viewer(Viewer *v) {
    free(v->pixels);
    bx_image_free(v->base);
    bx_image_free(v->digits);
    bx_image_free(v->vol.img);
    MBtn *bs[] = {&v->prev, &v->next, &v->cut, &v->hair, &v->exit, &v->detail, &v->stop, &v->steam, &v->ahe};
    for (size_t i = 0; i < sizeof bs / sizeof *bs; i++) bx_mbtn_free(bs[i]);
    bool hairOff = v->hairOff, cutOff = v->cutOff, steamOff = v->steamOff;
    memset(v, 0, sizeof *v);
    v->hairOff = hairOff;
    v->cutOff = cutOff;
    v->steamOff = steamOff;
}

static void close_list(Playlist *l) {
    bx_image_free(l->base);
    bx_image_free(l->bar);
    bx_mbtn_free(&l->play);
    bx_mbtn_free(&l->close);
    for (int i = 0; i < LISTS; i++) bx_mbtn_free(&l->pages[i]);
    free_images(l->thumbs, LIST_MAX);
    memset(l, 0, sizeof *l);
}

static void free_mode(MuseumPhase *u) {
    bx_image_free(u->under);
    bx_image_free(u->over);
    bx_image_free(u->parts);
    bx_image_free(u->percent);
    bx_image_free(u->vol.img);
    free_images(u->frames, 32);
    free_images(u->thumbs, 32);
    for (int i = 0; i < 16; i++) bx_mbtn_free(&u->pages[i]);
    for (int i = 0; i < 32; i++) bx_mbtn_free(&u->tracks[i]);
    MBtn *bs[] = {&u->play, &u->stop, &u->prev, &u->next, &u->volMin, &u->volMax, &u->playNow, &u->addList, &u->openList};
    for (size_t i = 0; i < sizeof bs / sizeof *bs; i++) bx_mbtn_free(bs[i]);
    close_viewer(&u->view);
    close_list(&u->list);
    bx_bgv_free(u->bgv);
    u->bgv = NULL;
    u->bgvOverlay = false;
    bx_strip_free(u->strip);
    u->strip = NULL;
    u->under = u->over = u->parts = u->percent = NULL;
    memset(&u->vol, 0, sizeof u->vol);
    u->frameCount = u->pageCount = u->trackCount = 0;
    u->addMode = u->dragVol = false;
}

static void number_spec(BX *b, const char *sec, const char *key, Image **img, int *frames, int *x, int *y) {
    const char *v = bx_cfg_get(&b->cfg, sec, key);
    char name[64], f[32];
    if (!v || bx_cfg_field(v, 0, name, sizeof name) <= 0) return;
    *img = own(b, name);
    *frames = bx_cfg_field(v, 1, f, sizeof f) > 0 ? clampi(bx_eval(f, NULL), 1, 16) : 10;
    *x = bx_cfg_field(v, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    *y = bx_cfg_field(v, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
}

static int load_frames(BX *b, const char *name, Image **out, int max) {
    size_t n;
    u8 *d = name && *name ? bx_res_read(&b->arc, RX_GFX, name, &n) : NULL;
    if (!d) return 0;
    int k = 0;
    if (n >= 0x20 && !memcmp(d, "BSS-Composition", 15))
        for (; k < max; k++)
            if (!(out[k] = bx_bsg_part(d, n, k))) break;
    free(d);
    return k;
}

static void open_mode(BX *b, MuseumPhase *u, int mode) {
    free_mode(u);
    u->mode = mode;
    u->page = 0;
    u->hover = -1;
    u->thumbsMade = 0;
    if (mode == MM_BGV) {
        u->bgv = bx_bgv_open(b, false);
        return;
    }
    if (mode == MM_STRIP) {
        u->strip = bx_strip_open(b);
        return;
    }
    const char *sec = mode_section(mode);
    if (!sec) return;
    if (mode == MM_BGM) {
        u->over = own(b, bx_cfg_get(&b->cfg, sec, "base"));
        bx_mbtn_cfg(b, &u->play, sec, "play icon", 0, 0);
        bx_mbtn_cfg(b, &u->stop, sec, "stop icon", 0, 0);
        bx_mbtn_cfg(b, &u->prev, sec, "prev icon", 0, 0);
        bx_mbtn_cfg(b, &u->next, sec, "next icon", 0, 0);
        bx_mbtn_cfg(b, &u->volMin, sec, "min button", 0, 0);
        bx_mbtn_cfg(b, &u->volMax, sec, "max button", 0, 0);
        int x = 0, y = 0;
        Image *bar = cfg_image(b, sec, "volume", &x, &y);
        bx_mbar_set(&u->vol, bar, x, y, NULL);
        for (int i = 1; i <= 32 && u->trackCount < 32; i++) {
            char key[32];
            snprintf(key, sizeof key, "track%02d", i);
            if (!bx_mbtn_cfg(b, &u->tracks[u->trackCount], sec, key, 0, 0)) continue;
            snprintf(key, sizeof key, "track%02d name", i);
            const char *nm = bx_cfg_get(&b->cfg, sec, key);
            snprintf(u->trackNames[u->trackCount], sizeof u->trackNames[0], "%s", nm ? nm : "");
            u->trackCount++;
        }
        u->track = clampi(u->track, 0, u->trackCount ? u->trackCount - 1 : 0);
        return;
    }
    u->under = own(b, bx_cfg_get(&b->cfg, sec, "under base"));
    const char *over = bx_cfg_get(&b->cfg, sec, "base");
    u->frameCount = load_frames(b, over, u->frames, 32);
    if (!u->frameCount) u->over = own(b, over);
    for (int i = 0; i < 16; i++) {
        char key[32];
        snprintf(key, sizeof key, "page icon%d", i);
        if (bx_mbtn_cfg(b, &u->pages[u->pageCount], sec, key, 0, 0)) u->pageCount++;
    }
    u->thumbsPerPage = 0;
    for (int i = 0; i < 32; i++) {
        char key[32];
        snprintf(key, sizeof key, "thumb%d pos", i);
        int pos[2];
        if (bx_cfg_ints(&b->cfg, sec, key, pos, 2) != 2) break;
        u->thumbPos[i][0] = pos[0];
        u->thumbPos[i][1] = pos[1];
        u->thumbsPerPage++;
    }
    number_spec(b, sec, "thumb number", &u->parts, &u->partsFrames, &u->partsX, &u->partsY);
    number_spec(b, sec, "percent number", &u->percent, &u->percentFrames, &u->percentX, &u->percentY);
    u->partsSpace = bx_cfg_int(&b->cfg, sec, "thumb number space", -10);
    u->percentSpace = bx_cfg_int(&b->cfg, sec, "percent number space", -26);
    if (mode == MM_SCENE) {
        bx_mbtn_cfg(b, &u->playNow, sec, "play now", 0, 0);
        bx_mbtn_cfg(b, &u->addList, sec, "add playlist", 0, 0);
        bx_mbtn_cfg(b, &u->openList, sec, "playlist button", 0, 0);
    }
}

static bool is_cutin(const Museum *m, const int *ids, int count, int i) {
    if (strchr(m->tags[ids[i]], 'c')) return true;
    const char *n = m->names[ids[i]];
    size_t len = strlen(n);
    if (len < 2 || n[len - 1] != 'c') return false;
    for (int k = 0; k < count; k++)
        if (k != i && ids[k] >= 0 && ids[k] < m->nameCount && strlen(m->names[ids[k]]) == len - 1 && !strncmp(m->names[ids[k]], n, len - 1)) return true;
    return false;
}

static void compose(BX *b, const int *ids, int count, u32 *dst, bool hairOff, bool cutOff, bool steamOff) {
    Museum *m = b->museum;
    size_t n = (size_t)b->W * (size_t)b->H;
    for (size_t i = 0; i < n; i++) dst[i] = 0xff000000u;
    const char *suffix = bx_cfg_get(&b->cfg, "main window", "hair suffix");
    for (int i = 0; i < count; i++) {
        if (ids[i] < 0 || ids[i] >= m->nameCount || (cutOff && is_cutin(m, ids, count, i)) || (steamOff && strchr(m->tags[ids[i]], 's'))) continue;
        const char *name = m->names[ids[i]];
        char alt[96];
        if (hairOff && suffix) {
            snprintf(alt, sizeof alt, "%s%s", name, suffix);
            if (bx_res_find(&b->arc, RX_GFX, alt)) name = alt;
        }
        Image *im = bx_image_load(b, name);
        if (im) bx_blit(dst, b->W, b->H, im, im->x, im->y, 255);
    }
}

static void box_blur(const u32 *src, u32 *dst, int w, int h, int r) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            u32 s[3] = {0, 0, 0}, k = 0;
            for (int yy = y - r; yy <= y + r; yy++)
                for (int xx = x - r; xx <= x + r; xx++) {
                    if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                    u32 p = src[(size_t)yy * w + xx];
                    s[0] += p & 0xff;
                    s[1] += p >> 8 & 0xff;
                    s[2] += p >> 16 & 0xff;
                    k++;
                }
            dst[(size_t)y * w + x] = 0xff000000u | (s[2] / k) << 16 | (s[1] / k) << 8 | s[0] / k;
        }
}

static void blur_masked(BX *b, Image *t, const int *ids, int count) {
    Museum *m = b->museum;
    u32 *blur = NULL;
    for (int i = 0; i < count; i++) {
        if (ids[i] < 0 || ids[i] >= m->nameCount) continue;
        char name[96];
        snprintf(name, sizeof name, "%sm", m->names[ids[i]]);
        if (!bx_res_find(&b->arc, RX_GFX, name)) continue;
        Image *mask = bx_image_load(b, name);
        if (!mask || mask->w != t->w || mask->h != t->h) continue;
        if (!blur) {
            u32 *tmp = bx_alloc(sizeof(u32) * (size_t)t->w * (size_t)t->h);
            blur = bx_alloc(sizeof(u32) * (size_t)t->w * (size_t)t->h);
            box_blur(t->px, tmp, t->w, t->h, 3);
            box_blur(tmp, blur, t->w, t->h, 3);
            free(tmp);
        }
        for (size_t k = 0; k < (size_t)t->w * (size_t)t->h; k++) {
            u32 a = (mask->px[k] >> 24) * (mask->px[k] & 0xff) / 255, p = t->px[k], q = blur[k], out = 0xff000000u;
            if (!a) continue;
            for (int s = 0; s < 24; s += 8) out |= (((q >> s & 0xff) * a + (p >> s & 0xff) * (255 - a)) / 255) << s;
            t->px[k] = out;
        }
    }
    free(blur);
}

static const Variant *first_seen(BX *b, const Thumb *t);

Image *bx_scaled(BX *b, const u32 *full, int tw, int th) {
    Image *t = bx_image_new(tw, th);
    for (int y = 0; y < th; y++) {
        int y0 = y * b->H / th, y1 = (y + 1) * b->H / th;
        for (int x = 0; x < tw; x++) {
            int x0 = x * b->W / tw, x1 = (x + 1) * b->W / tw;
            u32 s[3] = {0, 0, 0}, k = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++, k++) {
                    u32 p = full[(size_t)yy * b->W + xx];
                    s[0] += p & 0xff;
                    s[1] += p >> 8 & 0xff;
                    s[2] += p >> 16 & 0xff;
                }
            if (!k) k = 1;
            t->px[(size_t)y * tw + x] = 0xff000000u | (s[2] / k) << 16 | (s[1] / k) << 8 | s[0] / k;
        }
    }
    return t;
}

static Image *make_thumb(BX *b, const int *ids, int count) {
    int tw, th;
    bx_thumb_size(b, &tw, &th);
    u32 *full = bx_alloc(sizeof(u32) * (size_t)b->W * (size_t)b->H);
    compose(b, ids, count, full, false, false, false);
    Image *t = bx_scaled(b, full, tw, th);
    free(full);
    blur_masked(b, t, ids, count);
    return t;
}

int bx_museum_cg_pages(BX *b) { return b->museum ? b->museum->pageCount : 0; }

int bx_museum_cg_thumbs(BX *b, int page) { return b->museum && page >= 0 && page < b->museum->pageCount ? b->museum->pages[page].count : 0; }

int bx_museum_cg_seen_variant(BX *b, int page, int thumb) {
    if (thumb < 0 || thumb >= bx_museum_cg_thumbs(b, page)) return -1;
    const Thumb *t = &b->museum->pages[page].t[thumb];
    const Variant *v = first_seen(b, t);
    return v ? (int)(v - t->v) : -1;
}

Image *bx_museum_cg_thumb(BX *b, int page, int thumb) {
    int v = bx_museum_cg_seen_variant(b, page, thumb);
    if (v < 0) return NULL;
    const Variant *var = &b->museum->pages[page].t[thumb].v[v];
    return make_thumb(b, var->ids, var->count);
}

void bx_museum_cg_compose(BX *b, int page, int thumb, u32 *dst, bool hairOff, bool cutOff) {
    int v = bx_museum_cg_seen_variant(b, page, thumb);
    if (v < 0) return;
    const Variant *var = &b->museum->pages[page].t[thumb].v[v];
    compose(b, var->ids, var->count, dst, hairOff, cutOff, false);
}

static int thumb_count(BX *b, const MuseumPhase *u) {
    Museum *m = b->museum;
    if (u->mode == MM_CG) return u->page < m->pageCount ? m->pages[u->page].count : 0;
    int n = 0;
    for (int i = 0; i < m->sceneCount; i++)
        if (m->scenes[i].page == u->page && m->scenes[i].index + 1 > n) n = m->scenes[i].index + 1;
    return n;
}

static int scene_at(BX *b, int page, int index) {
    Museum *m = b->museum;
    for (int i = 0; i < m->sceneCount; i++)
        if (m->scenes[i].page == page && m->scenes[i].index == index) return i;
    return -1;
}

static const Variant *first_seen(BX *b, const Thumb *t) {
    for (int v = 0; v < t->count; v++)
        if (variant_seen(b, &t->v[v])) return &t->v[v];
    return NULL;
}

static Image *scene_thumb(BX *b, int s) {
    Museum *m = b->museum;
    return s >= 0 && scene_open(b, s) ? make_thumb(b, m->scenes[s].ids, m->scenes[s].count) : NULL;
}

static void make_thumbs(BX *b, MuseumPhase *u) {
    Museum *m = b->museum;
    Playlist *l = &u->list;
    if (l->open) {
        while (l->made < m->listLen[m->listSel] && l->made < l->max) {
            int i = l->made++;
            int *e = m->list[m->listSel][i];
            if ((l->thumbs[i] = scene_thumb(b, scene_at(b, e[0], e[1])))) return;
        }
        return;
    }
    int n = thumb_count(b, u);
    while (u->thumbsMade < n && u->thumbsMade < u->thumbsPerPage) {
        int i = u->thumbsMade++;
        if (u->mode == MM_CG) {
            const Variant *v = first_seen(b, &m->pages[u->page].t[i]);
            if (v) {
                u->thumbs[i] = make_thumb(b, v->ids, v->count);
                return;
            }
        } else if ((u->thumbs[i] = scene_thumb(b, scene_at(b, u->page, i)))) return;
    }
}

static void set_page(MuseumPhase *u, int page) {
    free_images(u->thumbs, 32);
    u->page = page;
    u->thumbsMade = 0;
}

static int draw_digits(BX *b, u32 *dst, Image *img, int frames, int value, int digits, int x, int y, int space) {
    int h = img->h / frames, i = 0;
    do {
        bx_blit_part(dst, b->W, b->H, img, 0, (value % 10) * h, img->w, h, x, y, 255);
        value /= 10;
        x += space;
    } while (++i < digits || value);
    return x;
}

static void view_variant(BX *b, MuseumPhase *u) {
    Museum *m = b->museum;
    Viewer *v = &u->view;
    const Thumb *t = &m->pages[u->page].t[v->thumb];
    if (!v->pixels) v->pixels = bx_alloc(sizeof(u32) * (size_t)b->W * (size_t)b->H);
    compose(b, t->v[v->variant].ids, t->v[v->variant].count, v->pixels, v->hairOff, v->cutOff, v->steamOff);
}

static bool step_variant(BX *b, MuseumPhase *u, int dir) {
    Museum *m = b->museum;
    Viewer *v = &u->view;
    const Thumb *t = &m->pages[u->page].t[v->thumb];
    for (int i = v->variant + dir; i >= 0 && i < t->count; i += dir)
        if (variant_seen(b, &t->v[i])) {
            v->variant = i;
            view_variant(b, u);
            return true;
        }
    return false;
}

static void number_pos(BX *b, const int *panel, const char *pos, const char *space, int *out) {
    int xy[2] = {0, 0}, sp[2] = {-10, 0};
    bx_cfg_ints(&b->cfg, "cg viewer", pos, xy, 2);
    bx_cfg_ints(&b->cfg, "cg viewer", space, sp, 2);
    out[0] = panel[0] + xy[0];
    out[1] = panel[1] + xy[1];
    out[2] = sp[0];
}

static void open_viewer(BX *b, MuseumPhase *u, int thumb, int variant) {
    Viewer *v = &u->view;
    close_viewer(v);
    const char *sec = "cg viewer";
    int pos[2] = {0, 0};
    bx_cfg_ints(&b->cfg, sec, "panel pos", pos, 2);
    v->px = pos[0];
    v->py = pos[1];
    v->base = own(b, bx_cfg_get(&b->cfg, sec, "base"));
    bx_mbtn_cfg(b, &v->prev, sec, "prev arrow", pos[0], pos[1]);
    bx_mbtn_cfg(b, &v->next, sec, "next arrow", pos[0], pos[1]);
    bx_mbtn_cfg(b, &v->cut, sec, "cut_in", pos[0], pos[1]);
    bx_mbtn_cfg(b, &v->hair, sec, "hair", pos[0], pos[1]);
    bx_mbtn_cfg(b, &v->exit, sec, "exit", pos[0], pos[1]);
    bx_mbtn_cfg(b, &v->steam, sec, "steam", pos[0], pos[1]);
    bx_mbtn_cfg(b, &v->ahe, sec, "ahe", pos[0], pos[1]);
    if (bx_cfg_section(&b->cfg, "museum phase - bgv")) {
        bx_mbtn_cfg(b, &v->detail, sec, "bgv detail", pos[0], pos[1]);
        bx_mbtn_cfg(b, &v->stop, sec, "bgv all stop", pos[0], pos[1]);
        int x = 0, y = 0, hit[4];
        Image *bar = cfg_image(b, sec, "bgv volume", &x, &y);
        bool region = bx_cfg_ints(&b->cfg, sec, "bgv volume region", hit, 4) == 4;
        hit[0] += pos[0];
        hit[1] += pos[1];
        bx_mbar_set(&v->vol, bar, x + pos[0], y + pos[1], region ? hit : NULL);
    }
    v->digits = own(b, bx_cfg_get(&b->cfg, sec, "number"));
    v->digitFrames = clampi(bx_cfg_int(&b->cfg, sec, "number patterns", 10), 1, 16);
    number_pos(b, pos, "now cg no pos", "now cg no space", v->now);
    number_pos(b, pos, "all thumbs pos", "all thumbs space", v->all);
    v->thumb = thumb;
    v->variant = variant;
    v->open = v->panel = true;
    view_variant(b, u);
}

static void stop_museum_sound(BX *b) {
    bx_snd_stop(b, CH_BGM, 300);
    bx_snd_loop_stop(b, CH_BGS, NULL, 0);
    bx_snd_loop_stop(b, CH_BGV, NULL, 0);
    b->bgmName[0] = 0;
}

static void play_scene(BX *b, int s) {
    Museum *m = b->museum;
    int l = bx_script_label(b->sc, m->scenes[s].label);
    if (l < 0) {
        bx_log(b, "no scene label %s", m->scenes[s].label);
        return;
    }
    if (b->museumPhase) b->replayPage = b->museumPhase->page;
    bx_museum_close(b);
    stop_museum_sound(b);
    memset(&b->scene, 0, sizeof b->scene);
    b->shownDirty = true;
    b->replaying = true;
    b->th.sp = 0;
    b->th.wait = W_NONE;
    b->th.skipLabel = -1;
    bx_vm_goto(b, l);
    bx_log(b, "scene replay %s", m->scenes[s].label);
}

static bool play_next(BX *b) {
    Museum *m = b->museum;
    while (m->queuePos < m->queueLen) {
        int *e = m->queue[m->queuePos++];
        int s = scene_at(b, e[0], e[1]);
        if (s >= 0 && scene_open(b, s) && bx_script_label(b->sc, m->scenes[s].label) >= 0) {
            play_scene(b, s);
            return true;
        }
    }
    m->queueLen = m->queuePos = 0;
    return false;
}

void bx_museum_replay_end(BX *b) {
    if (!b->replaying) return;
    b->replaying = false;
    stop_museum_sound(b);
    bx_msg_clear(b);
    memset(&b->scene, 0, sizeof b->scene);
    b->shownDirty = true;
    if (play_next(b)) return;
    bx_museum_open(b);
    MuseumPhase *u = b->museumPhase;
    if (u) {
        open_mode(b, u, MM_SCENE);
        set_page(u, clampi(b->replayPage, 0, u->pageCount ? u->pageCount - 1 : 0));
    }
}

bool bx_museum_open(BX *b) {
    if (!b->museum) {
        bx_log(b, "no cg.dat for the museum");
        return false;
    }
    bx_museum_close(b);
    MuseumPhase *u = bx_alloc(sizeof *u);
    b->museumPhase = u;
    const char *sec = "museum phase";
    u->base = own(b, bx_cfg_get(&b->cfg, sec, "base"));
    int count = 0;
    for (int i = 0; i < MM_COUNT; i++) {
        char key[24];
        snprintf(key, sizeof key, "mode%d", i);
        MBtn t;
        if (!bx_mbtn_cfg(b, &t, sec, key, 0, 0)) continue;
        const char *name = bx_cfg_get(&b->cfg, sec, key);
        int mode = strstr(name, "music") ? MM_BGM : strstr(name, "bgv") ? MM_BGV : strstr(name, "_cg") ? MM_CG : strstr(name, "scene") ? MM_SCENE : MM_STRIP;
        snprintf(key, sizeof key, "mode%d state", i);
        const char *bits = bx_cfg_get(&b->cfg, sec, key);
        u->modes[count] = t;
        u->modeBits[count] = bits ? (u32)strtoul(bits, NULL, 0) : 0xffffffffu;
        u->modeOf[count++] = mode;
    }
    for (int i = count; i < MM_COUNT; i++) u->modeOf[i] = -1;
    bx_mbtn_cfg(b, &u->back, sec, "return to title", 0, 0);
    b->phaseBack = b->phase;
    b->phase = PH_MUSEUM;
    open_mode(b, u, MM_BGM);
    return true;
}

void bx_museum_close(BX *b) {
    MuseumPhase *u = b->museumPhase;
    if (!u) return;
    free_mode(u);
    bx_image_free(u->base);
    for (int i = 0; i < MM_COUNT; i++) bx_mbtn_free(&u->modes[i]);
    bx_mbtn_free(&u->back);
    free(u);
    b->museumPhase = NULL;
    if (b->phase == PH_MUSEUM) b->phase = PH_NONE;
}

static bool mode_available(BX *b, const MuseumPhase *u, int i) {
    return (b->museum->state & u->modeBits[i]) && (u->modeOf[i] != MM_BGV || bx_cfg_section(&b->cfg, "museum phase - bgv")) &&
           (u->modeOf[i] != MM_STRIP || bx_cfg_get(&b->cfg, "museum phase - bu", "layout"));
}

static int thumb_at(BX *b, MuseumPhase *u) {
    int tw, th;
    bx_thumb_size(b, &tw, &th);
    int n = thumb_count(b, u);
    for (int i = 0; i < n && i < u->thumbsPerPage; i++)
        if (b->mx >= u->thumbPos[i][0] && b->my >= u->thumbPos[i][1] && b->mx < u->thumbPos[i][0] + tw && b->my < u->thumbPos[i][1] + th) return i;
    return -1;
}

static void play_track(BX *b, MuseumPhase *u, int i) {
    if (i < 0 || i >= u->trackCount || !u->trackNames[i][0]) return;
    u->track = i;
    u->playing = true;
    snprintf(b->bgmName, sizeof b->bgmName, "%s", u->trackNames[i]);
    bx_snd_play(b, CH_BGM, RX_BGM, u->trackNames[i], true, 0);
}

static void set_bgm_volume(BX *b, int v) {
    b->volBgm = clampi(v, 0, 100);
    bx_settings_apply(b);
}

static void leave(BX *b) {
    if (b->museumPhase->playing) stop_museum_sound(b);
    else bx_snd_loop_stop(b, CH_BGV, NULL, 0);
    bx_settings_save(b);
    bx_museum_close(b);
    bx_phase_start(b, PH_TITLE);
}

static void open_list(BX *b, MuseumPhase *u) {
    Playlist *l = &u->list;
    close_list(l);
    const char *sec = "scene playlist";
    l->base = own(b, bx_cfg_get(&b->cfg, sec, "base"));
    l->bar = cfg_image(b, sec, "scroll bar", &l->barX, &l->barY);
    bx_mbtn_cfg(b, &l->play, sec, "play button", 0, 0);
    bx_mbtn_cfg(b, &l->close, sec, "close button", 0, 0);
    for (int i = 0; i < LISTS; i++) {
        char key[24];
        snprintf(key, sizeof key, "page button%d", i);
        bx_mbtn_cfg(b, &l->pages[i], sec, key, 0, 0);
    }
    bx_cfg_ints(&b->cfg, sec, "thumb", l->thumb, 4);
    l->cols = clampi(bx_cfg_int(&b->cfg, sec, "thumb x count", 5), 1, 16);
    l->max = clampi(bx_cfg_int(&b->cfg, sec, "slot max", 25), 1, LIST_MAX);
    l->open = true;
}

static int list_at(BX *b, const Playlist *l, int count) {
    int tw, th;
    bx_thumb_size(b, &tw, &th);
    for (int i = 0; i < count && i < l->max; i++) {
        int x = l->thumb[0] + i % l->cols * l->thumb[2], y = l->thumb[1] + i / l->cols * l->thumb[3];
        if (b->mx >= x && b->my >= y && b->mx < x + tw && b->my < y + th) return i;
    }
    return -1;
}

static void list_update(BX *b, MuseumPhase *u) {
    Museum *m = b->museum;
    Playlist *l = &u->list;
    if (b->rclick) {
        b->rclick = false;
        close_list(l);
        return;
    }
    if (!b->click) return;
    b->click = false;
    if (bx_mbtn_on(b, &l->close)) {
        bx_snd_play(b, CH_SYS, RX_SYS, "cancel", false, 0);
        close_list(l);
        return;
    }
    if (bx_mbtn_on(b, &l->play)) {
        if (!m->listLen[m->listSel]) return;
        memcpy(m->queue, m->list[m->listSel], sizeof m->queue);
        m->queueLen = m->listLen[m->listSel];
        m->queuePos = 0;
        play_next(b);
        return;
    }
    for (int i = 0; i < LISTS; i++)
        if (bx_mbtn_on(b, &l->pages[i])) {
            if (i == m->listSel) return;
            m->listSel = i;
            free_images(l->thumbs, LIST_MAX);
            l->made = 0;
            save_lists(b);
            return;
        }
    int n = m->listLen[m->listSel], k = list_at(b, l, n);
    if (k < 0) return;
    memmove(m->list[m->listSel][k], m->list[m->listSel][k + 1], sizeof m->list[0][0] * (size_t)(n - k - 1));
    m->listLen[m->listSel]--;
    free_images(l->thumbs, LIST_MAX);
    l->made = 0;
    save_lists(b);
}

static void set_bgv_volume(BX *b, int v) {
    b->volBgv = clampi(v, 0, 100);
    bx_settings_apply(b);
}

static void viewer_update(BX *b, MuseumPhase *u) {
    Viewer *v = &u->view;
    if (v->dragVol) {
        if (b->lDown) set_bgv_volume(b, bx_mbar_value(b, &v->vol));
        else {
            v->dragVol = false;
            bx_settings_save(b);
        }
    }
    if (v->panel && b->lPress && bx_mbar_on(b, &v->vol)) {
        v->dragVol = true;
        set_bgv_volume(b, bx_mbar_value(b, &v->vol));
    }
    if (b->rclick) {
        b->rclick = false;
        v->panel = !v->panel;
        return;
    }
    if (!b->click) return;
    b->click = false;
    if (v->panel) {
        if (bx_mbar_on(b, &v->vol)) return;
        if (bx_mbtn_on(b, &v->prev) || bx_mbtn_on(b, &v->next)) {
            step_variant(b, u, bx_mbtn_on(b, &v->prev) ? -1 : 1);
            return;
        }
        if (bx_mbtn_on(b, &v->hair) || bx_mbtn_on(b, &v->cut) || bx_mbtn_on(b, &v->steam)) {
            if (bx_mbtn_on(b, &v->hair)) v->hairOff = !v->hairOff;
            else if (bx_mbtn_on(b, &v->cut)) v->cutOff = !v->cutOff;
            else v->steamOff = !v->steamOff;
            view_variant(b, u);
            return;
        }
        if (bx_mbtn_on(b, &v->exit)) {
            close_viewer(v);
            return;
        }
        if (bx_mbtn_on(b, &v->stop)) {
            bx_snd_loop_stop(b, CH_BGV, NULL, 300);
            return;
        }
        if (bx_mbtn_on(b, &v->detail)) {
            u->bgv = bx_bgv_open(b, true);
            u->bgvOverlay = u->bgv != NULL;
            return;
        }
        if (v->base && b->mx >= v->px && b->my >= v->py && b->mx < v->px + v->base->w && b->my < v->py + v->base->h) return;
    }
    v->panel = true;
    if (!step_variant(b, u, 1)) close_viewer(v);
}

static void bgm_update(BX *b, MuseumPhase *u) {
    if (u->dragVol) {
        if (b->lDown) set_bgm_volume(b, bx_mbar_value(b, &u->vol));
        else {
            u->dragVol = false;
            bx_settings_save(b);
        }
    }
    if (b->lPress && bx_mbar_on(b, &u->vol)) {
        u->dragVol = true;
        set_bgm_volume(b, bx_mbar_value(b, &u->vol));
    }
    if (u->playing && !bx_snd_playing(b, CH_BGM)) u->playing = false;
    if (!b->click) return;
    for (int i = 0; i < u->trackCount; i++)
        if (bx_mbtn_on(b, &u->tracks[i])) {
            b->click = false;
            play_track(b, u, i);
            return;
        }
    int n = u->trackCount ? u->trackCount : 1;
    if (bx_mbtn_on(b, &u->play)) play_track(b, u, u->track);
    else if (bx_mbtn_on(b, &u->stop)) {
        bx_snd_stop(b, CH_BGM, 300);
        b->bgmName[0] = 0;
        u->playing = false;
    } else if (bx_mbtn_on(b, &u->prev)) play_track(b, u, (u->track + n - 1) % n);
    else if (bx_mbtn_on(b, &u->next)) play_track(b, u, (u->track + 1) % n);
    else if (bx_mbtn_on(b, &u->volMin) || bx_mbtn_on(b, &u->volMax)) {
        set_bgm_volume(b, b->volBgm + (bx_mbtn_on(b, &u->volMin) ? -10 : 10));
        bx_settings_save(b);
    } else return;
    b->click = false;
}

static void grid_update(BX *b, MuseumPhase *u) {
    Museum *m = b->museum;
    for (int i = 0; i < u->pageCount; i++)
        if (bx_mbtn_on(b, &u->pages[i])) {
            if (i != u->page) set_page(u, i);
            return;
        }
    if (u->mode == MM_SCENE) {
        if (bx_mbtn_on(b, &u->playNow) || bx_mbtn_on(b, &u->addList)) {
            u->addMode = bx_mbtn_on(b, &u->addList);
            return;
        }
        if (bx_mbtn_on(b, &u->openList)) {
            open_list(b, u);
            return;
        }
    }
    if (u->hover < 0) return;
    if (u->mode == MM_CG) {
        const Variant *v = first_seen(b, &m->pages[u->page].t[u->hover]);
        if (v) open_viewer(b, u, u->hover, (int)(v - m->pages[u->page].t[u->hover].v));
        return;
    }
    int s = scene_at(b, u->page, u->hover);
    if (s < 0 || !scene_open(b, s)) return;
    if (!u->addMode) {
        m->queueLen = m->queuePos = 0;
        play_scene(b, s);
        return;
    }
    int max = clampi(bx_cfg_int(&b->cfg, "scene playlist", "slot max", 25), 1, LIST_MAX);
    if (!m->listOff || m->listLen[m->listSel] >= max) return;
    int *e = m->list[m->listSel][m->listLen[m->listSel]++];
    e[0] = u->page;
    e[1] = u->hover;
    e[2] = 0;
    save_lists(b);
    bx_snd_play(b, CH_SYS, RX_SYS, "check", false, 0);
}

void bx_museum_update(BX *b) {
    MuseumPhase *u = b->museumPhase;
    if (!u) return;
    if (u->strip) {
        if (bx_strip_update(b, u->strip)) {
            bx_snd_loop_stop(b, CH_BGV, NULL, 300);
            open_mode(b, u, MM_BGM);
        }
        b->click = b->rclick = false;
        return;
    }
    if (u->bgv) {
        bool closed = bx_bgv_update(b, u->bgv);
        if (u->bgvOverlay) {
            if (closed) {
                bx_bgv_free(u->bgv);
                u->bgv = NULL;
                u->bgvOverlay = false;
            }
            b->click = b->rclick = false;
            return;
        }
    }
    if (u->view.open) {
        viewer_update(b, u);
        return;
    }
    if (u->mode == MM_CG || u->mode == MM_SCENE || u->list.open) make_thumbs(b, u);
    if (u->list.open) {
        list_update(b, u);
        return;
    }
    u->hover = (u->mode == MM_CG || u->mode == MM_SCENE) ? thumb_at(b, u) : -1;
    if (u->mode == MM_BGM) bgm_update(b, u);
    if (b->rclick) {
        b->rclick = false;
        leave(b);
        return;
    }
    if (!b->click) return;
    b->click = false;
    if (bx_mbtn_on(b, &u->back)) {
        bx_snd_play(b, CH_SYS, RX_SYS, "cancel", false, 0);
        leave(b);
        return;
    }
    for (int i = 0; i < MM_COUNT && u->modeOf[i] >= 0; i++)
        if (bx_mbtn_on(b, &u->modes[i])) {
            if (mode_available(b, u, i) && u->modeOf[i] != u->mode) {
                bx_snd_play(b, CH_SYS, RX_SYS, "check", false, 0);
                open_mode(b, u, u->modeOf[i]);
            }
            return;
        }
    if (u->mode == MM_CG || u->mode == MM_SCENE) grid_update(b, u);
}

static void viewer_draw(BX *b, MuseumPhase *u, u32 *dst) {
    Museum *m = b->museum;
    Viewer *v = &u->view;
    memcpy(dst, v->pixels, sizeof(u32) * (size_t)b->W * (size_t)b->H);
    if (!v->panel) return;
    if (v->base) bx_blit(dst, b->W, b->H, v->base, v->px, v->py, 255);
    bx_mbar_draw(b, dst, &v->vol, b->volBgv);
    MBtn *plain[] = {&v->prev, &v->next, &v->exit, &v->detail, &v->stop, &v->ahe};
    for (size_t i = 0; i < sizeof plain / sizeof *plain; i++) bx_mbtn_draw(b, dst, plain[i], bx_mbtn_state(b, plain[i]));
    bx_mbtn_draw(b, dst, &v->hair, (v->hairOff ? 2 : 0) + bx_mbtn_state(b, &v->hair));
    bx_mbtn_draw(b, dst, &v->cut, (v->cutOff ? 0 : 2) + bx_mbtn_state(b, &v->cut));
    bx_mbtn_draw(b, dst, &v->steam, (v->steamOff ? 0 : 2) + bx_mbtn_state(b, &v->steam));
    if (v->digits) {
        const Thumb *t = &m->pages[u->page].t[v->thumb];
        draw_digits(b, dst, v->digits, v->digitFrames, v->variant + 1, 2, v->now[0], v->now[1], v->now[2]);
        draw_digits(b, dst, v->digits, v->digitFrames, t->count, 2, v->all[0], v->all[1], v->all[2]);
    }
}

static void draw_thumb(BX *b, u32 *dst, const Image *t, int x, int y, bool lit) {
    for (int j = 0; j < t->h; j++) {
        int py = y + j;
        if (py < 0 || py >= b->H) continue;
        for (int i = 0; i < t->w; i++) {
            int px = x + i;
            if (px < 0 || px >= b->W) continue;
            u32 p = t->px[(size_t)j * t->w + i];
            dst[(size_t)py * b->W + px] = lit ? p : 0xff000000u | (p >> 1 & 0x7f7f7f);
        }
    }
}

static void list_draw(BX *b, MuseumPhase *u, u32 *dst) {
    Museum *m = b->museum;
    Playlist *l = &u->list;
    int hover = list_at(b, l, m->listLen[m->listSel]);
    if (l->base) bx_blit(dst, b->W, b->H, l->base, l->base->x, l->base->y, 255);
    for (int i = 0; i < m->listLen[m->listSel] && i < l->max; i++)
        if (l->thumbs[i]) draw_thumb(b, dst, l->thumbs[i], l->thumb[0] + i % l->cols * l->thumb[2], l->thumb[1] + i / l->cols * l->thumb[3], i == hover);
    if (l->bar) bx_blit(dst, b->W, b->H, l->bar, l->barX, l->barY, 255);
    for (int i = 0; i < LISTS; i++) bx_mbtn_draw(b, dst, &l->pages[i], i == m->listSel ? 2 : bx_mbtn_state(b, &l->pages[i]));
    bx_mbtn_draw(b, dst, &l->play, bx_mbtn_state(b, &l->play));
    bx_mbtn_draw(b, dst, &l->close, bx_mbtn_state(b, &l->close));
}

static void grid_draw(BX *b, MuseumPhase *u, u32 *dst) {
    Museum *m = b->museum;
    int n = thumb_count(b, u);
    for (int i = 0; i < u->frameCount && i < n; i++) bx_blit(dst, b->W, b->H, u->frames[i], u->frames[i]->x, u->frames[i]->y, 255);
    for (int i = 0; i < n && i < u->thumbsPerPage; i++)
        if (u->thumbs[i]) draw_thumb(b, dst, u->thumbs[i], u->thumbPos[i][0], u->thumbPos[i][1], i == u->hover);
    if (u->over) bx_blit(dst, b->W, b->H, u->over, u->over->x, u->over->y, 255);
    for (int i = 0; i < u->pageCount; i++) bx_mbtn_draw(b, dst, &u->pages[i], i == u->page ? 2 : bx_mbtn_state(b, &u->pages[i]));
    if (u->mode == MM_CG && u->parts && u->partsFrames >= 11)
        for (int i = 0; i < n && i < u->thumbsPerPage; i++) {
            const Thumb *t = &m->pages[u->page].t[i];
            int got = 0;
            for (int v = 0; v < t->count; v++) got += variant_seen(b, &t->v[v]);
            int h = u->parts->h / u->partsFrames, x = u->thumbPos[i][0] + u->partsX, y = u->thumbPos[i][1] + u->partsY;
            x = draw_digits(b, dst, u->parts, u->partsFrames, t->count, 1, x, y, u->partsSpace);
            bx_blit_part(dst, b->W, b->H, u->parts, 0, 10 * h, u->parts->w, h, x, y, 255);
            draw_digits(b, dst, u->parts, u->partsFrames, got, 1, x + u->partsSpace, y, u->partsSpace);
        }
    if (u->percent) draw_digits(b, dst, u->percent, u->percentFrames, bx_museum_rate(b, u->mode == MM_CG ? "cg" : "scene"), 3, u->percentX, u->percentY, u->percentSpace);
    if (u->mode == MM_SCENE) {
        bx_mbtn_draw(b, dst, &u->playNow, !u->addMode ? 2 : bx_mbtn_state(b, &u->playNow));
        bx_mbtn_draw(b, dst, &u->addList, u->addMode ? 2 : bx_mbtn_state(b, &u->addList));
        bx_mbtn_draw(b, dst, &u->openList, bx_mbtn_state(b, &u->openList));
    }
}

static void bgm_draw(BX *b, MuseumPhase *u, u32 *dst) {
    if (u->over) bx_blit(dst, b->W, b->H, u->over, u->over->x, u->over->y, 255);
    for (int i = 0; i < u->trackCount; i++) bx_mbtn_draw(b, dst, &u->tracks[i], i == u->track ? 2 : bx_mbtn_state(b, &u->tracks[i]));
    bx_mbtn_draw(b, dst, &u->play, u->playing ? 2 : bx_mbtn_state(b, &u->play));
    MBtn *plain[] = {&u->stop, &u->prev, &u->next, &u->volMin, &u->volMax};
    for (size_t i = 0; i < sizeof plain / sizeof *plain; i++) bx_mbtn_draw(b, dst, plain[i], bx_mbtn_state(b, plain[i]));
    bx_mbar_draw(b, dst, &u->vol, b->volBgm);
}

void bx_museum_draw(BX *b, u32 *dst) {
    MuseumPhase *u = b->museumPhase;
    if (!u) return;
    if (u->strip) {
        bx_strip_draw(b, u->strip, dst);
        return;
    }
    if (u->view.open && !u->bgvOverlay) {
        viewer_draw(b, u, dst);
        return;
    }
    if (u->base) bx_blit(dst, b->W, b->H, u->base, u->base->x, u->base->y, 255);
    if (u->bgv) bx_bgv_draw(b, u->bgv, dst);
    else {
        if (u->under) bx_blit(dst, b->W, b->H, u->under, u->under->x, u->under->y, 255);
        if (u->mode == MM_CG || u->mode == MM_SCENE) grid_draw(b, u, dst);
        if (u->mode == MM_BGM) bgm_draw(b, u, dst);
    }
    int mode = u->bgvOverlay ? MM_BGV : u->mode;
    for (int i = 0; i < MM_COUNT && u->modeOf[i] >= 0; i++) {
        int frame = u->modeOf[i] == mode ? 2 : u->bgvOverlay || !mode_available(b, u, i) ? 3 : bx_mbtn_state(b, &u->modes[i]);
        bx_mbtn_draw(b, dst, &u->modes[i], frame);
    }
    if (!u->bgvOverlay) bx_mbtn_draw(b, dst, &u->back, bx_mbtn_state(b, &u->back));
    if (u->list.open) list_draw(b, u, dst);
}

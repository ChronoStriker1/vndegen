#include "bsx.h"

#include <sys/stat.h>

#define SLOTS_MAX 32
#define PAGES_MAX 20

typedef struct Btn {
    Image *img;
    int frames, x, y;
} Btn;

typedef struct SlotCache {
    bool loaded, exists;
    Image *thumb;
    char date[32], clock[32];
    int day;
    char *text;
    long long mtime;
} SlotCache;

struct DataPhase {
    bool save;
    int page, pages, perPage, first;
    int sort;
    int hover;
    int confirmSlot;
    int hoverBtn;
    Image *base[2], *empty, *frame, *autoOnly, *radio, *font, *noHeader, *week, *wday, *resave, *dayNo, *dayUnit, *daySp;
    int noHeaderPos[2], dayNoPos[2], dayNoOff, dayNoFrames, dayUnitPos[2], daySpPos[2], daySpFrames, daySpBorder;
    Btn pageBtn[PAGES_MAX], mode[2], back, title, quit, ok, cancel;
    int slotPos[SLOTS_MAX][2];
    int sortPos[3][2];
    int frameOff[2];
    int thumbPos[2], textArea[4], noPos[2], noOff, datePos[2], dateOff, timePos[2], timeOff, weekPos[2], wdayPos[2];
    int dayAdjust;
    SlotCache *cache;
    int cacheCount;
    int *order;
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

static void btn(BX *b, const char *sec, const char *key, Btn *bt) {
    memset(bt, 0, sizeof *bt);
    const char *v = bx_cfg_get(&b->cfg, sec, key);
    if (!v) return;
    char name[64], f[64];
    bx_cfg_field(v, 0, name, sizeof name);
    bt->img = own(b, name);
    bt->frames = bx_cfg_field(v, 1, f, sizeof f) > 0 ? bx_eval(f, NULL) : 1;
    if (bt->frames < 1) bt->frames = 1;
    bt->x = bx_cfg_field(v, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    bt->y = bx_cfg_field(v, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
}

static bool in_btn(const Btn *bt, int x, int y) {
    if (!bt->img) return false;
    int h = bt->img->h / bt->frames;
    return x >= bt->x && y >= bt->y && x < bt->x + bt->img->w && y < bt->y + h;
}

static void draw_btn(BX *b, u32 *dst, const Btn *bt, int frame) {
    if (!bt->img) return;
    int h = bt->img->h / bt->frames;
    frame = clampi(frame, 0, bt->frames - 1);
    bx_blit_part(dst, b->W, b->H, bt->img, 0, frame * h, bt->img->w, h, bt->x, bt->y, 255);
}

static void pair(BX *b, const char *sec, const char *key, int *out, int dx, int dy) {
    int v[2] = {dx, dy};
    bx_cfg_ints(&b->cfg, sec, key, v, 2);
    out[0] = v[0];
    out[1] = v[1];
}

static int slot_no(DataPhase *d, int i) { return d->order ? d->order[i] : i; }

static void slot_info(BX *b, DataPhase *d, int slot) {
    SlotCache *c = &d->cache[slot];
    if (c->loaded) return;
    c->loaded = true;
    char *path = bx_save_path(b, slot);
    char real[2048];
    struct stat st;
    if (bx_path_resolve(path, real, sizeof real) > 0 && !stat(real, &st)) c->mtime = (long long)st.st_mtime;
    size_t n;
    u8 *f = bx_read_file(path, &n);
    free(path);
    int tw, th;
    bx_thumb_size(b, &tw, &th);
    if (!f || n < 36 + (size_t)tw * th * 4 + 592 || rd32(f + 32) != (u32)(tw * th * 4 + 592)) {
        free(f);
        return;
    }
    c->exists = true;
    c->thumb = bx_image_new(tw, th);
    for (int y = 0; y < th; y++)
        for (int x = 0; x < tw; x++) {
            const u8 *p = f + 36 + ((size_t)(th - 1 - y) * tw + x) * 4;
            c->thumb->px[(size_t)y * tw + x] = (u32)p[2] | (u32)p[1] << 8 | (u32)p[0] << 16 | 0xff000000u;
        }
    const u8 *info = f + 36 + (size_t)tw * th * 4;
    char *date = bx_utf16_to_utf8(info, 16), *clock = bx_utf16_to_utf8(info + 32, 16);
    snprintf(c->date, sizeof c->date, "%s", date);
    snprintf(c->clock, sizeof c->clock, "%s", clock);
    free(date);
    free(clock);
    c->day = (int)rd32(info + 64);
    size_t len = 0;
    while (len < 256 && (info[80 + 2 * len] || info[81 + 2 * len])) len++;
    c->text = bx_utf16_to_utf8(info + 80, len);
    for (char *p = c->text; *p; p++)
        if (p[0] == '\\' && p[1] == 'n') {
            p[0] = ' ';
            memmove(p + 1, p + 2, strlen(p + 2) + 1);
        }
    free(f);
}

static void forget(DataPhase *d) {
    for (int i = 0; i < d->cacheCount; i++) {
        bx_image_free(d->cache[i].thumb);
        free(d->cache[i].text);
        memset(&d->cache[i], 0, sizeof d->cache[i]);
    }
}

static DataPhase *g_sort_phase;

static int by_time(const void *pa, const void *pb) {
    const SlotCache *a = &g_sort_phase->cache[*(const int *)pa], *b = &g_sort_phase->cache[*(const int *)pb];
    if (a->exists != b->exists) return a->exists ? -1 : 1;
    long long d = g_sort_phase->sort == 1 ? b->mtime - a->mtime : a->mtime - b->mtime;
    return d < 0 ? -1 : d > 0;
}

static void reorder(BX *b, DataPhase *d) {
    for (int i = 0; i < d->cacheCount; i++) {
        d->order[i] = i;
        slot_info(b, d, i);
    }
    if (d->sort) {
        g_sort_phase = d;
        qsort(d->order + d->first, (size_t)(d->cacheCount - d->first), sizeof(int), by_time);
    }
}

void bx_data_free(BX *b) {
    DataPhase *d = b->data;
    if (!d) return;
    forget(d);
    free(d->cache);
    free(d->order);
    Image *ims[] = {d->base[0], d->base[1], d->empty, d->frame, d->autoOnly, d->radio, d->font, d->noHeader, d->week, d->wday, d->resave, d->dayNo, d->dayUnit, d->daySp};
    for (size_t i = 0; i < sizeof ims / sizeof *ims; i++) bx_image_free(ims[i]);
    Btn *bs[] = {&d->mode[0], &d->mode[1], &d->back, &d->title, &d->quit, &d->ok, &d->cancel};
    for (size_t i = 0; i < sizeof bs / sizeof *bs; i++) bx_image_free(bs[i]->img);
    for (int i = 0; i < PAGES_MAX; i++) bx_image_free(d->pageBtn[i].img);
    free(d);
    b->data = NULL;
}

static Image *sv_image(BX *b, const char *sec, const char *key, int *frames, int *pos) {
    const char *v = bx_cfg_get(&b->cfg, sec, key);
    char name[64], f[32];
    if (!v || bx_cfg_field(v, 0, name, sizeof name) <= 0) return NULL;
    if (frames) *frames = bx_cfg_field(v, 1, f, sizeof f) > 0 ? clampi(bx_eval(f, NULL), 1, 64) : 1;
    pos[0] = bx_cfg_field(v, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    pos[1] = bx_cfg_field(v, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    return own(b, name);
}

void bx_data_open(BX *b, bool save) {
    bx_data_free(b);
    DataPhase *d = bx_alloc(sizeof *d);
    b->data = d;
    const char *sec = "data screen";
    d->save = save;
    d->base[0] = own(b, bx_cfg_get(&b->cfg, sec, "save base"));
    d->base[1] = own(b, bx_cfg_get(&b->cfg, sec, "load base"));
    d->empty = own(b, bx_cfg_get(&b->cfg, sec, "no data"));
    d->autoOnly = own(b, bx_cfg_get(&b->cfg, sec, "auto only"));
    d->radio = own(b, bx_cfg_get(&b->cfg, sec, "radio button"));
    const char *fr = bx_cfg_get(&b->cfg, sec, "select frame");
    if (fr) {
        char name[64], f[32];
        bx_cfg_field(fr, 0, name, sizeof name);
        d->frame = own(b, name);
        d->frameOff[0] = bx_cfg_field(fr, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
        d->frameOff[1] = bx_cfg_field(fr, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    }
    d->perPage = clampi(bx_cfg_int(&b->cfg, sec, "data slots", 15), 1, SLOTS_MAX);
    d->pages = clampi(bx_cfg_int(&b->cfg, sec, "page max", 10), 1, PAGES_MAX);
    d->first = bx_cfg_int(&b->cfg, sec, "no begin", 0);
    for (int i = 0; i < d->perPage; i++) {
        char key[32];
        snprintf(key, sizeof key, "slot%d pos", i);
        pair(b, sec, key, d->slotPos[i], 60 + 140 * (i % 5), 216 + 102 * (i / 5));
    }
    for (int i = 0; i < d->pages; i++) {
        char key[32];
        snprintf(key, sizeof key, "page button%d", i);
        btn(b, sec, key, &d->pageBtn[i]);
    }
    static const char *sorts[] = {"sort button0", "sort button3", "sort button2"};
    for (int i = 0; i < 3; i++) pair(b, sec, sorts[i], d->sortPos[i], 697, 125 + 20 * i);
    btn(b, sec, "save mode", &d->mode[0]);
    btn(b, sec, "load mode", &d->mode[1]);
    btn(b, sec, "back button", &d->back);
    btn(b, sec, "title button", &d->title);
    btn(b, sec, "exit button", &d->quit);
    const char *sv = "slot viewer";
    d->resave = own(b, bx_cfg_get(&b->cfg, sv, "base"));
    btn(b, sv, "ok", &d->ok);
    btn(b, sv, "cancel", &d->cancel);
    pair(b, sv, "thumbnail", d->thumbPos, 170, 97);
    int ta[4] = {300, 140, 346, 58};
    bx_cfg_ints(&b->cfg, sv, "text area", ta, 4);
    memcpy(d->textArea, ta, sizeof ta);
    d->font = own(b, bx_cfg_get(&b->cfg, sv, "numbers"));
    const char *nh = bx_cfg_get(&b->cfg, sv, "no header");
    if (nh) {
        char name[64];
        bx_cfg_field(nh, 0, name, sizeof name);
        d->noHeader = own(b, name);
        char f[32];
        d->noHeaderPos[0] = bx_cfg_field(nh, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 305;
        d->noHeaderPos[1] = bx_cfg_field(nh, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 97;
    } else {
        d->noHeaderPos[0] = 305;
        d->noHeaderPos[1] = 97;
    }
    d->dayNo = sv_image(b, sv, "day no", &d->dayNoFrames, d->dayNoPos);
    d->dayUnit = sv_image(b, sv, "day unit", NULL, d->dayUnitPos);
    d->daySp = sv_image(b, sv, "day sp", &d->daySpFrames, d->daySpPos);
    d->dayNoOff = bx_cfg_int(&b->cfg, sv, "day no offset", -9);
    d->daySpBorder = bx_cfg_int(&b->cfg, sv, "day sp border", 0);
    pair(b, sv, "slot no pos", d->noPos, 350, 96);
    d->noOff = bx_cfg_int(&b->cfg, sv, "slot no offset", -9);
    pair(b, sv, "date pos", d->datePos, 384, 96);
    d->dateOff = bx_cfg_int(&b->cfg, sv, "date offset", 9);
    pair(b, sv, "time pos", d->timePos, 494, 96);
    d->timeOff = bx_cfg_int(&b->cfg, sv, "time offset", 9);
    d->dayAdjust = bx_cfg_int(&b->cfg, sv, "day adjustment", -1);
    const char *wk = bx_cfg_get(&b->cfg, sv, "week no"), *wd = bx_cfg_get(&b->cfg, sv, "week day");
    char name[64], f[32];
    if (wk) {
        bx_cfg_field(wk, 0, name, sizeof name);
        d->week = own(b, name);
        d->weekPos[0] = bx_cfg_field(wk, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
        d->weekPos[1] = bx_cfg_field(wk, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    }
    if (wd) {
        bx_cfg_field(wd, 0, name, sizeof name);
        d->wday = own(b, name);
        d->wdayPos[0] = bx_cfg_field(wd, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
        d->wdayPos[1] = bx_cfg_field(wd, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    }
    d->cacheCount = d->pages * d->perPage;
    d->cache = bx_alloc(sizeof(SlotCache) * (size_t)d->cacheCount);
    d->order = bx_alloc(sizeof(int) * (size_t)d->cacheCount);
    d->page = 0;
    d->hover = -1;
    d->confirmSlot = -1;
    reorder(b, d);
}

static void close_data(BX *b) {
    bx_data_free(b);
    bx_phase_end(b);
}

static void do_save(BX *b, int slot) {
    DataPhase *d = b->data;
    bx_save_slot(b, slot);
    bx_snd_play(b, CH_SYS, RX_SYS, "done", false, 0);
    forget(d);
    reorder(b, d);
}

static int slot_at(BX *b, DataPhase *d, int x, int y) {
    for (int i = 0; i < d->perPage; i++) {
        int sx = d->slotPos[i][0], sy = d->slotPos[i][1];
        int tw, th;
        bx_thumb_size(b, &tw, &th);
        if (x >= sx && y >= sy && x < sx + tw && y < sy + th) return d->page * d->perPage + i;
    }
    return -1;
}

void bx_data_update(BX *b) {
    DataPhase *d = b->data;
    if (!d) return;
    if (d->confirmSlot >= 0) {
        d->hoverBtn = in_btn(&d->ok, b->mx, b->my) ? 1 : in_btn(&d->cancel, b->mx, b->my) ? 2 : 0;
        if (b->rclick) {
            b->rclick = false;
            d->confirmSlot = -1;
        } else if (b->click) {
            b->click = false;
            if (d->hoverBtn == 1) do_save(b, d->confirmSlot);
            if (d->hoverBtn) d->confirmSlot = -1;
        }
        return;
    }
    int idx = slot_at(b, d, b->mx, b->my);
    d->hover = idx >= 0 && idx < d->cacheCount ? slot_no(d, idx) : -1;
    if (b->rclick) {
        b->rclick = false;
        close_data(b);
        return;
    }
    if (b->wheel) {
        d->page = clampi(d->page - b->wheel, 0, d->pages - 1);
        b->wheel = 0;
    }
    if (!b->click) return;
    b->click = false;
    for (int i = 0; i < d->pages; i++)
        if (in_btn(&d->pageBtn[i], b->mx, b->my)) {
            d->page = i;
            return;
        }
    for (int i = 0; i < 3; i++)
        if (b->mx >= d->sortPos[i][0] - 2 && b->mx < d->sortPos[i][0] + 80 && b->my >= d->sortPos[i][1] - 2 && b->my < d->sortPos[i][1] + 14) {
            d->sort = i;
            reorder(b, d);
            return;
        }
    if (in_btn(&d->mode[0], b->mx, b->my) && b->phase != PH_TITLE_LOAD) {
        d->save = true;
        return;
    }
    if (in_btn(&d->mode[1], b->mx, b->my)) {
        d->save = false;
        return;
    }
    if (in_btn(&d->back, b->mx, b->my)) {
        close_data(b);
        return;
    }
    if (in_btn(&d->title, b->mx, b->my)) {
        close_data(b);
        bx_system(b, "@title");
        return;
    }
    if (in_btn(&d->quit, b->mx, b->my)) {
        b->quit = true;
        return;
    }
    if (d->hover < 0) return;
    int slot = d->hover;
    SlotCache *c = &d->cache[slot];
    if (d->save) {
        if (slot < d->first) return;
        if (c->exists) d->confirmSlot = slot;
        else do_save(b, slot);
    } else if (c->exists) {
        bx_snd_play(b, CH_SYS, RX_SYS, "done", false, 0);
        if (!bx_load_slot(b, slot)) close_data(b);
    }
}

static void digits(BX *b, DataPhase *d, u32 *dst, const char *s, int x, int y, int step) {
    if (!d->font) return;
    int gh = d->font->h / 12;
    for (; *s; s++) {
        int g = *s >= '0' && *s <= '9' ? *s - '0' : *s == '/' ? 10 : *s == ':' ? 11 : -1;
        if (g >= 0) bx_blit_part(dst, b->W, b->H, d->font, 0, g * gh, d->font->w, gh, x, y, 255);
        x += step;
    }
}

static void draw_info(BX *b, DataPhase *d, u32 *dst, int slot) {
    SlotCache *c = &d->cache[slot];
    if (d->noHeader) bx_blit(dst, b->W, b->H, d->noHeader, d->noHeaderPos[0], d->noHeaderPos[1], 255);
    int n = slot;
    int x = d->noPos[0];
    do {
        char one[2] = {(char)('0' + n % 10), 0};
        digits(b, d, dst, one, x, d->noPos[1], 0);
        x += d->noOff;
        n /= 10;
    } while (n);
    if (!c->exists) return;
    if (c->thumb) bx_blit(dst, b->W, b->H, c->thumb, d->thumbPos[0], d->thumbPos[1], 255);
    digits(b, d, dst, c->date, d->datePos[0], d->datePos[1], d->dateOff);
    digits(b, d, dst, c->clock, d->timePos[0], d->timePos[1], d->timeOff);
    int day = c->day + d->dayAdjust;
    if (d->daySp && (day < 0 || (d->daySpBorder > 0 && c->day >= d->daySpBorder))) {
        int h = d->daySp->h / d->daySpFrames, frame = day < 0 ? 0 : clampi(c->day - d->daySpBorder, 0, d->daySpFrames - 1);
        bx_blit_part(dst, b->W, b->H, d->daySp, 0, frame * h, d->daySp->w, h, d->daySpPos[0], d->daySpPos[1], 255);
        day = -1;
    } else if (d->dayNo && day >= 0) {
        if (d->dayUnit) bx_blit(dst, b->W, b->H, d->dayUnit, d->dayUnitPos[0], d->dayUnitPos[1], 255);
        int h = d->dayNo->h / d->dayNoFrames, x = d->dayNoPos[0], w = day / 7 + 1;
        do {
            bx_blit_part(dst, b->W, b->H, d->dayNo, 0, (w % 10) * h, d->dayNo->w, h, x, d->dayNoPos[1], 255);
            x += d->dayNoOff;
            w /= 10;
        } while (w);
    }
    if (day >= 0 && d->week) {
        int h = d->week->h / 6;
        bx_blit_part(dst, b->W, b->H, d->week, 0, clampi(day / 7, 0, 5) * h, d->week->w, h, d->weekPos[0], d->weekPos[1], 255);
    }
    if (day >= 0 && d->wday) {
        int h = d->wday->h / 7;
        bx_blit_part(dst, b->W, b->H, d->wday, 0, (day % 7) * h, d->wday->w, h, d->wdayPos[0], d->wdayPos[1], 255);
    }
    if (c->text) {
        int size = 12, width = 6;
        int x0 = d->textArea[0], y0 = d->textArea[1], w = d->textArea[2];
        char line[512];
        size_t ln = 0;
        int lx = 0, ly = 0;
        for (const char *p = c->text; *p && ly + size <= d->textArea[3];) {
            u32 cp;
            int len = bx_utf8_decode(p, &cp);
            int adv = cp < 0x2e80 ? width : width * 2;
            if (lx + adv > w) {
                line[ln] = 0;
                bx_text_draw(b, dst, b->W, b->H, line, x0, y0 + ly, size, width, 0xffffffffu, 1, 0);
                ln = 0;
                lx = 0;
                ly += size + 2;
            }
            if (ln + (size_t)len < sizeof line - 1) {
                memcpy(line + ln, p, (size_t)len);
                ln += (size_t)len;
            }
            lx += adv;
            p += len;
        }
        line[ln] = 0;
        if (ln && ly + size <= d->textArea[3]) bx_text_draw(b, dst, b->W, b->H, line, x0, y0 + ly, size, width, 0xffffffffu, 1, 0);
    }
}

void bx_data_draw(BX *b, u32 *dst) {
    DataPhase *d = b->data;
    if (!d) return;
    Image *base = d->base[d->save ? 0 : 1];
    if (base) bx_blit(dst, b->W, b->H, base, base->x, base->y, 255);
    for (int i = 0; i < d->pages; i++) draw_btn(b, dst, &d->pageBtn[i], i == d->page ? 2 : in_btn(&d->pageBtn[i], b->mx, b->my) ? 1 : 0);
    if (d->radio)
        for (int i = 0; i < 3; i++)
            if (i == d->sort) bx_blit(dst, b->W, b->H, d->radio, d->sortPos[i][0] + 1, d->sortPos[i][1] + 3, 255);
    draw_btn(b, dst, &d->mode[0], d->save ? 2 : in_btn(&d->mode[0], b->mx, b->my) ? 1 : 0);
    draw_btn(b, dst, &d->mode[1], !d->save ? 2 : in_btn(&d->mode[1], b->mx, b->my) ? 1 : 0);
    draw_btn(b, dst, &d->back, in_btn(&d->back, b->mx, b->my));
    draw_btn(b, dst, &d->title, in_btn(&d->title, b->mx, b->my));
    draw_btn(b, dst, &d->quit, in_btn(&d->quit, b->mx, b->my));
    for (int i = 0; i < d->perPage; i++) {
        int idx = d->page * d->perPage + i;
        if (idx >= d->cacheCount) break;
        int slot = slot_no(d, idx);
        SlotCache *c = &d->cache[slot];
        int x = d->slotPos[i][0], y = d->slotPos[i][1];
        if (c->exists && c->thumb) bx_blit(dst, b->W, b->H, c->thumb, x, y, 255);
        else if (d->empty) bx_blit(dst, b->W, b->H, d->empty, x, y, 255);
        if (slot < d->first && d->save && d->autoOnly && c->exists) bx_blit(dst, b->W, b->H, d->autoOnly, x, y, 255);
        if (slot == d->hover && d->frame) bx_blit(dst, b->W, b->H, d->frame, x + d->frameOff[0], y + d->frameOff[1], 255);
    }
    if (d->hover >= 0) draw_info(b, d, dst, d->hover);
    if (d->confirmSlot >= 0) {
        if (d->resave) bx_blit(dst, b->W, b->H, d->resave, 0, 0, 255);
        draw_info(b, d, dst, d->confirmSlot);
        draw_btn(b, dst, &d->ok, d->hoverBtn == 1);
        draw_btn(b, dst, &d->cancel, d->hoverBtn == 2);
    }
}

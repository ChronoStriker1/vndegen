#include "eagls.h"

#define HEROINES 48
#define FIG_W 224
#define FIG_H 457
#define FIG_Y 64
#define CENTRE_X 400
#define STEP 80
#define SIDE 6
#define BACK_STEP 56
#define BACK_Y 42
#define SLIDE_MS 150
#define NAME_H 27

enum { O_CLOSE = 10, O_BACK = 20, O_NEXT = 30, O_LEFT = 40, O_RIGHT = 50, O_TAB0 = 60, O_CENTRE = 120, O_FRAME = 121 };
enum { P_SCOUTER = 100, P_FIGURE = 101, P_RETURN = 110, P_BACK = 120, P_NEXT = 130 };

static const int groupStart[6] = {1, 4, 12, 20, 28, 42};
static const struct {
    int heroine, dx, y;
} crowns[5] = {{4, -26, 75}, {14, -13, 69}, {24, 21, 79}, {39, -7, 81}, {43, -7, 74}};

struct Oppai {
    int centre, hover, slideDir, page;
    u32 slideStart;
    bool scouter;
    Image *bg, *frame, *frameLit, *mask, *dark, *dim, *names, *crown, *pageMask, *pageOff, *pageOn;
    Image *fig[HEROINES + 1], *small[HEROINES + 1];
};

static int wrap(int k) { return ((k - 1) % HEROINES + HEROINES) % HEROINES + 1; }

bool oppai_scouted(EG *e, int k) {
    Var *v = var_find(&e->vars, "@MilkScautar");
    return v && k > 0 && k < v->n && v->v[k].i;
}

bool oppai_bra_found(EG *e, int block) {
    if (block < 0 || block >= 5) return false;
    Var *v = var_find(&e->vars, "_MilkFlg");
    int k = crowns[block].heroine;
    return v && k < v->n && v->v[k].i;
}

static Image *load(EG *e, const char *fmt, int k) {
    char n[32];
    snprintf(n, sizeof n, fmt, k);
    return eg_image_load(e, n);
}

static Image *figure(EG *e, Oppai *o, int k) {
    if (!o->fig[k]) o->fig[k] = load(e, "haku_x%02d", k);
    return o->fig[k];
}

static Image *small_figure(EG *e, Oppai *o, int k) {
    if (!o->small[k]) o->small[k] = load(e, "haku_x%02ds", k);
    return o->small[k];
}

static void free_page(Oppai *o) {
    image_free(o->pageOff);
    image_free(o->pageOn);
    o->pageOff = o->pageOn = NULL;
}

void oppai_free(EG *e) {
    Oppai *o = e->oppai;
    if (!o) return;
    Image *all[] = {o->bg, o->frame, o->frameLit, o->mask, o->dark, o->dim, o->names, o->crown, o->pageMask};
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) image_free(all[i]);
    for (int k = 0; k <= HEROINES; k++) {
        image_free(o->fig[k]);
        image_free(o->small[k]);
    }
    free_page(o);
    free(o);
    e->oppai = NULL;
}

void oppai_open(EG *e) {
    oppai_free(e);
    Oppai *o = eg_alloc(sizeof *o);
    e->oppai = o;
    o->bg = eg_image_load(e, "haku");
    if (o->bg) o->bg->alpha = false;
    o->frame = eg_image_load(e, "haku2");
    o->frameLit = eg_image_load(e, "haku2C");
    o->mask = eg_image_load(e, "haku2_mask");
    o->dark = eg_image_load(e, "haku_D");
    o->dim = eg_image_load(e, "haku_D2");
    o->names = eg_image_load(e, "haku_Name");
    o->crown = eg_image_load(e, "haku_Parts");
    o->pageMask = eg_image_load(e, "haku_maskP");
    o->centre = 1;
    e->uiMode = UI_OPPAI;
}

static u32 mix(u32 d, u32 s, u32 a) {
    if (a >= 255) return s | 0xff000000u;
    u32 ia = 255 - a;
    u32 r = ((s & 0xff) * a + (d & 0xff) * ia) / 255;
    u32 g = ((s >> 8 & 0xff) * a + (d >> 8 & 0xff) * ia) / 255;
    u32 b = ((s >> 16 & 0xff) * a + (d >> 16 & 0xff) * ia) / 255;
    return r | g << 8 | b << 16 | 0xff000000u;
}

static int mask_at(const Image *m, int x, int y) {
    if (!m || x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    u32 p = m->px[(size_t)y * m->w + x];
    if (m->alpha && !(p >> 24)) return 0;
    return (int)(p & 0xff);
}

static void copy_masked(EG *e, u32 *dst, const Image *src, const Image *mask, int id) {
    if (!src || !mask) return;
    int w = src->w < mask->w ? src->w : mask->w, h = src->h < mask->h ? src->h : mask->h;
    for (int y = 0; y < h && y < e->H; y++)
        for (int x = 0; x < w && x < e->W; x++) {
            if (mask_at(mask, x, y) != id) continue;
            u32 p = src->px[(size_t)y * src->w + x];
            u32 a = src->alpha ? p >> 24 : 255;
            if (!a) continue;
            u32 *d = &dst[(size_t)y * e->W + x];
            *d = mix(*d, p, a);
        }
}

static void shaded(EG *e, u32 *dst, const Image *im, const Image *shade, int ox, int oy) {
    if (!im) return;
    for (int y = 0; y < im->h; y++) {
        int dy = oy + y;
        if (dy < 0 || dy >= e->H) continue;
        for (int x = 0; x < im->w; x++) {
            int dx = ox + x;
            if (dx < 0 || dx >= e->W) continue;
            u32 p = im->px[(size_t)y * im->w + x];
            u32 a = im->alpha ? p >> 24 : 255;
            if (!a) continue;
            u32 keep = 255;
            if (shade) {
                int sx = x * shade->w / im->w, sy = y * shade->h / im->h;
                keep = 255 - (shade->px[(size_t)sy * shade->w + sx] >> 24);
            }
            u32 r = (p & 0xff) * keep / 255, g = (p >> 8 & 0xff) * keep / 255, b = (p >> 16 & 0xff) * keep / 255;
            u32 *d = &dst[(size_t)dy * e->W + dx];
            *d = mix(*d, r | g << 8 | b << 16, a);
        }
    }
}

static int slide_offset(EG *e, Oppai *o, int step) {
    if (!o->slideDir) return 0;
    u32 t = e->now - o->slideStart;
    if (t >= SLIDE_MS) {
        o->slideDir = 0;
        return 0;
    }
    return o->slideDir * step * (int)(SLIDE_MS - t) / SLIDE_MS;
}

static void rotate(EG *e, Oppai *o, int dir) {
    o->centre = wrap(o->centre + dir);
    o->slideDir = dir;
    o->slideStart = e->now;
}

static int group_of(int k) {
    int g = 0;
    for (int i = 0; i < 6; i++)
        if (k >= groupStart[i]) g = i;
    return g;
}

static void draw_crown(EG *e, Oppai *o, u32 *dst, int k, int x) {
    for (int i = 0; i < 5; i++)
        if (crowns[i].heroine == k) blit(dst, e->W, e->H, o->crown, x + crowns[i].dx, crowns[i].y, 255);
}

static void carousel_draw(EG *e, Oppai *o, u32 *dst) {
    blit(dst, e->W, e->H, o->bg, 0, 0, 255);
    int front = slide_offset(e, o, STEP), back = -front * BACK_STEP / STEP;
    for (int m = 18; m <= 38; m++) {
        int k = wrap(m - o->centre), x = BACK_STEP * m - 1196 + back;
        shaded(e, dst, small_figure(e, o, k), NULL, x, BACK_Y);
    }
    for (int d = SIDE; d >= 1; d--)
        for (int s = -1; s <= 1; s += 2) {
            int k = wrap(o->centre + s * d), x = CENTRE_X + s * d * STEP + front;
            shaded(e, dst, figure(e, o, k), oppai_scouted(e, k) ? o->dim : o->dark, x - FIG_W / 2, FIG_Y);
            if (oppai_scouted(e, k)) draw_crown(e, o, dst, k, x);
        }
    bool open = oppai_scouted(e, o->centre);
    shaded(e, dst, figure(e, o, o->centre), open ? NULL : o->dark, CENTRE_X - FIG_W / 2 + front, FIG_Y);
    if (open) draw_crown(e, o, dst, o->centre, CENTRE_X + front);
    blit(dst, e->W, e->H, o->frame, 0, 0, 255);
    copy_masked(e, dst, o->frameLit, o->mask, O_TAB0 + 10 * group_of(o->centre));
    if (o->hover == O_CENTRE || o->hover == O_FRAME) copy_masked(e, dst, o->frameLit, o->mask, O_FRAME);
    else if (o->hover) copy_masked(e, dst, o->frameLit, o->mask, o->hover);
    if (open && o->names) blit_part(dst, e->W, e->H, o->names, 0, (o->centre - 1) * NAME_H, o->names->w, NAME_H, 610, 6, 255);
}

static void open_page(EG *e, Oppai *o, int k) {
    free_page(o);
    o->page = k;
    o->scouter = false;
    o->pageOff = load(e, "haku_x%02dP", k);
    o->pageOn = load(e, "haku_x%02dPC", k);
    if (o->pageOff) o->pageOff->alpha = false;
    if (o->pageOn) o->pageOn->alpha = false;
}

static int next_scouted(EG *e, int k, int dir) {
    for (int i = 1; i <= HEROINES; i++) {
        int n = wrap(k + dir * i);
        if (oppai_scouted(e, n)) return n;
    }
    return k;
}

static void page_draw(EG *e, Oppai *o, u32 *dst) {
    blit(dst, e->W, e->H, o->pageOff, 0, 0, 255);
    if (o->scouter) {
        copy_masked(e, dst, o->pageOn, o->pageMask, P_SCOUTER);
        copy_masked(e, dst, o->pageOn, o->pageMask, P_FIGURE);
    }
    if (o->hover == P_RETURN || o->hover == P_BACK || o->hover == P_NEXT) copy_masked(e, dst, o->pageOn, o->pageMask, o->hover);
}

void oppai_draw(EG *e, u32 *dst) {
    Oppai *o = e->oppai;
    if (!o) return;
    if (o->page) page_draw(e, o, dst);
    else carousel_draw(e, o, dst);
}

static void close_gallery(EG *e) {
    e->uiMode = UI_NONE;
    if (e->scriptUi) e->scriptUiDone = true;
    oppai_free(e);
}

void oppai_update(EG *e) {
    Oppai *o = e->oppai;
    if (!o) return;
    int c = mask_at(o->page ? o->pageMask : o->mask, e->mx, e->my);
    if (o->page && c == P_FIGURE) c = 0;
    if (!o->page && c == O_FRAME) c = O_CENTRE;
    if (!o->page && c == O_CENTRE && !oppai_scouted(e, o->centre)) c = 0;
    if (c != o->hover && c) snd_play(e, "SYS_OV", 0, 2, 0);
    o->hover = c;
    if (e->rclick) c = o->page ? P_RETURN : O_CLOSE;
    else if (!e->click || !c) return;
    snd_play(e, "SYS_OK", 0, 2, 0);
    if (o->page) {
        if (c == P_SCOUTER) o->scouter = !o->scouter;
        else if (c == P_BACK || c == P_NEXT) open_page(e, o, next_scouted(e, o->page, c == P_NEXT ? 1 : -1));
        else if (c == P_RETURN) {
            o->centre = o->page;
            o->page = 0;
            o->hover = 0;
            free_page(o);
        }
        return;
    }
    if (c == O_CLOSE) close_gallery(e);
    else if (c == O_BACK || c == O_LEFT) rotate(e, o, -1);
    else if (c == O_NEXT || c == O_RIGHT) rotate(e, o, 1);
    else if (c >= O_TAB0 && c < O_TAB0 + 60) o->centre = groupStart[(c - O_TAB0) / 10];
    else if (c == O_CENTRE) {
        o->hover = 0;
        open_page(e, o, o->centre);
    }
}

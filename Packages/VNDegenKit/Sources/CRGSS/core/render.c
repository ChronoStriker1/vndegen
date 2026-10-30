// The compositor: draws every visible object onto the screen in z order, viewports clipping their children and
// tinting the area they cover, as the help's Sprite, Plane and Viewport topics describe.
#include "scene.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

Container rgss_screen;
int rgss_screen_w = 640, rgss_screen_h = 480;
static uint64_t nextSerial;

void drawable_attach(Drawable *d, DrawKind kind, Viewport *viewport) {
    d->kind = kind;
    d->serial = ++nextSerial;
    Container *c = viewport ? &viewport->children : &rgss_screen;
    d->container = c;
    d->next = NULL;
    d->prev = c->last;
    if (c->last) c->last->next = d;
    else c->first = d;
    c->last = d;
}

void drawable_detach(Drawable *d) {
    Container *c = d->container;
    if (!c) return;
    if (d->prev) d->prev->next = d->next;
    else c->first = d->next;
    if (d->next) d->next->prev = d->prev;
    else c->last = d->prev;
    d->prev = d->next = NULL;
    d->container = NULL;
}

void flash_start(Flash *flash, const ColorF *color, int duration) {
    flash->duration = flash->count = duration > 0 ? duration : 0;
    flash->hide = color == NULL;
    if (color) flash->color = *color;
}

void flash_update(Flash *flash) {
    if (flash->count > 0) flash->count--;
}

// MARK: Pixel effects

typedef struct {
    int cr, cg, cb, ca;   // the blend color and its weight, 0..255
    int tr, tg, tb, gray; // tone
    bool plain;           // nothing to apply
} Shade;

static Shade shade_for(const Effects *fx) {
    Shade s = {0};
    if (fx->color && fx->color->a > 0) {
        s.cr = (int)fx->color->r;
        s.cg = (int)fx->color->g;
        s.cb = (int)fx->color->b;
        s.ca = (int)fx->color->a;
    }
    // A flash replaces the color while its fading alpha is the stronger of the two.
    if (fx->flash && fx->flash->count > 0 && !fx->flash->hide) {
        int fa = (int)(fx->flash->color.a * fx->flash->count / fx->flash->duration);
        if (fa > s.ca) {
            s.cr = (int)fx->flash->color.r;
            s.cg = (int)fx->flash->color.g;
            s.cb = (int)fx->flash->color.b;
            s.ca = fa;
        }
    }
    if (fx->tone) {
        s.tr = (int)fx->tone->r;
        s.tg = (int)fx->tone->g;
        s.tb = (int)fx->tone->b;
        s.gray = (int)fx->tone->gray;
    }
    s.plain = !s.ca && !s.tr && !s.tg && !s.tb && !s.gray;
    return s;
}

static inline void apply_shade(const Shade *s, int *r, int *g, int *b) {
    if (s->ca) {
        *r += (s->cr - *r) * s->ca / 255;
        *g += (s->cg - *g) * s->ca / 255;
        *b += (s->cb - *b) * s->ca / 255;
    }
    if (s->gray) {
        int lum = (*r * 77 + *g * 151 + *b * 28) >> 8;
        *r += (lum - *r) * s->gray / 255;
        *g += (lum - *g) * s->gray / 255;
        *b += (lum - *b) * s->gray / 255;
    }
    if (s->tr || s->tg || s->tb) {
        *r = clamp255(*r + s->tr);
        *g = clamp255(*g + s->tg);
        *b = clamp255(*b + s->tb);
    }
}

// Puts one source pixel on the opaque screen with the blend type, `alpha` already including every opacity.
static inline void put(Pixel *dst, Pixel src, int alpha, const Shade *s, int blendType) {
    if (alpha <= 0) return;
    int r = px_r(src), g = px_g(src), b = px_b(src);
    if (!s->plain) apply_shade(s, &r, &g, &b);
    Pixel d = *dst;
    int dr = px_r(d), dg = px_g(d), db = px_b(d);
    switch (blendType) {
    case 1:
        dr = clamp255(dr + r * alpha / 255);
        dg = clamp255(dg + g * alpha / 255);
        db = clamp255(db + b * alpha / 255);
        break;
    case 2:
        dr = clamp255(dr - r * alpha / 255);
        dg = clamp255(dg - g * alpha / 255);
        db = clamp255(db - b * alpha / 255);
        break;
    default:
        dr += (r - dr) * alpha / 255;
        dg += (g - dg) * alpha / 255;
        db += (b - db) * alpha / 255;
        break;
    }
    *dst = px_make(dr, dg, db, 255);
}

void draw_image_rect(Image *target, IRect clip, const Image *src, IRect sr, int dx, int dy, const Effects *fx) {
    if (fx->opacity <= 0 || (fx->flash && fx->flash->count > 0 && fx->flash->hide)) return;
    IRect clipped = irect_clip(sr, src->w, src->h);
    dx += clipped.x - sr.x;
    dy += clipped.y - sr.y;
    sr = clipped;
    IRect d = {dx, dy, sr.w, sr.h};
    int x0 = d.x > clip.x ? d.x : clip.x, y0 = d.y > clip.y ? d.y : clip.y;
    int x1 = d.x + d.w < clip.x + clip.w ? d.x + d.w : clip.x + clip.w;
    int y1 = d.y + d.h < clip.y + clip.h ? d.y + d.h : clip.y + clip.h;
    if (x0 >= x1 || y0 >= y1) return;
    Shade s = shade_for(fx);
    int opacity = fx->opacity > 255 ? 255 : fx->opacity;
    for (int y = y0; y < y1; y++) {
        const Pixel *in = src->px + (size_t)(sr.y + y - d.y) * src->w + sr.x - d.x;
        Pixel *out = target->px + (size_t)y * target->w;
        for (int x = x0; x < x1; x++) put(&out[x], in[x], px_a(in[x]) * opacity / 255, &s, fx->blendType);
    }
}

// MARK: Sprites

static void draw_sprite(Sprite *sp, Image *target, IRect clip, int offX, int offY) {
    if (!sp->visible || !sp->bitmap || sp->bitmap->disposed || sp->opacity <= 0) return;
    if (sp->flash.count > 0 && sp->flash.hide) return;
    const Image *img = &sp->bitmap->image;
    IRect sr = irect_clip(*sp->srcRect, img->w, img->h);
    if (sr.w <= 0 || sr.h <= 0 || sp->zoomX == 0 || sp->zoomY == 0) return;
    // ox and oy count from the source rectangle's corner, even when it was clipped.
    int originX = sp->ox - (sr.x - sp->srcRect->x), originY = sp->oy - (sr.y - sp->srcRect->y);
    Effects fx = {sp->color, sp->tone, &sp->flash, sp->opacity, sp->blendType};
    Shade s = shade_for(&fx);
    int bushStart = sp->bushDepth > 0 ? sr.h - sp->bushDepth : sr.h;
    double angle = fmod(sp->angle, 360.0);
    double px = sp->x + offX, py = sp->y + offY;

    if (angle == 0 && sp->waveAmp == 0) {
        // Axis-aligned: map each screen pixel back to a source pixel.
        double left = px - originX * sp->zoomX, top = py - originY * sp->zoomY;
        double right = left + sr.w * sp->zoomX, bottom = top + sr.h * sp->zoomY;
        if (sp->zoomX < 0) { double t = left; left = right; right = t; }
        if (sp->zoomY < 0) { double t = top; top = bottom; bottom = t; }
        int x0 = (int)ceil(left - 0.5), x1 = (int)ceil(right - 0.5);
        int y0 = (int)ceil(top - 0.5), y1 = (int)ceil(bottom - 0.5);
        if (x0 < clip.x) x0 = clip.x;
        if (y0 < clip.y) y0 = clip.y;
        if (x1 > clip.x + clip.w) x1 = clip.x + clip.w;
        if (y1 > clip.y + clip.h) y1 = clip.y + clip.h;
        for (int y = y0; y < y1; y++) {
            int sy = (int)floor((y + 0.5 - py) / sp->zoomY + originY);
            if (sy < 0 || sy >= sr.h) continue;
            int alphaRow = sy >= bushStart ? sp->opacity * sp->bushOpacity / 255 : sp->opacity;
            const Pixel *row = img->px + (size_t)(sr.y + sy) * img->w + sr.x;
            Pixel *out = target->px + (size_t)y * target->w;
            for (int x = x0; x < x1; x++) {
                int sx = (int)floor((x + 0.5 - px) / sp->zoomX + originX);
                if (sx < 0 || sx >= sr.w) continue;
                Pixel p = row[sp->mirror ? sr.w - 1 - sx : sx];
                put(&out[x], p, px_a(p) * alphaRow / 255, &s, sp->blendType);
            }
        }
        return;
    }

    // Rotated or waving: angle turns counterclockwise on screen, about the origin.
    double rad = angle * M_PI / 180.0, c = cos(rad), sn = sin(rad);
    double corners[4][2] = {{-originX, -originY}, {sr.w - originX, -originY}, {-originX, sr.h - originY},
                            {sr.w - originX, sr.h - originY}};
    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    int wave = abs(sp->waveAmp);
    for (int i = 0; i < 4; i++) {
        double lx = corners[i][0] * sp->zoomX, ly = corners[i][1] * sp->zoomY;
        double sx = px + lx * c + ly * sn, sy = py - lx * sn + ly * c;
        minX = fmin(minX, sx - wave);
        maxX = fmax(maxX, sx + wave);
        minY = fmin(minY, sy);
        maxY = fmax(maxY, sy);
    }
    int x0 = (int)floor(minX), x1 = (int)ceil(maxX), y0 = (int)floor(minY), y1 = (int)ceil(maxY);
    if (x0 < clip.x) x0 = clip.x;
    if (y0 < clip.y) y0 = clip.y;
    if (x1 > clip.x + clip.w) x1 = clip.x + clip.w;
    if (y1 > clip.y + clip.h) y1 = clip.y + clip.h;
    for (int y = y0; y < y1; y++) {
        Pixel *out = target->px + (size_t)y * target->w;
        for (int x = x0; x < x1; x++) {
            double dx = x + 0.5 - px, dy = y + 0.5 - py;
            double lx = (dx * c - dy * sn) / sp->zoomX, ly = (dx * sn + dy * c) / sp->zoomY;
            int sy = (int)floor(ly + originY);
            if (sy < 0 || sy >= sr.h) continue;
            double shift = 0;
            if (sp->waveAmp && sp->waveLength > 0) {
                shift = sp->waveAmp * sin((sp->wavePhase + 360.0 * sy / sp->waveLength) * M_PI / 180.0);
            }
            int sx = (int)floor(lx - shift / sp->zoomX + originX);
            if (sx < 0 || sx >= sr.w) continue;
            Pixel p = img->px[(size_t)(sr.y + sy) * img->w + sr.x + (sp->mirror ? sr.w - 1 - sx : sx)];
            int alpha = sy >= bushStart ? sp->opacity * sp->bushOpacity / 255 : sp->opacity;
            put(&out[x], p, px_a(p) * alpha / 255, &s, sp->blendType);
        }
    }
}

// MARK: Planes

static void draw_plane(Plane *pl, Image *target, IRect clip, int offX, int offY) {
    (void)offX;
    (void)offY;
    if (!pl->visible || !pl->bitmap || pl->bitmap->disposed || pl->opacity <= 0) return;
    const Image *img = &pl->bitmap->image;
    if (img->w <= 0 || img->h <= 0 || pl->zoomX <= 0 || pl->zoomY <= 0) return;
    Effects fx = {pl->color, pl->tone, NULL, pl->opacity, pl->blendType};
    Shade s = shade_for(&fx);
    // The plane fills its viewport (or the screen) and repeats the bitmap from ox, oy.
    for (int y = clip.y; y < clip.y + clip.h; y++) {
        int ly = (int)floor((y - clip.y + pl->oy * pl->zoomY) / pl->zoomY);
        int sy = ((ly % img->h) + img->h) % img->h;
        const Pixel *row = img->px + (size_t)sy * img->w;
        Pixel *out = target->px + (size_t)y * target->w;
        for (int x = clip.x; x < clip.x + clip.w; x++) {
            int lx = (int)floor((x - clip.x + pl->ox * pl->zoomX) / pl->zoomX);
            int sx = ((lx % img->w) + img->w) % img->w;
            Pixel p = row[sx];
            put(&out[x], p, px_a(p) * pl->opacity / 255, &s, pl->blendType);
        }
    }
}

// MARK: Viewport effects

// Tints the area a viewport covers once its children are drawn.
static void viewport_effects(Viewport *vp, Image *target, IRect area) {
    Effects fx = {vp->color, vp->tone, &vp->flash, 255, 0};
    Shade s = shade_for(&fx);
    if (s.plain) return;
    for (int y = area.y; y < area.y + area.h; y++) {
        Pixel *row = target->px + (size_t)y * target->w;
        for (int x = area.x; x < area.x + area.w; x++) {
            int r = px_r(row[x]), g = px_g(row[x]), b = px_b(row[x]);
            apply_shade(&s, &r, &g, &b);
            row[x] = px_make(r, g, b, 255);
        }
    }
}

// MARK: Compositing

typedef struct {
    Drawable *d;
    int z;
    int layer;
} Item;

static int compare_items(const void *a, const void *b) {
    const Item *x = a, *y = b;
    if (x->z != y->z) return x->z < y->z ? -1 : 1;
    if (x->d->serial != y->d->serial) return x->d->serial < y->d->serial ? -1 : 1;
    return x->layer - y->layer;
}

static IRect intersect(IRect a, IRect b) {
    int x0 = a.x > b.x ? a.x : b.x, y0 = a.y > b.y ? a.y : b.y;
    int x1 = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
    int y1 = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
    IRect r = {x0, y0, x1 - x0, y1 - y0};
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}

static void render_container(Container *c, Image *target, IRect clip, int offX, int offY) {
    size_t count = 0, capacity = 64;
    Item *items = malloc(capacity * sizeof(Item));
    if (!items) return;
    for (Drawable *d = c->first; d; d = d->next) {
        int zs[256];
        int layers = 1;
        int z = 0;
        switch (d->kind) {
        case DRAW_SPRITE: z = ((Sprite *)d)->z; break;
        case DRAW_PLANE: z = ((Plane *)d)->z; break;
        case DRAW_WINDOW: z = ((Window *)d)->z; break;
        case DRAW_VIEWPORT: z = ((Viewport *)d)->z; break;
        case DRAW_TILEMAP: layers = tilemap_layers((Tilemap *)d, zs, 256); break;
        }
        for (int i = 0; i < layers; i++) {
            if (count == capacity) {
                capacity *= 2;
                Item *grown = realloc(items, capacity * sizeof(Item));
                if (!grown) {
                    free(items);
                    return;
                }
                items = grown;
            }
            items[count++] = (Item){d, d->kind == DRAW_TILEMAP ? zs[i] : z, i};
        }
    }
    qsort(items, count, sizeof(Item), compare_items);
    for (size_t i = 0; i < count; i++) {
        Drawable *d = items[i].d;
        switch (d->kind) {
        case DRAW_SPRITE: draw_sprite((Sprite *)d, target, clip, offX, offY); break;
        case DRAW_PLANE: draw_plane((Plane *)d, target, clip, offX, offY); break;
        case DRAW_WINDOW: window_draw((Window *)d, target, clip, offX, offY); break;
        case DRAW_TILEMAP: tilemap_draw((Tilemap *)d, items[i].layer, target, clip, offX, offY); break;
        case DRAW_VIEWPORT: {
            Viewport *vp = (Viewport *)d;
            if (!vp->visible || vp->disposed) break;
            if (vp->flash.count > 0 && vp->flash.hide) break;
            IRect area = intersect(clip, *vp->rect);
            if (area.w <= 0 || area.h <= 0) break;
            render_container(&vp->children, target, area, vp->rect->x - vp->ox, vp->rect->y - vp->oy);
            viewport_effects(vp, target, area);
            break;
        }
        }
    }
    free(items);
}

void scene_render(Image *target) {
    size_t count = (size_t)target->w * target->h;
    for (size_t i = 0; i < count; i++) target->px[i] = 0xFF000000u;
    IRect all = {0, 0, target->w, target->h};
    render_container(&rgss_screen, target, all, 0, 0);
}

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

// The common case: no color, tone or flash, normal blending onto the opaque screen.
static inline void put_plain(Pixel *dst, Pixel src, int alpha) {
    if (alpha >= 255) {
        *dst = src | 0xFF000000u;
        return;
    }
    if (alpha <= 0) return;
    Pixel d = *dst;
    int inv = 255 - alpha;
    // (a * s + (255 - a) * d) / 255, with the division done as a multiply and shift.
    unsigned r = px_r(src) * alpha + px_r(d) * inv, g = px_g(src) * alpha + px_g(d) * inv,
             b = px_b(src) * alpha + px_b(d) * inv;
    r = (r + 1 + (r >> 8)) >> 8;
    g = (g + 1 + (g >> 8)) >> 8;
    b = (b + 1 + (b >> 8)) >> 8;
    *dst = px_make((int)r, (int)g, (int)b, 255);
}

void draw_image_rect(Image *target, IRect clip, const Image *src, IRect sr, int dx, int dy, const Effects *fx) {
    if (fx->opacity <= 0 || (fx->flash && fx->flash->count > 0 && fx->flash->hide)) return;
    IRect clipped = irect_clip(sr, src->w, src->h);
    int64_t ddx = (int64_t)dx + clipped.x - sr.x, ddy = (int64_t)dy + clipped.y - sr.y;
    sr = clipped;
    int64_t cx1 = (int64_t)clip.x + clip.w, cy1 = (int64_t)clip.y + clip.h;
    int64_t lx0 = ddx > clip.x ? ddx : clip.x, ly0 = ddy > clip.y ? ddy : clip.y;
    int64_t lx1 = ddx + sr.w < cx1 ? ddx + sr.w : cx1, ly1 = ddy + sr.h < cy1 ? ddy + sr.h : cy1;
    if (lx0 >= lx1 || ly0 >= ly1) return;
    int x0 = (int)lx0, y0 = (int)ly0, x1 = (int)lx1, y1 = (int)ly1;
    Shade s = shade_for(fx);
    int opacity = fx->opacity > 255 ? 255 : fx->opacity;
    bool plain = s.plain && fx->blendType == 0;
    for (int y = y0; y < y1; y++) {
        const Pixel *in = src->px + (size_t)(sr.y + y - ddy) * src->w + sr.x - ddx;
        Pixel *out = target->px + (size_t)y * target->w;
        if (plain && opacity == 255) {
            for (int x = x0; x < x1; x++) put_plain(&out[x], in[x], px_a(in[x]));
        } else if (plain) {
            for (int x = x0; x < x1; x++) put_plain(&out[x], in[x], px_a(in[x]) * opacity / 255);
        } else {
            for (int x = x0; x < x1; x++) put(&out[x], in[x], px_a(in[x]) * opacity / 255, &s, fx->blendType);
        }
    }
}

// MARK: Sprites

static void draw_sprite(Sprite *sp, Image *target, IRect clip, int offX, int offY) {
    if (sp->disposed || !sp->visible || !sp->bitmap || sp->bitmap->disposed || sp->opacity <= 0) return;
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
        double fx0 = fmax(ceil(left - 0.5), clip.x), fx1 = fmin(ceil(right - 0.5), (double)clip.x + clip.w);
        double fy0 = fmax(ceil(top - 0.5), clip.y), fy1 = fmin(ceil(bottom - 0.5), (double)clip.y + clip.h);
        if (!(fx1 > fx0 && fy1 > fy0)) return;
        int x0 = (int)fx0, x1 = (int)fx1, y0 = (int)fy0, y1 = (int)fy1;
        // The source column of each screen column, worked out once for the whole sprite.
        int columns[x1 - x0];
        for (int x = x0; x < x1; x++) {
            int sx = (int)floor((x + 0.5 - px) / sp->zoomX + originX);
            columns[x - x0] = sx < 0 || sx >= sr.w ? -1 : (sp->mirror ? sr.w - 1 - sx : sx);
        }
        bool plain = s.plain && sp->blendType == 0;
        for (int y = y0; y < y1; y++) {
            int sy = (int)floor((y + 0.5 - py) / sp->zoomY + originY);
            if (sy < 0 || sy >= sr.h) continue;
            int alphaRow = sy >= bushStart ? sp->opacity * sp->bushOpacity / 255 : sp->opacity;
            const Pixel *row = img->px + (size_t)(sr.y + sy) * img->w + sr.x;
            Pixel *out = target->px + (size_t)y * target->w;
            for (int x = x0; x < x1; x++) {
                int sx = columns[x - x0];
                if (sx < 0) continue;
                Pixel p = row[sx];
                int alpha = alphaRow == 255 ? px_a(p) : px_a(p) * alphaRow / 255;
                if (plain) put_plain(&out[x], p, alpha);
                else put(&out[x], p, alpha, &s, sp->blendType);
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
    double fx0 = fmax(floor(minX), clip.x), fx1 = fmin(ceil(maxX), (double)clip.x + clip.w);
    double fy0 = fmax(floor(minY), clip.y), fy1 = fmin(ceil(maxY), (double)clip.y + clip.h);
    if (!(fx1 > fx0 && fy1 > fy0)) return;
    int x0 = (int)fx0, x1 = (int)fx1, y0 = (int)fy0, y1 = (int)fy1;
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

static int wrap_index(double v, int n) {
    double m = fmod(v, n);
    if (m < 0) m += n;
    return (int)m;
}

static void draw_plane(Plane *pl, Image *target, IRect clip, int offX, int offY) {
    (void)offX;
    (void)offY;
    if (!pl->visible || !pl->bitmap || pl->bitmap->disposed || pl->opacity <= 0) return;
    const Image *img = &pl->bitmap->image;
    if (img->w <= 0 || img->h <= 0 || pl->zoomX <= 0 || pl->zoomY <= 0) return;
    Effects fx = {pl->color, pl->tone, NULL, pl->opacity, pl->blendType};
    Shade s = shade_for(&fx);
    if (clip.w <= 0 || clip.h <= 0) return;
    // The plane fills its viewport (or the screen) and repeats the bitmap from ox, oy.
    int columns[clip.w];
    for (int x = 0; x < clip.w; x++) columns[x] = wrap_index(floor((x + pl->ox * pl->zoomX) / pl->zoomX), img->w);
    bool plain = s.plain && pl->blendType == 0;
    for (int y = clip.y; y < clip.y + clip.h; y++) {
        int sy = wrap_index(floor((y - clip.y + pl->oy * pl->zoomY) / pl->zoomY), img->h);
        const Pixel *row = img->px + (size_t)sy * img->w;
        Pixel *out = target->px + (size_t)y * target->w + clip.x;
        if (plain) {
            for (int x = 0; x < clip.w; x++) {
                Pixel p = row[columns[x]];
                put_plain(&out[x], p, px_a(p) * pl->opacity / 255);
            }
        } else {
            for (int x = 0; x < clip.w; x++) {
                Pixel p = row[columns[x]];
                put(&out[x], p, px_a(p) * pl->opacity / 255, &s, pl->blendType);
            }
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

static int sprite_y(const Item *item) { return ((const Sprite *)item->d)->y; }

static int compare_sprites_by_y(const void *a, const void *b) {
    const Item *x = a, *y = b;
    if (sprite_y(x) != sprite_y(y)) return sprite_y(x) < sprite_y(y) ? -1 : 1;
    return x->d->serial < y->d->serial ? -1 : x->d->serial > y->d->serial;
}

// In VX and VX Ace a sprite lower on the screen (larger y) is in front of another sprite at the same z, then the one
// made later. Other objects at that z keep their places among them, in the order they were made, so the order stays
// consistent whatever mix of objects shares a z.
static void order_sprites_by_y(Item *items, size_t count) {
    for (size_t start = 0; start < count;) {
        size_t end = start;
        while (end < count && items[end].z == items[start].z) end++;
        Item sprites[end - start];
        size_t n = 0;
        for (size_t i = start; i < end; i++) if (items[i].d->kind == DRAW_SPRITE) sprites[n++] = items[i];
        if (n > 1) {
            qsort(sprites, n, sizeof(Item), compare_sprites_by_y);
            size_t k = 0;
            for (size_t i = start; i < end; i++) if (items[i].d->kind == DRAW_SPRITE) items[i] = sprites[k++];
        }
        start = end;
    }
}

static IRect intersect(IRect a, IRect b) {
    int64_t x0 = a.x > b.x ? a.x : b.x, y0 = a.y > b.y ? a.y : b.y;
    int64_t ax1 = (int64_t)a.x + a.w, bx1 = (int64_t)b.x + b.w, ay1 = (int64_t)a.y + a.h, by1 = (int64_t)b.y + b.h;
    int64_t x1 = ax1 < bx1 ? ax1 : bx1, y1 = ay1 < by1 ? ay1 : by1;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    return (IRect){(int)x0, (int)y0, (int)(x1 - x0), (int)(y1 - y0)};
}

void draw_image_scaled(Image *target, IRect clip, const Image *src, IRect sr, IRect d, const Effects *fx) {
    if (fx->opacity <= 0 || (fx->flash && fx->flash->count > 0 && fx->flash->hide)) return;
    if (d.w <= 0 || d.h <= 0 || sr.w <= 0 || sr.h <= 0) return;
    IRect c = intersect(d, clip);
    if (c.w <= 0 || c.h <= 0) return;
    int columns[c.w];
    for (int x = 0; x < c.w; x++) {
        int64_t sx = sr.x + ((int64_t)c.x + x - d.x) * sr.w / d.w;
        columns[x] = sx >= 0 && sx < src->w ? (int)sx : -1;
    }
    Shade s = shade_for(fx);
    int opacity = fx->opacity > 255 ? 255 : fx->opacity;
    bool plain = s.plain && fx->blendType == 0;
    for (int y = c.y; y < c.y + c.h; y++) {
        int64_t sy = sr.y + ((int64_t)y - d.y) * sr.h / d.h;
        if (sy < 0 || sy >= src->h) continue;
        const Pixel *in = src->px + (size_t)sy * src->w;
        Pixel *out = target->px + (size_t)y * target->w + c.x;
        for (int x = 0; x < c.w; x++) {
            int sx = columns[x];
            if (sx < 0) continue;
            Pixel p = in[sx];
            if (plain && opacity == 255) put_plain(&out[x], p, px_a(p));
            else if (plain) put_plain(&out[x], p, px_a(p) * opacity / 255);
            else put(&out[x], p, px_a(p) * opacity / 255, &s, fx->blendType);
        }
    }
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
    if (rgss_version >= RGSS2) order_sprites_by_y(items, count);
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

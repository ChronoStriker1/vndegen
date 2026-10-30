// Viewport, Sprite, Plane, Window and Tilemap: Ruby objects over the scene structs in scene.h.
#include "binding.h"

#include <math.h>
#include <string.h>

VALUE rgss_cViewport, rgss_cSprite, rgss_cPlane, rgss_cWindow, rgss_cTilemap;
static VALUE cTilemapBitmaps;

static int clamp_opacity(VALUE v) { return clamp255(NUM2INT(v)); }

// RGSS3 lets scripts read and set a disposed object's properties, which games do while tearing scenes down, so the
// accessors don't check for disposal. The objects a drawable refers to. Each struct keeps the Ruby objects alive and points at their C data.
typedef struct {
    Viewport v;
    VALUE rect, color, tone;
} RbViewport;

typedef struct {
    Sprite s;
    VALUE viewport, bitmap, srcRect, color, tone;
} RbSprite;

typedef struct {
    Plane p;
    VALUE viewport, bitmap, color, tone;
} RbPlane;

typedef struct {
    Window w;
    VALUE viewport, windowskin, contents, cursorRect, tone;
} RbWindow;

typedef struct {
    Tilemap t;
    VALUE viewport, bitmaps[9], tileset, mapData, flashData, flags, bitmapsProxy;
} RbTilemap;

static Viewport *viewport_arg(VALUE v);
static VALUE window_put_contents(VALUE self, VALUE b);

// MARK: Viewport

static void viewport_mark(void *p) {
    RbViewport *v = p;
    rb_gc_mark(v->rect);
    rb_gc_mark(v->color);
    rb_gc_mark(v->tone);
}

static void viewport_free(void *p) {
    RbViewport *v = p;
    drawable_detach(&v->v.d);
    // Children outlive their viewport only as long as Ruby keeps them; they no longer draw.
    for (Drawable *d = v->v.children.first, *next; d; d = next) {
        next = d->next;
        d->container = NULL;
        d->prev = d->next = NULL;
    }
    ruby_xfree(v);
}

static const rb_data_type_t viewport_type = {"Viewport", {viewport_mark, viewport_free, NULL}, NULL, NULL, 0};

static VALUE viewport_alloc(VALUE klass) {
    RbViewport *v;
    VALUE obj = TypedData_Make_Struct(klass, RbViewport, &viewport_type, v);
    v->rect = rgss_rect_new(0, 0, 0, 0);
    v->color = rgss_color_new(0, 0, 0, 0);
    v->tone = rgss_tone_new(0, 0, 0, 0);
    v->v.rect = &rgss_rect(v->rect)->r;
    v->v.color = &rgss_color(v->color)->c;
    v->v.tone = &rgss_tone(v->tone)->t;
    v->v.visible = true;
    return obj;
}

static RbViewport *get_viewport(VALUE self) {
    RbViewport *v = rb_check_typeddata(self, &viewport_type);
    return v;
}

static Viewport *viewport_arg(VALUE v) {
    if (NIL_P(v)) return NULL;
    return &get_viewport(v)->v;
}

static VALUE viewport_initialize(int argc, VALUE *argv, VALUE self) {
    RbViewport *v = rb_check_typeddata(self, &viewport_type);
    IRect r;
    if (argc == 0) r = (IRect){0, 0, rgss_screen_w, rgss_screen_h};
    else if (argc == 1) r = rgss_rect(argv[0])->r;
    else if (argc == 4) r = (IRect){NUM2INT(argv[0]), NUM2INT(argv[1]), NUM2INT(argv[2]), NUM2INT(argv[3])};
    else rb_raise(rb_eArgError, "wrong number of arguments (given %d, expected 0, 1 or 4)", argc);
    *v->v.rect = r;
    drawable_attach(&v->v.d, DRAW_VIEWPORT, NULL);
    return self;
}

static VALUE viewport_dispose(VALUE self) {
    RbViewport *v = rb_check_typeddata(self, &viewport_type);
    if (!v->v.disposed) {
        v->v.disposed = true;
        drawable_detach(&v->v.d);
    }
    return Qnil;
}

static VALUE viewport_disposed(VALUE self) { return ((RbViewport *)rb_check_typeddata(self, &viewport_type))->v.disposed ? Qtrue : Qfalse; }

static VALUE viewport_flash(VALUE self, VALUE color, VALUE duration) {
    RbViewport *v = get_viewport(self);
    flash_start(&v->v.flash, NIL_P(color) ? NULL : &rgss_color(color)->c, NUM2INT(duration));
    return Qnil;
}

static VALUE viewport_update(VALUE self) {
    flash_update(&get_viewport(self)->v.flash);
    return Qnil;
}

static VALUE viewport_get_rect(VALUE self) { return get_viewport(self)->rect; }
static VALUE viewport_put_rect(VALUE self, VALUE r) {
    RbViewport *v = get_viewport(self);
    v->rect = r;
    v->v.rect = &rgss_rect(r)->r;
    return r;
}
static VALUE viewport_get_color(VALUE self) { return get_viewport(self)->color; }
static VALUE viewport_put_color(VALUE self, VALUE c) {
    RbViewport *v = get_viewport(self);
    v->v.color = &rgss_color(c)->c;
    v->color = c;
    return c;
}
static VALUE viewport_get_tone(VALUE self) { return get_viewport(self)->tone; }
static VALUE viewport_put_tone(VALUE self, VALUE t) {
    RbViewport *v = get_viewport(self);
    v->v.tone = &rgss_tone(t)->t;
    v->tone = t;
    return t;
}
static VALUE viewport_get_visible(VALUE self) { return get_viewport(self)->v.visible ? Qtrue : Qfalse; }
static VALUE viewport_put_visible(VALUE self, VALUE b) { get_viewport(self)->v.visible = RTEST(b); return b; }
static VALUE viewport_get_z(VALUE self) { return INT2NUM(get_viewport(self)->v.z); }
static VALUE viewport_put_z(VALUE self, VALUE z) { get_viewport(self)->v.z = NUM2INT(z); return z; }
static VALUE viewport_get_ox(VALUE self) { return INT2NUM(get_viewport(self)->v.ox); }
static VALUE viewport_put_ox(VALUE self, VALUE z) { get_viewport(self)->v.ox = NUM2INT(z); return z; }
static VALUE viewport_get_oy(VALUE self) { return INT2NUM(get_viewport(self)->v.oy); }
static VALUE viewport_put_oy(VALUE self, VALUE z) { get_viewport(self)->v.oy = NUM2INT(z); return z; }

// MARK: Sprite

static void sprite_mark(void *p) {
    RbSprite *s = p;
    rb_gc_mark(s->viewport);
    rb_gc_mark(s->bitmap);
    rb_gc_mark(s->srcRect);
    rb_gc_mark(s->color);
    rb_gc_mark(s->tone);
}

static void sprite_free(void *p) {
    RbSprite *s = p;
    drawable_detach(&s->s.d);
    ruby_xfree(s);
}

static const rb_data_type_t sprite_type = {"Sprite", {sprite_mark, sprite_free, NULL}, NULL, NULL, 0};

static VALUE sprite_alloc(VALUE klass) {
    RbSprite *s;
    VALUE obj = TypedData_Make_Struct(klass, RbSprite, &sprite_type, s);
    s->viewport = s->bitmap = Qnil;
    s->srcRect = rgss_rect_new(0, 0, 0, 0);
    s->color = rgss_color_new(0, 0, 0, 0);
    s->tone = rgss_tone_new(0, 0, 0, 0);
    s->s.srcRect = &rgss_rect(s->srcRect)->r;
    s->s.color = &rgss_color(s->color)->c;
    s->s.tone = &rgss_tone(s->tone)->t;
    s->s.visible = true;
    s->s.zoomX = s->s.zoomY = 1;
    s->s.opacity = 255;
    s->s.bushOpacity = 128;
    s->s.waveSpeed = 360;
    s->s.waveLength = 180;
    return obj;
}

static RbSprite *get_sprite(VALUE self) {
    RbSprite *s = rb_check_typeddata(self, &sprite_type);
    return s;
}

static VALUE sprite_initialize(int argc, VALUE *argv, VALUE self) {
    rb_check_arity(argc, 0, 1);
    RbSprite *s = rb_check_typeddata(self, &sprite_type);
    VALUE viewport = argc > 0 ? argv[0] : Qnil;
    s->s.viewport = viewport_arg(viewport);
    s->viewport = viewport;
    drawable_attach(&s->s.d, DRAW_SPRITE, s->s.viewport);
    return self;
}

static VALUE sprite_dispose(VALUE self) {
    RbSprite *s = rb_check_typeddata(self, &sprite_type);
    if (!s->s.disposed) {
        s->s.disposed = true;
        drawable_detach(&s->s.d);
    }
    return Qnil;
}

static VALUE sprite_disposed(VALUE self) { return ((RbSprite *)rb_check_typeddata(self, &sprite_type))->s.disposed ? Qtrue : Qfalse; }

static VALUE sprite_flash(VALUE self, VALUE color, VALUE duration) {
    RbSprite *s = get_sprite(self);
    flash_start(&s->s.flash, NIL_P(color) ? NULL : &rgss_color(color)->c, NUM2INT(duration));
    return Qnil;
}

static VALUE sprite_update(VALUE self) {
    RbSprite *s = get_sprite(self);
    flash_update(&s->s.flash);
    if (s->s.waveAmp) {
        // wave_speed is in degrees per frame across 8 lines of wave_length, as the help's default of 360 suggests.
        s->s.wavePhase = fmod(s->s.wavePhase + s->s.waveSpeed / 8.0 * 360.0 / (s->s.waveLength > 0 ? s->s.waveLength : 1) / 45.0, 360.0);
    }
    return Qnil;
}

static VALUE sprite_get_bitmap(VALUE self) { return get_sprite(self)->bitmap; }
static VALUE sprite_put_bitmap(VALUE self, VALUE b) {
    RbSprite *s = get_sprite(self);
    s->s.bitmap = rgss_bitmap_or_null(b);
    s->bitmap = b;
    // Setting a bitmap resets the source rectangle to all of it.
    if (s->s.bitmap && !s->s.bitmap->disposed) *s->s.srcRect = (IRect){0, 0, s->s.bitmap->image.w, s->s.bitmap->image.h};
    return b;
}
static VALUE sprite_get_src_rect(VALUE self) { return get_sprite(self)->srcRect; }
static VALUE sprite_put_src_rect(VALUE self, VALUE r) {
    RbSprite *s = get_sprite(self);
    s->s.srcRect = &rgss_rect(r)->r;
    s->srcRect = r;
    return r;
}
static VALUE sprite_get_color(VALUE self) { return get_sprite(self)->color; }
static VALUE sprite_put_color(VALUE self, VALUE c) {
    RbSprite *s = get_sprite(self);
    s->s.color = &rgss_color(c)->c;
    s->color = c;
    return c;
}
static VALUE sprite_get_tone(VALUE self) { return get_sprite(self)->tone; }
static VALUE sprite_put_tone(VALUE self, VALUE t) {
    RbSprite *s = get_sprite(self);
    s->s.tone = &rgss_tone(t)->t;
    s->tone = t;
    return t;
}
static VALUE sprite_get_viewport(VALUE self) { return get_sprite(self)->viewport; }
static VALUE sprite_put_viewport(VALUE self, VALUE v) {
    RbSprite *s = get_sprite(self);
    drawable_detach(&s->s.d);
    s->s.viewport = viewport_arg(v);
    s->viewport = v;
    uint64_t serial = s->s.d.serial;
    drawable_attach(&s->s.d, DRAW_SPRITE, s->s.viewport);
    s->s.d.serial = serial;
    return v;
}
static VALUE sprite_width(VALUE self) { return INT2NUM(get_sprite(self)->s.srcRect->w); }
static VALUE sprite_height(VALUE self) { return INT2NUM(get_sprite(self)->s.srcRect->h); }

#define INT_PROP(type, getter, name, field)                                                                            \
    static VALUE type##_get_##name(VALUE self) { return INT2NUM(getter(self)->field); }                               \
    static VALUE type##_put_##name(VALUE self, VALUE v) {                                                              \
        getter(self)->field = NUM2INT(v);                                                                              \
        return v;                                                                                                      \
    }
#define OPACITY_PROP(type, getter, name, field)                                                                        \
    static VALUE type##_get_##name(VALUE self) { return INT2NUM(getter(self)->field); }                               \
    static VALUE type##_put_##name(VALUE self, VALUE v) {                                                              \
        getter(self)->field = clamp_opacity(v);                                                                        \
        return v;                                                                                                      \
    }
#define BOOL_PROP(type, getter, name, field)                                                                           \
    static VALUE type##_get_##name(VALUE self) { return getter(self)->field ? Qtrue : Qfalse; }                       \
    static VALUE type##_put_##name(VALUE self, VALUE v) {                                                              \
        getter(self)->field = RTEST(v);                                                                                \
        return v;                                                                                                      \
    }
#define FLOAT_PROP(type, getter, name, field)                                                                          \
    static VALUE type##_get_##name(VALUE self) { return DBL2NUM(getter(self)->field); }                               \
    static VALUE type##_put_##name(VALUE self, VALUE v) {                                                              \
        getter(self)->field = NUM2DBL(v);                                                                              \
        return v;                                                                                                      \
    }

INT_PROP(sprite, get_sprite, x, s.x)
INT_PROP(sprite, get_sprite, y, s.y)
INT_PROP(sprite, get_sprite, z, s.z)
INT_PROP(sprite, get_sprite, ox, s.ox)
INT_PROP(sprite, get_sprite, oy, s.oy)
INT_PROP(sprite, get_sprite, bush_depth, s.bushDepth)
INT_PROP(sprite, get_sprite, blend_type, s.blendType)
INT_PROP(sprite, get_sprite, wave_amp, s.waveAmp)
INT_PROP(sprite, get_sprite, wave_length, s.waveLength)
INT_PROP(sprite, get_sprite, wave_speed, s.waveSpeed)
OPACITY_PROP(sprite, get_sprite, opacity, s.opacity)
OPACITY_PROP(sprite, get_sprite, bush_opacity, s.bushOpacity)
BOOL_PROP(sprite, get_sprite, visible, s.visible)
BOOL_PROP(sprite, get_sprite, mirror, s.mirror)
FLOAT_PROP(sprite, get_sprite, zoom_x, s.zoomX)
FLOAT_PROP(sprite, get_sprite, zoom_y, s.zoomY)
FLOAT_PROP(sprite, get_sprite, angle, s.angle)
FLOAT_PROP(sprite, get_sprite, wave_phase, s.wavePhase)

// MARK: Plane

static void plane_mark(void *p) {
    RbPlane *s = p;
    rb_gc_mark(s->viewport);
    rb_gc_mark(s->bitmap);
    rb_gc_mark(s->color);
    rb_gc_mark(s->tone);
}

static void plane_free(void *p) {
    RbPlane *s = p;
    drawable_detach(&s->p.d);
    ruby_xfree(s);
}

static const rb_data_type_t plane_type = {"Plane", {plane_mark, plane_free, NULL}, NULL, NULL, 0};

static VALUE plane_alloc(VALUE klass) {
    RbPlane *s;
    VALUE obj = TypedData_Make_Struct(klass, RbPlane, &plane_type, s);
    s->viewport = s->bitmap = Qnil;
    s->color = rgss_color_new(0, 0, 0, 0);
    s->tone = rgss_tone_new(0, 0, 0, 0);
    s->p.color = &rgss_color(s->color)->c;
    s->p.tone = &rgss_tone(s->tone)->t;
    s->p.visible = true;
    s->p.zoomX = s->p.zoomY = 1;
    s->p.opacity = 255;
    return obj;
}

static RbPlane *get_plane(VALUE self) {
    RbPlane *s = rb_check_typeddata(self, &plane_type);
    return s;
}

static VALUE plane_initialize(int argc, VALUE *argv, VALUE self) {
    rb_check_arity(argc, 0, 1);
    RbPlane *s = rb_check_typeddata(self, &plane_type);
    VALUE viewport = argc > 0 ? argv[0] : Qnil;
    s->p.viewport = viewport_arg(viewport);
    s->viewport = viewport;
    drawable_attach(&s->p.d, DRAW_PLANE, s->p.viewport);
    return self;
}

static VALUE plane_dispose(VALUE self) {
    RbPlane *s = rb_check_typeddata(self, &plane_type);
    if (!s->p.disposed) {
        s->p.disposed = true;
        drawable_detach(&s->p.d);
    }
    return Qnil;
}

static VALUE plane_disposed(VALUE self) { return ((RbPlane *)rb_check_typeddata(self, &plane_type))->p.disposed ? Qtrue : Qfalse; }
static VALUE plane_get_bitmap(VALUE self) { return get_plane(self)->bitmap; }
static VALUE plane_put_bitmap(VALUE self, VALUE b) {
    RbPlane *s = get_plane(self);
    s->p.bitmap = rgss_bitmap_or_null(b);
    s->bitmap = b;
    return b;
}
static VALUE plane_get_color(VALUE self) { return get_plane(self)->color; }
static VALUE plane_put_color(VALUE self, VALUE c) {
    RbPlane *s = get_plane(self);
    s->p.color = &rgss_color(c)->c;
    s->color = c;
    return c;
}
static VALUE plane_get_tone(VALUE self) { return get_plane(self)->tone; }
static VALUE plane_put_tone(VALUE self, VALUE t) {
    RbPlane *s = get_plane(self);
    s->p.tone = &rgss_tone(t)->t;
    s->tone = t;
    return t;
}
static VALUE plane_get_viewport(VALUE self) { return get_plane(self)->viewport; }
INT_PROP(plane, get_plane, z, p.z)
INT_PROP(plane, get_plane, ox, p.ox)
INT_PROP(plane, get_plane, oy, p.oy)
INT_PROP(plane, get_plane, blend_type, p.blendType)
OPACITY_PROP(plane, get_plane, opacity, p.opacity)
BOOL_PROP(plane, get_plane, visible, p.visible)
FLOAT_PROP(plane, get_plane, zoom_x, p.zoomX)
FLOAT_PROP(plane, get_plane, zoom_y, p.zoomY)

// MARK: Window

static void window_mark(void *p) {
    RbWindow *w = p;
    rb_gc_mark(w->viewport);
    rb_gc_mark(w->windowskin);
    rb_gc_mark(w->contents);
    rb_gc_mark(w->cursorRect);
    rb_gc_mark(w->tone);
}

static void window_free(void *p) {
    RbWindow *w = p;
    drawable_detach(&w->w.d);
    ruby_xfree(w);
}

static const rb_data_type_t window_type = {"Window", {window_mark, window_free, NULL}, NULL, NULL, 0};

static VALUE window_alloc(VALUE klass) {
    RbWindow *w;
    VALUE obj = TypedData_Make_Struct(klass, RbWindow, &window_type, w);
    w->viewport = w->windowskin = w->contents = Qnil;
    w->cursorRect = rgss_rect_new(0, 0, 0, 0);
    w->tone = rgss_tone_new(0, 0, 0, 0);
    w->w.cursorRect = &rgss_rect(w->cursorRect)->r;
    w->w.tone = &rgss_tone(w->tone)->t;
    w->w.stretch = w->w.active = w->w.visible = w->w.arrowsVisible = true;
    w->w.opacity = w->w.contentsOpacity = w->w.openness = 255;
    w->w.backOpacity = rgss_version == RGSS3 ? 192 : 255;
    w->w.z = rgss_version == RGSS3 ? 100 : 0;
    w->w.padding = w->w.paddingBottom = rgss_version == RGSS3 ? 12 : 16;
    return obj;
}

static RbWindow *get_window(VALUE self) {
    RbWindow *w = rb_check_typeddata(self, &window_type);
    return w;
}

static VALUE window_initialize(int argc, VALUE *argv, VALUE self) {
    RbWindow *w = rb_check_typeddata(self, &window_type);
    VALUE viewport = Qnil;
    if (rgss_version == RGSS3) {
        if (argc != 0 && argc != 4) rb_raise(rb_eArgError, "wrong number of arguments (given %d, expected 0 or 4)", argc);
        if (argc == 4) {
            w->w.x = NUM2INT(argv[0]);
            w->w.y = NUM2INT(argv[1]);
            w->w.width = NUM2INT(argv[2]);
            w->w.height = NUM2INT(argv[3]);
        }
    } else {
        rb_check_arity(argc, 0, 1);
        if (argc > 0) viewport = argv[0];
    }
    w->w.viewport = viewport_arg(viewport);
    w->viewport = viewport;
    drawable_attach(&w->w.d, DRAW_WINDOW, w->w.viewport);
    // VX Ace windows start with an empty contents bitmap, which Window_Base disposes before making its own.
    if (rgss_version == RGSS3) {
        VALUE size[2] = {INT2FIX(1), INT2FIX(1)};
        window_put_contents(self, rb_class_new_instance(2, size, rgss_cBitmap));
    }
    return self;
}

static VALUE window_dispose(VALUE self) {
    RbWindow *w = rb_check_typeddata(self, &window_type);
    if (!w->w.disposed) {
        w->w.disposed = true;
        drawable_detach(&w->w.d);
    }
    return Qnil;
}

static VALUE window_disposed(VALUE self) { return ((RbWindow *)rb_check_typeddata(self, &window_type))->w.disposed ? Qtrue : Qfalse; }
static VALUE window_update_m(VALUE self) {
    window_update(&get_window(self)->w);
    return Qnil;
}
static VALUE window_move(VALUE self, VALUE x, VALUE y, VALUE w, VALUE h) {
    RbWindow *win = get_window(self);
    win->w.x = NUM2INT(x);
    win->w.y = NUM2INT(y);
    win->w.width = NUM2INT(w);
    win->w.height = NUM2INT(h);
    return Qnil;
}
static VALUE window_open_p(VALUE self) { return get_window(self)->w.openness == 255 ? Qtrue : Qfalse; }
static VALUE window_close_p(VALUE self) { return get_window(self)->w.openness == 0 ? Qtrue : Qfalse; }
static VALUE window_get_windowskin(VALUE self) { return get_window(self)->windowskin; }
static VALUE window_put_windowskin(VALUE self, VALUE b) {
    RbWindow *w = get_window(self);
    w->w.windowskin = rgss_bitmap_or_null(b);
    w->windowskin = b;
    return b;
}
static VALUE window_get_contents(VALUE self) { return get_window(self)->contents; }
static VALUE window_put_contents(VALUE self, VALUE b) {
    RbWindow *w = get_window(self);
    w->w.contents = rgss_bitmap_or_null(b);
    w->contents = b;
    return b;
}
static VALUE window_get_cursor_rect(VALUE self) { return get_window(self)->cursorRect; }
static VALUE window_put_cursor_rect(VALUE self, VALUE r) {
    RbWindow *w = get_window(self);
    w->w.cursorRect = &rgss_rect(r)->r;
    w->cursorRect = r;
    return r;
}
static VALUE window_get_tone(VALUE self) { return get_window(self)->tone; }
static VALUE window_put_tone(VALUE self, VALUE t) {
    RbWindow *w = get_window(self);
    w->w.tone = &rgss_tone(t)->t;
    w->tone = t;
    return t;
}
static VALUE window_get_viewport(VALUE self) { return get_window(self)->viewport; }
static VALUE window_put_viewport(VALUE self, VALUE v) {
    RbWindow *w = get_window(self);
    drawable_detach(&w->w.d);
    w->w.viewport = viewport_arg(v);
    w->viewport = v;
    uint64_t serial = w->w.d.serial;
    drawable_attach(&w->w.d, DRAW_WINDOW, w->w.viewport);
    w->w.d.serial = serial;
    return v;
}
static VALUE window_put_padding(VALUE self, VALUE v) {
    RbWindow *w = get_window(self);
    w->w.padding = w->w.paddingBottom = NUM2INT(v);
    return v;
}
static VALUE window_get_padding(VALUE self) { return INT2NUM(get_window(self)->w.padding); }
INT_PROP(window, get_window, x, w.x)
INT_PROP(window, get_window, y, w.y)
INT_PROP(window, get_window, z, w.z)
INT_PROP(window, get_window, width, w.width)
INT_PROP(window, get_window, height, w.height)
INT_PROP(window, get_window, ox, w.ox)
INT_PROP(window, get_window, oy, w.oy)
INT_PROP(window, get_window, padding_bottom, w.paddingBottom)
OPACITY_PROP(window, get_window, opacity, w.opacity)
OPACITY_PROP(window, get_window, back_opacity, w.backOpacity)
OPACITY_PROP(window, get_window, contents_opacity, w.contentsOpacity)
OPACITY_PROP(window, get_window, openness, w.openness)
BOOL_PROP(window, get_window, stretch, w.stretch)
BOOL_PROP(window, get_window, active, w.active)
BOOL_PROP(window, get_window, visible, w.visible)
BOOL_PROP(window, get_window, pause, w.pause)
BOOL_PROP(window, get_window, arrows_visible, w.arrowsVisible)

// MARK: Tilemap

static void tilemap_mark(void *p) {
    RbTilemap *t = p;
    rb_gc_mark(t->viewport);
    for (int i = 0; i < 9; i++) rb_gc_mark(t->bitmaps[i]);
    rb_gc_mark(t->tileset);
    rb_gc_mark(t->mapData);
    rb_gc_mark(t->flashData);
    rb_gc_mark(t->flags);
    rb_gc_mark(t->bitmapsProxy);
}

static void tilemap_free(void *p) {
    RbTilemap *t = p;
    drawable_detach(&t->t.d);
    ruby_xfree(t);
}

static const rb_data_type_t tilemap_type = {"Tilemap", {tilemap_mark, tilemap_free, NULL}, NULL, NULL, 0};

static VALUE tilemap_alloc(VALUE klass) {
    RbTilemap *t;
    VALUE obj = TypedData_Make_Struct(klass, RbTilemap, &tilemap_type, t);
    t->viewport = t->tileset = t->mapData = t->flashData = t->flags = t->bitmapsProxy = Qnil;
    for (int i = 0; i < 9; i++) t->bitmaps[i] = Qnil;
    t->t.visible = true;
    return obj;
}

static RbTilemap *get_tilemap(VALUE self) {
    RbTilemap *t = rb_check_typeddata(self, &tilemap_type);
    return t;
}

// `tilemap.bitmaps` (VX, VX Ace) and `tilemap.autotiles` (XP) return this, which indexes into the tilemap.
static VALUE proxy_get(VALUE self, VALUE index) {
    RbTilemap *t = get_tilemap(rb_iv_get(self, "@tilemap"));
    int i = NUM2INT(index);
    return i >= 0 && i < 9 ? t->bitmaps[i] : Qnil;
}

static VALUE proxy_put(VALUE self, VALUE index, VALUE bitmap) {
    RbTilemap *t = get_tilemap(rb_iv_get(self, "@tilemap"));
    int i = NUM2INT(index);
    if (i < 0 || i >= 9) return bitmap;
    t->t.bitmaps[i] = rgss_bitmap_or_null(bitmap);
    t->bitmaps[i] = bitmap;
    return bitmap;
}

static VALUE tilemap_initialize(int argc, VALUE *argv, VALUE self) {
    rb_check_arity(argc, 0, 1);
    RbTilemap *t = rb_check_typeddata(self, &tilemap_type);
    VALUE viewport = argc > 0 ? argv[0] : Qnil;
    t->t.viewport = viewport_arg(viewport);
    t->viewport = viewport;
    t->bitmapsProxy = rb_obj_alloc(cTilemapBitmaps);
    rb_iv_set(t->bitmapsProxy, "@tilemap", self);
    drawable_attach(&t->t.d, DRAW_TILEMAP, t->t.viewport);
    return self;
}

static VALUE tilemap_dispose(VALUE self) {
    RbTilemap *t = rb_check_typeddata(self, &tilemap_type);
    if (!t->t.disposed) {
        t->t.disposed = true;
        drawable_detach(&t->t.d);
    }
    return Qnil;
}

static VALUE tilemap_disposed(VALUE self) { return ((RbTilemap *)rb_check_typeddata(self, &tilemap_type))->t.disposed ? Qtrue : Qfalse; }
static VALUE tilemap_update_m(VALUE self) {
    tilemap_update(&get_tilemap(self)->t);
    return Qnil;
}
static VALUE tilemap_get_bitmaps(VALUE self) { return get_tilemap(self)->bitmapsProxy; }
static VALUE tilemap_get_viewport(VALUE self) { return get_tilemap(self)->viewport; }
static VALUE tilemap_get_tileset(VALUE self) { return get_tilemap(self)->tileset; }
static VALUE tilemap_put_tileset(VALUE self, VALUE b) {
    RbTilemap *t = get_tilemap(self);
    t->t.tileset = rgss_bitmap_or_null(b);
    t->tileset = b;
    return b;
}
#define TABLE_PROP(name, field)                                                                                        \
    static VALUE tilemap_get_##name(VALUE self) { return get_tilemap(self)->field; }                                  \
    static VALUE tilemap_put_##name(VALUE self, VALUE v) {                                                             \
        RbTilemap *t = get_tilemap(self);                                                                              \
        t->t.field = NIL_P(v) ? NULL : rgss_table(v);                                                                  \
        t->field = v;                                                                                                  \
        return v;                                                                                                      \
    }
TABLE_PROP(map_data, mapData)
TABLE_PROP(flash_data, flashData)
TABLE_PROP(flags, flags)
INT_PROP(tilemap, get_tilemap, ox, t.ox)
INT_PROP(tilemap, get_tilemap, oy, t.oy)
BOOL_PROP(tilemap, get_tilemap, visible, t.visible)

// MARK: Definitions

#define DEF(klass, name, fn, n) rb_define_method(klass, name, RUBY_METHOD_FUNC(fn), n)
#define ACCESSOR(klass, type, name)                                                                                    \
    do {                                                                                                               \
        DEF(klass, #name, type##_get_##name, 0);                                                                       \
        DEF(klass, #name "=", type##_put_##name, 1);                                                                   \
    } while (0)

void rgss_define_drawables(void) {
    bool vx = rgss_version >= RGSS2, ace = rgss_version == RGSS3;

    rgss_cViewport = rb_define_class("Viewport", rb_cObject);
    rb_define_alloc_func(rgss_cViewport, viewport_alloc);
    DEF(rgss_cViewport, "initialize", viewport_initialize, -1);
    DEF(rgss_cViewport, "dispose", viewport_dispose, 0);
    DEF(rgss_cViewport, "disposed?", viewport_disposed, 0);
    DEF(rgss_cViewport, "flash", viewport_flash, 2);
    DEF(rgss_cViewport, "update", viewport_update, 0);
    ACCESSOR(rgss_cViewport, viewport, rect);
    ACCESSOR(rgss_cViewport, viewport, visible);
    ACCESSOR(rgss_cViewport, viewport, z);
    ACCESSOR(rgss_cViewport, viewport, ox);
    ACCESSOR(rgss_cViewport, viewport, oy);
    ACCESSOR(rgss_cViewport, viewport, color);
    ACCESSOR(rgss_cViewport, viewport, tone);

    rgss_cSprite = rb_define_class("Sprite", rb_cObject);
    rb_define_alloc_func(rgss_cSprite, sprite_alloc);
    DEF(rgss_cSprite, "initialize", sprite_initialize, -1);
    DEF(rgss_cSprite, "dispose", sprite_dispose, 0);
    DEF(rgss_cSprite, "disposed?", sprite_disposed, 0);
    DEF(rgss_cSprite, "flash", sprite_flash, 2);
    DEF(rgss_cSprite, "update", sprite_update, 0);
    DEF(rgss_cSprite, "viewport", sprite_get_viewport, 0);
    ACCESSOR(rgss_cSprite, sprite, bitmap);
    ACCESSOR(rgss_cSprite, sprite, src_rect);
    ACCESSOR(rgss_cSprite, sprite, visible);
    ACCESSOR(rgss_cSprite, sprite, x);
    ACCESSOR(rgss_cSprite, sprite, y);
    ACCESSOR(rgss_cSprite, sprite, z);
    ACCESSOR(rgss_cSprite, sprite, ox);
    ACCESSOR(rgss_cSprite, sprite, oy);
    ACCESSOR(rgss_cSprite, sprite, zoom_x);
    ACCESSOR(rgss_cSprite, sprite, zoom_y);
    ACCESSOR(rgss_cSprite, sprite, angle);
    ACCESSOR(rgss_cSprite, sprite, mirror);
    ACCESSOR(rgss_cSprite, sprite, bush_depth);
    ACCESSOR(rgss_cSprite, sprite, opacity);
    ACCESSOR(rgss_cSprite, sprite, blend_type);
    ACCESSOR(rgss_cSprite, sprite, color);
    ACCESSOR(rgss_cSprite, sprite, tone);
    if (vx) {
        DEF(rgss_cSprite, "width", sprite_width, 0);
        DEF(rgss_cSprite, "height", sprite_height, 0);
        DEF(rgss_cSprite, "viewport=", sprite_put_viewport, 1);
        ACCESSOR(rgss_cSprite, sprite, wave_amp);
        ACCESSOR(rgss_cSprite, sprite, wave_length);
        ACCESSOR(rgss_cSprite, sprite, wave_speed);
        ACCESSOR(rgss_cSprite, sprite, wave_phase);
        ACCESSOR(rgss_cSprite, sprite, bush_opacity);
    }

    rgss_cPlane = rb_define_class("Plane", rb_cObject);
    rb_define_alloc_func(rgss_cPlane, plane_alloc);
    DEF(rgss_cPlane, "initialize", plane_initialize, -1);
    DEF(rgss_cPlane, "dispose", plane_dispose, 0);
    DEF(rgss_cPlane, "disposed?", plane_disposed, 0);
    DEF(rgss_cPlane, "viewport", plane_get_viewport, 0);
    ACCESSOR(rgss_cPlane, plane, bitmap);
    ACCESSOR(rgss_cPlane, plane, visible);
    ACCESSOR(rgss_cPlane, plane, z);
    ACCESSOR(rgss_cPlane, plane, ox);
    ACCESSOR(rgss_cPlane, plane, oy);
    ACCESSOR(rgss_cPlane, plane, zoom_x);
    ACCESSOR(rgss_cPlane, plane, zoom_y);
    ACCESSOR(rgss_cPlane, plane, opacity);
    ACCESSOR(rgss_cPlane, plane, blend_type);
    ACCESSOR(rgss_cPlane, plane, color);
    ACCESSOR(rgss_cPlane, plane, tone);

    rgss_cWindow = rb_define_class("Window", rb_cObject);
    rb_define_alloc_func(rgss_cWindow, window_alloc);
    DEF(rgss_cWindow, "initialize", window_initialize, -1);
    DEF(rgss_cWindow, "dispose", window_dispose, 0);
    DEF(rgss_cWindow, "disposed?", window_disposed, 0);
    DEF(rgss_cWindow, "update", window_update_m, 0);
    DEF(rgss_cWindow, "viewport", window_get_viewport, 0);
    ACCESSOR(rgss_cWindow, window, windowskin);
    ACCESSOR(rgss_cWindow, window, contents);
    ACCESSOR(rgss_cWindow, window, cursor_rect);
    ACCESSOR(rgss_cWindow, window, active);
    ACCESSOR(rgss_cWindow, window, visible);
    ACCESSOR(rgss_cWindow, window, pause);
    ACCESSOR(rgss_cWindow, window, x);
    ACCESSOR(rgss_cWindow, window, y);
    ACCESSOR(rgss_cWindow, window, width);
    ACCESSOR(rgss_cWindow, window, height);
    ACCESSOR(rgss_cWindow, window, z);
    ACCESSOR(rgss_cWindow, window, ox);
    ACCESSOR(rgss_cWindow, window, oy);
    ACCESSOR(rgss_cWindow, window, opacity);
    ACCESSOR(rgss_cWindow, window, back_opacity);
    ACCESSOR(rgss_cWindow, window, contents_opacity);
    if (!vx) ACCESSOR(rgss_cWindow, window, stretch);
    if (vx) {
        ACCESSOR(rgss_cWindow, window, openness);
        DEF(rgss_cWindow, "viewport=", window_put_viewport, 1);
    }
    if (ace) {
        DEF(rgss_cWindow, "move", window_move, 4);
        DEF(rgss_cWindow, "open?", window_open_p, 0);
        DEF(rgss_cWindow, "close?", window_close_p, 0);
        ACCESSOR(rgss_cWindow, window, arrows_visible);
        ACCESSOR(rgss_cWindow, window, padding);
        ACCESSOR(rgss_cWindow, window, padding_bottom);
        ACCESSOR(rgss_cWindow, window, tone);
    }

    rgss_cTilemap = rb_define_class("Tilemap", rb_cObject);
    cTilemapBitmaps = rb_define_class_under(rgss_cTilemap, "BitmapArray", rb_cObject);
    DEF(cTilemapBitmaps, "[]", proxy_get, 1);
    DEF(cTilemapBitmaps, "[]=", proxy_put, 2);
    rb_define_alloc_func(rgss_cTilemap, tilemap_alloc);
    DEF(rgss_cTilemap, "initialize", tilemap_initialize, -1);
    DEF(rgss_cTilemap, "dispose", tilemap_dispose, 0);
    DEF(rgss_cTilemap, "disposed?", tilemap_disposed, 0);
    DEF(rgss_cTilemap, "update", tilemap_update_m, 0);
    DEF(rgss_cTilemap, "viewport", tilemap_get_viewport, 0);
    ACCESSOR(rgss_cTilemap, tilemap, map_data);
    ACCESSOR(rgss_cTilemap, tilemap, flash_data);
    ACCESSOR(rgss_cTilemap, tilemap, ox);
    ACCESSOR(rgss_cTilemap, tilemap, oy);
    ACCESSOR(rgss_cTilemap, tilemap, visible);
    if (vx) {
        DEF(rgss_cTilemap, "bitmaps", tilemap_get_bitmaps, 0);
        DEF(rgss_cTilemap, ace ? "flags" : "passages", tilemap_get_flags, 0);
        DEF(rgss_cTilemap, ace ? "flags=" : "passages=", tilemap_put_flags, 1);
    } else {
        DEF(rgss_cTilemap, "autotiles", tilemap_get_bitmaps, 0);
        ACCESSOR(rgss_cTilemap, tilemap, tileset);
        DEF(rgss_cTilemap, "priorities", tilemap_get_flags, 0);
        DEF(rgss_cTilemap, "priorities=", tilemap_put_flags, 1);
    }
}

// Font and Bitmap.
#include "binding.h"

#include <ruby/encoding.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

VALUE rgss_cFont, rgss_cBitmap;

// MARK: Font

// Font's class-level defaults, which every new Font copies.
static VALUE defaultName, defaultColor, defaultOutColor;
static int defaultSize;
static bool defaultBold, defaultItalic, defaultShadow, defaultOutline;

static void font_mark(void *p) {
    RbFont *f = p;
    rb_gc_mark(f->name);
    rb_gc_mark(f->color);
    rb_gc_mark(f->outColor);
}

static const rb_data_type_t font_type = {"Font", {font_mark, RUBY_TYPED_DEFAULT_FREE, NULL}, NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY};

RbFont *rgss_font(VALUE v) { return rb_check_typeddata(v, &font_type); }

static VALUE copy_color(VALUE color) {
    ColorF c = rgss_color(color)->c;
    return rgss_color_new(c.r, c.g, c.b, c.a);
}

static VALUE dup_name(VALUE name) { return RB_TYPE_P(name, T_ARRAY) ? rb_ary_dup(name) : rb_obj_dup(name); }

static VALUE font_alloc(VALUE klass) {
    RbFont *f;
    VALUE v = TypedData_Make_Struct(klass, RbFont, &font_type, f);
    f->name = dup_name(defaultName);
    f->size = defaultSize;
    f->bold = defaultBold;
    f->italic = defaultItalic;
    f->shadow = defaultShadow;
    f->outline = defaultOutline;
    f->color = copy_color(defaultColor);
    f->outColor = copy_color(defaultOutColor);
    return v;
}

VALUE rgss_font_new_default(void) { return font_alloc(rgss_cFont); }

static VALUE font_initialize(int argc, VALUE *argv, VALUE self) {
    rb_check_arity(argc, 0, 2);
    RbFont *f = rgss_font(self);
    if (argc > 0) f->name = argv[0];
    if (argc > 1) f->size = NUM2INT(argv[1]);
    return self;
}

static VALUE font_init_copy(VALUE self, VALUE other) {
    RbFont *f = rgss_font(self), *o = rgss_font(other);
    *f = *o;
    f->name = dup_name(o->name);
    f->color = copy_color(o->color);
    f->outColor = copy_color(o->outColor);
    return self;
}

VALUE rgss_font_copy(VALUE font) {
    VALUE copy = font_alloc(rgss_cFont);
    font_init_copy(copy, font);
    return copy;
}

static VALUE font_get_name(VALUE self) { return rgss_font(self)->name; }
static VALUE font_put_name(VALUE self, VALUE v) { return rgss_font(self)->name = v; }
static VALUE font_get_size(VALUE self) { return INT2NUM(rgss_font(self)->size); }
static VALUE font_put_size(VALUE self, VALUE v) {
    int size = NUM2INT(v);
    rgss_font(self)->size = size < 6 ? 6 : size > 96 ? 96 : size;
    return v;
}
#define FONT_FLAG(name)                                                                                                \
    static VALUE font_get_##name(VALUE self) { return rgss_font(self)->name ? Qtrue : Qfalse; }                       \
    static VALUE font_put_##name(VALUE self, VALUE v) {                                                                \
        rgss_font(self)->name = RTEST(v);                                                                              \
        return v;                                                                                                      \
    }
FONT_FLAG(bold)
FONT_FLAG(italic)
FONT_FLAG(shadow)
FONT_FLAG(outline)
static VALUE font_get_color(VALUE self) { return rgss_font(self)->color; }
static VALUE font_put_color(VALUE self, VALUE v) {
    rgss_color(v);
    return rgss_font(self)->color = v;
}
static VALUE font_get_out_color(VALUE self) { return rgss_font(self)->outColor; }
static VALUE font_put_out_color(VALUE self, VALUE v) {
    rgss_color(v);
    return rgss_font(self)->outColor = v;
}

static VALUE font_s_exist(VALUE klass, VALUE name) {
    (void)klass;
    return text_font_exists(StringValueCStr(name)) ? Qtrue : Qfalse;
}

static VALUE font_s_get_name(VALUE klass) { (void)klass; return defaultName; }
static VALUE font_s_put_name(VALUE klass, VALUE v) { (void)klass; return defaultName = v; }
static VALUE font_s_get_size(VALUE klass) { (void)klass; return INT2NUM(defaultSize); }
static VALUE font_s_put_size(VALUE klass, VALUE v) {
    (void)klass;
    int size = NUM2INT(v);
    defaultSize = size < 6 ? 6 : size > 96 ? 96 : size;
    return v;
}
#define FONT_DEFAULT_FLAG(name, var)                                                                                   \
    static VALUE font_s_get_##name(VALUE klass) { (void)klass; return var ? Qtrue : Qfalse; }                         \
    static VALUE font_s_put_##name(VALUE klass, VALUE v) {                                                             \
        (void)klass;                                                                                                   \
        var = RTEST(v);                                                                                                \
        return v;                                                                                                      \
    }
FONT_DEFAULT_FLAG(bold, defaultBold)
FONT_DEFAULT_FLAG(italic, defaultItalic)
FONT_DEFAULT_FLAG(shadow, defaultShadow)
FONT_DEFAULT_FLAG(outline, defaultOutline)
static VALUE font_s_get_color(VALUE klass) { (void)klass; return defaultColor; }
static VALUE font_s_put_color(VALUE klass, VALUE v) { (void)klass; rgss_color(v); return defaultColor = v; }
static VALUE font_s_get_out_color(VALUE klass) { (void)klass; return defaultOutColor; }
static VALUE font_s_put_out_color(VALUE klass, VALUE v) { (void)klass; rgss_color(v); return defaultOutColor = v; }

// The font request text.c takes, kept valid while `holder` (an Array of the name strings) lives.
static FontRequest font_request(RbFont *f, VALUE *holder, const char **names, int max) {
    VALUE list = RB_TYPE_P(f->name, T_ARRAY) ? f->name : rb_ary_new_from_args(1, f->name);
    *holder = list;
    int count = 0;
    for (long i = 0; i < RARRAY_LEN(list) && count < max; i++) {
        VALUE name = rb_ary_entry(list, i);
        if (RB_TYPE_P(name, T_STRING)) names[count++] = StringValueCStr(name);
    }
    return (FontRequest){count ? names[0] : NULL, names, count, f->size, f->bold, f->italic};
}

// MARK: Bitmap

static void bitmap_mark(void *p) { rb_gc_mark(((RbBitmap *)p)->font); }

static void bitmap_free(void *p) {
    RbBitmap *b = p;
    image_free(&b->b.image);
    ruby_xfree(b);
}

static size_t bitmap_memsize(const void *p) {
    const RbBitmap *b = p;
    return sizeof(RbBitmap) + (size_t)b->b.image.w * b->b.image.h * 4;
}

static const rb_data_type_t bitmap_type = {"Bitmap", {bitmap_mark, bitmap_free, bitmap_memsize}, NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY};

static VALUE bitmap_alloc(VALUE klass) {
    RbBitmap *b;
    VALUE v = TypedData_Make_Struct(klass, RbBitmap, &bitmap_type, b);
    b->font = Qnil;
    return v;
}

RbBitmap *rgss_bitmap(VALUE v) {
    RbBitmap *b = rb_check_typeddata(v, &bitmap_type);
    if (b->b.disposed) rgss_raise_disposed("bitmap");
    return b;
}

Bitmap *rgss_bitmap_or_null(VALUE v) {
    if (NIL_P(v)) return NULL;
    return &((RbBitmap *)rb_check_typeddata(v, &bitmap_type))->b;
}

VALUE rgss_bitmap_wrap_image(Image image) {
    VALUE v = bitmap_alloc(rgss_cBitmap);
    RbBitmap *b = rb_check_typeddata(v, &bitmap_type);
    b->b.image = image;
    b->font = rgss_font_new_default();
    rb_gc_adjust_memory_usage((ssize_t)image.w * image.h * 4);
    return v;
}

static void changed(RbBitmap *b) { b->b.generation++; }

static VALUE bitmap_initialize(int argc, VALUE *argv, VALUE self) {
    rb_check_arity(argc, 1, 2);
    RbBitmap *b = rb_check_typeddata(self, &bitmap_type);
    b->font = rgss_font_new_default();
    if (argc == 2) {
        int w = NUM2INT(argv[0]), h = NUM2INT(argv[1]);
        if (w <= 0 || h <= 0) rb_raise(rgss_eRGSSError, "failed to create bitmap");
        if (!image_alloc(&b->b.image, w, h)) rb_raise(rb_eNoMemError, "failed to create bitmap");
    } else {
        VALUE path = argv[0];
        VALUE data = rgss_read_file(path, rgss_image_extensions);
        if (!image_decode((const uint8_t *)RSTRING_PTR(data), (size_t)RSTRING_LEN(data), &b->b.image)) {
            rb_raise(rgss_eRGSSError, "failed to create bitmap from %s", StringValueCStr(path));
        }
    }
    rb_gc_adjust_memory_usage((ssize_t)b->b.image.w * b->b.image.h * 4);
    return self;
}

static VALUE bitmap_init_copy(VALUE self, VALUE other) {
    RbBitmap *b = rb_check_typeddata(self, &bitmap_type), *o = rgss_bitmap(other);
    if (!image_alloc(&b->b.image, o->b.image.w, o->b.image.h)) rb_raise(rb_eNoMemError, "failed to copy bitmap");
    memcpy(b->b.image.px, o->b.image.px, (size_t)o->b.image.w * o->b.image.h * 4);
    b->font = rgss_font_copy(o->font);
    return self;
}

static VALUE bitmap_dispose(VALUE self) {
    RbBitmap *b = rb_check_typeddata(self, &bitmap_type);
    if (!b->b.disposed) {
        rb_gc_adjust_memory_usage(-(ssize_t)b->b.image.w * b->b.image.h * 4);
        image_free(&b->b.image);
        b->b.disposed = true;
    }
    return Qnil;
}

static VALUE bitmap_disposed(VALUE self) {
    return ((RbBitmap *)rb_check_typeddata(self, &bitmap_type))->b.disposed ? Qtrue : Qfalse;
}

static VALUE bitmap_width(VALUE self) { return INT2NUM(rgss_bitmap(self)->b.image.w); }
static VALUE bitmap_height(VALUE self) { return INT2NUM(rgss_bitmap(self)->b.image.h); }
static VALUE bitmap_rect(VALUE self) {
    RbBitmap *b = rgss_bitmap(self);
    return rgss_rect_new(0, 0, b->b.image.w, b->b.image.h);
}

static VALUE bitmap_blt(int argc, VALUE *argv, VALUE self) {
    rb_check_arity(argc, 4, 5);
    RbBitmap *b = rgss_bitmap(self), *src = rgss_bitmap(argv[2]);
    int opacity = argc > 4 ? NUM2INT(argv[4]) : 255;
    image_blt(&b->b.image, NUM2INT(argv[0]), NUM2INT(argv[1]), &src->b.image, rgss_rect(argv[3])->r, opacity);
    changed(b);
    return self;
}

static VALUE bitmap_stretch_blt(int argc, VALUE *argv, VALUE self) {
    rb_check_arity(argc, 3, 4);
    RbBitmap *b = rgss_bitmap(self), *src = rgss_bitmap(argv[1]);
    int opacity = argc > 3 ? NUM2INT(argv[3]) : 255;
    image_stretch_blt(&b->b.image, rgss_rect(argv[0])->r, &src->b.image, rgss_rect(argv[2])->r, opacity);
    changed(b);
    return self;
}

static Pixel pixel_of(VALUE color) {
    ColorF c = rgss_color(color)->c;
    return px_make((int)c.r, (int)c.g, (int)c.b, (int)c.a);
}

// Methods that take either (x, y, width, height, ...) or (rect, ...). Returns how many arguments the area used.
static int area_args(int argc, VALUE *argv, IRect *r) {
    if (argc >= 1 && rb_obj_is_kind_of(argv[0], rgss_cRect)) {
        *r = rgss_rect(argv[0])->r;
        return 1;
    }
    if (argc < 4) rb_raise(rb_eArgError, "wrong number of arguments");
    *r = (IRect){NUM2INT(argv[0]), NUM2INT(argv[1]), NUM2INT(argv[2]), NUM2INT(argv[3])};
    return 4;
}

static VALUE bitmap_fill_rect(int argc, VALUE *argv, VALUE self) {
    RbBitmap *b = rgss_bitmap(self);
    IRect r;
    int used = area_args(argc, argv, &r);
    if (argc != used + 1) rb_raise(rb_eArgError, "wrong number of arguments");
    image_fill(&b->b.image, r, pixel_of(argv[used]));
    changed(b);
    return self;
}

static VALUE bitmap_gradient_fill_rect(int argc, VALUE *argv, VALUE self) {
    RbBitmap *b = rgss_bitmap(self);
    IRect r;
    int used = area_args(argc, argv, &r);
    if (argc < used + 2 || argc > used + 3) rb_raise(rb_eArgError, "wrong number of arguments");
    bool vertical = argc > used + 2 && RTEST(argv[used + 2]);
    image_gradient(&b->b.image, r, pixel_of(argv[used]), pixel_of(argv[used + 1]), vertical);
    changed(b);
    return self;
}

static VALUE bitmap_clear(VALUE self) {
    RbBitmap *b = rgss_bitmap(self);
    memset(b->b.image.px, 0, (size_t)b->b.image.w * b->b.image.h * 4);
    changed(b);
    return self;
}

static VALUE bitmap_clear_rect(int argc, VALUE *argv, VALUE self) {
    RbBitmap *b = rgss_bitmap(self);
    IRect r;
    if (area_args(argc, argv, &r) != argc) rb_raise(rb_eArgError, "wrong number of arguments");
    image_fill(&b->b.image, r, 0);
    changed(b);
    return self;
}

static VALUE bitmap_get_pixel(VALUE self, VALUE vx, VALUE vy) {
    RbBitmap *b = rgss_bitmap(self);
    int x = NUM2INT(vx), y = NUM2INT(vy);
    if (x < 0 || y < 0 || x >= b->b.image.w || y >= b->b.image.h) return rgss_color_new(0, 0, 0, 0);
    Pixel p = b->b.image.px[(size_t)y * b->b.image.w + x];
    return rgss_color_new(px_r(p), px_g(p), px_b(p), px_a(p));
}

static VALUE bitmap_set_pixel(VALUE self, VALUE vx, VALUE vy, VALUE color) {
    RbBitmap *b = rgss_bitmap(self);
    int x = NUM2INT(vx), y = NUM2INT(vy);
    if (x >= 0 && y >= 0 && x < b->b.image.w && y < b->b.image.h) {
        b->b.image.px[(size_t)y * b->b.image.w + x] = pixel_of(color);
        changed(b);
    }
    return self;
}

static VALUE bitmap_hue_change(VALUE self, VALUE hue) {
    RbBitmap *b = rgss_bitmap(self);
    image_hue_change(&b->b.image, NUM2INT(hue));
    changed(b);
    return self;
}

static VALUE bitmap_blur(VALUE self) {
    RbBitmap *b = rgss_bitmap(self);
    image_blur(&b->b.image);
    changed(b);
    return self;
}

static VALUE bitmap_radial_blur(VALUE self, VALUE angle, VALUE division) {
    RbBitmap *b = rgss_bitmap(self);
    image_radial_blur(&b->b.image, NUM2INT(angle), NUM2INT(division));
    changed(b);
    return self;
}

// Puts a coverage mask on the bitmap in one color, at (x, y), squeezed horizontally to `drawW` columns.
static void put_mask(Image *img, IRect clip, const uint8_t *mask, int mw, int mh, int x, int y, int drawW, Pixel color) {
    if (drawW <= 0) return;
    int ca = px_a(color);
    clip = irect_clip(clip, img->w, img->h);
    for (int row = 0; row < mh; row++) {
        int ty = y + row;
        if (ty < clip.y || ty >= clip.y + clip.h) continue;
        Pixel *out = img->px + (size_t)ty * img->w;
        for (int col = 0; col < drawW; col++) {
            int tx = x + col;
            if (tx < clip.x || tx >= clip.x + clip.w) continue;
            int sx = drawW == mw ? col : col * mw / drawW;
            int coverage = mask[row * mw + sx];
            if (!coverage) continue;
            int alpha = coverage * ca / 255;
            if (alpha <= 0) continue;
            Pixel d = out[tx];
            int da = px_a(d);
            int outA = alpha + da * (255 - alpha) / 255;
            if (outA <= 0) continue;
            int dw = da * (255 - alpha) / 255;
            out[tx] = px_make((px_r(color) * alpha + px_r(d) * dw) / outA, (px_g(color) * alpha + px_g(d) * dw) / outA,
                              (px_b(color) * alpha + px_b(d) * dw) / outA, outA);
        }
    }
}

// Grows a mask by one pixel in every direction, for VX Ace's outline.
static uint8_t *dilate(const uint8_t *mask, int w, int h, int *ow, int *oh) {
    int nw = w + 2, nh = h + 2;
    uint8_t *out = calloc((size_t)nw * nh, 1);
    if (!out) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t v = mask[y * w + x];
            if (!v) continue;
            for (int dy = 0; dy < 3; dy++)
                for (int dx = 0; dx < 3; dx++) {
                    uint8_t *o = &out[(y + dy) * nw + x + dx];
                    if (v > *o) *o = v;
                }
        }
    *ow = nw;
    *oh = nh;
    return out;
}

static VALUE text_of(VALUE v) {
    VALUE s = RB_TYPE_P(v, T_STRING) ? v : rb_obj_as_string(v);
    return rb_str_export_to_enc(s, rb_utf8_encoding());
}

static VALUE bitmap_draw_text(int argc, VALUE *argv, VALUE self) {
    RbBitmap *b = rgss_bitmap(self);
    IRect r;
    int used = area_args(argc, argv, &r);
    if (argc < used + 1 || argc > used + 2) rb_raise(rb_eArgError, "wrong number of arguments");
    VALUE str = text_of(argv[used]);
    int align = argc > used + 1 ? NUM2INT(argv[used + 1]) : 0;
    if (RSTRING_LEN(str) == 0 || r.w <= 0 || r.h <= 0) return self;
    RbFont *f = rgss_font(b->font);
    VALUE holder;
    const char *names[16];
    FontRequest request = font_request(f, &holder, names, 16);
    uint8_t *mask;
    int mw, mh, ascent;
    if (!text_render(&request, StringValueCStr(str), &mask, &mw, &mh, &ascent)) return self;
    int textW, textH;
    text_size(&request, StringValueCStr(str), &textW, &textH);
    // Text wider than the box is squeezed to fit, down to 60 percent of its width.
    int drawW = mw;
    if (textW > r.w) {
        int target = r.w > textW * 6 / 10 ? r.w : textW * 6 / 10;
        drawW = mw * target / textW;
        textW = target;
    }
    int x = r.x + (align == 1 ? (r.w - textW) / 2 : align == 2 ? r.w - textW : 0);
    int y = r.y + (r.h - mh) / 2;
    Pixel color = pixel_of(f->color);
    bool outline = rgss_version == RGSS3 && f->outline;
    bool shadow = rgss_version >= RGSS2 && f->shadow;
    if (outline) {
        int ow, oh;
        uint8_t *grown = dilate(mask, mw, mh, &ow, &oh);
        if (grown) {
            put_mask(&b->b.image, r, grown, ow, oh, x - 1, y - 1, drawW + 2, pixel_of(f->outColor));
            free(grown);
        }
    }
    if (shadow) {
        Pixel shade = px_make(0, 0, 0, px_a(color));
        put_mask(&b->b.image, r, mask, mw, mh, x + 1, y + 1, drawW, shade);
    }
    put_mask(&b->b.image, r, mask, mw, mh, x, y, drawW, color);
    free(mask);
    RB_GC_GUARD(holder);
    RB_GC_GUARD(str);
    changed(b);
    return self;
}

static VALUE bitmap_text_size(VALUE self, VALUE text) {
    RbBitmap *b = rgss_bitmap(self);
    VALUE str = text_of(text);
    RbFont *f = rgss_font(b->font);
    VALUE holder;
    const char *names[16];
    FontRequest request = font_request(f, &holder, names, 16);
    int w, h;
    text_size(&request, StringValueCStr(str), &w, &h);
    RB_GC_GUARD(holder);
    return rgss_rect_new(0, 0, w, h);
}

static VALUE bitmap_get_font(VALUE self) { return rgss_bitmap(self)->font; }
static VALUE bitmap_put_font(VALUE self, VALUE font) {
    rgss_font(font);
    rgss_bitmap(self)->font = font;
    return font;
}

void rgss_define_font(void) {
    bool ace = rgss_version == RGSS3, vx = rgss_version == RGSS2;
    defaultName = ace ? rb_ary_new_from_args(2, rb_str_new_cstr("VL Gothic"), rb_str_new_cstr("Verdana"))
                      : vx ? rb_ary_new_from_args(3, rb_str_new_cstr("UmePlus Gothic"), rb_str_new_cstr("MS Gothic"),
                                                  rb_str_new_cstr("Courier New"))
                           : rb_str_new_cstr("MS PGothic");
    defaultSize = ace ? 24 : vx ? 20 : 22;
    defaultBold = defaultItalic = false;
    defaultShadow = vx;
    defaultOutline = ace;
    rb_gc_register_address(&defaultName);
    rb_gc_register_address(&defaultColor);
    rb_gc_register_address(&defaultOutColor);

    rgss_cFont = rb_define_class("Font", rb_cObject);
    defaultColor = rgss_color_new(255, 255, 255, 255);
    defaultOutColor = rgss_color_new(0, 0, 0, 128);
    rb_define_alloc_func(rgss_cFont, font_alloc);
    rb_define_method(rgss_cFont, "initialize", font_initialize, -1);
    rb_define_method(rgss_cFont, "initialize_copy", font_init_copy, 1);
    rb_define_method(rgss_cFont, "name", font_get_name, 0);
    rb_define_method(rgss_cFont, "name=", font_put_name, 1);
    rb_define_method(rgss_cFont, "size", font_get_size, 0);
    rb_define_method(rgss_cFont, "size=", font_put_size, 1);
    rb_define_method(rgss_cFont, "bold", font_get_bold, 0);
    rb_define_method(rgss_cFont, "bold=", font_put_bold, 1);
    rb_define_method(rgss_cFont, "italic", font_get_italic, 0);
    rb_define_method(rgss_cFont, "italic=", font_put_italic, 1);
    rb_define_method(rgss_cFont, "color", font_get_color, 0);
    rb_define_method(rgss_cFont, "color=", font_put_color, 1);
    rb_define_singleton_method(rgss_cFont, "exist?", font_s_exist, 1);
    rb_define_singleton_method(rgss_cFont, "default_name", font_s_get_name, 0);
    rb_define_singleton_method(rgss_cFont, "default_name=", font_s_put_name, 1);
    rb_define_singleton_method(rgss_cFont, "default_size", font_s_get_size, 0);
    rb_define_singleton_method(rgss_cFont, "default_size=", font_s_put_size, 1);
    rb_define_singleton_method(rgss_cFont, "default_bold", font_s_get_bold, 0);
    rb_define_singleton_method(rgss_cFont, "default_bold=", font_s_put_bold, 1);
    rb_define_singleton_method(rgss_cFont, "default_italic", font_s_get_italic, 0);
    rb_define_singleton_method(rgss_cFont, "default_italic=", font_s_put_italic, 1);
    rb_define_singleton_method(rgss_cFont, "default_color", font_s_get_color, 0);
    rb_define_singleton_method(rgss_cFont, "default_color=", font_s_put_color, 1);
    if (rgss_version >= RGSS2) {
        rb_define_method(rgss_cFont, "shadow", font_get_shadow, 0);
        rb_define_method(rgss_cFont, "shadow=", font_put_shadow, 1);
        rb_define_singleton_method(rgss_cFont, "default_shadow", font_s_get_shadow, 0);
        rb_define_singleton_method(rgss_cFont, "default_shadow=", font_s_put_shadow, 1);
    }
    if (ace) {
        rb_define_method(rgss_cFont, "outline", font_get_outline, 0);
        rb_define_method(rgss_cFont, "outline=", font_put_outline, 1);
        rb_define_method(rgss_cFont, "out_color", font_get_out_color, 0);
        rb_define_method(rgss_cFont, "out_color=", font_put_out_color, 1);
        rb_define_singleton_method(rgss_cFont, "default_outline", font_s_get_outline, 0);
        rb_define_singleton_method(rgss_cFont, "default_outline=", font_s_put_outline, 1);
        rb_define_singleton_method(rgss_cFont, "default_out_color", font_s_get_out_color, 0);
        rb_define_singleton_method(rgss_cFont, "default_out_color=", font_s_put_out_color, 1);
    }
}

void rgss_define_bitmap(void) {
    rgss_cBitmap = rb_define_class("Bitmap", rb_cObject);
    rb_define_alloc_func(rgss_cBitmap, bitmap_alloc);
    rb_define_method(rgss_cBitmap, "initialize", bitmap_initialize, -1);
    rb_define_method(rgss_cBitmap, "initialize_copy", bitmap_init_copy, 1);
    rb_define_method(rgss_cBitmap, "dispose", bitmap_dispose, 0);
    rb_define_method(rgss_cBitmap, "disposed?", bitmap_disposed, 0);
    rb_define_method(rgss_cBitmap, "width", bitmap_width, 0);
    rb_define_method(rgss_cBitmap, "height", bitmap_height, 0);
    rb_define_method(rgss_cBitmap, "rect", bitmap_rect, 0);
    rb_define_method(rgss_cBitmap, "blt", bitmap_blt, -1);
    rb_define_method(rgss_cBitmap, "stretch_blt", bitmap_stretch_blt, -1);
    rb_define_method(rgss_cBitmap, "fill_rect", bitmap_fill_rect, -1);
    rb_define_method(rgss_cBitmap, "clear", bitmap_clear, 0);
    rb_define_method(rgss_cBitmap, "get_pixel", bitmap_get_pixel, 2);
    rb_define_method(rgss_cBitmap, "set_pixel", bitmap_set_pixel, 3);
    rb_define_method(rgss_cBitmap, "hue_change", bitmap_hue_change, 1);
    rb_define_method(rgss_cBitmap, "draw_text", bitmap_draw_text, -1);
    rb_define_method(rgss_cBitmap, "text_size", bitmap_text_size, 1);
    rb_define_method(rgss_cBitmap, "font", bitmap_get_font, 0);
    rb_define_method(rgss_cBitmap, "font=", bitmap_put_font, 1);
    if (rgss_version >= RGSS2) {
        rb_define_method(rgss_cBitmap, "gradient_fill_rect", bitmap_gradient_fill_rect, -1);
        rb_define_method(rgss_cBitmap, "clear_rect", bitmap_clear_rect, -1);
        rb_define_method(rgss_cBitmap, "blur", bitmap_blur, 0);
        rb_define_method(rgss_cBitmap, "radial_blur", bitmap_radial_blur, 2);
    }
}

// Color, Tone, Rect and Table: small value classes, with the _dump formats RGSS's data files use (four doubles for
// Color and Tone, four int32 for Rect, five int32 and int16 cells for Table).
#include "binding.h"

#include <math.h>
#include <string.h>

VALUE rgss_cColor, rgss_cTone, rgss_cRect, rgss_cTable;

static double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

// MARK: Color

static const rb_data_type_t color_type = {"Color", {NULL, RUBY_TYPED_DEFAULT_FREE, NULL}, NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY};

static VALUE color_alloc(VALUE klass) {
    RbColor *c;
    return TypedData_Make_Struct(klass, RbColor, &color_type, c);
}

RbColor *rgss_color(VALUE v) { return rb_check_typeddata(v, &color_type); }

static void color_set4(RbColor *c, double r, double g, double b, double a) {
    c->c.r = clampd(r, 0, 255);
    c->c.g = clampd(g, 0, 255);
    c->c.b = clampd(b, 0, 255);
    c->c.a = clampd(a, 0, 255);
}

static VALUE color_set(int argc, VALUE *argv, VALUE self) {
    RbColor *c = rgss_color(self);
    if (argc == 1) {
        c->c = rgss_color(argv[0])->c;
        return self;
    }
    rb_check_arity(argc, 3, 4);
    color_set4(c, NUM2DBL(argv[0]), NUM2DBL(argv[1]), NUM2DBL(argv[2]), argc > 3 ? NUM2DBL(argv[3]) : 255);
    return self;
}

static VALUE color_initialize(int argc, VALUE *argv, VALUE self) {
    if (argc == 0) {
        color_set4(rgss_color(self), 0, 0, 0, 0);
        return self;
    }
    return color_set(argc, argv, self);
}

static VALUE color_init_copy(VALUE self, VALUE other) {
    rgss_color(self)->c = rgss_color(other)->c;
    return self;
}

VALUE rgss_color_new(double r, double g, double b, double a) {
    VALUE v = color_alloc(rgss_cColor);
    color_set4(rgss_color(v), r, g, b, a);
    return v;
}

#define COLOR_FIELD(name, field)                                                                                       \
    static VALUE color_get_##name(VALUE self) { return DBL2NUM(rgss_color(self)->c.field); }                          \
    static VALUE color_put_##name(VALUE self, VALUE v) {                                                               \
        rgss_color(self)->c.field = clampd(NUM2DBL(v), 0, 255);                                                        \
        return v;                                                                                                      \
    }
COLOR_FIELD(red, r)
COLOR_FIELD(green, g)
COLOR_FIELD(blue, b)
COLOR_FIELD(alpha, a)

static VALUE color_eq(VALUE self, VALUE other) {
    if (!rb_typeddata_is_kind_of(other, &color_type)) return Qfalse;
    ColorF a = rgss_color(self)->c, b = rgss_color(other)->c;
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a ? Qtrue : Qfalse;
}

static VALUE color_to_s(VALUE self) {
    ColorF c = rgss_color(self)->c;
    return rb_sprintf("(%f, %f, %f, %f)", c.r, c.g, c.b, c.a);
}

static VALUE dump_doubles(double a, double b, double c, double d) {
    double values[4] = {a, b, c, d};
    // Little-endian doubles, as on the x86 machines RGSS ran on.
    return rb_str_new((const char *)values, sizeof values);
}

static void load_doubles(VALUE s, double out[4]) {
    StringValue(s);
    if (RSTRING_LEN(s) < 32) rb_raise(rb_eArgError, "marshal data too short");
    memcpy(out, RSTRING_PTR(s), 32);
}

static VALUE color_dump(VALUE self, VALUE depth) {
    (void)depth;
    ColorF c = rgss_color(self)->c;
    return dump_doubles(c.r, c.g, c.b, c.a);
}

static VALUE color_load(VALUE klass, VALUE s) {
    double v[4];
    load_doubles(s, v);
    VALUE obj = color_alloc(klass);
    color_set4(rgss_color(obj), v[0], v[1], v[2], v[3]);
    return obj;
}

// MARK: Tone

static const rb_data_type_t tone_type = {"Tone", {NULL, RUBY_TYPED_DEFAULT_FREE, NULL}, NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY};

static VALUE tone_alloc(VALUE klass) {
    RbTone *t;
    return TypedData_Make_Struct(klass, RbTone, &tone_type, t);
}

RbTone *rgss_tone(VALUE v) { return rb_check_typeddata(v, &tone_type); }

static void tone_set4(RbTone *t, double r, double g, double b, double gray) {
    t->t.r = clampd(r, -255, 255);
    t->t.g = clampd(g, -255, 255);
    t->t.b = clampd(b, -255, 255);
    t->t.gray = clampd(gray, 0, 255);
}

static VALUE tone_set(int argc, VALUE *argv, VALUE self) {
    RbTone *t = rgss_tone(self);
    if (argc == 1) {
        t->t = rgss_tone(argv[0])->t;
        return self;
    }
    rb_check_arity(argc, 3, 4);
    tone_set4(t, NUM2DBL(argv[0]), NUM2DBL(argv[1]), NUM2DBL(argv[2]), argc > 3 ? NUM2DBL(argv[3]) : 0);
    return self;
}

static VALUE tone_initialize(int argc, VALUE *argv, VALUE self) {
    if (argc == 0) {
        tone_set4(rgss_tone(self), 0, 0, 0, 0);
        return self;
    }
    return tone_set(argc, argv, self);
}

static VALUE tone_init_copy(VALUE self, VALUE other) {
    rgss_tone(self)->t = rgss_tone(other)->t;
    return self;
}

VALUE rgss_tone_new(double r, double g, double b, double gray) {
    VALUE v = tone_alloc(rgss_cTone);
    tone_set4(rgss_tone(v), r, g, b, gray);
    return v;
}

#define TONE_FIELD(name, field, lo)                                                                                    \
    static VALUE tone_get_##name(VALUE self) { return DBL2NUM(rgss_tone(self)->t.field); }                            \
    static VALUE tone_put_##name(VALUE self, VALUE v) {                                                                \
        rgss_tone(self)->t.field = clampd(NUM2DBL(v), lo, 255);                                                        \
        return v;                                                                                                      \
    }
TONE_FIELD(red, r, -255)
TONE_FIELD(green, g, -255)
TONE_FIELD(blue, b, -255)
TONE_FIELD(gray, gray, 0)

static VALUE tone_eq(VALUE self, VALUE other) {
    if (!rb_typeddata_is_kind_of(other, &tone_type)) return Qfalse;
    ToneF a = rgss_tone(self)->t, b = rgss_tone(other)->t;
    return a.r == b.r && a.g == b.g && a.b == b.b && a.gray == b.gray ? Qtrue : Qfalse;
}

static VALUE tone_to_s(VALUE self) {
    ToneF t = rgss_tone(self)->t;
    return rb_sprintf("(%f, %f, %f, %f)", t.r, t.g, t.b, t.gray);
}

static VALUE tone_dump(VALUE self, VALUE depth) {
    (void)depth;
    ToneF t = rgss_tone(self)->t;
    return dump_doubles(t.r, t.g, t.b, t.gray);
}

static VALUE tone_load(VALUE klass, VALUE s) {
    double v[4];
    load_doubles(s, v);
    VALUE obj = tone_alloc(klass);
    tone_set4(rgss_tone(obj), v[0], v[1], v[2], v[3]);
    return obj;
}

// MARK: Rect

static const rb_data_type_t rect_type = {"Rect", {NULL, RUBY_TYPED_DEFAULT_FREE, NULL}, NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY};

static VALUE rect_alloc(VALUE klass) {
    RbRect *r;
    return TypedData_Make_Struct(klass, RbRect, &rect_type, r);
}

RbRect *rgss_rect(VALUE v) { return rb_check_typeddata(v, &rect_type); }

static VALUE rect_set(int argc, VALUE *argv, VALUE self) {
    RbRect *r = rgss_rect(self);
    if (argc == 1) {
        r->r = rgss_rect(argv[0])->r;
        return self;
    }
    rb_check_arity(argc, 4, 4);
    r->r = (IRect){NUM2INT(argv[0]), NUM2INT(argv[1]), NUM2INT(argv[2]), NUM2INT(argv[3])};
    return self;
}

static VALUE rect_initialize(int argc, VALUE *argv, VALUE self) {
    if (argc == 0) return self;
    return rect_set(argc, argv, self);
}

static VALUE rect_init_copy(VALUE self, VALUE other) {
    rgss_rect(self)->r = rgss_rect(other)->r;
    return self;
}

VALUE rgss_rect_new(int x, int y, int w, int h) {
    VALUE v = rect_alloc(rgss_cRect);
    rgss_rect(v)->r = (IRect){x, y, w, h};
    return v;
}

#define RECT_FIELD(name, field)                                                                                        \
    static VALUE rect_get_##name(VALUE self) { return INT2NUM(rgss_rect(self)->r.field); }                            \
    static VALUE rect_put_##name(VALUE self, VALUE v) {                                                                \
        rgss_rect(self)->r.field = NUM2INT(v);                                                                         \
        return v;                                                                                                      \
    }
RECT_FIELD(x, x)
RECT_FIELD(y, y)
RECT_FIELD(width, w)
RECT_FIELD(height, h)

static VALUE rect_empty(VALUE self) {
    rgss_rect(self)->r = (IRect){0, 0, 0, 0};
    return self;
}

static VALUE rect_eq(VALUE self, VALUE other) {
    if (!rb_typeddata_is_kind_of(other, &rect_type)) return Qfalse;
    IRect a = rgss_rect(self)->r, b = rgss_rect(other)->r;
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h ? Qtrue : Qfalse;
}

static VALUE rect_to_s(VALUE self) {
    IRect r = rgss_rect(self)->r;
    return rb_sprintf("(%d, %d, %d, %d)", r.x, r.y, r.w, r.h);
}

static VALUE rect_dump(VALUE self, VALUE depth) {
    (void)depth;
    IRect r = rgss_rect(self)->r;
    int32_t values[4] = {r.x, r.y, r.w, r.h};
    return rb_str_new((const char *)values, sizeof values);
}

static VALUE rect_load(VALUE klass, VALUE s) {
    StringValue(s);
    if (RSTRING_LEN(s) < 16) rb_raise(rb_eArgError, "marshal data too short");
    int32_t v[4];
    memcpy(v, RSTRING_PTR(s), 16);
    VALUE obj = rect_alloc(klass);
    rgss_rect(obj)->r = (IRect){v[0], v[1], v[2], v[3]};
    return obj;
}

// MARK: Table

static void table_free(void *p) {
    Table *t = p;
    ruby_xfree(t->data);
    ruby_xfree(t);
}

static size_t table_size(const void *p) {
    const Table *t = p;
    return sizeof(Table) + (size_t)t->xs * t->ys * t->zs * sizeof(int16_t);
}

static const rb_data_type_t table_type = {"Table", {NULL, table_free, table_size}, NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY};

static VALUE table_alloc(VALUE klass) {
    Table *t;
    VALUE v = TypedData_Make_Struct(klass, Table, &table_type, t);
    t->dims = 1;
    t->xs = t->ys = t->zs = 0;
    return v;
}

Table *rgss_table(VALUE v) { return rb_check_typeddata(v, &table_type); }

// Keeps the cells that still fit, as Table#resize does.
static void table_resize_to(Table *t, int dims, int xs, int ys, int zs) {
    if (xs < 0) xs = 0;
    if (ys < 1) ys = 1;
    if (zs < 1) zs = 1;
    size_t count = (size_t)xs * ys * zs;
    int16_t *data = ruby_xcalloc(count ? count : 1, sizeof(int16_t));
    for (int z = 0; z < zs && z < t->zs; z++)
        for (int y = 0; y < ys && y < t->ys; y++)
            for (int x = 0; x < xs && x < t->xs; x++)
                data[((size_t)z * ys + y) * xs + x] = t->data[((size_t)z * t->ys + y) * t->xs + x];
    ruby_xfree(t->data);
    t->data = data;
    t->dims = dims;
    t->xs = xs;
    t->ys = ys;
    t->zs = zs;
}

static VALUE table_resize(int argc, VALUE *argv, VALUE self) {
    rb_check_arity(argc, 1, 3);
    table_resize_to(rgss_table(self), argc, NUM2INT(argv[0]), argc > 1 ? NUM2INT(argv[1]) : 1,
                    argc > 2 ? NUM2INT(argv[2]) : 1);
    return self;
}

static VALUE table_initialize(int argc, VALUE *argv, VALUE self) { return table_resize(argc, argv, self); }

static VALUE table_init_copy(VALUE self, VALUE other) {
    Table *t = rgss_table(self), *o = rgss_table(other);
    size_t count = (size_t)o->xs * o->ys * o->zs;
    ruby_xfree(t->data);
    t->data = ruby_xcalloc(count ? count : 1, sizeof(int16_t));
    memcpy(t->data, o->data, count * sizeof(int16_t));
    t->dims = o->dims;
    t->xs = o->xs;
    t->ys = o->ys;
    t->zs = o->zs;
    return self;
}

static VALUE table_xsize(VALUE self) { return INT2NUM(rgss_table(self)->xs); }
static VALUE table_ysize(VALUE self) { return INT2NUM(rgss_table(self)->ys); }
static VALUE table_zsize(VALUE self) { return INT2NUM(rgss_table(self)->zs); }

static bool table_index(Table *t, int argc, VALUE *argv, size_t *index) {
    int x = NUM2INT(argv[0]), y = argc > 1 ? NUM2INT(argv[1]) : 0, z = argc > 2 ? NUM2INT(argv[2]) : 0;
    if (x < 0 || y < 0 || z < 0 || x >= t->xs || y >= t->ys || z >= t->zs) return false;
    *index = ((size_t)z * t->ys + y) * t->xs + x;
    return true;
}

static VALUE table_get(int argc, VALUE *argv, VALUE self) {
    rb_check_arity(argc, 1, 3);
    Table *t = rgss_table(self);
    size_t index;
    return table_index(t, argc, argv, &index) ? INT2FIX(t->data[index]) : Qnil;
}

static VALUE table_put(int argc, VALUE *argv, VALUE self) {
    rb_check_arity(argc, 2, 4);
    Table *t = rgss_table(self);
    size_t index;
    VALUE value = argv[argc - 1];
    if (table_index(t, argc - 1, argv, &index)) t->data[index] = (int16_t)NUM2INT(value);
    return value;
}

static VALUE table_dump(VALUE self, VALUE depth) {
    (void)depth;
    Table *t = rgss_table(self);
    size_t count = (size_t)t->xs * t->ys * t->zs;
    VALUE s = rb_str_new(NULL, (long)(20 + count * 2));
    int32_t header[5] = {t->dims, t->xs, t->ys, t->zs, (int32_t)count};
    memcpy(RSTRING_PTR(s), header, 20);
    memcpy(RSTRING_PTR(s) + 20, t->data, count * 2);
    return s;
}

static VALUE table_load(VALUE klass, VALUE s) {
    StringValue(s);
    if (RSTRING_LEN(s) < 20) rb_raise(rb_eArgError, "marshal data too short");
    int32_t header[5];
    memcpy(header, RSTRING_PTR(s), 20);
    VALUE obj = table_alloc(klass);
    Table *t = rgss_table(obj);
    table_resize_to(t, header[0], header[1], header[2], header[3]);
    size_t count = (size_t)t->xs * t->ys * t->zs;
    size_t available = (size_t)(RSTRING_LEN(s) - 20) / 2;
    memcpy(t->data, RSTRING_PTR(s) + 20, (count < available ? count : available) * 2);
    return obj;
}

// MARK: Definitions

void rgss_define_values(void) {
    rgss_cColor = rb_define_class("Color", rb_cObject);
    rb_define_alloc_func(rgss_cColor, color_alloc);
    rb_define_method(rgss_cColor, "initialize", color_initialize, -1);
    rb_define_method(rgss_cColor, "initialize_copy", color_init_copy, 1);
    rb_define_method(rgss_cColor, "set", color_set, -1);
    rb_define_method(rgss_cColor, "red", color_get_red, 0);
    rb_define_method(rgss_cColor, "red=", color_put_red, 1);
    rb_define_method(rgss_cColor, "green", color_get_green, 0);
    rb_define_method(rgss_cColor, "green=", color_put_green, 1);
    rb_define_method(rgss_cColor, "blue", color_get_blue, 0);
    rb_define_method(rgss_cColor, "blue=", color_put_blue, 1);
    rb_define_method(rgss_cColor, "alpha", color_get_alpha, 0);
    rb_define_method(rgss_cColor, "alpha=", color_put_alpha, 1);
    rb_define_method(rgss_cColor, "==", color_eq, 1);
    rb_define_method(rgss_cColor, "to_s", color_to_s, 0);
    rb_define_method(rgss_cColor, "_dump", color_dump, 1);
    rb_define_singleton_method(rgss_cColor, "_load", color_load, 1);

    rgss_cTone = rb_define_class("Tone", rb_cObject);
    rb_define_alloc_func(rgss_cTone, tone_alloc);
    rb_define_method(rgss_cTone, "initialize", tone_initialize, -1);
    rb_define_method(rgss_cTone, "initialize_copy", tone_init_copy, 1);
    rb_define_method(rgss_cTone, "set", tone_set, -1);
    rb_define_method(rgss_cTone, "red", tone_get_red, 0);
    rb_define_method(rgss_cTone, "red=", tone_put_red, 1);
    rb_define_method(rgss_cTone, "green", tone_get_green, 0);
    rb_define_method(rgss_cTone, "green=", tone_put_green, 1);
    rb_define_method(rgss_cTone, "blue", tone_get_blue, 0);
    rb_define_method(rgss_cTone, "blue=", tone_put_blue, 1);
    rb_define_method(rgss_cTone, "gray", tone_get_gray, 0);
    rb_define_method(rgss_cTone, "gray=", tone_put_gray, 1);
    rb_define_method(rgss_cTone, "==", tone_eq, 1);
    rb_define_method(rgss_cTone, "to_s", tone_to_s, 0);
    rb_define_method(rgss_cTone, "_dump", tone_dump, 1);
    rb_define_singleton_method(rgss_cTone, "_load", tone_load, 1);

    rgss_cRect = rb_define_class("Rect", rb_cObject);
    rb_define_alloc_func(rgss_cRect, rect_alloc);
    rb_define_method(rgss_cRect, "initialize", rect_initialize, -1);
    rb_define_method(rgss_cRect, "initialize_copy", rect_init_copy, 1);
    rb_define_method(rgss_cRect, "set", rect_set, -1);
    rb_define_method(rgss_cRect, "x", rect_get_x, 0);
    rb_define_method(rgss_cRect, "x=", rect_put_x, 1);
    rb_define_method(rgss_cRect, "y", rect_get_y, 0);
    rb_define_method(rgss_cRect, "y=", rect_put_y, 1);
    rb_define_method(rgss_cRect, "width", rect_get_width, 0);
    rb_define_method(rgss_cRect, "width=", rect_put_width, 1);
    rb_define_method(rgss_cRect, "height", rect_get_height, 0);
    rb_define_method(rgss_cRect, "height=", rect_put_height, 1);
    rb_define_method(rgss_cRect, "empty", rect_empty, 0);
    rb_define_method(rgss_cRect, "==", rect_eq, 1);
    rb_define_method(rgss_cRect, "to_s", rect_to_s, 0);
    rb_define_method(rgss_cRect, "_dump", rect_dump, 1);
    rb_define_singleton_method(rgss_cRect, "_load", rect_load, 1);

    rgss_cTable = rb_define_class("Table", rb_cObject);
    rb_define_alloc_func(rgss_cTable, table_alloc);
    rb_define_method(rgss_cTable, "initialize", table_initialize, -1);
    rb_define_method(rgss_cTable, "initialize_copy", table_init_copy, 1);
    rb_define_method(rgss_cTable, "resize", table_resize, -1);
    rb_define_method(rgss_cTable, "xsize", table_xsize, 0);
    rb_define_method(rgss_cTable, "ysize", table_ysize, 0);
    rb_define_method(rgss_cTable, "zsize", table_zsize, 0);
    rb_define_method(rgss_cTable, "[]", table_get, -1);
    rb_define_method(rgss_cTable, "[]=", table_put, -1);
    rb_define_method(rgss_cTable, "_dump", table_dump, 1);
    rb_define_singleton_method(rgss_cTable, "_load", table_load, 1);
}

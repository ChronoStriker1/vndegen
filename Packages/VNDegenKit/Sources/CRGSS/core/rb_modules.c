// Graphics, Input and Audio, and RGSS's global functions: load_data, save_data, rgss_main, rgss_stop, msgbox.
#include "binding.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "host.h"

VALUE rgss_eRGSSError, rgss_eReset;
static VALUE stopTag;
int rgss_frame_rate = 60;
long rgss_frame_count;

void rgss_raise_disposed(const char *what) { rb_raise(rgss_eRGSSError, "disposed %s", what); }

// Unwinds the game when the host stops it. A throw passes `rescue Exception`, which games use, and runs `ensure`.
static void stop_game(void) { rb_throw_obj(stopTag, Qnil); }

VALUE rgss_stop_tag(void) { return stopTag; }

// MARK: Files

VALUE rgss_read_file(VALUE path, const char *const *extensions) {
    const char *name = StringValueCStr(path);
    uint8_t *data;
    size_t size;
    if (!rgss_fs_read(name, extensions, &data, &size, NULL)) {
        rb_raise(rb_const_get(rb_mErrno, rb_intern("ENOENT")), "No such file or directory - %s", name);
    }
    VALUE s = rb_str_new((const char *)data, (long)size);
    free(data);
    return s;
}

static VALUE f_load_data(VALUE self, VALUE path) {
    (void)self;
    VALUE data = rgss_read_file(path, NULL);
    // XP's data was dumped by Ruby 1.8, whose strings carry no encoding; rgss.rb tags them as UTF-8 as they load.
    return rb_funcall(rb_mKernel, rb_intern("rgss_marshal_load"), 1, data);
}

static VALUE f_save_data(VALUE self, VALUE obj, VALUE path) {
    (void)self;
    char out[2048];
    rgss_fs_write_path(StringValueCStr(path), out, sizeof out);
    VALUE file = rb_funcall(rb_cFile, rb_intern("open"), 2, rb_str_new_cstr(out), rb_str_new_cstr("wb"));
    rb_funcall(rb_const_get(rb_cObject, rb_intern("Marshal")), rb_intern("dump"), 2, obj, file);
    rb_funcall(file, rb_intern("close"), 0);
    return Qnil;
}

// MARK: Graphics

static Image screen, frozen, shown;
static bool isFrozen;
static int brightness = 255;

static void ensure_screen(void) {
    if (screen.w == rgss_screen_w && screen.h == rgss_screen_h && screen.px) return;
    image_free(&screen);
    image_free(&shown);
    image_alloc(&screen, rgss_screen_w, rgss_screen_h);
    image_alloc(&shown, rgss_screen_w, rgss_screen_h);
}

// Hands `frame` to the host at the current brightness and waits for the next frame.
static void present(const Image *frame) {
    const Image *out = frame;
    if (brightness < 255) {
        size_t n = (size_t)frame->w * frame->h;
        for (size_t i = 0; i < n; i++) {
            Pixel p = frame->px[i];
            shown.px[i] = px_make(px_r(p) * brightness / 255, px_g(p) * brightness / 255, px_b(p) * brightness / 255, 255);
        }
        out = &shown;
    }
    rgss_frame_count++;
    if (!host_present(out)) stop_game();
}

static void update_frame(void) {
    char *code = host_take_eval();
    if (code) {
        int state = 0;
        rb_eval_string_protect(code, &state);
        if (state) {
            VALUE message = rb_funcall(rb_errinfo(), rb_intern("full_message"), 0);
            rgss_log("eval failed: %s", StringValueCStr(message));
            rb_set_errinfo(Qnil);
        }
        free(code);
    }
    ensure_screen();
    if (isFrozen) {
        present(&frozen);
        return;
    }
    scene_render(&screen);
    present(&screen);
}

static VALUE graphics_update(VALUE self) {
    (void)self;
    update_frame();
    return Qnil;
}

static VALUE graphics_wait(VALUE self, VALUE duration) {
    (void)self;
    for (int i = NUM2INT(duration); i > 0; i--) update_frame();
    return Qnil;
}

static VALUE graphics_fadeout(VALUE self, VALUE duration) {
    (void)self;
    int d = NUM2INT(duration), start = brightness;
    for (int i = 1; i <= d; i++) {
        brightness = start * (d - i) / d;
        update_frame();
    }
    brightness = 0;
    return Qnil;
}

static VALUE graphics_fadein(VALUE self, VALUE duration) {
    (void)self;
    int d = NUM2INT(duration), start = brightness;
    for (int i = 1; i <= d; i++) {
        brightness = start + (255 - start) * i / d;
        update_frame();
    }
    brightness = 255;
    return Qnil;
}

static VALUE graphics_freeze(VALUE self) {
    (void)self;
    ensure_screen();
    image_free(&frozen);
    image_alloc(&frozen, screen.w, screen.h);
    scene_render(&frozen);
    isFrozen = true;
    return Qnil;
}

// Blends from the frozen screen to the new one over `duration` frames. A transition graphic gives each pixel its own
// moment: dark pixels change first, and `vague` softens the edge, as the help's Graphics.transition describes.
static VALUE graphics_transition(int argc, VALUE *argv, VALUE self) {
    (void)self;
    rb_check_arity(argc, 0, 3);
    int duration = argc > 0 ? NUM2INT(argv[0]) : (rgss_version == RGSS3 ? 10 : 8);
    VALUE file = argc > 1 ? argv[1] : Qnil;
    int vague = argc > 2 ? NUM2INT(argv[2]) : 40;
    ensure_screen();
    if (!isFrozen) return Qnil;
    scene_render(&screen);
    Image mask = {0};
    if (!NIL_P(file) && RSTRING_LEN(StringValue(file)) > 0) {
        uint8_t *data;
        size_t size;
        if (rgss_fs_read(StringValueCStr(file), rgss_image_extensions, &data, &size, NULL)) {
            image_decode(data, size, &mask);
            free(data);
        }
    }
    if (vague < 1) vague = 1;
    Image blend;
    image_alloc(&blend, screen.w, screen.h);
    for (int i = 1; i <= duration; i++) {
        size_t n = (size_t)screen.w * screen.h;
        int level = (256 + vague) * i / duration - vague; // pixels whose mask value is below this have changed
        for (size_t k = 0; k < n; k++) {
            int a;
            if (mask.px) {
                int mx = (int)(k % screen.w) * mask.w / screen.w, my = (int)(k / screen.w) * mask.h / screen.h;
                int m = px_r(mask.px[(size_t)my * mask.w + mx]);
                a = (level - m) * 255 / vague;
                a = clamp255(a);
            } else {
                a = 255 * i / duration;
            }
            Pixel f = frozen.px[k], c = screen.px[k];
            blend.px[k] = px_make(px_r(f) + (px_r(c) - px_r(f)) * a / 255, px_g(f) + (px_g(c) - px_g(f)) * a / 255,
                                  px_b(f) + (px_b(c) - px_b(f)) * a / 255, 255);
        }
        present(&blend);
    }
    image_free(&blend);
    image_free(&mask);
    isFrozen = false;
    return Qnil;
}

static VALUE graphics_snap_to_bitmap(VALUE self) {
    (void)self;
    ensure_screen();
    Image snap;
    image_alloc(&snap, screen.w, screen.h);
    scene_render(&snap);
    return rgss_bitmap_wrap_image(snap);
}

static VALUE graphics_frame_reset(VALUE self) {
    (void)self;
    host_frame_reset();
    return Qnil;
}

static VALUE graphics_width(VALUE self) { (void)self; return INT2NUM(rgss_screen_w); }
static VALUE graphics_height(VALUE self) { (void)self; return INT2NUM(rgss_screen_h); }

static VALUE graphics_resize_screen(VALUE self, VALUE w, VALUE h) {
    (void)self;
    int width = NUM2INT(w), height = NUM2INT(h);
    // RGSS allows up to 640x480; games built on patched DLLs ask for more, which costs nothing here.
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    if (width > 4096) width = 4096;
    if (height > 4096) height = 4096;
    rgss_screen_w = width;
    rgss_screen_h = height;
    ensure_screen();
    return Qnil;
}

static VALUE graphics_get_frame_rate(VALUE self) { (void)self; return INT2NUM(rgss_frame_rate); }
static VALUE graphics_put_frame_rate(VALUE self, VALUE v) {
    (void)self;
    int rate = NUM2INT(v);
    rgss_frame_rate = rate < 10 ? 10 : rate > 120 ? 120 : rate;
    host_set_frame_rate(rgss_frame_rate);
    return v;
}
static VALUE graphics_get_frame_count(VALUE self) { (void)self; return LONG2NUM(rgss_frame_count); }
static VALUE graphics_put_frame_count(VALUE self, VALUE v) { (void)self; rgss_frame_count = NUM2LONG(v); return v; }
static VALUE graphics_get_brightness(VALUE self) { (void)self; return INT2NUM(brightness); }
static VALUE graphics_put_brightness(VALUE self, VALUE v) { (void)self; brightness = clamp255(NUM2INT(v)); return v; }

static VALUE graphics_play_movie(VALUE self, VALUE name) {
    (void)self;
    rgss_log("Graphics.play_movie(%s): movies are not supported", StringValueCStr(name));
    return Qnil;
}

// MARK: Input

// RGSS's buttons, by their RGSS1 and RGSS2 numbers. VX Ace names them with symbols of the same names.
enum { B_DOWN = 2, B_LEFT = 4, B_RIGHT = 6, B_UP = 8, B_A = 11, B_B, B_C, B_X, B_Y, B_Z, B_L, B_R, B_SHIFT = 21,
       B_CTRL, B_ALT, B_F5 = 25, B_F6, B_F7, B_F8, B_F9, BUTTON_COUNT = 32 };

// RPG Maker's default keys for each button, from the help's "Playing a Game" table, by libretro key code.
static const struct { int button; unsigned keys[4]; } bindings[] = {
    {B_DOWN, {274, 258}}, {B_LEFT, {276, 260}}, {B_RIGHT, {275, 262}}, {B_UP, {273, 264}},
    {B_A, {304, 303}}, {B_B, {27, 'x', 256}}, {B_C, {' ', 13, 'z', 271}}, {B_X, {'a'}}, {B_Y, {'s'}}, {B_Z, {'d'}},
    {B_L, {'q', 280}}, {B_R, {'w', 281}}, {B_SHIFT, {304, 303}}, {B_CTRL, {306, 305}}, {B_ALT, {308, 307}},
    {B_F5, {286}}, {B_F6, {287}}, {B_F7, {288}}, {B_F8, {289}}, {B_F9, {290}},
};

static HostInput input;
static int held[BUTTON_COUNT]; // frames each button has been down, 0 when up
static int lastDirection;      // the direction pressed most recently, for dir4
static long directionOrder[10];
static long inputTick;

HostInput *rgss_input_state(void) { return &input; }

static VALUE input_update(VALUE self) {
    (void)self;
    host_input(&input);
    inputTick++;
    for (size_t i = 0; i < sizeof bindings / sizeof bindings[0]; i++) {
        bool down = false;
        for (int k = 0; k < 4 && bindings[i].keys[k]; k++) down |= input.keys[bindings[i].keys[k]];
        int b = bindings[i].button;
        held[b] = down ? held[b] + 1 : 0;
        if (b <= B_UP && held[b] == 1) directionOrder[b] = inputTick;
    }
    (void)lastDirection;
    return Qnil;
}

static int button_of(VALUE v) {
    if (SYMBOL_P(v)) {
        static const char *names[] = {"DOWN", "LEFT", "RIGHT", "UP", "A", "B", "C", "X", "Y", "Z", "L", "R", "SHIFT",
                                      "CTRL", "ALT", "F5", "F6", "F7", "F8", "F9"};
        static const int numbers[] = {B_DOWN, B_LEFT, B_RIGHT, B_UP, B_A, B_B, B_C, B_X, B_Y, B_Z, B_L, B_R, B_SHIFT,
                                      B_CTRL, B_ALT, B_F5, B_F6, B_F7, B_F8, B_F9};
        const char *name = rb_id2name(SYM2ID(v));
        for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) if (!strcmp(names[i], name)) return numbers[i];
        return 0;
    }
    if (!RB_INTEGER_TYPE_P(v)) return 0;
    int n = NUM2INT(v);
    return n > 0 && n < BUTTON_COUNT ? n : 0;
}

static VALUE input_press(VALUE self, VALUE b) {
    (void)self;
    int n = button_of(b);
    return n && held[n] > 0 ? Qtrue : Qfalse;
}

static VALUE input_trigger(VALUE self, VALUE b) {
    (void)self;
    int n = button_of(b);
    return n && held[n] == 1 ? Qtrue : Qfalse;
}

// A held button repeats after a delay, then at a steady rate, as RGSS's menus scroll.
static VALUE input_repeat(VALUE self, VALUE b) {
    (void)self;
    int n = button_of(b);
    if (!n || held[n] == 0) return Qfalse;
    int h = held[n];
    int delay = rgss_version == RGSS1 ? 16 : 24, interval = rgss_version == RGSS1 ? 4 : 6;
    return h == 1 || (h > delay && (h - delay - 1) % interval == 0) ? Qtrue : Qfalse;
}

static VALUE input_dir4(VALUE self) {
    (void)self;
    int best = 0;
    long when = -1;
    for (int d = 2; d <= 8; d += 2) {
        if (held[d] > 0 && directionOrder[d] > when) {
            best = d;
            when = directionOrder[d];
        }
    }
    return INT2FIX(best);
}

static VALUE input_dir8(VALUE self) {
    (void)self;
    bool up = held[B_UP] > 0, down = held[B_DOWN] > 0, left = held[B_LEFT] > 0, right = held[B_RIGHT] > 0;
    if (up && down) up = down = false;
    if (left && right) left = right = false;
    int d = 5;
    if (up) d += 3;
    if (down) d -= 3;
    if (left) d -= 1;
    if (right) d += 1;
    return INT2FIX(d == 5 ? 0 : d);
}

// The raw key and mouse state from the last Input.update, for Win32API's GetAsyncKeyState.
static VALUE input_key_down(VALUE self, VALUE code) {
    (void)self;
    int c = NUM2INT(code);
    return c >= 0 && c < HOST_KEY_COUNT && input.keys[c] ? Qtrue : Qfalse;
}

static VALUE input_mouse_down(VALUE self, VALUE button) {
    (void)self;
    int b = NUM2INT(button);
    return b >= 0 && b < 3 && input.mouse[b] ? Qtrue : Qfalse;
}

// MARK: Audio

static void play_track(AudioTrack track, int argc, VALUE *argv, bool positional) {
    rb_check_arity(argc, 1, positional ? 4 : 3);
    VALUE name = argv[0];
    int volume = argc > 1 ? NUM2INT(argv[1]) : 100;
    int pitch = argc > 2 ? NUM2INT(argv[2]) : 100;
    double position = positional && argc > 3 ? NUM2DBL(argv[3]) : 0;
    const char *path = StringValueCStr(name);
    if (!*path) {
        audio_stop_track(track);
        return;
    }
    uint8_t *data;
    size_t size;
    if (!rgss_fs_read(path, rgss_audio_extensions, &data, &size, NULL)) {
        rgss_log("audio: no file %s", path);
        return;
    }
    audio_play(track, data, size, path, volume, pitch, position);
}

static VALUE audio_bgm_play(int argc, VALUE *argv, VALUE self) { (void)self; play_track(TRACK_BGM, argc, argv, rgss_version == RGSS3); return Qnil; }
static VALUE audio_bgs_play(int argc, VALUE *argv, VALUE self) { (void)self; play_track(TRACK_BGS, argc, argv, rgss_version == RGSS3); return Qnil; }
static VALUE audio_me_play(int argc, VALUE *argv, VALUE self) { (void)self; play_track(TRACK_ME, argc, argv, false); return Qnil; }
static VALUE audio_bgm_stop(VALUE self) { (void)self; audio_stop_track(TRACK_BGM); return Qnil; }
static VALUE audio_bgs_stop(VALUE self) { (void)self; audio_stop_track(TRACK_BGS); return Qnil; }
static VALUE audio_me_stop(VALUE self) { (void)self; audio_stop_track(TRACK_ME); return Qnil; }
static VALUE audio_bgm_fade(VALUE self, VALUE ms) { (void)self; audio_fade_track(TRACK_BGM, NUM2INT(ms)); return Qnil; }
static VALUE audio_bgs_fade(VALUE self, VALUE ms) { (void)self; audio_fade_track(TRACK_BGS, NUM2INT(ms)); return Qnil; }
static VALUE audio_me_fade(VALUE self, VALUE ms) { (void)self; audio_fade_track(TRACK_ME, NUM2INT(ms)); return Qnil; }
static VALUE audio_bgm_pos(VALUE self) { (void)self; return LONG2NUM(audio_position(TRACK_BGM)); }
static VALUE audio_bgs_pos(VALUE self) { (void)self; return LONG2NUM(audio_position(TRACK_BGS)); }
static VALUE audio_setup_midi(VALUE self) { (void)self; return Qnil; }

static VALUE audio_se_play(int argc, VALUE *argv, VALUE self) {
    (void)self;
    rb_check_arity(argc, 1, 3);
    const char *path = StringValueCStr(argv[0]);
    if (!*path) return Qnil;
    uint8_t *data;
    size_t size;
    if (!rgss_fs_read(path, rgss_audio_extensions, &data, &size, NULL)) {
        rgss_log("audio: no file %s", path);
        return Qnil;
    }
    audio_play_se(data, size, path, argc > 1 ? NUM2INT(argv[1]) : 80, argc > 2 ? NUM2INT(argv[2]) : 100);
    return Qnil;
}

static VALUE audio_se_stop(VALUE self) { (void)self; audio_stop_se(); return Qnil; }

// MARK: Global functions

static VALUE f_rgss_log(VALUE self, VALUE text) {
    (void)self;
    rgss_log("%s", StringValueCStr(text));
    return Qnil;
}

// Where a relative path the game uses is on disk. On PC the game's folder is the working directory; here reads find the
// file in the save folder, then the game folder, whatever its case, and writes go to the save folder.
static VALUE f_rgss_game_path(VALUE self, VALUE path, VALUE writing) {
    (void)self;
    const char *p = StringValueCStr(path);
    if (p[0] == '/') return path;
    char out[2048];
    if (RTEST(writing)) {
        if (!rgss_fs_disk_path(p, out, sizeof out) || !rgss_fs_in_save_dir(out)) rgss_fs_write_path(p, out, sizeof out);
    } else {
        rgss_fs_disk_path(p, out, sizeof out);
    }
    return rb_str_new_cstr(out);
}

static VALUE f_rgss_game_dirs(VALUE self) {
    (void)self;
    return rb_ary_new_from_args(2, rb_str_new_cstr(rgss_fs_save_dir()), rb_str_new_cstr(rgss_fs_game_dir()));
}

// A game file by name, from the save folder, the game folder or the archive, for Win32API's profile reads.
static VALUE f_rgss_read_game_file(VALUE self, VALUE path) {
    (void)self;
    return rgss_read_file(path, NULL);
}

static VALUE run_block(VALUE unused) {
    (void)unused;
    return rb_yield(Qnil);
}

// VX Ace's main loop. F12's reset doesn't exist on VNDegen's devices, so the block runs once.
static VALUE f_rgss_main(VALUE self) {
    (void)self;
    return run_block(Qnil);
}

static VALUE f_rgss_stop(VALUE self) {
    (void)self;
    for (;;) update_frame();
    return Qnil;
}

// A message box blocks the game on PC until the player closes it. Here it goes to the log.
static VALUE f_msgbox(int argc, VALUE *argv, VALUE self) {
    (void)self;
    VALUE text = rb_str_new(NULL, 0);
    for (int i = 0; i < argc; i++) rb_str_append(text, rb_obj_as_string(argv[i]));
    rgss_log("msgbox: %s", StringValueCStr(text));
    return Qnil;
}

static VALUE f_msgbox_p(int argc, VALUE *argv, VALUE self) {
    (void)self;
    for (int i = 0; i < argc; i++) {
        VALUE s = rb_inspect(argv[i]);
        rgss_log("msgbox_p: %s", StringValueCStr(s));
    }
    return argc == 0 ? Qnil : argc == 1 ? argv[0] : rb_ary_new_from_values(argc, argv);
}

void rgss_define_modules(void) {
    rgss_eRGSSError = rb_define_class("RGSSError", rb_eStandardError);
    rgss_eReset = rb_define_class(rgss_version == RGSS3 ? "RGSSReset" : "Reset", rb_eException);
    stopTag = rb_obj_alloc(rb_cObject);
    rb_gc_register_address(&stopTag);

    rb_define_module_function(rb_mKernel, "rgss_log", f_rgss_log, 1);
    rb_define_module_function(rb_mKernel, "rgss_read_game_file", f_rgss_read_game_file, 1);
    rb_define_module_function(rb_mKernel, "rgss_game_path", f_rgss_game_path, 2);
    rb_define_module_function(rb_mKernel, "rgss_game_dirs", f_rgss_game_dirs, 0);
    rb_define_global_function("load_data", f_load_data, 1);
    rb_define_global_function("save_data", f_save_data, 2);
    if (rgss_version == RGSS3) {
        rb_define_global_function("rgss_main", f_rgss_main, 0);
        rb_define_global_function("rgss_stop", f_rgss_stop, 0);
        rb_define_global_function("msgbox", f_msgbox, -1);
        rb_define_global_function("msgbox_p", f_msgbox_p, -1);
    } else {
        // RGSS1 and RGSS2 show `print` and `p` in a message box.
        rb_define_global_function("print", f_msgbox, -1);
        rb_define_global_function("p", f_msgbox_p, -1);
    }

    VALUE g = rb_define_module("Graphics");
    rb_define_module_function(g, "update", graphics_update, 0);
    rb_define_module_function(g, "freeze", graphics_freeze, 0);
    rb_define_module_function(g, "transition", graphics_transition, -1);
    rb_define_module_function(g, "frame_reset", graphics_frame_reset, 0);
    rb_define_module_function(g, "frame_rate", graphics_get_frame_rate, 0);
    rb_define_module_function(g, "frame_rate=", graphics_put_frame_rate, 1);
    rb_define_module_function(g, "frame_count", graphics_get_frame_count, 0);
    rb_define_module_function(g, "frame_count=", graphics_put_frame_count, 1);
    if (rgss_version >= RGSS2) {
        rb_define_module_function(g, "wait", graphics_wait, 1);
        rb_define_module_function(g, "fadeout", graphics_fadeout, 1);
        rb_define_module_function(g, "fadein", graphics_fadein, 1);
        rb_define_module_function(g, "snap_to_bitmap", graphics_snap_to_bitmap, 0);
        rb_define_module_function(g, "width", graphics_width, 0);
        rb_define_module_function(g, "height", graphics_height, 0);
        rb_define_module_function(g, "resize_screen", graphics_resize_screen, 2);
        rb_define_module_function(g, "brightness", graphics_get_brightness, 0);
        rb_define_module_function(g, "brightness=", graphics_put_brightness, 1);
    }
    if (rgss_version == RGSS3) rb_define_module_function(g, "play_movie", graphics_play_movie, 1);

    VALUE in = rb_define_module("Input");
    rb_define_module_function(in, "update", input_update, 0);
    rb_define_module_function(in, "press?", input_press, 1);
    rb_define_module_function(in, "trigger?", input_trigger, 1);
    rb_define_module_function(in, "repeat?", input_repeat, 1);
    rb_define_module_function(in, "dir4", input_dir4, 0);
    rb_define_module_function(in, "dir8", input_dir8, 0);
    rb_define_module_function(in, "rgss_key_down?", input_key_down, 1);
    rb_define_module_function(in, "rgss_mouse_down?", input_mouse_down, 1);
    static const struct { const char *name; int number; } constants[] = {
        {"DOWN", B_DOWN}, {"LEFT", B_LEFT}, {"RIGHT", B_RIGHT}, {"UP", B_UP}, {"A", B_A}, {"B", B_B}, {"C", B_C},
        {"X", B_X}, {"Y", B_Y}, {"Z", B_Z}, {"L", B_L}, {"R", B_R}, {"SHIFT", B_SHIFT}, {"CTRL", B_CTRL},
        {"ALT", B_ALT}, {"F5", B_F5}, {"F6", B_F6}, {"F7", B_F7}, {"F8", B_F8}, {"F9", B_F9}};
    for (size_t i = 0; i < sizeof constants / sizeof constants[0]; i++) {
        VALUE value = rgss_version == RGSS3 ? ID2SYM(rb_intern(constants[i].name)) : INT2FIX(constants[i].number);
        rb_define_const(in, constants[i].name, value);
    }

    VALUE a = rb_define_module("Audio");
    rb_define_module_function(a, "bgm_play", audio_bgm_play, -1);
    rb_define_module_function(a, "bgm_stop", audio_bgm_stop, 0);
    rb_define_module_function(a, "bgm_fade", audio_bgm_fade, 1);
    rb_define_module_function(a, "bgs_play", audio_bgs_play, -1);
    rb_define_module_function(a, "bgs_stop", audio_bgs_stop, 0);
    rb_define_module_function(a, "bgs_fade", audio_bgs_fade, 1);
    rb_define_module_function(a, "me_play", audio_me_play, -1);
    rb_define_module_function(a, "me_stop", audio_me_stop, 0);
    rb_define_module_function(a, "me_fade", audio_me_fade, 1);
    rb_define_module_function(a, "se_play", audio_se_play, -1);
    rb_define_module_function(a, "se_stop", audio_se_stop, 0);
    if (rgss_version == RGSS3) {
        rb_define_module_function(a, "bgm_pos", audio_bgm_pos, 0);
        rb_define_module_function(a, "bgs_pos", audio_bgs_pos, 0);
        rb_define_module_function(a, "setup_midi", audio_setup_midi, 0);
    }
}

// Resets the module state a game left behind, before the next game starts.
void rgss_modules_reset(void) {
    image_free(&frozen);
    isFrozen = false;
    brightness = 255;
    rgss_frame_count = 0;
    memset(held, 0, sizeof held);
    memset(directionOrder, 0, sizeof directionOrder);
    memset(&input, 0, sizeof input);
}

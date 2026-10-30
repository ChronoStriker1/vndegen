// Runs one game on the game thread: reads Game.ini, sets up Ruby for the game's RGSS version, and runs its scripts.
// Ruby starts once per process; between games boot.rb puts the interpreter back as it was before the first game.
#include <ruby.h>
#include <ruby/encoding.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "audio.h"
#include "binding.h"
#include "host.h"
#include "prelude.h"

RGSSVersion rgss_version = RGSS3;
extern int rb_marshal_rgss_bare_strings;

static bool rubyReady;

// MARK: Game.ini

typedef struct {
    char scripts[512];
    char library[256];
    char rtps[4][128];
    int rtpCount;
} GameIni;

static void trim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = 0;
    size_t start = 0;
    while (s[start] == ' ' || s[start] == '\t') start++;
    if (start) memmove(s, s + start, n - start + 1);
}

// The [Game] section's keys RGSS reads. XP names its RTPs RTP1 to RTP3; VX and VX Ace have one RTP key.
static bool read_ini(const char *path, GameIni *ini) {
    memset(ini, 0, sizeof *ini);
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    char line[1024];
    bool inGame = false;
    while (fgets(line, sizeof line, file)) {
        trim(line);
        if (line[0] == '[') {
            inGame = strncasecmp(line, "[Game]", 6) == 0;
            continue;
        }
        char *eq = strchr(line, '=');
        if (!inGame || !eq) continue;
        *eq = 0;
        char *key = line, *value = eq + 1;
        trim(key);
        trim(value);
        if (!strcasecmp(key, "Scripts")) snprintf(ini->scripts, sizeof ini->scripts, "%s", value);
        else if (!strcasecmp(key, "Library")) snprintf(ini->library, sizeof ini->library, "%s", value);
        else if ((!strcasecmp(key, "RTP") || !strncasecmp(key, "RTP", 3)) && *value && ini->rtpCount < 4) {
            snprintf(ini->rtps[ini->rtpCount++], sizeof ini->rtps[0], "%s", value);
        }
    }
    fclose(file);
    return true;
}

static bool ends_with(const char *s, const char *suffix) {
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcasecmp(s + n - m, suffix) == 0;
}

// MARK: Ruby

// Ruby installs handlers for signals the app owns; put the app's back, keeping the one Ruby's threads use.
static void setup_ruby(void) {
    static const int owned[] = {SIGINT, SIGHUP, SIGQUIT, SIGTERM, SIGALRM, SIGUSR1, SIGUSR2, SIGSEGV, SIGBUS,
                                SIGILL, SIGFPE, SIGPIPE, SIGCHLD};
    enum { OWNED = sizeof owned / sizeof owned[0] };
    struct sigaction saved[OWNED];
    for (int i = 0; i < OWNED; i++) sigaction(owned[i], NULL, &saved[i]);

    int argc = 0;
    char **argv = NULL;
    ruby_sysinit(&argc, &argv);
    ruby_init();
    // ruby_options runs Ruby's own Ruby-written core methods and registers the encodings and extensions linked in.
    char *options[] = {"rgss", "--disable=gems,rubyopt,did_you_mean,error_highlight,syntax_suggest", "-EUTF-8:UTF-8",
                       "-e", "", NULL};
    void *node = ruby_options(5, options);
    ruby_exec_node(node);
    int state = 0;

    for (int i = 0; i < OWNED; i++) sigaction(owned[i], &saved[i], NULL);

    rb_eval_string_protect("require 'zlib'; require 'stringio'", &state);
    if (state) rgss_log("could not load zlib");
    rb_eval_string_protect(rgss_prelude_boot, &state);
    if (state) rgss_log("boot.rb failed");
    rubyReady = true;
}

static VALUE eval_prelude(VALUE source) {
    VALUE code = rb_str_new_cstr((const char *)source);
    rb_enc_associate(code, rb_utf8_encoding());
    return rb_funcall(rb_mKernel, rb_intern("eval"), 3, code, rb_const_get(rb_cObject, rb_intern("TOPLEVEL_BINDING")),
                      rb_str_new_cstr("rgss"));
}

static VALUE run_scripts(VALUE path) { return rb_funcall(rb_mKernel, rb_intern("rgss_run_scripts"), 1, path); }

static VALUE catch_run(VALUE tag, VALUE path, int argc, const VALUE *argv, VALUE blockarg) {
    (void)tag;
    (void)argc;
    (void)argv;
    (void)blockarg;
    return run_scripts(path);
}

static VALUE run_caught(VALUE path) { return rb_catch_obj(rgss_stop_tag(), catch_run, path); }

// The sentence RGSS shows for an exception, naming the script section and line it came from (rgss.rb).
static VALUE describe_safely(VALUE error) { return rb_funcall(rb_mKernel, rb_intern("rgss_describe_error"), 1, error); }

// Detaches every object the last game left, so nothing of it draws in the next. Ruby frees them later.
static void scene_clear(Container *c) {
    for (Drawable *d = c->first, *next; d; d = next) {
        next = d->next;
        if (d->kind == DRAW_VIEWPORT) scene_clear(&((Viewport *)d)->children);
        d->prev = d->next = NULL;
        d->container = NULL;
    }
    c->first = c->last = NULL;
}

void rgss_run_game(const HostLaunch *launch) {
    RUBY_INIT_STACK;
    GameIni ini;
    if (!read_ini(launch->gameIni, &ini)) {
        host_set_problem("The game's Game.ini can't be read.");
        return;
    }
    if (ends_with(ini.scripts, ".rxdata")) rgss_version = RGSS1;
    else if (ends_with(ini.scripts, ".rvdata")) rgss_version = RGSS2;
    else rgss_version = RGSS3;
    if (!ini.scripts[0]) snprintf(ini.scripts, sizeof ini.scripts, "Data/Scripts.rvdata2");

    char gameDir[1024];
    snprintf(gameDir, sizeof gameDir, "%s", launch->gameIni);
    char *slash = strrchr(gameDir, '/');
    if (slash) *slash = 0;
    else snprintf(gameDir, sizeof gameDir, ".");

    static const char *archives[] = {"", "Game.rgssad", "Game.rgss2a", "Game.rgss3a"};
    char archive[2048];
    bool hasArchive = rgss_resolve_case(gameDir, archives[rgss_version], archive, sizeof archive);
    rgss_fs_open(launch->saveDir, gameDir, hasArchive ? archive : NULL, (const char *const *)launch->rtps,
                 launch->rtpCount);

    text_reset();
    char fonts[2048];
    if (rgss_resolve_case(gameDir, "Fonts", fonts, sizeof fonts)) text_add_font_folder(fonts);
    for (int i = 0; i < launch->rtpCount; i++) {
        if (rgss_resolve_case(launch->rtps[i], "Fonts", fonts, sizeof fonts)) text_add_font_folder(fonts);
    }

    if (!rubyReady) setup_ruby();
    int state = 0;
    rb_eval_string_protect("RGSSBoot.reset", &state);
    if (state) rgss_log("reset failed");

    scene_clear(&rgss_screen);
    rgss_modules_reset();
    rgss_screen_w = rgss_version == RGSS1 ? 640 : 544;
    rgss_screen_h = rgss_version == RGSS1 ? 480 : 416;
    rgss_frame_rate = rgss_version == RGSS1 ? 40 : 60;
    host_set_frame_rate(rgss_frame_rate);
    rb_marshal_rgss_bare_strings = rgss_version != RGSS3;
    rgss_log("RGSS%d game in %s, scripts %s, archive %s", rgss_version, gameDir, ini.scripts,
             hasArchive ? archive : "none");

    rgss_define_values();
    rgss_define_font();
    rgss_define_bitmap();
    rgss_define_drawables();
    rgss_define_modules();
    rb_gv_set("$RGSS_VERSION", INT2FIX(rgss_version));
    rb_gv_set("$RGSS_LIBRARY", rb_str_new_cstr(ini.library));
    const char *version_prelude = rgss_version == RGSS1 ? rgss_prelude_rpg1 : rgss_version == RGSS2 ? rgss_prelude_rpg2 : rgss_prelude_rpg3;
    const char *preludes[] = {rgss_prelude_common, rgss_prelude_win32api, version_prelude};
    for (size_t i = 0; i < sizeof preludes / sizeof preludes[0]; i++) {
        rb_protect(eval_prelude, (VALUE)preludes[i], &state);
        if (state) {
            VALUE message = rb_protect(describe_safely, rb_errinfo(), &state);
            rgss_log("prelude %zu failed: %s", i, state ? "?" : StringValueCStr(message));
            rb_set_errinfo(Qnil);
            host_set_problem("VNDegen's RGSS runtime could not start.");
            return;
        }
    }

    for (char *p = ini.scripts; *p; p++) if (*p == '\\') *p = '/';
    rb_protect(run_caught, rb_str_new_cstr(ini.scripts), &state);
    if (state) {
        VALUE error = rb_errinfo();
        rb_set_errinfo(Qnil);
        if (!NIL_P(error) && !rb_obj_is_kind_of(error, rb_eSystemExit)) {
            VALUE message = rb_protect(describe_safely, error, &state);
            host_set_problem(state ? "The game stopped with an error." : StringValueCStr(message));
            rb_set_errinfo(Qnil);
        }
    }
    rgss_log("game ended");
    scene_clear(&rgss_screen);
    rgss_fs_close();
}

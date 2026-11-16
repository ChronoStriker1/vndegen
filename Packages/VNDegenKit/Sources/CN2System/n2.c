// The runtime's host interface and frame loop: opening a game, variables, loading scripts, running threads each
// frame, input, and the keys that start the system scripts (menu, shortcuts, backlog).
#include "n2.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

void n2_log(N2 *n, const char *fmt, ...) {
    if (!n->log) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(n->log, "[%7u] ", n->now);
    vfprintf(n->log, fmt, ap);
    fputc('\n', n->log);
    fflush(n->log);
    va_end(ap);
}

void n2_fail(N2 *n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(n->problem, sizeof n->problem, fmt, ap);
    va_end(ap);
    n2_log(n, "problem: %s", n->problem);
    n->quit = true;
}

// ---- system.ini ----

static void ini_parse(Map *m, const char *text) {
    char section[128] = "";
    const char *p = text;
    while (*p) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        char *line = n2_strndup(p, len);
        p += len + (e ? 1 : 0);
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        size_t l = strlen(s);
        while (l && (s[l - 1] == '\r' || s[l - 1] == ' ' || s[l - 1] == '\t')) s[--l] = 0;
        if (s[0] == '[' && l > 1 && s[l - 1] == ']') {
            snprintf(section, sizeof section, "%.*s", (int)(l - 2), s + 1);
        } else if (s[0] && s[0] != ';') {
            char *eq = strchr(s, '=');
            if (eq) {
                *eq = 0;
                char *k = s, *v = eq + 1;
                size_t kl = strlen(k);
                while (kl && (k[kl - 1] == ' ' || k[kl - 1] == '\t')) k[--kl] = 0;
                while (*v == ' ' || *v == '\t') v++;
                char *key = n2_fmt("%s/%s", section, k);
                char *old = map_get(m, key);
                free(old);
                map_set(m, key, n2_strdup(v));
                free(key);
            }
        }
        free(line);
    }
}

const char *n2_ini(N2 *n, const char *section, const char *key) {
    char *k = n2_fmt("%s/%s", section, key);
    const char *v = map_get(&n->ini, k);
    free(k);
    return v;
}

static bool read_ini(N2Fs *fs, Map *ini) {
    size_t size;
    u8 *data = fs_read(fs, "system/system.ini", &size);
    if (!data) data = fs_read(fs, "system.ini", &size);
    if (!data) return false;
    char *utf8 = n2_sjis_to_utf8(data, size, NULL);
    ini_parse(ini, utf8);
    free(utf8);
    free(data);
    return true;
}

bool vnn2_product(const char *gameDir, char *name, size_t nameSize, char *version, size_t versionSize) {
    char err[256];
    N2Fs fs;
    if (!fs_init(&fs, gameDir, err, sizeof err)) return false;
    Map ini = {0};
    bool ok = read_ini(&fs, &ini);
    if (ok) {
        const char *nm = map_get(&ini, "プロダクト/name"), *vs = map_get(&ini, "プロダクト/version");
        snprintf(name, nameSize, "%s", nm ? nm : "");
        snprintf(version, versionSize, "%s", vs ? vs : "");
    }
    map_free(&ini, free);
    fs_free(&fs);
    return ok;
}

// ---- variables ----

static void var_free(void *p) {
    Var *v = p;
    v_free(&v->v);
    free(v);
}

static void arr_free(void *p) {
    Arr *a = p;
    for (int i = 0; i < a->count; i++) v_free(&a->items[i]);
    free(a->items);
    free(a);
}

Value *var_get(N2 *n, const char *name) {
    Var *v = map_get(&n->vars, name);
    return v ? &v->v : NULL;
}

void var_set(N2 *n, const char *name, const Value *val) {
    Var *v = map_get(&n->vars, name);
    if (!v) {
        v = n2_alloc(sizeof *v);
        v->order = ++n->varOrder;
        map_set(&n->vars, name, v);
    }
    Value c = v_copy(val);
    c.delta = 0;
    v_free(&v->v);
    v->v = c;
}

Arr *arr_get(N2 *n, const char *name, bool create) {
    Arr *a = map_get(&n->arrays, name);
    if (!a && create) {
        a = n2_alloc(sizeof *a);
        map_set(&n->arrays, name, a);
    }
    return a;
}

// ---- scripts ----

static char *norm_script_path(const char *path) {
    while (path[0] == '.' && (path[1] == '/' || path[1] == '\\')) path += 2;
    char *p = n2_strdup(path);
    for (char *q = p; *q; q++) *q = *q == '\\' ? '/' : (char)tolower((u8)*q);
    return p;
}

static void register_functions(N2 *n, Script *s) {
    for (int i = 0; i < s->chunkCount; i++)
        if (s->chunks[i]->kind == CHUNK_FUNCTION) map_set(&n->functions, s->chunks[i]->name, s->chunks[i]);
}

Script *n2_load_script(N2 *n, const char *path) {
    char *key = norm_script_path(path);
    Script *s = map_get(&n->scripts, key);
    if (s) {
        free(key);
        return s;
    }
    size_t size;
    u8 *data = fs_read(&n->fs, key, &size);
    if (!data) {
        n2_log(n, "missing script %s", key);
        free(key);
        return NULL;
    }
    char err[512];
    s = script_compile(key, data, size, err, sizeof err);
    free(data);
    if (!s) {
        n2_log(n, "script error: %s", err);
        free(key);
        return NULL;
    }
    map_set(&n->scripts, key, s);
    free(key);
    for (int i = 0; i < s->includeCount; i++) n2_load_script(n, s->includes[i]);
    // A file's own functions win over its includes'.
    register_functions(n, s);
    return s;
}

Chunk *n2_find_function(N2 *n, const char *name) {
    return map_get(&n->functions, name);
}

// ---- opening ----

static void mkdirs(const char *path) {
    char *p = n2_strdup(path);
    for (char *q = p + 1; *q; q++) {
        if (*q == '/') {
            *q = 0;
            mkdir(p, 0755);
            *q = '/';
        }
    }
    mkdir(p, 0755);
    free(p);
}

static void set_num(N2 *n, const char *name, double v) {
    Value x = v_num(v);
    var_set(n, name, &x);
}

static void set_str(N2 *n, const char *name, const char *s) {
    Value x = v_str(s);
    var_set(n, name, &x);
    v_free(&x);
}

static void set_defaults(N2 *n) {
    // Engine variables the scripts read before they set them. Volumes come from system.ini.
    const char *bgm = n2_ini(n, "サウンド", "bgm_volume"), *se = n2_ini(n, "サウンド", "se_volume"),
               *voice = n2_ini(n, "サウンド", "voice_volume");
    if (!var_get(n, "#SYSTEM_sound_volume_bgm")) set_num(n, "#SYSTEM_sound_volume_bgm", bgm ? atof(bgm) : 700);
    if (!var_get(n, "#SYSTEM_sound_volume_se")) set_num(n, "#SYSTEM_sound_volume_se", se ? atof(se) : 700);
    if (!var_get(n, "#SYSTEM_sound_volume_voice")) set_num(n, "#SYSTEM_sound_volume_voice", voice ? atof(voice) : 1000);
    set_num(n, "#SYSTEM_sound_volume_bgm_default", bgm ? atof(bgm) : 700);
    set_num(n, "#SYSTEM_sound_volume_se_default", se ? atof(se) : 700);
    set_num(n, "#SYSTEM_sound_volume_voice_default", voice ? atof(voice) : 1000);
    if (!var_get(n, "#SYSTEM_text_speed")) set_num(n, "#SYSTEM_text_speed", 50);
    if (!var_get(n, "#SYSTEM_auto_wait_per_character")) set_num(n, "#SYSTEM_auto_wait_per_character", 50);
    if (!var_get(n, "#SYSTEM_sound_bgm")) set_num(n, "#SYSTEM_sound_bgm", 1);
    if (!var_get(n, "#SYSTEM_sound_se")) set_num(n, "#SYSTEM_sound_se", 1);
    if (!var_get(n, "#SYSTEM_sound_voice")) set_num(n, "#SYSTEM_sound_voice", 1);
    set_str(n, "#SYSTEM_save_path", n->saveDir);
    set_num(n, "#SYSTEM_window_full", 0);
}

VNN2 *vnn2_open(const VNN2Config *config, char *err, size_t errSize) {
    char tmp[256];
    if (!err) {
        err = tmp;
        errSize = sizeof tmp;
    }
    N2 *n = n2_alloc(sizeof *n);
    n->cfg = *config;
    if (config->logPath) n->log = fopen(config->logPath, "a");
    if (!fs_init(&n->fs, config->gameDir, err, errSize)) {
        vnn2_close(n);
        return NULL;
    }
    if (!read_ini(&n->fs, &n->ini)) {
        snprintf(err, errSize, "the game has no system.ini");
        vnn2_close(n);
        return NULL;
    }
    const char *title = n2_ini(n, "ウィンドウ", "title");
    n->title = n2_strdup(title ? title : "N2System");
    n->saveDir = n2_strdup(config->saveDir ? config->saveDir : ".");
    mkdirs(n->saveDir);
    n->startTime = config->startTime ? config->startTime : (long long)time(NULL);
    n->rngState = (int)(n->startTime & 0x7fffffff) | 1;
    n->screen = n2_alloc(sizeof(u32) * N2_W * N2_H);
    Obj *roots[2] = {&n->root, &n->sysRoot};
    for (int i = 0; i < 2; i++) {
        roots[i]->kind = OBJ_NAME;
        roots[i]->name = n2_strdup("");
        roots[i]->opacity = 1000;
        roots[i]->zoomX = roots[i]->zoomY = 1000;
        roots[i]->transition = -1;
    }
    n->curRoot = &n->root;
    n->pendingLoad = -1;
    n->mixer = mixer_new(config->audioRate > 0 ? config->audioRate : 44100);

    const char *face = n2_ini(n, "フォント", "face"), *size = n2_ini(n, "フォント", "size");
    snprintf(n->fontFace, sizeof n->fontFace, "%s", face ? face : "MS Gothic");
    n->fontSize = size ? atoi(size) : 18;
    n->fontColor = 0xffffff;
    n->fontOutline = 0x000000;
    n->fontWeight = 500;
    n->font = config->fontPath ? font_open(config->fontPath, config->fontIndex) : font_open_named(n->fontFace);
    if (!n->font) {
        snprintf(err, errSize, "no font for %s", n->fontFace);
        vnn2_close(n);
        return NULL;
    }

    load_globals(n);
    set_defaults(n);

    const char *start = n2_ini(n, "スクリプト", "start");
    Script *s = n2_load_script(n, start ? start : "nss/boot.nss");
    Chunk *mainChunk = s ? script_find(s, CHUNK_CHAPTER, "main") : NULL;
    if (!mainChunk) {
        snprintf(err, errSize, "the start script %s has no chapter main", start ? start : "nss/boot.nss");
        vnn2_close(n);
        return NULL;
    }
    n->main = thread_new(n, "main");
    n->main->isMain = true;
    thread_call(n, n->main, mainChunk, NULL, 0);
    n->dirty = true;
    n2_log(n, "opened %s, save folder %s", n->title, n->saveDir);
    return n;
}

static void script_free_v(void *p) { script_free(p); }
static void img_free_v(void *p) { img_unref(p); }

void vnn2_close(VNN2 *n) {
    if (!n) return;
    Obj *roots[2] = {&n->root, &n->sysRoot};
    for (int i = 0; i < 2; i++) {
        while (roots[i]->childCount) obj_destroy(n, roots[i]->children[roots[i]->childCount - 1]);
        free(roots[i]->children);
        free(roots[i]->name);
    }
    sound_release_globals(n);
    for (int i = 0; i < n->threadCount; i++) thread_free(n->threads[i]);
    free(n->threads);
    map_free(&n->scripts, script_free_v);
    map_free(&n->functions, NULL);
    map_free(&n->vars, var_free);
    map_free(&n->arrays, arr_free);
    map_free(&n->images, img_free_v);
    map_free(&n->imageCache, img_free_v);
    map_free(&n->ini, free);
    map_free(&n->pressedButtons, NULL);
    for (int i = 0; i < n->shortcutCount; i++) free(n->shortcuts[i].script);
    if (n->mixer) mixer_free(n->mixer);
    if (n->font) font_close(n->font);
    fs_free(&n->fs);
    free(n->screen);
    free(n->title);
    free(n->saveDir);
    free(n->outlineMode);
    if (n->log) fclose(n->log);
    free(n);
}

// ---- input ----

void vnn2_mouse_move(VNN2 *n, int x, int y) {
    n->mouseX = x;
    n->mouseY = y;
}

void vnn2_mouse_button(VNN2 *n, int button, bool down) {
    if (button < 1 || button > 2) return;
    if (down && !n->mouseDown[button]) n->mousePressed[button] = true;
    if (!down && n->mouseDown[button]) n->mouseReleased[button] = true;
    n->mouseDown[button] = down;
}

void vnn2_key(VNN2 *n, int vk, bool down) {
    if (vk < 0 || vk > 255) return;
    if (down && !n->keyDown[vk]) n->keyPressed[vk] = true;
    n->keyDown[vk] = down;
}

void vnn2_wheel(VNN2 *n, int notches) { n->wheel += notches; }

void vnn2_set_paused(VNN2 *n, bool paused) { n->paused = paused; }
void vnn2_request_quit(VNN2 *n) { n->quit = true; }
const char *vnn2_title(VNN2 *n) { return n->title; }
const char *vnn2_problem(VNN2 *n) { return n->problem[0] ? n->problem : NULL; }

const uint32_t *vnn2_pixels(VNN2 *n, int *w, int *h) {
    *w = N2_W;
    *h = N2_H;
    return n->screen;
}

uint32_t vnn2_frame_serial(VNN2 *n) { return n->frameSerial; }

void vnn2_audio(VNN2 *n, float *out, int frames) {
    if (n->paused) {
        memset(out, 0, sizeof(float) * 2 * (size_t)frames);
        return;
    }
    mixer_render(n->mixer, out, frames);
}

// ---- system scripts started by keys ----

bool n2_skip_active(N2 *n) {
    Value *s = var_get(n, "$SYSTEM_skip");
    if (s && v_truthy(s)) return true;
    Value *lock = var_get(n, "$SYSTEM_skip_lock");
    return n->keyDown[0x11] && !(lock && v_truthy(lock));
}

static bool flag(N2 *n, const char *name) {
    Value *v = var_get(n, name);
    return v && v_truthy(v);
}

static void start_system_script(N2 *n, const char *path) {
    if (n->system && n->system->wait != WAIT_DONE) return;
    Script *s = n2_load_script(n, path);
    Chunk *c = s ? script_find(s, CHUNK_CHAPTER, "main") : NULL;
    if (!c) return;
    if (!n->system) n->system = thread_new(n, "system");
    thread_kill(n, n->system);
    n->system->ctx = &n->sysRoot;
    thread_call(n, n->system, c, NULL, 0);
    n2_log(n, "system script %s", path);
}

static void set_key_flags(N2 *n) {
    static const struct {
        int vk;
        const char *name;
    } keys[] = {
        {0x1b, "$SYSTEM_keydown_esc"}, {0x20, "$SYSTEM_keydown_space"}, {0x11, "$SYSTEM_keydown_ctrl"},
        {0x26, "$SYSTEM_keydown_up"},  {0x28, "$SYSTEM_keydown_down"},  {0x0d, "$SYSTEM_keydown_enter"},
    };
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++)
        if (n->keyPressed[keys[i].vk]) set_num(n, keys[i].name, 1);
    for (int c = 'A'; c <= 'Z'; c++) {
        if (!n->keyPressed[c]) continue;
        char name[32];
        snprintf(name, sizeof name, "$SYSTEM_keydown_%c", tolower(c));
        set_num(n, name, 1);
    }
    if (n->mousePressed[VNN2_MOUSE_RIGHT]) set_num(n, "$SYSTEM_r_button_down", 1);
    if (n->mousePressed[VNN2_MOUSE_LEFT]) set_num(n, "$SYSTEM_l_button_down", 1);
}

static bool main_waits_for_player(N2 *n) {
    Thread *m = n->main;
    return m && (m->wait == WAIT_KEY || m->wait == WAIT_TEXT || m->wait == WAIT_FRAME || m->wait == WAIT_TIME);
}

static void handle_system_keys(N2 *n) {
    if (n->system && n->system->wait != WAIT_DONE) return;
    if (!main_waits_for_player(n)) return;
    bool menuLocked = flag(n, "$SYSTEM_menu_lock");
    if (n->mousePressed[VNN2_MOUSE_RIGHT] && !menuLocked) {
        const char *menu = n2_ini(n, "スクリプト", "menu");
        if (menu) {
            start_system_script(n, menu);
            n->mousePressed[VNN2_MOUSE_RIGHT] = false;
            return;
        }
    }
    if (n->wheel > 0 && !flag(n, "$SYSTEM_backlog_lock") && !menuLocked) {
        const char *bl = n2_ini(n, "バックログ", "script");
        if (bl) {
            start_system_script(n, bl);
            n->wheel = 0;
            return;
        }
    }
    if (menuLocked) return;
    for (int i = 0; i < n->shortcutCount; i++) {
        if (n->keyPressed[n->shortcuts[i].vk]) {
            start_system_script(n, n->shortcuts[i].script);
            n->keyPressed[n->shortcuts[i].vk] = false;
            return;
        }
    }
    if (n->keyPressed[0x20] && !flag(n, "$SYSTEM_text_erase_lock")) {
        set_num(n, "$SYSTEM_text_erase", flag(n, "$SYSTEM_text_erase") ? 0 : 1);
        n->keyPressed[0x20] = false;
        n->dirty = true;
    }
}

// ---- waits ----

typedef struct AnimCheck {
    int anim;
    bool running;
} AnimCheck;

static void check_anim(N2 *n, Obj *o, void *ctx) {
    AnimCheck *c = ctx;
    if (c->anim < 0) {
        for (int i = 0; i < ANIM_COUNT; i++)
            if (o->anims[i].active) c->running = true;
    } else if (o->anims[c->anim].active) {
        c->running = true;
    }
}

static void resolve_wait(N2 *n, Thread *t, bool advance, bool firstPass) {
    switch (t->wait) {
    case WAIT_TIME:
        if ((i32)(n->now - t->waitUntil) >= 0) t->wait = WAIT_NONE;
        break;
    case WAIT_KEY:
        if (advance || n2_skip_active(n) || (t->waitUntil && (i32)(n->now - t->waitUntil) >= 0)) t->wait = WAIT_NONE;
        break;
    case WAIT_FRAME:
        if (firstPass) t->wait = WAIT_NONE;
        break;
    case WAIT_ANIM: {
        AnimCheck c = {t->waitAnim, false};
        obj_query(n, t->waitQuery, check_anim, &c);
        if (!c.running || (t->waitUntil && (i32)(n->now - t->waitUntil) >= 0)) t->wait = WAIT_NONE;
        break;
    }
    case WAIT_TEXT:
        if (!t->waitObj || !t->waitObj->text || text_finished(t->waitObj->text)) {
            t->wait = WAIT_NONE;
            t->waitObj = NULL;
        }
        break;
    case WAIT_TEXTSEG:
        if (!t->textOwner || text_segment_done(t->textOwner)) t->wait = WAIT_NONE;
        break;
    case WAIT_SOUND:
        if (!t->waitObj || !t->waitObj->sound || !sound_playing(t->waitObj->sound)) {
            t->wait = WAIT_NONE;
            t->waitObj = NULL;
        }
        break;
    }
}

// Clears waitObj pointers to an object about to be destroyed.
void n2_forget_obj(N2 *n, Obj *o) {
    for (int i = 0; i < n->threadCount; i++)
        if (n->threads[i]->waitObj == o) n->threads[i]->waitObj = NULL;
}

static void update_objs(N2 *n, Obj *o) {
    for (int i = 0; i < o->childCount; i++) {
        Obj *c = o->children[i];
        if (c->disused && c->sound && !sound_playing(c->sound) && !c->locked) {
            obj_destroy(n, c);
            i--;
            continue;
        }
        obj_update_anims(n, c);
        if (c->text) text_update(n, c);
        update_objs(n, c);
    }
}

bool n2_obj_alive_in(Obj *root, Obj *o) {
    if (root == o) return true;
    for (int i = 0; i < root->childCount; i++)
        if (n2_obj_alive_in(root->children[i], o)) return true;
    return false;
}

bool n2_obj_alive(N2 *n, Obj *o) { return n2_obj_alive_in(&n->root, o) || n2_obj_alive_in(&n->sysRoot, o); }

// Clears every object and thread. With restartMain, the start script runs again from its chapter main.
void n2_reset(N2 *n, bool restartMain) {
    Obj *roots[2] = {&n->root, &n->sysRoot};
    for (int i = 0; i < 2; i++)
        while (roots[i]->childCount) obj_destroy(n, roots[i]->children[roots[i]->childCount - 1]);
    for (int i = 0; i < n->threadCount; i++) {
        Thread *t = n->threads[i];
        if (t != n->main && t != n->system) {
            thread_free(t);
            n->threads[i--] = n->threads[--n->threadCount];
        }
    }
    thread_kill(n, n->main);
    if (n->system) thread_kill(n, n->system);
    voice_stop(n);
    map_free(&n->images, (void (*)(void *))img_unref);
    map_free(&n->pressedButtons, NULL);
    n->activeBlock = NULL;
    n->activeBlockScript = NULL;
    if (restartMain) {
        const char *start = n2_ini(n, "スクリプト", "start");
        Script *s = n2_load_script(n, start ? start : "nss/boot.nss");
        Chunk *c = s ? script_find(s, CHUNK_CHAPTER, "main") : NULL;
        if (c) thread_call(n, n->main, c, NULL, 0);
    }
    n->dirty = true;
}

bool vnn2_frame(VNN2 *n, uint32_t elapsedMs) {
    if (n->quit) return false;
    if (n->paused) return true;
    if (elapsedMs > 100) elapsedMs = 100;
    n->now += elapsedMs;

    set_key_flags(n);
    buttons_update(n);
    handle_system_keys(n);

    // A click, Enter, or the wheel turning down advances text. Down arrow too, as on the PC.
    bool advance = n->mousePressed[VNN2_MOUSE_LEFT] || n->keyPressed[0x0d] || n->keyPressed[0x28] || n->wheel < 0;
    n->anyPress = advance;
    if (advance && flag(n, "$SYSTEM_text_erase")) {
        // The first click after hiding the text box shows it again.
        set_num(n, "$SYSTEM_text_erase", 0);
        advance = n->anyPress = false;
        n->dirty = true;
    }

    update_objs(n, &n->root);
    update_objs(n, &n->sysRoot);
    backlog_input(n);
    text_deliver_input(n, advance);

    // Run threads until none can move this frame. Threads created during the pass run in the same frame.
    bool systemRunning = n->system && n->system->wait != WAIT_DONE;
    for (int pass = 0; pass < 8; pass++) {
        bool moved = false;
        for (int i = 0; i < n->threadCount; i++) {
            Thread *t = n->threads[i];
            if (t->wait == WAIT_DONE || t->paused) continue;
            if (t == n->main && systemRunning) continue;
            if (t->wait != WAIT_NONE)
                resolve_wait(n, t, advance && pass == 0 && (t == n->main || t == n->system), pass == 0);
            if (t->wait == WAIT_NONE && t->fp > 0) {
                n->curRoot = t->ctx ? t->ctx : &n->root;
                thread_run(n, t);
                n->curRoot = &n->root;
                moved = true;
            }
            if (n->pendingLoad >= 0 || n->pendingReset) break;
        }
        systemRunning = n->system && n->system->wait != WAIT_DONE;
        if (n->pendingLoad >= 0) {
            int slot = n->pendingLoad;
            n->pendingLoad = -1;
            if (!load_slot(n, slot)) n2_log(n, "loading slot %d failed", slot);
            break;
        }
        if (n->pendingReset) {
            n->pendingReset = false;
            n2_reset(n, true);
            break;
        }
        if (!moved || n->quit) break;
    }

    // Free finished threads nothing refers to.
    for (int i = 0; i < n->threadCount;) {
        Thread *t = n->threads[i];
        if (t->wait == WAIT_DONE && t != n->main && t != n->system && !t->owner) {
            thread_free(t);
            n->threads[i] = n->threads[--n->threadCount];
        } else {
            i++;
        }
    }
    if (n->main->wait == WAIT_DONE && !n->quit) {
        n2_log(n, "the main script ended");
        n->quit = true;
    }

    mixer_update(n);
    n2_render(n);
    memset(n->mousePressed, 0, sizeof n->mousePressed);
    memset(n->mouseReleased, 0, sizeof n->mouseReleased);
    memset(n->keyPressed, 0, sizeof n->keyPressed);
    n->wheel = 0;
    return !n->quit;
}

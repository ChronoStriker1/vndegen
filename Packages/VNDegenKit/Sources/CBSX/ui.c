#include "bsx.h"

typedef struct Item {
    char key[32];
    Image *img;
    int frames;
    int x, y, w, h;
} Item;

struct Phase {
    Image *base;
    Item items[16];
    int count;
    int hover;
    u32 start;
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

static bool button_spec(BX *b, const char *sec, const char *key, Item *it) {
    const char *v = bx_cfg_get(&b->cfg, sec, key);
    if (!v) return false;
    char name[64], f[64];
    bx_cfg_field(v, 0, name, sizeof name);
    memset(it, 0, sizeof *it);
    snprintf(it->key, sizeof it->key, "%s", key);
    it->img = own(b, name);
    it->frames = bx_cfg_field(v, 1, f, sizeof f) > 0 ? bx_eval(f, NULL) : 1;
    if (it->frames < 1) it->frames = 1;
    it->x = bx_cfg_field(v, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    it->y = bx_cfg_field(v, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    it->w = it->img ? it->img->w : 0;
    it->h = it->img ? it->img->h / it->frames : 0;
    return true;
}

static void custom_map_update(BX *b);

static void draw_item(BX *b, u32 *dst, const Item *it, int frame, int ox, int oy) {
    if (!it->img) return;
    if (frame >= it->frames) frame = it->frames - 1;
    bx_blit_part(dst, b->W, b->H, it->img, 0, frame * it->h, it->w, it->h, ox + it->x, oy + it->y, 255);
}

static bool inside(const Item *it, int x, int y, int ox, int oy) {
    return x >= ox + it->x && x < ox + it->x + it->w && y >= oy + it->y && y < oy + it->y + it->h;
}

static void phase_free(BX *b) {
    Phase *p = b->ph;
    if (!p) return;
    bx_image_free(p->base);
    for (int i = 0; i < p->count; i++) bx_image_free(p->items[i].img);
    free(p);
    b->ph = NULL;
}

void bx_phase_end(BX *b) {
    if ((b->phase == PH_DATA || b->phase == PH_TITLE_LOAD) && b->phaseBack != PH_NONE) {
        int back = b->phaseBack;
        b->phaseBack = PH_NONE;
        b->phase = back;
        return;
    }
    phase_free(b);
    b->phase = PH_NONE;
    if (b->th.wait == W_PHASE) b->th.wait = W_NONE;
}

void bx_phase_reset(BX *b) {
    bx_config_free(b);
    bx_status_free(b);
    bx_data_free(b);
    bx_log_free(b);
    bx_map_free(b);
    phase_free(b);
    b->phaseBack = PH_NONE;
    b->phase = PH_NONE;
    if (b->th.wait == W_PHASE) b->th.wait = W_NONE;
}

void bx_open_data(BX *b, bool save) {
    bx_save_capture_thumb(b);
    b->phaseBack = b->phase;
    b->phase = b->phase == PH_TITLE ? PH_TITLE_LOAD : PH_DATA;
    bx_data_open(b, save);
}

static void title_start(BX *b) {
    Phase *p = b->ph;
    p->base = own(b, bx_cfg_get(&b->cfg, "title phase", "base"));
    static const char *keys[] = {"op", "start", "load", "museum", "config", "quit"};
    for (int i = 0; i < 6; i++)
        if (button_spec(b, "title phase", keys[i], &p->items[p->count])) p->count++;
    const char *bgm = bx_cfg_get(&b->cfg, "title phase", "bgm name");
    if (bgm && strcmp(bgm, b->bgmName)) {
        snprintf(b->bgmName, sizeof b->bgmName, "%s", bgm);
        bx_snd_play(b, CH_BGM, RX_BGM, bgm, true, 0);
    }
}

void bx_phase_start(BX *b, int phase) {
    phase_free(b);
    b->ph = bx_alloc(sizeof(Phase));
    b->ph->hover = -1;
    b->ph->start = b->now;
    b->phase = phase;
    b->th.wait = W_PHASE;
    bx_msg_clear(b);
    b->winShown = false;
    if (phase == PH_TITLE) title_start(b);
    else if (phase == PH_MAP) {
        if (bx_map_start(b)) bx_autosave(b);
        else bx_phase_end(b);
    } else if (phase == PH_CUSTOM_MAP && !bx_cfg_section(&b->cfg, "map phase")) bx_autosave(b);
}

static void title_pick(BX *b, const char *key) {
    if (!strcmp(key, "quit")) {
        b->quit = true;
        return;
    }
    if (!strcmp(key, "load")) {
        bx_open_data(b, false);
        return;
    }
    if (!strcmp(key, "config")) {
        bx_config_open(b);
        return;
    }
    if (!strcmp(key, "museum")) {
        phase_free(b);
        bx_museum_open(b);
        return;
    }
    const char *label = !strcmp(key, "op") ? "opening" : !strcmp(key, "start") ? "prologue" : NULL;
    if (!label) {
        bx_log(b, "title menu %s isn't available yet", key);
        return;
    }
    int l = bx_script_label(b->sc, label);
    if (l < 0) {
        bx_log(b, "no label %s", label);
        return;
    }
    bx_phase_end(b);
    b->th.sp = 0;
    bx_vm_goto(b, l);
}

void bx_phase_update(BX *b) {
    if (b->phase == PH_STATUS) {
        bx_status_update(b);
        return;
    }
    if (b->phase == PH_MUSEUM) {
        bx_museum_update(b);
        return;
    }
    if (b->phase == PH_CONFIG) {
        bx_config_update(b);
        return;
    }
    if (b->phase == PH_LOG) {
        bx_log_update(b);
        return;
    }
    if (b->phase == PH_DATA || b->phase == PH_TITLE_LOAD) {
        bx_data_update(b);
        return;
    }
    if (b->phase == PH_CUSTOM_MAP) {
        bx_ui_update(b);
        if (b->phase == PH_CUSTOM_MAP) custom_map_update(b);
        return;
    }
    if (b->phase == PH_MAP) {
        bx_map_update(b);
        return;
    }
    Phase *p = b->ph;
    if (!p) return;
    p->hover = -1;
    for (int i = 0; i < p->count; i++)
        if (inside(&p->items[i], b->mx, b->my, 0, 0)) p->hover = i;
    if (b->click && p->hover >= 0) {
        b->click = false;
        bx_snd_play(b, CH_SYS, RX_SYS, "check", false, 0);
        if (b->phase == PH_TITLE) title_pick(b, p->items[p->hover].key);
    }
}

void bx_phase_draw(BX *b, u32 *dst) {
    if (b->phase == PH_STATUS) {
        bx_status_draw(b, dst);
        return;
    }
    if (b->phase == PH_MUSEUM) {
        bx_museum_draw(b, dst);
        return;
    }
    if (b->phase == PH_CONFIG) {
        bx_config_draw(b, dst);
        return;
    }
    if (b->phase == PH_LOG) {
        bx_log_draw(b, dst);
        return;
    }
    if (b->phase == PH_DATA || b->phase == PH_TITLE_LOAD) {
        bx_data_draw(b, dst);
        return;
    }
    if (b->phase == PH_CUSTOM_MAP) {
        bx_ui_draw(b, dst);
        return;
    }
    if (b->phase == PH_MAP) {
        bx_map_draw(b, dst);
        return;
    }
    Phase *p = b->ph;
    if (!p) return;
    if (p->base) bx_blit(dst, b->W, b->H, p->base, p->base->x, p->base->y, 255);
    for (int i = 0; i < p->count; i++) draw_item(b, dst, &p->items[i], i == p->hover ? 1 : 0, 0, 0);
}

static int view_button(const char *key) { return !strcmp(key, "view up") ? 1 : !strcmp(key, "view middle") ? 2 : !strcmp(key, "view down") ? 3 : 0; }

void bx_ui_init(BX *b) {
    const Cfg *c = &b->cfg;
    b->menuBase = own(b, bx_cfg_get(c, "game menu", "base"));
    int pos[2] = {0, b->H - 104};
    bx_cfg_ints(c, "game menu", "base pos", pos, 2);
    b->menuX = pos[0];
    b->menuY = pos[1];
    int show[4] = {0, b->menuY, b->W, b->H - b->menuY};
    bx_cfg_ints(c, "game menu", "show area", show, 4);
    memcpy(b->menuShow, show, sizeof show);
    b->lamp = own(b, bx_cfg_get(c, "game menu", "hold icon"));
    b->menuVisible = true;
    b->winLayer = b->menuLayer = true;
    b->cutIn = true;
    for (int i = 0; i < c->count && b->buttonCount < 24; i++) {
        if (strcmp(c->sec[i], "system button")) continue;
        char group[32];
        if (bx_cfg_field(c->val[i], 4, group, sizeof group) <= 0 || strcmp(group, "game")) continue;
        Item it;
        if (!button_spec(b, "system button", c->key[i], &it)) continue;
        Button *bt = &b->buttons[b->buttonCount++];
        snprintf(bt->key, sizeof bt->key, "%s", it.key);
        if (view_button(it.key)) b->viewButtons = true;
        bt->img = it.img;
        bt->frames = it.frames;
        bt->x = it.x;
        bt->y = it.y;
    }
    b->buttonHover = -1;
}

void bx_ui_free(BX *b) {
    phase_free(b);
    bx_config_free(b);
    bx_status_free(b);
    bx_data_free(b);
    bx_log_free(b);
    bx_map_free(b);
    bx_image_free(b->menuBase);
    bx_image_free(b->lamp);
    for (int i = 0; i < b->buttonCount; i++) bx_image_free(b->buttons[i].img);
    bx_image_free(b->msgBase);
    bx_image_free(b->msgFrame);
    bx_image_free(b->msgName);
    bx_image_free(b->unagashi);
    bx_image_free(b->xCounter);
    bx_image_free(b->choiceItem[0]);
    bx_image_free(b->choiceItem[1]);
    bx_image_free(b->choiceItem[2]);
}

static Item button_item(const Button *bt) {
    Item it = {0};
    it.img = bt->img;
    it.frames = bt->frames;
    it.x = bt->x;
    it.y = bt->y;
    it.w = bt->img ? bt->img->w : 0;
    it.h = bt->img ? bt->img->h / bt->frames : 0;
    return it;
}

static bool button_enabled(BX *b, const Button *bt) {
    if (!strcmp(bt->key, "simple view")) return b->viewEnabled;
    if (!strcmp(bt->key, "simple strip")) return b->dressEnabled;
    return true;
}

static bool any_bustup(BX *b, bool closeUp) {
    for (int i = 0; i < BX_OBJS; i++) {
        Obj *o = &b->shown.obj[i];
        if (o->used && o->kind == OK_CHAR && (!closeUp || (strlen(o->name) == 12 && (o->name[4] == 'u' || o->name[4] == 'U')))) return true;
    }
    return false;
}

static void button_press(BX *b, const char *key) {
    if (!strcmp(key, "simple strip")) {
        int table[8] = {0}, n = bx_cfg_ints(&b->cfg, "system button", "simple strip table", table, 8);
        if (n > 0 && any_bustup(b, false)) {
            b->stripLevel = (b->stripLevel + 1) % n;
            b->shownDirty = true;
        }
        return;
    }
    if (!strcmp(key, "simple view")) {
        if (any_bustup(b, true)) {
            b->viewLevel = b->viewLevel % 3 + 1;
            b->shownDirty = true;
        }
        return;
    }
    if (view_button(key)) {
        b->viewLevel = view_button(key);
        b->shownDirty = true;
        return;
    }
    if (!strcmp(key, "skip")) b->skipMode = !b->skipMode;
    else if (!strcmp(key, "cut in")) {
        b->cutIn = !b->cutIn;
        b->shownDirty = true;
    }
    else if (!strcmp(key, "auto ex")) b->autoMode = !b->autoMode;
    else if (!strcmp(key, "voice")) {
        if (b->voice) bx_snd_play(b, CH_VOICE, RX_VOICE, b->voice, false, 0);
    } else if (!strcmp(key, "hold menu")) b->menuVisible = !b->menuVisible;
    else if (!strcmp(key, "quick save")) {
        bx_save_capture_thumb(b);
        bx_save_slot(b, -1);
    } else if (!strcmp(key, "quick load")) bx_load_slot(b, -1);
    else if (!strcmp(key, "log")) bx_log_open(b);
    else if (!strcmp(key, "config")) bx_config_open(b);
    else if (!strcmp(key, "status")) bx_status_open(b);
    else if (!strcmp(key, "save")) bx_open_data(b, true);
    else if (!strcmp(key, "load")) bx_open_data(b, false);
    else bx_log(b, "menu button %s isn't available yet", key);
}

static bool always_shown(const Button *bt) { return !strcmp(bt->key, "simple view") || !strcmp(bt->key, "simple strip"); }

static bool menu_context(BX *b) {
    if (b->phase == PH_CUSTOM_MAP) return b->menuLayer && !b->transActive && !bx_cfg_section(&b->cfg, "map phase");
    return b->phase == PH_NONE && b->winShown && !b->winHidden && b->menuLayer && b->th.wait == W_TEXT;
}

static bool map_disabled(BX *b, const Button *bt) { return b->phase == PH_CUSTOM_MAP && (!strcmp(bt->key, "skip") || !strcmp(bt->key, "voice")); }

void bx_ui_update(BX *b) {
    b->buttonHover = -1;
    bool inMenu = b->mx >= b->menuShow[0] && b->mx < b->menuShow[0] + b->menuShow[2] && b->my >= b->menuShow[1] && b->my < b->menuShow[1] + b->menuShow[3];
    if (!menu_context(b)) return;
    bool shown = inMenu || b->menuVisible;
    for (int i = 0; i < b->buttonCount; i++) {
        if (!shown && !always_shown(&b->buttons[i])) continue;
        if (map_disabled(b, &b->buttons[i])) continue;
        Item it = button_item(&b->buttons[i]);
        if (button_enabled(b, &b->buttons[i]) && inside(&it, b->mx, b->my, b->menuX, b->menuY)) b->buttonHover = i;
    }
    if (b->click && b->buttonHover >= 0) {
        b->click = false;
        bx_snd_play(b, CH_SYS, RX_SYS, "check", false, 0);
        button_press(b, b->buttons[b->buttonHover].key);
    }
}

static void draw_date(BX *b, u32 *dst, const char *sec, int ox, int oy);

void bx_ui_draw(BX *b, u32 *dst) {
    bool inMenu = b->mx >= b->menuShow[0] && b->mx < b->menuShow[0] + b->menuShow[2] && b->my >= b->menuShow[1] && b->my < b->menuShow[1] + b->menuShow[3];
    if (!menu_context(b) || b->transActive) return;
    bool menu = inMenu || b->menuVisible;
    if (menu && b->menuBase) bx_blit(dst, b->W, b->H, b->menuBase, b->menuX, b->menuY, 255);
    if (menu) {
        struct {
            const char *key;
            bool on;
        } lamps[] = {{"autoex pos", b->autoMode}, {"cut-in pos", b->cutIn}, {"hold pos", b->menuVisible}};
        for (int i = 0; i < 3 && b->lamp; i++) {
            int p[2] = {0, 0};
            int h = b->lamp->h / 2;
            if (bx_cfg_ints(&b->cfg, "game menu", lamps[i].key, p, 2) == 2)
                bx_blit_part(dst, b->W, b->H, b->lamp, 0, lamps[i].on ? h : 0, b->lamp->w, h, b->menuX + p[0], b->menuY + p[1], 255);
        }
    }
    for (int i = 0; i < b->buttonCount; i++) {
        if (!button_enabled(b, &b->buttons[i]) && !always_shown(&b->buttons[i])) continue;
        if (!menu && !always_shown(&b->buttons[i])) continue;
        Item it = button_item(&b->buttons[i]);
        bool on = (!strcmp(b->buttons[i].key, "skip") && b->skipMode) || (!strcmp(b->buttons[i].key, "auto ex") && b->autoMode) ||
                  (view_button(b->buttons[i].key) && bx_view_level(b) == view_button(b->buttons[i].key));
        draw_item(b, dst, &it, map_disabled(b, &b->buttons[i]) ? 3 : i == b->buttonHover ? 1 : on ? 2 : 0, b->menuX, b->menuY);
    }
    if (!menu) return;
    draw_date(b, dst, "game menu", 0, 0);
}

static void draw_date(BX *b, u32 *dst, const char *sec, int ox, int oy) {
    const char *date = bx_cfg_get(&b->cfg, sec, "date");
    int v = date ? bx_var_by_name(b, date) : -1;
    if (v < 0) return;
    int raw = b->globals[v].v, border = bx_cfg_int(&b->cfg, sec, "day sp border", 0);
    int day = raw + bx_cfg_int(&b->cfg, sec, "day adjustment", 0);
    bool sp = border > 0 && raw >= border;
    if (day < 0 && !sp) return;
    struct {
        const char *key;
        int frame;
    } info[] = {{"week no", day / 7}, {"week day", day % 7}, {"day sp", raw - border}};
    for (int i = sp ? 2 : 0; i < (sp ? 3 : 2); i++) {
        const char *s = bx_cfg_get(&b->cfg, sec, info[i].key);
        char f[64];
        if (!s || bx_cfg_field(s, 0, f, sizeof f) <= 0) continue;
        Image *img = bx_image_load(b, f);
        if (!img) continue;
        int frames = bx_cfg_field(s, 1, f, sizeof f) > 0 ? bx_eval(f, NULL) : 1;
        int x = bx_cfg_field(s, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
        int y = bx_cfg_field(s, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
        if (frames < 1) frames = 1;
        int h = img->h / frames;
        bx_blit_part(dst, b->W, b->H, img, 0, clampi(info[i].frame, 0, frames - 1) * h, img->w, h, ox + x, oy + y, 255);
    }
}

void bx_info_draw(BX *b, u32 *dst) {
    if (!b->infoLayer || !bx_cfg_get(&b->cfg, "information window", "date")) return;
    int pos[2] = {0, 0};
    bx_cfg_ints(&b->cfg, "information window", "base pos", pos, 2);
    Image *base = bx_image_load(b, bx_cfg_get(&b->cfg, "information window", "base"));
    if (base) bx_blit(dst, b->W, b->H, base, pos[0], pos[1], 255);
    draw_date(b, dst, "information window", pos[0], pos[1]);
}

static bool bg_name(const char *s) {
    if (strlen(s) != 8 || (s[0] != 'g' && s[0] != 'G') || (s[1] != 'b' && s[1] != 'B')) return false;
    for (int i = 2; i < 7; i++)
        if (s[i] < '0' || s[i] > '9') return false;
    return s[7] >= 'a' && s[7] <= 'z';
}

static void random_bg(BX *b) {
    ResIndex *x = &b->arc.ix[RX_GFX];
    int count = 0;
    char key[64];
    for (int i = 0; i < x->count; i++) {
        const char *base = strrchr(x->e[i].name, '/');
        snprintf(key, sizeof key, "%s", base ? base + 1 : x->e[i].name);
        char *dot = strchr(key, '.');
        if (dot) *dot = 0;
        count += bg_name(key);
    }
    if (!count) return;
    b->rng = b->rng * 1103515245u + 12345u;
    int pick = (int)((b->rng >> 8) % (u32)count);
    for (int i = 0; i < x->count; i++) {
        const char *base = strrchr(x->e[i].name, '/');
        snprintf(key, sizeof key, "%s", base ? base + 1 : x->e[i].name);
        char *dot = strchr(key, '.');
        if (dot) *dot = 0;
        if (bg_name(key) && pick-- == 0) {
            bx_obj_put(b, OK_BG, 0, key, 0, 0);
            return;
        }
    }
}

static int object_hit(BX *b) {
    int best = -1, bestSeq = -1;
    for (int i = 0; i < BX_OBJS; i++) {
        Obj *o = &b->shown.obj[i];
        if (!o->used || o->kind != OK_OBJ) continue;
        Image *im = bx_image_load(b, o->name);
        if (!im) continue;
        int fh = bx_cfg_get(&b->cfg, "button object", o->name) ? im->h / 2 : im->h;
        int lx = b->mx - (im->x + o->x + o->kx), ly = b->my - (im->y + o->y + o->ky);
        if (lx < 0 || ly < 0 || lx >= im->w || ly >= fh) continue;
        if (!(im->px[(size_t)ly * im->w + lx] >> 24)) continue;
        if (o->seq > bestSeq) {
            best = i;
            bestSeq = o->seq;
        }
    }
    return best;
}

static void custom_map_update(BX *b) {
    int hit = object_hit(b);
    int hover = hit >= 0 ? b->shown.obj[hit].id : 0;
    if (hover != b->objHover) {
        b->objHover = hover;
        b->shownDirty = true;
    }
    if (!b->click || hit < 0) return;
    b->click = false;
    Obj *o = &b->shown.obj[hit];
    const char *res = bx_cfg_get(&b->cfg, "custom map phase", "result");
    const char *sel = bx_cfg_get(&b->cfg, "custom map phase", "select object");
    int value = bx_cfg_int(&b->cfg, "button object", o->name, o->id);
    int v = res ? bx_var_by_name(b, res) : -1;
    if (v >= 0) b->globals[v] = (Val){0, value};
    v = sel ? bx_var_by_name(b, sel) : -1;
    if (v >= 0) b->globals[v] = (Val){0, o->id};
    bx_snd_play(b, CH_SYS, RX_SYS, "done", false, 0);
    b->objHover = 0;
    b->shownDirty = true;
    bx_phase_end(b);
}

void bx_system(BX *b, const char *cmd) {
    char var[64], kind[16];
    int level;
    if (!strcmp(cmd, "title")) bx_phase_start(b, PH_TITLE);
    else if (!strcmp(cmd, "map")) bx_phase_start(b, PH_MAP);
    else if (!strcmp(cmd, "custom map")) bx_phase_start(b, PH_CUSTOM_MAP);
    else if (!strncmp(cmd, "実績解除 ", strlen("実績解除 "))) bx_achieve(b, cmd + strlen("実績解除 "));
    else if (sscanf(cmd, "達成率 %63[^,],%15s", var, kind) == 2) {
        int v = bx_var_by_name(b, var);
        if (v >= 0) b->globals[v] = (Val){0, bx_museum_rate(b, kind)};
    }
    else if (!strcmp(cmd, "op") || !strcmp(cmd, "staffroll")) {
        if (bx_movie_play(b, cmd)) b->th.wait = W_MOVIE;
    }
    else if (!strcmp(cmd, "random bg")) random_bg(b);
    else if (sscanf(cmd, "is animeted %63s , %d", var, &level) == 2) {
        int v = bx_var_by_name(b, var);
        if (v >= 0) b->globals[v] = (Val){0, !b->skipMode};
    }
    else if (!strcmp(cmd, "@title") || !strcmp(cmd, "@end")) {
        bx_snd_stop(b, CH_BGM, 500);
        bx_snd_loop_stop(b, CH_BGS, NULL, 0);
        bx_snd_loop_stop(b, CH_BGV, NULL, 0);
        b->bgmName[0] = 0;
        memset(&b->scene, 0, sizeof b->scene);
        bx_transition(b, 0, 0, NULL);
        bx_msg_clear(b);
        b->choiceCount = 0;
        b->th.sp = 0;
        b->th.wait = W_NONE;
        int l = bx_script_label(b->sc, cmd[1] == 't' ? "title" : "boot_logo");
        if (l < 0) l = bx_script_label(b->sc, "boot");
        if (l < 0) l = bx_script_label(b->sc, "title");
        bx_vm_goto(b, l >= 0 ? l : 0);
    } else if (!strcmp(cmd, "dress_on")) b->dressEnabled = true;
    else if (!strcmp(cmd, "dress_off")) b->dressEnabled = false;
    else if (!strcmp(cmd, "view_on")) b->viewEnabled = true;
    else if (!strcmp(cmd, "view_off")) b->viewEnabled = false;
    else bx_log(b, "system command '%s' not supported", cmd);
}

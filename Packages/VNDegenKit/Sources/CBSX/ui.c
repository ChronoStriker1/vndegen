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
    phase_free(b);
    b->phase = PH_NONE;
    if (b->th.wait == W_PHASE) b->th.wait = W_NONE;
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
    else if (phase == PH_MAP && !bx_map_start(b)) bx_phase_end(b);
}

static void title_pick(BX *b, const char *key) {
    if (!strcmp(key, "quit")) {
        b->quit = true;
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
        bx_snd_play(b, CH_SYS, RX_SYS, "click", false, 0);
        if (b->phase == PH_TITLE) title_pick(b, p->items[p->hover].key);
    }
}

void bx_phase_draw(BX *b, u32 *dst) {
    if (b->phase == PH_MAP) {
        bx_map_draw(b, dst);
        return;
    }
    Phase *p = b->ph;
    if (!p) return;
    if (p->base) bx_blit(dst, b->W, b->H, p->base, p->base->x, p->base->y, 255);
    for (int i = 0; i < p->count; i++) draw_item(b, dst, &p->items[i], i == p->hover ? 1 : 0, 0, 0);
}

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
    for (int i = 0; i < c->count && b->buttonCount < 24; i++) {
        if (strcmp(c->sec[i], "system button")) continue;
        char group[32];
        if (bx_cfg_field(c->val[i], 4, group, sizeof group) <= 0 || strcmp(group, "game")) continue;
        Item it;
        if (!button_spec(b, "system button", c->key[i], &it)) continue;
        Button *bt = &b->buttons[b->buttonCount++];
        snprintf(bt->key, sizeof bt->key, "%s", it.key);
        bt->img = it.img;
        bt->frames = it.frames;
        bt->x = it.x;
        bt->y = it.y;
    }
    b->buttonHover = -1;
}

void bx_ui_free(BX *b) {
    phase_free(b);
    bx_map_free(b);
    bx_image_free(b->menuBase);
    for (int i = 0; i < b->buttonCount; i++) bx_image_free(b->buttons[i].img);
    bx_image_free(b->msgBase);
    bx_image_free(b->msgFrame);
    bx_image_free(b->msgName);
    bx_image_free(b->unagashi);
    bx_image_free(b->choiceItem[0]);
    bx_image_free(b->choiceItem[1]);
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

static void button_press(BX *b, const char *key) {
    if (!strcmp(key, "skip")) b->skipMode = !b->skipMode;
    else if (!strcmp(key, "auto ex")) b->autoMode = !b->autoMode;
    else if (!strcmp(key, "voice")) {
        if (b->voice) bx_snd_play(b, CH_VOICE, RX_VOICE, b->voice, false, 0);
    } else if (!strcmp(key, "hold menu")) b->menuVisible = !b->menuVisible;
    else bx_log(b, "menu button %s isn't available yet", key);
}

void bx_ui_update(BX *b) {
    b->buttonHover = -1;
    bool inMenu = b->mx >= b->menuShow[0] && b->mx < b->menuShow[0] + b->menuShow[2] && b->my >= b->menuShow[1] && b->my < b->menuShow[1] + b->menuShow[3];
    bool shown = b->winShown && !b->winHidden && (inMenu || b->menuVisible) && b->th.wait == W_TEXT;
    if (!shown) return;
    for (int i = 0; i < b->buttonCount; i++) {
        Item it = button_item(&b->buttons[i]);
        if (button_enabled(b, &b->buttons[i]) && inside(&it, b->mx, b->my, b->menuX, b->menuY)) b->buttonHover = i;
    }
    if (b->click && b->buttonHover >= 0) {
        b->click = false;
        bx_snd_play(b, CH_SYS, RX_SYS, "click", false, 0);
        button_press(b, b->buttons[b->buttonHover].key);
    }
}

void bx_ui_draw(BX *b, u32 *dst) {
    bool inMenu = b->mx >= b->menuShow[0] && b->mx < b->menuShow[0] + b->menuShow[2] && b->my >= b->menuShow[1] && b->my < b->menuShow[1] + b->menuShow[3];
    if (!b->winShown || b->winHidden || b->transActive || b->th.wait != W_TEXT || !(inMenu || b->menuVisible)) return;
    if (b->menuBase) bx_blit(dst, b->W, b->H, b->menuBase, b->menuX, b->menuY, 255);
    for (int i = 0; i < b->buttonCount; i++) {
        if (!button_enabled(b, &b->buttons[i])) continue;
        Item it = button_item(&b->buttons[i]);
        bool on = (!strcmp(b->buttons[i].key, "skip") && b->skipMode) || (!strcmp(b->buttons[i].key, "auto ex") && b->autoMode);
        draw_item(b, dst, &it, i == b->buttonHover ? 1 : on ? 2 : 0, b->menuX, b->menuY);
    }
}

void bx_system(BX *b, const char *cmd) {
    char var[64];
    int level;
    if (!strcmp(cmd, "title")) bx_phase_start(b, PH_TITLE);
    else if (!strcmp(cmd, "map")) bx_phase_start(b, PH_MAP);
    else if (sscanf(cmd, "is animeted %63s , %d", var, &level) == 2) {
        int v = bx_var_by_name(b, var);
        if (v >= 0) b->globals[v] = (Val){0, !b->skipMode};
    }
    else if (!strcmp(cmd, "@end")) {
        int l = bx_script_label(b->sc, "boot_logo");
        b->th.sp = 0;
        b->th.wait = W_NONE;
        bx_vm_goto(b, l >= 0 ? l : 0);
    } else if (!strcmp(cmd, "dress_on")) b->dressEnabled = true;
    else if (!strcmp(cmd, "dress_off")) b->dressEnabled = false;
    else if (!strcmp(cmd, "view_on")) b->viewEnabled = true;
    else if (!strcmp(cmd, "view_off")) b->viewEnabled = false;
    else bx_log(b, "system command '%s' not supported", cmd);
}

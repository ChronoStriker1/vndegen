#include "sg.h"

static const int kTitlePx = 23;

bool sg_pressed(SG *g, u32 key) {
    if (!(g->pressed & key)) return false;
    g->pressed &= ~key;
    return true;
}

bool sg_local_switch(SG *g, Guid event, int index) {
    if (index < 0 || index >= 4) return false;
    for (int i = 0; i < g->localCount; i++)
        if (guid_eq(g->locals[i].event, event)) return g->locals[i].on[index];
    return false;
}

static u32 key_bit(int vk) {
    switch (vk) {
    case 0x26: case 'W': return KEY_UP;
    case 0x28: case 'S': return KEY_DOWN;
    case 0x25: case 'A': return KEY_LEFT;
    case 0x27: case 'D': return KEY_RIGHT;
    case 0x0d: case 0x20: case 'Z': return KEY_OK;
    case 0x1b: case 'X': return KEY_CANCEL;
    case 0x10: return KEY_DASH;
    default: return 0;
    }
}

void vnsmilegame_key(VNSmileGame *vg, int vk, bool down) {
    SG *g = (SG *)vg;
    u32 bit = key_bit(vk);
    if (!bit) return;
    if (down) {
        if (!(g->keys & bit)) g->pressed |= bit;
        g->keys |= bit;
    } else {
        g->keys &= ~bit;
    }
}

VNSmileGame *vnsmilegame_open(const VNSmileGameConfig *cfg, char *err, size_t errSize) {
    SG *g = sg_alloc(sizeof *g);
    g->cfg = *cfg;
    g->gameDir = sg_strdup(cfg->gameDir);
    g->saveDir = cfg->saveDir ? sg_strdup(cfg->saveDir) : NULL;
    if (cfg->logPath) g->log = fopen(cfg->logPath, "w");
    if (!game_load(g, err, errSize)) {
        vnsmilegame_close((VNSmileGame *)g);
        return NULL;
    }
    res_load(g);
    size_t fontSize = 0;
    u8 *fontData = pack_read(&g->data.pack, "font.ttf", &fontSize);
    if (fontData) g->font = font_load(fontData, fontSize);
    free(fontData);
    g->w = g->data.settings.screenW;
    g->h = g->data.settings.screenH;
    g->fb = sg_alloc(sizeof(u32) * (size_t)g->w * (size_t)g->h);
    r3_init(&g->r3, g->w, g->h);
    g->scene = SCENE_TITLE;
    sg_log(g, "title: %s, screen %dx%d, font %s", g->data.settings.name, g->w, g->h, g->font ? "game" : "none");
    return (VNSmileGame *)g;
}

void vnsmilegame_close(VNSmileGame *vg) {
    SG *g = (SG *)vg;
    if (!g) return;
    interp_free(&g->interp);
    field_free(&g->field);
    model_free(g->field.player.model);
    for (int i = 0; i < g->imageCacheCount; i++) img_free(g->imageCache[i].im);
    free(g->imageCache);
    free(g->locals);
    font_free(g->font);
    res_free(&g->res);
    game_free(&g->data);
    r3_free(&g->r3);
    free(g->fb);
    free(g->gameDir);
    free(g->saveDir);
    if (g->log) fclose(g->log);
    free(g);
}

bool vnsmilegame_probe(const char *gameDir, char *title, size_t titleSize) {
    SG g = {0};
    g.gameDir = (char *)gameDir;
    char err[256];
    bool ok = game_load(&g, err, sizeof err);
    if (ok && title && titleSize) snprintf(title, titleSize, "%s", g.data.settings.name ? g.data.settings.name : "");
    game_free(&g.data);
    return ok;
}

static void start_new_game(SG *g) {
    const Settings *s = &g->data.settings;
    memset(g->vars, 0, sizeof g->vars);
    memset(g->switches, 0, sizeof g->switches);
    g->scene = SCENE_FIELD;
    sg_log(g, "new game");
    field_load(g, s->startMap, s->startX, s->startZ, 2);
}

static void title_update(SG *g) {
    if (sg_pressed(g, KEY_DOWN) || sg_pressed(g, KEY_RIGHT)) g->titleCursor = (g->titleCursor + 1) % 3;
    if (sg_pressed(g, KEY_UP) || sg_pressed(g, KEY_LEFT)) g->titleCursor = (g->titleCursor + 2) % 3;
    if (sg_pressed(g, KEY_OK) && g->titleCursor == 0) start_new_game(g);
}

void text_outlined(SG *g, int x, int y, int px, const char *s, u32 color) {
    static const int ox[8] = {-1, 0, 1, -1, 1, -1, 0, 1}, oy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    for (int i = 0; i < 8; i++) font_draw(g->font, g->fb, g->w, g->h, x + ox[i], y + oy[i], px, s, 0x60000000u);
    font_draw(g->font, g->fb, g->w, g->h, x, y, px, s, color);
}

static void title_draw(SG *g) {
    const Settings *s = &g->data.settings;
    for (size_t i = 0; i < (size_t)g->w * (size_t)g->h; i++) g->fb[i] = 0xff000000u;
    Image *bg = sg_image(g, s->titleImage);
    if (bg) blit(g->fb, g->w, g->h, bg, 0, 0, bg->w, bg->h, (g->w - bg->w) / 2, (g->h - bg->h) / 2, bg->w, bg->h, 255);
    Image *over = sg_image(g, s->titleOverlay);
    if (over) blit(g->fb, g->w, g->h, over, 0, 0, over->w, over->h, (g->w - over->w) / 2, (g->h - over->h) / 2, over->w, over->h, 255);
    Image *cursor = sg_image(g, s->titleCursor);
    for (int i = 0; i < 3; i++) {
        const char *item = s->titleItems[i] ? s->titleItems[i] : "";
        int tw = font_width(g->font, kTitlePx, item);
        int x, y;
        if (s->titleLayout == 1) {
            x = g->w * 582 / 960 + (i - 1) * 152 - tw / 2;
            y = g->h - 106;
        } else {
            x = (g->w - tw) / 2;
            y = g->h - 160 + i * 54;
        }
        u32 color = i == 1 ? 0xff808080u : s->titleColor;
        text_outlined(g, x, y, kTitlePx, item, color);
        if (i == g->titleCursor) {
            fill_rect(g->fb, g->w, g->h, x - 17, y + 5, 7, 14, 0xffffffffu);
            (void)cursor;
            fill_rect(g->fb, g->w, g->h, x - 3, y + 39, tw + 11, 2, 0xffffffffu);
        }
    }
}

bool vnsmilegame_frame(VNSmileGame *vg, uint32_t ms) {
    SG *g = (SG *)vg;
    g->clockMs += ms;
    if (g->scene == SCENE_TITLE) {
        title_update(g);
        title_draw(g);
    } else {
        interp_update(g, ms);
        if (g->pendingTransfer) {
            g->pendingTransfer = false;
            field_load(g, g->pendingMap, g->pendingX, g->pendingZ, g->pendingDir);
        }
        if (g->fadeTotal) {
            g->fadeMs += ms;
            float t = g->fadeMs >= g->fadeTotal ? 1.0f : (float)g->fadeMs / (float)g->fadeTotal;
            g->fadeLevel = g->fadeFrom + (g->fadeTo - g->fadeFrom) * t;
            if (t >= 1.0f) g->fadeTotal = 0;
        }
        if (!g->interp.active && sg_pressed(g, KEY_OK)) {
            Field *f = &g->field;
            static const int dx[4] = {0, 1, 0, -1}, dz[4] = {-1, 0, 1, 0};
            int tx = (int)f->player.x + dx[f->player.dir & 3], tz = (int)f->player.z + dz[f->player.dir & 3];
            for (int i = 0; i < f->npcCount; i++) {
                if ((int)f->npcs[i].x != tx || (int)f->npcs[i].z != tz) continue;
                int sheet = event_sheet(g, &f->events[i]);
                if (sheet < 0) continue;
                const SgrRec *sr = game_record(&g->data, SGR_EVENT, 16, f->events[i].sheets[sheet].script, NULL);
                Script s;
                if (sr && script_parse(&s, sr)) {
                    sg_log(g, "talk to %s", f->events[i].name);
                    interp_start(g, &s);
                    script_free(&s);
                }
                break;
            }
        }
        field_update(g, ms);
        field_draw(g);
        if (g->fadeLevel > 0) {
            u32 a = (u32)(g->fadeLevel * 255.0f + 0.5f);
            fill_rect(g->fb, g->w, g->h, 0, 0, g->w, g->h, a << 24);
        }
        interp_draw(g);
    }
    g->pressed = 0;
    g->serial++;
    return true;
}

const uint32_t *vnsmilegame_pixels(VNSmileGame *vg, int *w, int *h) {
    SG *g = (SG *)vg;
    *w = g->w;
    *h = g->h;
    return g->fb;
}

uint32_t vnsmilegame_frame_serial(VNSmileGame *vg) { return ((SG *)vg)->serial; }

void vnsmilegame_audio(VNSmileGame *vg, float *out, int frames) {
    (void)vg;
    memset(out, 0, sizeof(float) * (size_t)frames * 2);
}

const char *vnsmilegame_title(VNSmileGame *vg) { return ((SG *)vg)->data.settings.name; }

const char *vnsmilegame_problem(VNSmileGame *vg) {
    SG *g = (SG *)vg;
    return g->problem[0] ? g->problem : NULL;
}

const char *vnsmilegame_scene(VNSmileGame *vg) {
    SG *g = (SG *)vg;
    if (g->scene == SCENE_TITLE) return "title";
    if (g->interp.active) return g->interp.state == 1 ? "message" : g->interp.state == 2 ? "scroll" : "event";
    return "field";
}

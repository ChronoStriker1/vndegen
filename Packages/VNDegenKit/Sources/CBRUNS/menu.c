#include "br.h"

typedef struct MenuItem {
    int x, y, parts;
    const char *event;
} MenuItem;

static const struct {
    const char *event;
    int script;
} scripts[] = {
    {"qsave", 9014}, {"qload", 9015}, {"load", 9002}, {"save", 9006}, {"flowmode", 9017},
    {"backlog", 9004}, {"config", 9003}, {"restart", 9016}, {"quit", 9001},
};

static Node *entry(BR *e) { return sx_assoc(sx_find(sx_find(e->cfg.root, ":menu"), "entryMap"), 0); }

static int items(BR *e, MenuItem *out, int max) {
    Node *en = entry(e);
    int n = 0;
    for (int i = 0; i < 16 && n < max; i++) {
        char k[32];
        snprintf(k, sizeof k, "cp-%d-parts", i);
        Node *p = sx_cdr_value(sx_find(en, k));
        if (!p) continue;
        snprintf(k, sizeof k, "cp-%d-offset-x", i);
        out[n].x = sx_int(sx_cdr_value(sx_find(en, k)), 0);
        snprintf(k, sizeof k, "cp-%d-offset-y", i);
        out[n].y = sx_int(sx_cdr_value(sx_find(en, k)), 0);
        snprintf(k, sizeof k, "cp-%d-event", i);
        out[n].event = sx_str(sx_cdr_value(sx_find(en, k)));
        out[n].parts = p->i;
        n++;
    }
    return n;
}

static int width(BR *e) { return sx_int(sx_cdr_value(sx_find(entry(e), "menu-width")), 160); }

static int sensor(BR *e) { return sx_int(sx_cdr_value(sx_find(entry(e), "sensor-width")), 16); }

static Image *bg(BR *e) {
    Node *u = sx_cdr_value(sx_find(entry(e), "bg-imageUrl"));
    return u ? image_url(e, u) : NULL;
}

bool menu_allowed(BR *e) { return !e->systemMode && e->wait == W_TEXT && (e->menu.show || e->menu.pinned); }

static bool hit_item(BR *e, MenuItem *it, int ox) {
    Parts *p = parts_get(e, it->parts);
    Image *im = parts_image(e, p, 0, e->now);
    if (!im) return false;
    int x = ox + it->x, y = it->y;
    bool hit = e->mouseX >= x && e->mouseY >= y && e->mouseX < x + im->w && e->mouseY < y + im->h;
    image_free(im);
    return hit;
}

static void run_event(BR *e, const char *ev) {
    if (!ev) return;
    snd_se_click(e);
    if (!strcmp(ev, "skipmsg")) {
        e->skipMode = !e->skipMode;
        e->autoMode = false;
        return;
    }
    if (!strcmp(ev, "automsg")) {
        e->autoMode = !e->autoMode;
        e->skipMode = false;
        return;
    }
    for (size_t i = 0; i < sizeof scripts / sizeof *scripts; i++)
        if (!strcmp(scripts[i].event, ev)) {
            e->menu.open = false;
            sys_call(e, scripts[i].script);
            return;
        }
}

void menu_event(BR *e, const char *ev) {
    if (!e->systemMode && e->wait == W_TEXT) run_event(e, ev);
}

bool menu_input(BR *e) {
    if (!menu_allowed(e)) {
        e->menu.open = false;
        return false;
    }
    int w = width(e), ox = BR_W - w;
    Image *b = bg(e);
    int h = b ? b->h : BR_H;
    image_free(b);
    bool inBar = e->mouseX >= ox && e->mouseY < h;
    if (e->mouseX >= BR_W - sensor(e)) e->menu.open = true;
    else if (!inBar) e->menu.open = false;
    bool visible = e->menu.pinned || !e->menu.autohide || e->menu.open;
    if (!visible) return false;
    MenuItem it[16];
    int n = items(e, it, 16);
    e->menu.hover = -1;
    for (int i = 0; i < n; i++)
        if (hit_item(e, &it[i], ox)) e->menu.hover = i;
    if (e->menu.hover >= 0 && e->lPress) {
        e->lPress = false;
        run_event(e, it[e->menu.hover].event);
        return true;
    }
    return inBar && e->lPress;
}

void menu_draw(BR *e, u32 *dst) {
    if (!menu_allowed(e)) return;
    bool visible = e->menu.pinned || !e->menu.autohide || e->menu.open;
    if (!visible) return;
    int ox = BR_W - width(e);
    Image *b = bg(e);
    if (b) {
        blit(dst, BR_W, BR_H, b, 0, 0, -1, -1, ox, 0, 255);
        image_free(b);
    }
    MenuItem it[16];
    int n = items(e, it, 16);
    for (int i = 0; i < n; i++) {
        Parts *p = parts_get(e, it[i].parts);
        bool on = (!strcmp(it[i].event, "skipmsg") && e->skipMode) || (!strcmp(it[i].event, "automsg") && e->autoMode);
        int state = i == e->menu.hover ? (e->lDown ? 2 : 1) : on ? 2 : 0;
        Image *im = parts_image(e, p, state, e->now);
        if (!im) continue;
        blit(dst, BR_W, BR_H, im, 0, 0, -1, -1, ox + it[i].x, it[i].y, 255);
        image_free(im);
    }
}

void menu_builtin(BR *e, int id, int a, int b) {
    if (id == 2043) {
        if (a == 0) e->menu.show = b != 0;
        else {
            e->menu.autohide = b != 0;
            e->menu.pinned = false;
        }
    } else if (id == 2044) {
        if (a == 1) e->menu.pinned = b != 0;
    }
}

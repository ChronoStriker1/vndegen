#include "bsx.h"

typedef struct Slot {
    Image *img;
    int x, y;
    char var[64];
    char op[3];
    int value;
} Slot;

static Image *own(BX *b, const char *name) {
    Image *im = name && *name ? bx_image_load(b, name) : NULL;
    if (!im) return NULL;
    Image *c = bx_image_new(im->w, im->h);
    c->x = im->x;
    c->y = im->y;
    memcpy(c->px, im->px, sizeof(u32) * (size_t)im->w * (size_t)im->h);
    return c;
}

static bool quoted(const char *s, const char *key, char *out, size_t size, const char **after) {
    const char *p = strstr(s, key);
    if (!p) return false;
    p = strchr(p + strlen(key), '"');
    if (!p) return false;
    const char *e = strchr(p + 1, '"');
    if (!e) return false;
    size_t n = (size_t)(e - p - 1) < size - 1 ? (size_t)(e - p - 1) : size - 1;
    memcpy(out, p + 1, n);
    out[n] = 0;
    if (after) *after = e + 1;
    return true;
}

static bool pos(const char *s, int *x, int *y) {
    const char *p = strstr(s, "pos:");
    if (!p || !(p = strchr(p, '['))) return false;
    const char *e;
    *x = bx_eval(p + 1, &e);
    if (*e != ',') return false;
    *y = bx_eval(e + 1, NULL);
    return true;
}

typedef struct Achievement {
    char id[16];
    char *name, *notes;
    bool secret;
} Achievement;

struct StatusPhase {
    Image *base, *inner, *back, *bar;
    int backStates, backX, backY;
    Slot slots[32];
    int count;
    Achievement *ach;
    int achCount, top, rows, fontSize, textDY;
    int barX, barY, nameX, nameY, notesX, notesY, step;
    u32 lockColor, unlockColor;
};

static int field_int(const char *s, const char *key, int def) {
    const char *p = strstr(s, key);
    return p ? bx_eval(p + strlen(key), NULL) : def;
}

static const char *block(const char *s, const char *key, char *out, size_t size) {
    const char *p = strstr(s, key);
    const char *o = p ? strchr(p, '{') : NULL;
    const char *e = o ? strchr(o, '}') : NULL;
    if (!e) return NULL;
    size_t n = (size_t)(e - o) < size - 1 ? (size_t)(e - o) : size - 1;
    memcpy(out, o, n);
    out[n] = 0;
    return out;
}

static char *quoted_dup(const char *s, const char *key) {
    const char *p = strstr(s, key);
    if (!p || !(p = strchr(p + strlen(key), '"'))) return bx_strdup("");
    const char *e = strchr(p + 1, '"');
    if (!e) return bx_strdup("");
    char *r = bx_alloc((size_t)(e - p));
    memcpy(r, p + 1, (size_t)(e - p - 1));
    return r;
}

static void load_achievements(BX *b, StatusPhase *s, const char *layout) {
    const char *db = bx_cfg_get(&b->cfg, "main window", "achievement db");
    char name[64];
    size_t n;
    snprintf(name, sizeof name, "%s.db", db ? db : "achievement");
    u8 *d = bx_res_read(&b->arc, RX_DEF, name, &n);
    if (!d) d = bx_res_read(&b->arc, RX_DEF, db ? db : "achievement", &n);
    if (!d) return;
    char *text = bx_alloc(n + 1);
    memcpy(text, d, n);
    free(d);
    for (const char *p = strstr(text, "\"ID"); p; p = strstr(p + 1, "\"ID")) {
        const char *e = strchr(p + 1, '"'), *o = e ? strchr(e, '{') : NULL, *c = o ? strchr(o, '}') : NULL;
        if (!c || (size_t)(e - p - 1) >= sizeof s->ach->id) break;
        s->ach = bx_realloc(s->ach, sizeof(Achievement) * (size_t)(s->achCount + 1));
        Achievement *a = &s->ach[s->achCount++];
        memset(a, 0, sizeof *a);
        memcpy(a->id, p + 1, (size_t)(e - p - 1));
        char *entry = bx_alloc((size_t)(c - o) + 2);
        memcpy(entry, o, (size_t)(c - o) + 1);
        const char *sec = strstr(entry, "\"secret\"");
        a->secret = sec && strstr(sec, "TRUE") && (!strstr(sec, "FALSE") || strstr(sec, "TRUE") < strstr(sec, "FALSE"));
        a->name = quoted_dup(entry, "\"name\":");
        a->notes = quoted_dup(entry, "\"notes\":");
        free(entry);
        p = c;
    }
    free(text);
    char part[512];
    int points = block(layout, "font", part, sizeof part) ? field_int(part, "size:", 17) : 17;
    s->fontSize = points * 4 / 3;
    s->textDY = s->fontSize - points;
    if (block(layout, "\"scroll bar\"", part, sizeof part)) {
        char bar[64];
        if (quoted(part, "name:", bar, sizeof bar, NULL)) s->bar = own(b, bar);
        s->barX = field_int(part, "x:", 0);
        s->barY = field_int(part, "y:", 0);
        s->rows = field_int(part, "max_no:", 21);
    }
    if (block(layout, "name", part, sizeof part)) {
        s->nameX = field_int(part, "x:", 8);
        s->nameY = field_int(part, "y:", 130);
    }
    if (block(layout, "notes", part, sizeof part)) {
        s->notesX = field_int(part, "x:", 340);
        s->notesY = field_int(part, "y:", 130);
    }
    const char *iv = strstr(layout, "interval");
    s->step = iv ? field_int(iv, "y:", 30) : 30;
    if (block(layout, "color", part, sizeof part)) {
        const char *l = strstr(part, "lock:"), *u = strstr(part, "unlock:");
        u32 lock = l ? (u32)strtoul(l + 5, NULL, 0) : 0xff606060u, unlock = u ? (u32)strtoul(u + 7, NULL, 0) : 0xffffffffu;
        s->lockColor = (lock & 0xff00ff00u) | (lock >> 16 & 0xff) | (lock & 0xff) << 16;
        s->unlockColor = (unlock & 0xff00ff00u) | (unlock >> 16 & 0xff) | (unlock & 0xff) << 16;
    }
    if (s->rows < 1) s->rows = 21;
}

void bx_status_free(BX *b) {
    StatusPhase *s = b->status;
    if (!s) return;
    bx_image_free(s->base);
    bx_image_free(s->inner);
    bx_image_free(s->back);
    bx_image_free(s->bar);
    for (int i = 0; i < s->count; i++) bx_image_free(s->slots[i].img);
    for (int i = 0; i < s->achCount; i++) {
        free(s->ach[i].name);
        free(s->ach[i].notes);
    }
    free(s->ach);
    free(s);
    b->status = NULL;
}

bool bx_status_open(BX *b) {
    const char *layout = bx_cfg_get(&b->cfg, "status screen", "layout");
    size_t n;
    u8 *d = layout ? bx_res_read(&b->arc, RX_DEF, layout, &n) : NULL;
    if (!d) {
        bx_log(b, "no status layout");
        return false;
    }
    bx_status_free(b);
    StatusPhase *s = bx_alloc(sizeof *s);
    b->status = s;
    char *text = bx_alloc(n + 1);
    memcpy(text, d, n);
    free(d);
    char name[64];
    if (quoted(text, "base:", name, sizeof name, NULL)) s->base = own(b, name);
    const char *mb = strstr(text, "\"mode button\"");
    if (mb && quoted(mb, "name:", name, sizeof name, NULL)) {
        s->back = own(b, name);
        const char *st = strstr(mb, "max_state:");
        s->backStates = st ? atoi(st + 10) : 1;
        if (s->backStates < 1) s->backStates = 1;
        pos(mb, &s->backX, &s->backY);
    }
    const char *achLayout = strstr(text, "achievement:");
    if (achLayout) load_achievements(b, s, achLayout);
    const char *inv = achLayout ? NULL : strstr(text, "layout:");
    if (inv && quoted(inv, "base:", name, sizeof name, NULL)) s->inner = own(b, name);
    const char *slot = inv ? strstr(inv, "slot:") : NULL;
    for (const char *p = slot ? strchr(slot, '{') : NULL; p && s->count < 32; p = strchr(p + 1, '{')) {
        const char *end = strchr(p, '}');
        if (!end) break;
        char item[512];
        size_t len = (size_t)(end - p) < sizeof item - 1 ? (size_t)(end - p) : sizeof item - 1;
        memcpy(item, p, len);
        item[len] = 0;
        Slot *sl = &s->slots[s->count];
        char cond[128] = "";
        if (!quoted(item, "name:", name, sizeof name, NULL) || !pos(item, &sl->x, &sl->y)) continue;
        quoted(item, "condition:", cond, sizeof cond, NULL);
        sl->img = own(b, name);
        char op[3] = "";
        if (sscanf(cond, "%63s %2s %d", sl->var, op, &sl->value) == 3) snprintf(sl->op, sizeof sl->op, "%s", op);
        s->count++;
        p = end;
    }
    free(text);
    b->phaseBack = b->phase;
    b->phase = PH_STATUS;
    return true;
}

static bool met(BX *b, const Slot *s) {
    if (!s->var[0]) return true;
    int v = bx_var_by_name(b, s->var);
    i32 x = v >= 0 ? b->globals[v].v : 0;
    if (!strcmp(s->op, "!=")) return x != s->value;
    if (!strcmp(s->op, "==")) return x == s->value;
    if (!strcmp(s->op, ">=")) return x >= s->value;
    if (!strcmp(s->op, "<=")) return x <= s->value;
    if (!strcmp(s->op, ">")) return x > s->value;
    if (!strcmp(s->op, "<")) return x < s->value;
    return true;
}

static bool on_back(BX *b, StatusPhase *s) {
    if (!s->back) return false;
    int h = s->back->h / s->backStates;
    return b->mx >= s->backX && b->my >= s->backY && b->mx < s->backX + s->back->w && b->my < s->backY + h;
}

static int knob_height(const StatusPhase *s) { return s->achCount > s->rows ? s->bar->h * s->rows / s->achCount : s->bar->h; }

void bx_status_update(BX *b) {
    StatusPhase *s = b->status;
    if (!s) return;
    int maxTop = s->achCount > s->rows ? s->achCount - s->rows : 0;
    int scroll = -b->wheel * 3 + (b->keyPress[0x28] ? 1 : 0) - (b->keyPress[0x26] ? 1 : 0) + (b->keyPress[0x22] ? s->rows : 0) - (b->keyPress[0x21] ? s->rows : 0);
    b->wheel = 0;
    s->top = clampi(s->top + scroll, 0, maxTop);
    if (s->bar && b->lDown && b->mx >= s->barX && b->mx < s->barX + s->bar->w && b->my >= s->barY && maxTop > 0) {
        int track = s->bar->h - knob_height(s);
        if (track > 0) s->top = clampi((b->my - s->barY - knob_height(s) / 2) * maxTop / track, 0, maxTop);
    }
    if (b->rclick || (b->click && on_back(b, s))) {
        b->rclick = b->click = false;
        bx_status_free(b);
        b->phase = b->phaseBack;
        b->phaseBack = PH_NONE;
    }
    b->click = false;
}

void bx_status_draw(BX *b, u32 *dst) {
    StatusPhase *s = b->status;
    if (!s) return;
    if (s->base) bx_blit(dst, b->W, b->H, s->base, s->base->x, s->base->y, 255);
    if (s->inner) bx_blit(dst, b->W, b->H, s->inner, s->inner->x, s->inner->y, 255);
    for (int i = 0; i < s->count; i++)
        if (s->slots[i].img && met(b, &s->slots[i])) bx_blit(dst, b->W, b->H, s->slots[i].img, s->slots[i].x, s->slots[i].y, 255);
    for (int i = 0; i < s->rows && s->top + i < s->achCount; i++) {
        Achievement *a = &s->ach[s->top + i];
        bool on = bx_achieved(b, a->id);
        u32 color = on ? s->unlockColor : s->lockColor;
        const char *name = !on && a->secret ? "？？？" : a->name, *notes = !on && a->secret ? "？？？？？" : a->notes;
        bx_text_draw(NULL, dst, b->W, b->H, name, s->nameX, s->nameY + s->textDY + i * s->step, s->fontSize, 0, color, 0, 0);
        bx_text_draw(NULL, dst, b->W, b->H, notes, s->notesX, s->notesY + s->textDY + i * s->step, s->fontSize, 0, color, 0, 0);
    }
    if (s->bar && s->achCount > s->rows) {
        int kh = knob_height(s);
        bx_blit_part(dst, b->W, b->H, s->bar, 0, 0, s->bar->w, kh, s->barX, s->barY + (s->bar->h - kh) * s->top / (s->achCount - s->rows), 255);
    }
    if (s->back) {
        int h = s->back->h / s->backStates;
        bx_blit_part(dst, b->W, b->H, s->back, 0, on_back(b, s) && s->backStates > 1 ? h : 0, s->back->w, h, s->backX, s->backY, 255);
    }
}

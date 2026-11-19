#include "ikura.h"

#define MAX_STEPS 200000

u32 ik_value(IK *t, u32 raw) {
    if (raw & 0x80000000u) {
        u32 i = raw & 0x7fffffffu;
        return i < (u32)t->varCount ? (u32)t->vars[i] : 0;
    }
    return raw & 0x40000000u ? raw | 0x80000000u : raw;
}

static i32 *var_at(IK *t, u32 i) { return i < (u32)t->varCount ? &t->vars[i] : NULL; }

void vm_start(IK *t, const char *script) {
    Script *s = script_load(t, script);
    if (!s) {
        snprintf(t->problem, sizeof t->problem, "The script %s is missing.", script);
        ik_log(t, "%s", t->problem);
        t->wait = W_QUIT;
        return;
    }
    t->sc = s;
    t->pc = 0;
}

static void goto_label(IK *t, u32 label) {
    if (label >= (u32)t->sc->labelCount) {
        ik_log(t, "%s: label %u out of range", t->sc->name, label);
        return;
    }
    t->pc = t->sc->labels[label];
}

static void push(IK *t) {
    if (t->sp >= IK_STACK) {
        ik_log(t, "call stack full");
        return;
    }
    t->stack[t->sp++] = (Frame){t->sc, t->pc, t->wait == W_MSG};
}

static void pop(IK *t) {
    if (t->sp <= 0) {
        ik_log(t, "%s: return with an empty stack", t->sc->name);
        return;
    }
    Frame f = t->stack[--t->sp];
    t->sc = f.sc;
    t->pc = f.pc;
    if (f.resumeMsg && t->msg.tags) t->wait = W_MSG;
}

void vm_branch(IK *t, Script *sc, int label, bool call) {
    if (!sc) return;
    if (call) push(t);
    else t->msg.tags = NULL;
    t->sc = sc;
    t->wait = W_NONE;
    goto_label(t, (u32)label);
    t->dirty = true;
}

static bool flag(IK *t, u32 i) { return i < (u32)t->flagCount * 8 && (t->flags[i >> 3] >> (i & 7) & 1); }

static void set_flag(IK *t, u32 i, bool on) {
    if (i >= (u32)t->flagCount * 8) return;
    if (on) t->flags[i >> 3] |= (u8)(1 << (i & 7));
    else t->flags[i >> 3] &= (u8)~(1 << (i & 7));
}

static i32 apply(int op, i32 acc, i32 v) {
    switch (op) {
    case 0: return acc + v;
    case 1: return acc - v;
    case 2: return acc * v;
    case 3: return v ? acc / v : 0;
    case 4: return v ? acc % v : 0;
    default: return acc;
    }
}

static void expr(IK *t, const u8 *a, int n) {
    if (n < 3) return;
    i32 acc = 0;
    int i = 2;
    while (i + 5 <= n && a[i] != 5) {
        acc = apply(a[i], acc, (i32)ik_value(t, rd32(a + i + 1)));
        i += 5;
    }
    i32 *v = var_at(t, rd16(a));
    if (v) *v = acc;
}

static bool compare(int op, i32 a, i32 b) {
    switch (op) {
    case 0: return a == b;
    case 1: return a != b;
    case 2: return a <= b;
    case 3: return a > b;
    case 4: return a >= b;
    case 5: return a < b;
    default: return false;
    }
}

static void cond(IK *t, const u8 *a, int n) {
    int i = 0;
    bool result = true;
    for (;;) {
        if (i + 10 > n) return;
        bool c = compare(a[i + 4], (i32)ik_value(t, rd32(a + i)), (i32)ik_value(t, rd32(a + i + 5)));
        result = result && c;
        int conj = a[i + 9];
        i += 10;
        if (conj != 2) break;
    }
    if (i + 2 <= n && result) goto_label(t, rd16(a + i));
}

static Layer *layer(IK *t, u32 id) {
    id = ik_value(t, id);
    if (id >= (u32)t->layerCount) {
        ik_log(t, "layer %u out of range", id);
        return NULL;
    }
    return &t->layers[id];
}

static Button *button(IK *t, u32 id) {
    id = ik_value(t, id);
    if (id >= IK_BUTTONS) {
        ik_log(t, "button %u out of range", id);
        return NULL;
    }
    return &t->buttons[id];
}

void image_load(IK *t, int slot, const char *name) {
    if (slot < 0 || slot >= t->imageCount) return;
    image_free(t->images[slot]);
    t->images[slot] = NULL;
    snprintf(t->imageNames[slot], sizeof t->imageNames[slot], "%s", name);
    size_t n;
    u8 *d = store_load(&t->store, AR_GRP, name, &n);
    if (!d) {
        ik_log(t, "missing picture %s", name);
        return;
    }
    t->images[slot] = image_decode(d, n);
    free(d);
    if (!t->images[slot]) ik_log(t, "can't decode %s", name);
}

static void resize(IK *t, int which, int n) {
    if (n <= 0 || n > 100000) return;
    if (which == 0) {
        t->vars = ik_realloc(t->vars, sizeof(i32) * (size_t)n);
        if (n > t->varCount) memset(t->vars + t->varCount, 0, sizeof(i32) * (size_t)(n - t->varCount));
        t->varCount = n;
    } else if (which == 1) {
        for (int i = n; i < t->imageCount; i++) image_free(t->images[i]);
        t->images = ik_realloc(t->images, sizeof(Image *) * (size_t)n);
        t->imageNames = ik_realloc(t->imageNames, sizeof(*t->imageNames) * (size_t)n);
        if (n > t->imageCount) {
            memset(t->images + t->imageCount, 0, sizeof(Image *) * (size_t)(n - t->imageCount));
            memset(t->imageNames + t->imageCount, 0, sizeof(*t->imageNames) * (size_t)(n - t->imageCount));
        }
        t->imageCount = n;
    } else {
        t->layers = ik_realloc(t->layers, sizeof(Layer) * (size_t)n);
        if (n > t->layerCount) memset(t->layers + t->layerCount, 0, sizeof(Layer) * (size_t)(n - t->layerCount));
        t->layerCount = n;
    }
}

static const Cell *cell_at(IK *t, int img, int c) {
    if (c < 0) c = -c;
    if (img < 0 || img >= t->imageCount || !t->images[img]) return NULL;
    Image *im = t->images[img];
    if (c >= im->cellCount) return NULL;
    return &im->cells[c];
}

static void layer_reset(IK *t, Layer *l, int w, int h) {
    image_free(l->surf);
    memset(l, 0, sizeof *l);
    l->made = true;
    l->w = l->tw = w;
    l->h = l->th = h;
    l->img = -1;
    l->textured = true;
    for (int k = 0; k < 4; k++) l->argb[k] = 255;
}

static void draw_cell(IK *t, const u8 *a, bool toImage, bool copy) {
    int src = (int)ik_value(t, rd32(a)), c = (int)ik_value(t, rd32(a + 4));
    const Cell *ce = cell_at(t, src, c);
    if (!ce) return;
    Image *dst = NULL;
    if (toImage) {
        int di = (int)ik_value(t, rd32(a + 8));
        if (di >= 0 && di < t->imageCount) dst = t->images[di];
    } else {
        Layer *l = layer(t, rd32(a + 8));
        if (l) dst = layer_surface(t, l);
    }
    int x = (int)ik_value(t, rd32(a + 12)), y = (int)ik_value(t, rd32(a + 16));
    if (c < 0) {
        x += ce->dx;
        y += ce->dy;
    }
    static const int white[4] = {255, 255, 255, 255};
    image_blit(dst, t->images[src], ce->sx, ce->sy, ce->w, ce->h, x, y, white, 0, copy);
    t->dirty = true;
}

static void cell_button(IK *t, u32 id, int type, int lay, int x, int y, bool place, int img, const u8 *cells, int mode) {
    Button *b = button(t, id);
    if (!b) return;
    memset(b, 0, sizeof *b);
    b->made = true;
    b->type = type;
    b->var = -1;
    b->layer = lay;
    for (int k = 0; k < 4; k++) {
        const Cell *ce = cell_at(t, img, (int)ik_value(t, rd32(cells + 4 * k)));
        b->state[k] = ce ? (ButtonState){img, ce->sx, ce->sy} : (ButtonState){-1, 0, 0};
        if (ce && !b->w) {
            b->w = ce->w;
            b->h = ce->h;
            if (!place) {
                x = ce->dx;
                y = ce->dy;
            }
        }
    }
    b->x = x;
    b->y = y;
    b->mode = (u8)clampi(mode, 0, 2);
    b->enabled = b->mode != 0;
    b->shown = true;
}

static const char *name_arg(const u8 *a, int n) {
    static char buf[64];
    int k = 0;
    while (k < n && k < 63 && a[k]) {
        buf[k] = (char)a[k];
        k++;
    }
    buf[k] = 0;
    return buf;
}

static void exec(IK *t, u16 op, const u8 *a, int n) {
    Layer *l;
    switch (op) {
    case 0x0000: t->wait = W_QUIT; break;
    case 0x0001:
        vm_start(t, name_arg(a, n));
        if (!t->sp) {
            t->resumeSc = t->sc;
            t->resumeLabel = -1;
        }
        break;
    case 0x0002: push(t); vm_start(t, name_arg(a, n)); break;
    case 0x0003: pop(t); break;
    case 0x0004: goto_label(t, rd16(a)); break;
    case 0x0005: push(t); goto_label(t, rd16(a)); break;
    case 0x0006: pop(t); break;
    case 0x0007: {
        u32 v = ik_value(t, rd32(a)), count = rd32(a + 4);
        if (v < count && 8 + (int)v * 2 + 2 <= n) goto_label(t, rd16(a + 8 + v * 2));
        break;
    }
    case 0x0008: {
        u32 v = ik_value(t, rd32(a)), count = rd32(a + 4);
        if (v < count && 8 + (int)v * 2 + 2 <= n) {
            push(t);
            goto_label(t, rd16(a + 8 + v * 2));
        }
        break;
    }
    case 0x0009: break;
    case 0x000a: if (gfx_animating(t)) t->wait = W_ANIM; break;
    case 0x000b:
        t->wait = W_TIME;
        t->waitUntil = t->now + ik_value(t, rd32(a));
        break;
    case 0x0100: resize(t, 1, rd16(a)); break;
    case 0x0101: image_load(t, rd16(a), name_arg(a + 2, n - 2)); break;
    case 0x0108: {
        int slot = (int)ik_value(t, rd32(a)), ts = (int)ik_value(t, rd32(a + 4));
        if (ts >= 0 && ts < t->textCount && t->texts[ts]) image_load(t, slot, t->texts[ts]);
        break;
    }
    case 0x0109:
        for (int i = (int)ik_value(t, rd32(a)), e = (int)ik_value(t, rd32(a + 4)); i <= e && i < t->imageCount; i++) {
            if (i < 0) continue;
            image_free(t->images[i]);
            t->images[i] = NULL;
            t->imageNames[i][0] = 0;
        }
        break;
    case 0x010a:
    case 0x010b:
    case 0x010c:
    case 0x010d:
        if (n >= 20) draw_cell(t, a, op >= 0x010c, op == 0x010b || op == 0x010d);
        break;
    case 0x0102: {
        int s = rd16(a);
        if (s >= 0 && s < t->imageCount) {
            image_free(t->images[s]);
            t->images[s] = NULL;
            t->imageNames[s][0] = 0;
        }
        break;
    }
    case 0x0200: resize(t, 2, rd16(a)); break;
    case 0x0202:
        if ((l = layer(t, rd32(a)))) layer_reset(t, l, (int)ik_value(t, rd32(a + 4)), (int)ik_value(t, rd32(a + 8)));
        break;
    case 0x0212: {
        u32 i = ik_value(t, rd32(a)), j = ik_value(t, rd32(a + 4));
        if (i < (u32)t->layerCount && j < (u32)t->layerCount && i != j) {
            Layer tmp = t->layers[i];
            t->layers[i] = t->layers[j];
            t->layers[j] = tmp;
        }
        break;
    }
    case 0x0213: {
        bool on = ik_value(t, rd32(a + 8)) != 0;
        for (u32 i = ik_value(t, rd32(a)), e = ik_value(t, rd32(a + 4)); i <= e && i < (u32)t->layerCount; i++)
            if (t->layers[i].made) t->layers[i].shown = on;
        break;
    }
    case 0x0214:
        if ((l = layer(t, rd32(a))) && n >= 32) {
            layer_reset(t, l, (int)ik_value(t, rd32(a + 12)), (int)ik_value(t, rd32(a + 16)));
            l->x = (int)ik_value(t, rd32(a + 4));
            l->y = (int)ik_value(t, rd32(a + 8));
            l->img = (int)ik_value(t, rd32(a + 20));
            l->sx = (int)ik_value(t, rd32(a + 24));
            l->sy = (int)ik_value(t, rd32(a + 28));
            l->ready = true;
        }
        break;
    case 0x0215:
        if ((l = layer(t, rd32(a))) && n >= 20) {
            int img = (int)ik_value(t, rd32(a + 12));
            const Cell *ce = cell_at(t, img, (int)ik_value(t, rd32(a + 16)));
            layer_reset(t, l, ce ? ce->w : 0, ce ? ce->h : 0);
            l->x = (int)ik_value(t, rd32(a + 4));
            l->y = (int)ik_value(t, rd32(a + 8));
            l->img = img;
            l->sx = ce ? ce->sx : 0;
            l->sy = ce ? ce->sy : 0;
            l->ready = true;
        }
        break;
    case 0x0216:
        if ((l = layer(t, rd32(a))) && n >= 36) {
            layer_reset(t, l, (int)ik_value(t, rd32(a + 12)), (int)ik_value(t, rd32(a + 16)));
            l->x = (int)ik_value(t, rd32(a + 4));
            l->y = (int)ik_value(t, rd32(a + 8));
            u32 c = 0;
            for (int k = 0; k < 4; k++) c = c << 8 | (u32)clampi((int)ik_value(t, rd32(a + 20 + 4 * k)), 0, 255);
            l->fillArgb = c;
            l->fill = true;
            l->ready = true;
        }
        break;
    case 0x0217:
    case 0x0218:
        if ((l = layer(t, rd32(a))) && n >= 6 && (l->made && l->shown) == (op == 0x0217)) goto_label(t, rd16(a + 4));
        break;
    case 0x0203:
        if ((l = layer(t, rd32(a)))) {
            l->x = (int)ik_value(t, rd32(a + 4));
            l->y = (int)ik_value(t, rd32(a + 8));
            l->move.on = false;
        }
        break;
    case 0x0205:
        if ((l = layer(t, rd32(a)))) l->shown = ik_value(t, rd32(a + 4)) != 0;
        break;
    case 0x0204:
        if ((l = layer(t, rd32(a))) && n >= 36) {
            int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
            for (int k = 0; k < 4; k++) {
                int x = (int)ik_value(t, rd32(a + 4 + 8 * k)), y = (int)ik_value(t, rd32(a + 8 + 8 * k));
                if (x < x0) x0 = x;
                if (y < y0) y0 = y;
                if (x > x1) x1 = x;
                if (y > y1) y1 = y;
            }
            l->x = x0;
            l->y = y0;
            l->w = x1 - x0;
            l->h = y1 - y0;
            l->quad = true;
        }
        break;
    case 0x0206:
        if ((l = layer(t, rd32(a))) && n >= 20) {
            u32 c = 0;
            for (int k = 0; k < 4; k++) c = c << 8 | (u32)clampi((int)ik_value(t, rd32(a + 4 + 4 * k)), 0, 255);
            l->fillArgb = c;
            l->fill = true;
        }
        break;
    case 0x0207:
        if ((l = layer(t, rd32(a)))) {
            l->img = (int)ik_value(t, rd32(a + 4));
            l->sx = (int)ik_value(t, rd32(a + 8));
            l->sy = (int)ik_value(t, rd32(a + 12));
        }
        break;
    case 0x0208:
        if ((l = layer(t, rd32(a)))) {
            l->ready = true;
            image_free(l->surf);
            l->surf = NULL;
        }
        break;
    case 0x0209:
        if ((l = layer(t, rd32(a))) && n >= 20)
            layer_restore(t, l, (int)ik_value(t, rd32(a + 4)), (int)ik_value(t, rd32(a + 8)), (int)ik_value(t, rd32(a + 12)),
                          (int)ik_value(t, rd32(a + 16)));
        break;
    case 0x0103:
    case 0x0104:
    case 0x0105:
    case 0x0106:
        if (n >= 32) {
            int src = (int)ik_value(t, rd32(a));
            if (src < 0 || src >= t->imageCount || !t->images[src]) break;
            Image *dst = NULL;
            if (op <= 0x0104) {
                if ((l = layer(t, rd32(a + 4)))) dst = layer_surface(t, l);
            } else {
                int di = (int)ik_value(t, rd32(a + 4));
                if (di >= 0 && di < t->imageCount) dst = t->images[di];
            }
            static const int white[4] = {255, 255, 255, 255};
            if (dst)
                image_blit(dst, t->images[src], (int)ik_value(t, rd32(a + 8)), (int)ik_value(t, rd32(a + 12)),
                           (int)ik_value(t, rd32(a + 24)), (int)ik_value(t, rd32(a + 28)), (int)ik_value(t, rd32(a + 16)),
                           (int)ik_value(t, rd32(a + 20)), white, 0, op == 0x0104 || op == 0x0106);
            t->dirty = true;
        }
        break;
    case 0x0107: {
        int slot = (int)ik_value(t, rd32(a));
        if (slot >= 0 && slot < t->imageCount && n >= 12) {
            image_free(t->images[slot]);
            int w = (int)ik_value(t, rd32(a + 4)), h = (int)ik_value(t, rd32(a + 8));
            t->images[slot] = image_new(w, h);
            memset(t->images[slot]->px, 0, sizeof(u32) * (size_t)w * (size_t)h);
            t->imageNames[slot][0] = 0;
        }
        break;
    }
    case 0x020a:
        if (n >= 3 && 3 + a[1] + 16 <= n) {
            char name[64];
            int len = a[1] < 63 ? a[1] : 63;
            memcpy(name, a + 3, (size_t)len);
            name[len] = 0;
            const u8 *q = a + 3 + a[1];
            font_set(t, a[0], name, (int)ik_value(t, rd32(q)), (int)ik_value(t, rd32(q + 4)), ik_value(t, rd32(q + 8)) != 0);
        }
        break;
    case 0x0300: {
        int c = ((int)rd16(a) + 7) / 8;
        t->flags = ik_realloc(t->flags, (size_t)c + 1);
        memset(t->flags, 0, (size_t)c + 1);
        t->flagCount = c;
        break;
    }
    case 0x0301: set_flag(t, ik_value(t, rd32(a)), true); break;
    case 0x0302: set_flag(t, ik_value(t, rd32(a)), false); break;
    case 0x0305:
        for (u32 i = ik_value(t, rd32(a)), e = ik_value(t, rd32(a + 4)); i <= e && i < (u32)t->flagCount * 8; i++) set_flag(t, i, false);
        break;
    case 0x0307:
        if (flag(t, ik_value(t, rd32(a)))) goto_label(t, rd16(a + 4));
        break;
    case 0x0309:
        for (u32 i = 0, src = ik_value(t, rd32(a)), dst = ik_value(t, rd32(a + 4)), c = ik_value(t, rd32(a + 8)); i < c && i < 0x10000; i++)
            set_flag(t, dst + i, flag(t, src + i));
        break;
    case 0x0308:
        if (!flag(t, ik_value(t, rd32(a)))) goto_label(t, rd16(a + 4));
        break;
    case 0x0408: {
        i32 *v = var_at(t, rd32(a));
        i32 m = (i32)ik_value(t, rd32(a + 4));
        t->rng ^= t->rng << 13;
        t->rng ^= t->rng >> 17;
        t->rng ^= t->rng << 5;
        if (v) *v = m > 0 ? (i32)(t->rng % (u32)m) : 0;
        break;
    }
    case 0x0405:
        if (n >= 12) {
            u32 e = ik_value(t, rd32(a + 4));
            i32 v = (i32)ik_value(t, rd32(a + 8));
            for (u32 i = ik_value(t, rd32(a)); i <= e && i < (u32)t->varCount; i++) t->vars[i] = v;
        }
        break;
    case 0x000c: break;
    case 0x0500: break;
    case 0x0501: snd_se_load(t, (int)ik_value(t, rd32(a)), name_arg(a + 4, n - 4)); break;
    case 0x0502: snd_se_stop(t, (int)ik_value(t, rd32(a))); break;
    case 0x0503: snd_se_play(t, (int)ik_value(t, rd32(a)), n >= 8 && ik_value(t, rd32(a + 4)) != 0); break;
    case 0x050b:
        if (n >= 16) {
            int slot = (int)ik_value(t, rd32(a));
            snd_se_load(t, slot, name_arg(a + 4, 12));
            snd_se_play(t, slot, n >= 20 && ik_value(t, rd32(a + 16)) != 0);
        }
        break;
    case 0x0601: snd_bgm_fade(t, 0); break;
    case 0x0506: snd_voice(t, name_arg(a, n)); break;
    case 0x0507: snd_voice_stop(t); break;
    case 0x0508: t->wait = W_VOICE; break;
    case 0x0600: snd_bgm(t, name_arg(a, n < 12 ? n : 12), n >= 16 && ik_value(t, rd32(a + 12)) != 0); break;
    case 0x0602: snd_bgm_fade(t, (int)ik_value(t, rd32(a))); break;
    case 0x020b:
        if ((l = layer(t, rd32(a)))) {
            u32 ms = ik_value(t, rd32(a + 4));
            for (int k = 0; k < 4; k++) {
                l->fade.from[k] = (int)ik_value(t, rd32(a + 8 + 4 * k));
                l->fade.to[k] = (int)ik_value(t, rd32(a + 24 + 4 * k));
            }
            l->fade.start = t->now;
            l->fade.dur = ms;
            l->fade.on = true;
            l->shown = true;
            memcpy(l->argb, ms ? l->fade.from : l->fade.to, sizeof l->argb);
        }
        break;
    case 0x020c:
        if ((l = layer(t, rd32(a))))
            layer_move(t, l, (int)ik_value(t, rd32(a + 4)), (int)ik_value(t, rd32(a + 8)), (int)ik_value(t, rd32(a + 12)));
        break;
    case 0x0211:
        if ((l = layer(t, rd32(a))))
            layer_move(t, l, (int)ik_value(t, rd32(a + 4)), l->x + (int)ik_value(t, rd32(a + 8)), l->y + (int)ik_value(t, rd32(a + 12)));
        break;
    case 0x020f: {
        i32 *v = var_at(t, rd32(a + 4));
        if ((l = layer(t, rd32(a))) && v) *v = l->shown && l->made;
        break;
    }
    case 0x0210:
        if ((l = layer(t, rd32(a))) && n >= 24) {
            u32 ms = ik_value(t, rd32(a + 4));
            for (int k = 0; k < 4; k++) {
                l->fade.from[k] = l->argb[k];
                l->fade.to[k] = (int)ik_value(t, rd32(a + 8 + 4 * k));
            }
            l->fade.start = t->now;
            l->fade.dur = ms;
            l->fade.on = true;
            l->shown = true;
            if (!ms) memcpy(l->argb, l->fade.to, sizeof l->argb);
        }
        break;
    case 0x020e:
        if ((l = layer(t, rd32(a)))) l->textured = ik_value(t, rd32(a + 4)) != 0;
        break;
    case 0x0700: t->buttonCount = clampi((int)ik_value(t, rd32(a)), 0, IK_BUTTONS); break;
    case 0x070b: {
        Button *b = button(t, rd32(a));
        if (b) {
            if (b->type == 1) b->type = 3;
            b->var = (int)rd32(a + 4);
        }
        break;
    }
    case 0x070e: {
        Button *b = button(t, rd32(a));
        if (b) b->max = (int)ik_value(t, rd32(a + 4));
        break;
    }
    case 0x070f: {
        Button *b = button(t, rd32(a));
        if (b) b->value = (int)ik_value(t, rd32(a + 4));
        t->dirty = true;
        break;
    }
    case 0x0710:
        t->wheelVar = (int)rd32(a);
        t->wheelLabel = rd16(a + 4);
        t->wheelCall = n >= 10 && rd32(a + 6) != 0;
        t->wheelSc = t->sc;
        t->wheelBound = true;
        break;
    case 0x0409: {
        i32 *v = var_at(t, rd32(a));
        u32 i = ik_value(t, rd32(a + 4));
        if (v) *v = i < 64 ? t->settings[i] : 0;
        break;
    }
    case 0x040a: {
        u32 i = ik_value(t, rd32(a + 4));
        if (i < 64) t->settings[i] = (i32)ik_value(t, rd32(a));
        settings_apply(t);
        break;
    }
    case 0x071a:
    case 0x071b: {
        int which = op == 0x071a, id = (int)ik_value(t, rd32(a));
        Button *b = button(t, (u32)id);
        if (!b) break;
        t->modeButton[which] = id;
        bool on = which ? t->skipMode : t->autoMode;
        b->mode = n >= 8 && !ik_value(t, rd32(a + 4)) ? 0 : on ? 2 : 1;
        b->enabled = b->mode != 0;
        break;
    }
    case 0x0e00:
        if (n >= 40 && movie_play(t, name_arg(a + 4, 12), (int)ik_value(t, rd32(a + 16)), (int)ik_value(t, rd32(a + 20)),
                                  (int)ik_value(t, rd32(a + 24)), (int)ik_value(t, rd32(a + 28)), ik_value(t, rd32(a + 36)) != 0))
            t->wait = W_MOVIE;
        break;
    case 0x0010:
    case 0x0011:
    case 0x0f00:
    case 0x0f01:
    case 0x0f03:
    case 0x0f04:
    case 0x0201:
    case 0x000d:
    case 0x0705:
    case 0x0711: break;
    case 0x0704: {
        u32 key = ik_value(t, rd32(a)), id = rd32(a + 4);
        if (key < 256) t->keyButton[key] = id == IK_CANCEL ? -1 : (int)id + 1;
        break;
    }
    case 0x070a: {
        Button *b = button(t, rd32(a));
        if (b && n >= 36)
            b->tip = (ButtonTip){true, (int)ik_value(t, rd32(a + 4)), (int)ik_value(t, rd32(a + 8)), (int)ik_value(t, rd32(a + 12)),
                                 (int)ik_value(t, rd32(a + 16)), (int)ik_value(t, rd32(a + 20)), (int)ik_value(t, rd32(a + 24)),
                                 (int)ik_value(t, rd32(a + 28)), (int)ik_value(t, rd32(a + 32))};
        break;
    }
    case 0x070c: {
        u32 slot = ik_value(t, rd32(a)) & 3;
        if (!t->savedEnabled[slot]) t->savedEnabled[slot] = ik_alloc(IK_BUTTONS);
        for (int k = 0; k < IK_BUTTONS; k++) {
            t->savedEnabled[slot][k] = t->buttons[k].mode;
            t->buttons[k].mode = 0;
            t->buttons[k].enabled = false;
        }
        break;
    }
    case 0x070d: {
        u32 slot = ik_value(t, rd32(a)) & 3;
        if (!t->savedEnabled[slot]) break;
        for (int k = 0; k < IK_BUTTONS; k++)
            if (t->buttons[k].made) {
                t->buttons[k].mode = t->savedEnabled[slot][k];
                t->buttons[k].enabled = t->buttons[k].mode != 0;
            }
        break;
    }
    case 0x0912: {
        i32 *v = var_at(t, rd32(a + 4));
        if (v) *v = ik_value(t, rd32(a)) ? t->skipMode : t->autoMode;
        break;
    }
    case 0x0913: {
        int which = ik_value(t, rd32(a)) ? 1 : 0, on = ik_value(t, rd32(a + 4)) != 0;
        if (which) t->skipMode = on;
        else t->autoMode = on;
        if (t->lastToggle >= 0) t->modeButton[which] = t->lastToggle;
        t->lastToggle = -1;
        t->autoAt = 0;
        break;
    }
    case 0x0701: {
        Button *b = button(t, rd32(a));
        if (!b) break;
        memset(b, 0, sizeof *b);
        if (rd32(a + 4) == IK_CANCEL) break;
        b->made = true;
        b->type = (int)ik_value(t, rd32(a + 4));
        b->var = -1;
        b->layer = (int)ik_value(t, rd32(a + 8));
        b->x = (int)ik_value(t, rd32(a + 12));
        b->y = (int)ik_value(t, rd32(a + 16));
        b->w = (int)ik_value(t, rd32(a + 20));
        b->h = (int)ik_value(t, rd32(a + 24));
        b->enabled = true;
        b->mode = 1;
        b->shown = true;
        break;
    }
    case 0x0702: {
        u32 id = rd32(a);
        bool call = n >= 10 && rd32(a + 6) != 0;
        if (id == IK_CANCEL) {
            t->cancelLabel = rd16(a + 4);
            t->cancelBound = true;
            t->cancelCall = call;
            t->cancelSc = t->sc;
            break;
        }
        Button *b = button(t, id);
        if (!b) break;
        b->label = rd16(a + 4);
        b->bound = true;
        b->call = call;
        b->sc = t->sc;
        break;
    }
    case 0x0703: {
        Button *b = button(t, rd32(a));
        if (b) {
            b->mode = (u8)clampi((int)ik_value(t, rd32(a + 4)), 0, 2);
            b->enabled = b->mode != 0;
        }
        break;
    }
    case 0x0706: t->wait = W_INPUT; break;
    case 0x0716:
        if (n >= 36)
            cell_button(t, rd32(a), 1, (int)ik_value(t, rd32(a + 4)), 0, 0, false, (int)ik_value(t, rd32(a + 8)), a + 12,
                        (int)ik_value(t, rd32(a + 32)));
        break;
    case 0x0717:
        if (n >= 44)
            cell_button(t, rd32(a), 1, (int)ik_value(t, rd32(a + 4)), (int)ik_value(t, rd32(a + 8)), (int)ik_value(t, rd32(a + 12)), true,
                        (int)ik_value(t, rd32(a + 16)), a + 20, (int)ik_value(t, rd32(a + 40)));
        break;
    case 0x071f:
        if (n >= 40)
            cell_button(t, rd32(a), (int)ik_value(t, rd32(a + 4)), (int)ik_value(t, rd32(a + 8)), 0, 0, false, (int)ik_value(t, rd32(a + 12)),
                        a + 16, (int)ik_value(t, rd32(a + 36)));
        break;
    case 0x0719: {
        int m = clampi((int)ik_value(t, rd32(a + 8)), 0, 2);
        for (int i = (int)ik_value(t, rd32(a)), e = (int)ik_value(t, rd32(a + 4)); i <= e && i < IK_BUTTONS; i++) {
            if (i < 0 || !t->buttons[i].made) continue;
            t->buttons[i].mode = (u8)m;
            t->buttons[i].enabled = m != 0;
        }
        break;
    }
    case 0x071c:
        for (int i = (int)ik_value(t, rd32(a)), e = (int)ik_value(t, rd32(a + 4)); i <= e && i < IK_BUTTONS; i++)
            if (i >= 0) memset(&t->buttons[i], 0, sizeof t->buttons[i]);
        break;
    case 0x071d: {
        Button *b = button(t, rd32(a));
        u32 st = ik_value(t, rd32(a + 4));
        if (!b || st > 3 || n < 16) break;
        int img = (int)ik_value(t, rd32(a + 8));
        const Cell *ce = cell_at(t, img, (int)ik_value(t, rd32(a + 12)));
        b->state[st] = ce ? (ButtonState){img, ce->sx, ce->sy} : (ButtonState){-1, 0, 0};
        if (n >= 20) b->stateFlag[st] = (u8)ik_value(t, rd32(a + 16));
        t->dirty = true;
        break;
    }
    case 0x0722: {
        Button *b = button(t, rd32(a));
        if (!b || n < 28) break;
        int img = (int)ik_value(t, rd32(a + 20));
        const Cell *ce = cell_at(t, img, (int)ik_value(t, rd32(a + 24)));
        if (ce)
            b->tip = (ButtonTip){true, (int)ik_value(t, rd32(a + 8)), (int)ik_value(t, rd32(a + 12)), (int)ik_value(t, rd32(a + 16)),
                                 ce->w, ce->h, img, ce->sx, ce->sy};
        else b->tip.on = false;
        break;
    }
    case 0x0709: {
        Button *b = button(t, rd32(a));
        u32 st = ik_value(t, rd32(a + 4));
        if (!b || st > 3) break;
        b->state[st] = (ButtonState){(int)ik_value(t, rd32(a + 8)), (int)ik_value(t, rd32(a + 12)), (int)ik_value(t, rd32(a + 16))};
        if (n >= 24) b->stateFlag[st] = (u8)ik_value(t, rd32(a + 20));
        break;
    }
    case 0x0400: resize(t, 0, rd16(a)); break;
    case 0x0401: {
        i32 *v = var_at(t, rd32(a));
        if (v) *v = (i32)ik_value(t, rd32(a + 4));
        break;
    }
    case 0x0402: {
        i32 *v = var_at(t, rd32(a));
        if (v) (*v)++;
        break;
    }
    case 0x0403: {
        i32 *v = var_at(t, rd32(a));
        if (v) (*v)--;
        break;
    }
    case 0x0404: expr(t, a, n); break;
    case 0x040b: {
        i32 *v = var_at(t, rd32(a)), *src = var_at(t, ik_value(t, rd32(a + 4)));
        if (v) *v = src ? *src : 0;
        break;
    }
    case 0x040d: {
        u32 dst = ik_value(t, rd32(a)), src = ik_value(t, rd32(a + 4)), c = ik_value(t, rd32(a + 8));
        if (dst < (u32)t->varCount && src < (u32)t->varCount && c <= (u32)t->varCount - dst && c <= (u32)t->varCount - src)
            memmove(t->vars + dst, t->vars + src, sizeof(i32) * c);
        break;
    }
    case 0x0407: cond(t, a, n); break;
    default:
        if ((op >> 8) == 0x09 && msg_exec(t, op, a, n)) break;
        if ((op >> 8) == 0x08 && dun_exec(t, op, a, n)) break;
        if ((op >> 8) == 0x0a && save_exec(t, op, a, n)) break;
        if ((op >> 8) == 0x0b && text_exec(t, op, a, n)) break;
        if (t->trace) ik_log(t, "%s:%x op %04x not handled", t->sc->name, t->pc, op);
        break;
    }
}

void vm_run(IK *t) {
    int steps = 0;
    while (t->wait == W_NONE && t->sc && !t->quit) {
        if (++steps > MAX_STEPS) {
            ik_log(t, "%s ran %d steps without waiting", t->sc->name, MAX_STEPS);
            return;
        }
        if (t->pc + 4 > t->sc->codeSize) {
            ik_log(t, "%s ended", t->sc->name);
            t->wait = W_QUIT;
            return;
        }
        const u8 *p = t->sc->code + t->pc;
        u16 op = rd16(p), len = rd16(p + 2);
        if (len < 4 || t->pc + len > t->sc->codeSize) {
            ik_log(t, "%s: bad instruction at %x", t->sc->name, t->pc);
            t->wait = W_QUIT;
            return;
        }
        if (!t->sp && t->sc->labelAt[t->pc] >= 0) {
            t->resumeSc = t->sc;
            t->resumeLabel = t->sc->labelAt[t->pc];
        }
        t->pc += len;
        if (t->trace) ik_log(t, "%s:%x %04x", t->sc->name, t->pc - len, op);
        exec(t, op, p + 4, len - 4);
        t->dirty = true;
    }
}

void vm_set_text(IK *t, int slot, const char *s, int len) {
    if (slot < 0 || slot >= t->textCount) return;
    free(t->texts[slot]);
    t->texts[slot] = ik_alloc((size_t)len + 1);
    memcpy(t->texts[slot], s, (size_t)len);
    t->texts[slot][len] = 0;
}

static void layer_text(IK *t, int slot, const u8 *a, bool toImage) {
    Image *s = NULL;
    if (toImage) {
        int i = (int)ik_value(t, rd32(a + 4));
        if (i >= 0 && i < t->imageCount) s = t->images[i];
    } else {
        Layer *l = layer(t, rd32(a + 4));
        if (l) s = layer_surface(t, l);
    }
    if (!s || slot < 0 || slot >= t->textCount || !t->texts[slot]) return;
    int x = (int)ik_value(t, rd32(a + 8)), y = (int)ik_value(t, rd32(a + 12)), w = (int)ik_value(t, rd32(a + 16)),
        h = (int)ik_value(t, rd32(a + 20)), pitch = (int)ik_value(t, rd32(a + 24)), font = (int)ik_value(t, rd32(a + 28)) & (IK_FONTS - 1);
    u32 colour = 0, shadow = 0;
    for (int k = 0; k < 4; k++) colour = colour << 8 | (u32)clampi((int)ik_value(t, rd32(a + 32 + 4 * k)), 0, 255);
    for (int k = 0; k < 4; k++) shadow = shadow << 8 | (u32)clampi((int)ik_value(t, rd32(a + 48 + 4 * k)), 0, 255);
    colour = (colour & 0xff000000u) | (colour & 0xff) << 16 | (colour & 0xff00) | (colour >> 16 & 0xff);
    shadow = (shadow & 0xff000000u) | (shadow & 0xff) << 16 | (shadow & 0xff00) | (shadow >> 16 & 0xff);
    int clip[4] = {x, y, x + w, y + h};
    const u8 *p = (const u8 *)t->texts[slot];
    int penX = x, penY = y;
    while (*p) {
        u32 cp;
        int k = ik_sjis_decode(p, &cp);
        if (k <= 0) k = 1;
        if (cp == '\n') {
            penX = x;
            penY += pitch;
            p += k;
            continue;
        }
        int wordW = 0;
        const u8 *q = p;
        for (bool first = true; *q; first = false) {
            u32 c2;
            int k2 = ik_sjis_decode(q, &c2);
            if (k2 <= 0) k2 = 1;
            if (!first && (c2 == ' ' || c2 >= 0x80 || c2 == '\n')) break;
            wordW += text_advance(t, font, c2);
            q += k2;
            if (c2 >= 0x80) break;
        }
        if (penX + wordW > x + w && penX > x) {
            penX = x;
            penY += pitch;
        }
        while (p < q) {
            u32 c3;
            int k3 = ik_sjis_decode(p, &c3);
            if (k3 <= 0) k3 = 1;
            text_draw_on(t, s, font, c3, penX, penY, colour, shadow, clip);
            penX += text_advance(t, font, c3);
            p += k3;
        }
    }
    t->dirty = true;
}

bool text_exec(IK *t, u16 op, const u8 *a, int n) {
    switch (op) {
    case 0x0b00: {
        int c = clampi((int)ik_value(t, rd32(a)), 0, 4096);
        for (int i = c; i < t->textCount; i++) free(t->texts[i]);
        t->texts = ik_realloc(t->texts, sizeof(char *) * (size_t)(c ? c : 1));
        for (int i = t->textCount; i < c; i++) t->texts[i] = NULL;
        t->textCount = c;
        break;
    }
    case 0x0b01:
        if (n >= 64) layer_text(t, (int)ik_value(t, rd32(a)), a, false);
        break;
    case 0x0b02:
        if (n >= 64) layer_text(t, (int)ik_value(t, rd32(a)), a, true);
        break;
    case 0x0b03:
        if (n >= 6) {
            int len = rd16(a + 4);
            if (6 + len > n) len = n - 6;
            vm_set_text(t, (int)ik_value(t, rd32(a)), (const char *)a + 6, len);
        }
        break;
    default: return false;
    }
    return true;
}

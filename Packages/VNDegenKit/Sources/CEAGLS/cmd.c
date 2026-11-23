#include "eagls.h"

static i32 I(EG *e, const Ins *in, int k) { return k < in->argc ? vm_int(e, &in->args[k]) : 0; }
static const char *S(EG *e, const Ins *in, int k) { return k < in->argc ? vm_str(e, &in->args[k]) : ""; }
static const char *N(EG *e, const Ins *in, int k) { return k < in->argc ? vm_name(e, &in->args[k]) : ""; }

static void set_int(EG *e, const Ins *in, int k, i32 v) {
    if (k >= in->argc) return;
    const Arg *a = &in->args[k];
    if (a->kind == A_VAR) vm_set_int(e, a, v);
    else if (a->kind == A_STR && a->s) {
        Var *var = var_get(&e->vars, a->s, true);
        Val *x = var_at(var, 0);
        x->i = v;
        free(x->s);
        x->s = NULL;
    }
}

static void set_str(EG *e, const char *name, const char *s) { vm_set_str_name(e, name, s); }

static void unknown(EG *e, const Ins *in) {
    static u8 seen[2048];
    int n = in->n < 2048 ? in->n : 2047;
    if (seen[n]) return;
    seen[n] = 1;
    eg_log(e, "command %d not supported (%s:%d)", in->n, e->cur && e->cur->sc ? e->cur->sc->name : "-", in->line);
}

static void sprintf_cmd(EG *e, const Ins *in) {
    const char *fmt = S(e, in, 1);
    char out[1024];
    size_t k = 0;
    int arg = 2;
    for (const char *p = fmt; *p && k + 1 < sizeof out;) {
        if (*p == '%' && p[1]) {
            const char *q = p + 1;
            while (*q && strchr("0123456789-.", *q)) q++;
            char spec[16];
            int sl = (int)(q - p) + 1;
            if (sl > 15) sl = 15;
            memcpy(spec, p, (size_t)sl);
            spec[sl] = 0;
            int w;
            if (*q == 's') w = snprintf(out + k, sizeof out - k, spec, S(e, in, arg++));
            else if (*q == 'd' || *q == 'x' || *q == 'X') w = snprintf(out + k, sizeof out - k, spec, I(e, in, arg++));
            else if (*q == '%') w = snprintf(out + k, sizeof out - k, "%%");
            else w = 0;
            if (w > 0) k += (size_t)w < sizeof out - k ? (size_t)w : sizeof out - k - 1;
            p = *q ? q + 1 : q;
            continue;
        }
        int l = eg_sjis_len((const u8 *)p);
        for (int i = 0; i < l && *p && k + 1 < sizeof out; i++) out[k++] = *p++;
    }
    out[k] = 0;
    set_str(e, N(e, in, 0), out);
}

static void substring(EG *e, const Ins *in) {
    const char *src = S(e, in, 1);
    int a = I(e, in, 2), b = I(e, in, 3);
    int len = (int)strlen(src);
    if (a < 0) a = 0;
    if (b >= len) b = len - 1;
    char out[512] = {0};
    if (b >= a && a < len) {
        int n = b - a + 1;
        if (n > 511) n = 511;
        memcpy(out, src + a, (size_t)n);
    }
    set_str(e, N(e, in, 0), out);
    set_int(e, in, 4, out[0] != 0);
}

static void layer_draw(EG *e, const Ins *in, bool full) {
    int base = full ? 3 : 1;
    int layer = I(e, in, 0);
    int plane = full ? I(e, in, 2) : layer;
    int r[8];
    for (int i = 0; i < 8; i++) r[i] = I(e, in, base + i);
    int alpha = full ? I(e, in, 11) : I(e, in, 12);
    bool now = full ? I(e, in, 15) != 0 : I(e, in, 13) != 0;
    if (layer < 0 || layer >= EG_PLANES) return;
    gfx_set_layer(e, layer, plane, r, alpha);
    e->layer[layer].movie = full && in->argc > 16 ? I(e, in, 16) + 1 : 0;
    if (now) gfx_present(e);
}

static void screen(EG *e, Thread *t, int mode) {
    if (mode == UI_SAVE) save_capture_thumb(e);
    ui_open(e, mode);
    e->scriptUi = true;
    e->scriptUiDone = false;
    t->wait = W_UI;
}

static void name_field(EG *e, const Ins *in) {
    int p = I(e, in, 0);
    if (p < 0 || p >= EG_PLANES) return;
    int x0 = I(e, in, 1), y0 = I(e, in, 2), x1 = I(e, in, 3), y1 = I(e, in, 4);
    if (!e->plane[p]) {
        e->plane[p] = image_new(e->W, e->H);
        e->plane[p]->alpha = true;
        snprintf(e->planeName[p], sizeof e->planeName[p], "%s", "");
    }
    Image *im = e->plane[p];
    for (int y = y0; y < y1 && y < im->h; y++)
        for (int x = x0; x < x1 && x < im->w; x++) im->px[(size_t)y * im->w + x] = 0;
    const char *s = S(e, in, 12);
    int fi = I(e, in, 11);
    const Font *f = fi >= 0 && fi < 16 && e->fonts[fi].size ? &e->fonts[fi] : &e->fonts[0];
    int size = f->size > 0 ? f->size : 21;
    int tw = text_width(s, size, f->spacing);
    u32 color = (u32)clampi(I(e, in, 5), 0, 255) | (u32)clampi(I(e, in, 6), 0, 255) << 8 | (u32)clampi(I(e, in, 7), 0, 255) << 16;
    text_draw(e, im->px, im->w, im->h, s, (x0 + x1 - tw) / 2, (y0 + y1 - size) / 2, size, f->spacing, color, true);
}

static void wait_frames(Thread *t, int n) {
    if (n <= 0) n = 1;
    t->wait = W_FRAMES;
    t->frames = n;
}

void cmd_exec(EG *e, Thread *t, const Ins *in) {
    switch (in->n) {
    case 0: {
        int plane = I(e, in, 1);
        const Arg *a = in->argc ? &in->args[0] : NULL;
        const char *name = a ? vm_str(e, a) : "0";
        if (!a || (a->kind == A_NUM && a->num == 0) || !strcmp(name, "0") || !*name) gfx_free_plane(e, plane);
        else gfx_load(e, name, plane);
        break;
    }
    case 2: gfx_copy(e, I(e, in, 0), I(e, in, 1), I(e, in, 2), I(e, in, 3), I(e, in, 4), I(e, in, 5), I(e, in, 6), I(e, in, 7)); break;
    case 3: gfx_present(e); break;
    case 4: {
        const char *mask = in->argc > 1 && in->args[1].kind != A_NUM ? S(e, in, 1) : "0";
        gfx_transition(e, I(e, in, 0), mask, I(e, in, 2), I(e, in, 3) != 0);
        if (in->argc < 5 || I(e, in, 4)) t->wait = W_TRANS;
        break;
    }
    case 6: {
        int p = I(e, in, 0);
        Image *im = p >= 0 && p < EG_PLANES ? e->plane[p] : NULL;
        set_int(e, in, 1, im != NULL);
        if (in->argc > 2) set_str(e, N(e, in, 2), im ? e->planeName[p] : "");
        set_int(e, in, 3, im ? im->w : 0);
        set_int(e, in, 4, im ? im->h : 0);
        break;
    }
    case 8: gfx_copy_mask(e, I(e, in, 0), I(e, in, 1), I(e, in, 2), I(e, in, 3), I(e, in, 4), I(e, in, 5), I(e, in, 6), I(e, in, 7), I(e, in, 8), I(e, in, 9)); break;
    case 18: gfx_layer_reset(e, I(e, in, 0)); break;
    case 19: layer_draw(e, in, true); break;
    case 20: layer_draw(e, in, false); break;
    case 24:
        e->mesMode = I(e, in, 0);
        e->mesMode2 = I(e, in, 1);
        e->mesAlpha = in->argc > 2 ? clampi(I(e, in, 2), 0, 255) : 255;
        break;
    case 31: {
        int c = gfx_color_at(e, I(e, in, 1), e->mx, e->my);
        if (e->uiMode != UI_NONE || e->winHover) c = 0;
        set_int(e, in, 0, c);
        break;
    }
    case 34: {
        bool right = I(e, in, 0) == 1;
        bool reset = I(e, in, 4) != 0;
        bool *flag = right ? &e->rclicked : &e->clicked;
        int r = 0;
        if (reset) *flag = false;
        else if (*flag) {
            r = 1;
            *flag = false;
        }
        set_int(e, in, 1, r);
        set_int(e, in, 2, e->mx);
        set_int(e, in, 3, e->my);
        break;
    }
    case 36: {
        int k = I(e, in, 1);
        bool down = k == 238 ? e->keyDown[0x11] : k >= 0 && k < 256 ? e->keyDown[k] : false;
        set_int(e, in, 0, down);
        break;
    }
    case 41: vm_goto(e, t, S(e, in, 1), S(e, in, 0)); break;
    case 42: vm_call(e, t, S(e, in, 1), S(e, in, 0)); break;
    case 44: {
        int n = I(e, in, 0);
        if (n >= 0 && n < 8) e->timer[n] = e->now;
        break;
    }
    case 45: {
        int n = I(e, in, 0);
        set_int(e, in, 1, n >= 0 && n < 8 ? (i32)(e->now - e->timer[n]) : 0);
        break;
    }
    case 46:
        t->wait = W_TIME;
        t->until = e->now + (u32)I(e, in, 0);
        break;
    case 50: {
        Var *v = var_get(&e->vars, N(e, in, 0), true);
        v->str = I(e, in, 1) == 1;
        break;
    }
    case 51: var_remove(&e->vars, N(e, in, 0)); break;
    case 52: set_str(e, N(e, in, 0), S(e, in, 1)); break;
    case 53: set_int(e, in, 1, var_find(&e->vars, S(e, in, 0)) != NULL); break;
    case 54: substring(e, in); break;
    case 56: {
        int n = I(e, in, 1);
        set_int(e, in, 0, n > 0 ? eg_rand(&e->rng) % n : 0);
        break;
    }
    case 58: set_int(e, in, 0, strcmp(S(e, in, 1), S(e, in, 2)) == 0); break;
    case 59: sprintf_cmd(e, in); break;
    case 60: movie_stop(e, I(e, in, 0)); break;
    case 61: movie_open(e, I(e, in, 0), S(e, in, 1), I(e, in, 7)); break;
    case 62: movie_play(e, I(e, in, 0), I(e, in, 2) != 0); break;
    case 68: set_int(e, in, 1, movie_state(e, I(e, in, 0))); break;
    case 69: {
        int w = 0, h = 0;
        movie_size(e, I(e, in, 0), &w, &h);
        set_int(e, in, 1, w);
        set_int(e, in, 2, h);
        break;
    }
    case 70: {
        int i = I(e, in, 0);
        if (i < 0 || i >= 8) break;
        ChoiceBox *c = &e->choice[i];
        c->used = true;
        c->plane = I(e, in, 1);
        c->hplane = I(e, in, 2);
        c->x = I(e, in, 3);
        c->y = I(e, in, 4);
        c->hx = I(e, in, 5);
        c->hy = I(e, in, 6);
        c->sx = I(e, in, 7);
        c->sy = I(e, in, 8);
        c->hsx = I(e, in, 9);
        c->hsy = I(e, in, 10);
        c->w = I(e, in, 11);
        c->h = I(e, in, 12);
        c->tx = I(e, in, 16);
        c->ty = I(e, in, 17);
        c->htx = I(e, in, 18);
        c->hty = I(e, in, 19);
        break;
    }
    case 71: {
        int i = I(e, in, 0);
        if (i < 0 || i >= 8) break;
        free(e->choice[i].text);
        e->choice[i].text = I(e, in, 3) ? eg_strdup(S(e, in, 1)) : NULL;
        break;
    }
    case 72: {
        snprintf(e->choiceVar, sizeof e->choiceVar, "%s", in->args[0].kind == A_VAR ? in->args[0].s : "_Choices2");
        e->choiceVarIndex = in->args[0].index ? vm_int(e, in->args[0].index) : 0;
        choice_begin(e);
        t->wait = W_CHOICE;
        break;
    }
    case 74:
        e->choiceX = I(e, in, 0);
        e->choiceY = I(e, in, 1);
        break;
    case 80: {
        int n = I(e, in, 0);
        if (n <= 0 || n >= EG_THREADS) break;
        Thread *nt = &e->th[n];
        memset(nt, 0, sizeof *nt);
        nt->sc = vm_script(e, S(e, in, 1));
        nt->active = nt->sc != NULL;
        break;
    }
    case 81: {
        int n = I(e, in, 0);
        if (n > 0 && n < EG_THREADS) {
            e->th[n].active = false;
            if (&e->th[n] == t) t->wait = W_NONE;
        }
        break;
    }
    case 82: {
        int n = I(e, in, 0);
        set_int(e, in, 1, n > 0 && n < EG_THREADS && e->th[n].active);
        break;
    }
    case 86: wait_frames(t, I(e, in, 0)); break;
    case 100: snd_play(e, S(e, in, 0), I(e, in, 1), I(e, in, 2), I(e, in, 3)); break;
    case 110: msg_window_parts(e, I(e, in, 0), S(e, in, 1)); break;
    case 111:
        for (int i = 0; i < 12; i++) e->winRect[i] = I(e, in, i);
        break;
    case 112:
        for (int i = 0; i < 16; i++) e->nameRect[i] = I(e, in, i);
        break;
    case 150: {
        int i = I(e, in, 0);
        if (i < 0 || i >= 16) break;
        e->fonts[i].em = I(e, in, 1);
        e->fonts[i].size = I(e, in, 3);
        e->fonts[i].bold = I(e, in, 2) != 0;
        e->fonts[i].spacing = I(e, in, 4);
        e->fonts[i].lineGap = I(e, in, 5);
        break;
    }
    case 151: {
        int i = I(e, in, 0);
        if (i < 0 || i >= 4) break;
        free(e->nameFmt[i]);
        e->nameFmt[i] = eg_strdup(S(e, in, 2));
        break;
    }
    case 157: msg_clear(e); break;
    case 136:
        screen(e, t, UI_SAVE);
        break;
    case 137:
        if (I(e, in, 0) == 0) {
            ui_open_title_load(e);
            e->scriptUi = true;
            e->scriptUiDone = false;
            t->wait = W_UI;
        } else screen(e, t, UI_LOAD);
        break;
    case 146: screen(e, t, UI_CONFIG); break;
    case 130:
        ui_open_sound(e);
        e->scriptUi = true;
        e->scriptUiDone = false;
        t->wait = W_UI;
        break;
    case 135: screen(e, t, UI_CONFIG); break;
    case 131:
    case 132:
        gallery_open(e, in->n == 131 ? UI_CG : UI_SCENE);
        e->scriptUi = true;
        e->scriptUiDone = false;
        t->wait = W_UI;
        break;
    case 140:
        gallery_scene_end(e, t);
        break;
    case 133:
        ui_message_box(e, 2);
        e->scriptUi = true;
        e->scriptUiDone = false;
        t->wait = W_UI;
        break;
    case 90: break;
    case 91: {
        for (int i = 0; i < 4; i++) e->editRect[i] = I(e, in, i);
        const char *v = S(e, in, 15);
        snprintf(e->editBuf, sizeof e->editBuf, "%s", v);
        e->editMax = I(e, in, 16) > 0 ? I(e, in, 16) : 10;
        e->editing = true;
        break;
    }
    case 92:
        set_int(e, in, 0, e->editing ? 1 : 0);
        break;
    case 93: set_str(e, N(e, in, 0), e->editBuf); break;
    case 159: name_field(e, in); break;
    case 12:
    case 49:
    case 75:
    case 134:
    case 138:
    case 141:
    case 147:
    case 149:
    case 171: break;
    default: unknown(e, in); break;
    }
}

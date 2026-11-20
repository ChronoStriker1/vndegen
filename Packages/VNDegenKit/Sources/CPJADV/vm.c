#include "pj.h"

static i32 imm(u32 a) { return (i32)((a & 0x08000000u) ? (a | 0xf0000000u) : (a & 0x0fffffffu)); }

i32 *pj_var(PJ *p, u32 a) {
    static i32 sink;
    u32 n = a & 0x0fffffffu;
    switch (a >> 28) {
    case 1: return n < PJ_SAVEVARS ? &p->R[n] : &sink;
    case 2: return n < PJ_GLOBALS ? &p->g[n] : &sink;
    case 3: return n < 16 ? &p->reg[n] : &sink;
    case 4: return &p->result;
    default: sink = 0; return &sink;
    }
}

static i32 val(PJ *p, u32 a) { return (a >> 28) == 0 ? imm(a) : *pj_var(p, a); }

static const char *fname(PJ *p, i32 index) {
    const char *n = pj_script_name(&p->sc, index);
    return n ? n : "";
}

static void call(PJ *p, u32 target, u32 ret) {
    if (p->sp >= PJ_STACK) {
        pj_fail(p, "script call stack overflow at %x", ret);
        return;
    }
    p->stack[p->sp++] = ret;
    p->pc = target;
}

static bool ret(PJ *p) {
    if (!p->sp) {
        pj_log(p, "return with an empty stack at %x", p->pc);
        p->halted = true;
        return false;
    }
    u32 v = p->stack[--p->sp];
    if (v & 0x80000000u) {
        p->pc = v & 0x7fffffffu;
        p->wait = (int)(p->stack[--p->sp]);
        return true;
    }
    p->pc = v;
    return true;
}

void pj_call_handler(PJ *p, u32 target) {
    if (p->sp + 2 > PJ_STACK) return;
    p->stack[p->sp++] = (u32)p->wait;
    p->stack[p->sp++] = p->pc | 0x80000000u;
    p->wait = W_NONE;
    p->pc = target;
}

static void begin_group(PJ *p) {
    p->tweenCount = 0;
    p->groupMs = -1;
    p->groupT = 0;
    p->grouping = true;
}

static void add_tween(PJ *p, int layer, int kind, int d0, int d1) {
    if (p->tweenCount >= PJ_TWEENS) return;
    Tween *t = &p->tween[p->tweenCount++];
    memset(t, 0, sizeof *t);
    t->layer = layer;
    t->kind = kind;
    t->d0 = d0;
    t->d1 = d1;
}

static void unfreeze(PJ *p) {
    if (p->frozen) pj_commit(p, 0);
    p->dirty = true;
}

static Button *new_button(PJ *p) {
    for (int i = 0; i < PJ_BUTTONS; i++)
        if (!p->btn[i].used) return &p->btn[i];
    return NULL;
}

static void play_bgm(PJ *p, const char *name, bool loop) {
    const char *intro = pj_ini_get(&p->bgmIni, name, "ogg_intro_src");
    const char *body = pj_ini_get(&p->bgmIni, name, "ogg_loop_src");
    char file[64];
    if (!body) {
        snprintf(file, sizeof file, "%s.ogg", name);
        body = file;
    }
    pj_stop(p, CH_BGM, 1, 0);
    if (intro) pj_play_intro(p, CH_BGM, 0, intro, body);
    else pj_play(p, CH_BGM, 0, body, loop);
}

static void play_se(PJ *p, int slot, const char *name, bool loop) {
    char file[64];
    static const char *const exts[] = {".ogg", ".wav", ".WAV", ""};
    for (int i = 0; exts[i]; i++) {
        snprintf(file, sizeof file, "%s%s", name, exts[i]);
        if (pj_pac_find(&p->pac, file)) {
            pj_play(p, CH_SE, slot, file, loop);
            return;
        }
    }
    pj_log(p, "missing sound %s", name);
}

static void message(PJ *p, i32 voice, u32 nameOff, u32 textOff) {
    free(p->msgName);
    free(p->msgText);
    p->msgName = nameOff ? pj_script_text(&p->sc, nameOff) : NULL;
    p->msgText = pj_script_text(&p->sc, textOff);
    if (p->lang == VNPJADV_LANG_ENGLISH) {
        char *o = p->msgText;
        for (const char *s = p->msgText; *s;) {
            u32 cp;
            int k = pj_utf8_decode(s, &cp);
            if (cp == 0x300c || cp == 0x300d || cp == 0x300e || cp == 0x300f) *o++ = '"';
            else if (cp == 0x3000) *o++ = ' ';
            else {
                memmove(o, s, (size_t)k);
                o += k;
            }
            s += k;
        }
        *o = 0;
    }
    p->msgShown = true;
    p->msgShownChars = 0;
    p->msgT = 0;
    int n = 0;
    for (const char *s = p->msgText; *s;) {
        u32 cp;
        s += pj_utf8_decode(s, &cp);
        n++;
    }
    p->msgTotalChars = n;
    pj_stop(p, CH_VOICE, 0, 0);
    if (voice > 0) {
        char file[64];
        snprintf(file, sizeof file, "%s.ogg", fname(p, voice));
        if (pj_pac_find(&p->pac, file)) pj_play(p, CH_VOICE, 0, file, false);
        else pj_log(p, "missing voice %s", file);
    }
    pj_log(p, "text: %s%s%s", p->msgName ? p->msgName : "", p->msgName ? ": " : "", p->msgText);
    p->dirty = true;
}

static void unknown(PJ *p, u32 pc, const u8 *c, int len) {
    char args[160] = "";
    for (int i = 1; i < len && i < 12; i++) {
        char one[16];
        snprintf(one, sizeof one, " %x", rd32(c + 4 * i));
        strncat(args, one, sizeof args - strlen(args) - 1);
    }
    pj_log(p, "op %02x.%02x.%02x at %x:%s", c[3], c[1], c[2], pc, args);
}

static bool step(PJ *p) {
    if (p->pc + 4 > p->sc.codeSize) {
        pj_fail(p, "script ran past its end");
        return false;
    }
    const u8 *c = p->sc.code + p->pc;
    int len = c[0];
    if (!len || p->pc + (u32)len * 4 > p->sc.codeSize) {
        pj_fail(p, "bad command at %x", p->pc);
        return false;
    }
    u32 at = p->pc;
    u32 a[24] = {0};
    for (int i = 1; i < len && i <= 24; i++) a[i - 1] = rd32(c + 4 * i);
    p->pc += (u32)len * 4;
    int cat = c[3], op = c[1], fl = c[2];
    switch (cat << 16 | op << 8 | fl) {
    case 0x020100: *pj_var(p, a[0]) = val(p, a[1]); break;
    case 0x020200: *pj_var(p, a[0]) += val(p, a[1]); break;
    case 0x020300: *pj_var(p, a[0]) -= val(p, a[1]); break;
    case 0x020400: *pj_var(p, a[0]) *= val(p, a[1]); break;
    case 0x020500: {
        i32 d = val(p, a[1]);
        *pj_var(p, a[0]) = d ? *pj_var(p, a[0]) / d : 0;
        break;
    }
    case 0x020600: {
        i32 d = val(p, a[1]);
        *pj_var(p, a[0]) = d ? *pj_var(p, a[0]) % d : 0;
        break;
    }
    case 0x020700: (*pj_var(p, a[0]))++; break;
    case 0x020800: (*pj_var(p, a[0]))--; break;
    case 0x020900: *pj_var(p, a[0]) &= val(p, a[1]); break;
    case 0x020001: *pj_var(p, a[0]) = val(p, a[0]) == val(p, a[1]); break;
    case 0x020101: *pj_var(p, a[0]) = val(p, a[0]) < val(p, a[1]); break;
    case 0x020201: *pj_var(p, a[0]) = val(p, a[0]) > val(p, a[1]); break;
    case 0x020301: *pj_var(p, a[0]) = val(p, a[0]) <= val(p, a[1]); break;
    case 0x020401: *pj_var(p, a[0]) = val(p, a[0]) >= val(p, a[1]); break;
    case 0x020501: *pj_var(p, a[0]) = val(p, a[0]) && val(p, a[1]); break;
    case 0x020601: *pj_var(p, a[0]) = val(p, a[0]) || val(p, a[1]); break;
    case 0x020701: *pj_var(p, a[0]) = val(p, a[0]) != val(p, a[1]); break;
    case 0x020002: {
        if (p->savedCount < PJ_STACK) memcpy(p->saved[p->savedCount++], p->reg, sizeof p->saved[0]);
        break;
    }
    case 0x020102:
        if (p->savedCount) memcpy(p->reg, p->saved[--p->savedCount], sizeof p->saved[0]);
        break;

    case 0x010200: p->pc = a[0]; break;
    case 0x010300: call(p, a[0], p->pc); break;
    case 0x011300: call(p, a[0], p->pc); break;
    case 0x010400:
    case 0x011000: return ret(p);
    case 0x011200: break;
    case 0x010700:
        if (!val(p, a[0])) p->pc = a[1];
        break;
    case 0x010500:
        p->wait = W_TIME;
        p->waitMs = val(p, a[0]);
        return false;
    case 0x010000:
        pj_log(p, "game exit at %x", at);
        p->quit = true;
        p->halted = true;
        return false;
    case 0x010f00: {
        int k = val(p, a[0]);
        if (k >= 0 && k < 16) p->keyHandler[k] = a[1];
        break;
    }
    case 0x010102: {
        Button *b = new_button(p);
        if (!b) break;
        memset(b, 0, sizeof *b);
        b->used = true;
        b->group = val(p, a[0]);
        b->id = val(p, a[1]);
        b->x1 = val(p, a[2]);
        b->y1 = val(p, a[3]);
        b->x2 = val(p, a[4]);
        b->y2 = val(p, a[5]);
        b->handler = a[8];
        pj_sprite_load(p, &b->spr, fname(p, val(p, a[7])));
        b->spr.id = -1;
        b->spr.parent = -1;
        b->spr.z = ++p->zNext;
        unfreeze(p);
        break;
    }
    case 0x010002: {
        int g = val(p, a[0]);
        for (int i = 0; i < PJ_BUTTONS; i++)
            if (p->btn[i].used && p->btn[i].group == g) {
                pj_sprite_free(&p->btn[i].spr);
                p->btn[i].used = false;
            }
        p->hover = -1;
        p->pressBtn = -1;
        p->dirty = true;
        break;
    }
    case 0x010202:
        p->activeGroup = val(p, a[0]);
        p->R[0] = 0;
        break;
    case 0x010e02: {
        int g = val(p, a[0]), id = val(p, a[1]), dx = val(p, a[2]), dy = val(p, a[3]);
        for (int i = 0; i < PJ_BUTTONS; i++) {
            Button *b = &p->btn[i];
            if (!b->used || b->group != g || (id >= 0 && b->id != id)) continue;
            b->x1 += dx;
            b->x2 += dx;
            b->y1 += dy;
            b->y2 += dy;
            b->spr.x += dx;
            b->spr.y += dy;
        }
        p->dirty = true;
        break;
    }
    case 0x010004: {
        int id = val(p, a[0]);
        for (int i = 0; i < PJ_SLIDERS; i++)
            if (p->slider[i].used && p->slider[i].id == id) {
                pj_sprite_free(&p->slider[i].knob);
                p->slider[i].used = false;
            }
        break;
    }
    case 0x010104: {
        Slider *s = NULL;
        for (int i = 0; i < PJ_SLIDERS && !s; i++)
            if (!p->slider[i].used) s = &p->slider[i];
        if (!s || len < 24) break;
        pj_sprite_free(&s->knob);
        memset(s, 0, sizeof *s);
        s->used = true;
        s->id = val(p, a[0]);
        for (int i = 0; i < 21; i++) s->args[i] = val(p, a[i + 1]);
        s->var = a[22];
        pj_sprite_load(p, &s->knob, fname(p, s->args[1]));
        s->knob.id = -1;
        s->knob.parent = -1;
        s->knob.shown = false;
        break;
    }
    case 0x010304:
    case 0x010504:
    case 0x010704: {
        int id = val(p, a[0]);
        for (int i = 0; i < PJ_SLIDERS; i++) {
            Slider *s = &p->slider[i];
            if (!s->used || s->id != id) continue;
            if (op == 3) {
                s->shown = true;
                s->knob.shown = true;
                s->knob.z = ++p->zNext;
            } else if (op == 5) {
                s->var = a[1];
                s->value = *pj_var(p, a[1]);
            } else {
                s->dx += val(p, a[1]);
                s->dy += val(p, a[2]);
            }
            pj_slider_place(p, s);
        }
        unfreeze(p);
        break;
    }
    case 0x011105:
    case 0x010205:
    case 0x010405:
    case 0x010005:
    case 0x050302:
    case 0x050402:
        break;

    case 0x800001: {
        int mode = val(p, a[0]), ms = val(p, a[1]);
        pj_commit(p, mode ? ms : 0);
        p->wait = mode ? W_TRANS : W_FRAME;
        return false;
    }
    case 0x800101:
    case 0x800201:
        pj_freeze(p);
        pj_layer_load(p, val(p, a[0]), fname(p, val(p, a[1])));
        break;
    case 0x800100:
        break;
    case 0x800c02:
        p->msgFrameFile = val(p, a[0]);
        break;
    case 0x800102:
        p->msgShown = true;
        p->dirty = true;
        break;
    case 0x800202:
        p->msgShown = false;
        p->dirty = true;
        break;
    case 0x800502:
        free(p->msgText);
        free(p->msgName);
        p->msgText = p->msgName = NULL;
        p->msgShown = false;
        p->dirty = true;
        break;
    case 0x800b02:
        break;
    case 0x800300:
        unfreeze(p);
        message(p, val(p, a[0]), (u32)val(p, a[1]), (u32)val(p, a[2]));
        p->wait = W_TEXT;
        p->click = false;
        return false;

    case 0x040000:
        unfreeze(p);
        pj_layer_load(p, val(p, a[0]), fname(p, val(p, a[1])));
        break;
    case 0x040100:
        unfreeze(p);
        pj_layer_clear(p, val(p, a[0]));
        break;
    case 0x040500: {
        Layer *l = pj_layer(p, val(p, a[0]));
        unfreeze(p);
        if (l) l->alpha = val(p, a[1]);
        break;
    }
    case 0x040300: {
        Layer *l = pj_layer(p, val(p, a[0]));
        unfreeze(p);
        if (l) {
            l->x = val(p, a[1]);
            l->y = val(p, a[2]);
        }
        break;
    }
    case 0x041500: {
        Layer *l = pj_layer(p, val(p, a[0]));
        unfreeze(p);
        if (l) {
            l->ox += val(p, a[1]);
            l->oy += val(p, a[2]);
        }
        break;
    }
    case 0x040e00: {
        Layer *l = pj_layer(p, val(p, a[0]));
        unfreeze(p);
        if (l) l->shown = val(p, a[1]) != 0;
        break;
    }
    case 0x041600: {
        Layer *l = pj_layer(p, val(p, a[0]));
        unfreeze(p);
        if (l) {
            l->sx = val(p, a[1]);
            l->sy = val(p, a[2]);
        }
        break;
    }
    case 0x041700: {
        Layer *dst = pj_layer(p, val(p, a[0])), *src = pj_layer(p, val(p, a[1]));
        unfreeze(p);
        if (dst && src && src->used && dst != src) {
            int id = val(p, a[0]);
            pj_layer_load(p, id, src->name);
            dst->x = src->x;
            dst->y = src->y;
            dst->sx = src->sx;
            dst->sy = src->sy;
            dst->alpha = src->alpha;
        }
        break;
    }
    case 0x040001: begin_group(p); break;
    case 0x040501: add_tween(p, val(p, a[1]), TW_ALPHA, val(p, a[2]), 0); break;
    case 0x040201: add_tween(p, val(p, a[1]), TW_MOVE, val(p, a[2]), val(p, a[3])); break;
    case 0x040101:
        unfreeze(p);
        p->grouping = false;
        p->groupMs = val(p, a[0]);
        p->groupT = 0;
        if (p->groupMs <= 0) {
            pj_layers_tick(p, 0);
            break;
        }
        p->wait = W_GROUP;
        return false;

    case 0x050000:
        play_bgm(p, fname(p, val(p, a[0])), val(p, a[1]) != 0);
        break;
    case 0x050100:
        pj_stop(p, CH_BGM, 0, val(p, a[0]));
        pj_stop(p, CH_BGM, 1, val(p, a[0]));
        break;
    case 0x050001:
        play_se(p, clampi(val(p, a[1]), 0, 7), fname(p, val(p, a[0])), false);
        break;
    case 0x050101:
        for (int i = 0; i < 8; i++) pj_stop(p, CH_SE, i, val(p, a[1]));
        break;

    case 0x080100:
    {
        char *t = pj_script_text(&p->sc, (u32)val(p, a[0]));
        pj_log(p, "achievement %s", t);
        free(t);
        break;
    }

    case 0x030000: {
        int w = val(p, a[0]);
        if (w < 0 || w > 1) break;
        MsgWin *m = &p->win[w];
        m->x = val(p, a[1]);
        m->y = val(p, a[2]);
        m->w = val(p, a[3]);
        m->h = val(p, a[4]);
        m->cw = val(p, a[5]);
        m->ch = val(p, a[6]);
        m->pitch = val(p, a[7]);
        m->color = (u32)val(p, a[8]);
        m->font = val(p, a[9]);
        break;
    }
    case 0x031400: {
        int w = val(p, a[0]);
        (void)w;
        break;
    }
    case 0x031500: {
        int w = val(p, a[0]);
        if (w >= 0 && w <= 1) p->win[w].edge = (u32)val(p, a[1]);
        break;
    }
    case 0x031000:
    case 0x030001:
    case 0x030100:
        break;

    case 0x000410: pj_set_volume(p, CH_BGM, (float)val(p, a[0]) / 10000.0f); break;
    case 0x000510: pj_set_volume(p, CH_VOICE, (float)val(p, a[0]) / 10000.0f); break;
    case 0x000610: pj_set_volume(p, CH_SE, (float)val(p, a[0]) / 10000.0f); break;
    case 0x008120: p->result = p->lang; break;
    case 0x000120: p->result = 0; break;

    case 0xf10200:
        if (p->game) pj_game_free(p->game);
        p->game = pj_game_new(p, val(p, a[0]), val(p, a[1]), val(p, a[2]), (i32)a[5]);
        break;
    case 0xf10400:
        if (p->game) pj_game_free(p->game);
        p->game = NULL;
        p->dirty = true;
        break;
    case 0xf10600: p->result = pj_stage_cleared(p, val(p, a[0]), val(p, a[1])); break;
    case 0xf10700: p->result = p->game ? pj_game_result(p->game) : 0; break;

    default:
        if (cat == 0) break;
        unknown(p, at, c, len);
        break;
    }
    return true;
}

void pj_vm_run(PJ *p, int ms) {
    bool frameFree = p->wait == W_FRAME;
    for (int guard = 0; guard < 200000 && !p->halted && !p->quit; guard++) {
        switch (p->wait) {
        case W_TIME:
            p->waitMs -= ms;
            ms = 0;
            if (p->waitMs > 0) return;
            p->wait = W_NONE;
            break;
        case W_GROUP:
            if (p->groupMs >= 0) return;
            p->wait = W_NONE;
            break;
        case W_TRANS:
            if (p->transMs) return;
            p->wait = W_NONE;
            break;
        case W_FRAME:
            if (!frameFree) return;
            frameFree = false;
            p->wait = W_NONE;
            break;
        case W_TEXT:
            return;
        default:
            break;
        }
        if (!step(p)) {
            if (p->wait == W_NONE) return;
        }
    }
}

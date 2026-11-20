#include "anim.h"

static const char *const FMT[256] = {
    [0x01] = "", [0x02] = "b", [0x03] = "", [0x05] = "", [0x0a] = "s", [0x0b] = "A", [0x0c] = "A", [0x0d] = "",
    [0x0e] = "", [0x0f] = "", [0x15] = "", [0x19] = "TT", [0x1a] = "TT", [0x1b] = "TT", [0x1c] = "TT", [0x1d] = "TT",
    [0x1e] = "TT", [0x1f] = "S", [0x21] = "S", [0x22] = "T", [0x26] = "Tii", [0x27] = "TTA", [0x28] = "TTA",
    [0x29] = "TTA", [0x2a] = "TTA", [0x2b] = "TTA", [0x2c] = "TTA", [0x3c] = "TTA", [0x43] = "i", [0x48] = "T",
    [0x4b] = "S", [0x4d] = "TTT", [0x52] = "TS", [0x53] = "bTT", [0x54] = "bTT", [0x55] = "hbbb", [0x57] = "T",
    [0x58] = "", [0x59] = "TTTbT", [0x5d] = "i", [0x5e] = "ddd", [0x5f] = "TTT", [0x66] = "d", [0x6a] = "", [0x6b] = "",
    [0x6d] = "X", [0x7a] = "TTTTT", [0x7c] = "TTT", [0x80] = "TTTT", [0x81] = "TSi", [0x85] = "Ti", [0x86] = "b",
    [0x87] = "", [0x8c] = "hS", [0x8d] = "hSS", [0x8e] = "h", [0x90] = "hS", [0x91] = "h", [0x92] = "S", [0x93] = "",
    [0x94] = "iS", [0x95] = "iS", [0x96] = "iST", [0x97] = "", [0x98] = "T", [0x99] = "i", [0x9b] = "STTTTTTT",
    [0x9e] = "T", [0xa2] = "b", [0xa3] = "i", [0xa6] = "", [0xa7] = "", [0xa8] = "T", [0xa9] = "hT", [0xaa] = "T",
    [0xab] = "T", [0xac] = "T", [0xaf] = "T", [0xb0] = "bb", [0xb2] = "TT", [0xb4] = "", [0xb5] = "", [0xb6] = "",
    [0xb7] = "", [0xb8] = "TT", [0xb9] = "TT", [0xbb] = "T", [0xbc] = "TTTbT", [0xbd] = "T", [0xbe] = "TSi",
    [0xc0] = "TTT", [0xc2] = "TT", [0xc3] = "hS", [0xc4] = "TTT", [0xc5] = "T", [0xc8] = "STTTTTTT", [0xcb] = "i",
    [0xd0] = "C", [0xd1] = "b", [0xd4] = "", [0xde] = "SSSi", [0xdf] = "SSi", [0xe1] = "Si", [0xe6] = "ii",
};

typedef struct Arg {
    u8 tag;
    i32 v;
} Arg;

typedef struct Op {
    u8 code;
    Arg arg[16];
    int n;
    const char *inline_;
    u32 next;
} Op;

enum { TAG_NONE = 0, TAG_LOCAL = 0x40, TAG_GLOBAL = 0x41, TAG_CONST = 0x43, TAG_RAW = 1, TAG_STR = 2, TAG_ADDR = 3 };

static bool decode(AN *a, u32 pc, Op *op) {
    const Script *s = a->sc;
    const u8 *d = s->data;
    u32 end = s->codeEnd;
    memset(op, 0, sizeof *op);
    if (pc >= end) return false;
    op->code = d[pc];
    const char *f = FMT[op->code];
    if (!f) return false;
    u32 q = pc + 1;
    for (; *f; f++) {
        if (op->n >= 15) return false;
        Arg *g = &op->arg[op->n];
        switch (*f) {
        case 'T': {
            if (q >= end) return false;
            u8 t = d[q++];
            g->tag = t;
            if (t == TAG_LOCAL || t == TAG_GLOBAL) {
                g->v = rd16(d + q);
                q += 2;
            } else if (t == TAG_CONST) {
                g->v = (i32)rd32(d + q);
                q += 4;
            } else if (t != TAG_NONE) return false;
            op->n++;
            break;
        }
        case 'b':
            *g = (Arg){TAG_RAW, d[q]};
            q += 1;
            op->n++;
            break;
        case 'h':
            *g = (Arg){TAG_RAW, rd16(d + q)};
            q += 2;
            op->n++;
            break;
        case 'i':
            *g = (Arg){TAG_RAW, (i32)rd32(d + q)};
            q += 4;
            op->n++;
            break;
        case 'A':
            *g = (Arg){TAG_ADDR, (i32)rd32(d + q)};
            q += 4;
            op->n++;
            break;
        case 'S':
            *g = (Arg){TAG_STR, (i32)rd32(d + q)};
            q += 4;
            op->n++;
            break;
        case 'd': {
            double v;
            memcpy(&v, d + q, 8);
            *g = (Arg){TAG_RAW, (i32)(v * 1000.0)};
            q += 8;
            op->n++;
            break;
        }
        case 's':
            op->inline_ = (const char *)d + q;
            while (q < end && d[q]) q++;
            q++;
            break;
        case 'X': {
            u8 t = d[q++];
            *g = (Arg){TAG_RAW, t};
            op->n++;
            int extra = t == 0x28 ? 3 : t == 0x47 ? 5 : 1;
            for (int i = 0; i < extra; i++) {
                if (i == 2 && t == 0x28) {
                    op->arg[op->n++] = (Arg){TAG_RAW, d[q]};
                    q += 1;
                } else {
                    op->arg[op->n++] = (Arg){TAG_RAW, (i32)rd32(d + q)};
                    q += 4;
                }
            }
            break;
        }
        case 'C': {
            int count = d[q++];
            if (count > 16) count = 16;
            *g = (Arg){TAG_RAW, count};
            op->n++;
            for (int i = 0; i < count; i++) {
                a->choice[i].value = (i32)rd32(d + q);
                int str = (i32)rd32(d + q + 4);
                a->choice[i].text = str >= 0 && str < s->strCount ? s->strs[str] : "";
                q += 8;
            }
            break;
        }
        }
    }
    op->next = q;
    return q <= end;
}

static i32 *var(AN *a, const Arg *g) {
    if (g->tag == TAG_LOCAL && g->v >= 0 && g->v < AN_VARS) return &a->L[g->v];
    if (g->tag == TAG_GLOBAL && g->v >= 0 && g->v < AN_VARS) return &a->G[g->v];
    return NULL;
}

static i32 val(AN *a, const Arg *g) {
    i32 *p = var(a, g);
    return p ? *p : g->v;
}

static const char *str(AN *a, const Arg *g) {
    int i = g->v;
    return i >= 0 && i < a->sc->strCount ? a->sc->strs[i] : "";
}

static void set(AN *a, const Arg *g, i32 v) {
    i32 *p = var(a, g);
    if (p) *p = v;
}

static bool compare(int code, i32 x, i32 y) {
    switch (code) {
    case 0x27: return x == y;
    case 0x28: return x != y;
    case 0x29: return x < y;
    case 0x2a: return x > y;
    case 0x2b: return x <= y;
    case 0x2c: return x >= y;
    }
    return false;
}

static void push_log(AN *a, const char *name, const char *text, const char *voice) {
    if (a->logCount == a->logCap) {
        a->logCap = a->logCap ? a->logCap * 2 : 256;
        a->logs = an_realloc(a->logs, sizeof(LogEntry) * (size_t)a->logCap);
    }
    a->logs[a->logCount++] = (LogEntry){(char *)name, (char *)text, (char *)voice};
}

static void show_text(AN *a, const char *name, const char *text, const char *voice) {
    a->name = name && *name ? name : NULL;
    a->text = text;
    a->voiceName = voice;
    a->textStart = a->now;
    a->scene.window = true;
    push_log(a, a->name, text, voice);
    if (voice && *voice) an_snd_play(a, CH_VOICE, voice, false, true);
    a->th.wait = W_TEXT;
}

static Layer *layer(AN *a, const Arg *g) {
    i32 i = val(a, g);
    return i >= 0 && i < AN_LAYERS ? &a->scene.layer[i] : NULL;
}

static bool ends_with(const char *s, const char *tail) {
    size_t n = strlen(s), m = strlen(tail);
    return n >= m && an_ieq(s + n - m, tail);
}

static void skip_case(AN *a) {
    u32 pc = a->th.pc;
    Op op;
    while (decode(a, pc, &op)) {
        if (op.code == 0xd1 || op.code == 0x0a) {
            a->th.pc = pc;
            return;
        }
        pc = op.next;
    }
    a->th.pc = pc;
}

static void end_game(AN *a) {
    a->th.wait = W_END;
    a->phase = PH_TITLE;
    an_snd_stop(a, CH_BGM);
    an_snd_stop(a, CH_VOICE);
    for (int c = CH_SE0; c < CH_COUNT; c++) an_snd_stop(a, c);
    memset(&a->scene, 0, sizeof a->scene);
}

static void exec(AN *a, const Op *op) {
    const Arg *g = op->arg;
    Scene *s = &a->scene;
    switch (op->code) {
    case 0x01:
    case 0x05:
        end_game(a);
        break;
    case 0x0b: a->th.pc = (u32)g[0].v; break;
    case 0x0c:
        if (a->th.sp < AN_STACK) a->th.stack[a->th.sp++] = a->th.pc;
        a->th.pc = (u32)g[0].v;
        break;
    case 0x0d:
        if (a->th.sp > 0) a->th.pc = a->th.stack[--a->th.sp];
        else end_game(a);
        break;
    case 0x19: set(a, &g[0], val(a, &g[0]) + val(a, &g[1])); break;
    case 0x1a: set(a, &g[0], val(a, &g[0]) - val(a, &g[1])); break;
    case 0x1b: set(a, &g[0], val(a, &g[0]) * val(a, &g[1])); break;
    case 0x1c: {
        i32 d = val(a, &g[1]);
        set(a, &g[0], d ? val(a, &g[0]) / d : 0);
        break;
    }
    case 0x1d: {
        i32 d = val(a, &g[1]);
        set(a, &g[0], d ? val(a, &g[0]) % d : 0);
        break;
    }
    case 0x1e: set(a, &g[0], val(a, &g[1])); break;
    case 0x22: set(a, &g[0], 0); break;
    case 0x26: {
        i32 lo = g[1].v, hi = g[2].v;
        set(a, &g[0], hi > lo ? lo + rand() % (hi - lo + 1) : lo);
        break;
    }
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2a:
    case 0x2b:
    case 0x2c:
        if (compare(op->code, val(a, &g[0]), val(a, &g[1]))) a->th.pc = (u32)g[2].v;
        break;
    case 0x3c: a->th.pc = (u32)g[2].v; break;
    case 0x43:
    case 0x99:
        a->th.wait = W_TIME;
        a->th.waitUntil = a->now + (u32)(g[0].v > 0 ? g[0].v : 0);
        break;
    case 0x98:
        a->th.wait = W_TIME;
        a->th.waitUntil = a->now + (u32)(val(a, &g[0]) > 0 ? val(a, &g[0]) : 0);
        break;
    case 0x4b:
        snprintf(s->bg, sizeof s->bg, "%s", str(a, &g[0]));
        memset(s->layer, 0, sizeof s->layer);
        s->bgX = s->bgY = s->bgFromX = s->bgFromY = 0;
        s->bgMoveTime = 0;
        s->bgScale = 1000;
        break;
    case 0x4d:
        s->bgFromX = s->bgX;
        s->bgFromY = s->bgY;
        s->bgX = -val(a, &g[0]);
        s->bgY = -val(a, &g[1]);
        s->bgMoveStart = a->now;
        s->bgMoveTime = (u32)val(a, &g[2]);
        break;
    case 0x80:
        s->bgFromX = s->bgX;
        s->bgFromY = s->bgY;
        s->bgX = val(a, &g[0]);
        s->bgY = val(a, &g[1]);
        s->bgScale = val(a, &g[2]);
        s->bgMoveStart = a->now;
        s->bgMoveTime = (u32)val(a, &g[3]);
        break;
    case 0x52: {
        Layer *l = layer(a, &g[0]);
        if (!l) break;
        const char *name = str(a, &g[1]);
        memset(l, 0, sizeof *l);
        l->kind = ends_with(name, ".psb") ? LK_EMOTE : LK_IMAGE;
        snprintf(l->name, sizeof l->name, "%s", name);
        l->alpha = l->fromAlpha = l->toAlpha = 255;
        l->scale = 1000;
        break;
    }
    case 0x57: {
        Layer *l = layer(a, &g[0]);
        if (l) memset(l, 0, sizeof *l);
        break;
    }
    case 0x59: {
        Layer *l = layer(a, &g[0]);
        if (!l) break;
        u32 t = (u32)val(a, &g[4]);
        l->fromX = t ? l->toX : val(a, &g[1]);
        l->fromY = t ? l->toY : val(a, &g[2]);
        l->toX = val(a, &g[1]);
        l->toY = val(a, &g[2]);
        l->moveStart = a->now;
        l->moveTime = t;
        break;
    }
    case 0x7c: {
        Layer *l = layer(a, &g[0]);
        if (l) l->scale = val(a, &g[1]);
        break;
    }
    case 0x81: {
        Layer *l = layer(a, &g[0]);
        if (l) snprintf(l->motion, sizeof l->motion, "%s", str(a, &g[1]));
        break;
    }
    case 0x6b:
        a->th.wait = W_FRAME;
        a->th.waitUntil = a->now;
        break;
    case 0x6d: {
        int kind = g[0].v;
        u32 ms = (u32)g[1].v;
        if (kind == 0x47) break;
        u32 color = kind == 0x28 ? (u32)g[2].v : 0;
        u32 rgba = (color >> 16 & 0xff) | (color & 0xff00) | (color & 0xff) << 16;
        an_gfx_begin_transition(a, kind, a->skipMode ? 0 : ms, rgba);
        a->th.wait = W_TRANS;
        break;
    }
    case 0x87: an_snd_stop(a, CH_BGM); break;
    case 0x93:
        for (int c = CH_SE0; c < CH_COUNT; c++) an_snd_stop(a, c);
        break;
    case 0x8c:
    case 0x90:
    case 0xc3:
        an_snd_play(a, CH_SE0 + (g[0].v & (AN_SE - 1)), str(a, &g[1]), op->code == 0x8c, true);
        break;
    case 0x8e:
    case 0x91: an_snd_stop(a, CH_SE0 + (g[0].v & (AN_SE - 1))); break;
    case 0xa9: set(a, &g[1], an_snd_playing(a, CH_SE0 + (g[0].v & (AN_SE - 1))) ? 1 : 0); break;
    case 0xb2: {
        char name[16];
        snprintf(name, sizeof name, "%02d", val(a, &g[0]));
        an_snd_play(a, CH_BGM, name, true, false);
        break;
    }
    case 0xb8: an_snd_stop(a, CH_BGM); break;
    case 0xa2: s->window = g[0].v != 0; break;
    case 0xa8: set(a, &g[0], (i32)a->now); break;
    case 0xaa: set(a, &g[0], a->ctrl ? 1 : 0); break;
    case 0xab:
        set(a, &g[0], a->clicked ? 1 : 0);
        a->clicked = false;
        break;
    case 0xac: set(a, &g[0], 0); break;
    case 0xaf: set(a, &g[0], a->skipMode ? 1 : 0); break;
    case 0xd0:
        a->choiceCount = g[0].v;
        a->choiceHover = -1;
        a->th.wait = W_CHOICE;
        break;
    case 0xd1:
        if (a->choiceResult != g[0].v) skip_case(a);
        break;
    case 0xde: show_text(a, str(a, &g[1]), str(a, &g[2]), str(a, &g[0])); break;
    case 0xdf: show_text(a, str(a, &g[0]), str(a, &g[1]), NULL); break;
    case 0xe1: show_text(a, NULL, str(a, &g[0]), NULL); break;
    case 0xe6: end_game(a); break;
    default: break;
    }
}

void an_vm_start(AN *a, u32 pc) {
    memset(&a->th, 0, sizeof a->th);
    a->th.pc = pc;
    a->phase = PH_GAME;
}

bool an_vm_step(AN *a) {
    for (int budget = 0; budget < 20000; budget++) {
        Thread *t = &a->th;
        switch (t->wait) {
        case W_END: return false;
        case W_FRAME:
            if (t->waitUntil == a->now) return true;
            t->wait = W_NONE;
            break;
        case W_TIME:
            if ((i32)(a->now - t->waitUntil) < 0 && !a->skipMode) return true;
            t->wait = W_NONE;
            break;
        case W_TRANS:
            if (!an_gfx_transition_done(a)) return true;
            t->wait = W_NONE;
            break;
        case W_TEXT:
        case W_CHOICE:
        case W_MOVIE: return true;
        }
        Op op;
        if (!decode(a, t->pc, &op)) {
            snprintf(a->problem, sizeof a->problem, "bad instruction %02x at %x", t->pc < a->sc->codeEnd ? a->sc->data[t->pc] : 0, t->pc);
            an_log(a, "%s", a->problem);
            end_game(a);
            return false;
        }
        t->pc = op.next;
        exec(a, &op);
    }
    return true;
}

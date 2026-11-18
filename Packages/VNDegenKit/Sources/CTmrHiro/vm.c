#include "th.h"

static i32 *var_ref(TH *t, int kind, int index) {
    if (kind < 0 || kind > 5 || index < 0 || index >= TH_VARS) return NULL;
    return &t->vars[kind][index];
}

static u32 rnd(TH *t) {
    t->rng = t->rng * 1103515245u + 12345u;
    return t->rng >> 16;
}

static void fail(TH *t, const char *msg) {
    snprintf(t->problem, sizeof t->problem, "%s", msg);
    th_log(t, "problem: %s", msg);
    t->quit = true;
}

bool vm_call(TH *t, const char *scene, const char *label) {
    if (t->sp >= TH_STACK) {
        fail(t, "script call stack overflow");
        return false;
    }
    if (scene && *scene && !scene_load(t, scene)) {
        th_log(t, "call: no scene %s, passed by", scene);
        return true;
    }
    if (t->sc) {
        snprintf(t->stack[t->sp].scene, sizeof t->stack[t->sp].scene, "%s", t->sc->name);
        t->stack[t->sp].pc = t->pc;
        t->sp++;
    }
    vm_goto(t, scene, label);
    return true;
}

void vm_goto(TH *t, const char *scene, const char *label) {
    Scene *s = scene && *scene ? scene_load(t, scene) : t->sc;
    if (!s) {
        th_log(t, "goto: no scene %s", scene ? scene : "");
        return;
    }
    int pc = 0;
    if (label && *label) {
        pc = scene_label(s, label, s == t->sc ? t->pc : 0);
        if (pc < 0) {
            th_log(t, "goto: no label %s in %s", label, s->name);
            if (s == t->sc) return;
            pc = 0;
        }
    }
    if (t->sc != s) {
        th_log(t, "scene %s%s%s", s->name, label && *label ? " at " : "", label && *label ? label : "");
        gallery_mark_scene(t, s->name);
    }
    t->sc = s;
    snprintf(t->scene, sizeof t->scene, "%s", s->name);
    t->pc = pc;
    t->jumped = true;
}

void vm_start(TH *t, const char *scene, const char *label) {
    t->sp = 0;
    t->overlayCount = 0;
    t->sc = NULL;
    vm_goto(t, scene, label);
}

static bool vm_return(TH *t) {
    if (t->sp == 0) {
        th_log(t, "return with an empty stack in %s", t->sc ? t->sc->name : "?");
        return false;
    }
    t->sp--;
    if (t->replayDepth && t->sp < t->replayDepth) t->replayDepth = 0;
    Scene *s = scene_load(t, t->stack[t->sp].scene);
    if (!s) return false;
    t->sc = s;
    snprintf(t->scene, sizeof t->scene, "%s", s->name);
    t->pc = t->stack[t->sp].pc;
    return true;
}

static void split_scene_label(const char *s, char *scene, size_t ss, char *label, size_t ls) {
    const char *comma = strchr(s, ',');
    if (comma) {
        snprintf(scene, ss, "%.*s", (int)(comma - s), s);
        snprintf(label, ls, "%s", comma + 1);
    } else {
        snprintf(scene, ss, "%s", s);
        label[0] = 0;
    }
}

static i32 operand(TH *t, const u8 *b) {
    if (!b[2]) return (i16)rdbe16(b + 4);
    i32 *v = var_ref(t, b[2], rdbe16(b + 4));
    return v ? *v : 0;
}

static bool cond(TH *t, const u8 *b) {
    int op = b[0];
    i32 value = operand(t, b);
    i32 *v = var_ref(t, b[1], b[3]);
    i32 x = v ? *v : 0;
    switch (op) {
    case 0: return x == value;
    case 1: return x != value;
    case 2: return x > value;
    case 3: return x < value;
    case 4: return x >= value;
    case 5: return x <= value;
    default: th_log(t, "cond op %d", op); return false;
    }
}

static void set_var(TH *t, const u8 *b) {
    int op = b[0];
    i32 value = operand(t, b);
    i32 *v = var_ref(t, b[1], b[3]);
    if (!v) return;
    switch (op) {
    case 0: *v = value; break;
    case 1: *v += value; break;
    case 2: *v -= value; break;
    default: th_log(t, "set op %d", op); *v = value; break;
    }
}

static void begin_wait(TH *t, int kind, u32 ms) {
    t->wait = kind;
    t->waitUntil = t->now + ms;
}

static void parse_choice(TH *t, const Rec *r) {
    t->choiceCount = 0;
    int count = r->len > 2 ? r->body[2] : 0;
    const char *p = (const char *)r->body + 3;
    for (int i = 0; i < count && t->choiceCount < 8 && *p; i++) {
        while (*p == ' ') p++;
        if (*p) p++;
        const char *end = strchr(p, ' ');
        size_t n = end ? (size_t)(end - p) : strlen(p);
        char *item = th_alloc(n + 1);
        memcpy(item, p, n);
        char *comma = strrchr(item, ',');
        Choice *c = &t->choices[t->choiceCount++];
        if (comma) {
            *comma = 0;
            snprintf(c->label, sizeof c->label, "%s", comma + 1);
        }
        c->text = item;
        p += n;
    }
}

static void sound_cmd(TH *t, const Rec *r, int ch, int kind, bool loop) {
    const char *name = (const char *)r->body + 4;
    if (r->len < 5 || !*name) return;
    if (t->skipMode && (ch == CH_SE || ch == CH_VOICE)) return;
    if (ch == CH_BGM) snprintf(t->bgmName, sizeof t->bgmName, "%s", name);
    char full[96];
    size_t n = strlen(name);
    snprintf(full, sizeof full, "%s%s", name, n && name[n - 1] == '_' ? "A" : "");
    snd_play(t, ch, kind, full, loop, ch == CH_BGM ? (int)rd16(r->body) : 0);
}

static int step(TH *t) {
    if (!t->sc || t->pc >= t->sc->count) {
        if (!vm_return(t)) {
            fail(t, "the script ended");
            return 0;
        }
        return 1;
    }
    bool jumped = t->jumped;
    t->jumped = false;
    Rec *r = &t->sc->recs[t->pc];
    bool building = r->cls == 0x10 && (r->op == 0x01 || r->op == 0x04 || r->op == 0x33 || r->op == 0x34 || (r->op == 0x00 && (jumped || !t->mapLastActive)));
    bool anyActive = false;
    for (int i = 0; i < t->mapCount; i++)
        if (t->maps[i].kind == 0x20) anyActive = true;
    if (t->mapBuilding && !building && !anyActive) t->mapBuilding = false;
    if (t->mapBuilding && anyActive && !building) {
        t->mapBuilding = false;
        t->hover = -1;
        t->sceneDirty = true;
        begin_wait(t, W_MAP, 0);
        return 0;
    }
    t->pc++;
    const u8 *b = r->body;
    const char *s = (const char *)b;
    switch (r->cls) {
    case 0x00:
        if (r->op == 0x14 || r->op == 0x50 || r->op == 0x51) return 1;
        t->savePc = t->pc - 1;
        msg_show(t, r->op, s);
        return 0;
    case 0x10:
        switch (r->op) {
        case 0x00: return 1;
        case 0x01: {
            if (r->len < 2) return 1;
            if (b[1] == 2) vm_goto(t, s + 2, NULL);
            else vm_goto(t, NULL, s + 2);
            return 1;
        }
        case 0x02: {
            char scene[64], label[64];
            split_scene_label(s + 2, scene, sizeof scene, label, sizeof label);
            vm_call(t, scene, label);
            return 1;
        }
        case 0x03:
            if (!vm_return(t)) {
                fail(t, "the script ended");
                return 0;
            }
            return 1;
        case 0x04:
            if (r->len >= 8 && cond(t, b)) vm_goto(t, NULL, s + 8);
            return 1;
        case 0x14:
            t->savePc = t->pc - 1;
            if (!t->replayDepth) save_auto(t);
            parse_choice(t, r);
            t->hover = -1;
            begin_wait(t, W_CHOICE, 0);
            return 0;
        case 0x28:
            t->buttonCount = 0;
            t->mapCount = 0;
            return 1;
        case 0x29: {
            const char *list = s + 4;
            const char *comma = strchr(list, ',');
            snprintf(t->buttonImg, sizeof t->buttonImg, "%.*s", comma ? (int)(comma - list) : (int)strlen(list), list);
            snprintf(t->buttonImgOn, sizeof t->buttonImgOn, "%s", comma ? comma + 1 : "");
            t->buttonCount = 0;
            return 1;
        }
        case 0x32: {
            if (t->buttonCount >= 32 || r->len < 8) return 1;
            Button *bt = &t->buttons[t->buttonCount++];
            bt->x = rd16(b);
            bt->y = rd16(b + 2);
            bt->w = rd16(b + 4);
            bt->h = rd16(b + 6);
            snprintf(bt->label, sizeof bt->label, "%s", s + 8);
            if (t->pc < t->sc->count && !(t->sc->recs[t->pc].cls == 0x10 && t->sc->recs[t->pc].op == 0x32)) {
                t->hover = -1;
                begin_wait(t, W_BUTTONS, 0);
                return 0;
            }
            return 1;
        }
        case 0x2b: {
            char f[3][64] = {{0}};
            const char *p = s + 4;
            for (int k = 0; k < 3 && p; k++) {
                const char *comma = strchr(p, ',');
                snprintf(f[k], sizeof f[k], "%.*s", comma ? (int)(comma - p) : (int)strlen(p), p);
                p = comma ? comma + 1 : NULL;
            }
            snprintf(t->mapBg, sizeof t->mapBg, "%s", f[0]);
            snprintf(t->mapBase, sizeof t->mapBase, "%s", strcmp(f[1], "0000") ? f[1] : "");
            snprintf(t->mapSheet, sizeof t->mapSheet, "%s", f[2]);
            t->mapMode = b[0];
            t->mapBuilding = true;
            t->savePc = t->pc - 1;
            if (!t->replayDepth) save_auto(t);
            t->mapCount = 0;
            t->mapAnimCount = 0;
            return 1;
        }
        case 0x33: {
            if (t->mapCount >= 16 || r->len < 19) return 1;
            MapButton *m = &t->maps[t->mapCount++];
            memset(m, 0, sizeof *m);
            m->kind = 0x20;
            m->x = (i16)rd16(b + 2);
            m->y = (i16)rd16(b + 4);
            m->sx = rd16(b + 6);
            m->sy = rd16(b + 8);
            m->w = rd16(b + 10);
            m->h = rd16(b + 12);
            m->hx = m->sx + (i16)rd16(b + 14);
            m->hy = m->sy + (i16)rd16(b + 16);
            snprintf(m->label, sizeof m->label, "%s", s + 18);
            t->mapLastActive = true;
            return 1;
        }
        case 0x34: {
            if (t->mapCount >= 16 || r->len < 14) return 1;
            MapButton *m = &t->maps[t->mapCount++];
            memset(m, 0, sizeof *m);
            m->kind = b[0];
            m->x = (i16)rd16(b + 2);
            m->y = (i16)rd16(b + 4);
            m->sx = rd16(b + 6);
            m->sy = rd16(b + 8);
            m->w = rd16(b + 10);
            m->h = rd16(b + 12);
            m->hx = m->sx;
            m->hy = m->sy + m->h;
            t->mapLastActive = m->kind == 0x20;
            if (m->kind == 0x20 && r->len > 26) {
                char tmp[192];
                snprintf(tmp, sizeof tmp, "%s", s + 26);
                char *c1 = strchr(tmp, ',');
                if (c1) {
                    *c1 = 0;
                    char *c2 = strchr(c1 + 1, ',');
                    snprintf(m->label, sizeof m->label, "%s", c2 ? c2 + 1 : "");
                }
                snprintf(m->voice, sizeof m->voice, "%s", tmp);
            }
            return 1;
        }
        default:
            th_log(t, "%s:%d unhandled flow op %04x", t->sc->name, t->pc - 1, r->op);
            return 1;
        }
    case 0x20:
        switch (r->op) {
        case 0x00:
        case 0x01:
        case 0x02: {
            int at = r->op == 0x01 ? 3 : 2;
            if (r->len < at + 1) return 1;
            if (!strncmp(s + at, "fm_", 3)) gallery_mark_cg(t, s + at);
            gfx_set_bg(t, s + at, t->skipMode ? 0 : rd16(b));
            if (r->op == 0x01) t->bgEffect = b[2];
            if (t->transActive) {
                begin_wait(t, W_TRANS, 0);
                return 0;
            }
            return 1;
        }
        case 0x08: {
            if (r->len < 13) return 1;
            const char *name = s + 12;
            Image *im = image_get(t, name);
            if (!im) return 1;
            gfx_begin_transition(t, 1);
            t->pan.active = true;
            t->pan.img = im;
            t->pan.fromY = 0;
            t->pan.toY = t->H - im->ch;
            t->pan.start = t->now;
            t->pan.dur = (u32)(-(i16)rd16(b + 4)) * 20;
            t->bg = NULL;
            snprintf(t->bgName, sizeof t->bgName, "%s", name);
            t->sprCount = 0;
            t->bgEffect = 0;
            t->bgMove.on = false;
            if (b[11] == 'w' && !t->skipMode) {
                begin_wait(t, W_TIME, t->pan.dur);
                t->waitSkippable = true;
                return 0;
            }
            return 1;
        }
        case 0xc8:
            gfx_clear_sprites(t, t->skipMode ? 0 : (r->len >= 2 ? rd16(b) : 0));
            if (t->transActive) {
                begin_wait(t, W_TRANS, 0);
                return 0;
            }
            return 1;
        case 0xcb: {
            if (r->len < 4) return 1;
            gfx_set_sprites(t, s + 3, t->skipMode ? 0 : rd16(b));
            if (t->transActive) {
                begin_wait(t, W_TRANS, 0);
                return 0;
            }
            return 1;
        }
        case 0x6e: {
            if (r->len < 27) return 1;
            const char *name = s + 26;
            if (strncmp(name, "fm_C", 4) && !(name[0] == 'C' && name[1] >= '0' && name[1] <= '9') && strcmp(name, "-")) {
                gfx_set_overlay(t, b, name);
                return 1;
            }
            gfx_set_sprites(t, name, t->skipMode ? 0 : rd16(b));
            if (t->transActive) {
                begin_wait(t, W_TRANS, 0);
                return 0;
            }
            return 1;
        }
        case 0x190:
        case 0x191:
            if (r->len < 10 || t->skipMode) return 1;
            t->flashColor = (u32)b[0] | (u32)b[1] << 8 | (u32)b[2] << 16;
            t->flashStart = t->now;
            t->flashDur = (u32)rd16(b + 6) * 4;
            begin_wait(t, W_FLASH, t->flashDur);
            return 0;
        case 0x1fe: {
            const char *name = r->len > 1 ? s + 1 : "";
            if (!*name || !strcmp(name, "-")) {
                t->navi = NULL;
                t->naviName[0] = 0;
            } else {
                snprintf(t->naviName, sizeof t->naviName, "%s", name);
                t->navi = image_get(t, name);
            }
            t->sceneDirty = true;
            return 1;
        }
        case 0x12c:
            if (r->len < 4 || t->skipMode) return 1;
            if (!movie_play(t, s + 3)) return 1;
            begin_wait(t, W_MOVIE, 0);
            return 0;
        case 0x0c:
            if (r->len < 22) return 1;
            gfx_move_bg(t, b, s + 21);
            if (t->transActive) {
                begin_wait(t, W_TRANS, 0);
                return 0;
            }
            return 1;
        case 0x6b: {
            if (r->len < 24) return 1;
            char spec[160];
            snprintf(spec, sizeof spec, "%s/%d/%.2f/%.2f", s + 23, (i16)rd16(b + 11), rd16(b + 13) / 100.0, rd16(b + 15) / 100.0);
            gfx_set_sprites(t, spec, t->skipMode ? 0 : rd16(b));
            if (t->transActive) {
                begin_wait(t, W_TRANS, 0);
                return 0;
            }
            return 1;
        }
        case 0x1f6:
            if (r->len >= 1) gfx_clear_overlay(t, b[0]);
            return 1;
        case 0x1f7:
            gfx_clear_overlay(t, -1);
            return 1;
        case 0x66:
        case 0x68:
        case 0x69:
        case 0x19a:
        case 0x1f5:
            return 1;
        default:
            th_log(t, "%s:%d unhandled graphics op %04x", t->sc->name, t->pc - 1, r->op);
            return 1;
        }
    case 0x30:
        switch (r->op) {
        case 0x00: sound_cmd(t, r, CH_BGM, PAC_BGM, true); return 1;
        case 0x01: snd_stop(t, CH_BGM, 0); t->bgmName[0] = 0; return 1;
        case 0x02: snd_stop(t, CH_BGM, r->len >= 2 ? rd16(b) : 0); t->bgmName[0] = 0; return 1;
        case 0xc8: sound_cmd(t, r, CH_SE, PAC_SE, false); return 1;
        case 0xc9: sound_cmd(t, r, CH_SE, PAC_SE, true); return 1;
        case 0xca: snd_stop(t, CH_SE, 0); return 1;
        case 0x12c: sound_cmd(t, r, CH_SEH1, PAC_SE, true); return 1;
        case 0x12d: sound_cmd(t, r, CH_SEH2, PAC_SE, true); return 1;
        case 0x130: sound_cmd(t, r, CH_SEH3, PAC_SE, false); return 1;
        case 0x12e:
            snd_stop(t, CH_SEH1, 0);
            snd_stop(t, CH_SEH2, 0);
            snd_stop(t, CH_SEH3, 0);
            return 1;
        case 0x191:
        case 0x196: sound_cmd(t, r, CH_SEHV, PAC_SE, true); return 1;
        case 0x192:
            for (int c = CH_SEHV; c <= CH_SEHV4; c++) snd_stop(t, c, 0);
            return 1;
        case 0x193: {
            if (r->len < 1 || t->skipMode) return 1;
            char first[96];
            snprintf(first, sizeof first, "%.*s", (int)strcspn(s, ","), s);
            int who = 0;
            const char *v = strstr(first, "_V_");
            if (v) who = atoi(v + 3) - 1;
            snd_play(t, CH_SEHV + clampi(who, 0, 3), PAC_SE, first, true, 0);
            return 1;
        }
        case 0x64:
            if (r->len > 0 && !t->skipMode) snd_play(t, CH_SYSVOICE, PAC_VOICE, s, false, 0);
            return 1;
        case 0x65: snd_stop(t, CH_SYSVOICE, 0); return 1;
        case 0x384: snd_stop(t, CH_VOICE, 0); return 1;
        default:
            th_log(t, "%s:%d unhandled sound op %04x", t->sc->name, t->pc - 1, r->op);
            return 1;
        }
    case 0x40:
        switch (r->op) {
        case 0x00:
            t->quit = true;
            return 0;
        case 0x15:
            ui_open(t, UI_LOAD);
            return 0;
        case 0x19:
            return 1;
        case 0x17:
            th_log(t, "open URL %s (ignored)", s);
            return 1;
        case 0x2b:
            t->menuBlocked = r->len > 0 && b[0];
            return 1;
        case 0x3c:
            gallery_page(t, b, r->len);
            return 1;
        case 0x46:
            gallery_item(t, b, r->len);
            if (t->pc < t->sc->count && !(t->sc->recs[t->pc].cls == 0x40 && t->sc->recs[t->pc].op == 0x46)) {
                t->galLoopPc = t->pc;
                t->hover = -1;
                begin_wait(t, W_GALLERY, 0);
                t->sceneDirty = true;
                return 0;
            }
            return 1;
        case 0x3e:
            th_log(t, "character viewer is not supported");
            return 1;
        case 0x28:
        case 0x29:
        case 0x2a:
        case 0x2c:
            return 1;
        default:
            th_log(t, "%s:%d unhandled system op %04x", t->sc->name, t->pc - 1, r->op);
            return 1;
        }
    case 0x50:
        switch (r->op) {
        case 0x00: return 1;
        case 0x01:
            if (r->len >= 6) set_var(t, b);
            return 1;
        case 0x03:
            t->galleryAll = r->len > 0 && b[0];
            return 1;
        case 0x04:
            if (r->len > 1) gallery_mark_scene(t, s + 1);
            return 1;
        case 0x37:
            if (t->skipMode) return 1;
            begin_wait(t, W_TIME, 0x7fffffff);
            t->waitSkippable = true;
            return 0;
        case 0x14:
            t->sceneId = r->len >= 2 ? rd16(b) : 0;
            return 1;
        case 0x15:
            snprintf(t->sceneTitle, sizeof t->sceneTitle, "%s", s);
            return 1;
        case 0x1e: {
            int n = r->len >= 2 ? rd16(b) : 1;
            t->vars[2][0] = n > 0 ? (i32)(rnd(t) % (u32)n) : 0;
            return 1;
        }
        case 0x28:
            if (t->skipMode) return 1;
            begin_wait(t, W_TIME, r->len >= 4 ? rd32(b) : 0);
            t->waitSkippable = true;
            return 0;
        default:
            th_log(t, "%s:%d unhandled scene op %04x", t->sc->name, t->pc - 1, r->op);
            return 1;
        }
    default:
        th_log(t, "%s:%d unknown record %04x:%04x", t->sc->name, t->pc - 1, r->cls, r->op);
        return 1;
    }
}

void vm_run(TH *t) {
    for (int guard = 0; guard < 100000 && !t->quit && t->wait == W_NONE && t->uiMode == UI_NONE; guard++)
        if (!step(t)) break;
}

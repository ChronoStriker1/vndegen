#include "age.h"

void input_init(AG *g) {
    Input *in = &g->in;
    memset(in->keyAction, 0xff, sizeof in->keyAction);
    in->keyAction[0x26] = 0;
    in->keyAction[0x27] = 1;
    in->keyAction[0x28] = 2;
    in->keyAction[0x25] = 3;
    in->keyAction[0x0d] = 4;
    in->keyAction[0x20] = 5;
    in->keyAction[0x08] = 6;
    in->actionCount = 7;
    for (int i = 0; i < 32; i++) { in->joy[i] = 0xffffffffu; in->joyButton[i] = -1; }
    in->mouseTarget = 0xffffffffu;
    in->mouseOwner = -1;
    in->hit = in->hovered = -1;
    in->lastMx = in->lastMy = -100000;
}

static void hit_test(AG *g) {
    Input *in = &g->in;
    in->hit = -1;
    for (int i = 0; i < in->nhot; i++) {
        Hotspot *h = &in->hot[i];
        if (in->mx >= h->x && in->mx <= h->x + h->w && in->my >= h->y && in->my <= h->y + h->h) { in->hit = i; break; }
    }
}

static void wait_callback(AG *g, u32 target) {
    Frame *f = &g->fr[g->waitFrame];
    g->wait = W_NONE;
    f->pc = g->waitPc;
    int top = g->top;
    g->top = g->waitFrame;
    vm_callback(g, target);
    g->top = top;
}

void co_enter(AG *g, int fi, u32 handler, u32 resume, int state) {
    Frame *f = &g->fr[fi];
    g->coState = state;
    g->coResume = resume;
    g->coFrame = fi;
    f->pc = handler;
}

static void advance(AG *g) {
    g->wait = W_NONE;
    g->text.indActive = false;
    Layout *L = &g->text.lay[g->text.indLayout];
    if (L->indHandle >= 0) {
        Obj *o = gfx_obj(g, L->indHandle, false);
        if (o) o->visible = false;
    }
}

void input_service(AG *g) {
    Input *in = &g->in;
    if (in->mx != in->lastMx || in->my != in->lastMy) {
        in->lastMx = in->mx;
        in->lastMy = in->my;
        hit_test(g);
    }
    u32 mask = input_mask(g);
    bool ff = (in->skip && !in->skipSuspended) || (mask & 0x40);
    if (g->wait == W_TEXT) {
        if (ff || (in->pressed & 0x10) || (in->clicks & 1)) {
            text_finish(g);
            in->pressed &= ~0x10u;
            in->clicks &= ~1u;
        }
        if (!text_revealing(g)) g->wait = W_NONE;
        return;
    }
    if (g->wait == W_HIDDEN) {
        if (in->pressed || in->clicks || in->wheel) {
            Frame *f = &g->fr[g->top];
            in->pressed = 0;
            in->clicks = 0;
            g->wait = W_NONE;
            if (f->handlerB != 0xffffffffu) co_enter(g, g->top, f->handlerB, f->pc, 3);
            else g->coState = 0;
        }
        return;
    }
    if (g->wait != W_INPUT) return;
    if (ff) { advance(g); return; }
    if ((in->pressed & 0x20) && g->coState == 0) {
        Frame *f = &g->fr[g->waitFrame];
        if (f->handlerA != 0xffffffffu && g->waitFrame == g->top) {
            in->pressed &= ~0x20u;
            g->wait = W_NONE;
            co_enter(g, g->waitFrame, f->handlerA, g->waitPc, 1);
            return;
        }
    }
    if (in->hotArmed) {
        if (in->hovered != in->hit) {
            if (in->hovered >= 0) {
                u32 t = in->hot[in->hovered].leave;
                in->hovered = -1;
                if (t != 0xffffffffu) { wait_callback(g, t); return; }
            } else {
                in->hovered = in->hit;
                u32 t = in->hit >= 0 ? in->hot[in->hit].enter : 0xffffffffu;
                if (t != 0xffffffffu) { wait_callback(g, t); return; }
            }
        }
        for (int i = 0; i < in->nhot; i++) {
            Hotspot *h = &in->hot[i];
            if (h->action >= 0 && h->action < 32 && (in->pressed & (1u << h->action)) && h->click != 0xffffffffu) {
                in->pressed &= ~(1u << h->action);
                if (h->action == ((in->mouseSlot[0] + 4) & 31)) in->clicks &= ~1u;
                wait_callback(g, h->click);
                return;
            }
        }
        if ((in->clicks & 1) && in->hit >= 0 && in->hot[in->hit].click != 0xffffffffu) {
            in->clicks &= ~1u;
            in->pressed &= ~0x10u;
            wait_callback(g, in->hot[in->hit].click);
            return;
        }
    }
    if ((in->clicks & 1) || (in->pressed & 0x10)) {
        in->clicks &= ~1u;
        in->pressed &= ~0x10u;
        advance(g);
        return;
    }
    if (in->autoMode && !g->snd.mixer) return;
    if (in->autoMode) {
        u32 wait = (u32)g->text.autoTime[1];
        if ((i32)(g->now - in->waitStart) >= (i32)wait) advance(g);
    }
}

void input_end_frame(AG *g) {
    Input *in = &g->in;
    in->pressed = 0;
    in->clicks = 0;
    in->prevMask = input_mask(g);
}

static int op_skip_set(AG *g) { g->in.skip = arg_i(g, 1) != 0; return OP_NEXT; }
static int op_skip_get(AG *g) { arg_set_i(g, 1, g->in.skip); return OP_NEXT; }
static int op_skip_active(AG *g) { arg_set_i(g, 1, (g->in.skip && !g->in.skipSuspended) || (input_mask(g) & 0x40) ? 1 : 0); return OP_NEXT; }
static int op_read_skip_set(AG *g) { g->in.readSkip = arg_i(g, 1) != 0; return OP_NEXT; }
static int op_read_skip_get(AG *g) { arg_set_i(g, 1, g->in.readSkip); return OP_NEXT; }
static int op_read_skip_state(AG *g) { arg_set_i(g, 1, 0); return OP_NEXT; }
static int op_auto_set(AG *g) { g->in.autoMode = arg_i(g, 1) != 0; return OP_NEXT; }
static int op_auto_get(AG *g) { arg_set_i(g, 1, g->in.autoMode); return OP_NEXT; }
static int op_skip_suspend(AG *g) { g->in.skipSuspended = true; return OP_NEXT; }
static int op_skip_resume(AG *g) { g->in.skipSuspended = false; return OP_NEXT; }
static int op_skip_reset(AG *g) { (void)g; return OP_NEXT; }

u32 input_mask(AG *g) {
    Input *in = &g->in;
    u32 m = 0;
    for (int k = 0; k < 256; k++)
        if (in->keys[k] && in->keyAction[k] < 32) m |= 1u << in->keyAction[k];
    for (int b = 0; b < 3; b++)
        if (in->buttons & (1u << b)) m |= 1u << ((in->mouseSlot[b] + 4) & 31);
    return m;
}

static const u8 dik_to_vk[256] = {
    [0x01] = 0x1b, [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5', [0x07] = '6', [0x08] = '7',
    [0x09] = '8', [0x0a] = '9', [0x0b] = '0', [0x0e] = 0x08, [0x0f] = 0x09, [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E',
    [0x13] = 'R', [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P', [0x1c] = 0x0d,
    [0x1d] = 0x11, [0x1e] = 'A', [0x1f] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G', [0x23] = 'H', [0x24] = 'J',
    [0x25] = 'K', [0x26] = 'L', [0x2a] = 0x10, [0x2c] = 'Z', [0x2d] = 'X', [0x2e] = 'C', [0x2f] = 'V', [0x30] = 'B',
    [0x31] = 'N', [0x32] = 'M', [0x36] = 0x10, [0x39] = 0x20, [0x3b] = 0x70, [0x3c] = 0x71, [0x3d] = 0x72,
    [0x3e] = 0x73, [0x3f] = 0x74, [0x40] = 0x75, [0x41] = 0x76, [0x42] = 0x77, [0x43] = 0x78, [0x44] = 0x79,
    [0x57] = 0x7a, [0x58] = 0x7b, [0x9c] = 0x0d, [0x9d] = 0x11, [0xc8] = 0x26, [0xc9] = 0x21, [0xcb] = 0x25,
    [0xcd] = 0x27, [0xd0] = 0x28, [0xd1] = 0x22, [0xd2] = 0x2d, [0xd3] = 0x2e, [0xc7] = 0x24, [0xcf] = 0x23,
};

static int op_action_count(AG *g) {
    i32 n = arg_i(g, 1);
    if (n >= 0 && n < 32) g->in.actionCount = n;
    return OP_NEXT;
}

static int op_map_joy(AG *g) {
    i32 s = arg_i(g, 1);
    if (s >= 0 && s < 32) g->in.joyButton[s] = arg_i(g, 2);
    return OP_NEXT;
}

static int op_map_mouse(AG *g) {
    i32 slot = arg_i(g, 1), b = arg_i(g, 2);
    if (b >= 0 && b < 8) g->in.mouseSlot[b] = (u8)slot;
    return OP_NEXT;
}

static int op_map_key(AG *g) {
    i32 a = arg_i(g, 1), dik = arg_i(g, 2);
    if (dik >= 0 && dik < 256 && dik_to_vk[dik]) g->in.keyAction[dik_to_vk[dik]] = (u8)a;
    return OP_NEXT;
}

static int op_joy_count(AG *g) { arg_set_i(g, 1, 0); return OP_NEXT; }

static int op_joy_register(AG *g) {
    i32 i = arg_i(g, 1);
    if (i >= 0 && i < 32) g->in.joy[i] = (u32)arg_i(g, 2);
    return OP_NEXT;
}

static int op_joy_poll(AG *g) {
    g->in.mask = input_mask(g);
    g->in.scan = 0;
    g->in.scanEmptyDone = false;
    return OP_NEXT;
}

static int op_joy_dispatch(AG *g) {
    Input *in = &g->in;
    if (in->mask) {
        for (int a = in->scan; a < in->actionCount && a < 32; a++) {
            if (!(in->mask & (1u << a))) continue;
            in->scan = a + 1;
            if (in->joy[a] != 0xffffffffu) {
                vm_callback(g, in->joy[a]);
                return OP_JUMP;
            }
        }
        return OP_NEXT;
    }
    int slot = in->actionCount;
    if (slot < 32 && in->joy[slot] != 0xffffffffu) {
        vm_call_local(g, in->joy[slot]);
        return OP_JUMP;
    }
    return OP_NEXT;
}

static int op_mouse_register(AG *g) {
    Input *in = &g->in;
    in->mouseInterval = arg_i(g, 1);
    in->mouseTarget = (u32)arg_i(g, 2);
    in->mouseLast = vm_now(g);
    in->mouseOwner = g->top;
    return OP_NEXT;
}

static int op_mouse_dispatch(AG *g) {
    Input *in = &g->in;
    if ((i32)(vm_now(g) - in->mouseLast) < in->mouseInterval && !in->skip) return OP_NEXT;
    in->mouseLast = vm_now(g);
    if (in->mouseTarget == 0xffffffffu) return OP_NEXT;
    vm_call_local(g, in->mouseTarget);
    return OP_JUMP;
}

static int op_cursor_get(AG *g) {
    arg_set_i(g, 1, g->in.mx);
    arg_set_i(g, 2, g->in.my);
    return OP_NEXT;
}

static int op_cursor_set(AG *g) {
    g->in.mx = arg_i(g, 1);
    g->in.my = arg_i(g, 2);
    return OP_NEXT;
}

static int op_buttons(AG *g) {
    arg_set_i(g, 1, (i32)(g->in.buttons | g->in.latch));
    g->in.latch = 0;
    return OP_NEXT;
}

static int op_wheel(AG *g) {
    arg_set_i(g, 1, g->in.wheel);
    g->in.wheel = 0;
    return OP_NEXT;
}

static int op_sleep(AG *g) {
    i32 ms = arg_i(g, 1);
    if (ms < 1) ms = 1;
    g->wait = W_SLEEP;
    g->waitUntil = vm_now(g) + (u32)ms;
    return OP_WAIT;
}

static int op_hit_rect(AG *g) {
    i32 idx = arg_i(g, 1), px = arg_i(g, 3), py = arg_i(g, 4), n = arg_i(g, 8);
    Ref ref, rects, xo, yo;
    if (!arg_addr(g, 2, &ref) || !arg_addr(g, 5, &rects) || !arg_addr(g, 6, &xo) || !arg_addr(g, 7, &yo)) return OP_NEXT;
    i32 q[4];
    for (int k = 0; k < 4; k++) { Ref c = ref; c.index += k; q[k] = *ref_int(g, c); }
    i32 pl = px + q[0], pr = px + q[1], pt = py + q[2], pb = py + q[3];
    for (i32 i = idx + 1; i < n; i++) {
        i32 r[4];
        for (int k = 0; k < 4; k++) { Ref c = rects; c.index += i * 4 + k; r[k] = *ref_int(g, c); }
        Ref a = xo, b = yo;
        a.index += i;
        b.index += i;
        i32 ox = *ref_int(g, a), oy = *ref_int(g, b);
        if (pl <= r[1] + ox && pr >= r[0] + ox && pt <= r[3] + oy && pb >= r[2] + oy) { arg_set_i(g, 1, i); return OP_NEXT; }
    }
    arg_set_i(g, 1, -1);
    return OP_NEXT;
}

static int op_point_in_poly(AG *g) {
    i32 px = arg_i(g, 2), py = arg_i(g, 3), n = arg_i(g, 6);
    Ref xs, ys;
    if (!arg_addr(g, 4, &xs) || !arg_addr(g, 5, &ys)) return OP_NEXT;
    bool in = false;
    for (i32 i = 0, j = n - 1; i < n; j = i++) {
        Ref xi = xs, yi = ys, xj = xs, yj = ys;
        xi.index += i;
        yi.index += i;
        xj.index += j;
        yj.index += j;
        double x1 = *ref_int(g, xi), y1 = *ref_int(g, yi), x2 = *ref_int(g, xj), y2 = *ref_int(g, yj);
        if ((y1 > py) != (y2 > py) && px < (x2 - x1) * (py - y1) / (y2 - y1) + x1) in = !in;
    }
    arg_set_i(g, 1, in);
    return OP_NEXT;
}

static int op_hot_register(AG *g) {
    Input *in = &g->in;
    in->hit = -1;
    if (in->nhot >= 100) return OP_NEXT;
    Hotspot *h = &in->hot[in->nhot++];
    memset(h, 0, sizeof *h);
    h->x = arg_i(g, 1);
    h->y = arg_i(g, 2);
    h->w = arg_i(g, 3);
    h->h = arg_i(g, 4);
    h->enter = (u32)arg_i(g, 5);
    h->leave = (u32)arg_i(g, 6);
    h->click = (u32)arg_i(g, 7);
    h->action = -1;
    h->owner = g->top;
    return OP_NEXT;
}

static int op_hot_cancel(AG *g) {
    g->in.nhot = 0;
    g->in.hotArmed = false;
    g->in.hit = g->in.hovered = -1;
    return OP_NEXT;
}

static int op_hot_arm(AG *g) {
    if (!g->in.hotArmed) hit_test(g);
    g->in.hotArmed = true;
    return OP_NEXT;
}

static int op_hot_key(AG *g) {
    Input *in = &g->in;
    i32 x = arg_i(g, 1), y = arg_i(g, 2), w = arg_i(g, 3), h = arg_i(g, 4);
    for (int i = 0; i < in->nhot; i++)
        if (in->hot[i].x == x && in->hot[i].y == y && in->hot[i].w == w && in->hot[i].h == h) in->hot[i].action = arg_i(g, 5);
    return OP_NEXT;
}

static int op_cursor_res(AG *g) { (void)g; return OP_NEXT; }

void input_register(void) {
    vm_register(0x86, op_cursor_res);
    vm_register(0x87, op_cursor_res);
    vm_register(0x90, op_hot_register);
    vm_register(0x93, op_hot_cancel);
    vm_register(0x94, op_hot_arm);
    vm_register(0x97, op_hot_key);
    vm_register(0xc8, op_sleep);
    vm_register(0xcc, op_mouse_register);
    vm_register(0xcd, op_mouse_dispatch);
    vm_register(0xfb, op_joy_register);
    vm_register(0xfe, op_action_count);
    vm_register(0xff, op_joy_poll);
    vm_register(0x100, op_joy_dispatch);
    vm_register(0x106, op_joy_count);
    vm_register(0x107, op_map_joy);
    vm_register(0x108, op_buttons);
    vm_register(0x109, op_cursor_get);
    vm_register(0x10a, op_cursor_set);
    vm_register(0x10b, op_map_mouse);
    vm_register(0x10c, op_map_key);
    vm_register(0x10d, op_wheel);
    vm_register(0x12e, op_hit_rect);
    vm_register(0x147, op_point_in_poly);
    vm_register(0x88, op_skip_set);
    vm_register(0x19a, op_skip_get);
    vm_register(0x1c7, op_skip_active);
    vm_register(0x1ca, op_read_skip_set);
    vm_register(0x1cb, op_read_skip_get);
    vm_register(0x1cc, op_read_skip_state);
    vm_register(0x1b6, op_auto_get);
    vm_register(0x1b7, op_auto_set);
    vm_register(0x19b, op_skip_suspend);
    vm_register(0x19c, op_skip_resume);
    vm_register(0x101, op_skip_reset);
    vm_register(0xfa, op_skip_reset);
}

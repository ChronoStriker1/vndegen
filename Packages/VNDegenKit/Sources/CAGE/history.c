#include "age.h"

static u32 bgr(i32 c) { u32 v = (u32)c; return (v >> 16 & 255) | (v & 0xff00) | (v & 255) << 16; }

static HistRec *push(AG *g) {
    Text *t = &g->text;
    if (t->nrec >= t->crec) {
        t->crec = t->crec ? t->crec * 2 : 256;
        t->rec = ag_realloc(t->rec, sizeof(HistRec) * (size_t)t->crec);
    }
    HistRec *r = &t->rec[t->nrec++];
    memset(r, 0, sizeof *r);
    if (t->armGroup) {
        r->d[10] |= HF_GROUP;
        t->armGroup = false;
    }
    return r;
}

void hist_text(AG *g, i32 layout, i32 l, i32 tp, i32 rt, i32 b, const char *s) {
    Text *t = &g->text;
    if (!t->recording) return;
    HistRec *r = push(g);
    r->d[0] = layout;
    r->d[1] = l;
    r->d[2] = tp;
    r->d[3] = rt;
    r->d[4] = b;
    r->d[5] = (i32)bgr(t->color);
    r->d[6] = (i32)bgr(t->effColor);
    r->d[8] = t->effMode;
    r->d[9] = t->size;
    r->text = ag_strdup(s);
}

void hist_break(AG *g, i32 layout) {
    Text *t = &g->text;
    if (!t->recording) return;
    HistRec *r = push(g);
    r->d[0] = layout;
    r->d[5] = (i32)bgr(t->color);
    r->d[6] = (i32)bgr(t->effColor);
    r->d[8] = t->effMode;
    r->d[10] |= HF_BREAK;
}

void hist_voice(AG *g, i32 id, i32 variant) {
    Text *t = &g->text;
    if (!t->recording) return;
    HistRec *r = push(g);
    r->d[0] = t->cur;
    r->d[5] = id;
    r->d[6] = variant;
    r->d[10] |= HF_VOICE;
}

void hist_boundary(AG *g, i32 layout) {
    Text *t = &g->text;
    if (!t->recording) return;
    if (t->nent >= t->cent) {
        t->cent = t->cent ? t->cent * 2 : 64;
        t->ent = ag_realloc(t->ent, sizeof(HistEntry) * (size_t)t->cent);
    }
    t->ent[t->nent].layout = layout;
    t->ent[t->nent].first = t->nrec;
    t->anchor = t->nent;
    t->nent++;
    t->navOff = 0;
    t->armGroup = true;
}

void hist_clear(AG *g) {
    Text *t = &g->text;
    for (int i = 0; i < t->nrec; i++) free(t->rec[i].text);
    t->nrec = 0;
    t->nent = 0;
    t->anchor = 0;
    t->navOff = 0;
    t->armGroup = false;
}

void hist_drop_last_group(AG *g) {
    Text *t = &g->text;
    while (t->nrec > 0) {
        HistRec *r = &t->rec[--t->nrec];
        bool group = r->d[10] & HF_GROUP;
        free(r->text);
        if (group) break;
    }
}

static int op_clear(AG *g) { hist_clear(g); return OP_NEXT; }

static int op_meta(AG *g) {
    Text *t = &g->text;
    if (!t->recording) return OP_NEXT;
    HistRec *r = push(g);
    r->d[0] = t->cur;
    r->d[5] = arg_i(g, 2);
    r->d[6] = arg_i(g, 1);
    r->d[10] |= HF_META;
    return OP_NEXT;
}

static int op_step(AG *g) {
    Text *t = &g->text;
    i32 d = arg_i(g, 3);
    int idx = t->anchor + d;
    if (t->nent == 0 || idx < 0 || idx > t->anchor) {
        arg_set_i(g, 1, -1);
        arg_set_i(g, 2, -1);
        return OP_NEXT;
    }
    arg_set_i(g, 1, t->ent[idx].layout);
    arg_set_i(g, 2, t->ent[idx].first);
    return OP_NEXT;
}

static int op_find(AG *g) {
    Text *t = &g->text;
    i32 first = arg_i(g, 4), type = arg_i(g, 5);
    for (int i = first; i >= 0 && i < t->nrec; i++) {
        HistRec *r = &t->rec[i];
        if (i > first && (r->d[10] & HF_GROUP)) break;
        if ((r->d[10] & HF_META) && r->d[6] == type) {
            arg_set_i(g, 1, 1);
            arg_set_i(g, 2, r->d[5]);
            return OP_NEXT;
        }
    }
    arg_set_i(g, 1, 0);
    return OP_NEXT;
}

static int op_pair(AG *g) {
    Text *t = &g->text;
    i32 first = arg_i(g, 4);
    for (int i = first; i >= 0 && i < t->nrec; i++) {
        HistRec *r = &t->rec[i];
        if (i > first && (r->d[10] & HF_GROUP)) break;
        if (r->d[10] & HF_VOICE) {
            arg_set_i(g, 1, r->d[5]);
            arg_set_i(g, 2, r->d[6]);
            return OP_NEXT;
        }
    }
    arg_set_i(g, 1, -1);
    arg_set_i(g, 2, -1);
    return OP_NEXT;
}

static int op_render(AG *g) {
    text_render_records(g, arg_i(g, 1), arg_i(g, 2), true, arg_i(g, 3), arg_i(g, 4), arg_i(g, 5));
    return OP_NEXT;
}

static int op_redraw(AG *g) {
    text_render_records(g, arg_i(g, 1), arg_i(g, 2), false, arg_i(g, 3), arg_i(g, 4), arg_i(g, 5));
    return OP_NEXT;
}

static int op_redraw_query(AG *g) {
    Text *t = &g->text;
    arg_set_i(g, 1, 0);
    if (!t->nent) { arg_set_i(g, 2, -1); arg_set_i(g, 3, -1); return OP_NEXT; }
    arg_set_i(g, 2, t->ent[t->nent - 1].layout);
    arg_set_i(g, 3, t->ent[t->nent - 1].first);
    return OP_NEXT;
}

static int op_label_picker(AG *g) {
    arg_set_i(g, 1, 0);
    return OP_NEXT;
}

static int op_menu_enabled(AG *g) { (void)g; return OP_NEXT; }

static char diag[1024];

static int op_diag_value(AG *g) {
    size_t n = strlen(diag);
    snprintf(diag + n, sizeof diag - n, "%s", arg_s(g, 1));
    return OP_NEXT;
}

static int op_diag_newline(AG *g) {
    size_t n = strlen(diag);
    snprintf(diag + n, sizeof diag - n, "\r\n");
    return OP_NEXT;
}

static int op_diag_show(AG *g) {
    char *u = ag_sjis_to_utf8(diag);
    ag_log(g, "diagnostic: %s", u);
    free(u);
    diag[0] = 0;
    return OP_NEXT;
}

void history_register(void) {
    vm_register_draw(0x82, op_redraw);
    vm_register(0x83, op_redraw_query);
    vm_register(0x85, op_clear);
    vm_register(0x140, op_label_picker);
    vm_register(0x142, op_menu_enabled);
    vm_register(0x1b2, op_diag_value);
    vm_register(0x1b3, op_diag_newline);
    vm_register(0x1b4, op_diag_show);
    vm_register(0x1d0, op_step);
    vm_register_draw(0x1d1, op_render);
    vm_register(0x1d2, op_meta);
    vm_register(0x1d3, op_find);
    vm_register(0x1d4, op_pair);
}

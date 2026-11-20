#include "anim.h"

enum { SYS_NODE_STATE = 0x17bc, SYS_HISTORY = 0x7f344, HISTORY_MAX = 512, NODE_BASE = 101, NODE_SPAN = 400 };
enum { ZOOM_DEFAULT = 800, SYM = 25, DESC_X = 27, DESC_Y = 566, DESC_W = 1038, DESC_H = 134 };

static bool in_rect(AN *a, int x, int y, int w, int h) { return a->mx >= x && a->my >= y && a->mx < x + w && a->my < y + h; }

void an_flow_load(AN *a) {
    const u8 *d = a->def.data;
    size_t n = a->def.size;
    if (!d) return;
    size_t start = 0;
    int best = 0;
    for (size_t at = 0; at + 20 <= n; at++) {
        if (rd32(d + at) != 0x11) continue;
        int count = 0;
        size_t p = at;
        while (p + 20 <= n && rd32(d + p) == 0x11 && rd32(d + p + 4) <= 8 && rd32(d + p + 8) >= (u32)NODE_BASE &&
               rd32(d + p + 8) < (u32)(NODE_BASE + NODE_SPAN)) {
            count++;
            p += 20;
        }
        if (count > best) {
            best = count;
            start = at;
        }
        if (count) at = p - 1;
    }
    if (best < 2) return;
    a->flow = an_alloc(sizeof(FlowNode) * (size_t)best);
    for (int i = 0; i < best; i++) {
        const u8 *r = d + start + 20 * (size_t)i;
        a->flow[i] = (FlowNode){(int)rd32(r + 4), (int)rd32(r + 8), (int)rd32(r + 12), (int)rd32(r + 16), "", {0}, 0};
    }
    a->flowCount = best;
    size_t p = start + 20 * (size_t)best;
    while (p + 8 < n && rd32(d + p) == 0x12) {
        int id = (int)rd32(d + p + 4);
        const u8 *z = memchr(d + p + 8, 0, n - p - 8);
        if (!z) break;
        for (int i = 0; i < a->flowCount; i++)
            if (a->flow[i].id == id) a->flow[i].text = (const char *)d + p + 8;
        p = (size_t)(z - d) + 1;
    }
    while (p + 12 < n && rd32(d + p) == 0x22) {
        int id = (int)rd32(d + p + 8);
        FlowNode *f = NULL;
        for (int i = 0; i < a->flowCount; i++)
            if (a->flow[i].id == id) f = &a->flow[i];
        p += 12;
        while (p + 4 <= n && rd32(d + p) != 0x80a) {
            if (f && f->linkCount < 4) f->links[f->linkCount++] = (int)rd32(d + p);
            p += 4;
        }
        p += 4;
    }
}

static int node_state(AN *a, int id) {
    size_t off = SYS_NODE_STATE + 4 * (size_t)(id - NODE_BASE);
    if (!a->sys || id < NODE_BASE || id >= NODE_BASE + NODE_SPAN || off + 4 > a->sysSize) return 0;
    return (int)rd32(a->sys + off);
}

static void set_node_state(AN *a, int id, int state) {
    size_t off = SYS_NODE_STATE + 4 * (size_t)(id - NODE_BASE);
    if (!a->sys || id < NODE_BASE || id >= NODE_BASE + NODE_SPAN || off + 4 > a->sysSize) return;
    if ((int)rd32(a->sys + off) >= state) return;
    wr32(a->sys + off, (u32)state);
    u32 count = rd32(a->sys + SYS_HISTORY);
    for (u32 i = 0; i < count && i < HISTORY_MAX; i++)
        if ((int)rd32(a->sys + SYS_HISTORY + 4 + 4 * i) == id) return;
    if (count < HISTORY_MAX && SYS_HISTORY + 8 + 4 * (size_t)count <= a->sysSize) {
        wr32(a->sys + SYS_HISTORY + 4 + 4 * count, (u32)id);
        wr32(a->sys + SYS_HISTORY, count + 1);
    }
}

void an_flow_mark(AN *a, int index) {
    int id = 100 + index;
    set_node_state(a, id, 2);
    for (int i = 0; i < a->flowCount; i++)
        if (a->flow[i].id == id)
            for (int k = 0; k < a->flow[i].linkCount; k++) set_node_state(a, a->flow[i].links[k], 1);
}

static int sx(AN *a, int x) { return x * a->flowZoom / 1000 - a->flowScroll; }
static int sy(AN *a, int y) { return y * a->flowZoom / 1000; }

static int flow_hit(AN *a) {
    if (a->my >= DESC_Y - 10) return -1;
    for (int i = 0; i < a->flowCount; i++) {
        FlowNode *f = &a->flow[i];
        if (!node_state(a, f->id)) continue;
        int x = sx(a, f->x), y = sy(a, f->y), s = SYM * a->flowZoom / 1000;
        if (a->mx >= x && a->my >= y && a->mx < x + s && a->my < y + s) return i;
    }
    return -1;
}

static void line(AN *a, int x0, int y, int x1) {
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    for (int yy = y - 1; yy <= y; yy++) {
        if (yy < 0 || yy >= DESC_Y - 10) continue;
        for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < a->W; x++) a->fb[(size_t)yy * (size_t)a->W + (size_t)x] = 0xff3c2a4au;
    }
}

static void draw_flow(AN *a) {
    Image *canvas = an_image_load(a, "spFlowChartBG");
    an_gfx_fill(a->fb, a->W, a->H, 0xffeed8f0u);
    if (canvas)
        for (int y = 0; y < a->H; y++) {
            u32 *d = a->fb + (size_t)y * (size_t)a->W;
            int cy = y * 1000 / a->flowZoom;
            for (int x = 0; x < a->W; x++) {
                u32 c = an_image_at(canvas, (x + a->flowScroll) * 1000 / a->flowZoom, cy);
                if (c >> 24) d[x] = c | 0xff000000u;
            }
        }
    Image *sym = an_image_load(a, "spFlowChartButton");
    int hover = flow_hit(a), s = SYM * a->flowZoom / 1000;
    for (int i = 0; i < a->flowCount; i++) {
        FlowNode *f = &a->flow[i];
        if (!node_state(a, f->id)) continue;
        for (int k = i + 1; k < a->flowCount; k++)
            if (a->flow[k].y == f->y && node_state(a, a->flow[k].id)) {
                line(a, sx(a, f->x) + s / 2, sy(a, f->y) + s / 2, sx(a, a->flow[k].x) + s / 2);
                break;
            }
    }
    for (int i = 0; i < a->flowCount && sym; i++) {
        FlowNode *f = &a->flow[i];
        int st = node_state(a, f->id);
        if (!st) continue;
        int shape = f->kind == 1 ? 1 : 0, col;
        if (f->kind == 2) col = 10;
        else if (f->kind == 3) col = 4 + shape;
        else if (f->id == a->L[90]) col = 8 + shape;
        else col = (st >= 2 ? 6 : 0) + shape;
        int fy = i == hover ? 30 : 3;
        int fx = 182 + 27 * col;
        int x = sx(a, f->x), y = sy(a, f->y);
        for (int j = 0; j < s; j++) {
            if (y + j < 0 || y + j >= DESC_Y - 10) continue;
            u32 *d = a->fb + (size_t)(y + j) * (size_t)a->W;
            for (int k = 0; k < s; k++) {
                if (x + k < 0 || x + k >= a->W) continue;
                u32 c = an_image_at(sym, fx + k * 1000 / a->flowZoom, fy + j * 1000 / a->flowZoom);
                u32 al = c >> 24;
                if (!al) continue;
                u32 dd = d[x + k], ia = 255 - al;
                u32 rb = (((c & 0xff00ffu) * al + (dd & 0xff00ffu) * ia) >> 8) & 0xff00ffu;
                u32 g = (((c & 0xff00u) * al + (dd & 0xff00u) * ia) >> 8) & 0xff00u;
                d[x + k] = rb | g | 0xff000000u;
            }
        }
    }
    Image *base = an_image_load(a, "spFlowChart");
    if (base) an_gfx_blit(a->fb, a->W, a->H, base, DESC_X, DESC_Y, DESC_W, DESC_H, DESC_X, DESC_Y, 255);
    if (hover >= 0 && a->flow[hover].text[0])
        an_text_draw(a, DESC_X + 34, DESC_Y + 38, a->flow[hover].text, 20, 0xff4a3a46u, 0, DESC_W - 70, 36);
    if (sym) {
        an_gfx_blit(a->fb, a->W, a->H, sym, 2, in_rect(a, 1080, 662, 178, 39) ? 43 : 2, 178, 39, 1080, 662, 255);
        an_gfx_blit(a->fb, a->W, a->H, sym, 2, in_rect(a, 1080, 614, 178, 39) ? 125 : 84, 178, 39, 1080, 614, 255);
        for (int i = 0; i < 3; i++) an_gfx_blit(a->fb, a->W, a->H, sym, 2 + 60 * i, in_rect(a, 1075 + 60 * i, 566, 58, 39) ? 207 : 166, 58, 39, 1075 + 60 * i, 566, 255);
    }
}

void an_flow_open(AN *a, bool fromScript) {
    if (!a->flowCount) return;
    a->screen = SCR_FLOW;
    a->flowFromScript = fromScript;
    a->flowScroll = 0;
    if (!a->flowZoom) a->flowZoom = ZOOM_DEFAULT;
    for (int i = 0; i < a->flowCount; i++)
        if (a->flow[i].id == a->L[90]) {
            int x = a->flow[i].x * a->flowZoom / 1000 - a->W / 2;
            int max = 2048 * a->flowZoom / 1000 - a->W;
            a->flowScroll = clampi(x, 0, max);
        }
}

void an_flow_jump(AN *a, int id) {
    an_reset_scene(a);
    memset(a->L, 0, sizeof a->L);
    a->G[2] = 1;
    a->L[91] = id;
    a->screen = SCR_NONE;
    an_vm_start(a, 8);
}

static void flow_input(AN *a) {
    int max = 2048 * a->flowZoom / 1000 > a->W ? 2048 * a->flowZoom / 1000 - a->W : 0;
    if (a->wheel) {
        a->flowScroll = clampi(a->flowScroll - a->wheel * 80, 0, max);
        a->wheel = 0;
    }
    if (a->rclicked) {
        a->rclicked = false;
        if (!a->flowFromScript) a->screen = SCR_NONE;
        return;
    }
    if (!a->clicked) return;
    a->clicked = false;
    for (int i = 0; i < 3; i++)
        if (in_rect(a, 1075 + 60 * i, 566, 58, 39)) {
            a->flowZoom = i == 1 ? ZOOM_DEFAULT : clampi(a->flowZoom + (i == 0 ? 100 : -100), 500, 1200);
            a->flowScroll = clampi(a->flowScroll, 0, 2048 * a->flowZoom / 1000 > a->W ? 2048 * a->flowZoom / 1000 - a->W : 0);
            return;
        }
    if (in_rect(a, 1080, 662, 178, 39)) {
        if (!a->flowFromScript) a->screen = SCR_NONE;
        return;
    }
    if (a->my < DESC_Y - 10 && a->mx < 40) {
        a->flowScroll = clampi(a->flowScroll - 200, 0, max);
        return;
    }
    if (a->my < DESC_Y - 10 && a->mx > a->W - 40) {
        a->flowScroll = clampi(a->flowScroll + 200, 0, max);
        return;
    }
    int hit = flow_hit(a);
    if (hit >= 0) an_confirm(a, MSG_JUMP, ACT_JUMP, a->flow[hit].id);
}

void an_flow_render(AN *a) {
    if (a->screen == SCR_FLOW) draw_flow(a);
}

bool an_flow_input(AN *a) {
    if (a->screen != SCR_FLOW) return false;
    flow_input(a);
    return true;
}

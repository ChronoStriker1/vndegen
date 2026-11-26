#include "sa.h"

enum {
    POP_CX = 641,
    POP_CY = 360,
    POP_LINE = 24,
    POP_PAD_X = 22,
    POP_PAD_Y = 20,
    POP_TEXT_X = 13,
    POP_TEXT_Y = 10,
    POP_SIZE = 18,
    POP_SPACING = 5,
    POP_EDGE_W = 12,
    POP_EDGE_H = 10,
    POP_SRC_W = 1280,
    POP_SRC_H = 720,
};

static const u32 popColors[] = {0xffffff, 0x00ff00, 0xffff00, 0xffffff, 0xff0000, 0xff00ff};

void sa_popup_load(Image **frame, Vfs *vfs) {
    for (int i = 0; i < 4; i++) {
        char name[16];
        snprintf(name, sizeof name, "pop_a%02d.agf", i);
        frame[i] = sa_load_image(vfs, name);
    }
}

void sa_dim(u32 *fb) {
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) {
        u32 p = fb[i];
        fb[i] = 0xff000000u | ((p >> 16 & 0xff) * 3 / 8) << 16 | ((p >> 8 & 0xff) * 3 / 8) << 8 | (p & 0xff) * 3 / 8;
    }
}

static int line_width(const char *s) {
    int w = 0;
    while (*s) {
        if ((u8)*s < 6) {
            s++;
            continue;
        }
        const char *e = s;
        while (*e && (u8)*e >= 6) e++;
        int n = 0;
        for (const char *q = s; q < e; n++) {
            u32 cp;
            q += sa_utf8_decode(q, &cp);
        }
        w += sa_text_width(s, n, POP_SIZE);
        s = e;
    }
    return w;
}

static void draw_line(u32 *fb, const char *s, int x, int y) {
    u32 color = popColors[0];
    while (*s) {
        if ((u8)*s < 6) {
            color = popColors[(u8)*s];
            s++;
            continue;
        }
        const char *e = s;
        while (*e && (u8)*e >= 6) e++;
        int n = 0;
        for (const char *q = s; q < e; n++) {
            u32 cp;
            q += sa_utf8_decode(q, &cp);
        }
        x += sa_text_draw(fb, SA_W, SA_H, s, n, x, y, POP_SIZE, color, 0x000000);
        s = e;
    }
}

void sa_popup_box(Image *const *frame, u32 *fb, const char *const *lines, int count, int x, int y, int w, int h) {
    sa_font_set_spacing(POP_SPACING);
    sa_blit_rect(fb, frame[3], 0, POP_SRC_H - (h - POP_EDGE_H), POP_EDGE_W, h - POP_EDGE_H, x, y + POP_EDGE_H);
    sa_blit_rect(fb, frame[2], POP_SRC_W - (w - POP_EDGE_W - 2), 0, w - POP_EDGE_W - 2, POP_EDGE_H, x + POP_EDGE_W, y + h - POP_EDGE_H);
    sa_blit_rect(fb, frame[1], 0, 0, POP_EDGE_W, h - POP_EDGE_H, x + w - POP_EDGE_W, y);
    sa_blit_rect(fb, frame[0], 0, 0, w - POP_EDGE_W, POP_EDGE_H, x, y);
    for (int i = 0; i < count; i++) draw_line(fb, lines[i], x + POP_TEXT_X, y + POP_TEXT_Y + i * POP_LINE);
    sa_font_set_spacing(1);
}

void sa_popup_draw(Image *const *frame, u32 *fb, const char *const *lines, int count) {
    sa_font_set_spacing(POP_SPACING);
    int w = 0;
    for (int i = 0; i < count; i++) {
        int lw = line_width(lines[i]);
        if (lw > w) w = lw;
    }
    w += POP_PAD_X;
    int h = POP_PAD_Y + POP_LINE * count;
    sa_popup_box(frame, fb, lines, count, POP_CX - w / 2, POP_CY - h / 2, w, h);
}

// Windows, drawn from a windowskin: XP's 192x128 skin and VX and VX Ace's 128x128 skin. docs/rgss-cleanroom.md
// lists the help topics and the comparisons against RGSS under Wine that fixed the layout.
#include "scene.h"

#include <stdlib.h>

static IRect clip_to(IRect r, IRect clip) {
    int x0 = r.x > clip.x ? r.x : clip.x, y0 = r.y > clip.y ? r.y : clip.y;
    int x1 = r.x + r.w < clip.x + clip.w ? r.x + r.w : clip.x + clip.w;
    int y1 = r.y + r.h < clip.y + clip.h ? r.y + r.h : clip.y + clip.h;
    IRect out = {x0, y0, x1 - x0, y1 - y0};
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

// Draws part of the skin into a screen rectangle, stretched to fill it.
static void stretch(Image *target, IRect clip, const Image *skin, IRect s, IRect d, const Effects *fx) {
    if (d.w <= 0 || d.h <= 0 || s.w <= 0 || s.h <= 0) return;
    if (d.w == s.w && d.h == s.h) {
        draw_image_rect(target, clip_to(d, clip), skin, s, d.x, d.y, fx);
        return;
    }
    IRect c = clip_to(d, clip);
    // One row or column at a time, through the shared blitter, so effects stay in one place.
    for (int y = c.y; y < c.y + c.h; y++) {
        int sy = s.y + (y - d.y) * s.h / d.h;
        for (int x = c.x; x < c.x + c.w;) {
            int sx = s.x + (x - d.x) * s.w / d.w;
            // Extend the run while consecutive screen pixels map to consecutive source pixels.
            int run = 1;
            while (x + run < c.x + c.w && s.x + (x + run - d.x) * s.w / d.w == sx + run) run++;
            IRect src = {sx, sy, run, 1};
            IRect one = {x, y, run, 1};
            draw_image_rect(target, one, skin, src, x, y, fx);
            x += run;
        }
    }
}

// Draws part of the skin repeated across a screen rectangle, from its top left corner.
static void tile(Image *target, IRect clip, const Image *skin, IRect s, IRect d, const Effects *fx) {
    if (s.w <= 0 || s.h <= 0) return;
    IRect area = clip_to(d, clip);
    if (area.w <= 0 || area.h <= 0) return;
    for (int y = d.y; y < d.y + d.h; y += s.h) {
        for (int x = d.x; x < d.x + d.w; x += s.w) {
            int w = d.x + d.w - x < s.w ? d.x + d.w - x : s.w;
            int h = d.y + d.h - y < s.h ? d.y + d.h - y : s.h;
            IRect part = {s.x, s.y, w, h};
            draw_image_rect(target, area, skin, part, x, y, fx);
        }
    }
}

// The frame's nine parts: corners as they are, edges tiled (XP) or stretched (VX and VX Ace), around `d`.
static void frame(Image *target, IRect clip, const Image *skin, IRect s, IRect d, int corner, bool tileEdges,
                  const Effects *fx) {
    int c = corner, inW = s.w - 2 * c, inH = s.h - 2 * c;
    int dw = d.w - 2 * c, dh = d.h - 2 * c;
    void (*edge)(Image *, IRect, const Image *, IRect, IRect, const Effects *) = tileEdges ? tile : stretch;
    if (dw > 0) {
        edge(target, clip, skin, (IRect){s.x + c, s.y, inW, c}, (IRect){d.x + c, d.y, dw, c}, fx);
        edge(target, clip, skin, (IRect){s.x + c, s.y + s.h - c, inW, c}, (IRect){d.x + c, d.y + d.h - c, dw, c}, fx);
    }
    if (dh > 0) {
        edge(target, clip, skin, (IRect){s.x, s.y + c, c, inH}, (IRect){d.x, d.y + c, c, dh}, fx);
        edge(target, clip, skin, (IRect){s.x + s.w - c, s.y + c, c, inH}, (IRect){d.x + d.w - c, d.y + c, c, dh}, fx);
    }
    int cw = d.w < 2 * c ? d.w / 2 : c, ch = d.h < 2 * c ? d.h / 2 : c;
    stretch(target, clip, skin, (IRect){s.x, s.y, cw, ch}, (IRect){d.x, d.y, cw, ch}, fx);
    stretch(target, clip, skin, (IRect){s.x + s.w - cw, s.y, cw, ch}, (IRect){d.x + d.w - cw, d.y, cw, ch}, fx);
    stretch(target, clip, skin, (IRect){s.x, s.y + s.h - ch, cw, ch}, (IRect){d.x, d.y + d.h - ch, cw, ch}, fx);
    stretch(target, clip, skin, (IRect){s.x + s.w - cw, s.y + s.h - ch, cw, ch},
            (IRect){d.x + d.w - cw, d.y + d.h - ch, cw, ch}, fx);
}

// The cursor stretches its middle and keeps a border: 8 pixels on VX and VX Ace skins, 2 on XP's.
static void cursor(Image *target, IRect clip, const Image *skin, IRect s, IRect d, int border, const Effects *fx) {
    if (d.w <= 0 || d.h <= 0) return;
    int b = border;
    if (d.w < 2 * b || d.h < 2 * b) {
        stretch(target, clip, skin, s, d, fx);
        return;
    }
    stretch(target, clip, skin, (IRect){s.x + b, s.y + b, s.w - 2 * b, s.h - 2 * b},
            (IRect){d.x + b, d.y + b, d.w - 2 * b, d.h - 2 * b}, fx);
    frame(target, clip, skin, s, d, b, false, fx);
}

// The cursor's opacity while it blinks, over a 40-frame cycle.
static int cursor_opacity(const Window *w) {
    if (!w->active) return 160;
    int phase = w->cursorCount % 40;
    return phase < 20 ? 255 - phase * 6 : 135 + (phase - 20) * 6;
}

void window_update(Window *w) {
    w->cursorCount = (w->cursorCount + 1) % 40;
    w->pauseCount = (w->pauseCount + 1) % 64;
}

void window_draw(Window *w, Image *target, IRect clip, int offX, int offY) {
    if (!w->visible || w->disposed || w->width <= 0 || w->height <= 0) return;
    bool xp = rgss_version == RGSS1;
    int x = w->x + offX, y = w->y + offY;
    int height = w->height;
    if (!xp && w->openness < 255) {
        height = w->height * w->openness / 255;
        y += (w->height - height) / 2;
        if (height <= 0) return;
    }
    const Image *skin = w->windowskin && !w->windowskin->disposed ? &w->windowskin->image : NULL;
    IRect box = {x, y, w->width, height};
    int opacity = w->opacity;
    if (skin && opacity > 0) {
        Effects back = {NULL, xp ? NULL : w->tone, NULL, opacity * w->backOpacity / 255, 0};
        IRect inner = {x + 2, y + 2, w->width - 4, height - 4};
        if (xp) {
            IRect area = clip_to(inner, clip);
            if (w->stretch) stretch(target, area, skin, (IRect){0, 0, 128, 128}, inner, &back);
            else tile(target, area, skin, (IRect){0, 0, 128, 128}, inner, &back);
        } else {
            IRect area = clip_to(inner, clip);
            stretch(target, area, skin, (IRect){0, 0, 64, 64}, inner, &back);
            tile(target, area, skin, (IRect){0, 64, 64, 64}, inner, &back);
        }
        Effects edge = {NULL, NULL, NULL, opacity, 0};
        frame(target, clip, skin, xp ? (IRect){128, 0, 64, 64} : (IRect){64, 0, 64, 64}, box, 16, xp, &edge);
    }
    if (!xp && w->openness < 255) return;

    int padding = xp ? 16 : w->padding, paddingBottom = xp ? 16 : w->paddingBottom;
    IRect inner = {x + padding, y + padding, w->width - 2 * padding, height - padding - paddingBottom};
    IRect innerClip = clip_to(inner, clip);

    if (skin && w->cursorRect->w > 0 && w->cursorRect->h > 0) {
        Effects fx = {NULL, NULL, NULL, cursor_opacity(w) * w->contentsOpacity / 255, 0};
        IRect d = {inner.x + w->cursorRect->x - (xp ? 0 : w->ox), inner.y + w->cursorRect->y - (xp ? 0 : w->oy),
                   w->cursorRect->w, w->cursorRect->h};
        // XP draws the cursor inside the frame; VX and VX Ace clip it to the contents area.
        IRect cursorClip = xp ? clip_to((IRect){x + 16, y + 16, w->width - 32, height - 32}, clip) : innerClip;
        cursor(target, cursorClip, skin, xp ? (IRect){128, 64, 32, 32} : (IRect){64, 64, 32, 32}, d, xp ? 2 : 8, &fx);
    }

    if (w->contents && !w->contents->disposed) {
        Effects fx = {NULL, NULL, NULL, w->contentsOpacity, 0};
        const Image *c = &w->contents->image;
        draw_image_rect(target, innerClip, c, (IRect){0, 0, c->w, c->h}, inner.x - w->ox, inner.y - w->oy, &fx);
    }

    if (!skin) return;
    Effects deco = {NULL, NULL, NULL, opacity > 0 ? 255 : 0, 0};
    if (opacity <= 0) deco.opacity = 255;
    // Scroll arrows where the contents run past the visible area.
    if (w->contents && !w->contents->disposed && (xp || w->arrowsVisible)) {
        int cw = w->contents->image.w, ch = w->contents->image.h;
        int ax = xp ? 160 : 64, ay = xp ? 64 : 0;
        int midX = x + w->width / 2, midY = y + height / 2;
        int base = xp ? 0 : 16;
        if (w->oy > 0) stretch(target, clip, skin, (IRect){ax + base + 8, ay + base, 16, 8}, (IRect){midX - 8, y + 4, 16, 8}, &deco);
        if (ch - w->oy > inner.h) stretch(target, clip, skin, (IRect){ax + base + 8, ay + base + 24, 16, 8}, (IRect){midX - 8, y + height - 12, 16, 8}, &deco);
        if (w->ox > 0) stretch(target, clip, skin, (IRect){ax + base, ay + base + 8, 8, 16}, (IRect){x + 4, midY - 8, 8, 16}, &deco);
        if (cw - w->ox > inner.w) stretch(target, clip, skin, (IRect){ax + base + 24, ay + base + 8, 8, 16}, (IRect){x + w->width - 12, midY - 8, 8, 16}, &deco);
    }
    if (w->pause) {
        int frameIndex = w->pauseCount / 16 % 4;
        int px = xp ? 160 : 96, py = xp ? 96 : 64;
        IRect s = {px + frameIndex % 2 * 16, py + frameIndex / 2 * 16, 16, 16};
        stretch(target, clip, skin, s, (IRect){x + w->width / 2 - 8, y + height - 16, 16, 16}, &deco);
    }
}

#include "sg.h"

#include <math.h>

static const EffectDef *find_effect(SG *g, Guid id) {
    const Tables *t = &g->b.tables;
    for (int i = 0; i < t->effectCount; i++)
        if (guid_eq(t->effects[i].id, id)) return &t->effects[i];
    return NULL;
}

static int balloon_icon(const char *name) {
    static const struct {
        const char *name;
        int cell;
    } map[] = {{"!Surprise", 8}, {"!Question", 7}, {"!Speechless", 9}, {"!Death", 3}, {"!Lucky", 10}, {"!Heart", 6},
               {"!Sleep", 1},    {"!Devil", 4},    {"!Shy", 12},       {"!Poison", 19}};
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
        if (strcmp(name, map[i].name) == 0) return map[i].cell;
    return 9;
}

static bool effect_header(SG *g, const EffectDef *e, Guid *res, u32 *ms) {
    Rd r = rd_make(e->body, (size_t)e->size);
    rd_guid(&r);
    free(rd_str(&r));
    *res = rd_guid(&r);
    rd_i32(&r);
    rd_i32(&r);
    rd_i32(&r);
    i32 frames = rd_i32(&r);
    if (r.bad || frames <= 0 || frames > 6000) return false;
    *ms = (u32)frames * 1000u / 60u;
    (void)g;
    return true;
}

u32 sgi_fx_start(SG *g, Guid effect, int target, bool balloon) {
    const EffectDef *e = find_effect(g, effect);
    Guid resId;
    u32 ms = 0;
    if (!e || !effect_header(g, e, &resId, &ms)) {
        sg_log(g, "effect not found");
        return balloon ? 1000 : 0;
    }
    const Res *res = res_find(&g->res, resId);
    Image *im = res ? sg_image(g, res->id) : NULL;
    SgiFx *slot = NULL;
    for (int i = 0; i < SGI_FX && !slot; i++)
        if (!g->fi.fx[i].im) slot = &g->fi.fx[i];
    if (!slot || !im) return ms;
    int cols = res->divisions & 255;
    if (cols <= 0) cols = 1;
    *slot = (SgiFx){.im = im, .cols = cols, .rows = (res->divisions >> 8) & 255, .total = ms, .target = target, .balloon = balloon,
                    .icon = balloon ? balloon_icon(e->name) : 0};
    if (slot->rows <= 0) slot->rows = 1;
    sg_log(g, "%s %s on %s, %u ms", balloon ? "balloon" : "effect", e->name, target < 0 ? "player" : "event", ms);
    return ms;
}

void sgi_fx_frame(SG *g, u32 ms) {
    for (int i = 0; i < SGI_FX; i++) {
        SgiFx *f = &g->fi.fx[i];
        if (!f->im) continue;
        f->ms += ms;
        if (f->ms >= f->total) *f = (SgiFx){0};
    }
}

void sgi_fx_clear(SG *g) { memset(g->fi.fx, 0, sizeof g->fi.fx); }

static bool to_screen(SG *g, float x, float y, float z, float *sx, float *sy) {
    const Cam *c = &g->field.cam;
    const float *m = c->m;
    float vx = m[0] * x + m[1] * y + m[2] * z + m[3];
    float vy = m[4] * x + m[5] * y + m[6] * z + m[7];
    float vz = m[8] * x + m[9] * y + m[10] * z + m[11];
    if (vz <= 0.01f) return false;
    *sx = c->cx + vx * c->focal / vz;
    *sy = c->cy + vy * c->focal / vz;
    return true;
}

static float actor_height(const Actor *a) {
    if (a->model) return 1.6f;
    if (a->sheet && a->graphic) {
        int rows = sg_sprite_rows(a->graphic, a->sheet);
        return (float)(a->sheet->h / rows) / 32.0f;
    }
    return 1.0f;
}

static void draw_cell(SG *g, const SgiFx *f, int cell, float cx, float cy, float scale, int alpha, bool add) {
    int cw = f->im->w / f->cols, chh = cw;
    int sx = (cell % f->cols) * cw, sy = (cell / f->cols) * chh;
    int dw = (int)((float)cw * scale), dh = (int)((float)chh * scale);
    int dx = (int)cx - dw / 2, dy = (int)cy - dh / 2;
    if (!add) {
        blit(g->fb, g->w, g->h, f->im, sx, sy, cw, chh, dx, dy, dw, dh, alpha);
        return;
    }
    for (int y = 0; y < dh; y++) {
        int ty = dy + y;
        if (ty < 0 || ty >= g->h) continue;
        int syy = sy + y * chh / dh;
        for (int x = 0; x < dw; x++) {
            int tx = dx + x;
            if (tx < 0 || tx >= g->w) continue;
            u32 s = f->im->px[(size_t)syy * (size_t)f->im->w + (size_t)(sx + x * cw / dw)];
            u32 a = (s >> 24) * (u32)alpha / 255;
            if (!a) continue;
            u32 *d = &g->fb[(size_t)ty * (size_t)g->w + (size_t)tx], o = 0xff000000u;
            for (int k = 0; k < 24; k += 8) {
                u32 v = (*d >> k & 255) + (s >> k & 255) * a / 255;
                o |= (v > 255 ? 255 : v) << k;
            }
            *d = o;
        }
    }
}

static void state_balloon(SG *g) {
    const Party *p = &g->b.party;
    bool poisoned = false;
    for (int i = 0; i < p->count; i++) poisoned |= p->members[i].hp > 0 && (p->members[i].state & 1);
    const Actor *a = &g->field.player;
    if (!poisoned || !a->visible || !a->graphic) return;
    const Tables *t = &g->b.tables;
    const EffectDef *e = NULL;
    for (int i = 0; i < t->effectCount && !e; i++)
        if (t->effects[i].name && strcmp(t->effects[i].name, "!Poison") == 0) e = &t->effects[i];
    Guid resId;
    u32 ms;
    if (!e || !effect_header(g, e, &resId, &ms)) return;
    const Res *res = res_find(&g->res, resId);
    Image *im = res ? sg_image(g, res->id) : NULL;
    float sx, sy;
    if (!im || !to_screen(g, a->x, a->y + actor_height(a), a->z, &sx, &sy)) return;
    SgiFx f = {.im = im, .cols = res->divisions & 255 ? res->divisions & 255 : 1};
    int icon = balloon_icon(e->name), phase = (int)(g->clockMs / 250 % 4);
    draw_cell(g, &f, 0, sx, sy + 2.0f, 1.0f, 255, false);
    draw_cell(g, &f, icon, sx + 6.0f, sy - 5.0f - (float)phase, 0.7f, 255, false);
    draw_cell(g, &f, icon, sx - 9.0f, sy + 3.0f - (float)((phase + 2) % 4), 0.45f, 255, false);
    draw_cell(g, &f, icon, sx - 1.0f, sy + 6.0f, 0.4f, 255, false);
}

void sgi_fx_draw(SG *g) {
    Field *fl = &g->field;
    if (!fl->loaded) return;
    if (g->scene == SCENE_FIELD && !sg_battle_active(g)) state_balloon(g);
    for (int i = 0; i < SGI_FX; i++) {
        const SgiFx *f = &g->fi.fx[i];
        if (!f->im) continue;
        const Actor *a = f->target < 0 ? &fl->player : f->target < fl->npcCount ? &fl->npcs[f->target] : NULL;
        if (!a) continue;
        float t = (float)f->ms / (float)(f->total ? f->total : 1);
        float sx, sy;
        if (f->balloon) {
            if (!to_screen(g, a->x, a->y + actor_height(a), a->z, &sx, &sy)) continue;
            float pop = f->ms < 120 ? 0.6f + 0.4f * (float)f->ms / 120.0f : 1.0f;
            int alpha = t > 0.85f ? (int)(255.0f * (1.0f - t) / 0.15f) : 255;
            float by = sy - 26.0f;
            draw_cell(g, f, 0, sx, by, pop, alpha, false);
            if (f->ms >= 120 && f->icon == 9) {
                draw_cell(g, f, 9, sx - 7.0f, by - 2.0f, 0.8f, alpha, false);
                draw_cell(g, f, 9, sx + 7.0f, by - 2.0f, 0.8f, alpha, false);
            } else if (f->ms >= 120) {
                draw_cell(g, f, f->icon, sx, by - 2.0f, 0.8f, alpha, false);
            }
        } else {
            if (!to_screen(g, a->x, a->y + actor_height(a) * 0.5f, a->z, &sx, &sy)) continue;
            int cells = f->cols * f->rows;
            int cell = (int)(t * (float)cells);
            if (cell >= cells) cell = cells - 1;
            draw_cell(g, f, cell, sx, sy, 1.5f, 255, true);
        }
    }
}

#include "sg.h"

static const int kFxFps = 30;
static const float kFxScale = 0.6f;

static const SgrRec *effect_resource(SG *g, Guid id) {
    Game *d = &g->data;
    for (int k = -1; k < d->packCount; k++) {
        Sgr *s = k < 0 ? &d->resFile : &d->packRes[k];
        for (int i = 0; i < s->count; i++)
            if (s->recs[i].type == SGR_RESOURCE && s->recs[i].sub == RES_EFFECT && s->recs[i].size >= 16 && memcmp(s->recs[i].body, id.b, 16) == 0)
                return &s->recs[i];
    }
    return NULL;
}

bool battle_fx_def(SG *g, Guid effect, BattleFxDef *out) {
    memset(out, 0, sizeof *out);
    const Tables *t = &sgb(g)->tables;
    const EffectDef *e = NULL;
    for (int i = 0; i < t->effectCount && !e; i++)
        if (guid_eq(t->effects[i].id, effect)) e = &t->effects[i];
    if (!e) return false;
    Rd r = rd_make(e->body, (size_t)e->size);
    rd_guid(&r);
    free(rd_str(&r));
    Guid image = rd_guid(&r);
    rd_i32(&r);
    rd_i32(&r);
    out->frames = rd_i32(&r);
    if (r.bad) return false;
    for (i32 k = 16; k + 16 <= e->size; k++) {
        Guid id;
        memcpy(id.b, e->body + k, 16);
        const Res *res = res_find(&g->res, id);
        if (res && res->kind == RES_SE) {
            out->sound = id;
            break;
        }
    }
    const SgrRec *rec = effect_resource(g, image);
    if (!rec) return false;
    Rd q = rd_make(rec->body, rec->size);
    rd_guid(&q);
    free(rd_str(&q));
    free(rd_str(&q));
    out->cols = rd_u8(&q);
    out->rows = rd_u8(&q);
    if (q.bad || out->cols <= 0 || out->rows <= 0) out->cols = out->rows = 1;
    out->image = sg_image(g, image);
    if (out->frames <= 0) out->frames = out->cols * out->rows;
    return out->image != NULL;
}

u32 battle_fx_ms(const BattleFxDef *d) { return (u32)(d->frames * 1000 / kFxFps); }

void battle_fx_draw(SG *g, const BattleFxDef *d, u32 ms, int cx, int cy) {
    const Image *im = d->image;
    if (!im) return;
    int cells = d->cols * d->rows;
    u32 total = battle_fx_ms(d);
    if (ms >= total) return;
    int cell = (int)((unsigned long long)ms * (unsigned long long)cells / (total ? total : 1));
    int cw = im->w / d->cols, ch = im->h / d->rows;
    int w = (int)((float)cw * kFxScale), h = (int)((float)ch * kFxScale);
    blit(g->fb, g->w, g->h, im, (cell % d->cols) * cw, (cell / d->cols) * ch, cw, ch, cx - w / 2, cy - h / 2, w, h, 255);
}

#include "strip.h"

#include <regex.h>

static const char *tabKeys[TAB_COUNT] = {"bg_select", "setting_bgv", "edit_message", "data_store", "setting_bu"};
static const char *noticeKeys[TAB_COUNT] = {"bg", "bgv", "msgwnd", "data", "bu"};

static Image *copy_image(BX *b, const char *name) {
    Image *im = name && *name ? bx_image_load(b, name) : NULL;
    if (!im) return NULL;
    Image *c = bx_image_new(im->w, im->h);
    c->x = im->x;
    c->y = im->y;
    memcpy(c->px, im->px, sizeof(u32) * (size_t)im->w * (size_t)im->h);
    return c;
}

static int ints(const LNode *n, int *out, int max) { return bx_layout_ints(n, NULL, out, max); }

static int name_cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static void load_backgrounds(BX *b, Strip *s) {
    const char *sig = bx_cfg_get(&b->cfg, "main window", "bg signature");
    regex_t re;
    char pattern[160];
    snprintf(pattern, sizeof pattern, "^(%s)$", sig ? sig : "bg[0-9]{5}a[a-z]");
    if (regcomp(&re, pattern, REG_EXTENDED | REG_ICASE | REG_NOSUB)) return;
    const ResIndex *x = &b->arc.ix[RX_GFX];
    for (int i = 0; i < x->count; i++) {
        const char *base = strrchr(x->e[i].name, '/');
        base = base ? base + 1 : x->e[i].name;
        char name[64];
        size_t n = 0;
        while (base[n] && base[n] != '.' && n < sizeof name - 1) {
            name[n] = base[n];
            n++;
        }
        name[n] = 0;
        if (regexec(&re, name, 0, NULL, 0)) continue;
        bool dup = false;
        for (int k = 0; k < s->bgCount && !dup; k++) dup = bx_ieq(s->bgs[k], name);
        if (dup) continue;
        s->bgs = bx_realloc(s->bgs, sizeof(char *) * (size_t)(s->bgCount + 1));
        s->bgs[s->bgCount++] = bx_strdup(name);
    }
    regfree(&re);
    qsort(s->bgs, (size_t)s->bgCount, sizeof(char *), name_cmp);
}

static void load_info(BX *b, Strip *s, const LNode *chars) {
    for (int i = 0; chars && i < chars->count && s->charCount < STRIP_CHARS; i++) {
        const LNode *c = &chars->kids[i];
        BuInfo *f = &s->info[s->charCount++];
        bx_mbtn_node(b, &f->icon, bx_layout_get(c, "icon"), 0, 0);
        const LNode *info = bx_layout_get(c, "info");
        f->wearCount = ints(bx_layout_get(info, "wear"), f->wear, 16);
        f->under = bx_layout_int(info, "under_wear", 1);
        const LNode *face = bx_layout_get(info, "face");
        for (int p = 0; p < 2; p++) f->faceCount[p] = ints(face && p < face->count ? &face->kids[p] : NULL, f->face[p], 16);
    }
}

Strip *bx_strip_open(BX *b) {
    LNode *root = bx_layout_load(b, bx_cfg_get(&b->cfg, "museum phase - bu", "layout"));
    if (!root) return NULL;
    Strip *s = bx_alloc(sizeof *s);
    s->layout = root;
    const LNode *menu = bx_layout_get(root, "main_menu");
    for (int i = 0; i < TAB_COUNT; i++) {
        bx_mbtn_node(b, &s->tabs[i], bx_layout_get(menu, tabKeys[i]), 0, 0);
        s->notice[i] = copy_image(b, bx_layout_str(bx_layout_get(root, "notice"), noticeKeys[i]));
    }
    bx_mbtn_node(b, &s->back, bx_layout_get(menu, "back_to_thumbs"), 0, 0);
    s->noticeOn = -1;

    const LNode *bu = bx_layout_get(root, "setting_bu");
    s->buBase = copy_image(b, bx_layout_str(bu, "base"));
    const LNode *btn = bx_layout_get(bu, "buttons");
    bx_mbtn_node(b, &s->orderUp, bx_layout_get(btn, "order_up"), 0, 0);
    bx_mbtn_node(b, &s->orderDown, bx_layout_get(btn, "order_down"), 0, 0);
    bx_mbtn_node(b, &s->onOff, bx_layout_get(btn, "on_off_button"), 0, 0);
    bx_mbtn_node(b, &s->pointer, bx_layout_get(btn, "pos_pointer"), 0, 0);
    bx_mbtn_node(b, &s->switchBtn, bx_layout_get(btn, "switch"), 0, 0);
    bx_mbtn_node(b, &s->hairBtn, bx_layout_get(btn, "hair_mode"), 0, 0);
    bx_mbtn_node(b, &s->cutinBtn, bx_layout_get(btn, "cutin_mode"), 0, 0);
    bx_mbtn_node(b, &s->steamBtn, bx_layout_get(btn, "steam_mode"), 0, 0);
    bx_mbtn_node(b, &s->aheBtn, bx_layout_get(btn, "ahe_mode"), 0, 0);
    ints(bx_layout_get(bu, "pos_region"), s->posRegion, 4);
    const LNode *rows = bx_layout_get(bu, "switchs");
    for (int r = 0; r < STRIP_ROWS && rows && r < rows->count; r++) {
        ints(bx_layout_get(&rows->kids[r], "pos"), s->rowPos[r], 2);
        ints(bx_layout_get(&rows->kids[r], "space"), s->rowSpace[r], 2);
        s->rowRepeat[r] = bx_layout_int(&rows->kids[r], "repeat_x", 6);
    }
    snprintf(s->poseSig, sizeof s->poseSig, "%s", bx_layout_str(bu, "pose_signature") ? bx_layout_str(bu, "pose_signature") : "ab");
    snprintf(s->viewSig, sizeof s->viewSig, "%s", bx_layout_str(bu, "view_signature") ? bx_layout_str(bu, "view_signature") : "fu");
    s->ngngCount = ints(bx_layout_get(bu, "ngng_convert_table"), s->ngng, 8);
    load_info(b, s, bx_layout_get(bu, "characters"));

    const char *fs = "free-sight screen";
    bx_mbtn_cfg(b, &s->faceBtn, fs, "face button", 0, 0);
    bx_mbtn_cfg(b, &s->bustBtn, fs, "bust button", 0, 0);
    bx_mbtn_cfg(b, &s->hipBtn, fs, "hip button", 0, 0);
    bx_mbtn_cfg(b, &s->knob, fs, "knob", 0, 0);
    bx_cfg_ints(&b->cfg, fs, "meter region", s->meter, 4);

    const LNode *bg = bx_layout_get(root, "bg_select");
    s->bgBase = copy_image(b, bx_layout_str(bg, "base"));
    bx_mbtn_node(b, &s->barBase, bx_layout_get(bg, "bar_base"), 0, 0);
    bx_mbtn_node(b, &s->bar, bx_layout_get(bg, "scroll_bar"), 0, 0);
    s->barInterval = bx_layout_int(bx_layout_get(bg, "scroll_bar"), "interval", 5);
    const LNode *groups = bx_layout_get(bg, "page_select");
    for (int g = 0; groups && g < groups->count && g < 2; g++)
        for (int i = 0; i < groups->kids[g].count && s->pageCount[g] < STRIP_PAGES; i++)
            bx_mbtn_node(b, &s->pages[g][s->pageCount[g]++], &groups->kids[g].kids[i], 0, 0);
    const LNode *tp = bx_layout_get(bg, "thumb_pos");
    for (int i = 0; tp && i < tp->count && s->thumbCount < STRIP_THUMBS; i++) ints(&tp->kids[i], s->thumbPos[s->thumbCount++], 2);
    load_backgrounds(b, s);

    s->hasBgv = bx_cfg_section(&b->cfg, "museum phase - bgv") && s->tabs[TAB_BGV].img;
    bx_strip_data_open(b, s);
    bx_strip_defaults(b, s);
    bx_strip_set_tab(b, s, TAB_BU);
    return s;
}

static void free_thumbs(Strip *s) {
    for (int i = 0; i < STRIP_THUMBS; i++) {
        bx_image_free(s->thumbs[i]);
        s->thumbs[i] = NULL;
    }
    s->thumbsMade = 0;
}

void bx_strip_free(Strip *s) {
    if (!s) return;
    MBtn *bs[] = {&s->back, &s->orderUp, &s->orderDown, &s->onOff, &s->pointer, &s->switchBtn, &s->hairBtn, &s->cutinBtn,
                  &s->steamBtn, &s->aheBtn, &s->faceBtn, &s->bustBtn, &s->hipBtn, &s->knob, &s->barBase, &s->bar};
    for (size_t i = 0; i < sizeof bs / sizeof *bs; i++) bx_mbtn_free(bs[i]);
    for (int i = 0; i < TAB_COUNT; i++) {
        bx_mbtn_free(&s->tabs[i]);
        bx_image_free(s->notice[i]);
    }
    for (int i = 0; i < s->charCount; i++) bx_mbtn_free(&s->info[i].icon);
    for (int g = 0; g < 2; g++)
        for (int i = 0; i < s->pageCount[g]; i++) bx_mbtn_free(&s->pages[g][i]);
    for (int i = 0; i < s->bgCount; i++) free(s->bgs[i]);
    free(s->bgs);
    free_thumbs(s);
    bx_image_free(s->buBase);
    bx_image_free(s->bgBase);
    bx_bgv_free(s->bgv);
    bx_strip_data_close(s);
    free(s->scenePx);
    bx_layout_free(s->layout);
    free(s);
}

BuState *bx_strip_char(Strip *s, int no) {
    for (int i = 0; i < s->charCount; i++)
        if (s->st[i].no == no) return &s->st[i];
    return NULL;
}

void bx_strip_name(const Strip *s, const BuState *c, int ngngIndex, int wear, int under, char *out, size_t size) {
    const BuInfo *f = &s->info[clampi(c->no - 1, 0, s->charCount - 1)];
    int pose = clampi(c->pose, 0, (int)strlen(s->poseSig) - 1);
    int face = f->faceCount[pose] ? f->face[pose][clampi(c->face, 0, f->faceCount[pose] - 1)] : 1;
    int level = s->ngngCount ? s->ngng[clampi(ngngIndex, 0, s->ngngCount - 1)] : ngngIndex;
    snprintf(out, size, "bu%02d%c%d%02d%d%c%02d", c->no, s->viewSig[clampi(c->view, 0, (int)strlen(s->viewSig) - 1)], level,
             f->wearCount ? f->wear[clampi(wear, 0, f->wearCount - 1)] : wear, under + 1, s->poseSig[pose], face);
}

static bool exists(BX *b, const char *name) {
    char body[16];
    snprintf(body, sizeof body, "%.10s", name);
    return bx_res_find(&b->arc, RX_GFX, body) != NULL;
}

static int first_under(BX *b, const Strip *s, const BuState *c, int ngng, int wear) {
    const BuInfo *f = &s->info[clampi(c->no - 1, 0, s->charCount - 1)];
    char name[32];
    for (int u = 0; u < f->under; u++) {
        bx_strip_name(s, c, ngng, wear, u, name, sizeof name);
        if (exists(b, name)) return u;
    }
    return -1;
}

static bool option_ok(BX *b, const Strip *s, const BuState *c, int row, int value) {
    char name[32];
    BuState t = *c;
    switch (row) {
    case STRIP_POSE: t.pose = value; break;
    case STRIP_VIEW: t.view = value; break;
    case STRIP_NGNG: return first_under(b, s, c, value, c->wear) >= 0;
    case STRIP_WEAR: return first_under(b, s, c, c->ngng, value) >= 0;
    case STRIP_UNDER:
        bx_strip_name(s, c, c->ngng, c->wear, value, name, sizeof name);
        return exists(b, name);
    default: return true;
    }
    bx_strip_name(s, &t, t.ngng, t.wear, t.under, name, sizeof name);
    return exists(b, name) || first_under(b, s, &t, t.ngng, t.wear) >= 0;
}

static int row_count(const Strip *s, const BuState *c, int row) {
    const BuInfo *f = &s->info[clampi(c->no - 1, 0, s->charCount - 1)];
    switch (row) {
    case STRIP_POSE: return (int)strlen(s->poseSig);
    case STRIP_VIEW: return (int)strlen(s->viewSig);
    case STRIP_NGNG: return s->ngngCount;
    case STRIP_WEAR: return f->wearCount;
    case STRIP_UNDER: return f->under;
    default: return f->faceCount[clampi(c->pose, 0, 1)];
    }
}

int *bx_strip_field(BuState *c, int row) {
    switch (row) {
    case STRIP_POSE: return &c->pose;
    case STRIP_VIEW: return &c->view;
    case STRIP_NGNG: return &c->ngng;
    case STRIP_WEAR: return &c->wear;
    case STRIP_UNDER: return &c->under;
    default: return &c->face;
    }
}

static void fix_under(BX *b, const Strip *s, BuState *c) {
    char name[32];
    bx_strip_name(s, c, c->ngng, c->wear, c->under, name, sizeof name);
    if (exists(b, name)) return;
    int u = first_under(b, s, c, c->ngng, c->wear);
    if (u >= 0) c->under = u;
}

void bx_strip_defaults(BX *b, Strip *s) {
    s->bgGroup = s->bgPage = s->bgIndex = 0;
    s->flags = b->sc->minor ? 0xb7 : 0;
    memcpy(s->head, b->sc->minor ? (u8[3]){0x1d, 0x00, 0x80} : (u8[3]){0, 0, 0}, 3);
    s->sight = 0;
    s->level = 0;
    s->sel = 0;
    for (int i = 0; i < s->charCount; i++) {
        BuState *c = &s->st[i];
        memset(c, 0, sizeof *c);
        c->no = i + 1;
        c->hidden = 2;
        c->six = 6;
        fix_under(b, s, c);
    }
    s->dirty = true;
}

static bool switch_at(const Strip *s, int row, int i, int *x, int *y) {
    int rep = s->rowRepeat[row] > 0 ? s->rowRepeat[row] : 6;
    *x = s->rowPos[row][0] + (i % rep) * s->rowSpace[row][0];
    *y = s->rowPos[row][1] + (i / rep) * s->rowSpace[row][1];
    return true;
}

static int selected_index(const Strip *s) {
    for (int i = 0; i < s->charCount; i++)
        if (s->st[i].no == s->sel + 1) return i;
    return 0;
}

static int char_x(const BX *b, float pos) { return (int)(pos * (float)b->W / 2.0f); }

static bool close_up(const Strip *s, const BuState *c) {
    const char *u = strchr(s->viewSig, 'u');
    return u && c->view == (int)(u - s->viewSig);
}

static void fixed_positions(BX *b, const Strip *s, const BuState *c, int *pos) {
    char key[64];
    snprintf(key, sizeof key, "fixed position%02d%c", c->no, s->poseSig[clampi(c->pose, 0, (int)strlen(s->poseSig) - 1)]);
    pos[0] = pos[1] = pos[2] = 0;
    if (bx_cfg_ints(&b->cfg, "free-sight screen", key, pos, 3) < 3) bx_cfg_ints(&b->cfg, "free-sight screen", "fixed position", pos, 3);
}

static bool body_span(BX *b, const Strip *s, const BuState *c, int *top, int *height) {
    char name[32], body[16];
    bx_strip_name(s, c, c->ngng, c->wear, c->under, name, sizeof name);
    snprintf(body, sizeof body, "%.10s", name);
    Image *im = bx_image_load(b, body);
    if (!im || im->h <= b->H) return false;
    *top = im->y;
    *height = im->h;
    return true;
}

static int canvas_shift(BX *b, const Strip *s, const BuState *c) {
    char name[32], body[16];
    u8 head[0x1a];
    bx_strip_name(s, c, c->ngng, c->wear, c->under, name, sizeof name);
    snprintf(body, sizeof body, "%.10s", name);
    if (!bx_res_head(&b->arc, RX_GFX, body, head, sizeof head)) return 0;
    int w = rd16(head + 0x16);
    return w > b->W ? (b->W - w) / 2 : 0;
}

static int free_sight(BX *b, const Strip *s, const BuState *c) {
    int pos[3], top, h;
    if (!close_up(s, c) || !body_span(b, s, c, &top, &h)) return 0;
    fixed_positions(b, s, c, pos);
    if (!b->sc->minor) return -(pos[0] - b->H) - (int)(s->sight * (float)(top + h - pos[0]) + 0.5f);
    float v = (float)(b->H + top) + s->sight * (float)(h - b->H);
    int y = -((int)(v + 0.5f) - pos[0]);
    return y > 0 ? 0 : y;
}

static void sfx_bg(BX *b, const char *name, char *out, size_t size) {
    snprintf(out, size, "%s", name);
    const char *rule = bx_cfg_get(&b->cfg, "main window", "sfx bg");
    const char *comma = rule ? strrchr(rule, ',') : NULL;
    if (!comma) return;
    char pattern[128];
    snprintf(pattern, sizeof pattern, "^%.*s$", (int)(comma - rule), rule);
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED | REG_ICASE)) return;
    regmatch_t m[10];
    if (!regexec(&re, name, 10, m, 0)) {
        char alt[64];
        size_t k = 0;
        for (const char *t = comma + 1; *t && k < sizeof alt - 1; t++) {
            while (*t == ' ' || *t == '\t') t++;
            if (*t == '$' && t[1] >= '0' && t[1] <= '9') {
                int g = t[1] - '0';
                for (regoff_t i = m[g].rm_so; g < 10 && m[g].rm_so >= 0 && i < m[g].rm_eo && k < sizeof alt - 1; i++) alt[k++] = name[i];
                t++;
            } else if (*t) alt[k++] = *t;
        }
        alt[k] = 0;
        if (bx_res_find(&b->arc, RX_GFX, alt)) snprintf(out, size, "%s", alt);
    }
    regfree(&re);
}

void bx_strip_compose(BX *b, Strip *s, u32 *dst) {
    Scene sc;
    memset(&sc, 0, sizeof sc);
    int n = 0;
    if (s->bgGroup == 0 && s->bgIndex + s->bgPage * s->thumbCount < s->bgCount) {
        Obj *o = &sc.obj[n++];
        o->used = true;
        o->kind = OK_BG;
        o->alpha = 255;
        snprintf(o->name, sizeof o->name, "%s", s->bgs[s->bgIndex + s->bgPage * s->thumbCount]);
    }
    for (int i = 0; i < s->charCount && n < BX_OBJS; i++) {
        BuState *c = &s->st[i];
        if (c->hidden) continue;
        Obj *o = &sc.obj[n++];
        o->used = true;
        o->kind = OK_CHAR;
        o->charNo = c->no;
        o->alpha = 255;
        o->seq = s->charCount - i;
        bx_strip_name(s, c, c->ngng, c->wear, c->under, o->name, sizeof o->name);
        o->x = char_x(b, c->pos) + canvas_shift(b, s, c);
        o->y = free_sight(b, s, c);
    }
    bool anyone = false;
    for (int i = 0; i < BX_OBJS; i++) anyone |= sc.obj[i].used && sc.obj[i].kind == OK_CHAR;
    for (int i = 0; i < BX_OBJS && anyone; i++)
        if (sc.obj[i].used && sc.obj[i].kind == OK_BG) {
            char name[64];
            sfx_bg(b, sc.obj[i].name, name, sizeof name);
            snprintf(sc.obj[i].name, sizeof sc.obj[i].name, "%s", name);
        }
    int stripLevel = b->stripLevel, viewLevel = b->viewLevel;
    bool hair = b->hairOff;
    b->stripLevel = 0;
    b->viewLevel = 0;
    b->hairOff = s->flags & STRIP_HAIR_OFF;
    if (s->bgGroup == 1) {
        for (size_t i = 0; i < (size_t)b->W * (size_t)b->H; i++) dst[i] = 0xff000000u;
        bx_museum_cg_compose(b, s->bgPage, s->bgIndex, dst, b->hairOff, !(s->flags & STRIP_CUTIN), s->steamOff);
        bx_compose_over(b, &sc, dst);
    } else bx_compose(b, &sc, dst);
    b->stripLevel = stripLevel;
    b->viewLevel = viewLevel;
    b->hairOff = hair;
}

void bx_strip_set_tab(BX *b, Strip *s, int tab) {
    if (tab == TAB_BGV && !s->hasBgv) return;
    s->tab = tab;
    free_thumbs(s);
    bx_bgv_free(s->bgv);
    s->bgv = NULL;
    if (tab == TAB_BGV) s->bgv = bx_bgv_open(b, false);
    if (tab == TAB_DATA) bx_strip_data_enter(b, s);
    s->noticeOn = !(s->noticeSeen & 1u << tab) && s->notice[tab] ? tab : -1;
    s->noticeSeen |= 1u << tab;
}

static int bg_items(BX *b, const Strip *s, int group, int page) {
    if (group == 1) return bx_museum_cg_thumbs(b, page);
    int n = s->bgCount - page * s->thumbCount;
    return clampi(n, 0, s->thumbCount);
}

static void make_bg_thumb(BX *b, Strip *s) {
    int n = bg_items(b, s, s->viewGroup, s->viewPage);
    if (s->thumbsMade >= n || s->thumbsMade >= STRIP_THUMBS) return;
    int i = s->thumbsMade++;
    int tw, th;
    bx_thumb_size(b, &tw, &th);
    if (s->viewGroup == 1) s->thumbs[i] = bx_museum_cg_thumb(b, s->viewPage, i);
    else {
        Image *im = bx_image_load(b, s->bgs[s->viewPage * s->thumbCount + i]);
        if (!im) return;
        u32 *full = bx_alloc(sizeof(u32) * (size_t)b->W * (size_t)b->H);
        for (size_t k = 0; k < (size_t)b->W * (size_t)b->H; k++) full[k] = 0xff000000u;
        bx_blit(full, b->W, b->H, im, im->x, im->y, 255);
        s->thumbs[i] = bx_scaled(b, full, tw, th);
        free(full);
    }
}

static int thumb_at(BX *b, const Strip *s, int n) {
    int tw, th;
    bx_thumb_size(b, &tw, &th);
    for (int i = 0; i < n && i < s->thumbCount; i++)
        if (b->mx >= s->thumbPos[i][0] && b->my >= s->thumbPos[i][1] && b->mx < s->thumbPos[i][0] + tw && b->my < s->thumbPos[i][1] + th) return i;
    return -1;
}

static void bg_update(BX *b, Strip *s) {
    make_bg_thumb(b, s);
    if (!b->click) return;
    for (int g = 0; g < 2; g++)
        for (int i = 0; i < s->pageCount[g]; i++)
            if (bx_mbtn_on(b, &s->pages[g][i])) {
                b->click = false;
                s->viewGroup = g;
                s->viewPage = i;
                free_thumbs(s);
                return;
            }
    int k = thumb_at(b, s, bg_items(b, s, s->viewGroup, s->viewPage));
    if (k < 0) return;
    b->click = false;
    if (s->viewGroup == 1 && bx_museum_cg_seen_variant(b, s->viewPage, k) < 0) return;
    s->bgGroup = s->viewGroup;
    s->bgPage = s->viewPage;
    s->bgIndex = k;
    s->dirty = true;
    bx_snd_play(b, CH_SYS, RX_SYS, "check", false, 0);
}

static void bg_draw(BX *b, Strip *s, u32 *dst) {
    if (s->bgBase) bx_blit(dst, b->W, b->H, s->bgBase, s->bgBase->x, s->bgBase->y, 255);
    int n = bg_items(b, s, s->viewGroup, s->viewPage);
    for (int i = 0; i < n && i < s->thumbCount; i++)
        if (s->thumbs[i]) bx_blit(dst, b->W, b->H, s->thumbs[i], s->thumbPos[i][0], s->thumbPos[i][1], 255);
    for (int g = 0; g < 2; g++)
        for (int i = 0; i < s->pageCount[g]; i++) {
            bool on = g == s->viewGroup && i == s->viewPage;
            bx_mbtn_draw(b, dst, &s->pages[g][i], on ? 2 : bx_mbtn_state(b, &s->pages[g][i]));
        }
    bx_mbtn_draw(b, dst, &s->barBase, 0);
    bx_mbtn_draw(b, dst, &s->bar, 0);
}

static bool on_switch(const BX *b, const Strip *s, int x, int y) {
    if (!s->switchBtn.img) return false;
    int h = s->switchBtn.img->h / s->switchBtn.frames;
    return b->mx >= x && b->my >= y && b->mx < x + s->switchBtn.img->w && b->my < y + h;
}

static void set_pos(BX *b, Strip *s) {
    BuState *c = &s->st[selected_index(s)];
    float t = s->posRegion[2] > 0 ? (float)(b->mx - s->posRegion[0]) / (float)s->posRegion[2] : 0.5f;
    c->pos = t < 0 ? -1.0f : t > 1 ? 1.0f : t * 2.0f - 1.0f;
    s->dirty = true;
}

static void set_sight(BX *b, Strip *s) {
    int h = s->meter[3] - s->meter[1] - (s->knob.img ? s->knob.img->h : 0);
    if (h <= 0) return;
    float t = (float)(b->my - s->meter[1]) / (float)h;
    s->sight = t < 0 ? 0 : t > 1 ? 1 : t;
    s->dirty = true;
}

static void level_sight(BX *b, Strip *s, int level) {
    s->level = level;
    BuState *c = &s->st[selected_index(s)];
    int pos[3], top, h;
    fixed_positions(b, s, c, pos);
    if (body_span(b, s, c, &top, &h)) {
        float t = b->sc->minor ? (float)(pos[level] - b->H - top) / (float)(h - b->H) : (float)(pos[level] - pos[0]) / (float)(top + h - pos[0]);
        s->sight = t < 0 ? 0 : t > 1 ? 1 : t;
    }
    s->dirty = true;
}

static void bu_update(BX *b, Strip *s) {
    if (s->drag == DRAG_POS) {
        if (b->lDown) set_pos(b, s);
        else s->drag = 0;
    } else if (s->drag == DRAG_SIGHT) {
        if (b->lDown) set_sight(b, s);
        else s->drag = 0;
    }
    if (b->lPress) {
        int *r = s->posRegion;
        if (b->mx >= r[0] && b->my >= r[1] && b->mx < r[0] + r[2] && b->my < r[1] + r[3]) {
            s->drag = DRAG_POS;
            set_pos(b, s);
        } else if (b->mx >= s->meter[0] && b->my >= s->meter[1] && b->mx < s->meter[2] && b->my < s->meter[3] + 32) {
            s->drag = DRAG_SIGHT;
            set_sight(b, s);
        }
    }
    if (!b->click) return;
    for (int i = 0; i < s->charCount; i++) {
        MBtn *icon = &s->info[i].icon;
        MBtn onOff = s->onOff;
        onOff.x += icon->x;
        onOff.y += icon->y;
        BuState *c = bx_strip_char(s, i + 1);
        if (bx_mbtn_on(b, &onOff) || bx_mbtn_on(b, icon)) {
            b->click = false;
            c->hidden = c->hidden ? 0 : 2;
            s->sel = i;
            s->dirty = true;
            return;
        }
    }
    int k = selected_index(s);
    if (bx_mbtn_on(b, &s->orderUp) || bx_mbtn_on(b, &s->orderDown)) {
        b->click = false;
        int to = k + (bx_mbtn_on(b, &s->orderUp) ? 1 : -1);
        if (to < 0 || to >= s->charCount) return;
        BuState t = s->st[k];
        s->st[k] = s->st[to];
        s->st[to] = t;
        s->dirty = true;
        return;
    }
    BuState *c = &s->st[k];
    for (int row = 0; row < STRIP_ROWS && !c->hidden; row++) {
        for (int i = 0; i < row_count(s, c, row); i++) {
            int x, y;
            switch_at(s, row, i, &x, &y);
            if (!on_switch(b, s, x, y)) continue;
            b->click = false;
            if (!option_ok(b, s, c, row, i)) return;
            *bx_strip_field(c, row) = i;
            if (row == STRIP_POSE) c->face = clampi(c->face, 0, row_count(s, c, STRIP_FACE) - 1);
            fix_under(b, s, c);
            s->dirty = true;
            return;
        }
    }
    if (bx_mbtn_on(b, &s->hairBtn)) s->flags ^= STRIP_HAIR_OFF;
    else if (bx_mbtn_on(b, &s->cutinBtn)) s->flags ^= STRIP_CUTIN;
    else if (bx_mbtn_on(b, &s->steamBtn)) s->steamOff = !s->steamOff;
    else if (bx_mbtn_on(b, &s->faceBtn)) level_sight(b, s, 0);
    else if (bx_mbtn_on(b, &s->bustBtn)) level_sight(b, s, 1);
    else if (bx_mbtn_on(b, &s->hipBtn)) level_sight(b, s, 2);
    else return;
    b->click = false;
    s->dirty = true;
}

static void bu_draw(BX *b, Strip *s, u32 *dst) {
    if (s->buBase) bx_blit(dst, b->W, b->H, s->buBase, s->buBase->x, s->buBase->y, 255);
    for (int i = 0; i < s->charCount; i++) {
        MBtn *icon = &s->info[i].icon;
        BuState *c = bx_strip_char(s, i + 1);
        bx_mbtn_draw(b, dst, icon, i == s->sel ? 2 : bx_mbtn_state(b, icon));
        MBtn onOff = s->onOff;
        onOff.x += icon->x;
        onOff.y += icon->y;
        bx_mbtn_draw(b, dst, &onOff, (c && !c->hidden ? 0 : 2) + bx_mbtn_state(b, &onOff));
    }
    bx_mbtn_draw(b, dst, &s->orderUp, bx_mbtn_state(b, &s->orderUp));
    bx_mbtn_draw(b, dst, &s->orderDown, bx_mbtn_state(b, &s->orderDown));
    BuState *c = &s->st[selected_index(s)];
    for (int row = 0; row < STRIP_ROWS && s->switchBtn.img && !c->hidden; row++)
        for (int i = 0; i < row_count(s, c, row); i++) {
            int x, y;
            switch_at(s, row, i, &x, &y);
            MBtn t = s->switchBtn;
            t.x = x;
            t.y = y;
            bx_mbtn_draw(b, dst, &t, *bx_strip_field(c, row) == i ? 1 : option_ok(b, s, c, row, i) ? 0 : 2);
        }
    MBtn p = s->pointer;
    if (p.img) p.x = s->posRegion[0] + (int)((c->pos + 1.0f) / 2.0f * (float)s->posRegion[2]) - p.img->w / 2;
    bx_mbtn_draw(b, dst, &p, s->drag == DRAG_POS ? 1 : 0);
    bx_mbtn_draw(b, dst, &s->hairBtn, (s->flags & STRIP_HAIR_OFF ? 2 : 0) + bx_mbtn_state(b, &s->hairBtn));
    bx_mbtn_draw(b, dst, &s->cutinBtn, (s->flags & STRIP_CUTIN ? 2 : 0) + bx_mbtn_state(b, &s->cutinBtn));
    bx_mbtn_draw(b, dst, &s->steamBtn, (s->steamOff ? 0 : 2) + bx_mbtn_state(b, &s->steamBtn));
    bx_mbtn_draw(b, dst, &s->aheBtn, bx_mbtn_state(b, &s->aheBtn));
    bx_mbtn_draw(b, dst, &s->faceBtn, s->level == 0 ? 2 : bx_mbtn_state(b, &s->faceBtn));
    bx_mbtn_draw(b, dst, &s->bustBtn, s->level == 1 ? 2 : bx_mbtn_state(b, &s->bustBtn));
    bx_mbtn_draw(b, dst, &s->hipBtn, s->level == 2 ? 2 : bx_mbtn_state(b, &s->hipBtn));
    MBtn k = s->knob;
    if (k.img) {
        k.x = s->meter[0];
        k.y = s->meter[1] + (int)(s->sight * (float)(s->meter[3] - s->meter[1] - k.img->h / k.frames));
    }
    bx_mbtn_draw(b, dst, &k, 0);
}

bool bx_strip_update(BX *b, Strip *s) {
    if (s->noticeOn >= 0) {
        if (b->click || b->rclick) s->noticeOn = -1;
        b->click = b->rclick = false;
        return false;
    }
    if (b->rclick) {
        b->rclick = false;
        return true;
    }
    if (b->click) {
        if (bx_mbtn_on(b, &s->back)) {
            b->click = false;
            bx_snd_play(b, CH_SYS, RX_SYS, "cancel", false, 0);
            return true;
        }
        for (int i = 0; i < TAB_COUNT; i++)
            if (bx_mbtn_on(b, &s->tabs[i]) && i != TAB_MSG && (i != TAB_BGV || s->hasBgv)) {
                b->click = false;
                if (i != s->tab) {
                    bx_snd_play(b, CH_SYS, RX_SYS, "check", false, 0);
                    bx_strip_set_tab(b, s, i);
                }
                return false;
            }
    }
    switch (s->tab) {
    case TAB_BG: bg_update(b, s); break;
    case TAB_BGV:
        if (s->bgv) bx_bgv_update(b, s->bgv);
        break;
    case TAB_DATA: bx_strip_data_update(b, s); break;
    case TAB_BU: bu_update(b, s); break;
    default: break;
    }
    return false;
}

void bx_strip_draw(BX *b, Strip *s, u32 *dst) {
    size_t px = (size_t)b->W * (size_t)b->H;
    if (!s->scenePx) s->scenePx = bx_alloc(sizeof(u32) * px);
    if (s->dirty || s->sceneSerial != b->now / 100) {
        bx_strip_compose(b, s, s->scenePx);
        s->dirty = false;
        s->sceneSerial = b->now / 100;
    }
    memcpy(dst, s->scenePx, sizeof(u32) * px);
    switch (s->tab) {
    case TAB_BG: bg_draw(b, s, dst); break;
    case TAB_BGV:
        if (s->bgv) bx_bgv_draw(b, s->bgv, dst);
        break;
    case TAB_DATA: bx_strip_data_draw(b, s, dst); break;
    case TAB_BU: bu_draw(b, s, dst); break;
    default: break;
    }
    for (int i = 0; i < TAB_COUNT; i++)
        if (i != TAB_BGV || s->hasBgv) bx_mbtn_draw(b, dst, &s->tabs[i], i == s->tab ? 2 : i == TAB_MSG ? -1 : bx_mbtn_state(b, &s->tabs[i]));
    bx_mbtn_draw(b, dst, &s->back, bx_mbtn_state(b, &s->back));
    if (s->noticeOn >= 0 && s->notice[s->noticeOn]) {
        Image *n = s->notice[s->noticeOn];
        bx_blit(dst, b->W, b->H, n, n->x, n->y, 255);
    }
}

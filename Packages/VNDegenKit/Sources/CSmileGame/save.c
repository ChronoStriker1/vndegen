#include "sg.h"

#include <dirent.h>
#include <sys/stat.h>

enum { HERO_GUID = 0, HERO_STATS = 16, HERO_EXP = 52, HERO_STATE = 56, HERO_EQUIP = 60, HERO_SKILLS = 156, HERO_MIN = 160 };

typedef struct Wr {
    u8 *p;
    size_t n, cap;
} Wr;

static void w_bytes(Wr *w, const void *d, size_t n) {
    if (w->n + n > w->cap) {
        w->cap = (w->n + n) * 2 + 256;
        w->p = sg_realloc(w->p, w->cap);
    }
    if (n) memcpy(w->p + w->n, d, n);
    w->n += n;
}

static void w_u8(Wr *w, u8 v) { w_bytes(w, &v, 1); }

static void w_i32(Wr *w, i32 v) {
    u8 b[4] = {(u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24)};
    w_bytes(w, b, 4);
}

static void w_f32(Wr *w, float f) {
    u32 v;
    memcpy(&v, &f, 4);
    w_i32(w, (i32)v);
}

static void w_guid(Wr *w, Guid g) { w_bytes(w, g.b, 16); }

static void w_str(Wr *w, const char *s) {
    size_t n = s ? strlen(s) : 0, k = n;
    do {
        u8 b = (u8)(k & 0x7f);
        k >>= 7;
        w_u8(w, k ? (u8)(b | 0x80) : b);
    } while (k);
    w_bytes(w, s, n);
}

static void put_i32(u8 *p, i32 v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

static i32 get_i32(const u8 *p) { return (i32)((u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24); }

static u8 *rd_dup(Rd *r, size_t n) {
    if (!rd_need(r, n)) return NULL;
    u8 *d = sg_alloc(n ? n : 1);
    memcpy(d, r->p, n);
    r->p += n;
    return d;
}

static bool read_heroes(Rd *r, SaveHero **out, int *count) {
    i32 n = rd_i32(r);
    if (n < 0 || n > 64) return false;
    *out = sg_alloc(sizeof(SaveHero) * (size_t)(n ? n : 1));
    *count = n;
    for (int i = 0; i < n; i++) {
        SaveHero *h = &(*out)[i];
        h->size = rd_i32(r);
        if (h->size < HERO_MIN || h->size > 1 << 16) return false;
        h->body = rd_dup(r, (size_t)h->size);
        if (!h->body) return false;
        memcpy(h->id.b, h->body + HERO_GUID, 16);
        i32 *f = &h->level;
        for (int k = 0; k < 9; k++) f[k] = get_i32(h->body + HERO_STATS + 4 * k);
        for (int k = 0; k < 6; k++) memcpy(h->equip[k].b, h->body + HERO_EQUIP + 16 * k, 16);
        h->exp = get_i32(h->body + HERO_EXP);
        h->state = get_i32(h->body + HERO_STATE);
        i32 n = get_i32(h->body + HERO_SKILLS);
        if (n < 0 || HERO_MIN + 16 * n > h->size) return false;
        h->learnedCount = n < 16 ? n : 16;
        for (int k = 0; k < h->learnedCount; k++) memcpy(h->learned[k].b, h->body + HERO_MIN + 16 * k, 16);
    }
    return !r->bad;
}

static bool read_items(Rd *r, SaveItem **out, int *count) {
    i32 n = rd_i32(r);
    if (n < 0 || n > 100000) return false;
    *out = sg_alloc(sizeof(SaveItem) * (size_t)(n ? n : 1));
    *count = n;
    for (int i = 0; i < n; i++) {
        (*out)[i].id = rd_guid(r);
        (*out)[i].count = rd_i32(r);
    }
    return !r->bad;
}

static bool parse_party(SaveData *s, Rd *r) {
    if (!read_heroes(r, &s->party, &s->partyCount) || !read_heroes(r, &s->reserve, &s->reserveCount)) return false;
    if (!read_items(r, &s->items, &s->itemCount)) return false;
    s->money = rd_i32(r);
    if (!read_items(r, &s->seen, &s->seenCount)) return false;
    i32 n = rd_i32(r);
    if (n < 0 || n > 1000) return false;
    s->looks = sg_alloc(sizeof(SaveLook) * (size_t)(n ? n : 1));
    s->lookCount = n;
    for (int i = 0; i < n; i++) {
        s->looks[i].hero = rd_guid(r);
        s->looks[i].face = rd_guid(r);
        s->looks[i].graphic = rd_guid(r);
    }
    return !r->bad && r->p == r->end;
}

static bool parse_flags(SaveData *s, Rd *r) {
    if (rd_i32(r) != SAVE_SWITCHES) return false;
    for (int i = 0; i < SAVE_SWITCHES; i++) s->switches[i] = rd_u8(r);
    if (rd_i32(r) != SAVE_VARS) return false;
    for (int i = 0; i < SAVE_VARS; i++) s->vars[i] = rd_i32(r);
    if (!rd_need(r, sizeof s->config)) return false;
    memcpy(s->config, r->p, sizeof s->config);
    r->p += sizeof s->config;
    i32 n = rd_i32(r);
    if (n < 0 || n > 100000) return false;
    s->locals = sg_alloc(sizeof(SaveLocal) * (size_t)(n ? n : 1));
    s->localCount = n;
    for (int i = 0; i < n; i++) {
        s->locals[i].event = rd_guid(r);
        s->locals[i].index = rd_i32(r);
        s->locals[i].on = rd_u8(r);
    }
    s->playSeconds = rd_f32(r);
    if (r->bad) return false;
    s->flagsTailSize = (int)(r->end - r->p);
    s->flagsTail = rd_dup(r, (size_t)s->flagsTailSize);
    return s->flagsTail != NULL;
}

static bool parse_map(SaveData *s, Rd *r) {
    s->map = rd_guid(r);
    s->x = rd_i32(r);
    s->z = rd_i32(r);
    i32 n = rd_i32(r);
    if (n < 0 || n > 100000) return false;
    s->events = sg_alloc(sizeof(SaveEvent) * (size_t)(n ? n : 1));
    s->eventCount = n;
    for (int i = 0; i < n; i++) {
        i32 len = rd_i32(r);
        if (len < 0 || !rd_need(r, (size_t)len)) return false;
        Rd e = rd_make(r->p, (size_t)len);
        r->p += len;
        SaveEvent *ev = &s->events[i];
        ev->id = rd_guid(&e);
        ev->graphic = rd_guid(&e);
        ev->motion = rd_str(&e);
        ev->dir = rd_i32(&e);
        ev->x = rd_f32(&e);
        ev->y = rd_f32(&e);
        ev->z = rd_f32(&e);
        ev->flag = rd_u8(&e);
        if (e.bad || e.p != e.end) return false;
    }
    s->playerY = rd_f32(r);
    s->cameraMode = rd_i32(r);
    float *cam = &s->cameraRoll;
    for (int i = 0; i < 6; i++) cam[i] = rd_f32(r);
    for (int i = 0; i < 3; i++) s->cameraTarget[i] = rd_f32(r);
    s->cameraFlag = rd_u8(r);
    n = rd_i32(r);
    if (n < 0 || n > 1000) return false;
    s->pictures = sg_alloc(sizeof(SavePicture) * (size_t)(n ? n : 1));
    s->pictureCount = n;
    for (int i = 0; i < n; i++) {
        s->pictures[i].size = rd_i32(r);
        if (s->pictures[i].size < 0) return false;
        s->pictures[i].body = rd_dup(r, (size_t)s->pictures[i].size);
        if (!s->pictures[i].body) return false;
    }
    s->dir = rd_i32(r);
    if (r->bad) return false;
    s->mapTailSize = (int)(r->end - r->p);
    s->mapTail = rd_dup(r, (size_t)s->mapTailSize);
    return s->mapTail != NULL;
}

bool save_parse(SaveData *s, const u8 *data, size_t size) {
    memset(s, 0, sizeof *s);
    if (size < 12 || memcmp(data, "YUKRDATA", 8) != 0) return false;
    Rd r = rd_make(data + 8, size - 8);
    s->head = rd_i32(&r);
    bool (*parts[3])(SaveData *, Rd *) = {parse_party, parse_flags, parse_map};
    for (int i = 0; i < 3; i++) {
        i32 n = rd_i32(&r);
        if (n < 0 || !rd_need(&r, (size_t)n)) {
            save_free(s);
            return false;
        }
        Rd sec = rd_make(r.p, (size_t)n);
        r.p += n;
        if (!parts[i](s, &sec)) {
            save_free(s);
            return false;
        }
    }
    if (r.p != r.end) {
        save_free(s);
        return false;
    }
    return true;
}

static void write_heroes(Wr *w, const SaveHero *h, int n) {
    w_i32(w, n);
    for (int i = 0; i < n; i++) {
        i32 old = get_i32(h[i].body + HERO_SKILLS);
        if (old < 0 || HERO_MIN + 16 * old > h[i].size) old = 0;
        int tail = h[i].size - HERO_MIN - 16 * old;
        int size = HERO_MIN + 16 * h[i].learnedCount + tail;
        u8 *body = sg_alloc((size_t)size);
        memcpy(body, h[i].body, HERO_SKILLS);
        put_i32(body + HERO_SKILLS, h[i].learnedCount);
        for (int k = 0; k < h[i].learnedCount; k++) memcpy(body + HERO_MIN + 16 * k, h[i].learned[k].b, 16);
        memcpy(body + HERO_MIN + 16 * h[i].learnedCount, h[i].body + HERO_MIN + 16 * old, (size_t)tail);
        memcpy(body + HERO_GUID, h[i].id.b, 16);
        const i32 *f = &h[i].level;
        for (int k = 0; k < 9; k++) put_i32(body + HERO_STATS + 4 * k, f[k]);
        for (int k = 0; k < 6; k++) memcpy(body + HERO_EQUIP + 16 * k, h[i].equip[k].b, 16);
        put_i32(body + HERO_EXP, h[i].exp);
        put_i32(body + HERO_STATE, h[i].state);
        w_i32(w, size);
        w_bytes(w, body, (size_t)size);
        free(body);
    }
}

static void write_items(Wr *w, const SaveItem *it, int n) {
    w_i32(w, n);
    for (int i = 0; i < n; i++) {
        w_guid(w, it[i].id);
        w_i32(w, it[i].count);
    }
}

static void write_section(Wr *w, Wr *sec) {
    w_i32(w, (i32)sec->n);
    w_bytes(w, sec->p, sec->n);
    free(sec->p);
    *sec = (Wr){0};
}

u8 *save_serialize(const SaveData *s, size_t *size) {
    Wr w = {0}, sec = {0};
    w_bytes(&w, "YUKRDATA", 8);
    w_i32(&w, s->head);
    write_heroes(&sec, s->party, s->partyCount);
    write_heroes(&sec, s->reserve, s->reserveCount);
    write_items(&sec, s->items, s->itemCount);
    w_i32(&sec, s->money);
    write_items(&sec, s->seen, s->seenCount);
    w_i32(&sec, s->lookCount);
    for (int i = 0; i < s->lookCount; i++) {
        w_guid(&sec, s->looks[i].hero);
        w_guid(&sec, s->looks[i].face);
        w_guid(&sec, s->looks[i].graphic);
    }
    write_section(&w, &sec);
    w_i32(&sec, SAVE_SWITCHES);
    w_bytes(&sec, s->switches, SAVE_SWITCHES);
    w_i32(&sec, SAVE_VARS);
    for (int i = 0; i < SAVE_VARS; i++) w_i32(&sec, s->vars[i]);
    w_bytes(&sec, s->config, sizeof s->config);
    w_i32(&sec, s->localCount);
    for (int i = 0; i < s->localCount; i++) {
        w_guid(&sec, s->locals[i].event);
        w_i32(&sec, s->locals[i].index);
        w_u8(&sec, s->locals[i].on);
    }
    w_f32(&sec, s->playSeconds);
    w_bytes(&sec, s->flagsTail, (size_t)s->flagsTailSize);
    write_section(&w, &sec);
    w_guid(&sec, s->map);
    w_i32(&sec, s->x);
    w_i32(&sec, s->z);
    w_i32(&sec, s->eventCount);
    for (int i = 0; i < s->eventCount; i++) {
        const SaveEvent *ev = &s->events[i];
        Wr e = {0};
        w_guid(&e, ev->id);
        w_guid(&e, ev->graphic);
        w_str(&e, ev->motion);
        w_i32(&e, ev->dir);
        w_f32(&e, ev->x);
        w_f32(&e, ev->y);
        w_f32(&e, ev->z);
        w_u8(&e, ev->flag);
        write_section(&sec, &e);
    }
    w_f32(&sec, s->playerY);
    w_i32(&sec, s->cameraMode);
    const float *cam = &s->cameraRoll;
    for (int i = 0; i < 6; i++) w_f32(&sec, cam[i]);
    for (int i = 0; i < 3; i++) w_f32(&sec, s->cameraTarget[i]);
    w_u8(&sec, s->cameraFlag);
    w_i32(&sec, s->pictureCount);
    for (int i = 0; i < s->pictureCount; i++) {
        w_i32(&sec, s->pictures[i].size);
        w_bytes(&sec, s->pictures[i].body, (size_t)s->pictures[i].size);
    }
    w_i32(&sec, s->dir);
    w_bytes(&sec, s->mapTail, (size_t)s->mapTailSize);
    write_section(&w, &sec);
    *size = w.n;
    return w.p;
}

void save_free(SaveData *s) {
    for (int i = 0; i < s->partyCount; i++) free(s->party[i].body);
    for (int i = 0; i < s->reserveCount; i++) free(s->reserve[i].body);
    for (int i = 0; i < s->eventCount; i++) free(s->events[i].motion);
    for (int i = 0; i < s->pictureCount; i++) free(s->pictures[i].body);
    free(s->party);
    free(s->reserve);
    free(s->items);
    free(s->seen);
    free(s->looks);
    free(s->locals);
    free(s->flagsTail);
    free(s->events);
    free(s->pictures);
    free(s->mapTail);
    memset(s, 0, sizeof *s);
}

static char *save_dir(SG *g) { return g->saveDir ? sg_strdup(g->saveDir) : sg_fmt("%s/savedata", g->gameDir); }

static char *find_ci(const char *dir, const char *name) {
    char *exact = sg_fmt("%s/%s", dir, name);
    struct stat st;
    if (stat(exact, &st) == 0) return exact;
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)))
            if (strcasecmp(de->d_name, name) == 0) {
                free(exact);
                exact = sg_fmt("%s/%s", dir, de->d_name);
                break;
            }
        closedir(d);
    }
    return exact;
}

char *sg_save_path(SG *g, int slot) {
    char *dir = save_dir(g);
    char name[32];
    snprintf(name, sizeof name, "Save_%02d.sgs", slot);
    char *p = find_ci(dir, name);
    free(dir);
    return p;
}

char *sg_settings_path(SG *g) {
    char *dir = save_dir(g);
    char *p = find_ci(dir, "settings.dat");
    free(dir);
    return p;
}

int sg_save_slots(SG *g, SaveSlot *out, int max) {
    int n = max < SAVE_SLOTS ? max : SAVE_SLOTS;
    for (int i = 0; i < n; i++) {
        memset(&out[i], 0, sizeof out[i]);
        char *path = sg_save_path(g, i);
        struct stat st;
        size_t size = 0;
        u8 *data = stat(path, &st) == 0 ? sg_read_file(path, &size) : NULL;
        SaveData s;
        if (data && save_parse(&s, data, size)) {
            out[i].used = true;
            out[i].written = st.st_mtime;
            out[i].map = s.map;
            for (int k = 0; k < s.partyCount && k < 4; k++) {
                const SaveHero *h = &s.party[k];
                out[i].party[out[i].partyCount++] = (SlotHero){h->id, h->level, h->hp, h->maxHp, h->mp, h->maxMp};
            }
            save_free(&s);
        }
        free(data);
        free(path);
    }
    return n;
}

static const int kToSave[4] = {0, 3, 1, 2};
static const int kFromSave[4] = {0, 2, 3, 1};

SaveData *sg_save_state(SG *g) { return &g->e.save; }

float sg_play_seconds(SG *g) { return (float)g->b.party.playMs / 1000.0f; }

bool sg_player_at_least(SG *g, int major, int minor) {
    char *path = find_ci(g->gameDir, "sgb_rpgplayer.exe");
    size_t n = 0;
    u8 *d = sg_read_file(path, &n);
    free(path);
    bool ok = false;
    static const char key[] = "FileVersion";
    for (size_t i = 0; d && i + 2 * sizeof key + 8 < n; i++) {
        bool hit = true;
        for (size_t k = 0; k < sizeof key && hit; k++) hit = d[i + 2 * k] == (u8)key[k] && d[i + 2 * k + 1] == 0;
        if (!hit) continue;
        size_t p = i + 2 * sizeof key;
        while (p + 1 < n && d[p] == 0) p++;
        char v[32] = {0};
        for (int k = 0; k < 31 && p + 1 < n && d[p] && !d[p + 1]; k++, p += 2) v[k] = (char)d[p];
        int a = 0, b = 0;
        if (sscanf(v, "%d.%d", &a, &b) == 2) ok = a > major || (a == major && b >= minor);
        break;
    }
    free(d);
    return ok;
}

static bool default_camera(SG *g, float *pitch, float *dist, float *fov) {
    Sgr *f = &g->data.settingsFile;
    for (int i = 0; i < f->count; i++) {
        const SgrRec *r = &f->recs[i];
        if (r->type != SGR_SETTINGS) continue;
        for (u32 o = 12; o + 8 <= r->size; o++) {
            float v;
            memcpy(&v, r->body + o, 4);
            if (v != 70.0f || get_i32(r->body + o - 8) != 0 || get_i32(r->body + o - 4) != 0) continue;
            memcpy(pitch, r->body + o - 12, 4);
            memcpy(dist, r->body + o + 4, 4);
            *fov = v;
            return *pitch < 0 && *pitch > -90 && *dist > 0 && *dist < 100;
        }
    }
    return false;
}

void sg_save_new_game(SG *g) {
    SgE *e = &g->e;
    SaveData *s = &e->save;
    save_free(s);
    bool newer = sg_player_at_least(g, 1, 8);
    e->heroSize = newer ? 188 : 160;
    s->party = sg_alloc(sizeof(SaveHero));
    s->reserve = sg_alloc(sizeof(SaveHero));
    s->items = sg_alloc(sizeof(SaveItem));
    s->seen = sg_alloc(sizeof(SaveItem));
    s->looks = sg_alloc(sizeof(SaveLook));
    s->locals = sg_alloc(sizeof(SaveLocal));
    s->events = sg_alloc(sizeof(SaveEvent));
    s->pictures = sg_alloc(sizeof(SavePicture));
    static const u8 config[22] = {1, 1, 100, 0, 0, 0, 100, 0, 0, 0, 1};
    memcpy(s->config, config, sizeof config);
    static const u8 flagsNew[6] = {1, 1, 0xff, 0xff, 0xff, 0xff}, flagsOld[1] = {1};
    s->flagsTailSize = newer ? 6 : 1;
    s->flagsTail = sg_alloc(6);
    memcpy(s->flagsTail, newer ? flagsNew : flagsOld, (size_t)s->flagsTailSize);
    static const u8 tailNew[4] = {0, 0, 1, 1}, tailOld[4] = {0, 0, 0, 1};
    s->mapTailSize = 4;
    s->mapTail = sg_alloc(4);
    memcpy(s->mapTail, newer ? tailNew : tailOld, 4);
    s->cameraMode = 37;
    s->cameraPitch = -49;
    s->cameraDistance = 3;
    s->cameraFov = 70;
    s->cameraScale = 0.9f;
    if (!default_camera(g, &s->cameraPitch, &s->cameraDistance, &s->cameraFov)) sg_log(g, "save: no default camera in game settings");
    e->haveSave = true;
}

static char *sheet_motion(SG *g, Guid event, int sheet) {
    const SgrRec *rec = game_record(&g->data, SGR_EVENT, 0, event, NULL);
    if (!rec || sheet < 0) return sg_strdup("");
    Rd r = rd_make(rec->body, rec->size);
    rd_guid(&r);
    free(rd_str(&r));
    i32 n = rd_i32(&r);
    for (int i = 0; i < n && !r.bad; i++) {
        i32 conds = rd_i32(&r);
        if (conds < 0 || conds > 64) break;
        rd_skip(&r, 32 * (size_t)conds);
        free(rd_str(&r));
        rd_skip(&r, 16 + 22 + 16);
        char *motion = rd_str(&r);
        if (i == sheet && !r.bad) return motion;
        free(motion);
    }
    return sg_strdup("");
}

static const SaveHero *find_hero(const SaveData *s, Guid id) {
    for (int i = 0; i < s->partyCount; i++)
        if (guid_eq(s->party[i].id, id)) return &s->party[i];
    for (int i = 0; i < s->reserveCount; i++)
        if (guid_eq(s->reserve[i].id, id)) return &s->reserve[i];
    return NULL;
}

static SaveHero *heroes_from(const SaveData *s, const Member *m, int n, int heroSize) {
    SaveHero *out = sg_alloc(sizeof(SaveHero) * (size_t)(n ? n : 1));
    for (int i = 0; i < n; i++) {
        const SaveHero *old = find_hero(s, m[i].hero);
        SaveHero *h = &out[i];
        h->size = old ? old->size : heroSize;
        h->body = sg_alloc((size_t)h->size);
        if (old) memcpy(h->body, old->body, (size_t)h->size);
        h->id = m[i].hero;
        h->level = m[i].level;
        h->maxHp = m[i].maxHp;
        h->maxMp = m[i].maxMp;
        h->hp = m[i].hp;
        h->mp = m[i].mp;
        h->power = m[i].stat[STAT_POWER];
        h->vitality = m[i].stat[STAT_VITALITY];
        h->magic = old ? old->magic : 0;
        h->speed = m[i].stat[STAT_SPEED];
        memcpy(h->equip, m[i].equip, sizeof h->equip);
        h->exp = m[i].exp;
        h->state = m[i].state;
        h->learnedCount = m[i].learnedCount;
        memcpy(h->learned, m[i].learned, sizeof h->learned);
    }
    return out;
}

static void free_heroes(SaveHero *h, int n) {
    for (int i = 0; i < n; i++) free(h[i].body);
    free(h);
}

static void capture_party(SG *g, SaveData *s) {
    const Party *p = &g->b.party;
    int heroSize = g->e.heroSize ? g->e.heroSize : HERO_MIN;
    SaveHero *party = heroes_from(s, p->members, p->count, heroSize);
    SaveHero *reserve = heroes_from(s, p->reserve, p->reserveCount, heroSize);
    free_heroes(s->party, s->partyCount);
    free_heroes(s->reserve, s->reserveCount);
    s->party = party;
    s->partyCount = p->count;
    s->reserve = reserve;
    s->reserveCount = p->reserveCount;
    free(s->items);
    s->items = sg_alloc(sizeof(SaveItem) * (size_t)(p->itemCount + 1));
    s->itemCount = p->itemCount;
    for (int i = 0; i < p->itemCount; i++) s->items[i] = (SaveItem){p->items[i].item, p->items[i].count};
    s->money = p->money;
    SaveItem *seen = sg_alloc(sizeof(SaveItem) * (size_t)(p->seenCount + 1));
    for (int i = 0; i < p->seenCount; i++) {
        seen[i] = (SaveItem){p->seen[i], 1};
        for (int k = 0; k < s->seenCount; k++)
            if (guid_eq(s->seen[k].id, p->seen[i])) seen[i].count = s->seen[k].count;
    }
    free(s->seen);
    s->seen = seen;
    s->seenCount = p->seenCount;
    free(s->looks);
    s->looks = sg_alloc(sizeof(SaveLook) * (size_t)(p->lookCount + 1));
    s->lookCount = p->lookCount;
    for (int i = 0; i < p->lookCount; i++) s->looks[i] = (SaveLook){p->looks[i].hero, p->looks[i].face, p->looks[i].graphic};
}

static void member_from(SG *g, Member *m, const SaveHero *h) {
    memset(m, 0, sizeof *m);
    m->hero = h->id;
    m->level = h->level;
    m->exp = h->exp;
    m->state = (u8)h->state;
    m->hp = h->hp;
    m->mp = h->mp;
    m->maxHp = h->maxHp;
    m->maxMp = h->maxMp;
    m->stat[STAT_POWER] = h->power;
    m->stat[STAT_VITALITY] = h->vitality;
    m->stat[STAT_SPEED] = h->speed;
    const HeroDef *def = hero_def(&g->b.tables, h->id);
    m->stat[STAT_MAGIC] = def ? grow_stat(&def->stats[STAT_MAGIC], h->level) : h->magic;
    memcpy(m->equip, h->equip, sizeof m->equip);
    m->learnedCount = h->learnedCount;
    memcpy(m->learned, h->learned, sizeof m->learned);
}

static void restore_party(SG *g, const SaveData *s) {
    Party *p = &g->b.party;
    party_clear(p);
    for (int i = 0; i < s->partyCount && p->count < 8; i++) member_from(g, &p->members[p->count++], &s->party[i]);
    for (int i = 0; i < s->reserveCount && p->reserveCount < 16; i++) member_from(g, &p->reserve[p->reserveCount++], &s->reserve[i]);
    p->itemCap = s->itemCount + 16;
    p->items = sg_alloc(sizeof(Stack) * (size_t)p->itemCap);
    for (int i = 0; i < s->itemCount; i++)
        if (s->items[i].count > 0) p->items[p->itemCount++] = (Stack){s->items[i].id, s->items[i].count};
    p->seen = sg_alloc(sizeof(Guid) * (size_t)(s->seenCount + 1));
    for (int i = 0; i < s->seenCount; i++) p->seen[p->seenCount++] = s->seen[i].id;
    for (int i = 0; i < s->lookCount; i++) party_set_look(p, s->looks[i].hero, s->looks[i].face, s->looks[i].graphic);
    p->money = s->money;
    p->playMs = s->playSeconds > 0 ? (u64)((double)s->playSeconds * 1000.0) : 0;
}

static void capture(SG *g, SaveData *s) {
    capture_party(g, s);
    memcpy(s->switches, g->switches, SAVE_SWITCHES);
    memcpy(s->vars, g->vars, sizeof(i32) * SAVE_VARS);
    SaveLocal *locals = sg_alloc(sizeof(SaveLocal) * (size_t)(g->localCount * 4 + s->localCount + 1));
    int n = 0;
    for (int i = 0; i < g->localCount; i++)
        for (int k = 0; k < 4; k++)
            if (g->locals[i].on[k]) locals[n++] = (SaveLocal){g->locals[i].event, 65536 + k, 1};
    for (int i = 0; i < s->localCount; i++) {
        if (s->locals[i].on) continue;
        bool dup = false;
        for (int k = 0; k < n && !dup; k++) dup = guid_eq(locals[k].event, s->locals[i].event) && locals[k].index == s->locals[i].index;
        if (!dup) locals[n++] = s->locals[i];
    }
    free(s->locals);
    s->locals = locals;
    s->localCount = n;
    s->playSeconds = sg_play_seconds(g);
    Field *f = &g->field;
    if (!f->loaded) return;
    sgi_pictures_save(g, s);
    bool sameMap = guid_eq(s->map, f->map.id);
    s->map = f->map.id;
    s->x = (i32)f->player.x;
    s->z = (i32)f->player.z;
    s->dir = kToSave[f->player.dir & 3];
    s->playerY = f->player.y;
    ViewPose pose;
    sg_camera_get(g, &pose);
    s->cameraPitch = pose.pitch;
    s->cameraYaw = pose.yaw;
    s->cameraDistance = pose.distance;
    s->cameraFov = pose.fov;
    SaveEvent *ev = sg_alloc(sizeof(SaveEvent) * (size_t)(f->npcCount ? f->npcCount : 1));
    for (int i = 0; i < f->npcCount; i++) {
        const Actor *a = &f->npcs[i];
        const SaveEvent *old = NULL;
        for (int k = 0; k < s->eventCount && sameMap && !old; k++)
            if (guid_eq(s->events[k].id, a->eventId)) old = &s->events[k];
        ev[i].id = a->eventId;
        ev[i].flag = old ? old->flag : 0;
        const SgaState *st = &g->interp.a;
        bool ready = st->evCount == f->npcCount && guid_eq(st->mapId, f->map.id);
        if (ready && st->ev[i].sheet >= 0) ev[i].graphic = st->ev[i].graphicOverride ? st->ev[i].graphic : st->ev[i].sheets[st->ev[i].sheet].graphic;
        else if (ready) ev[i].graphic = (Guid){{0}};
        else ev[i].graphic = a->graphic ? a->graphic->id : old ? old->graphic : (Guid){{0}};
        if (ready && st->ev[i].motion) ev[i].motion = sg_strdup(st->ev[i].motion);
        else if (old && guid_eq(old->graphic, ev[i].graphic)) ev[i].motion = sg_strdup(old->motion);
        else ev[i].motion = sheet_motion(g, a->eventId, event_sheet(g, &f->events[i]));
        ev[i].dir = kToSave[a->dir & 3];
        ev[i].x = a->moving ? a->tx : a->x;
        ev[i].y = a->y;
        ev[i].z = a->moving ? a->tz : a->z;
    }
    for (int i = 0; i < s->eventCount; i++) free(s->events[i].motion);
    free(s->events);
    s->events = ev;
    s->eventCount = f->npcCount;
}

static bool write_file(const char *path, const u8 *data, size_t size) {
    char *tmp = sg_fmt("%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    bool ok = f && fwrite(data, 1, size, f) == size;
    if (f && fclose(f) != 0) ok = false;
    if (ok) ok = rename(tmp, path) == 0;
    if (!ok) remove(tmp);
    free(tmp);
    return ok;
}

bool sg_save_write(SG *g, int slot) {
    SgE *e = &g->e;
    if (!e || slot < 0 || slot >= SAVE_SLOTS) return false;
    if (!e->haveSave) sg_save_new_game(g);
    capture(g, &e->save);
    e->save.map = g->field.loaded ? g->field.map.id : e->save.map;
    size_t size = 0;
    u8 *data = save_serialize(&e->save, &size);
    char *dir = save_dir(g);
    mkdir(dir, 0755);
    free(dir);
    char *path = sg_save_path(g, slot);
    bool ok = write_file(path, data, size);
    sg_log(g, "save slot %d %s (%zu bytes)%s", slot, path, size, ok ? "" : " failed");
    free(path);
    free(data);
    return ok;
}

bool sg_save_read(SG *g, int slot) {
    SgE *e = &g->e;
    char *path = sg_save_path(g, slot);
    size_t size = 0;
    u8 *data = sg_read_file(path, &size);
    SaveData s;
    bool ok = data && save_parse(&s, data, size);
    free(data);
    if (!ok) {
        sg_log(g, "load slot %d %s failed", slot, path);
        free(path);
        return false;
    }
    sg_log(g, "load slot %d %s", slot, path);
    free(path);
    battle_abort(g);
    menu_close(g);
    interp_free(&g->interp);
    memset(&g->interp, 0, sizeof g->interp);
    memcpy(g->switches, s.switches, SAVE_SWITCHES);
    memset(g->switches + SAVE_SWITCHES, 0, sizeof g->switches - SAVE_SWITCHES);
    memcpy(g->vars, s.vars, sizeof(i32) * SAVE_VARS);
    memset(g->vars + SAVE_VARS, 0, sizeof g->vars - sizeof(i32) * SAVE_VARS);
    free(g->locals);
    g->locals = NULL;
    g->localCount = 0;
    for (int i = 0; i < s.localCount; i++) {
        int k = s.locals[i].index - 65536;
        if (k < 0 || k >= 4) continue;
        int at = -1;
        for (int j = 0; j < g->localCount && at < 0; j++)
            if (guid_eq(g->locals[j].event, s.locals[i].event)) at = j;
        if (at < 0) {
            g->locals = sg_realloc(g->locals, sizeof *g->locals * (size_t)(g->localCount + 1));
            at = g->localCount++;
            memset(&g->locals[at], 0, sizeof g->locals[at]);
            g->locals[at].event = s.locals[i].event;
        }
        g->locals[at].on[k] = s.locals[i].on;
    }
    save_free(&e->save);
    e->save = s;
    e->haveSave = true;
    if (!e->heroSize) e->heroSize = s.partyCount ? s.party[0].size : HERO_MIN;
    restore_party(g, &s);
    g->fadeLevel = 0;
    g->fadeTotal = 0;
    g->pendingTransfer = false;
    g->scene = SCENE_FIELD;
    if (!field_load(g, s.map, s.x, s.z, kFromSave[s.dir & 3])) return false;
    Field *f = &g->field;
    f->player.y = s.playerY;
    ViewPose pose;
    sg_camera_get(g, &pose);
    pose.pitch = s.cameraPitch;
    pose.yaw = s.cameraYaw;
    pose.distance = s.cameraDistance;
    pose.fov = s.cameraFov;
    pose.follow = true;
    sg_camera_to(g, &pose, 0);
    sga_map_loaded(g);
    SgaState *st = &g->interp.a;
    for (int i = 0; i < f->npcCount; i++)
        for (int k = 0; k < s.eventCount; k++) {
            const SaveEvent *se = &s.events[k];
            if (!guid_eq(se->id, f->npcs[i].eventId)) continue;
            Actor *a = &f->npcs[i];
            a->x = a->tx = se->x;
            a->z = a->tz = se->z;
            a->y = se->y;
            a->dir = kFromSave[se->dir & 3];
            if (i < st->evCount && st->ev[i].sheet >= 0) {
                SgaEvent *ev = &st->ev[i];
                if (!guid_zero(se->graphic) && !guid_eq(se->graphic, ev->sheets[ev->sheet].graphic)) {
                    ev->graphicOverride = true;
                    ev->graphic = se->graphic;
                    sga_actor_graphic(g, a, se->graphic);
                    a->visible = a->graphic != NULL;
                }
                if (se->motion && (!ev->motion || strcmp(se->motion, ev->motion) != 0)) {
                    free(ev->motion);
                    ev->motion = sg_strdup(se->motion);
                    sg_actor_motion(g, a, ev->motion);
                }
            }
            break;
        }
    sgi_pictures_load(g, &e->save);
    st->started = true;
    snd_set_master(g, get_i32(s.config + 2), get_i32(s.config + 6));
    return true;
}

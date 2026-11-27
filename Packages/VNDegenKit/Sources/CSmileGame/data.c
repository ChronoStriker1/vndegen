#include "sg.h"

#include <math.h>

static void read_strs(Rd *r, char **out, int n) {
    for (int i = 0; i < n; i++) out[i] = rd_str(r);
}

static void skip_names(Rd *r) {
    i32 n = rd_i32(r);
    if (n < 0 || n > 100000) {
        r->bad = true;
        return;
    }
    for (i32 i = 0; i < n && !r->bad; i++) {
        rd_i32(r);
        free(rd_str(r));
    }
}

bool gametext_parse(GameText *t, const SgrRec *rec) {
    memset(t, 0, sizeof *t);
    Rd r = rd_make(rec->body, rec->size);
    rd_guid(&r);
    free(rd_str(&r));
    i32 party = rd_i32(&r);
    if (party < 0 || party > 64) return false;
    rd_skip(&r, 16 * (size_t)party + 8 + 8 + 16);
    skip_names(&r);
    skip_names(&r);
    for (int i = 0; i < 6; i++) t->windows[i] = rd_guid(&r);
    rd_skip(&r, 2);
    read_strs(&r, t->shop, SHOP_COUNT);
    i32 n = rd_i32(&r);
    if (n < 0 || n > 1000) return false;
    t->elements = sg_alloc(sizeof(char *) * (size_t)(n ? n : 1));
    t->elementCount = n;
    read_strs(&r, t->elements, n);
    rd_guid(&r);
    read_strs(&r, t->menu, MT_COUNT);
    t->battleBgm = rd_guid(&r);
    read_strs(&r, t->battle, 23);
    for (int i = 0; i < SND_COUNT; i++) t->sounds[i] = rd_guid(&r);
    rd_skip(&r, 33);
    t->textColor = rd_str(&r);
    rd_skip(&r, 28 + 32 + 16);
    for (int i = 0; i < 3; i++) free(rd_str(&r));
    rd_skip(&r, 8);
    free(rd_str(&r));
    rd_skip(&r, 8 + 32);
    read_strs(&r, t->yesNo, 2);
    free(rd_str(&r));
    read_strs(&r, t->config, CFG_COUNT);
    n = rd_i32(&r);
    if (n < 0 || n > 1000) return false;
    rd_skip(&r, 16 * (size_t)n);
    t->money = rd_str(&r);
    t->playTime = rd_str(&r);
    rd_skip(&r, 1);
    read_strs(&r, t->battleStatus, 14);
    rd_skip(&r, 16 * 3 + 1);
    t->saveName = rd_str(&r);
    return !r.bad;
}

static void gametext_free(GameText *t) {
    for (int i = 0; i < SHOP_COUNT; i++) free(t->shop[i]);
    for (int i = 0; i < t->elementCount; i++) free(t->elements[i]);
    free(t->elements);
    for (int i = 0; i < MT_COUNT; i++) free(t->menu[i]);
    for (int i = 0; i < 23; i++) free(t->battle[i]);
    free(t->textColor);
    for (int i = 0; i < 2; i++) free(t->yesNo[i]);
    for (int i = 0; i < CFG_COUNT; i++) free(t->config[i]);
    free(t->money);
    free(t->playTime);
    for (int i = 0; i < 14; i++) free(t->battleStatus[i]);
    free(t->saveName);
    memset(t, 0, sizeof *t);
}

static GrowStat rd_grow(Rd *r) {
    GrowStat s;
    s.base = rd_i32(r);
    s.growth = rd_f32(r);
    s.curve = rd_f32(r);
    return s;
}

static bool count_ok(Rd *r, i32 n, size_t each) {
    if (n < 0 || (size_t)n > (size_t)(r->end - r->p) / each) {
        r->bad = true;
        return false;
    }
    return true;
}

bool hero_parse(HeroDef *h, const SgrRec *rec) {
    memset(h, 0, sizeof *h);
    Rd r = rd_make(rec->body, rec->size);
    h->id = rd_guid(&r);
    h->name = rd_str(&r);
    h->graphic = rd_guid(&r);
    h->level = rd_i32(&r);
    for (int i = 0; i < STAT_COUNT; i++) h->stats[i] = rd_grow(&r);
    i32 n = rd_i32(&r);
    if (!count_ok(&r, n, 20)) return false;
    h->skills = sg_alloc(sizeof(HeroSkill) * (size_t)(n ? n : 1));
    h->skillCount = n;
    for (i32 i = 0; i < n; i++) {
        h->skills[i].skill = rd_guid(&r);
        h->skills[i].level = rd_i32(&r);
    }
    n = rd_i32(&r);
    if (!count_ok(&r, n, 17)) return false;
    h->equipRules = sg_alloc(sizeof(HeroEquipRule) * (size_t)(n ? n : 1));
    h->equipRuleCount = n;
    for (i32 i = 0; i < n; i++) {
        h->equipRules[i].item = rd_guid(&r);
        h->equipRules[i].flag = rd_u8(&r);
    }
    for (int i = 0; i < 5; i++) h->equip[i] = rd_guid(&r);
    rd_skip(&r, 2);
    n = rd_i32(&r);
    if (!count_ok(&r, n, 16)) return false;
    h->commands = sg_alloc(sizeof(Guid) * (size_t)(n ? n : 1));
    h->commandCount = n;
    for (i32 i = 0; i < n; i++) h->commands[i] = rd_guid(&r);
    rd_skip(&r, 16);
    h->desc = rd_str(&r);
    h->face = rd_guid(&r);
    h->hp = rd_grow(&r);
    h->mp = rd_grow(&r);
    rd_skip(&r, 6);
    return !r.bad && r.p == r.end;
}

static void hero_free(HeroDef *h) {
    free(h->name);
    free(h->skills);
    free(h->equipRules);
    free(h->commands);
    free(h->desc);
}

static bool command_parse(CommandDef *c, const SgrRec *rec) {
    memset(c, 0, sizeof *c);
    Rd r = rd_make(rec->body, rec->size);
    c->id = rd_guid(&r);
    c->name = rd_str(&r);
    c->desc = rd_str(&r);
    c->icon = rd_guid(&r);
    c->iconX = rd_u8(&r);
    c->iconY = rd_u8(&r);
    c->kind = rd_i32(&r);
    c->rate = rd_i32(&r);
    c->flag = rd_i32(&r);
    c->priority = rd_i32(&r);
    return !r.bad;
}

static bool sub_block(Rd *r, Rd *out, const u8 **start, i32 *size) {
    i32 n = rd_i32(r);
    if (n < 0 || !rd_need(r, (size_t)n)) {
        r->bad = true;
        return false;
    }
    *out = rd_make(r->p, (size_t)n);
    if (start) *start = r->p;
    if (size) *size = n;
    r->p += n;
    return true;
}

bool item_parse(ItemDef *it, const SgrRec *rec) {
    memset(it, 0, sizeof *it);
    Rd r = rd_make(rec->body, rec->size);
    it->id = rd_guid(&r);
    it->name = rd_str(&r);
    it->desc = rd_str(&r);
    it->price = rd_i32(&r);
    it->icon = rd_guid(&r);
    it->iconX = rd_u8(&r);
    it->iconY = rd_u8(&r);
    it->kind = rd_i32(&r);
    Rd b;
    if (!sub_block(&r, &b, &it->block, &it->blockSize)) return false;
    if (it->kind == ITEM_EXPENDABLE) {
        it->effect = rd_guid(&b);
        it->hp = rd_i32(&b);
        it->mp = rd_i32(&b);
        it->hpPercent = rd_i32(&b);
        it->mpPercent = rd_i32(&b);
        rd_skip(&b, 20);
        for (int i = 0; i < 5; i++) it->cure[i] = rd_u8(&b);
        it->usable[0] = rd_u8(&b);
        it->usable[1] = rd_u8(&b);
        it->revive = rd_u8(&b);
    } else if (it->kind == ITEM_SKILLBOOK) {
        rd_skip(&b, 2);
        it->skill = rd_guid(&b);
    } else if (it->kind >= ITEM_WEAPON && it->kind <= ITEM_ACCESSORY) {
        for (int i = 0; i < EQ_COUNT; i++) it->eq[i] = rd_i32(&b);
        rd_skip(&b, 1);
        it->eqExtra = rd_i32(&b);
        if (it->kind == ITEM_WEAPON) {
            Rd w;
            if (!sub_block(&r, &w, NULL, NULL)) return false;
            it->effect = rd_guid(&w);
            it->attack = rd_i32(&w);
            rd_skip(&w, 1);
            it->element = rd_i32(&w);
            it->elementAttack = rd_i32(&w);
        }
    }
    it->model = rd_guid(&r);
    return !r.bad && r.p == r.end;
}

static void item_free(ItemDef *it) {
    free(it->name);
    free(it->desc);
}

bool skill_parse(SkillDef *s, const SgrRec *rec) {
    memset(s, 0, sizeof *s);
    Rd r = rd_make(rec->body, rec->size);
    s->id = rd_guid(&r);
    s->name = rd_str(&r);
    s->desc = rd_str(&r);
    s->icon = rd_guid(&r);
    s->iconX = rd_u8(&r);
    s->iconY = rd_u8(&r);
    Rd u, a;
    if (!sub_block(&r, &u, &s->use, &s->useSize)) return false;
    s->fieldUse = rd_u8(&u);
    s->battleUse = rd_u8(&u);
    rd_i32(&u);
    s->mpCost = rd_i32(&u);
    s->target = rd_i32(&u);
    if (!sub_block(&r, &a, &s->act, &s->actSize)) return false;
    s->effect = rd_guid(&a);
    s->power = rd_i32(&a);
    return !r.bad && !u.bad && !a.bad;
}

static void skill_free(SkillDef *s) {
    free(s->name);
    free(s->desc);
}

bool monster_parse(MonsterDef *m, const SgrRec *rec) {
    memset(m, 0, sizeof *m);
    Rd r = rd_make(rec->body, rec->size);
    m->id = rd_guid(&r);
    m->name = rd_str(&r);
    m->graphic = rd_guid(&r);
    m->attackEffect = rd_guid(&r);
    m->desc = rd_str(&r);
    for (int i = 0; i < 10; i++) m->stats[i] = rd_i32(&r);
    for (int i = 0; i < 13; i++) m->resist[i] = rd_i32(&r);
    i32 n = rd_i32(&r);
    if (!count_ok(&r, n, 40)) return false;
    m->actions = sg_alloc(sizeof(MonsterAction) * (size_t)(n ? n : 1));
    m->actionCount = n;
    for (i32 i = 0; i < n; i++) {
        MonsterAction *a = &m->actions[i];
        a->cond = rd_i32(&r);
        a->a = rd_i32(&r);
        a->b = rd_i32(&r);
        a->rate = rd_i32(&r);
        a->kind = rd_i32(&r);
        a->skill = rd_guid(&r);
        a->priority = rd_i32(&r);
    }
    m->money = rd_i32(&r);
    m->exp = rd_i32(&r);
    for (int i = 0; i < 2; i++) {
        m->drops[i].item = rd_guid(&r);
        m->drops[i].rate = rd_i32(&r);
    }
    m->tail = rd_i32(&r);
    return !r.bad && r.p == r.end;
}

static void monster_free(MonsterDef *m) {
    free(m->name);
    free(m->desc);
    free(m->actions);
}

static bool load_file(Pack *p, const char *name, Sgr *out) {
    size_t size = 0;
    u8 *data = pack_read(p, name, &size);
    if (!data) {
        memset(out, 0, sizeof *out);
        return false;
    }
    if (!sgr_parse(out, data, size)) {
        sgr_free(out);
        return false;
    }
    return true;
}

static int count_type(const Sgr *s, u8 type) {
    int n = 0;
    for (int i = 0; i < s->count; i++)
        if (s->recs[i].type == type) n++;
    return n;
}

bool tables_load(SG *g, Tables *t) {
    memset(t, 0, sizeof *t);
    Game *d = &g->data;
    bool ok = d->settingsFile.count > 0 && gametext_parse(&t->text, &d->settingsFile.recs[0]);
    if (!ok) sg_log(g, "tables: game settings text unreadable");
    const Sgr *hf = &d->heroFile;
    t->heroes = sg_alloc(sizeof(HeroDef) * (size_t)(count_type(hf, SGR_HERO) + 1));
    t->commands = sg_alloc(sizeof(CommandDef) * (size_t)(count_type(hf, 6) + 1));
    for (int i = 0; i < hf->count; i++) {
        const SgrRec *rec = &hf->recs[i];
        if (rec->type == SGR_HERO) {
            if (hero_parse(&t->heroes[t->heroCount], rec)) t->heroCount++;
            else {
                sg_log(g, "tables: hero %d unreadable", i);
                hero_free(&t->heroes[t->heroCount]);
                ok = false;
            }
        } else if (rec->type == 6) {
            if (command_parse(&t->commands[t->commandCount], rec)) t->commandCount++;
        }
    }
    load_file(&d->pack, "Item.sgr", &t->itemFile);
    t->items = sg_alloc(sizeof(ItemDef) * (size_t)(t->itemFile.count + 1));
    for (int i = 0; i < t->itemFile.count; i++) {
        if (t->itemFile.recs[i].type != SGR_ITEM) continue;
        if (item_parse(&t->items[t->itemCount], &t->itemFile.recs[i])) t->itemCount++;
        else {
            sg_log(g, "tables: item %d unreadable", i);
            item_free(&t->items[t->itemCount]);
            ok = false;
        }
    }
    load_file(&d->pack, "Skill.sgr", &t->skillFile);
    t->skills = sg_alloc(sizeof(SkillDef) * (size_t)(t->skillFile.count + 1));
    for (int i = 0; i < t->skillFile.count; i++) {
        if (t->skillFile.recs[i].type != SGR_SKILL) continue;
        if (skill_parse(&t->skills[t->skillCount], &t->skillFile.recs[i])) t->skillCount++;
        else {
            sg_log(g, "tables: skill %d unreadable", i);
            skill_free(&t->skills[t->skillCount]);
            ok = false;
        }
    }
    load_file(&d->pack, "Monster.sgr", &t->monsterFile);
    t->monsters = sg_alloc(sizeof(MonsterDef) * (size_t)(t->monsterFile.count + 1));
    for (int i = 0; i < t->monsterFile.count; i++) {
        if (t->monsterFile.recs[i].type != SGR_MONSTER) continue;
        if (monster_parse(&t->monsters[t->monsterCount], &t->monsterFile.recs[i])) t->monsterCount++;
        else {
            sg_log(g, "tables: monster %d unreadable", i);
            monster_free(&t->monsters[t->monsterCount]);
            ok = false;
        }
    }
    load_file(&d->pack, "Effect.sgr", &t->effectFile);
    t->effects = sg_alloc(sizeof(EffectDef) * (size_t)(t->effectFile.count + 1));
    for (int i = 0; i < t->effectFile.count; i++) {
        const SgrRec *rec = &t->effectFile.recs[i];
        if (rec->type != 9) continue;
        Rd r = rd_make(rec->body, rec->size);
        EffectDef *e = &t->effects[t->effectCount];
        e->id = rd_guid(&r);
        e->name = rd_str(&r);
        e->body = rec->body;
        e->size = (i32)rec->size;
        if (r.bad) free(e->name);
        else t->effectCount++;
    }
    sg_log(g, "tables: %d heroes, %d commands, %d items, %d skills, %d monsters, %d effects", t->heroCount, t->commandCount,
           t->itemCount, t->skillCount, t->monsterCount, t->effectCount);
    return ok;
}

void tables_free(Tables *t) {
    for (int i = 0; i < t->heroCount; i++) hero_free(&t->heroes[i]);
    free(t->heroes);
    for (int i = 0; i < t->commandCount; i++) {
        free(t->commands[i].name);
        free(t->commands[i].desc);
    }
    free(t->commands);
    for (int i = 0; i < t->itemCount; i++) item_free(&t->items[i]);
    free(t->items);
    for (int i = 0; i < t->skillCount; i++) skill_free(&t->skills[i]);
    free(t->skills);
    for (int i = 0; i < t->monsterCount; i++) monster_free(&t->monsters[i]);
    free(t->monsters);
    for (int i = 0; i < t->effectCount; i++) free(t->effects[i].name);
    free(t->effects);
    sgr_free(&t->itemFile);
    sgr_free(&t->skillFile);
    sgr_free(&t->monsterFile);
    sgr_free(&t->effectFile);
    gametext_free(&t->text);
    memset(t, 0, sizeof *t);
}

const HeroDef *hero_def(const Tables *t, Guid id) {
    for (int i = 0; i < t->heroCount; i++)
        if (guid_eq(t->heroes[i].id, id)) return &t->heroes[i];
    return NULL;
}

const ItemDef *item_def(const Tables *t, Guid id) {
    if (guid_zero(id)) return NULL;
    for (int i = 0; i < t->itemCount; i++)
        if (guid_eq(t->items[i].id, id)) return &t->items[i];
    return NULL;
}

const SkillDef *skill_def(const Tables *t, Guid id) {
    for (int i = 0; i < t->skillCount; i++)
        if (guid_eq(t->skills[i].id, id)) return &t->skills[i];
    return NULL;
}

const MonsterDef *monster_def(const Tables *t, Guid id) {
    for (int i = 0; i < t->monsterCount; i++)
        if (guid_eq(t->monsters[i].id, id)) return &t->monsters[i];
    return NULL;
}

const CommandDef *command_def(const Tables *t, Guid id) {
    for (int i = 0; i < t->commandCount; i++)
        if (guid_eq(t->commands[i].id, id)) return &t->commands[i];
    return NULL;
}

int grow_stat(const GrowStat *s, int level) {
    if (level <= 1) return s->base;
    double c = s->curve, sum = fabs(c - 1.0) < 1e-6 ? (double)(level - 1) : (pow(c, (double)(level - 1)) - 1.0) / (c - 1.0);
    return s->base + (int)((double)s->growth * sum + 1e-6);
}

int exp_for_level(int level) { return battle_exp_for_level(level); }

int slot_kind(int slot) {
    static const int kinds[SLOT_COUNT] = {ITEM_WEAPON, ITEM_SHIELD, ITEM_HEAD, ITEM_BODY, ITEM_ACCESSORY, ITEM_ACCESSORY};
    return slot >= 0 && slot < SLOT_COUNT ? kinds[slot] : 0;
}

int member_equip_sum(const Tables *t, const Member *m, int eq) {
    int sum = 0;
    for (int i = 0; i < SLOT_COUNT; i++) {
        const ItemDef *it = item_def(t, m->equip[i]);
        if (it && it->kind >= ITEM_WEAPON) sum += it->eq[eq];
    }
    return sum;
}

int member_stat(const Tables *t, const Member *m, int stat) { return stat >= 0 && stat < STAT_COUNT ? m->stat[stat] : 0; }

int member_max_hp(const Tables *t, const Member *m) { return m->maxHp + member_equip_sum(t, m, EQ_HP); }

int member_max_mp(const Tables *t, const Member *m) { return m->maxMp + member_equip_sum(t, m, EQ_MP); }

void member_init(const Tables *t, Member *m, const HeroDef *h) {
    memset(m, 0, sizeof *m);
    m->hero = h->id;
    m->level = h->level > 0 ? h->level : 1;
    m->exp = exp_for_level(m->level);
    for (int i = 0; i < STAT_COUNT; i++) m->stat[i] = grow_stat(&h->stats[i], m->level);
    m->maxHp = grow_stat(&h->hp, m->level);
    m->maxMp = grow_stat(&h->mp, m->level);
    for (int i = 0; i < 5; i++) m->equip[i] = h->equip[i];
    m->hp = member_max_hp(t, m);
    m->mp = member_max_mp(t, m);
}

void member_set_level(const Tables *t, Member *m, int level) {
    const HeroDef *h = hero_def(t, m->hero);
    if (h) {
        for (int i = 0; i < STAT_COUNT; i++) m->stat[i] += grow_stat(&h->stats[i], level) - grow_stat(&h->stats[i], m->level);
        m->maxHp += grow_stat(&h->hp, level) - grow_stat(&h->hp, m->level);
        m->maxMp += grow_stat(&h->mp, level) - grow_stat(&h->mp, m->level);
    }
    m->level = level;
}

int member_attack(const Tables *t, const Member *m) {
    const ItemDef *w = item_def(t, m->equip[SLOT_WEAPON]);
    return member_stat(t, m, STAT_POWER) + (w ? w->attack : 0) + member_equip_sum(t, m, EQ_POWER);
}

int member_defense(const Tables *t, const Member *m) {
    return member_stat(t, m, STAT_VITALITY) + member_equip_sum(t, m, EQ_DEFENSE);
}

int member_speed(const Tables *t, const Member *m) {
    return member_stat(t, m, STAT_SPEED) + member_equip_sum(t, m, EQ_SPEED);
}

int member_element_attack(const Tables *t, const Member *m) {
    const ItemDef *w = item_def(t, m->equip[SLOT_WEAPON]);
    return w ? w->elementAttack : 0;
}

int member_hit(const Tables *t, const Member *m) {
    const ItemDef *w = item_def(t, m->equip[SLOT_WEAPON]);
    return w ? w->eq[EQ_HIT] : 95;
}

int member_evasion(const Tables *t, const Member *m) { return member_equip_sum(t, m, EQ_EVASION); }

bool member_can_equip(const Tables *t, const Member *m, const ItemDef *it) {
    const HeroDef *h = hero_def(t, m->hero);
    if (!h || !it) return false;
    for (int i = 0; i < h->equipRuleCount; i++)
        if (guid_eq(h->equipRules[i].item, it->id)) return false;
    return true;
}

SgB *sgb(SG *g) { return &g->b; }

void menu_free(SgB *b);

void sgb_open(SG *g) {
    SgB *b = &g->b;
    if (b->loaded) return;
    tables_load(g, &b->tables);
    b->loaded = true;
    party_new(g);
}

void sgb_close(SG *g) {
    SgB *b = &g->b;
    menu_free(b);
    tables_free(&b->tables);
    party_clear(&b->party);
    memset(b, 0, sizeof *b);
}

void party_add_hero(SG *g, Guid hero) {
    Party *p = &g->b.party;
    if (party_has_hero(p, hero) || p->count >= 8) return;
    for (int i = 0; i < p->reserveCount; i++) {
        if (!guid_eq(p->reserve[i].hero, hero)) continue;
        p->members[p->count++] = p->reserve[i];
        memmove(&p->reserve[i], &p->reserve[i + 1], sizeof(Member) * (size_t)(p->reserveCount - i - 1));
        p->reserveCount--;
        return;
    }
    const HeroDef *h = hero_def(&g->b.tables, hero);
    if (h) member_init(&g->b.tables, &p->members[p->count++], h);
}

void party_remove_hero(SG *g, Guid hero) {
    Party *p = &g->b.party;
    for (int i = 0; i < p->count; i++) {
        if (!guid_eq(p->members[i].hero, hero)) continue;
        if (p->reserveCount < 16) p->reserve[p->reserveCount++] = p->members[i];
        memmove(&p->members[i], &p->members[i + 1], sizeof(Member) * (size_t)(p->count - i - 1));
        p->count--;
        return;
    }
}

bool party_has_hero(const Party *p, Guid hero) {
    for (int i = 0; i < p->count; i++)
        if (guid_eq(p->members[i].hero, hero)) return true;
    return false;
}

void party_clear(Party *p) {
    free(p->items);
    free(p->seen);
    free(p->looks);
    memset(p, 0, sizeof *p);
}

void party_new(SG *g) {
    party_clear(&g->b.party);
    const Settings *s = &g->data.settings;
    for (int i = 0; i < s->partyCount; i++)
        if (!guid_zero(s->party[i])) party_add_hero(g, s->party[i]);
}

void party_set_look(Party *p, Guid hero, Guid face, Guid graphic) {
    int i = 0;
    while (i < p->lookCount && !guid_eq(p->looks[i].hero, hero)) i++;
    if (i == p->lookCount) p->looks = sg_realloc(p->looks, sizeof(Look) * (size_t)++p->lookCount);
    p->looks[i] = (Look){hero, face, graphic};
}

Guid party_leader_graphic(SG *g) {
    Party *p = &g->b.party;
    if (!p->count) return (Guid){{0}};
    Guid hero = p->members[0].hero;
    for (int i = 0; i < p->lookCount; i++)
        if (guid_eq(p->looks[i].hero, hero)) return p->looks[i].graphic;
    const HeroDef *h = hero_def(&g->b.tables, hero);
    return h ? h->graphic : (Guid){{0}};
}

void party_recover(SG *g) {
    Party *p = &g->b.party;
    for (int i = 0; i < p->count; i++) {
        Member *mb = &p->members[i];
        mb->hp = member_max_hp(&g->b.tables, mb);
        mb->mp = member_max_mp(&g->b.tables, mb);
        mb->state = 0;
    }
}

int party_item_count(const Party *p, Guid item) {
    for (int i = 0; i < p->itemCount; i++)
        if (guid_eq(p->items[i].item, item)) return p->items[i].count;
    return 0;
}

void party_give_item(Party *p, Guid item, int delta) {
    for (int i = 0; i < p->itemCount; i++) {
        if (!guid_eq(p->items[i].item, item)) continue;
        p->items[i].count += delta;
        if (p->items[i].count > 99) p->items[i].count = 99;
        if (p->items[i].count <= 0) {
            memmove(&p->items[i], &p->items[i + 1], sizeof(Stack) * (size_t)(p->itemCount - i - 1));
            p->itemCount--;
        }
        return;
    }
    if (delta <= 0) return;
    bool seen = false;
    for (int i = 0; i < p->seenCount && !seen; i++) seen = guid_eq(p->seen[i], item);
    if (!seen) {
        p->seen = sg_realloc(p->seen, sizeof(Guid) * (size_t)(p->seenCount + 1));
        p->seen[p->seenCount++] = item;
    }
    if (p->itemCount == p->itemCap) {
        p->itemCap = p->itemCap ? p->itemCap * 2 : 16;
        p->items = sg_realloc(p->items, sizeof(Stack) * (size_t)p->itemCap);
    }
    p->items[p->itemCount++] = (Stack){item, delta > 99 ? 99 : delta};
}

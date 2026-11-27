#include "sg.h"

#include <math.h>

enum { PH_INTRO, PH_COMMAND, PH_TARGET, PH_LIST, PH_ALLY, PH_ACT, PH_ESCAPE, PH_VICTORY, PH_RESULT, PH_DEFEAT, PH_GAMEOVER, PH_DONE };

enum { ACT_NONE, ACT_ATTACK, ACT_DEFEND, ACT_SKILL, ACT_ITEM, ACT_WATCH, ACT_CHARGE, ACT_FLEE };

enum { LIST_SKILLS, LIST_ITEMS };

enum { CMD_KIND_ATTACK = 0, CMD_KIND_SKILL = 1, CMD_KIND_DEFEND = 2, CMD_KIND_SKILL2 = 4, CMD_KIND_ITEM = 5, CMD_KIND_ESCAPE = 99 };

enum { MON_ATTACK = 0, MON_WATCH = 2, MON_SKILL = 3, MON_CHARGE = 4, MON_FLEE = 6 };

enum { MAX_FIGHTERS = 16, MAX_CMDS = 8, MAX_LIST = 64 };

enum { MO_WAIT, MO_ATTACK, MO_DAMAGE, MO_GUARD, MO_KO, MO_SKILL, MO_COUNT };

enum { ST_POISON = 1, ST_SLEEP = 2, ST_PARALYSIS = 4, ST_CONFUSION = 8, ST_CHARM = 16, ST_DEATH = 32 };

static const int kTickMs = 900;
static const int kRecoverPct = 34;

static const u32 kWhite = 0xffffffffu;
static const u32 kGray = 0xff808080u;
static const int kIntroMs = 1100;
static const int kEnterMs = 400;
static const int kDefeatMs = 500;
static const int kPushMs = 800;
static const float kFrontZoom = 1.886f;
static const float kActorPull = 0.35f;
static const float kActorZoom = 0.85f;
static const float kFrontPitch = 8.8f;
static const float kFrontLift = 2.4f;
static const int kActMs = 1250;
static const int kHitMs = 450;
static const float kHeroScale = 1.29f;
static const float kMonsterScale = 0.63f;
static const float kPartyX = 2.83f;
static const float kPartyZ = 2.57f;
static const float kPartySpacing = 3.3f;

typedef struct Fighter {
    bool monster;
    int member;
    const MonsterDef *def;
    const char *name;
    i32 hp, maxHp, mp, maxMp;
    i32 atk, dfn, mag, spd, hit, eva;
    i32 resist[12];
    i32 element;
    u8 state;
    float x, z;
    bool defending, charged, fled;
    const Res *graphic;
    Image *image;
    Model *model;
    Image *motion[MO_COUNT];
    int motionFrames[MO_COUNT], motionMs[MO_COUNT];
    int pose;
    u32 poseMs;
    int popup;
    bool popupMiss, popupHeal;
    u32 popupMs, hitMs, fadeMs, flashMs;
} Fighter;

typedef struct Plan {
    int actor, kind, target;
    const SkillDef *skill;
    const ItemDef *item;
} Plan;

typedef struct Battle {
    SG *g;
    BattleSetup setup;
    Fighter f[MAX_FIGHTERS];
    int count, partyCount;
    Plan plan[MAX_FIGHTERS];
    Plan queue[MAX_FIGHTERS];
    int queueCount, queuePos;
    int member;
    int cmds[MAX_CMDS];
    const CommandDef *cmdDefs[MAX_CMDS];
    int cmdCount, cmdCursor;
    int listKind, listCursor, listCount;
    const void *list[MAX_LIST];
    int targetCursor;
    int phase;
    u32 phaseMs, totalMs;
    char bar[256];
    int result;
    Field saved;
    u32 rng;
    i32 expGain, moneyGain;
    Guid drops[16];
    int dropCount;
    char lines[12][160];
    int lineCount;
    int turn;
    bool applied, started, restored, staged, ticked;
    Guid fieldMap, fieldBgm;
    bool hadBgm, front;
    u32 enterMs;
    Image *gameover;
    struct {
        BattleFxDef def;
        u32 ms;
        int target;
        bool on;
    } fx[8];
    Cam stage;
    View *fieldView;
    ShadowMap fieldShadow;
    bool fieldShadowOn;
    float fieldLight[9], fieldAnimU;
} Battle;

static struct {
    SG *g;
    Battle *b;
    u32 seed;
    bool seeded;
    BattleEnc enc;
} registry[8];

static int slot_of(SG *g, bool create) {
    for (int i = 0; i < 8; i++)
        if (registry[i].g == g) return i;
    if (!create) return -1;
    for (int i = 0; i < 8; i++)
        if (!registry[i].g) {
            registry[i].g = g;
            return i;
        }
    return -1;
}

static Battle *get(SG *g) {
    int s = slot_of(g, false);
    return s >= 0 ? registry[s].b : NULL;
}

BattleEnc *battle_enc(SG *g) {
    int s = slot_of(g, true);
    return s >= 0 ? &registry[s].enc : NULL;
}

static u32 next_rand(u32 *s) {
    u32 x = *s ? *s : 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static int rand_pct(u32 *s) { return (int)(next_rand(s) % 100u); }

float battle_spread(u32 *rng) { return 0.94f + (float)(next_rand(rng) % 1001u) / 1000.0f * 0.10f; }

int battle_attack_damage(int attack, int defense, float spread) {
    float base = ((float)attack * 1.5f - (float)defense) / 4.0f;
    if (base <= 0) return 0;
    return (int)floorf(base * spread + 0.5f);
}

int battle_skill_damage(int power, int magic, int defense, float spread) {
    float base = (float)(power + magic) * spread - (float)defense / 4.0f;
    if (base <= 0) return 0;
    return (int)floorf(base + 0.5f);
}

int battle_exp_for_level(int level) {
    static const int seen[] = {0, 0, 26, 84, 184, 350, 600, 966, 1484, 2196, 3150};
    if (level <= 10) return seen[level < 0 ? 0 : level];
    return (int)(3.7984 * pow((double)level, 2.5) * pow(1.101, (double)level) + 3.05 + 0.5);
}

bool battle_escape_roll(u32 *rng, int partySpeed, int monsterSpeed) {
    int chance = monsterSpeed > 0 ? 50 * partySpeed / monsterSpeed : 100;
    if (chance > 100) chance = 100;
    if (chance < 5) chance = 5;
    return rand_pct(rng) < chance;
}

static Tables *TB(SG *g) { return &sgb(g)->tables; }
static const char *MT(SG *g, int i) { return TB(g)->text.menu[i] ? TB(g)->text.menu[i] : ""; }
static Party *PT(SG *g) { return &sgb(g)->party; }

static const char *btext(SG *g, int i, const char *fallback) {
    const char *s = TB(g)->text.battle[i];
    return s && *s ? s : fallback;
}

static const char *stext(SG *g, int i, const char *fallback) {
    const char *s = i < 14 ? TB(g)->text.battleStatus[i] : NULL;
    return s && *s ? s : fallback;
}

static void fmt(char *out, size_t size, const char *pattern, const char *a0, const char *a1) {
    size_t n = 0;
    for (const char *p = pattern; *p && n + 1 < size;) {
        const char *rep = NULL;
        if (p[0] == '{' && p[1] == '0' && p[2] == '}') rep = a0;
        else if (p[0] == '{' && p[1] == '1' && p[2] == '}') rep = a1;
        if (rep) {
            for (const char *r = rep; *r && n + 1 < size; r++) out[n++] = *r;
            p += 3;
        } else {
            out[n++] = *p++;
        }
    }
    out[n] = 0;
}

bool battle_setup_cmd(BattleSetup *s, const Cmd *c) {
    memset(s, 0, sizeof *s);
    if (!c || c->argCount < 1) return false;
    int n = cmd_int(c, 0), k = 1;
    for (int i = 0; i < n && k < c->argCount; i++, k++)
        if (s->count < BATTLE_MAX_MONSTERS) s->monsters[s->count++] = cmd_guid(c, k);
    s->continueOnLose = cmd_int(c, k++) != 0;
    s->noEscape = cmd_int(c, k++) != 0;
    s->flag = cmd_int(c, k++);
    s->bgm = cmd_guid(c, k++);
    s->background = cmd_guid(c, k++);
    while (k < c->argCount) {
        int tag = cmd_int(c, k++);
        if (tag == 1000 && k + 1 < c->argCount) {
            s->hasCenter = true;
            s->centerX = cmd_int(c, k++);
            s->centerZ = cmd_int(c, k++);
        } else if (tag == 1001) {
            s->hasLayout = true;
            for (int i = 0; i < s->count && k + 1 < c->argCount; i++) {
                s->layout[i][0] = cmd_int(c, k++);
                s->layout[i][1] = cmd_int(c, k++);
            }
        } else {
            break;
        }
    }
    return s->count > 0;
}

static const HeroDef *hero_of(SG *g, const Fighter *f) {
    if (f->monster) return NULL;
    return hero_def(TB(g), PT(g)->members[f->member].hero);
}

static Member *member_of(SG *g, const Fighter *f) { return f->monster ? NULL : &PT(g)->members[f->member]; }

static bool alive(const Fighter *f) { return f->hp > 0 && !f->fled; }

static void sync_member(SG *g, Fighter *f) {
    Member *m = member_of(g, f);
    if (!m) return;
    m->hp = f->hp;
    m->mp = f->mp;
    m->state = f->hp > 0 ? (u8)(f->state & ST_POISON) : 0;
}

static const SgrRec *res_record(SG *g, Guid id) {
    Game *d = &g->data;
    for (int k = -1; k < d->packCount; k++) {
        Sgr *s = k < 0 ? &d->resFile : &d->packRes[k];
        for (int i = 0; i < s->count; i++)
            if (s->recs[i].type == SGR_RESOURCE && s->recs[i].sub == RES_CHARACTER && s->recs[i].size >= 16 &&
                memcmp(s->recs[i].body, id.b, 16) == 0)
                return &s->recs[i];
    }
    return NULL;
}

static void load_motions(SG *g, Fighter *f) {
    static const char *names[MO_COUNT] = {"wait", "attack", "damage", "guard", "KO", "skill"};
    const SgrRec *rec = res_record(g, f->graphic->id);
    if (!rec) return;
    Rd r = rd_make(rec->body, rec->size);
    rd_guid(&r);
    free(rd_str(&r));
    free(rd_str(&r));
    for (int i = 0; i < 4; i++) rd_i32(&r);
    int n = rd_i32(&r);
    const char *base = f->graphic->path;
    const char *slash = base;
    for (const char *p = base; *p; p++)
        if (*p == '\\' || *p == '/') slash = p + 1;
    for (int m = 0; m < n && !r.bad; m++) {
        i32 size = rd_i32(&r);
        if (size <= 0 || !rd_need(&r, (size_t)size)) break;
        Rd e = rd_make(r.p, (size_t)size);
        rd_skip(&r, (size_t)size);
        rd_guid(&e);
        char *name = rd_str(&e), *path = rd_str(&e);
        i32 frames = rd_i32(&e), ms = rd_i32(&e);
        for (int k = 0; k < MO_COUNT && name && path && !e.bad; k++) {
            if (strcmp(name, names[k]) != 0 || f->motion[k]) continue;
            const char *file = path;
            for (const char *p = path; *p; p++)
                if (*p == '\\' || *p == '/') file = p + 1;
            char *full = sg_fmt("%.*s%s", (int)(slash - base), base, file);
            size_t sz = 0;
            u8 *data = pack_read(&g->data.pack, full, &sz);
            if (data) f->motion[k] = img_decode(data, sz);
            free(data);
            free(full);
            if (f->motion[k]) {
                f->motionFrames[k] = frames > 0 ? frames : 1;
                f->motionMs[k] = ms > 0 ? ms : 100;
            }
        }
        free(name);
        free(path);
    }
}

static void load_graphic(SG *g, Fighter *f, Guid id) {
    f->graphic = res_find(&g->res, id);
    if (!f->graphic) return;
    if (f->graphic->kind == RES_CHARACTER) load_motions(g, f);
    size_t n = strlen(f->graphic->path);
    if (n > 4 && strcasecmp(f->graphic->path + n - 4, ".fbx") == 0) f->model = model_load(g, f->graphic->path);
    else f->image = sg_image(g, id);
}

static void add_monster(Battle *b, const MonsterDef *m, float x, float z) {
    if (b->count >= MAX_FIGHTERS) return;
    Fighter *f = &b->f[b->count++];
    memset(f, 0, sizeof *f);
    f->monster = true;
    f->def = m;
    f->name = m->name;
    f->hp = f->maxHp = m->stats[0];
    f->mp = f->maxMp = m->stats[1];
    f->atk = m->stats[2];
    f->dfn = m->stats[3];
    f->mag = m->stats[4];
    f->hit = m->stats[5];
    f->eva = m->stats[8];
    f->spd = m->stats[9];
    memcpy(f->resist, m->resist, sizeof f->resist);
    f->x = x;
    f->z = z;
    load_graphic(b->g, f, m->graphic);
}

static void add_member(Battle *b, int index, float x, float z) {
    SG *g = b->g;
    Member *m = &PT(g)->members[index];
    const HeroDef *h = hero_def(TB(g), m->hero);
    if (!h) return;
    Fighter *f = &b->f[b->count++];
    memset(f, 0, sizeof *f);
    f->member = index;
    f->name = h->name;
    f->maxHp = member_max_hp(TB(g), m);
    f->maxMp = member_max_mp(TB(g), m);
    f->hp = m->hp;
    f->mp = m->mp;
    f->atk = member_attack(TB(g), m);
    f->dfn = member_defense(TB(g), m);
    f->mag = member_stat(TB(g), m, STAT_MAGIC);
    f->spd = member_speed(TB(g), m);
    f->hit = member_hit(TB(g), m);
    f->eva = member_evasion(TB(g), m);
    const ItemDef *w = item_def(TB(g), m->equip[SLOT_WEAPON]);
    f->element = w ? w->element : 0;
    for (int i = 0; i < 6; i++) f->resist[i] = member_equip_sum(TB(g), m, EQ_RESIST + i);
    for (int i = 0; i < 6; i++) f->resist[6 + i] = member_equip_sum(TB(g), m, EQ_STATE_RESIST + i);
    f->state = m->state & ST_POISON;
    f->x = x;
    f->z = z;
    load_graphic(g, f, h->graphic);
}

static void build_commands(Battle *b) {
    SG *g = b->g;
    const HeroDef *h = hero_of(g, &b->f[b->member]);
    b->cmdCount = 0;
    if (h)
        for (int i = 0; i < h->commandCount && b->cmdCount < MAX_CMDS - 1; i++) {
            const CommandDef *c = command_def(TB(g), h->commands[i]);
            if (!c) continue;
            b->cmdDefs[b->cmdCount] = c;
            b->cmds[b->cmdCount++] = c->kind == CMD_KIND_SKILL2 ? CMD_KIND_SKILL : c->kind;
        }
    if (!b->cmdCount) {
        b->cmdDefs[0] = NULL;
        b->cmds[b->cmdCount++] = CMD_KIND_ATTACK;
    }
    b->cmdDefs[b->cmdCount] = NULL;
    b->cmds[b->cmdCount++] = CMD_KIND_ESCAPE;
    if (b->cmdCursor >= b->cmdCount) b->cmdCursor = 0;
}

static void set_phase(Battle *b, int phase) {
    b->phase = phase;
    b->phaseMs = 0;
}

static void apply_cam(Battle *b, Cam c) {
    SG *g = b->g;
    Field *f = &g->field;
    f->cam = c;
    if (f->loaded) sg_camera_to(g, &(ViewPose){c.target, -c.pitch, -c.yaw, c.distance / 20.0f, c.fov * 10.0f, 0, false}, 0);
}

static float ease(float t) { return t <= 0 ? 0 : t >= 1 ? 1 : t * t * (3 - 2 * t); }

static float stage_ground(SG *g, float x, float z);

static void front_cam(Battle *b) {
    Cam c = b->stage;
    float x = 0, z = 0;
    for (int i = 0; i < b->partyCount; i++) x += b->f[i].x, z += b->f[i].z;
    if (b->partyCount) x /= (float)b->partyCount, z /= (float)b->partyCount;
    c.target = (Vec3){x, stage_ground(b->g, x, z) + kFrontLift, z};
    c.yaw = 180.0f;
    c.pitch = kFrontPitch;
    c.distance = b->stage.distance / kFrontZoom;
    b->front = true;
    apply_cam(b, c);
}

static bool commandable(const Fighter *f) { return alive(f) && !(f->state & (ST_SLEEP | ST_PARALYSIS | ST_CONFUSION | ST_CHARM)); }

static int next_member(Battle *b, int from) {
    for (int i = from; i < b->partyCount; i++)
        if (commandable(&b->f[i])) return i;
    return -1;
}

static int prev_member(Battle *b, int from) {
    for (int i = from; i >= 0; i--)
        if (commandable(&b->f[i])) return i;
    return -1;
}

static void build_queue(Battle *b);

static void begin_commands(Battle *b) {
    b->turn++;
    for (int i = 0; i < b->count; i++) {
        b->plan[i] = (Plan){i, ACT_NONE, -1, NULL, NULL};
        b->f[i].defending = false;
    }
    b->member = next_member(b, 0);
    b->cmdCursor = 0;
    b->bar[0] = 0;
    if (b->member < 0) {
        build_queue(b);
        return;
    }
    build_commands(b);
    set_phase(b, PH_COMMAND);
}

static int living(Battle *b, bool monsters) {
    int n = 0;
    for (int i = 0; i < b->count; i++)
        if (b->f[i].monster == monsters && alive(&b->f[i])) n++;
    return n;
}

static int first_target(Battle *b, bool monsters) {
    for (int i = 0; i < b->count; i++)
        if (b->f[i].monster == monsters && alive(&b->f[i])) return i;
    return -1;
}

static int cycle_target(Battle *b, int cur, int dir, bool monsters) {
    for (int step = 1; step <= b->count; step++) {
        int i = ((cur + dir * step) % b->count + b->count) % b->count;
        if (b->f[i].monster == monsters && alive(&b->f[i])) return i;
    }
    return cur;
}

static const MonsterAction *pick_monster_action(Battle *b, const Fighter *f) {
    const MonsterDef *m = f->def;
    int total = 0;
    for (int i = 0; i < m->actionCount; i++) {
        const MonsterAction *a = &m->actions[i];
        if (a->a == 1 && b->turn % 2 == 0) continue;
        if (a->a == 2 && b->turn % 2 == 1) continue;
        if (a->kind == MON_SKILL) {
            const SkillDef *s = skill_def(TB(b->g), a->skill);
            if (!s || s->mpCost > f->mp) continue;
        }
        total += a->rate > 0 ? a->rate : 0;
    }
    if (total <= 0) return NULL;
    int r = (int)(next_rand(&b->rng) % (u32)total);
    for (int i = 0; i < m->actionCount; i++) {
        const MonsterAction *a = &m->actions[i];
        if (a->a == 1 && b->turn % 2 == 0) continue;
        if (a->a == 2 && b->turn % 2 == 1) continue;
        if (a->kind == MON_SKILL) {
            const SkillDef *s = skill_def(TB(b->g), a->skill);
            if (!s || s->mpCost > f->mp) continue;
        }
        int w = a->rate > 0 ? a->rate : 0;
        if (r < w) return a;
        r -= w;
    }
    return NULL;
}

static int random_any(Battle *b) {
    int n = 0;
    for (int i = 0; i < b->count; i++) n += alive(&b->f[i]);
    if (!n) return -1;
    int k = (int)(next_rand(&b->rng) % (u32)n);
    for (int i = 0; i < b->count; i++)
        if (alive(&b->f[i]) && k-- == 0) return i;
    return -1;
}

static int random_target(Battle *b, bool monsters) {
    int n = living(b, monsters);
    if (!n) return -1;
    int k = (int)(next_rand(&b->rng) % (u32)n);
    for (int i = 0; i < b->count; i++)
        if (b->f[i].monster == monsters && alive(&b->f[i]) && k-- == 0) return i;
    return -1;
}

static void plan_monsters(Battle *b) {
    for (int i = b->partyCount; i < b->count; i++) {
        Fighter *f = &b->f[i];
        if (!alive(f)) continue;
        const MonsterAction *a = pick_monster_action(b, f);
        Plan *p = &b->plan[i];
        *p = (Plan){i, ACT_ATTACK, random_target(b, false), NULL, NULL};
        if (!a) continue;
        switch (a->kind) {
        case MON_WATCH: p->kind = ACT_WATCH; break;
        case MON_CHARGE: p->kind = ACT_CHARGE; break;
        case MON_FLEE: p->kind = ACT_FLEE; break;
        case MON_SKILL:
            p->kind = ACT_SKILL;
            p->skill = skill_def(TB(b->g), a->skill);
            if (p->skill && (p->skill->target == TARGET_ALLY || p->skill->target == TARGET_SELF)) p->target = i;
            break;
        default: break;
        }
    }
}

static void build_queue(Battle *b) {
    plan_monsters(b);
    for (int i = 0; i < b->partyCount; i++)
        if (alive(&b->f[i]) && !commandable(&b->f[i])) b->plan[i] = (Plan){i, ACT_ATTACK, -1, NULL, NULL};
    b->queueCount = 0;
    for (int i = 0; i < b->count; i++)
        if (alive(&b->f[i]) && b->plan[i].kind != ACT_NONE) b->queue[b->queueCount++] = b->plan[i];
    for (int i = 1; i < b->queueCount; i++) {
        Plan p = b->queue[i];
        int j = i - 1;
        while (j >= 0 && b->f[b->queue[j].actor].spd < b->f[p.actor].spd) {
            b->queue[j + 1] = b->queue[j];
            j--;
        }
        b->queue[j + 1] = p;
    }
    for (int i = 0; i < b->queueCount; i++)
        if (b->queue[i].kind == ACT_DEFEND) b->f[b->queue[i].actor].defending = true;
    b->queuePos = 0;
    b->applied = b->started = false;
    b->bar[0] = 0;
    set_phase(b, PH_ACT);
}

static void set_pose(Fighter *f, int pose) {
    f->pose = pose;
    f->poseMs = 0;
}

static void popup(Fighter *t, int value, bool miss, bool heal) {
    t->popup = value;
    t->popupMiss = miss;
    t->popupHeal = heal;
    t->popupMs = 1;
    if (!miss && !heal && value > 0) {
        t->hitMs = 1;
        if (!t->monster) set_pose(t, MO_DAMAGE);
    }
}

static int element_scale(const Fighter *t, int element, int dmg) {
    if (element <= 0 || element > 6) return dmg;
    int r = t->resist[element - 1];
    int out = dmg * (100 - r) / 100;
    return out < 0 ? 0 : out;
}

static void start_fx(Battle *b, Guid effect, const Fighter *t) {
    if (guid_zero(effect)) {
        const Tables *tb = TB(b->g);
        if (!tb->effectCount) return;
        effect = tb->effects[0].id;
    }
    for (int i = 0; i < 8; i++) {
        if (b->fx[i].on) continue;
        if (!battle_fx_def(b->g, effect, &b->fx[i].def)) {
            sg_log(b->g, "battle: effect not read");
            return;
        }
        sg_log(b->g, "battle: effect %dx%d %d frames", b->fx[i].def.cols, b->fx[i].def.rows, b->fx[i].def.frames);
        b->fx[i].on = true;
        b->fx[i].ms = 0;
        b->fx[i].target = (int)(t - b->f);
        if (!guid_zero(b->fx[i].def.sound)) snd_se(b->g, b->fx[i].def.sound, 100, 100);
        return;
    }
}

static Guid attack_effect(Battle *b, const Fighter *a) {
    if (a->monster) return a->def->attackEffect;
    const Member *m = member_of(b->g, a);
    const ItemDef *w = m ? item_def(TB(b->g), m->equip[SLOT_WEAPON]) : NULL;
    return w ? w->effect : (Guid){{0}};
}

static void hurt(Battle *b, Fighter *t, int dmg) {
    if (dmg < 0) dmg = 0;
    sg_log(b->g, "battle: %s takes %d", t->name, dmg);
    t->hp -= dmg;
    if (t->hp < 0) t->hp = 0;
    popup(t, dmg, false, false);
    if (!t->monster) sync_member(b->g, t);
}

static void do_attack(Battle *b, Fighter *a, Fighter *t) {
    start_fx(b, attack_effect(b, a), t);
    int chance = a->hit - t->eva;
    if (rand_pct(&b->rng) >= chance) {
        popup(t, 0, true, false);
        return;
    }
    int dmg = battle_attack_damage(a->atk, t->defending ? t->dfn * 2 : t->dfn, battle_spread(&b->rng));
    if (a->charged) {
        dmg *= 2;
        a->charged = false;
    }
    if (!a->monster) {
        const Member *m = member_of(b->g, a);
        const ItemDef *w = item_def(TB(b->g), m->equip[SLOT_WEAPON]);
        int crit = w ? w->eqExtra / 2 : 0;
        if (crit > 0 && rand_pct(&b->rng) < crit) {
            dmg = battle_attack_damage(a->atk, 0, battle_spread(&b->rng));
            fmt(b->bar, sizeof b->bar, btext(b->g, 5, "{0}の会心の一撃！"), a->name, "");
        }
    }
    hurt(b, t, element_scale(t, a->element, dmg));
}

BattleSkillInfo battle_skill_info(const SkillDef *s) {
    BattleSkillInfo k = {0};
    k.hit = 100;
    if (s->act && s->actSize >= 94) {
        for (int i = 0; i < 6; i++)
            if (s->act[56 + i]) k.states |= (u8)(1u << i);
        memcpy(&k.element, s->act + 86, 4);
        memcpy(&k.hit, s->act + 90, 4);
    }
    if (s->use && s->useSize > 14) k.drain = s->use[14] != 0;
    return k;
}

static const char *const kStateRecover[6] = {"{0}は毒から回復した！", "{0}は目を覚ました！", "{0}は麻痺から回復した！", "{0}は混乱から回復した！",
                                              "{0}は魅了から回復した！", "{0}は立ち上がった！"};
static const int kStateRecoverText[6] = {9, 10, 8, 5, 7, 6};

static void inflict(Battle *b, Fighter *t, u8 states) {
    for (int i = 0; i < 6; i++) {
        if (!(states & (1u << i)) || !alive(t)) continue;
        int chance = 100 - t->resist[6 + i];
        if (chance <= 0 || rand_pct(&b->rng) >= chance) continue;
        if (i == 5) {
            t->hp = 0;
            if (!t->monster) sync_member(b->g, t);
            continue;
        }
        t->state |= (u8)(1u << i);
        sg_log(b->g, "battle: %s state %d", t->name, i);
    }
}

static void cure(Battle *b, Fighter *t, u8 states) {
    for (int i = 0; i < 5; i++)
        if ((states & (1u << i)) && (t->state & (1u << i))) {
            t->state &= (u8)~(1u << i);
            fmt(b->bar, sizeof b->bar, stext(b->g, kStateRecoverText[i], kStateRecover[i]), t->name, "");
        }
}

static void apply_skill(Battle *b, Fighter *a, Fighter *t, const SkillDef *s) {
    BattleSkillInfo k = battle_skill_info(s);
    if (!guid_zero(s->effect)) start_fx(b, s->effect, t);
    bool friendly = s->target == TARGET_ALLY || s->target == TARGET_ALLIES || s->target == TARGET_SELF;
    if (friendly) {
        if ((k.states & 32) && t->hp <= 0) t->hp = 1;
        cure(b, t, k.states);
        int v = s->power > 0 ? s->power + a->mag : 0;
        if (v > 0 && t->hp > 0) {
            t->hp += v;
            if (t->hp > t->maxHp) t->hp = t->maxHp;
            popup(t, v, false, true);
        }
        if (!t->monster) sync_member(b->g, t);
        return;
    }
    if (rand_pct(&b->rng) >= k.hit - t->eva) {
        popup(t, 0, true, false);
        return;
    }
    int dmg = battle_skill_damage(s->power, a->mag, t->defending ? t->dfn * 2 : t->dfn, battle_spread(&b->rng));
    if (dmg < 1) dmg = 1;
    dmg = element_scale(t, k.element, dmg);
    int before = t->hp;
    hurt(b, t, dmg);
    if (k.drain && before > t->hp) {
        int v = before - t->hp;
        a->hp += v;
        if (a->hp > a->maxHp) a->hp = a->maxHp;
        popup(a, v, false, true);
        if (!a->monster) sync_member(b->g, a);
    }
    inflict(b, t, k.states);
}

static void apply_item(Battle *b, Fighter *t, const ItemDef *it) {
    if (!guid_zero(it->effect)) start_fx(b, it->effect, t);
    int hp = it->hp + t->maxHp * it->hpPercent / 100;
    int mp = it->mp + t->maxMp * it->mpPercent / 100;
    if (it->revive && t->hp <= 0) t->hp = 1;
    if (t->hp > 0) {
        t->hp += hp;
        if (t->hp > t->maxHp) t->hp = t->maxHp;
    }
    t->mp += mp;
    if (t->mp > t->maxMp) t->mp = t->maxMp;
    if (hp > 0) popup(t, hp, false, true);
    party_give_item(PT(b->g), it->id, -1);
    sync_member(b->g, t);
}

static void action_text(Battle *b, const Plan *p) {
    Fighter *a = &b->f[p->actor];
    SG *g = b->g;
    switch (p->kind) {
    case ACT_ATTACK: fmt(b->bar, sizeof b->bar, btext(g, 8, "{0}の攻撃！"), a->name, ""); break;
    case ACT_DEFEND: fmt(b->bar, sizeof b->bar, btext(g, 6, "{0}は身を守っている……"), a->name, ""); break;
    case ACT_WATCH: fmt(b->bar, sizeof b->bar, btext(g, 2, "{0}は様子を見ている"), a->name, ""); break;
    case ACT_CHARGE: fmt(b->bar, sizeof b->bar, btext(g, 7, "{0}は力をためている……"), a->name, ""); break;
    case ACT_FLEE: fmt(b->bar, sizeof b->bar, stext(g, 4, "{0}は逃げ出した！"), a->name, ""); break;
    case ACT_SKILL: snprintf(b->bar, sizeof b->bar, "%s", p->skill ? p->skill->name : ""); break;
    case ACT_ITEM: snprintf(b->bar, sizeof b->bar, "%s", p->item ? p->item->name : ""); break;
    default: b->bar[0] = 0; break;
    }
}

static int retarget(Battle *b, int target, bool monsters) {
    if (target >= 0 && b->f[target].monster == monsters && alive(&b->f[target])) return target;
    return first_target(b, monsters);
}

static void apply_plan(Battle *b, const Plan *p) {
    Fighter *a = &b->f[p->actor];
    switch (p->kind) {
    case ACT_ATTACK: {
        bool lost = a->state & (ST_CONFUSION | ST_CHARM);
        int t = lost && p->target >= 0 && alive(&b->f[p->target]) ? p->target : retarget(b, p->target, !a->monster);
        if (t >= 0) do_attack(b, a, &b->f[t]);
        break;
    }
    case ACT_CHARGE: a->charged = true; break;
    case ACT_FLEE: a->fled = true; break;
    case ACT_SKILL: {
        const SkillDef *s = p->skill;
        if (!s || a->mp < s->mpCost) break;
        a->mp -= s->mpCost;
        if (!a->monster) sync_member(b->g, a);
        bool foes = s->target == TARGET_ENEMY || s->target == TARGET_ENEMIES;
        bool targetsMonsters = foes ? !a->monster : a->monster;
        if (s->target == TARGET_ENEMIES || s->target == TARGET_ALLIES) {
            for (int i = 0; i < b->count; i++)
                if (b->f[i].monster == targetsMonsters && alive(&b->f[i])) apply_skill(b, a, &b->f[i], s);
        } else if (s->target == TARGET_SELF) {
            apply_skill(b, a, a, s);
        } else {
            int t = retarget(b, p->target, targetsMonsters);
            if (t >= 0) apply_skill(b, a, &b->f[t], s);
        }
        break;
    }
    case ACT_ITEM:
        if (p->item && party_item_count(PT(b->g), p->item->id) > 0 && p->target >= 0) apply_item(b, &b->f[p->target], p->item);
        break;
    default: break;
    }
}

static void finish(Battle *b, int result);

static void learn_levels(const HeroDef *h, Member *m, int from, int to) {
    if (!h) return;
    for (int i = 0; i < h->skillCount; i++) {
        if (h->skills[i].level <= from || h->skills[i].level > to) continue;
        bool dup = false;
        for (int k = 0; k < m->learnedCount && !dup; k++) dup = guid_eq(m->learned[k], h->skills[i].skill);
        if (!dup && m->learnedCount < 16) m->learned[m->learnedCount++] = h->skills[i].skill;
    }
}

static void grant_rewards(Battle *b) {
    SG *g = b->g;
    Tables *t = TB(g);
    Party *party = PT(g);
    b->expGain = b->moneyGain = 0;
    b->dropCount = 0;
    for (int i = b->partyCount; i < b->count; i++) {
        const Fighter *f = &b->f[i];
        if (f->fled) continue;
        b->expGain += f->def->exp;
        b->moneyGain += f->def->money;
        for (int k = 0; k < 2; k++)
            if (!guid_zero(f->def->drops[k].item) && rand_pct(&b->rng) < f->def->drops[k].rate && b->dropCount < 16)
                b->drops[b->dropCount++] = f->def->drops[k].item;
    }
    party->money += b->moneyGain;
    for (int i = 0; i < b->dropCount; i++) party_give_item(party, b->drops[i], 1);
    char line[160];
    b->lineCount = 0;
    for (int i = 0; i < b->partyCount; i++) {
        Fighter *f = &b->f[i];
        if (f->hp <= 0) continue;
        Member *m = member_of(g, f);
        m->exp += b->expGain;
        bool up = false;
        int level = m->level;
        while (level < 99 && m->exp >= battle_exp_for_level(level + 1)) {
            level++;
            up = true;
        }
        if (up) {
            learn_levels(hero_def(t, m->hero), m, m->level, level);
            member_set_level(t, m, level);
            f->maxHp = member_max_hp(t, m);
            f->maxMp = member_max_mp(t, m);
            snprintf(line, sizeof line, "%s %s %d", f->name, btext(g, 18, "レベルアップ！"), m->level);
            if (b->lineCount < 12) snprintf(b->lines[b->lineCount++], sizeof b->lines[0], "%s", line);
            sg_log(g, "battle: %s level %d exp %d", f->name, m->level, m->exp);
        }
    }
    sg_log(g, "battle: won, exp %d money %d drops %d", b->expGain, b->moneyGain, b->dropCount);
}

static void after_actions(Battle *b) {
    if (!living(b, true)) {
        set_phase(b, PH_VICTORY);
        fmt(b->bar, sizeof b->bar, btext(b->g, 12, "戦闘に勝利した！"), "", "");
        snd_bgm_stop(b->g, 0);
        snd_me(b->g, TB(b->g)->text.sounds[SND_VICTORY], 100, 100);
        grant_rewards(b);
        return;
    }
    if (!living(b, false)) {
        set_phase(b, PH_DEFEAT);
        b->bar[0] = 0;
        sg_log(b->g, "battle: party defeated");
        return;
    }
    begin_commands(b);
}

static void act_update(Battle *b) {
    while (b->queuePos < b->queueCount && !alive(&b->f[b->queue[b->queuePos].actor])) {
        b->queuePos++;
        b->applied = b->started = false;
        b->phaseMs = 0;
    }
    if (b->queuePos >= b->queueCount) {
        after_actions(b);
        return;
    }
    Plan *p = &b->queue[b->queuePos];
    Fighter *actor = &b->f[p->actor];
    if (!b->started) {
        b->started = true;
        b->ticked = false;
        if (actor->state & (ST_SLEEP | ST_PARALYSIS)) {
            bool sleep = actor->state & ST_SLEEP;
            if (rand_pct(&b->rng) < kRecoverPct) {
                actor->state &= (u8)~(ST_SLEEP | ST_PARALYSIS);
                fmt(b->bar, sizeof b->bar, sleep ? stext(b->g, 10, "{0}は目を覚ました！") : stext(b->g, 8, "{0}は麻痺から回復した！"), actor->name, "");
            } else {
                fmt(b->bar, sizeof b->bar, sleep ? btext(b->g, 3, "{0}は眠っている……") : btext(b->g, 4, "{0}は麻痺で動けない！"), actor->name, "");
            }
            p->kind = ACT_NONE;
            b->applied = true;
        } else if (actor->state & (ST_CONFUSION | ST_CHARM)) {
            bool charm = actor->state & ST_CHARM;
            if (rand_pct(&b->rng) < kRecoverPct) {
                actor->state &= (u8)~(ST_CONFUSION | ST_CHARM);
                fmt(b->bar, sizeof b->bar, charm ? stext(b->g, 7, "{0}は魅了から回復した！") : stext(b->g, 5, "{0}は混乱から回復した！"), actor->name, "");
                p->kind = ACT_NONE;
                b->applied = true;
            } else {
                p->kind = ACT_ATTACK;
                p->skill = NULL;
                p->target = charm ? random_target(b, actor->monster) : random_any(b);
                action_text(b, p);
            }
        } else {
            action_text(b, p);
        }
        if (actor->monster && p->kind != ACT_NONE) actor->flashMs = 1;
    }
    if (!b->applied && b->phaseMs >= (u32)kHitMs) {
        b->applied = true;
        apply_plan(b, p);
    } else if (!b->applied && b->phaseMs >= 16 && b->f[p->actor].pose == MO_WAIT && !b->f[p->actor].monster) {
        set_pose(&b->f[p->actor], p->kind == ACT_SKILL ? MO_SKILL : p->kind == ACT_DEFEND ? MO_GUARD : MO_ATTACK);
    }
    if (b->phaseMs < (u32)kActMs) return;
    if (!b->ticked && alive(actor) && (actor->state & ST_POISON)) {
        b->ticked = true;
        int dmg = actor->maxHp / 10;
        fmt(b->bar, sizeof b->bar, stext(b->g, 1, "{0}は毒を受けている！"), actor->name, "");
        hurt(b, actor, dmg);
        b->phaseMs = (u32)(kActMs - kTickMs);
        return;
    }
    b->queuePos++;
    b->applied = b->started = false;
    b->phaseMs = 0;
    b->bar[0] = 0;
    if (!living(b, true) || !living(b, false)) after_actions(b);
}

static void commit_member(Battle *b) {
    int n = next_member(b, b->member + 1);
    if (n < 0) {
        build_queue(b);
        return;
    }
    b->member = n;
    b->cmdCursor = 0;
    build_commands(b);
    set_phase(b, PH_COMMAND);
}

static void try_escape(Battle *b) {
    int ps = 0, pn = 0, ms = 0, mn = 0;
    for (int i = 0; i < b->count; i++) {
        if (!alive(&b->f[i])) continue;
        if (b->f[i].monster) ms += b->f[i].spd, mn++;
        else ps += b->f[i].spd, pn++;
    }
    bool ok = battle_escape_roll(&b->rng, pn ? ps / pn : 0, mn ? ms / mn : 0);
    sg_log(b->g, "battle: escape %s", ok ? "succeeded" : "failed");
    if (ok) {
        fmt(b->bar, sizeof b->bar, btext(b->g, 14, "逃げ出した……"), "", "");
        set_phase(b, PH_ESCAPE);
        b->result = BATTLE_ESCAPED;
        return;
    }
    snprintf(b->bar, sizeof b->bar, "%s", stext(b->g, 11, "逃げることができなかった！"));
    for (int i = 0; i < b->partyCount; i++) b->plan[i].kind = ACT_NONE;
    set_phase(b, PH_ESCAPE);
}

static void build_list(Battle *b) {
    SG *g = b->g;
    b->listCount = 0;
    if (b->listKind == LIST_SKILLS) {
        const Member *m = member_of(g, &b->f[b->member]);
        const HeroDef *h = hero_of(g, &b->f[b->member]);
        if (!h) return;
        for (int i = 0; i < h->skillCount && b->listCount < MAX_LIST; i++) {
            if (h->skills[i].level > m->level) continue;
            const SkillDef *s = skill_def(TB(g), h->skills[i].skill);
            if (s && s->battleUse) b->list[b->listCount++] = s;
        }
        for (int i = 0; i < m->learnedCount && b->listCount < MAX_LIST; i++) {
            const SkillDef *s = skill_def(TB(g), m->learned[i]);
            bool dup = false;
            for (int k = 0; k < b->listCount && !dup; k++) dup = b->list[k] == s;
            if (s && s->battleUse && !dup) b->list[b->listCount++] = s;
        }
    } else {
        Party *p = PT(g);
        for (int i = 0; i < p->itemCount && b->listCount < MAX_LIST; i++) {
            const ItemDef *it = item_def(TB(g), p->items[i].item);
            if (it && it->kind == ITEM_EXPENDABLE && it->usable[0]) b->list[b->listCount++] = it;
        }
    }
    if (b->listCursor >= b->listCount) b->listCursor = 0;
}

static void choose_command(Battle *b) {
    int kind = b->cmds[b->cmdCursor];
    Plan *p = &b->plan[b->member];
    switch (kind) {
    case CMD_KIND_ATTACK:
        p->kind = ACT_ATTACK;
        b->targetCursor = first_target(b, true);
        set_phase(b, PH_TARGET);
        break;
    case CMD_KIND_DEFEND:
        p->kind = ACT_DEFEND;
        commit_member(b);
        break;
    case CMD_KIND_SKILL:
    case CMD_KIND_ITEM:
        b->listKind = kind == CMD_KIND_SKILL ? LIST_SKILLS : LIST_ITEMS;
        b->listCursor = 0;
        build_list(b);
        set_phase(b, PH_LIST);
        break;
    case CMD_KIND_ESCAPE:
        if (b->setup.noEscape) {
            sgb_sound(b->g, SND_CANCEL);
            break;
        }
        try_escape(b);
        break;
    default: break;
    }
}

static void choose_list(Battle *b) {
    if (!b->listCount) return;
    Plan *p = &b->plan[b->member];
    if (b->listKind == LIST_SKILLS) {
        const SkillDef *s = b->list[b->listCursor];
        if (s->mpCost > b->f[b->member].mp) {
            sgb_sound(b->g, SND_CANCEL);
            return;
        }
        p->kind = ACT_SKILL;
        p->skill = s;
        p->item = NULL;
        if (s->target == TARGET_ENEMY) {
            b->targetCursor = first_target(b, true);
            set_phase(b, PH_TARGET);
        } else if (s->target == TARGET_ALLY) {
            b->targetCursor = b->member;
            set_phase(b, PH_ALLY);
        } else {
            p->target = s->target == TARGET_SELF ? b->member : -1;
            commit_member(b);
        }
    } else {
        p->kind = ACT_ITEM;
        p->item = b->list[b->listCursor];
        p->skill = NULL;
        b->targetCursor = b->member;
        set_phase(b, PH_ALLY);
    }
}

static void input(Battle *b) {
    SG *g = b->g;
    switch (b->phase) {
    case PH_COMMAND:
        if (sg_pressed(g, KEY_UP)) b->cmdCursor = (b->cmdCursor + b->cmdCount - 1) % b->cmdCount;
        if (sg_pressed(g, KEY_DOWN)) b->cmdCursor = (b->cmdCursor + 1) % b->cmdCount;
        if (sg_pressed(g, KEY_OK)) choose_command(b);
        else if (sg_pressed(g, KEY_CANCEL)) {
            int pm = prev_member(b, b->member - 1);
            if (pm >= 0) {
                b->member = pm;
                b->plan[pm].kind = ACT_NONE;
                b->cmdCursor = 0;
                build_commands(b);
            }
        }
        break;
    case PH_TARGET:
        if (sg_pressed(g, KEY_LEFT) || sg_pressed(g, KEY_UP)) b->targetCursor = cycle_target(b, b->targetCursor, -1, true);
        if (sg_pressed(g, KEY_RIGHT) || sg_pressed(g, KEY_DOWN)) b->targetCursor = cycle_target(b, b->targetCursor, 1, true);
        if (sg_pressed(g, KEY_OK)) {
            b->plan[b->member].target = b->targetCursor;
            commit_member(b);
        } else if (sg_pressed(g, KEY_CANCEL)) {
            b->plan[b->member].kind = ACT_NONE;
            set_phase(b, b->plan[b->member].skill ? PH_LIST : PH_COMMAND);
        }
        break;
    case PH_ALLY:
        if (sg_pressed(g, KEY_UP) || sg_pressed(g, KEY_LEFT)) b->targetCursor = (b->targetCursor + b->partyCount - 1) % b->partyCount;
        if (sg_pressed(g, KEY_DOWN) || sg_pressed(g, KEY_RIGHT)) b->targetCursor = (b->targetCursor + 1) % b->partyCount;
        if (sg_pressed(g, KEY_OK)) {
            b->plan[b->member].target = b->targetCursor;
            commit_member(b);
        } else if (sg_pressed(g, KEY_CANCEL)) {
            set_phase(b, PH_LIST);
        }
        break;
    case PH_LIST:
        if (b->listCount) {
            if (sg_pressed(g, KEY_UP)) b->listCursor = (b->listCursor + b->listCount - 1) % b->listCount;
            if (sg_pressed(g, KEY_DOWN)) b->listCursor = (b->listCursor + 1) % b->listCount;
        }
        if (sg_pressed(g, KEY_OK)) choose_list(b);
        else if (sg_pressed(g, KEY_CANCEL)) {
            b->plan[b->member] = (Plan){b->member, ACT_NONE, -1, NULL, NULL};
            set_phase(b, PH_COMMAND);
        }
        break;
    case PH_RESULT:
        if (b->phaseMs > 300 && sg_pressed(g, KEY_OK)) finish(b, BATTLE_WON);
        break;
    case PH_GAMEOVER:
        if (b->phaseMs > 1500 && sg_pressed(g, KEY_OK)) finish(b, BATTLE_LOST);
        break;
    default: break;
    }
}

static void restore_field(Battle *b) {
    if (b->restored || !b->staged) return;
    b->restored = true;
    SG *g = b->g;
    field_free(&g->field);
    g->field = b->saved;
    g->e.curMap = b->fieldMap;
    R3x *x = g->r3.x;
    if (g->r3.view != b->fieldView) {
        r3_shadow_free(&g->r3);
        view_free(g->r3.view);
    }
    g->r3.view = b->fieldView;
    x->shadow = b->fieldShadow;
    x->shadowOn = b->fieldShadowOn;
    memcpy(x->amb, b->fieldLight, sizeof x->amb);
    memcpy(x->sun, b->fieldLight + 3, sizeof x->sun);
    memcpy(x->sunDir, b->fieldLight + 6, sizeof x->sunDir);
    x->animU = b->fieldAnimU;
}

static void resume_music(Battle *b) {
    SG *g = b->g;
    if (snd_bgm_release(g, true)) return;
    if (b->hadBgm) snd_bgm(g, b->fieldBgm, 100, 100);
    else snd_bgm_stop(g, 0);
}

static void free_battle(SG *g) {
    int s = slot_of(g, false);
    if (s < 0) return;
    Battle *b = registry[s].b;
    if (b) {
        for (int i = 0; i < b->count; i++) {
            model_free(b->f[i].model);
            for (int k = 0; k < MO_COUNT; k++) img_free(b->f[i].motion[k]);
        }
        img_free(b->gameover);
        free(b);
    }
    registry[s].b = NULL;
}

static void finish(Battle *b, int result) {
    SG *g = b->g;
    b->result = result;
    for (int i = 0; i < b->partyCount; i++) sync_member(g, &b->f[i]);
    restore_field(b);
    if (b->staged && (result != BATTLE_LOST || b->setup.continueOnLose)) resume_music(b);
    else snd_bgm_release(g, false);
    if (result == BATTLE_LOST && !b->setup.continueOnLose) {
        interp_free(&g->interp);
        g->scene = SCENE_TITLE;
        g->titleCursor = 0;
        sg_log(g, "battle: game over");
    }
    sg_log(g, "battle: end %d", result);
    set_phase(b, PH_DONE);
}

void battle_seed(SG *g, u32 seed) {
    int s = slot_of(g, true);
    if (s < 0) return;
    registry[s].seed = seed;
    registry[s].seeded = true;
    if (registry[s].b) registry[s].b->rng = seed;
}

static void setup_stage(Battle *b) {
    SG *g = b->g;
    const BattleSetup *s = &b->setup;
    b->saved = g->field;
    b->fieldMap = g->e.curMap;
    b->hadBgm = snd_bgm_playing(g, &b->fieldBgm);
    Guid mapId = g->field.map.id;
    Guid bg = battle_background_for(g, s, mapId);
    Guid bgm = battle_bgm_for(g, s, mapId);
    R3x *x = g->r3.x;
    b->fieldView = g->r3.view;
    b->fieldShadow = x->shadow;
    b->fieldShadowOn = x->shadowOn;
    memcpy(b->fieldLight, x->amb, sizeof x->amb);
    memcpy(b->fieldLight + 3, x->sun, sizeof x->sun);
    memcpy(b->fieldLight + 6, x->sunDir, sizeof x->sunDir);
    b->fieldAnimU = x->animU;
    g->r3.view = NULL;
    memset(&x->shadow, 0, sizeof x->shadow);
    x->shadowOn = false;
    Actor player = g->field.player;
    memset(&g->field, 0, sizeof g->field);
    g->field.player = player;
    int cx = 0, cz = 0;
    bool loaded = false;
    if (!guid_zero(bg) && game_record(&g->data, SGR_MAP, 0, bg, NULL)) {
        bool interpWasActive = g->interp.active;
        g->interp.active = true;
        loaded = field_load(g, bg, 0, 0, 0);
        g->interp.active = interpWasActive;
        for (int i = 0; i < g->field.npcCount; i++) g->field.npcs[i].visible = false;
        View *v = g->r3.view;
        if (loaded && v && v->look.sky != 0) {
            static const char *parts[3] = {"Sky noon - 01", "Cloud noon - 01", "Sun noon - 01"};
            v->look.sky = 0;
            for (int i = 0; i < 3; i++)
                if (!v->sky[i]) {
                    char *path = sg_fmt("samples/common/res/system/%s.fbx", parts[i]);
                    v->sky[i] = model_load(g, path);
                    free(path);
                }
        }
        if (loaded) {
            cx = s->hasCenter ? s->centerX : g->field.map.w / 2;
            cz = s->hasCenter ? s->centerZ : g->field.map.h / 2;
        }
    }
    Field *f = &g->field;
    f->player.visible = false;
    float ground = 0;
    if (loaded && cx >= 0 && cz >= 0 && cx < f->map.w && cz < f->map.h) ground = (float)f->map.height[(size_t)cz * (size_t)f->map.w + (size_t)cx];
    float bx = (float)cx + 0.5f, bz = (float)cz + 0.5f;
    for (int i = 0; i < b->partyCount; i++) {
        b->f[i].x = bx + kPartyX + ((float)i - (float)(b->partyCount - 1) / 2.0f) * kPartySpacing;
        b->f[i].z = bz + kPartyZ;
    }
    for (int i = b->partyCount; i < b->count; i++) {
        int k = i - b->partyCount, n = b->count - b->partyCount;
        float ox = ((float)k - (float)(n - 1) / 2.0f) * 2.0f, oz = -3.0f;
        if (s->hasLayout) {
            ox = (float)s->layout[k][0];
            oz = (float)s->layout[k][1];
        }
        b->f[i].x = bx + ox;
        b->f[i].z = bz + oz;
    }
    f->cam.pitch = 28.0f;
    f->cam.yaw = 0.0f;
    f->cam.fov = 19.0f;
    f->cam.distance = 33.0f;
    f->cam.target = (Vec3){bx + 1.44f, ground + 0.49f, bz - 1.31f};
    b->stage = f->cam;
    apply_cam(b, f->cam);
    for (int i = 0; i < b->count; i++) b->f[i].fadeMs = 0;
    f->loaded = loaded;
    g->e.curMap = f->map.id;
    if (!guid_zero(bgm)) {
        snd_bgm_hold(g);
        snd_bgm(g, bgm, 100, 100);
    }
    b->staged = true;
}

void sg_battle_start(SG *g, const BattleSetup *s) {
    int slot = slot_of(g, true);
    if (slot < 0) return;
    if (registry[slot].b) free_battle(g);
    Battle *b = sg_alloc(sizeof *b);
    registry[slot].b = b;
    b->g = g;
    b->setup = *s;
    b->rng = registry[slot].seeded ? registry[slot].seed : (g->clockMs * 2654435761u) ^ 0x5bd1e995u;
    Party *p = PT(g);
    for (int i = 0; i < p->count && b->count < 4; i++) add_member(b, i, 0, 0);
    b->partyCount = b->count;
    for (int i = 0; i < s->count; i++) {
        const MonsterDef *m = monster_def(TB(g), s->monsters[i]);
        if (m) add_monster(b, m, 0, 0);
    }
    fmt(b->bar, sizeof b->bar, btext(g, 11, "モンスターが現れた！"), "", "");
    sg_log(g, "battle: start, %d members, %d monsters", b->partyCount, b->count - b->partyCount);
    set_phase(b, PH_INTRO);
    if (b->partyCount == 0 || b->count == b->partyCount) finish(b, BATTLE_WON);
}

bool sg_battle_active(SG *g) {
    Battle *b = get(g);
    return b && b->phase != PH_DONE;
}

int sg_battle_result(SG *g) {
    Battle *b = get(g);
    return b ? b->result : BATTLE_NONE;
}

const char *battle_phase(SG *g) {
    static const char *names[] = {"intro", "command", "target", "list", "ally", "act", "escape", "victory", "result", "defeat", "gameover", "done"};
    Battle *b = get(g);
    return b ? names[b->phase] : "none";
}

int battle_monster_hp(SG *g, int index) {
    Battle *b = get(g);
    if (!b || index < 0 || b->partyCount + index >= b->count) return -1;
    return b->f[b->partyCount + index].hp;
}

static const char *shown_bar(Battle *b) {
    if ((b->phase == PH_TARGET || b->phase == PH_ALLY) && b->targetCursor >= 0) return b->f[b->targetCursor].name;
    return b->bar;
}

const char *battle_bar(SG *g) {
    Battle *b = get(g);
    return b ? shown_bar(b) : "";
}

void battle_abort(SG *g) {
    Battle *b = get(g);
    if (!b || b->phase == PH_DONE) return;
    restore_field(b);
    snd_bgm_release(g, false);
    set_phase(b, PH_DONE);
    sg_log(g, "battle: abandoned");
}

void battle_close(SG *g) {
    free_battle(g);
    int s = slot_of(g, false);
    if (s >= 0) memset(&registry[s], 0, sizeof registry[s]);
}

static float clip_length(Model *m, const char *name);
static const char *motion_name(Model *m, int pose, bool won);

static u32 pose_length(Fighter *f) {
    if (f->model) {
        const char *mo = motion_name(f->model, f->pose, false);
        float len = mo ? clip_length(f->model, mo) : 0;
        return (u32)(len * 1000.0f);
    }
    return (u32)(f->motionFrames[f->pose] * f->motionMs[f->pose]);
}

static void update(Battle *b, u32 ms) {
    b->phaseMs += ms;
    b->totalMs += ms;
    for (int i = 0; i < b->count; i++) {
        Fighter *f = &b->f[i];
        if (f->popupMs) f->popupMs = f->popupMs + ms > 900 ? 0 : f->popupMs + ms;
        if (f->flashMs) f->flashMs = f->flashMs + ms > 300 ? 0 : f->flashMs + ms;
    }
    for (int i = 0; i < 8; i++) {
        if (!b->fx[i].on) continue;
        b->fx[i].ms += ms;
        if (b->fx[i].ms >= battle_fx_ms(&b->fx[i].def)) b->fx[i].on = false;
    }
    for (int i = 0; i < b->count; i++) {
        Fighter *f = &b->f[i];
        if (f->hitMs) f->hitMs = f->hitMs + ms > 300 ? 0 : f->hitMs + ms;
        if (f->monster && !alive(f)) f->fadeMs += ms;
        f->poseMs += ms;
        if (!f->monster) {
            if (f->hp <= 0) {
                if (f->pose != MO_KO) set_pose(f, MO_KO);
            } else if (f->pose == MO_KO || (f->pose != MO_WAIT && f->pose != MO_GUARD && f->poseMs >= pose_length(f))) {
                set_pose(f, f->defending ? MO_GUARD : MO_WAIT);
            } else if (f->pose == MO_GUARD && !f->defending) {
                set_pose(f, MO_WAIT);
            }
        }
    }
    input(b);
    switch (b->phase) {
    case PH_INTRO: {
        float t = ease((float)b->phaseMs / (float)kPushMs);
        Cam c = b->stage;
        c.distance = b->stage.distance * (1.6f - 0.6f * t);
        c.pitch = b->stage.pitch + 8.0f * (1.0f - t);
        apply_cam(b, c);
        if (b->phaseMs >= (u32)kIntroMs) {
            apply_cam(b, b->stage);
            begin_commands(b);
        }
        break;
    }
    case PH_ACT: {
        act_update(b);
        if (b->phase != PH_ACT || b->queuePos >= b->queueCount) {
            if (b->phase == PH_COMMAND) apply_cam(b, b->stage);
            break;
        }
        const Fighter *a = &b->f[b->queue[b->queuePos].actor];
        float in = ease((float)b->phaseMs / 300.0f), out = ease((float)((int)kActMs - (int)b->phaseMs) / 300.0f);
        float t = in < out ? in : out;
        Cam c = b->stage;
        c.target.x += (a->x - c.target.x) * kActorPull * t;
        c.target.z += (a->z - c.target.z) * kActorPull * t;
        c.distance *= 1.0f - (1.0f - kActorZoom) * t;
        apply_cam(b, c);
        break;
    }
    case PH_ESCAPE:
        if (b->phaseMs >= 1200) {
            if (b->result == BATTLE_ESCAPED) finish(b, BATTLE_ESCAPED);
            else {
                b->bar[0] = 0;
                build_queue(b);
            }
        }
        break;
    case PH_VICTORY:
        if (b->phaseMs >= 1200) {
            set_phase(b, PH_RESULT);
            front_cam(b);
        }
        break;
    case PH_DEFEAT:
        if (b->phaseMs >= (u32)kDefeatMs) {
            if (b->setup.continueOnLose) {
                finish(b, BATTLE_LOST);
            } else {
                restore_field(b);
                snd_bgm_stop(b->g, 0);
                snd_bgs_stop(b->g, 0);
                snd_me(b->g, TB(b->g)->text.sounds[SND_GAMEOVER], 100, 100);
                set_phase(b, PH_GAMEOVER);
            }
        }
        break;
    default: break;
    }
}

static void draw_billboard_ex(SG *g, const Cam *cam, const Image *im, int sx, int sy, int sw, int sh, float x, float y, float z, float scale, bool add) {
    float w = (float)sw / 48.0f * scale, h = (float)sh / 48.0f * scale;
    float p = cam->pitch * 3.14159265f / 180.0f, yaw = cam->yaw * 3.14159265f / 180.0f;
    float tall = h / cosf(p);
    float rx = cosf(yaw) * w / 2, rz = sinf(yaw) * w / 2;
    float u0 = (float)sx / (float)im->w, u1 = (float)(sx + sw) / (float)im->w;
    float v0 = (float)sy / (float)im->h, v1 = (float)(sy + sh) / (float)im->h;
    float inset = 0.5f / (float)im->w;
    Vert va = {x - rx, y + tall, z - rz, u0 + inset, v0, 1}, vb = {x + rx, y + tall, z + rz, u1 - inset, v0, 1};
    Vert vc = {x + rx, y, z + rz, u1 - inset, v1, 1}, vd = {x - rx, y, z - rz, u0 + inset, v1, 1};
    Tri t1 = {{va, vb, vc}, im, true, false}, t2 = {{va, vc, vd}, im, true, false};
    if (add) {
        r3_tri_ex(&g->r3, cam, &t1, TRI_ADD, TF_NOZWRITE);
        r3_tri_ex(&g->r3, cam, &t2, TRI_ADD, TF_NOZWRITE);
    } else {
        r3_tri(&g->r3, cam, &t1);
        r3_tri(&g->r3, cam, &t2);
    }
}

static void draw_billboard(SG *g, const Cam *cam, const Image *im, int sx, int sy, int sw, int sh, float x, float y, float z, float scale) {
    draw_billboard_ex(g, cam, im, sx, sy, sw, sh, x, y, z, scale, false);
}

static float clip_length(Model *m, const char *name) {
    if (!m->rig || !model_has_motion(m, name)) return 0;
    for (int i = 0; i < m->rig->clipCount; i++)
        if (strcasecmp(m->rig->clips[i].name, name) == 0) return (float)(m->rig->clips[i].end - m->rig->clips[i].start);
    return 0;
}

static const char *motion_name(Model *m, int pose, bool won) {
    static const char *names[MO_COUNT] = {"wait", "attack", "damage", "guard", "ko", "skill"};
    if (won && model_has_motion(m, "win")) return "win";
    if (model_has_motion(m, names[pose])) return names[pose];
    return model_has_motion(m, "wait") ? "wait" : NULL;
}

static void draw_model(SG *g, const Cam *cam, Model *m, float x, float y, float z, float angle, float scale, const char *motion, float seconds) {
    if (motion) {
        float len = clip_length(m, motion);
        if (len > 0 && (strcmp(motion, "win") == 0 || strcmp(motion, "wait") == 0 || strcmp(motion, "guard") == 0)) seconds = fmodf(seconds, len);
        model_pose(m, motion, seconds);
    }
    const Mesh *mesh = model_mesh(m);
    float cs = cosf(angle), sn = sinf(angle);
    for (int i = 0; i < mesh->count; i++) {
        Tri t = mesh->tris[i];
        for (int k = 0; k < 3; k++) {
            float vx = t.v[k].x * scale, vz = t.v[k].z * scale;
            t.v[k].x = x + vx * cs - vz * sn;
            t.v[k].z = z + vx * sn + vz * cs;
            t.v[k].y = y + t.v[k].y * scale;
        }
        r3_tri(&g->r3, cam, &t);
    }
}

static float stage_ground(SG *g, float x, float z) {
    Field *f = &g->field;
    if (!f->loaded) return 0;
    int cx = (int)x, cz = (int)z;
    if (cx < 0 || cz < 0 || cx >= f->map.w || cz >= f->map.h) return 0;
    return (float)f->map.height[(size_t)cz * (size_t)f->map.w + (size_t)cx];
}

static void draw_fighter(Battle *b, const Fighter *f, bool active) {
    SG *g = b->g;
    const Cam *cam = &g->field.cam;
    float y = stage_ground(g, f->x, f->z);
    if (f->monster && !alive(f) && f->fadeMs > 500) return;
    if (!f->monster && active) {
        static Image tile = {1, 1, NULL};
        static u32 px = 0x80a0ff80u;
        tile.px = &px;
        Vert a = {f->x - 0.7f, y + 0.02f, f->z - 0.7f, 0, 0, 1}, c = {f->x + 0.7f, y + 0.02f, f->z + 0.7f, 1, 1, 1};
        Vert bb = {c.x, a.y, a.z, 1, 0, 1}, d = {a.x, a.y, c.z, 0, 1, 1};
        r3_tri(&g->r3, cam, &(Tri){{a, bb, c}, &tile, false, false});
        r3_tri(&g->r3, cam, &(Tri){{a, c, d}, &tile, false, false});
    }
    if (f->hitMs && (f->hitMs / 60) % 2) return;
    if (f->model) {
        bool won = b->front && !f->monster;
        const char *mo = motion_name(f->model, f->hp <= 0 && !f->monster ? MO_KO : f->pose, won);
        draw_model(g, cam, f->model, f->x, y, f->z, f->monster ? 0.0f : 3.14159265f, f->monster ? 1.0f : kHeroScale, mo,
                   (float)(won ? b->phaseMs : f->poseMs) / 1000.0f);
        return;
    }
    const Image *im = f->image;
    if (!im) return;
    if (f->monster) {
        draw_billboard(g, cam, im, 0, 0, im->w, im->h, f->x, y, f->z, kMonsterScale);
        if (f->flashMs) draw_billboard_ex(g, cam, im, 0, 0, im->w, im->h, f->x, y, f->z, kMonsterScale, true);
        return;
    }
    int pose = f->motion[f->pose] ? f->pose : MO_WAIT;
    const Image *mo = f->motion[pose];
    if (mo) {
        int frames = f->motionFrames[pose];
        int cw = mo->w / frames;
        int frame = (int)(f->poseMs / (u32)f->motionMs[pose]);
        frame = pose == MO_WAIT || pose == MO_GUARD ? frame % frames : frame < frames ? frame : frames - 1;
        draw_billboard(g, cam, mo, frame * cw, 0, cw, mo->h, f->x, y, f->z, kHeroScale);
        return;
    }
    const Res *r = f->graphic;
    int frames = r && r->frames > 0 ? r->frames : 1;
    int rows = r ? sg_sprite_rows(r, im) : 1;
    int cw = im->w / frames, ch = im->h / rows;
    int frame = frames == 3 && rows > 1 ? 1 : rows == 1 && r->frameMs > 0 ? (int)(f->poseMs / (u32)r->frameMs) % frames : 0;
    draw_billboard(g, cam, im, frame * cw, b->front && rows > 1 ? ch : 0, cw, ch, f->x, y, f->z, kHeroScale);
}

static void project_point(const Cam *c, float x, float y, float z, int *sx, int *sy) {
    const float *m = c->m;
    float vx = m[0] * x + m[1] * y + m[2] * z + m[3];
    float vy = m[4] * x + m[5] * y + m[6] * z + m[7];
    float vz = m[8] * x + m[9] * y + m[10] * z + m[11];
    if (vz < 0.05f) vz = 0.05f;
    *sx = (int)(c->cx + vx * c->focal / vz);
    *sy = (int)(c->cy + vy * c->focal / vz);
}

static void text_shadow(SG *g, int x, int y, int px, const char *s, u32 color) {
    font_draw(g->font, g->fb, g->w, g->h, x + 1, y + 1, px, s, 0xc0000000u);
    font_draw(g->font, g->fb, g->w, g->h, x, y, px, s, color);
}

static void rounded_box(SG *g, int x, int y, int w, int h, u32 fill, u32 edge) {
    static const int cut[3] = {3, 1, 1};
    for (int r = 0; r < h; r++) {
        int k = r < 3 ? cut[r] : h - 1 - r < 3 ? cut[h - 1 - r] : 0;
        bool border = r < 2 || r >= h - 2;
        fill_rect(g->fb, g->w, g->h, x + k, y + r, w - 2 * k, 1, border ? edge : fill);
        if (!border) {
            fill_rect(g->fb, g->w, g->h, x + k, y + r, 2, 1, edge);
            fill_rect(g->fb, g->w, g->h, x + w - k - 2, y + r, 2, 1, edge);
        }
    }
}

static void intro_box(Battle *b) {
    SG *g = b->g;
    if (b->phaseMs < 200 || b->phaseMs >= (u32)kIntroMs) return;
    const char *s = b->bar;
    int tw = font_width(g->font, 23, s), w = tw + 48, h = 54;
    if (b->phaseMs > 900) {
        float t = (float)(b->phaseMs - 900) / (float)(kIntroMs - 900);
        w = (int)((float)w * (1.0f - 0.75f * t));
        s = "";
    }
    int x = (g->w - w) / 2, y = (g->h - h) / 2;
    rounded_box(g, x, y, w, h, 0xe0101010u, 0xfff0f0f0u);
    if (*s) text_shadow(g, (g->w - tw) / 2, y + 13, 23, s, kWhite);
}

static void draw_states(Battle *b) {
    static const int cells[5][2] = {{7, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}};
    SG *g = b->g;
    Image *icons = NULL;
    for (int i = 0; i < g->res.count && !icons; i++)
        if (g->res.items[i].kind == RES_ICON && strcmp(g->res.items[i].name, "icon_system") == 0) icons = sg_image(g, g->res.items[i].id);
    if (!icons || b->front) return;
    const Cam *cam = &g->field.cam;
    for (int i = 0; i < b->count; i++) {
        const Fighter *f = &b->f[i];
        if (!alive(f) || !(f->state & 31)) continue;
        int k = 0;
        while (!(f->state & (1u << k))) k++;
        float h = f->monster && f->image ? (float)f->image->h / 48.0f * kMonsterScale : 2.6f;
        int sx, sy;
        project_point(cam, f->x, stage_ground(g, f->x, f->z) + h + 0.2f, f->z, &sx, &sy);
        int cell = icons->w / 16;
        rounded_box(g, sx - 14, sy - 26, 28, 24, 0xf0ffffffu, 0xffd0d0d0u);
        blit(g->fb, g->w, g->h, icons, cells[k][0] * cell, cells[k][1] * cell, cell, cell, sx - 10, sy - 24, 20, 20, 255);
    }
}

static void bar_box(SG *g, const char *s) {
    if (!s || !*s) return;
    int x = 190, y = 4, w = 586, h = 32;
    fill_rect(g->fb, g->w, g->h, x, y, w, h, 0xd0333333u);
    fill_rect(g->fb, g->w, g->h, x, y, w, 2, 0xffd0d0d0u);
    int tw = font_width(g->font, 20, s);
    text_shadow(g, x + (w - tw) / 2, y + 4, 20, s, kWhite);
}

static void header_line(SG *g, int x, int y, int w, const char *label) {
    text_shadow(g, x, y - 16, 15, label, kWhite);
    fill_rect(g->fb, g->w, g->h, x, y, w, 1, 0xffffffffu);
}

static void gauge(SG *g, int x, int y, int w, int v, int max, u32 color) {
    fill_rect(g->fb, g->w, g->h, x, y, w, 6, 0xc0303040u);
    int fw = max > 0 ? w * (v < 0 ? 0 : v) / max : 0;
    fill_rect(g->fb, g->w, g->h, x + 1, y + 1, fw > 2 ? fw - 2 : 0, 4, color);
}

static void draw_status(Battle *b) {
    SG *g = b->g;
    int x = 544, y = 434;
    header_line(g, x, y, 210, "Name");
    header_line(g, x + 210, y, 104, TB(g)->text.menu[MT_HP] ? TB(g)->text.menu[MT_HP] : "HP");
    header_line(g, x + 314, y, 102, TB(g)->text.menu[MT_MP] ? TB(g)->text.menu[MT_MP] : "MP");
    for (int i = 0; i < b->partyCount; i++) {
        const Fighter *f = &b->f[i];
        int ry = y + 10 + i * 25;
        bool cur = (b->phase == PH_COMMAND || b->phase == PH_TARGET || b->phase == PH_LIST) && i == b->member;
        bool ally = b->phase == PH_ALLY && i == b->targetCursor;
        if (cur || ally) fill_rect(g->fb, g->w, g->h, x, ry, 206, 23, 0x60ffffffu);
        u32 color = f->hp <= 0 ? 0xffff8080u : kWhite;
        text_shadow(g, x + 2, ry, 18, f->name, color);
        char num[32];
        snprintf(num, sizeof num, "%d", f->hp);
        text_shadow(g, x + 210, ry, 18, num, color);
        gauge(g, x + 210, ry + 18, 94, f->hp, f->maxHp, 0xffe09090u);
        snprintf(num, sizeof num, "%d", f->mp);
        text_shadow(g, x + 314, ry, 18, num, kWhite);
        gauge(g, x + 314, ry + 18, 94, f->mp, f->maxMp, 0xff60d060u);
    }
}

static void icon_at(SG *g, Guid sheet, int ix, int iy, int x, int y) {
    Image *im = sg_image(g, sheet);
    if (!im) return;
    int cell = im->w / 16;
    blit(g->fb, g->w, g->h, im, ix * cell, iy * cell, cell, cell, x, y, 28, 28, 255);
}

static void escape_icon(SG *g, int x, int y) {
    const SgrRec *r = g->data.settingsFile.count ? &g->data.settingsFile.recs[0] : NULL;
    if (!r) return;
    int found = 0;
    Guid id = {{0}};
    u8 ix = 0, iy = 0;
    for (u32 k = r->size >= 18 ? r->size - 18 : 0; k > 0 && found < 3; k--) {
        Guid gg;
        memcpy(gg.b, r->body + k, 16);
        const Res *res = res_find(&g->res, gg);
        if (!res || res->kind != RES_ICON) continue;
        id = gg, ix = r->body[k + 16], iy = r->body[k + 17];
        found++;
        k -= 15;
    }
    if (found == 3) icon_at(g, id, ix, iy, x, y);
}

static void draw_commands(Battle *b) {
    SG *g = b->g;
    int x = 4, y = 370;
    header_line(g, x, y, 226, "Command");
    for (int i = 0; i < b->cmdCount; i++) {
        int ry = y + 6 + i * 34;
        if (i == b->cmdCursor) fill_rect(g->fb, g->w, g->h, x + 4, ry, 222, 28, b->phase == PH_COMMAND ? 0xb0e8d898u : 0x60e8d898u);
        const CommandDef *c = b->cmdDefs[i];
        const char *name = c ? c->name : btext(g, 19, "逃げる");
        if (c) icon_at(g, c->icon, c->iconX, c->iconY, x + 6, ry);
        else escape_icon(g, x + 6, ry);
        text_shadow(g, x + 38, ry + 2, 19, name, kWhite);
    }
}

static void draw_list(Battle *b) {
    SG *g = b->g;
    int x = 240, y = 300, w = 300, rows = 6;
    int top = b->listCursor >= rows ? b->listCursor - rows + 1 : 0;
    fill_rect(g->fb, g->w, g->h, x, y - 4, w, rows * 30 + 8, 0xc0202020u);
    if (!b->listCount) {
        text_shadow(g, x + 10, y + 2, 18, b->listKind == LIST_SKILLS ? MT(g, MT_NO_SKILLS) : MT(g, MT_NO_ITEMS), kGray);
        return;
    }
    for (int i = top; i < b->listCount && i < top + rows; i++) {
        int ry = y + (i - top) * 30;
        if (i == b->listCursor) fill_rect(g->fb, g->w, g->h, x + 2, ry, w - 4, 28, 0xb0e8d898u);
        char right[32];
        if (b->listKind == LIST_SKILLS) {
            const SkillDef *s = b->list[i];
            icon_at(g, s->icon, s->iconX, s->iconY, x + 4, ry);
            snprintf(right, sizeof right, "%d", s->mpCost);
            u32 color = s->mpCost > b->f[b->member].mp ? kGray : kWhite;
            text_shadow(g, x + 36, ry + 3, 18, s->name, color);
        } else {
            const ItemDef *it = b->list[i];
            icon_at(g, it->icon, it->iconX, it->iconY, x + 4, ry);
            snprintf(right, sizeof right, "%d", party_item_count(PT(g), it->id));
            text_shadow(g, x + 36, ry + 3, 18, it->name, kWhite);
        }
        text_shadow(g, x + w - 10 - font_width(g->font, 18, right), ry + 3, 18, right, kWhite);
    }
}

static void draw_popups(Battle *b) {
    SG *g = b->g;
    const Cam *cam = &g->field.cam;
    for (int i = 0; i < b->count; i++) {
        const Fighter *f = &b->f[i];
        if (!f->popupMs) continue;
        int sx, sy;
        float h = 1.2f;
        if (f->monster && f->image) h = (float)f->image->h / 48.0f * 0.5f;
        project_point(cam, f->x, stage_ground(g, f->x, f->z) + h, f->z, &sx, &sy);
        char s[32];
        if (f->popupMiss) snprintf(s, sizeof s, "%s", btext(g, 21, "ミス"));
        else snprintf(s, sizeof s, "%d", f->popup);
        int rise = (int)(f->popupMs < 200 ? f->popupMs / 10 : 20);
        int tw = font_width(g->font, 30, s);
        text_outlined(g, sx - tw / 2, sy - rise, 30, s, f->popupHeal ? 0xff80ff80u : kWhite);
    }
}

static void draw_result(Battle *b) {
    SG *g = b->g;
    fill_rect(g->fb, g->w, g->h, 0, 0, g->w, 32, 0xff9b2323u);
    fill_rect(g->fb, g->w, g->h, 0, 32, g->w, 32, 0xffb34b3au);
    text_shadow(g, 8, 4, 22, MT(g, MT_RESULT), kWhite);
    char n[32];
    text_shadow(g, 32, 38, 17, MT(g, MT_EXP), kWhite);
    snprintf(n, sizeof n, "%d", b->expGain);
    text_shadow(g, 250, 38, 17, n, kWhite);
    text_shadow(g, 512, 38, 17, "G", kWhite);
    snprintf(n, sizeof n, "%d", b->moneyGain);
    text_shadow(g, 730, 38, 17, n, kWhite);
    text_shadow(g, 32, 70, 17, MT(g, MT_ITEMS), kWhite);
    for (int i = 0, row = 0; i < b->dropCount; i++) {
        const ItemDef *it = item_def(TB(g), b->drops[i]);
        if (!it) continue;
        int y = 96 + row++ * 30;
        icon_at(g, it->icon, it->iconX, it->iconY, 98, y);
        text_shadow(g, 128, y + 1, 22, it->name, kWhite);
    }
}

static void front_sky(SG *g) {
    static const int ys[7] = {70, 150, 200, 230, 260, 280, 300};
    static const u8 rgb[7][3] = {{83, 118, 204}, {97, 128, 204}, {116, 140, 206}, {161, 186, 228}, {215, 236, 249}, {244, 254, 255}, {253, 255, 255}};
    for (int y = 0; y < g->h; y++) {
        int sy = y * 544 / g->h, k = 0;
        while (k < 6 && sy > ys[k + 1]) k++;
        float t = sy <= ys[0] ? 0 : sy >= ys[6] ? 1 : (float)(sy - ys[k]) / (float)(ys[k + 1] - ys[k]);
        int k2 = sy >= ys[6] ? 6 : k + 1;
        if (sy <= ys[0]) k2 = 0;
        u32 c = 0xff000000u;
        for (int ch = 0; ch < 3; ch++) c |= (u32)(rgb[k][ch] + (rgb[k2][ch] - rgb[k][ch]) * t) << (8 * ch);
        u32 *row = g->fb + (size_t)y * (size_t)g->w;
        const float *z = g->r3.zbuf + (size_t)y * (size_t)g->w;
        for (int x = 0; x < g->w; x++)
            if (z[x] == 0) row[x] = c;
    }
}

static void draw(Battle *b) {
    SG *g = b->g;
    if (b->phase == PH_GAMEOVER) {
        field_draw(g);
        if (!b->gameover) {
            size_t n = 0;
            u8 *data = pack_read(&g->data.pack, "samples/common/res/system/gameover.png", &n);
            if (data) b->gameover = img_decode(data, n);
            free(data);
        }
        int a = b->phaseMs >= 1000 ? 255 : (int)(b->phaseMs * 255 / 1000);
        if (b->gameover) blit(g->fb, g->w, g->h, b->gameover, 0, 0, b->gameover->w, b->gameover->h, 0, 0, g->w, g->h, a);
        return;
    }
    Field *f = &g->field;
    if (f->loaded) {
        field_draw(g);
        if (b->front) front_sky(g);
    } else {
        r3_begin(&g->r3, g->fb, 0xff29967eu);
        cam_update(&f->cam, g->w, g->h);
    }
    const Cam *cam = &f->cam;
    int order[MAX_FIGHTERS];
    for (int i = 0; i < b->count; i++) order[i] = i;
    for (int i = 1; i < b->count; i++) {
        int k = order[i], j = i - 1;
        while (j >= 0 && b->f[order[j]].z > b->f[k].z) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = k;
    }
    for (int i = 0; i < b->count; i++) {
        int k = order[i];
        bool active = (b->phase == PH_COMMAND || b->phase == PH_LIST) && k == b->member;
        if (b->phase == PH_TARGET && b->f[k].monster && k == b->targetCursor && (b->phaseMs / 200) % 2) continue;
        draw_fighter(b, &b->f[k], active);
    }
    (void)cam;
    r3_flush(&g->r3);
    for (int i = 0; i < 8; i++) {
        if (!b->fx[i].on) continue;
        const Fighter *t = &b->f[b->fx[i].target];
        float h = t->monster && t->image ? (float)t->image->h / 48.0f * kMonsterScale * 0.5f : 1.0f;
        int sx, sy;
        project_point(cam, t->x, stage_ground(g, t->x, t->z) + h, t->z, &sx, &sy);
        battle_fx_draw(g, &b->fx[i].def, b->fx[i].ms, sx, sy);
    }
    draw_popups(b);
    if (b->phase == PH_RESULT) {
        draw_result(b);
        return;
    }
    draw_states(b);
    if (b->phase == PH_INTRO) intro_box(b);
    else bar_box(g, shown_bar(b));
    if (b->phase == PH_COMMAND || b->phase == PH_TARGET || b->phase == PH_LIST || b->phase == PH_ALLY) draw_commands(b);
    if (b->phase == PH_LIST || (b->phase == PH_ALLY && b->plan[b->member].kind != ACT_ATTACK)) draw_list(b);
    draw_status(b);
    if (b->totalMs < 300) {
        u32 a = 255 - b->totalMs * 255 / 300;
        fill_rect(g->fb, g->w, g->h, 0, 0, g->w, g->h, a << 24);
    }
}

static void draw_enter(Battle *b) {
    SG *g = b->g;
    field_draw(g);
    float t = (float)b->enterMs / (float)kEnterMs;
    if (t < 0.4f) {
        u32 a = (u32)(180.0f * (1.0f - t / 0.4f));
        fill_rect(g->fb, g->w, g->h, 0, 0, g->w, g->h, a << 24 | 0xffffffu);
    } else {
        u32 a = (u32)(255.0f * ((t - 0.4f) / 0.6f));
        fill_rect(g->fb, g->w, g->h, 0, 0, g->w, g->h, (a > 255 ? 255 : a) << 24);
    }
}

bool battle_frame(SG *g, u32 ms) {
    Battle *b = get(g);
    if (!b || b->phase == PH_DONE) {
        battle_encounter_tick(g);
        b = get(g);
        if (!b || b->phase == PH_DONE) return false;
    }
    if (!b->staged) {
        if (g->field.loaded && b->enterMs < (u32)kEnterMs) {
            draw_enter(b);
            b->enterMs += ms;
            return true;
        }
        setup_stage(b);
        b->phaseMs = b->totalMs = 0;
    }
    update(b, ms);
    if (b->phase == PH_DONE) return false;
    draw(b);
    return true;
}

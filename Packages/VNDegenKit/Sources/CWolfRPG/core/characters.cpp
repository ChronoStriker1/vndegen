// Characters on the map: movement, passability, move routes, the hero's controls, and event triggers.
#include "game.h"

#include <bit>
#include "game_internal.h"

#include <algorithm>
#include <cmath>

namespace wolf {

namespace {

int dirX(int dir) { return dir == 4 || dir == 1 || dir == 7 ? -1 : dir == 6 || dir == 3 || dir == 9 ? 1 : 0; }
int dirY(int dir) { return dir == 8 || dir == 7 || dir == 9 ? -1 : dir == 2 || dir == 1 || dir == 3 ? 1 : 0; }
int opposite(int dir) { return 10 - dir; }

// Route step codes, as Wolf RPG Editor stores them.
enum RouteCode : uint8_t {
    MoveDown = 0x00, MoveLeft, MoveRight, MoveUp, MoveDownLeft, MoveDownRight, MoveUpLeft, MoveUpRight,
    FaceDown = 0x08, FaceLeft, FaceRight, FaceUp, FaceDownLeft, FaceDownRight, FaceUpLeft, FaceUpRight,
    MoveRandom = 0x10, MoveTowardHero, MoveAwayFromHero, StepForward, StepBackward, Jump,
    TurnRight90 = 0x16, TurnLeft90, TurnRightOrLeft, FaceRandom, FaceHero, FaceAwayFromHero,
    AssignVariable = 0x1C, SetSpeed, SetFrequency, SetAnimSpeed,
    IdleAnimOn = 0x20, IdleAnimOff, WalkAnimOn, WalkAnimOff, FixDirOn, FixDirOff, ThroughOn, ThroughOff,
    AboveOn = 0x28, AboveOff,
    SetGraphic = 0x2C, SetOpacity, PlaySound, Wait, HalfStep, FullStep, Pattern1, Pattern2, Pattern3,
    ApproachEvent = 0x35, ApproachPosition, AddVariable, Pattern4, Pattern5, SetHeight,
};

bool routeTakesTime(const Actor &a) {
    if (!a.route) return false;
    for (size_t i = a.routeStep; i < a.route->steps.size(); i++) {
        uint8_t id = a.route->steps[i].id;
        if (id <= MoveUpRight || (id >= MoveRandom && id <= Jump) || id == Wait || id == ApproachEvent ||
            id == ApproachPosition)
            return true;
    }
    return false;
}

constexpr int kStepFrames[7] = {64, 32, 24, 16, 12, 8, 4};
constexpr int kIdleFrames[6] = {0, 15, 30, 60, 120, 240};

void actorBox(const Actor &a, int x, int y, int box[4], bool tiles = false) {
    box[0] = x;
    box[1] = (tiles ? a.tileSquare : a.square) ? y : y + 1;
    box[2] = x + 2;
    box[3] = y + 2;
}

bool overlaps(const int a[4], const int b[4]) { return a[0] < b[2] && b[0] < a[2] && a[1] < b[3] && b[1] < a[3]; }

}  // namespace

int Game::framesPerStep(const Actor &a) const {
    int frames = kStepFrames[std::clamp(a.speed, 0, 6)];
    return halfSteps() ? std::max(1, frames * a.unit / 2) : frames;
}

std::vector<const Character *> Game::characters() const {
    std::vector<const Character *> out;
    if (!partyTransparent_) {
        out.push_back(&hero_.chara);
        for (auto &p : party_)
            if (p.chara.visible && !p.chara.graphic.empty()) out.push_back(&p.chara);
    }
    for (auto &e : events_)
        if (e.actor.chara.visible) out.push_back(&e.actor.chara);
    return out;
}

Actor *Game::actorFor(int target, const Thread &t) {
    if (target == -2) return &hero_;
    if (target == -1) {
        int ev = t.stack.empty() || t.stack.back().map != map_ ? -1 : t.stack.back().mapEvent;
        return ev >= 0 && ev < int(events_.size()) ? &events_[ev].actor : nullptr;
    }
    if (target <= -3 && target >= -7) return &party_[-3 - target];
    for (auto &e : events_)
        if (e.data->id == target) return &e.actor;
    return nullptr;
}

// The passability of a map position. Upper layers decide over lower ones, except tiles that are drawn over
// characters, which leave it to the layers below. Values are the tileset's passability words: the low four bits
// block movement, all four meaning impassable.
uint32_t Game::tileFlags(int x, int y) const {
    if (!map_ || x < 0 || y < 0 || x >= map_->width || y >= map_->height) return 0x0F;
    if (map_->tileset < 0 || map_->tileset >= int(tilesets_.size())) return 0;
    const Tileset &ts = tilesets_[map_->tileset];
    for (int layer = map_->layers - 1; layer >= 0; layer--) {
        int32_t v = map_->tile(layer, x, y);
        // An upper layer's empty tile, base tile 0 or the empty autotile slot, leaves it to the layers below.
        if (layer > 0 && (v == 0 || v / 100000 == 1)) continue;
        uint32_t f = tilePassability(ts, v);
        if (layer > 0 && (f & 0x200)) continue;
        return f;
    }
    return 0;
}

bool Game::quarterBlocked(int qx, int qy) const {
    if (!map_ || qx < 0 || qy < 0 || qx >= map_->width * 2 || qy >= map_->height * 2) return true;
    static const uint32_t bits[4] = {0x08, 0x04, 0x02, 0x01};
    return tileFlags(qx >> 1, qy >> 1) & bits[(qx & 1) + 2 * (qy & 1)];
}

bool Game::boxBlocked(const Actor &a, int x, int y) const {
    int box[4];
    actorBox(a, x, y, box, true);
    for (int qy = box[1]; qy < box[3]; qy++)
        for (int qx = box[0]; qx < box[2]; qx++)
            if (quarterBlocked(qx, qy)) return true;
    return false;
}

bool Game::canMoveHalf(const Actor &a, int dir) const {
    int nx = a.px + dirX(dir) * a.unit, ny = a.py + dirY(dir) * a.unit;
    if (!map_ || nx < 0 || ny < 0 || nx + 2 > map_->width * 2 || ny + 2 > map_->height * 2) return false;
    if (a.through || (walkThroughWalls_ && &a == &hero_)) return true;
    if (boxBlocked(a, a.px, a.py) || boxBlocked(a, nx, ny)) return false;
    int box[4];
    actorBox(a, nx, ny, box);
    auto blocks = [&](const Actor &o) {
        if (&o == &a || !o.chara.visible || o.through || (o.chara.graphic.empty() && o.chara.chip < 0)) return false;
        int other[4];
        actorBox(o, o.px, o.py, other);
        return overlaps(box, other);
    };
    if (blocks(hero_)) return false;
    for (auto &e : events_)
        if (blocks(e.actor)) return false;
    return true;
}

void Game::eventArea(const MapEventState &e, int box[4]) const {
    const MapPage &pg = e.data->pages[e.page];
    actorBox(e.actor, e.actor.px, e.actor.py, box);
    box[0] -= pg.rangeX * 2;
    box[2] += pg.rangeX * 2;
    box[1] -= pg.rangeY * 2;
    box[3] += pg.rangeY * 2;
}

bool Game::eventSolid(const MapEventState &e) const {
    const Actor &a = e.actor;
    if (!a.through && (!a.chara.graphic.empty() || a.chara.chip >= 0)) return true;
    return boxBlocked(a, a.px, a.py);
}

bool Game::touchHalf(int trigger, int dir, int reach, bool onTop, bool startIt) {
    int here[4], ahead[4];
    actorBox(hero_, hero_.px, hero_.py, here);
    actorBox(hero_, hero_.px + dirX(dir) * reach, hero_.py + dirY(dir) * reach, ahead);
    for (size_t i = 0; i < events_.size(); i++) {
        auto &e = events_[i];
        if (e.page < 0) continue;
        const MapPage &pg = e.data->pages[e.page];
        if (pg.trigger != trigger || pg.commands.size() <= 1) continue;
        int area[4];
        eventArea(e, area);
        bool on = onTop && overlaps(here, area) && (trigger == 4 || (here[0] >= area[0] && here[2] <= area[2]));
        bool front = reach && overlaps(ahead, area) && eventSolid(e);
        if (!on && !front) continue;
        if (startIt) {
            Thread t;
            startPage(int(i), t);
            threads_.push_back(std::move(t));
        }
        return true;
    }
    return false;
}

void Game::placeEventsOnEntry() {
    for (auto &e : events_) {
        e.actor.unit = halfSteps() ? 1 : 2;
        if (e.page < 0 || !halfSteps()) continue;
        uint8_t flags = e.data->pages[e.page].flags;
        e.actor.placeHalf(e.actor.px - (flags & 0x80 ? 1 : 0), e.actor.py - (flags & 0x40 ? 1 : 0));
    }
}

bool Game::canMove(const Actor &a, int dir) const {
    if (halfSteps()) return canMoveHalf(a, dir);
    int nx = a.tx() + dirX(dir), ny = a.ty() + dirY(dir);
    if (!map_ || nx < 0 || ny < 0 || nx >= map_->width || ny >= map_->height) return false;
    if (a.through || (walkThroughWalls_ && &a == &hero_)) return true;
    if ((tileFlags(nx, ny) & 0x0F) == 0x0F) return false;
    // Other characters with a graphic block the way.
    auto blocks = [&](const Actor &o) {
        return &o != &a && o.chara.visible && !o.through && (!o.chara.graphic.empty() || o.chara.chip >= 0) &&
               o.tx() == nx && o.ty() == ny;
    };
    if (blocks(hero_)) return false;
    for (auto &e : events_)
        if (blocks(e.actor)) return false;
    return true;
}

void Game::turn(Actor &a, int dir) {
    if (!a.fixedDirection) a.chara.direction = dir;
}

bool Game::step(Actor &a, int dir) {
    turn(a, dir);
    if (!canMove(a, dir)) return false;
    int unit = halfSteps() ? a.unit : 2;
    a.fromX = a.px / 2.0;
    a.fromY = a.py / 2.0;
    a.px += dirX(dir) * unit;
    a.py += dirY(dir) * unit;
    a.moveFrame = 0;
    a.moveFrames = framesPerStep(a);
    return true;
}

void Game::runRoute(Actor &a, bool isHero) {
    if (!a.route || a.moving() || a.routeWait > 0) {
        if (a.routeWait > 0) a.routeWait--;
        return;
    }
    const MoveRoute &r = *a.route;
    bool repeat = r.flags & 0x01, skip = r.flags & 0x02;
    if (a.routeStep >= r.steps.size()) {
        if (repeat && !r.steps.empty()) a.routeStep = 0;
        else {
            a.route = nullptr;
            return;
        }
    }
    const RouteStep &s = r.steps[a.routeStep];
    auto arg = [&](size_t i) { return i < s.args.size() ? s.args[i] : 0; };
    bool done = true;
    auto towardHero = [&](bool away) {
        int dx = hero_.px - a.px, dy = hero_.py - a.py;
        int dir = std::abs(dx) > std::abs(dy) ? (dx > 0 ? 6 : 4) : (dy > 0 ? 2 : 8);
        if (dx == 0 && dy == 0) return 0;
        return away ? opposite(dir) : dir;
    };
    switch (s.id) {
        case MoveDown: case MoveLeft: case MoveRight: case MoveUp:
        case MoveDownLeft: case MoveDownRight: case MoveUpLeft: case MoveUpRight: {
            static const int dirs[8] = {2, 4, 6, 8, 1, 3, 7, 9};
            done = step(a, dirs[s.id]) || skip;
            break;
        }
        case FaceDown: case FaceLeft: case FaceRight: case FaceUp:
        case FaceDownLeft: case FaceDownRight: case FaceUpLeft: case FaceUpRight: {
            static const int dirs[8] = {2, 4, 6, 8, 1, 3, 7, 9};
            turn(a, dirs[s.id - FaceDown]);
            break;
        }
        case MoveRandom: {
            static const int dirs[4] = {2, 4, 6, 8};
            step(a, dirs[std::uniform_int_distribution<int>(0, 3)(rng_)]);
            break;
        }
        case MoveTowardHero:
        case MoveAwayFromHero:
            if (int d = towardHero(s.id == MoveAwayFromHero)) done = step(a, d) || skip;
            break;
        case StepForward: done = step(a, a.chara.direction) || skip; break;
        case StepBackward: {
            bool fixed = a.fixedDirection;
            a.fixedDirection = true;
            done = step(a, opposite(a.chara.direction)) || skip;
            a.fixedDirection = fixed;
            break;
        }
        case Jump: {
            a.fromX = a.px / 2.0;
            a.fromY = a.py / 2.0;
            a.px += arg(0) * 2;
            a.py += arg(1) * 2;
            a.moveFrame = 0;
            a.moveFrames = framesPerStep(a) * 2;
            a.jumpHeight = gameDat_.tileSize();
            break;
        }
        case TurnRight90: {
            static const int next[10] = {0, 4, 1, 2, 7, 0, 3, 8, 9, 6};
            turn(a, next[validDirection(a.chara.direction)]);
            break;
        }
        case TurnLeft90: {
            static const int next[10] = {0, 2, 3, 6, 1, 0, 9, 4, 7, 8};
            turn(a, next[validDirection(a.chara.direction)]);
            break;
        }
        case TurnRightOrLeft: turn(a, std::uniform_int_distribution<int>(0, 1)(rng_) ? 4 : 6); break;
        case FaceRandom: {
            static const int dirs[4] = {2, 4, 6, 8};
            turn(a, dirs[std::uniform_int_distribution<int>(0, 3)(rng_)]);
            break;
        }
        case FaceHero:
        case FaceAwayFromHero:
            if (int d = towardHero(s.id == FaceAwayFromHero)) turn(a, d);
            break;
        case SetSpeed: a.speed = arg(0); break;
        case SetFrequency: a.frequency = arg(0); break;
        case SetAnimSpeed: a.animSpeed = arg(0); break;
        case IdleAnimOn: a.idleAnimation = true; break;
        case IdleAnimOff: a.idleAnimation = false; break;
        case WalkAnimOn: a.walkAnimation = true; break;
        case WalkAnimOff: a.walkAnimation = false; break;
        case FixDirOn: a.fixedDirection = true; break;
        case FixDirOff: a.fixedDirection = false; break;
        case ThroughOn: a.through = true; break;
        case ThroughOff: a.through = false; break;
        case AboveOn: a.chara.above = true; break;
        case AboveOff: a.chara.above = false; break;
        case SetOpacity: a.chara.opacity = arg(0); break;
        case SetGraphic:  // a character image from the system database's list
            a.chara.graphic = dbString(sdb_, 8, arg(0), 0);
            a.chara.chip = -1;
            break;
        case PlaySound:
            playSoundEffect(arg(0));
            break;
        case Wait: a.routeWait = arg(0); break;
        case HalfStep:
        case FullStep:
            if (halfSteps()) a.unit = s.id == HalfStep ? 1 : 2;
            else if (s.id == HalfStep && unsupported_[10000 + s.id]++ == 0)
                log(format("move route step %d is not supported yet", s.id));
            break;
        case Pattern1: a.chara.pattern = 0; break;
        case Pattern2: a.chara.pattern = 1; break;
        case Pattern3: a.chara.pattern = 2; break;
        case Pattern4: a.chara.pattern = 3; break;
        case Pattern5: a.chara.pattern = 4; break;
        case SetHeight: a.chara.lift = arg(0); break;
        default:
            if (unsupported_[10000 + s.id]++ == 0) log(format("move route step %d is not supported yet", s.id));
            break;
    }
    (void)isHero;
    if (done) a.routeStep++;
}

void Game::updateActor(Actor &a, bool isHero) {
    if (a.moving()) {
        a.moveFrame++;
        double f = double(a.moveFrame) / a.moveFrames;
        a.chara.x = a.fromX + (a.px / 2.0 - a.fromX) * f;
        a.chara.y = a.fromY + (a.py / 2.0 - a.fromY) * f;
        a.chara.lift = a.jumpHeight ? int(std::sin(f * M_PI) * a.jumpHeight) : a.chara.lift;
        if (!a.moving()) {
            a.jumpHeight = 0;
            if (isHero) checkTouch(a.tx(), a.ty(), true);
        }
    } else {
        a.chara.x = a.px / 2.0;
        a.chara.y = a.py / 2.0;
    }

    // Walking cycles the frames 1 0 1 2 for three-frame sets; standing returns to the middle frame.
    a.chara.moving = a.moving();
    bool animate = (a.moving() && a.walkAnimation) || a.idleAnimation;
    if (animate) {
        if (++a.animTimer >= std::max(2, 18 - a.animSpeed * 3)) {
            a.animTimer = 0;
            static const int cycle[4] = {1, 0, 1, 2};
            a.animStep = (a.animStep + 1) % 4;
            a.chara.pattern = cycle[a.animStep];
        }
    } else if (a.walkAnimation && !a.moving() && a.chara.pattern != 1 && !a.route) {
        a.chara.pattern = 1;
        a.animStep = 0;
    }

    runRoute(a, isHero);
}

bool Game::eventAt(int x, int y, int trigger, bool startIt) {
    for (size_t i = 0; i < events_.size(); i++) {
        auto &e = events_[i];
        if (e.page < 0 || e.actor.tx() != x || e.actor.ty() != y) continue;
        const MapPage &pg = e.data->pages[e.page];
        if (pg.trigger != trigger || pg.commands.size() <= 1) continue;
        if (startIt) {
            Thread t;
            startPage(int(i), t);
            threads_.push_back(std::move(t));
        }
        return true;
    }
    return false;
}

void Game::checkTouch(int x, int y, bool heroMoved) {
    if (!heroMoved) return;
    if (halfSteps()) touchHalf(3, 0, 0, true, true);
    else eventAt(x, y, 3, true);
}

// Events that walk on their own: random walks and walking toward the hero, at the page's frequency.
void Game::autonomousMove(MapEventState &e) {
    Actor &a = e.actor;
    if (e.page < 0 || a.route || a.moving()) return;
    const MapPage &pg = e.data->pages[e.page];
    if (pg.moveType != 2 && pg.moveType != 3) return;
    if (++a.idleTimer < kIdleFrames[std::clamp(a.frequency, 0, 5)]) return;
    a.idleTimer = 0;
    static const int dirs[4] = {2, 4, 6, 8};
    int dir = dirs[std::uniform_int_distribution<int>(0, 3)(rng_)];
    if (pg.moveType == 3) {
        int dx = hero_.px - a.px, dy = hero_.py - a.py;
        if (dx || dy) dir = std::abs(dx) > std::abs(dy) ? (dx > 0 ? 6 : 4) : (dy > 0 ? 2 : 8);
    }
    auto reachesHero = [&] {
        if (!halfSteps()) return hero_.tx() == a.tx() + dirX(dir) && hero_.ty() == a.ty() + dirY(dir);
        int box[4], hero[4];
        actorBox(a, a.px + dirX(dir) * a.unit, a.py + dirY(dir) * a.unit, box);
        actorBox(hero_, hero_.px, hero_.py, hero);
        return overlaps(box, hero);
    };
    if (!step(a, dir) && pg.trigger == 4 && reachesHero()) {
        // An event that starts on touching the hero starts when it walks into the hero.
        Thread t;
        for (size_t i = 0; i < events_.size(); i++)
            if (&events_[i] == &e) startPage(int(i), t);
        threads_.push_back(std::move(t));
    }
}

void Game::updateCharacters() {
    heroControl();
    updateActor(hero_, true);
    for (auto &p : party_) updateActor(p, false);
    for (auto &e : events_) {
        if (e.page < 0) {
            e.actor.route = nullptr;
            continue;
        }
        autonomousMove(e);
        updateActor(e.actor, false);
    }
}

bool Game::anyRouteRunning() const {
    if (hero_.route && hero_.routeFromCommand) return true;
    for (auto &e : events_)
        if (e.actor.route && e.actor.routeFromCommand) return true;
    return false;
}

void Game::heroControl() {
    // Commands 230 and 231 let the hero walk while an event runs; the event goes on meanwhile.
    bool eventRunning = !threads_.empty();
    if ((eventRunning && !moveDuringEvents_) || message_.active || choice_.active || hero_.moving() ||
        routeTakesTime(hero_))
        return;
    // Keys command 126 stopped from moving the hero or starting events still reach key input commands.
    auto moves = [&](Key key) { return (input_.held & key) && basicControl_[std::countr_zero(uint32_t(key))] == 0; };
    int dir = 0;
    if (moves(KeyDown)) dir = 2;
    else if (moves(KeyLeft)) dir = 4;
    else if (moves(KeyRight)) dir = 6;
    else if (moves(KeyUp)) dir = 8;
    if ((input_.pressed & KeyConfirm) && !eventRunning && basicControl_[std::countr_zero(uint32_t(KeyConfirm))] == 0) {
        // The event on the hero's tile, then the one in front, then one across a counter.
        int facing = hero_.chara.direction;
        if (halfSteps()) {
            if (touchHalf(0, facing, 1, true, true)) return;
            int cx = (hero_.px + 1 + dirX(facing) * 2) >> 1, cy = (hero_.py + 1 + dirY(facing) * 2) >> 1;
            if ((tileFlags(cx, cy) & 0x80) && touchHalf(0, facing, 3, false, true)) return;
        } else {
            int fx = hero_.tx() + dirX(facing), fy = hero_.ty() + dirY(facing);
            if (eventAt(hero_.tx(), hero_.ty(), 0, true) || eventAt(fx, fy, 0, true)) return;
            if ((tileFlags(fx, fy) & 0x80) && eventAt(fx + dirX(facing), fy + dirY(facing), 0, true)) return;
        }
    }
    if (dir) {
        if (!step(hero_, dir)) {
            // Walking into an event that starts on touch starts it.
            if (halfSteps()) touchHalf(3, dir, 1, false, true) || touchHalf(4, dir, 1, false, true);
            else eventAt(hero_.tx() + dirX(dir), hero_.ty() + dirY(dir), 3, true);
        }
    }
}

}  // namespace wolf

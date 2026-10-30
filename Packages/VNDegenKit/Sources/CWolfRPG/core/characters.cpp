// Characters on the map: movement, passability, move routes, the hero's controls, and event triggers.
#include "game.h"
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

constexpr int kStepFrames[7] = {64, 32, 24, 16, 12, 8, 4};
constexpr int kIdleFrames[6] = {0, 15, 30, 60, 120, 240};

}  // namespace

int Game::framesPerStep(const Actor &a) const { return kStepFrames[std::clamp(a.speed, 0, 6)]; }

std::vector<const Character *> Game::characters() const {
    std::vector<const Character *> out{&hero_.chara};
    for (auto &p : party_)
        if (p.chara.visible && !p.chara.graphic.empty()) out.push_back(&p.chara);
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

bool Game::canMove(const Actor &a, int dir) const {
    int nx = a.tx + dirX(dir), ny = a.ty + dirY(dir);
    if (!map_ || nx < 0 || ny < 0 || nx >= map_->width || ny >= map_->height) return false;
    if (a.through) return true;
    if ((tileFlags(nx, ny) & 0x0F) == 0x0F) return false;
    // Other characters with a graphic block the way.
    auto blocks = [&](const Actor &o) {
        return &o != &a && o.chara.visible && !o.through && (!o.chara.graphic.empty() || o.chara.chip >= 0) &&
               o.tx == nx && o.ty == ny;
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
    a.fromX = a.tx;
    a.fromY = a.ty;
    a.tx += dirX(dir);
    a.ty += dirY(dir);
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
        int dx = hero_.tx - a.tx, dy = hero_.ty - a.ty;
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
            a.fromX = a.tx;
            a.fromY = a.ty;
            a.tx += arg(0);
            a.ty += arg(1);
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
        case FullStep: break;  // characters already move a whole tile a step; half steps aren't supported
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
        a.chara.x = a.fromX + (a.tx - a.fromX) * f;
        a.chara.y = a.fromY + (a.ty - a.fromY) * f;
        a.chara.lift = a.jumpHeight ? int(std::sin(f * M_PI) * a.jumpHeight) : a.chara.lift;
        if (!a.moving()) {
            a.jumpHeight = 0;
            if (isHero) checkTouch(a.tx, a.ty, true);
        }
    } else {
        a.chara.x = a.tx;
        a.chara.y = a.ty;
    }

    // Walking cycles the frames 1 0 1 2 for three-frame sets; standing returns to the middle frame.
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
        if (e.page < 0 || e.actor.tx != x || e.actor.ty != y) continue;
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
    if (heroMoved) eventAt(x, y, 3, true);
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
        int dx = hero_.tx - a.tx, dy = hero_.ty - a.ty;
        if (dx || dy) dir = std::abs(dx) > std::abs(dy) ? (dx > 0 ? 6 : 4) : (dy > 0 ? 2 : 8);
    }
    if (!step(a, dir) && pg.trigger == 4 && hero_.tx == a.tx + dirX(dir) && hero_.ty == a.ty + dirY(dir)) {
        // An event that starts on touching the hero starts when it walks into the hero.
        Thread t;
        startPage(int(&e - events_.data()), t);
        threads_.push_back(std::move(t));
    }
}

void Game::updateCharacters() {
    heroControl();
    updateActor(hero_, true);
    for (auto &p : party_) updateActor(p, false);
    for (auto &e : events_) {
        if (e.page < 0) continue;
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
    if ((eventRunning && !moveDuringEvents_) || message_.active || choice_.active || hero_.moving() || hero_.route) return;
    int dir = 0;
    if (input_.held & KeyDown) dir = 2;
    else if (input_.held & KeyLeft) dir = 4;
    else if (input_.held & KeyRight) dir = 6;
    else if (input_.held & KeyUp) dir = 8;
    if ((input_.pressed & KeyConfirm) && !eventRunning) {
        // The event on the hero's tile, then the one in front, then one across a counter.
        int fx = hero_.tx + dirX(hero_.chara.direction), fy = hero_.ty + dirY(hero_.chara.direction);
        if (eventAt(hero_.tx, hero_.ty, 0, true) || eventAt(fx, fy, 0, true)) return;
        if (tileFlags(fx, fy) & 0x80) {
            if (eventAt(fx + dirX(hero_.chara.direction), fy + dirY(hero_.chara.direction), 0, true)) return;
        }
    }
    if (dir) {
        if (!step(hero_, dir)) {
            // Walking into an event that starts on touch starts it.
            eventAt(hero_.tx + dirX(dir), hero_.ty + dirY(dir), 3, true);
        }
    }
}

}  // namespace wolf

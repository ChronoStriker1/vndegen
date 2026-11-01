// Event commands with larger parameter sets: the database, extended variable reads, key input, party graphics,
// effects, map chips, sound and saves.
#include "game.h"
#include "game_internal.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace wolf {

const CommonEvent *Game::commonById(int id) const {
    if (id >= 0 && id < int(commons_.size()) && commons_[id].id == id) return &commons_[id];
    for (auto &e : commons_)
        if (e.id == id) return &e;
    return nullptr;
}

// Database kinds in the database command: 0 the changeable database, 1 the system database, 2 the user database.
Database *Game::databaseKind(int kind) {
    switch (kind) {
        case 0: return &cdb_;
        case 1: return &sdb_;
        default: return &udb_;
    }
}

// Reads or writes one database cell, or one of the database's names or sizes.
void Game::databaseCommand(Thread &t, const Command &c) {
    uint32_t flags = uint32_t(c.arg(3));
    int op = flags & 0xF0;
    int leftCode = flags & 0x0F;
    bool read = ((flags >> 8) & 0xF0) == 0x10;
    Database &db = *databaseKind((flags >> 8) & 0x0F);
    int byName = (flags >> 16) & 0xFF;

    auto findName = [](const std::vector<std::string> &names, const std::string &n) {
        for (size_t i = 0; i < names.size(); i++)
            if (names[i] == n) return int(i);
        return -1;
    };
    int type = byName & 0x01 ? -1 : value(c.arg(0), t);
    if (byName & 0x01) {
        for (size_t i = 0; i < db.types.size(); i++)
            if (db.types[i].name == c.str(1)) type = int(i);
    }
    int row = value(c.arg(1), t), field = value(c.arg(2), t);
    DatabaseType *dt = type >= 0 && type < int(db.types.size()) ? &db.types[type] : nullptr;
    // The command's strings are the text to write, then the type, row and field names.
    if (dt && (byName & 0x02)) row = findName(dt->dataNames, c.str(2));
    if (dt && (byName & 0x04)) field = findName(dt->fieldNames, c.str(3));
    int32_t target = c.arg(4);

    // Queries: names, ids and sizes.
    if (c.arg(1) == -3 && c.arg(2) == -3) {  // type name or id
        if (byName & 0x01) writeVar(target, type, t);
        else writeStr(target, dt ? dt->name : "", t);
        return;
    }
    if (c.arg(2) == -3) {  // row name or id
        if (byName & 0x02) writeVar(target, row, t);
        else writeStr(target, dt && row >= 0 && row < int(dt->dataNames.size()) ? dt->dataNames[row] : "", t);
        return;
    }
    if (c.arg(1) == -3) {  // field name or id
        if (byName & 0x04) writeVar(target, field, t);
        else writeStr(target, dt && field >= 0 && field < int(dt->fieldNames.size()) ? dt->fieldNames[field] : "", t);
        return;
    }
    if (c.arg(1) == -1) {  // number of rows
        writeVar(target, dt ? int(dt->rows.size()) : 0, t);
        return;
    }
    if (c.arg(2) == -1) {  // number of fields
        writeVar(target, dt ? int(dt->fieldKinds.size()) : 0, t);
        return;
    }
    if (!dt) return;
    // Row -2 resets every row of the type to the defaults, and field -2 every field of one row (WodiLib's
    // DBManagementClearData and DBManagementClearField).
    if (c.arg(1) == -2 || c.arg(2) == -2) {
        auto reset = [&](DatabaseType::Row &r, int f) {
            if (dt->isString(f)) {
                if (dt->slot(f) < r.strings.size()) r.strings[dt->slot(f)].clear();
            } else if (dt->slot(f) < r.numbers.size()) {
                r.numbers[dt->slot(f)] = f < int(dt->defaults.size()) ? dt->defaults[f] : 0;
            }
        };
        auto resetRow = [&](DatabaseType::Row &r) {
            for (int f = 0; f < int(dt->fieldKinds.size()); f++) reset(r, f);
        };
        if (c.arg(1) == -2) {
            for (auto &r : dt->rows) resetRow(r);
        } else if (row >= 0 && row < int(dt->rows.size())) {
            resetRow(dt->rows[row]);
        }
        return;
    }
    if (row < 0 || field < 0 || field >= int(dt->fieldKinds.size())) return;
    if (row >= int(dt->rows.size())) {
        if (read) {
            writeVar(target, 0, t);
            return;
        }
        // Writing past the end adds rows, as the editor's database grows.
        size_t numbers = 0, strings = 0;
        for (size_t f = 0; f < dt->fieldKinds.size(); f++) (dt->isString(f) ? strings : numbers)++;
        while (int(dt->rows.size()) <= row) {
            DatabaseType::Row r;
            r.numbers.assign(numbers, 0);
            r.strings.assign(strings, "");
            dt->rows.push_back(std::move(r));
        }
    }
    DatabaseType::Row &r = dt->rows[row];
    size_t slot = dt->slot(field);

    if (dt->isString(field)) {
        if (slot >= r.strings.size()) return;
        if (read) {
            // Operator 0x10 appends the field to the string variable.
            writeStr(target, op == 0x10 ? readStr(target, t) + r.strings[slot] : r.strings[slot], t);
        } else {
            // Low code 2 assigns, with 0x10 appending; the text is given or comes from a string variable.
            std::string rhs = (flags & 0x0F) == 0x02 ? expandText(c.str(0), &t) : readStr(target, t);
            if (op == 0x10) r.strings[slot] += rhs;
            else r.strings[slot] = rhs;
        }
        return;
    }
    if (slot >= r.numbers.size()) return;
    int32_t &cell = r.numbers[slot];
    if (read) {
        int32_t cur = op ? readVar(target, t) : 0;
        int32_t v = cell;
        switch (op) {
            case 0x00: cur = v; break;
            case 0x10: cur += v; break;
            case 0x20: cur -= v; break;
            case 0x30: cur *= v; break;
            case 0x40: cur = v ? cur / v : cur; break;
            case 0x50: cur = v ? cur % v : cur; break;
            case 0x60: cur = std::max(cur, v); break;
            case 0x70: cur = std::min(cur, v); break;
        }
        writeVar(target, cur, t);
        return;
    }
    int32_t v = leftCode == 1 ? target : value(target, t);
    switch (op) {
        case 0x00: cell = v; break;
        case 0x10: cell += v; break;
        case 0x20: cell -= v; break;
        case 0x30: cell *= v; break;
        case 0x40: cell = v ? cell / v : cell; break;
        case 0x50: cell = v ? cell % v : cell; break;
        case 0x60: cell = std::max(cell, v); break;
        case 0x70: cell = std::min(cell, v); break;
    }
}

// Reads a property of a character, a map position, a picture, or the game into a variable.
void Game::setVariablePlus(Thread &t, const Command &c) {
    int32_t target = c.arg(0);
    uint32_t flags = uint32_t(c.arg(1));
    if (flags & 0x10) target = readVar(target, t);
    int exec = (flags >> 8) & 0xF0;
    int assign = (flags >> 8) & 0x0F;
    int info = (flags >> 16) & 0xFF;
    int32_t v = 0;
    int tile = gameDat_.tileSize();
    if (exec == 0x10) {  // character
        const Actor *a = actorFor(value(c.arg(2), t), t);
        if (!a) return;
        switch (info) {
            case 0: v = a->tx; break;
            case 1: v = a->ty; break;
            case 2: v = a->tx * 2; break;
            case 3: v = a->ty * 2; break;
            case 4: v = a->chara.lift; break;
            case 5: v = a->chara.direction; break;
            case 6: v = int(std::lround((a->chara.x + 0.5) * tile - cameraX_)); break;
            case 7: v = int(std::lround((a->chara.y + 1) * tile - cameraY_)); break;
            case 8: v = a->chara.shadow; break;
            case 9: {
                uint32_t f = tileFlags(a->tx, a->ty);
                (void)f;
                v = 0;
                break;
            }
            case 10: {
                v = -1;
                for (auto &e : events_)
                    if (&e.actor == a) v = e.data->id;
                break;
            }
            case 11: {
                double sx = (a->chara.x + 0.5) * tile - cameraX_, sy = (a->chara.y + 0.5) * tile - cameraY_;
                v = sx >= 0 && sy >= 0 && sx < screenWidth() && sy < screenHeight();
                break;
            }
            case 12: {
                v = 0;
                for (auto &e : events_)
                    if (&e.actor == a) v = e.page + 1;
                break;
            }
            case 16: v = a->chara.pattern; break;
            case 17: v = a->moving(); break;
            default: v = 0; break;
        }
    } else if (exec == 0x20) {  // map position
        int x = value(c.arg(2), t), y = value(c.arg(3), t);
        if (info & 0x80) {  // precise coordinates
            x /= 2;
            y /= 2;
        }
        switch (info & 0x0F) {
            case 1: {
                v = -1;
                for (auto &e : events_)
                    if (e.page >= 0 && e.actor.tx == x && e.actor.ty == y) v = e.data->id;
                break;
            }
            case 2: v = (tileFlags(x, y) & 0x0F) == 0x0F ? 0 : 1; break;
            case 3: {
                v = (tileFlags(x, y) & 0x0F) == 0x0F ? 0 : 1;
                for (auto &e : events_)
                    if (e.page >= 0 && e.actor.tx == x && e.actor.ty == y && !e.actor.through) v = 0;
                break;
            }
            case 4: case 5: case 6: case 7: {
                if (!map_ || x < 0 || y < 0 || x >= map_->width || y >= map_->height) break;
                int layer = (info & 0x0F) - 5;
                if (layer < 0) {
                    for (layer = map_->layers - 1; layer > 0 && map_->tile(layer, x, y) == 0; layer--) {}
                }
                v = map_->tile(layer, x, y);
                break;
            }
            default: {  // tile tags
                if (!map_ || x < 0 || y < 0 || x >= map_->width || y >= map_->height) break;
                const auto &tags = tilesets_[map_->tileset].tags;
                int layer = (info & 0x0F) == 0 ? -1 : (info & 0x0F) - 8;
                if (layer < 0) {
                    for (layer = map_->layers - 1; layer > 0 && map_->tile(layer, x, y) == 0; layer--) {}
                }
                int32_t tv = map_->tile(layer, x, y);
                size_t index = tv >= 100000 ? size_t(tv / 100000) : size_t(tv) + 16;
                v = index < tags.size() ? tags[index] : 0;
                break;
            }
        }
    } else if (exec == 0x40) {  // picture
        auto it = pictures_.find(value(c.arg(2), t));
        if (it == pictures_.end()) {
            v = info == 9 ? 0 : 0;
        } else {
            const Picture &p = it->second;
            switch (info) {
                case 0: v = int(p.x.value()); break;
                case 1: v = int(p.y.value()); break;
                case 2: v = p.imageW / std::max(1, p.divX); break;
                case 3: v = p.imageH / std::max(1, p.divY); break;
                case 4: v = p.pattern; break;
                case 5: v = int(p.opacity.value()); break;
                case 6: v = int(p.angle.value()); break;
                case 7: case 11: v = int(p.zoomX.value()); break;
                case 12: v = int(p.zoomY.value()); break;
                case 9: v = 1; break;
                case 10: v = 1; break;
                default: v = 0; break;
            }
        }
    } else if (exec == 0x30) {  // other
        switch (info) {
            case 0: v = mapId_; break;
            case 14: {
                const auto &call = t.stack.back();
                v = call.mapEvent < 0 || call.map != map_ ? -1 : events_[call.mapEvent].data->id;
                break;
            }
            case 15: v = map_ ? map_->width : 0; break;
            case 16: v = map_ ? map_->height : 0; break;
            case 17: v = t.stack.empty() ? -1 : t.stack.back().commonEvent; break;
            default: v = 0; break;
        }
    } else {
        unsupported(c);
        return;
    }
    int32_t cur = assign ? readVar(target, t) : 0;
    switch (assign) {
        case 0: cur = v; break;
        case 1: cur += v; break;
        case 2: cur -= v; break;
        case 3: cur *= v; break;
        case 4: cur = v ? cur / v : cur; break;
        case 5: cur = v ? cur % v : cur; break;
        case 6: cur = std::max(cur, v); break;
        case 7: cur = std::min(cur, v); break;
        case 8: cur = std::abs(v); break;
    }
    writeVar(target, cur, t);
}

uint32_t Game::keysNow() const { return input_.held | autoKeys_; }

// Key input. Basic keys give 2 4 6 8 for the directions (1 3 7 9 for diagonals), 10 confirm, 11 cancel, 12 sub,
// and 0 for none. With "wait for input" the event waits until one of the accepted keys is pressed.
bool Game::keyInput(Thread &t, const Command &c) {
    int32_t target = c.arg(0);
    uint32_t flags = uint32_t(c.arg(1));
    int kind = (flags >> 8) & 0x0F;
    bool wait = flags & 0x80;
    uint32_t keys = wait ? (input_.pressed | autoKeys_) : keysNow();
    int32_t v = 0;
    if (kind == 0 && (flags & 0x40000)) {
        // Wolf RPG Editor 3: how many frames one key has been held, 0 when it is up. The low byte names the key as
        // the direction and button fields do: 3 up, 4 down, 5 left, 6 right, 0x10 confirm, 0x20 cancel, 0x40 sub.
        // Games use it for their own key repeat in menus (Clumsy Witch Seris).
        static const std::pair<int, Key> named[] = {{3, KeyUp}, {4, KeyDown}, {5, KeyLeft}, {6, KeyRight},
                                                    {0x10, KeyConfirm}, {0x20, KeyCancel}, {0x40, KeySub}};
        for (auto [code, key] : named)
            if (int(flags & 0xFF) == code) v = heldFrames_[std::countr_zero(uint32_t(key))];
    } else if (kind == 0) {
        int dirs = flags & 0x0F;
        bool ok = flags & 0x10, cancel = flags & 0x20, sub = flags & 0x40;
        if (ok && (keys & KeyConfirm)) v = 10;
        else if (cancel && (keys & KeyCancel)) v = 11;
        else if (sub && (keys & KeySub)) v = 12;
        else if (dirs) {
            bool d = keys & KeyDown, l = keys & KeyLeft, r = keys & KeyRight, u = keys & KeyUp;
            if (dirs == 2) {
                if (d && l) v = 1;
                else if (d && r) v = 3;
                else if (u && l) v = 7;
                else if (u && r) v = 9;
            }
            if (!v) {
                if (d && (dirs <= 2 || dirs == 4 || dirs == 7)) v = 2;
                else if (l && (dirs <= 2 || dirs == 5 || dirs == 8)) v = 4;
                else if (r && (dirs <= 2 || dirs == 6 || dirs == 8)) v = 6;
                else if (u && (dirs <= 2 || dirs == 3 || dirs == 7)) v = 8;
            }
        }
    } else if (kind == 1 || kind == 2) {
        // Keyboard and pad codes. VNDegen maps controllers and touch onto the Wolf keys, so these report the key a
        // game most often asks for: the keyboard keys behind confirm, cancel, sub and the arrows.
        bool specific = (flags >> 8) & 0xF0;
        int code = value(c.arg(2), t);
        auto held = [&](int k) {
            switch (k) {
                case 13: case 32: case 90: return bool(keys & KeyConfirm);
                case 27: case 88: return bool(keys & KeyCancel);
                case 16: return bool(keys & KeySub);
                case 37: return bool(keys & KeyLeft);
                case 38: return bool(keys & KeyUp);
                case 39: return bool(keys & KeyRight);
                case 40: return bool(keys & KeyDown);
            }
            return false;
        };
        if (specific) v = held(code) ? 1 : 0;
        else {
            for (int k : {90, 88, 16, 37, 38, 39, 40})
                if (held(k)) {
                    v = k;
                    break;
                }
        }
    } else {
        v = 0;  // mouse
    }
    if (wait && v == 0) {
        t.stack.back().pc--;
        return false;
    }
    // A press a waiting input accepts is used up, so the next wait in the same frame waits for a press of its own.
    if (wait) input_.pressed &= ~keys;
    writeVar(target, v, t);
    return true;
}

// Party graphics: the hero is member 0 and up to five members follow. The names live in system strings 10 to 15.
void Game::partyCommand(Thread &t, const Command &c) {
    uint32_t flags = uint32_t(c.arg(0));
    int exec = flags & 0x0F;
    std::vector<std::string> members;
    for (size_t i = 10; i <= 15; i++) {
        std::string n = i < systemStrings_.size() ? systemStrings_[i] : "";
        if (!n.empty() || i == 10) members.push_back(n);
    }
    int index = value(c.arg(1), t);
    std::string graphic = (flags >> 8) & 0x10 ? readStr(c.arg(2), t) : c.str(0);
    switch (exec) {
        case 0:  // remove a member
            if (index >= 0 && index < int(members.size())) members.erase(members.begin() + index);
            break;
        case 1:  // insert a member
            index = std::clamp(index, 0, int(members.size()));
            members.insert(members.begin() + index, graphic);
            break;
        case 2:  // replace a member's graphic
            if (index >= 0 && index < int(members.size())) members[index] = graphic;
            else members.push_back(graphic);
            break;
        case 3:  // remove every member with this graphic
            std::erase(members, graphic);
            break;
        default:
            if (((flags & 0xF0) >> 4) == 1) members.assign(1, "");  // erase every character
            break;
    }
    members.resize(6);
    if (systemStrings_.size() < 16) systemStrings_.resize(16);
    for (size_t i = 0; i < 6; i++) systemStrings_[10 + i] = members[i];
    syncPartyGraphics();
}

void Game::effectCommand(Thread &t, const Command &c) {
    uint32_t flags = uint32_t(c.arg(0));
    int targetKind = flags & 0x0F;
    int type = flags & 0xF0;
    int frames = value(c.arg(1), t);
    int a1 = value(c.arg(4), t), a2 = value(c.arg(5), t), a3 = value(c.arg(6), t);
    if (targetKind == 0) {  // pictures
        int first = value(c.arg(2), t);
        int last = c.arg(3) ? value(c.arg(3), t) : first;
        for (int n = first; n <= last; n++) {
            auto it = pictures_.find(n);
            if (it == pictures_.end()) continue;
            Picture &p = it->second;
            switch (type) {
                case 0x00:  // flash: tint toward a colour and back
                    p.r.set(a1, frames / 2);
                    p.g.set(a2, frames / 2);
                    p.b.set(a3, frames / 2);
                    break;
                case 0x10:  // colour correction, as offsets from the normal 100
                    p.r.set(100 + a1, frames);
                    p.g.set(100 + a2, frames);
                    p.b.set(100 + a3, frames);
                    break;
                case 0x20:  // shift where the picture draws, apart from its position, which moves leave alone
                    p.shiftX = a1;
                    p.shiftY = a2;
                    break;
                case 0x30:  // shake
                    p.shakePower = a1;
                    p.shakeSpeed = a2;
                    p.shakeFrames = frames;
                    break;
                case 0x40:  // zoom
                    p.zoomX.set(a1, frames);
                    p.zoomY.set(a2 ? a2 : a1, frames);
                    break;
                case 0x50:  // pulse the colour by the offsets, the frames field giving the interval
                    p.pulse = {a1, a2, a3};
                    p.pulseInterval = (a1 || a2 || a3) ? std::max(0, frames) : 0;
                    break;
                case 0x60:
                    p.flickerFrames = frames;
                    p.flickerInterval = std::max(1, a1);
                    break;
                case 0x80: case 0x90: case 0xA0:  // animate the pattern
                    p.autoPatternMode = (type - 0x80) / 0x10 + 1;
                    p.autoFrom = a1;
                    p.autoTo = a2;
                    p.autoInterval = std::max(1, frames);
                    p.autoTimer = 0;
                    p.autoDir = a2 >= a1 ? 1 : -1;
                    p.pattern = a1;
                    break;
                default:
                    unsupported(c);
                    break;
            }
        }
    } else if (targetKind == 2) {  // the map: zoom around a point; 100% returns to normal
        if (type == 0x00) {
            mapZoomX_.set(a1, frames);
            mapZoomY_.set(a2, frames);
            mapZoom_.set(a3 > 0 ? a3 : 100, frames);
        } else {
            unsupported(c);
        }
    } else if (targetKind == 1) {  // a character
        Actor *a = actorFor(value(c.arg(2), t), t);
        if (!a) return;
        if (type == 0x10) {
            shake_.power = std::max(1, a1);
            shake_.speed = std::max(1, a2);
            shake_.frames = frames;
        }
    } else {
        unsupported(c);
    }
}

// A sound from the system database's lists: type 1 BGM, 2 BGS and 3 SE, each row a file, volume, frequency and
// loop start (for SE, a release mode).
void Game::playListed(SoundKind kind, int id, int fadeFrames, int startMs) {
    if (!audio_) return;
    int type = kind == SoundKind::BGM ? 1 : kind == SoundKind::BGS ? 2 : 3;
    std::string file = dbString(sdb_, type, id, 0);
    if (file.empty()) return;
    int volume = dbNumber(sdb_, type, id, 1), frequency = dbNumber(sdb_, type, id, 2);
    if (kind == SoundKind::SE) {
        audio_->playSE(file, volume ? volume : 100, frequency ? frequency : 100);
        return;
    }
    MusicParams p;
    p.volume = volume ? volume : 100;
    p.frequency = frequency ? frequency : 100;
    p.loopStartMs = dbNumber(sdb_, type, id, 3);
    p.fadeFrames = fadeFrames;
    p.startMs = startMs;
    audio_->playMusic(kind, file, p);
}

// The map's music from the system database's map list: a list number, -1 for silence, -2 to keep what plays, and -3
// for a file named in the row.
void Game::playMapMusic() {
    if (!audio_ || mapId_ < 0) return;
    for (SoundKind kind : {SoundKind::BGM, SoundKind::BGS}) {
        bool bgm = kind == SoundKind::BGM;
        int id = dbNumber(sdb_, SysMaps, mapId_, bgm ? 1 : 2);
        if (id == -2) continue;
        if (id == -1) audio_->stopMusic(kind);
        else if (id == -3) {
            std::string file = dbString(sdb_, SysMaps, mapId_, bgm ? 5 : 6);
            if (file.empty()) audio_->stopMusic(kind);
            else audio_->playMusic(kind, file, MusicParams{});
        } else {
            playListed(kind, id, 0);
        }
    }
}

void Game::playSoundEffect(int id) { playListed(SoundKind::SE, id, 0); }

// The sound command. Its first word holds the kind (BGM 0x00, BGS 0x10, SE 0x20) and what to do in its low byte, the
// list number in the next two bytes (0xFFFF stops), and in the top byte whether the sound is a list number, a list
// number in a variable, or a file named in the command.
void Game::soundCommand(Thread &t, const Command &c) {
    if (!audio_) return;
    uint32_t w = uint32_t(c.arg(0));
    int kindCode = w & 0xF0, exec = w & 0x0F;
    if (exec != 0) return;  // preloading and releasing sounds need nothing here
    SoundKind kind = kindCode == 0x10 ? SoundKind::BGS : kindCode == 0x20 ? SoundKind::SE : SoundKind::BGM;
    int id = int((w >> 8) & 0xFFFF);
    int spec = (w >> 24) & 0xFF;
    // Fade times are in tenths of a second.
    int fade = value(c.arg(1), t) * gameDat_.fps() / 10;
    if (spec == 0xFF || (spec != 2 && id == 0xFFFF)) {
        if (kind == SoundKind::SE) audio_->stopAllSE();
        else audio_->stopMusic(kind, fade);
        return;
    }
    if (spec == 1) id = value(c.arg(2), t);
    int startMs = value(c.arg(3), t);
    if (spec == 2) {
        // File names may hold codes such as \cself[9] that name the file through a variable.
        std::string file = expandText(c.str(0), &t);
        int volume = c.args.size() > 4 ? value(c.arg(4), t) : 100, frequency = c.args.size() > 5 ? value(c.arg(5), t) : 100;
        if (kind == SoundKind::SE) {
            audio_->playSE(file, volume, frequency);
        } else {
            MusicParams p;
            p.volume = volume;
            p.frequency = frequency;
            p.loopStartMs = c.args.size() > 6 ? value(c.arg(6), t) : 0;
            p.fadeFrames = fade;
            p.startMs = startMs;
            audio_->playMusic(kind, file, p);
        }
        return;
    }
    playListed(kind, id, fade, startMs);
}

void Game::chipCommand(Thread &t, const Command &c) {
    if (!map_) return;
    if (c.code == 242) {  // overwrite map tiles
        int layer = value(c.arg(0), t), x = value(c.arg(1), t), y = value(c.arg(2), t);
        int w = value(c.arg(3), t), h = value(c.arg(4), t), chip = value(c.arg(5), t);
        for (int yy = y; yy < y + h; yy++)
            for (int xx = x; xx < x + w; xx++)
                if (layer >= 0 && layer < map_->layers && xx >= 0 && yy >= 0 && xx < map_->width && yy < map_->height)
                    map_->tiles[(size_t(layer) * map_->width + xx) * map_->height + yy] = chip;
        return;
    }
    if (c.code == 241) {  // use another tileset
        map_->tileset = value(c.arg(0), t);
        return;
    }
    unsupported(c);
}

}  // namespace wolf

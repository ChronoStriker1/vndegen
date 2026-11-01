// Event commands with larger parameter sets: the database, extended variable reads, key input, party graphics,
// effects, map chips, sound and saves.
#include "game.h"
#include "game_internal.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <string_view>
#include <tuple>

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
    // A row's name is the one the project file gives it, or by the type's setting its first string field, or the name
    // of the same row in the type before (the manual's database command page and wolf-rpg-formats' data_id_method).
    auto rowName = [&db](int typeIndex, int r) {
        const DatabaseType &rt = db.types[typeIndex];
        if (rt.idMethod == 1) {
            for (size_t f = 0; f < rt.fieldKinds.size(); f++)
                if (rt.isString(f)) return r < int(rt.rows.size()) && rt.slot(f) < rt.rows[r].strings.size() ? rt.rows[r].strings[rt.slot(f)] : std::string();
        }
        const DatabaseType &named = rt.idMethod == 2 && typeIndex > 0 ? db.types[typeIndex - 1] : rt;
        return r < int(named.dataNames.size()) ? named.dataNames[r] : std::string();
    };
    auto findRow = [&](int typeIndex, const std::string &n) {
        for (int r = 0; r < int(db.types[typeIndex].rows.size()); r++)
            if (rowName(typeIndex, r) == n) return r;
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
    if (dt && (byName & 0x02)) row = findRow(type, c.str(2));
    if (dt && (byName & 0x04)) field = findName(dt->fieldNames, c.str(3));
    int32_t target = c.arg(4);

    // Queries: names, ids and sizes.
    if (c.arg(1) == -3 && c.arg(2) == -3) {  // type name or id
        if (byName & 0x01) writeVar(target, type, t);
        else writeStr(target, dt ? dt->name : "", t);
        return;
    }
    if (c.arg(2) == -3) {  // row name or id
        // The row may be named by a string variable, which looks its text up the same way (Clumsy Witch Seris's tags).
        if (dt && isStringVar(c.arg(1))) writeVar(target, findRow(type, readStr(c.arg(1), t)), t);
        else if (byName & 0x02) writeVar(target, row, t);
        else writeStr(target, dt && row >= 0 ? rowName(type, row) : "", t);
        return;
    }
    if (c.arg(1) == -3) {  // field name or id
        if (dt && isStringVar(c.arg(2))) writeVar(target, findName(dt->fieldNames, readStr(c.arg(2), t)), t);
        else if (byName & 0x04) writeVar(target, field, t);
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

namespace {

// Splits CSV text into rows of cells. A cell may be quoted, which lets it hold commas and line breaks, with "" for a
// quote inside. Each row of Wolf's own files ends with a comma, which adds no cell.
std::vector<std::vector<std::string>> parseCsv(std::string_view text) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string cell;
    bool quoted = false, started = false;
    auto endCell = [&] {
        row.push_back(std::move(cell));
        cell.clear();
        started = false;
    };
    auto endRow = [&] {
        if (started || !cell.empty()) endCell();
        rows.push_back(std::move(row));
        row.clear();
    };
    for (size_t i = 0; i < text.size(); i++) {
        char ch = text[i];
        if (quoted) {
            if (ch != '"') cell += ch;
            else if (i + 1 < text.size() && text[i + 1] == '"') {
                cell += '"';
                i++;
            } else {
                quoted = false;
            }
        } else if (ch == '"') {
            quoted = started = true;
        } else if (ch == ',') {
            endCell();
        } else if (ch == '\n') {
            endRow();
        } else if (ch != '\r') {
            cell += ch;
            started = true;
        }
    }
    if (started || !cell.empty() || !row.empty()) endRow();
    return rows;
}

}  // namespace

// Command 252. The flags' second byte's high nibble is 0x20 to copy the row in field 4 onto the row in field 1, or 0x30
// to sort the rows from field 1 on, as many as field 4, by the field in field 2. Byte three has 0x10 for descending.
void Game::dbRowCommand(Thread &t, const Command &c) {
    uint32_t flags = uint32_t(c.arg(3));
    Database &db = *databaseKind((flags >> 8) & 0x0F);
    int type = value(c.arg(0), t);
    if (flags & 0x10000) {
        type = -1;
        for (size_t i = 0; i < db.types.size(); i++)
            if (db.types[i].name == c.str(1)) type = int(i);
    }
    if (type < 0 || type >= int(db.types.size())) return;
    DatabaseType &dt = db.types[type];
    int row = value(c.arg(1), t), field = value(c.arg(2), t), other = value(c.arg(4), t);
    int rows = int(dt.rows.size());
    switch ((flags >> 8) & 0xF0) {
        case 0x20:
            if (row >= 0 && row < rows && other >= 0 && other < rows && row != other) dt.rows[row] = dt.rows[other];
            break;
        case 0x30: {
            if (row < 0 || row >= rows || field < 0 || field >= int(dt.fieldKinds.size())) break;
            bool descending = flags & 0x100000;
            size_t slot = dt.slot(field);
            bool isString = dt.isString(field);
            auto less = [&](const DatabaseType::Row &a, const DatabaseType::Row &b) {
                if (isString) return slot < a.strings.size() && slot < b.strings.size() && a.strings[slot] < b.strings[slot];
                return slot < a.numbers.size() && slot < b.numbers.size() && a.numbers[slot] < b.numbers[slot];
            };
            auto first = dt.rows.begin() + row, last = first + std::min(std::max(other, 0), rows - row);
            if (descending) std::stable_sort(first, last, [&](auto &a, auto &b) { return less(b, a); });
            else std::stable_sort(first, last, less);
            break;
        }
        default:
            unsupported(c);
            break;
    }
}

const XYArray *Game::xyArray(int number) const {
    auto it = xyNumbered_.find(number);
    return it == xyNumbered_.end() ? nullptr : &it->second;
}

const XYArray *Game::xyArray(const std::string &name) const {
    auto it = xyNamed_.find(name);
    return it == xyNamed_.end() ? nullptr : &it->second;
}

// The array a command names: by the string after the file name when the flags' 0x10000 is set, by number otherwise.
XYArray *Game::xyTarget(Thread &t, const Command &c, bool create) {
    if (uint32_t(c.arg(3)) & 0x10000) {
        auto it = xyNamed_.find(c.str(1));
        if (it != xyNamed_.end()) return &it->second;
        return create ? &xyNamed_[c.str(1)] : nullptr;
    }
    int number = value(c.arg(0), t);
    auto it = xyNumbered_.find(number);
    if (it != xyNumbered_.end()) return &it->second;
    return create ? &xyNumbered_[number] : nullptr;
}

// Command 255, the XY arrays of the manual's database command page. The fields are the array, X, Y, the flags and a
// value. The flags work as the database command's do: 0x1000 reads into the variable named by the value, otherwise the
// value goes in with the operator in the low byte. X of -1 reads the number of columns, and Y of -1 the number of values
// in column X. X of -2 deletes the array, and Y of -2 zeroes column X.
void Game::xyCommand(Thread &t, const Command &c) {
    uint32_t flags = uint32_t(c.arg(3));
    bool read = ((flags >> 8) & 0xF0) == 0x10;
    int op = flags & 0xF0, leftCode = flags & 0x0F;
    int32_t target = c.arg(4);
    int rawX = c.arg(1), rawY = c.arg(2);
    int x = value(rawX, t), y = value(rawY, t);
    // A Y of 10000 or more moves on to the next column, as the manual says.
    if (y >= 10000) {
        x += y / 10000;
        y %= 10000;
    }
    if (read) {
        const XYArray *a = xyTarget(t, c, false);
        int32_t v = 0;
        if (rawX == -1) v = a ? int32_t(a->size()) : 0;
        else if (rawY == -1) v = a && x >= 0 && x < int(a->size()) ? int32_t((*a)[x].size()) : 0;
        else if (a && x >= 0 && x < int(a->size()) && y >= 0 && y < int((*a)[x].size())) v = (*a)[x][y];
        int32_t cur = op ? readVar(target, t) : 0;
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
    if (rawX == -2) {
        if (flags & 0x10000) xyNamed_.erase(c.str(1));
        else xyNumbered_.erase(value(c.arg(0), t));
        return;
    }
    if (rawY == -2) {
        if (XYArray *a = xyTarget(t, c, false); a && x >= 0 && x < int(a->size())) std::fill((*a)[x].begin(), (*a)[x].end(), 0);
        return;
    }
    if (x < 0 || y < 0 || x >= 10000) return;
    XYArray &a = *xyTarget(t, c, true);
    if (x >= int(a.size())) a.resize(x + 1);
    if (y >= int(a[x].size())) a[x].resize(y + 1);
    int32_t v = leftCode == 1 ? target : value(target, t);
    int32_t &cell = a[x][y];
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

// Command 257 sorts columns of an XY array: from column field 1, as many as the value, by the number at Y field 2. The
// flags' 0x100000 is descending, as for command 252.
void Game::xySortCommand(Thread &t, const Command &c) {
    uint32_t flags = uint32_t(c.arg(3));
    XYArray *a = xyTarget(t, c, false);
    if (!a || ((flags >> 8) & 0xF0) != 0x30) return;
    int first = value(c.arg(1), t), y = value(c.arg(2), t), count = value(c.arg(4), t);
    if (first < 0 || first >= int(a->size())) return;
    auto key = [y](const std::vector<int32_t> &col) { return y >= 0 && y < int(col.size()) ? col[y] : 0; };
    auto begin = a->begin() + first, end = begin + std::min(std::max(count, 0), int(a->size()) - first);
    bool descending = flags & 0x100000;
    std::stable_sort(begin, end, [&](const auto &l, const auto &r) { return descending ? key(l) > key(r) : key(l) < key(r); });
}

XYImage resolveXYImage(const std::string &source, const Game &game) {
    XYImage out;
    bool named = source.starts_with("<XYN_IMAGE");
    size_t end = source.find('>');
    std::string body = source.substr(0, end);
    int xsize = 0, ysize = 0;
    for (size_t at = body.find('/'); at != std::string::npos;) {
        size_t next = body.find('/', at + 1);
        std::string part = body.substr(at + 1, next == std::string::npos ? next : next - at - 1);
        size_t eq = part.find('=');
        if (eq != std::string::npos) {
            std::string key = part.substr(0, eq), val = part.substr(eq + 1);
            int channel = key == "A" ? 0 : key == "R" ? 1 : key == "G" ? 2 : key == "B" ? 3 : -1;
            if (channel >= 0 && !val.empty()) out.channels[channel] = named ? game.xyArray(val) : game.xyArray(std::atoi(val.c_str()));
            if (key == "XSIZE") xsize = std::atoi(val.c_str());
            if (key == "YSIZE") ysize = std::atoi(val.c_str());
        }
        at = next;
    }
    for (const XYArray *a : out.channels) {
        if (!a) continue;
        out.w = std::max(out.w, int(a->size()));
        for (auto &col : *a) out.h = std::max(out.h, int(col.size()));
    }
    if (xsize > 0) out.w = xsize;
    if (ysize > 0) out.h = ysize;
    return out;
}

// Command 251: reads rows of a database from a CSV file, or writes them to one. The fields are the type, first row and
// first field, the flags, and the number of rows; the strings are the file, relative to the game folder, and the type's
// name when the flags say it goes by name. As the PC engine writes them, files start with a byte order mark and a row
// of the field names, every row ends with a comma, and strings are quoted.
void Game::csvCommand(Thread &t, const Command &c) {
    uint32_t flags = uint32_t(c.arg(3));
    Database &db = *databaseKind((flags >> 8) & 0x0F);
    bool write = ((flags >> 8) & 0xF0) == 0x10;
    int type = value(c.arg(0), t);
    if (flags & 0x10000) {
        type = -1;
        for (size_t i = 0; i < db.types.size(); i++)
            if (db.types[i].name == c.str(1)) type = int(i);
    }
    if (type < 0 || type >= int(db.types.size())) return;
    DatabaseType &dt = db.types[type];
    int firstRow = value(c.arg(1), t), firstField = value(c.arg(2), t), count = value(c.arg(4), t);
    if (firstRow < 0 || firstField < 0 || firstField >= int(dt.fieldKinds.size())) return;
    std::string name = expandText(c.str(0), &t);
    std::filesystem::path path = saveFilePath(name);

    if (write) {
        std::string out = "\xEF\xBB\xBF";
        for (size_t f = firstField; f < dt.fieldKinds.size(); f++)
            out += '"' + (f < dt.fieldNames.size() ? dt.fieldNames[f] : std::string()) + "\",";
        out += "\r\n";
        for (int r = firstRow; r < firstRow + count && r < int(dt.rows.size()); r++) {
            for (size_t f = firstField; f < dt.fieldKinds.size(); f++) {
                if (dt.isString(f)) out += '"' + (dt.slot(f) < dt.rows[r].strings.size() ? dt.rows[r].strings[dt.slot(f)] : std::string()) + "\",";
                else out += std::to_string(dt.slot(f) < dt.rows[r].numbers.size() ? dt.rows[r].numbers[dt.slot(f)] : 0) + ",";
            }
            out += "\r\n";
        }
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::binary);
        file.write(out.data(), std::streamsize(out.size()));
        if (!file) log("cannot write " + name);
        return;
    }

    std::string text;
    if (std::ifstream file(path, std::ios::binary); file) text.assign(std::istreambuf_iterator<char>(file), {});
    else if (auto bytes = files_.read(name)) text.assign(bytes->begin(), bytes->end());
    else {
        log("no CSV file " + name);
        return;
    }
    auto rows = parseCsv(decodeTextFile(text));
    size_t numbers = 0, strings = 0;
    for (size_t f = 0; f < dt.fieldKinds.size(); f++) (dt.isString(f) ? strings : numbers)++;
    // The first row names the fields. The rest fill the rows from firstRow, as many as the command says and the file has.
    for (int i = 0; i < count && size_t(i) + 1 < rows.size(); i++) {
        int r = firstRow + i;
        while (int(dt.rows.size()) <= r) {
            DatabaseType::Row added;
            added.numbers.assign(numbers, 0);
            added.strings.assign(strings, "");
            dt.rows.push_back(std::move(added));
        }
        const auto &cells = rows[size_t(i) + 1];
        for (size_t col = 0; col < cells.size() && firstField + col < dt.fieldKinds.size(); col++) {
            size_t f = firstField + col;
            if (dt.isString(f)) {
                if (dt.slot(f) < dt.rows[r].strings.size()) dt.rows[r].strings[dt.slot(f)] = cells[col];
            } else if (dt.slot(f) < dt.rows[r].numbers.size()) {
                dt.rows[r].numbers[dt.slot(f)] = std::atoi(cells[col].c_str());
            }
        }
    }
}

// Reads a property of a character, a map position, a picture, or the game into a variable.
void Game::setVariablePlus(Thread &t, const Command &c) {
    int32_t target = c.arg(0);
    uint32_t flags = uint32_t(c.arg(1));
    if (flags & 0x10) target = readVar(target, t);
    int exec = (flags >> 8) & 0xF0;
    int assign = (flags >> 8) & 0x0F;
    // A map position names what to read in the flags. A character, a picture and "other" name it in a field of its own.
    int info = exec == 0x20 ? (flags >> 16) & 0xFF : exec == 0x30 ? c.arg(2) : c.arg(3);
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
                // A Wolf RPG Editor 3.5 map may have fewer than three layers; a layer it lacks reads as empty.
                v = layer < map_->layers ? map_->tile(layer, x, y) : 0;
                break;
            }
            default: {  // tile tags
                if (!map_ || x < 0 || y < 0 || x >= map_->width || y >= map_->height) break;
                const auto &tags = tilesets_[map_->tileset].tags;
                int layer = (info & 0x0F) == 0 ? -1 : (info & 0x0F) - 8;
                if (layer < 0) {
                    for (layer = map_->layers - 1; layer > 0 && map_->tile(layer, x, y) == 0; layer--) {}
                }
                if (layer >= map_->layers) break;
                int32_t tv = map_->tile(layer, x, y);
                size_t index = tv >= 100000 ? size_t(tv / 100000) : size_t(tv) + 16;
                v = index < tags.size() ? tags[index] : 0;
                break;
            }
        }
    } else if (exec == 0x40) {  // picture
        auto it = pictures_.find(value(c.arg(2), t));
        if (it == pictures_.end()) {
            v = 0;
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
                case 8: v = pictureUnderMouse(p); break;
                case 9: v = 1; break;
                case 10: v = 1; break;
                default: v = 0; break;
            }
        }
    } else if (exec == 0x30) {  // other
        // The clicks read 0 up, 1 held and 2 just pressed (the manual's variable operation page).
        auto click = [&](uint32_t button) { return input_.mousePressed & button ? 2 : input_.mouseHeld & button ? 1 : 0; };
        switch (info) {
            case 0: v = mapId_; break;
            case 7: v = click(MouseLeft); break;
            case 8: v = click(MouseRight); break;
            case 9: v = click(MouseMiddle); break;
            case 10: v = input_.wheel; break;
            case 11: v = mouseX_ - lastMouseX_; break;
            case 12: v = mouseY_ - lastMouseY_; break;
            case 13: v = eventUnderMouse(); break;
            case 20: v = mouseX_; break;
            case 21: v = mouseY_; break;
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

uint32_t Game::keysNow() const { return input_.held; }

// Builds this frame's input from the host's and from what command 125 pressed last frame. Input a game turned off with
// command 126 counts as not there, except what command 125 presses. The manual's key input page says auto input of
// basic keys obeys command 126, but YUME-SAME turns confirm off every frame and presses it with command 125 for each
// click, so the engine can't work that way.
void Game::gatherInput(const Input &in) {
    input_ = in;
    if (keyboardOff_) {
        input_.held = input_.pressed = 0;
        input_.keysHeld.reset();
        input_.keysPressed.reset();
    }
    input_.keysHeld &= ~keyCodesOff_;
    input_.keysPressed &= ~keyCodesOff_;
    if (mouseOff_) input_.mouseHeld = input_.mousePressed = 0, input_.wheel = 0;

    input_.keysHeld |= autoKeyCodes_;
    input_.keysPressed |= autoKeyCodes_ & ~lastKeysHeld_;
    input_.mouseHeld |= autoMouse_;
    input_.mousePressed |= autoMouse_ & ~lastMouseHeld_;
    input_.wheel += autoWheel_;
    // The keys the game's own key settings name work as the basic keys (system variables 52 to 54).
    static const std::array<Key, 3> configured = {KeyConfirm, KeyCancel, KeySub};
    for (size_t i = 0; i < configured.size() && 52 + i < system_.size(); i++) {
        int code = system_[52 + i];
        if (code > 0 && code < kKeyCodeCount && input_.keysHeld[code]) {
            input_.held |= configured[i];
            if (input_.keysPressed[code]) input_.pressed |= configured[i];
        }
    }
    for (size_t k = 0; k < basicControl_.size(); k++) {
        if (basicControl_[k] == 2) {
            input_.held &= ~(1u << k);
            input_.pressed &= ~(1u << k);
        }
    }
    input_.held |= autoKeys_;
    input_.pressed |= autoKeys_ & ~lastHeld_;
    autoKeys_ = autoMouse_ = 0;
    autoKeyCodes_.reset();
    autoWheel_ = 0;

    for (size_t k = 0; k < heldFrames_.size(); k++) heldFrames_[k] = (input_.held >> k) & 1 ? heldFrames_[k] + 1 : 0;
    for (size_t k = 0; k < mouseHeldFrames_.size(); k++)
        mouseHeldFrames_[k] = (input_.mouseHeld >> k) & 1 ? mouseHeldFrames_[k] + 1 : 0;
    for (int k = 0; k < kKeyCodeCount; k++) keyHeldFrames_[k] = input_.keysHeld[k] ? keyHeldFrames_[k] + 1 : 0;
    lastHeld_ = input_.held;
    lastMouseHeld_ = input_.mouseHeld;
    lastKeysHeld_ = input_.keysHeld;

    // The pointer follows the host's whenever that moves. Otherwise it stays where it is, which may be where the game
    // put it, with command 125 or by writing system variables 71 and 72.
    lastMouseX_ = mouseX_;
    lastMouseY_ = mouseY_;
    if (system_.size() > 72 && (system_[71] != mouseX_ || system_[72] != mouseY_)) {
        mouseX_ = system_[71];
        mouseY_ = system_[72];
    }
    if (in.mouseX != hostMouseX_ || in.mouseY != hostMouseY_) {
        mouseX_ = hostMouseX_ = in.mouseX;
        mouseY_ = hostMouseY_ = in.mouseY;
    }
    if (system_.size() > 72) {
        system_[71] = mouseX_;
        system_[72] = mouseY_;
    }
}

// Command 125. The top byte says what it presses: basic keys (flag bits below), one keyboard key, or the mouse.
void Game::autoInput(Thread &t, const Command &c) {
    uint32_t flags = uint32_t(c.arg(0));
    switch (flags >> 24) {
        case 0x00: {
            static const std::array<std::pair<uint32_t, Key>, 7> bits = {{{0x01, KeyConfirm}, {0x02, KeyCancel}, {0x04, KeySub},
                {0x10, KeyDown}, {0x20, KeyLeft}, {0x40, KeyRight}, {0x80, KeyUp}}};
            for (auto [bit, key] : bits)
                if (flags & bit) autoKeys_ |= key;
            break;
        }
        case 0x10: {
            int code = value(c.arg(1), t);
            if (code >= 0 && code < kKeyCodeCount) autoKeyCodes_.set(code);
            break;
        }
        case 0x20:
            if (flags & 0x01) autoMouse_ |= MouseLeft;
            if (flags & 0x02) autoMouse_ |= MouseRight;
            if (flags & 0x04) autoMouse_ |= MouseMiddle;
            if (flags & 0x08) {
                mouseX_ = value(c.arg(1), t);
                mouseY_ = value(c.arg(2), t);
                if (system_.size() > 72) {
                    system_[71] = mouseX_;
                    system_[72] = mouseY_;
                }
            }
            if (flags & 0x10) autoWheel_ += value(c.arg(1), t);
            break;
        default:
            unsupported(c);
            break;
    }
}

// Command 126. A top byte of 0 sets how the basic keys named by the low byte work; 0x10 turns a device on or off: the
// low byte is 0 for a keyboard key (-1 for all of them), 1 the mouse, 2 a gamepad and 3 every device, and the next
// byte is 1 for off. VNDegen delivers gamepads as keyboard keys, so a keyboard turned off stops those too, and a
// gamepad off alone changes nothing.
void Game::keyControl(Thread &t, const Command &c) {
    uint32_t flags = uint32_t(c.arg(0));
    int mode = (flags >> 8) & 0xFF;
    if ((flags >> 24) == 0x10) {
        bool off = mode == 1;
        switch (flags & 0xFF) {
            case 0: {
                int code = value(c.arg(1), t);
                if (code < 0) keyboardOff_ = off;
                else if (code < kKeyCodeCount) keyCodesOff_.set(code, off);
                break;
            }
            case 1: mouseOff_ = off; break;
            case 2: break;
            default:
                keyboardOff_ = mouseOff_ = off;
                break;
        }
        return;
    }
    static const std::array<std::pair<uint32_t, Key>, 7> bits = {{{0x01, KeyConfirm}, {0x02, KeyCancel}, {0x04, KeySub},
        {0x10, KeyDown}, {0x20, KeyLeft}, {0x40, KeyRight}, {0x80, KeyUp}}};
    for (auto [bit, key] : bits)
        if (flags & bit) basicControl_[std::countr_zero(uint32_t(key))] = mode;
}

// The rectangle a picture covers, by its anchor and zoom. A turned picture counts as its unturned rectangle.
bool Game::pictureUnderMouse(const Picture &p) const {
    double w = p.imageW * p.zoomX.value() / 100, h = p.imageH * p.zoomY.value() / 100;
    if (p.kind == PictureKind::File) {
        w /= std::max(1, p.divX);
        h /= std::max(1, p.divY);
    }
    double x = p.x.value() + p.shiftX, y = p.y.value() + p.shiftY;
    switch (p.anchor) {
        case 1: x -= w / 2; y -= h / 2; break;
        case 2: y -= h; break;
        case 3: x -= w; break;
        case 4: x -= w; y -= h; break;
        case 5: x -= w / 2; break;
        case 6: x -= w / 2; y -= h; break;
    }
    return mouseX_ >= x && mouseX_ < x + w && mouseY_ >= y && mouseY_ < y + h;
}

// The lowest numbered map event under the pointer, or -1: one tile wide and tall, the map's own coordinates.
int Game::eventUnderMouse() const {
    int tile = gameDat_.tileSize(), best = -1;
    for (auto &e : events_) {
        if (e.page < 0) continue;
        double x = e.actor.chara.x * tile - cameraX_, y = e.actor.chara.y * tile - cameraY_;
        if (mouseX_ >= x && mouseX_ < x + tile && mouseY_ >= y && mouseY_ < y + tile && (best < 0 || e.data->id < best))
            best = e.data->id;
    }
    return best;
}

// Key input. Basic keys give 2 4 6 8 for the directions (1 3 7 9 for diagonals), 10 confirm, 11 cancel, 12 sub,
// and 0 for none. With "wait for input" the event waits until one of the accepted keys is pressed.
bool Game::keyInput(Thread &t, const Command &c) {
    int32_t target = c.arg(0);
    uint32_t flags = uint32_t(c.arg(1));
    int kind = (flags >> 8) & 0x0F;
    bool wait = flags & 0x80;
    // Wolf RPG Editor 3: bit 0x20000 reports only a key that was just pressed, so a menu opened by a key doesn't
    // see that key still held and close again (Clumsy Witch Seris). Bit 0x40000 reports how long it has been held.
    bool fresh = wait || (flags & 0x20000);
    bool heldTime = flags & 0x40000;
    uint32_t keys = fresh ? input_.pressed : keysNow();
    int32_t v = 0;
    uint32_t usedMouse = 0;
    if (kind == 0 && heldTime) {
        // The low byte names the key as the direction and button fields do: 3 up, 4 down, 5 left, 6 right, 0x10
        // confirm, 0x20 cancel, 0x40 sub. Games use it for their own key repeat in menus (Clumsy Witch Seris).
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
    } else if (kind == 1) {
        // Keyboard keys by Wolf's key code: one key, which reads 1 when down, or the first key down from the code given.
        const KeyCodes &codes = fresh ? input_.keysPressed : input_.keysHeld;
        int code = value(c.arg(2), t);
        if ((flags >> 8) & 0xF0) {
            if (code >= 0 && code < kKeyCodeCount) v = heldTime ? keyHeldFrames_[code] : codes[code];
        } else {
            for (int k = 100; k < kKeyCodeCount && !v; k++)
                if (codes[k]) v = heldTime ? keyHeldFrames_[k] : k;
        }
    } else if (kind == 3) {
        // The mouse: the low nibble is 0 for clicks, reading 20 left, 21 right and 22 middle, 1 for X, 2 for Y, and 3
        // for the wheel (WodiLib's KeyInputMouse).
        switch (flags & 0x0F) {
            case 1: v = mouseX_; break;
            case 2: v = mouseY_; break;
            case 3: v = input_.wheel; break;
            default: {
                uint32_t buttons = fresh ? input_.mousePressed : input_.mouseHeld;
                static const std::array<std::tuple<uint32_t, uint32_t, int>, 3> accepted = {{{0x10, MouseLeft, 20},
                    {0x20, MouseRight, 21}, {0x40, MouseMiddle, 22}}};
                for (size_t i = 0; i < accepted.size() && !v; i++) {
                    auto [bit, button, result] = accepted[i];
                    if (!(flags & bit)) continue;
                    if (heldTime) v = mouseHeldFrames_[i];
                    else if (buttons & button) {
                        v = result;
                        usedMouse |= button;
                    }
                }
                break;
            }
        }
    }
    // Kind 2 is a gamepad's own buttons. VNDegen delivers gamepads as keyboard keys, so none reads as down.
    if (wait && v == 0) {
        t.stack.back().pc--;
        return false;
    }
    // A press a waiting input accepts is used up, so the next wait in the same frame waits for a press of its own.
    if (wait) {
        input_.pressed &= ~keys;
        input_.mousePressed &= ~usedMouse;
        if (kind == 1) input_.keysPressed.reset();
    }
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

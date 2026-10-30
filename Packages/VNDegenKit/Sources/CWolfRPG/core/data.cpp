#include "data.h"
#include <array>

#include "reader.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#else
#include <iconv.h>
#endif

namespace wolf {

const std::string &Command::str(size_t i) const {
    static const std::string empty;
    return i < strings.size() ? strings[i] : empty;
}

namespace {

// Converts one Shift-JIS character of one or two bytes. Returns false when the bytes are not a valid character.
bool convertCharacter(const char *bytes, size_t n, std::string &out) {
#ifdef __APPLE__
    CFStringRef s = CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8 *>(bytes), CFIndex(n),
                                            kCFStringEncodingDOSJapanese, false);
    if (!s) return false;
    char buf[16];
    bool ok = CFStringGetCString(s, buf, sizeof buf, kCFStringEncodingUTF8);
    CFRelease(s);
    if (ok) out += buf;
    return ok;
#else
    thread_local iconv_t cd = iconv_open("UTF-8", "CP932");
    char *in = const_cast<char *>(bytes);
    size_t inLeft = n;
    char buf[16];
    char *o = buf;
    size_t oLeft = sizeof buf;
    iconv(cd, nullptr, nullptr, nullptr, nullptr);
    if (iconv(cd, &in, &inLeft, &o, &oLeft) == size_t(-1)) return false;
    out.append(buf, o - buf);
    return true;
#endif
}

}  // namespace

// Converts Shift-JIS (code page 932) to UTF-8, one character at a time. Translated games often hold stray bytes that
// are not valid Shift-JIS, which become U+FFFD rather than failing the whole file.
std::string toUtf8(const std::string &raw, Encoding enc) {
    if (enc == Encoding::UTF8) return raw;
    std::string out;
    out.reserve(raw.size() * 3 / 2);
    for (size_t i = 0; i < raw.size();) {
        auto c = static_cast<unsigned char>(raw[i]);
        if (c < 0x80) {
            out += char(c);
            i++;
            continue;
        }
        bool lead = (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
        size_t n = lead && i + 1 < raw.size() ? 2 : 1;
        if (!convertCharacter(raw.data() + i, n, out)) out += "\xEF\xBF\xBD";
        i += n;
    }
    return out;
}

namespace {

// Data files start with a zero byte and a magic number in which one byte is 'U' for UTF-8 files.
Encoding readMagic(Reader &r, std::initializer_list<uint8_t> magic, size_t encodingIndex, const char *what) {
    Encoding enc = Encoding::ShiftJIS;
    size_t i = 0;
    for (uint8_t m : magic) {
        uint8_t b = r.u8();
        if (i == encodingIndex && b == 'U') enc = Encoding::UTF8;
        else if (b != m) throw FormatError(std::string("bad magic in ") + what);
        i++;
    }
    return enc;
}

struct Parser {
    Reader r;
    Encoding enc;
    bool v35 = false;

    std::string str() { return toUtf8(r.rawString(), enc); }

    MoveRoute route(uint8_t flags) {
        MoveRoute route;
        route.flags = flags;
        uint32_t n = r.count(4);  // id, argument count, and the 01 00 that ends a step
        route.steps.reserve(n);
        for (uint32_t i = 0; i < n; i++) {
            RouteStep step;
            step.id = r.u8();
            uint8_t argc = r.u8();
            for (uint8_t a = 0; a < argc; a++) step.args.push_back(r.s32());
            r.expect({0x01, 0x00}, "route step");
            route.steps.push_back(std::move(step));
        }
        return route;
    }

    Command command() {
        Command c;
        uint8_t argc = r.u8();
        if (argc == 0) throw FormatError("empty command at " + std::to_string(r.pos()));
        c.code = r.s32();
        c.args.reserve(argc - 1);
        for (uint8_t i = 1; i < argc; i++) c.args.push_back(r.s32());
        c.indent = r.u8();
        uint8_t strc = r.u8();
        for (uint8_t i = 0; i < strc; i++) c.strings.push_back(str());
        uint8_t term = r.u8();
        if (term == 0x01) {
            r.skip(5);
            uint8_t flags = r.u8();
            c.route = std::make_shared<MoveRoute>(route(flags));
        } else if (term != 0x00) {
            throw FormatError("bad command terminator " + std::to_string(term) + " at " + std::to_string(r.pos()));
        }
        if (v35) {
            uint8_t extra = r.u8();
            r.skip(extra);
        }
        return c;
    }

    Commands commands() {
        uint32_t n = r.count(8);  // argument count, code, indent, string count, terminator
        Commands cmds;
        cmds.reserve(n);
        for (uint32_t i = 0; i < n; i++) cmds.push_back(command());
        return cmds;
    }
};

}  // namespace

GameDat parseGameDat(std::span<const uint8_t> data) {
    Reader r(data);
    GameDat g;
    g.encoding = readMagic(r, {0x00, 'W', 0, 0, 'O', 'L', 0, 'F', 'M', 0}, 9, "Game.dat");
    Parser p{r, g.encoding};
    auto settings = p.r.bytes(p.r.u32());
    g.settings.assign(settings.begin(), settings.end());
    uint32_t count = p.r.u32();
    std::vector<std::string> raw;
    for (uint32_t i = 0; i < count; i++) {
        // The third string is the archive key, which is binary.
        if (i == 2) raw.push_back(p.r.rawString());
        else raw.push_back(p.str());
    }
    auto at = [&](size_t i) { return i < raw.size() ? raw[i] : std::string(); };
    g.title = at(0);
    g.serial = at(1);
    g.encryptionKey = at(2);
    for (int i = 0; i < 4; i++) g.fonts[i] = at(3 + i);
    g.startHeroGraphic = at(7);
    g.subtitle = at(8);
    p.r.u32();  // file size
    p.r.u32();
    uint32_t words = p.r.u32();
    std::vector<uint16_t> w;
    for (uint32_t i = 0; i < words; i++) w.push_back(p.r.u16());
    g.screenWidth = g.screenHeight = 0;
    if (w.size() > 17) {
        g.screenWidth = w[16];
        g.screenHeight = w[17];
    }
    if (w.size() > 18) g.editorVersion = w[18];
    // Files from before the screen size was stored show 20 by 15 tiles.
    if (w.size() <= 17 || g.screenWidth <= 0 || g.screenHeight <= 0) {
        g.screenWidth = g.tileSize() * 20;
        g.screenHeight = g.tileSize() * 15;
    }
    p.r.u32();
    p.r.u32();
    auto rest = p.r.bytes(p.r.size() - p.r.pos());
    g.randoms.assign(rest.begin(), rest.end());
    return g;
}

std::vector<CommonEvent> parseCommonEvents(std::span<const uint8_t> data) {
    Reader r(data);
    Encoding enc = readMagic(r, {0x00, 'W', 0, 0, 'O', 'L', 0, 'F', 'C', 0}, 6, "CommonEvent.dat");
    Parser p{r, enc};
    uint8_t version = p.r.u8();
    if (version == 0x93 || version == 0xCC) throw FormatError("compressed CommonEvent.dat (Wolf RPG Editor 3.5) is not supported yet");
    uint32_t count = p.r.count(16);
    std::vector<CommonEvent> events;
    events.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        CommonEvent ev;
        p.r.expect({0x8E}, "common event");
        ev.id = p.r.s32();
        uint8_t cond = p.r.u8();
        ev.conditionOperator = cond >> 4;
        ev.runCondition = cond & 0x0F;
        ev.conditionVariable = p.r.s32();
        ev.conditionValue = p.r.s32();
        ev.numberArgCount = p.r.u8();
        ev.stringArgCount = p.r.u8();
        ev.name = p.str();
        ev.commands = p.commands();
        p.str();  // unknown
        p.str();  // memo
        p.r.expect({0x8F}, "common event");
        uint32_t n = p.r.u32();
        for (uint32_t k = 0; k < n; k++) p.str();  // argument names
        n = p.r.u32();
        p.r.skip(n);  // argument special settings
        n = p.r.u32();
        for (uint32_t k = 0; k < n; k++) {
            uint32_t m = p.r.u32();
            for (uint32_t j = 0; j < m; j++) p.str();
        }
        n = p.r.u32();
        for (uint32_t k = 0; k < n; k++) {
            uint32_t m = p.r.u32();
            p.r.skip(size_t(m) * 4);
        }
        n = p.r.u32();
        for (uint32_t k = 0; k < n; k++) ev.numberArgDefaults.push_back(p.r.s32());
        p.r.expect({0x90}, "common event");
        p.r.u32();  // editor color
        for (auto &name : ev.cselfNames) name = p.str();
        p.r.expect({0x91}, "common event");
        p.str();
        uint8_t ind = p.r.u8();
        if (ind == 0x92) {
            ev.returnName = p.str();
            ev.returnVariable = p.r.s32();
            p.r.expect({0x92}, "common event");
        } else if (ind != 0x91) {
            throw FormatError("bad common event footer");
        }
        events.push_back(std::move(ev));
    }
    return events;
}

Database parseDatabase(std::span<const uint8_t> project, std::span<const uint8_t> dat, Encoding enc) {
    Database db;
    Parser pp{Reader(project), enc};
    uint32_t typeCount = pp.r.count(28);  // a name and six counts at least
    db.types.resize(typeCount);
    for (auto &t : db.types) {
        t.name = pp.str();
        uint32_t n = pp.r.u32();
        for (uint32_t i = 0; i < n; i++) t.fieldNames.push_back(pp.str());
        n = pp.r.u32();
        for (uint32_t i = 0; i < n; i++) t.dataNames.push_back(pp.str());
        pp.str();  // memo
        n = pp.r.u32();
        pp.r.skip(n);  // special settings per field
        n = pp.r.u32();
        for (uint32_t i = 0; i < n; i++) pp.str();
        n = pp.r.u32();
        for (uint32_t i = 0; i < n; i++) {
            uint32_t m = pp.r.u32();
            for (uint32_t j = 0; j < m; j++) pp.str();
        }
        n = pp.r.u32();
        for (uint32_t i = 0; i < n; i++) pp.r.skip(size_t(pp.r.u32()) * 4);
        n = pp.r.u32();
        for (uint32_t i = 0; i < n; i++) t.defaults.push_back(pp.r.s32());
    }

    Reader dr(dat);
    Encoding datEnc = readMagic(dr, {0x00, 'W', 0, 0, 'O', 'L', 0, 'F', 'M', 0}, 6, "database");
    Parser dp{dr, datEnc};
    uint8_t version = dp.r.u8();
    uint32_t datTypes = dp.r.u32();
    if (datTypes != typeCount) throw FormatError("database type count differs between .project and .dat");
    for (auto &t : db.types) {
        dp.r.expect({0xFE, 0xFF, 0xFF, 0xFF}, "database type");
        t.idMethod = dp.r.u32();
        uint32_t fields = dp.r.count(4);
        if (t.idMethod == 0x1D4C0) dp.str();
        size_t numbers = 0, strings = 0;
        for (uint32_t i = 0; i < fields; i++) {
            uint32_t kind = dp.r.u32();
            t.fieldKinds.push_back(kind);
            if (kind >= 2000) strings++;
            else numbers++;
        }
        uint32_t rows = dp.r.count(std::max<size_t>(1, (numbers + strings) * 4));
        t.rows.resize(rows);
        for (auto &row : t.rows) {
            for (size_t i = 0; i < numbers; i++) row.numbers.push_back(dp.r.s32());
            for (size_t i = 0; i < strings; i++) row.strings.push_back(dp.str());
        }
    }
    if (dp.r.u8() != version) throw FormatError("database footer mismatch");
    return db;
}

Map parseMap(std::span<const uint8_t> data) {
    Reader r(data);
    Map m;
    r.expect({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'W', 'O', 'L', 'F', 'M', 0}, "map");
    Encoding enc = r.u8() == 'U' ? Encoding::UTF8 : Encoding::ShiftJIS;
    r.skip(3);
    r.u32();  // 0x64
    uint8_t version = r.u8();
    if (version >= 0x67) throw FormatError("compressed map (Wolf RPG Editor 3.5) is not supported yet");
    Parser p{r, enc};
    m.name = p.str();
    m.tileset = p.r.s32();
    m.width = p.r.s32();
    m.height = p.r.s32();
    uint32_t eventCount = p.r.u32();
    // The editor makes maps of at most 9999 tiles a side; anything larger is damage, as is a size the file can't hold.
    if (m.width < 0 || m.height < 0 || m.width > 9999 || m.height > 9999 ||
        size_t(m.width) * size_t(m.height) > size_t(4) << 20)
        throw FormatError("bad map size " + std::to_string(m.width) + "x" + std::to_string(m.height));
    bool tiles = true;
    if (enc == Encoding::UTF8) {
        size_t at = p.r.pos();
        if (p.r.s32() == -1) tiles = false;
        else p.r.seek(at);
    }
    if (tiles) {
        size_t n = size_t(m.width) * m.height * m.layers;
        if (n * 4 > p.r.size() - p.r.pos()) throw FormatError("map tiles run past the end");
        m.tiles.resize(n);
        for (auto &t : m.tiles) t = p.r.s32();
    } else {
        m.tiles.assign(size_t(m.width) * m.height * m.layers, 0);
    }
    uint8_t ind;
    while ((ind = p.r.u8()) == 0x6F) {
        MapEvent ev;
        p.r.expect({0x39, 0x30, 0, 0}, "map event");
        ev.id = p.r.s32();
        ev.name = p.str();
        ev.x = p.r.s32();
        ev.y = p.r.s32();
        uint32_t pages = p.r.u32();
        p.r.expect({0, 0, 0, 0}, "map event");
        uint8_t pi;
        while ((pi = p.r.u8()) == 0x79) {
            MapPage pg;
            pg.chip = p.r.s32();
            pg.graphic = p.str();
            pg.direction = p.r.u8();
            pg.frame = p.r.u8();
            pg.opacity = p.r.u8();
            pg.blend = p.r.u8();
            pg.trigger = p.r.u8();
            for (auto &o : pg.condOps) o = p.r.u8();
            for (auto &v : pg.condVars) v = p.r.s32();
            for (auto &v : pg.condValues) v = p.r.s32();
            pg.moveType = p.r.u8();
            pg.moveSpeed = p.r.u8();
            pg.moveFrequency = p.r.u8();
            pg.animSpeed = p.r.u8();
            pg.flags = p.r.u8();
            uint8_t routeFlags = p.r.u8();
            pg.route = p.route(routeFlags);
            pg.commands = p.commands();
            uint32_t features = p.r.u32();
            pg.shadow = p.r.u8();
            pg.rangeX = p.r.u8();
            pg.rangeY = p.r.u8();
            if (features > 3) p.r.u8();
            p.r.expect({0x7A}, "map page");
            ev.pages.push_back(std::move(pg));
        }
        if (pi != 0x70 || ev.pages.size() != pages) throw FormatError("bad map event pages");
        m.events.push_back(std::move(ev));
    }
    if (ind != 0x66 || m.events.size() != eventCount) throw FormatError("bad map event list");
    return m;
}

std::vector<Tileset> parseTilesets(std::span<const uint8_t> data) {
    Reader r(data);
    Encoding enc = readMagic(r, {0x00, 'W', 0, 0, 'O', 'L', 0, 'F', 'M', 0}, 6, "TileSetData.dat");
    Parser p{r, enc};
    uint8_t version = p.r.u8();
    uint32_t count = p.r.u32();
    std::vector<Tileset> sets(count);
    int autotiles = version >= 0xD2 ? 31 : 15;
    for (auto &t : sets) {
        t.name = p.str();
        t.baseFile = p.str();
        for (int i = 0; i < autotiles; i++) t.autotileFiles.push_back(p.str());
        p.r.expect({0xFF}, "tileset");
        uint32_t n = p.r.u32();
        auto tags = p.r.bytes(n);
        t.tags.assign(tags.begin(), tags.end());
        p.r.expect({0xFF}, "tileset");
        n = p.r.u32();
        for (uint32_t i = 0; i < n; i++) t.passability.push_back(p.r.u32());
    }
    return sets;
}

// Microsoft C's rand(), which the protection draws its key stream from.
namespace {
struct MsvcRand {
    uint32_t state;
    int next() {
        state = state * 214013 + 2531011;
        return int((state >> 16) & 0x7FFF);
    }
};
}  // namespace

bool decryptBasicData(std::vector<uint8_t> &bytes, BasicFile kind) {
    constexpr size_t headerSize = 10;
    if (bytes.size() < headerSize || bytes[0] == 0) return false;
    // Three passes, each seeded by one byte of the header, flip the low bits of every first, second and fifth byte.
    const std::array<size_t, 3> seedAt = kind == BasicFile::GameDat ? std::array<size_t, 3>{0, 8, 6}
                                                                      : std::array<size_t, 3>{0, 3, 9};
    constexpr size_t steps[3] = {1, 2, 5};
    std::vector<uint8_t> out = {0x00, 'W', 0x00, 0x00, 'O', 'L', 0x00, 'F', kind == BasicFile::CommonEvent ? uint8_t('C') : uint8_t('M'), 0x00};
    size_t body = out.size();
    out.insert(out.end(), bytes.begin() + headerSize, bytes.end());
    for (size_t pass = 0; pass < 3; pass++) {
        MsvcRand rng{bytes[seedAt[pass]]};
        for (size_t j = body; j < out.size(); j += steps[pass]) out[j] ^= uint8_t(rng.next() >> 12);
    }
    bytes = std::move(out);
    return true;
}

}  // namespace wolf

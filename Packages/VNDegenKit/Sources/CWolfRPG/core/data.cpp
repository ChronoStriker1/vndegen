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

// Wolf RPG Editor 3.5 compresses the body of its common event, database and map files with LZ4. From `at` on, such a
// file holds the unpacked size, the packed size, and one LZ4 block (the block format of the LZ4 project's
// lz4_Block_format.md). This returns the file with that part unpacked, the bytes before `at` kept as they are.
std::vector<uint8_t> unpackLz4(std::span<const uint8_t> data, size_t at) {
    Reader r(data);
    r.seek(at);
    uint32_t unpacked = r.u32(), packed = r.u32();
    // LZ4 expands at most 255 to 1, so a larger claim is damage.
    if (uint64_t(unpacked) > uint64_t(packed) * 255 + 16) throw FormatError("bad LZ4 sizes");
    auto src = r.bytes(packed);
    std::vector<uint8_t> out(data.begin(), data.begin() + at);
    out.reserve(at + unpacked);
    size_t i = 0;
    auto length = [&](size_t n) {
        if (n != 15) return n;
        uint8_t b;
        do {
            if (i >= src.size()) throw FormatError("LZ4 length runs past the end");
            b = src[i++];
            n += b;
        } while (b == 255);
        return n;
    };
    while (i < src.size()) {
        uint8_t token = src[i++];
        size_t literals = length(token >> 4);
        if (literals > src.size() - i) throw FormatError("LZ4 literals run past the end");
        out.insert(out.end(), src.begin() + i, src.begin() + i + literals);
        i += literals;
        if (i == src.size()) break;  // the last sequence has literals only
        if (src.size() - i < 2) throw FormatError("LZ4 offset runs past the end");
        size_t offset = src[i] | src[i + 1] << 8;
        i += 2;
        size_t match = length(token & 0x0F) + 4;
        if (offset == 0 || offset > out.size() - at) throw FormatError("bad LZ4 offset");
        if (out.size() - at + match > unpacked) throw FormatError("LZ4 output runs past its size");
        // The copy may overlap the bytes it writes, so it goes one byte at a time.
        size_t from = out.size() - offset;
        for (size_t k = 0; k < match; k++) out.push_back(out[from + k]);
    }
    if (out.size() - at != unpacked) throw FormatError("LZ4 output size differs from its header");
    return out;
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
    uint8_t version = r.u8();
    // Wolf RPG Editor 3.5 compresses the rest of the file, and its commands end with a few bytes of their own.
    std::vector<uint8_t> unpacked;
    bool v35 = version == 0x93 || version == 0xCC;
    if (v35) {
        unpacked = unpackLz4(data, r.pos());
        r = Reader(unpacked);
        r.seek(11);
    }
    Parser p{r, enc, v35};
    uint32_t count = p.r.count(16);
    std::vector<CommonEvent> events;
    events.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        CommonEvent ev;
        p.r.expect({0x8E}, "common event");
        ev.id = p.r.s32();
        // Variable addresses give common events four digits (15000000 + id * 100 + n), so ids are 0 to 9999.
        if (ev.id < 0 || ev.id > 9999) throw FormatError("common event id " + std::to_string(ev.id));
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
    uint8_t version = dr.u8();
    std::vector<uint8_t> unpacked;
    if (version == 0xC4) {  // Wolf RPG Editor 3.5 compresses the rest
        unpacked = unpackLz4(dat, dr.pos());
        dr = Reader(unpacked);
        dr.seek(11);
    }
    Parser dp{dr, datEnc};
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
        // A type with no fields stores no bytes a row, so only a fixed cap bounds its row count.
        uint32_t rows = numbers + strings ? dp.r.count((numbers + strings) * 4) : dp.r.u32();
        if (rows > 100000) throw FormatError("database type with " + std::to_string(rows) + " rows");
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
    // The map version: 0x64 in Wolf RPG Editor 2 and 3, 0x65 and up in 3.5, which compresses the rest of the file.
    // From 0x67 a map has its own number of tile layers, and its commands end with a few bytes of their own.
    uint32_t version = r.u32();
    r.u8();
    std::vector<uint8_t> unpacked;
    if (version >= 0x65) {
        unpacked = unpackLz4(data, r.pos());
        r = Reader(unpacked);
        r.seek(25);
    }
    Parser p{r, enc, version >= 0x67};
    m.name = p.str();
    m.tileset = p.r.s32();
    m.width = p.r.s32();
    m.height = p.r.s32();
    uint32_t eventCount = p.r.u32();
    if (version >= 0x67) {
        p.r.u32();
        uint32_t layers = p.r.u32();
        if (layers < 1 || layers > 16) throw FormatError("map with " + std::to_string(layers) + " layers");
        m.layers = int(layers);
    }
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
    uint32_t count = p.r.count(36);  // two names and the autotile names at least
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

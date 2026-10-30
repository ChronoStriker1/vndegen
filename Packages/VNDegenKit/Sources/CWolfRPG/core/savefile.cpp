// Wolf RPG Editor save files. One visitor per section describes the layout once; an input and an output stream
// run it to parse and to write, so the two directions cannot drift apart.
#include "savefile.h"

#include "reader.h"

#include <algorithm>

namespace wolf {

namespace {

constexpr size_t kHeaderSize = 20;
constexpr uint8_t kMarker = 0x19;

// The C runtime's rand() that the engine uses for the XOR stream.
struct MsvcRand {
    uint32_t state;
    uint8_t next() {
        state = state * 214013u + 2531011u;
        return uint8_t(((state >> 16) & 0x7FFF) >> 12);
    }
};

class In {
public:
    static constexpr bool reading = true;
    In(std::span<const uint8_t> data, size_t pos) : r_(data) { r_.seek(pos); }
    uint16_t version = 0;

    void u8(uint8_t &v) { v = r_.u8(); }
    void u16(uint16_t &v) { v = r_.u16(); }
    void s32(int32_t &v) { v = r_.s32(); }
    void u32(uint32_t &v) { v = r_.u32(); }
    void u64(uint64_t &v) {
        uint64_t lo = r_.u32();
        v = lo | uint64_t(r_.u32()) << 32;
    }
    template <size_t N> void u8s(std::array<uint8_t, N> &a) { for (auto &v : a) u8(v); }
    template <size_t N> void u16s(std::array<uint16_t, N> &a) { for (auto &v : a) u16(v); }
    template <size_t N> void s32s(std::array<int32_t, N> &a) { for (auto &v : a) s32(v); }

    // A string with a u32 length, or a u16 length when `shortLength`.
    void str(SaveString &s, bool shortLength = false) {
        uint32_t len = shortLength ? r_.u16() : r_.u32();
        auto b = r_.bytes(len);
        s.terminated = len > 0 && b[len - 1] == 0;
        s.bytes.assign(reinterpret_cast<const char *>(b.data()), len - (s.terminated ? 1 : 0));
    }

    // Reads a count and sizes `v` to it. Each element takes at least `minSize` bytes, which bounds the count by what
    // is left of the file before anything is allocated.
    template <class T> void count32(std::vector<T> &v, size_t minSize) { sized(v, r_.u32(), minSize); }
    template <class T> void count16(std::vector<T> &v, size_t minSize) { sized(v, r_.u16(), minSize); }
    template <class T> void count8(std::vector<T> &v, size_t minSize) { sized(v, r_.u8(), minSize); }
    template <class T> void sized(std::vector<T> &v, uint32_t n, size_t minSize) {
        if (int32_t(n) < 0 || (minSize > 0 && n > (r_.size() - r_.pos()) / minSize))
            throw FormatError("bad count " + std::to_string(n) + " at " + std::to_string(r_.pos()));
        v.resize(n);
    }

    // Section 1's tile layers: a -1 in place of the first tile means none are stored.
    void tiles(SaveMap &m) {
        size_t at = r_.pos();
        if (r_.u32() == 0xFFFFFFFF) {
            m.hasTiles = false;
            m.tiles.clear();
            return;
        }
        r_.seek(at);
        m.hasTiles = true;
        if (m.width < 0 || m.height < 0) throw FormatError("bad map size");
        uint64_t n = 3 * uint64_t(m.width) * uint64_t(m.height);
        sized(m.tiles, n > 0x7FFFFFFF ? 0xFFFFFFFFu : uint32_t(n), 4);
        for (auto &t : m.tiles) s32(t);
    }

    size_t pos() const { return r_.pos(); }
    bool atEnd() const { return r_.atEnd(); }

private:
    Reader r_;
};

class Out {
public:
    static constexpr bool reading = false;
    std::vector<uint8_t> bytes;
    uint16_t version = 0;

    void u8(const uint8_t &v) { bytes.push_back(v); }
    void u16(const uint16_t &v) {
        bytes.push_back(uint8_t(v));
        bytes.push_back(uint8_t(v >> 8));
    }
    void u32(const uint32_t &v) {
        for (int i = 0; i < 4; i++) bytes.push_back(uint8_t(v >> (8 * i)));
    }
    void s32(const int32_t &v) { u32(uint32_t(v)); }
    void u64(const uint64_t &v) {
        u32(uint32_t(v));
        u32(uint32_t(v >> 32));
    }
    template <size_t N> void u8s(const std::array<uint8_t, N> &a) { for (auto v : a) u8(v); }
    template <size_t N> void u16s(const std::array<uint16_t, N> &a) { for (auto v : a) u16(v); }
    template <size_t N> void s32s(const std::array<int32_t, N> &a) { for (auto v : a) s32(v); }

    void str(const SaveString &s, bool shortLength = false) {
        size_t len = s.bytes.size() + (s.terminated ? 1 : 0);
        if (shortLength ? len > 0xFFFF : len > 0xFFFFFFFFu) throw FormatError("string too long to save");
        if (shortLength) u16(uint16_t(len));
        else u32(uint32_t(len));
        bytes.insert(bytes.end(), s.bytes.begin(), s.bytes.end());
        if (s.terminated) bytes.push_back(0);
    }

    template <class T> void count32(const std::vector<T> &v, size_t) { u32(checked(v.size(), 0x7FFFFFFF)); }
    template <class T> void count16(const std::vector<T> &v, size_t) { u16(uint16_t(checked(v.size(), 0xFFFF))); }
    template <class T> void count8(const std::vector<T> &v, size_t) { u8(uint8_t(checked(v.size(), 0xFF))); }

    void tiles(const SaveMap &m) {
        if (!m.hasTiles) {
            u32(0xFFFFFFFF);
            return;
        }
        if (m.width < 0 || m.height < 0 || m.tiles.size() != 3 * size_t(m.width) * size_t(m.height))
            throw FormatError("map tiles do not match the map size");
        for (auto t : m.tiles) s32(t);
    }

private:
    static uint32_t checked(size_t n, size_t max) {
        if (n > max) throw FormatError("list too long to save");
        return uint32_t(n);
    }
};

// Strings before version 0x6F that later became u32-length strings.
template <class IO, class S> void oldOrNewStr(IO &io, S &s) { io.str(s, io.version < 0x6F); }

template <class IO, class R> void route(IO &io, R &r) {
    io.u8s(r.flags);
    io.count32(r.steps, 3);
    if (r.steps.size() > 0x10000) throw FormatError("move route too long");
    for (auto &st : r.steps) {
        io.u8(st.id);
        io.count8(st.args, 4);
        for (auto &a : st.args) io.s32(a);
        io.count8(st.extra, 1);
        for (auto &e : st.extra) io.u8(e);
    }
}

template <class IO, class C> void character(IO &io, C &c) {
    io.s32(c.id);
    io.s32(c.x);
    io.s32(c.y);
    io.s32(c.unknown3);
    io.u8(c.unknown4);
    io.s32(c.unknown5);
    io.str(c.graphic, true);
    io.u16s(c.appearance);
    io.u8(c.unknown7);
    io.u8(c.hidden);
    route(io, c.route);
    route(io, c.route2);
    io.u16s(c.unknown9);
    io.u8s(c.unknown10);
    io.count32(c.selfVariables, 4);
    for (auto &v : c.selfVariables) io.s32(v);
    if (io.version >= 0x70) io.u8s(c.v70);
    if (io.version >= 0x73) io.s32s(c.v73);
    if (io.version >= 0x78) io.s32(c.v78);
    if (io.version >= 0x85) io.s32s(c.v85);
    if (io.version >= 0x8A) {
        io.count16(c.v8aStrings, 2);
        for (auto &s : c.v8aStrings) io.str(s, true);
        io.s32(c.v8a1);
        io.u8(c.v8a2);
        io.s32(c.v8a3);
        io.s32(c.v8a4);
    }
    if (io.version >= 0x8B) io.s32(c.v8b);
    if (io.version >= 0x8C) io.s32(c.v8c);
}

template <class IO, class M> void mapSection(IO &io, M &m) {
    io.s32(m.mapId);
    io.s32s(m.unknown1);
    if (io.version >= 0x69) {
        io.s32(m.width);
        io.s32(m.height);
        io.tiles(m);
    }
    io.str(m.tilesetBase);
    size_t n = io.version >= 0x8A ? 31 : io.version >= 0x73 ? 15 : 7;
    if constexpr (IO::reading) m.autotiles.resize(n);
    else if (m.autotiles.size() != n) throw FormatError("map section needs " + std::to_string(n) + " autotiles");
    for (auto &s : m.autotiles) io.str(s);
    if (io.version >= 0x64) {
        io.count32(m.tileFlags, 4);
        for (auto &v : m.tileFlags) io.s32(v);
    } else {
        io.count32(m.tileFlags, 1);
        for (auto &v : m.tileFlags) {
            uint8_t b = uint8_t(v);
            io.u8(b);
            if constexpr (IO::reading) v = b;
        }
    }
    io.count32(m.events, 40);
    for (auto &e : m.events) character(io, e);
    if (io.version >= 0x72) io.s32s(m.v72);
    io.count32(m.values2, 4);
    for (auto &v : m.values2) io.s32(v);
}

template <class IO, class S> void systemSection(IO &io, S &s) {
    io.u8s(s.a);
    io.s32s(s.b);
    io.u16s(s.c);
    io.s32(s.d);
    io.u16s(s.e);
    io.s32s(s.f);
    io.u8(s.g);
    io.u16(s.h);
    io.s32s(s.i);
    io.u16s(s.j);
    io.s32s(s.k);
    io.u8(s.l);
    if (io.version <= 96) io.s32(s.upTo96);
    io.s32(s.m);
    if (io.version >= 98) io.s32s(s.v98);
    if (io.version >= 100) {
        io.s32s(s.v100a);
        io.u8(s.v100b);
        io.count32(s.v100Strings, 4);
        for (auto &x : s.v100Strings) io.str(x);
        io.count32(s.v100Values, 4);
        for (auto &x : s.v100Values) io.s32(x);
        io.s32s(s.v100c);
    }
    if (io.version >= 101) io.s32s(s.v101);
    if (io.version >= 102) io.u16(s.v102);
    if (io.version >= 103) for (auto &x : s.v103) io.str(x, true);
    if (io.version >= 104) io.s32(s.frameCount);
    if (io.version >= 106) io.s32s(s.volumes);
    if (io.version >= 108) {
        io.str(s.v108s1, true);
        io.s32s(s.v108a);
        io.str(s.v108s2, true);
        io.s32s(s.v108b);
    }
    if (io.version >= 109) {
        for (auto &x : s.v109s) io.str(x, true);
        io.u8(s.v109);
    }
    if (io.version >= 110) io.u8(s.v110);
    if (io.version >= 119) for (auto &x : s.v119) io.str(x, true);
    if (io.version >= 121) io.s32(s.v121);
    if (io.version >= 122) io.s32(s.v122);
    if (io.version >= 124) io.s32(s.v124);
    if (io.version >= 126) io.s32(s.v126);
    if (io.version >= 128) io.s32(s.v128);
    if (io.version >= 129) io.s32s(s.v129);
    if (io.version >= 130) io.s32(s.v130);
    if (io.version >= 131) io.s32(s.v131);
    if (io.version >= 132) io.s32s(s.v132);
    if (io.version >= 134) io.u8(s.v134);
    if (io.version >= 136) io.u8(s.v136);
    if (io.version >= 137) io.s32s(s.v137);
    if (io.version >= 0x8A) {
        io.s32s(s.v8a);
        io.str(s.v8aString);
        io.count32(s.v8aStrings, 4);
        for (auto &x : s.v8aStrings) io.str(x);
    }
    if (io.version >= 0x8D) {
        io.u8s(s.v8d);
        io.u8s(s.v8dBytes);
        io.u8s(s.v8d2);
    }
    if (io.version >= 0x8E) io.s32(s.v8e);
}

template <class IO, class S> void variablesSection(IO &io, S &s) {
    io.s32(s.tickCount);
    io.count32(s.eventSelf, 4);
    for (auto &l : s.eventSelf) {
        io.count32(l, 1);
        for (auto &inner : l) {
            io.count8(inner, 4);
            for (auto &v : inner) io.s32(v);
        }
    }
    io.count32(s.numbers, 4);
    if (s.numbers.size() > 9999) throw FormatError("section 3: too many number lists");
    for (auto &l : s.numbers) {
        io.count32(l, 4);
        for (auto &v : l) io.s32(v);
    }
    io.count32(s.strings, 2);
    for (auto &x : s.strings) oldOrNewStr(io, x);
    io.count32(s.commonSelf, 1);
    if (s.commonSelf.size() > 10000) throw FormatError("section 3: too many lists");
    for (auto &l : s.commonSelf) {
        io.count8(l, 4);
        for (auto &v : l) io.s32(v);
    }
    io.count32(s.commonSelfStrings, 1);
    if (s.commonSelfStrings.empty() || s.commonSelfStrings.size() > 10000)
        throw FormatError("section 3: bad string list count");
    for (auto &l : s.commonSelfStrings) {
        io.count8(l, 2);
        for (auto &x : l) oldOrNewStr(io, x);
    }
}

template <class IO, class P> void partySection(IO &io, P &p) {
    character(io, p.hero);
    io.count32(p.members, 40);
    for (auto &m : p.members) character(io, m);
    io.u8s(p.unknown1);
    io.count32(p.trail, 4);
    for (auto &v : p.trail) io.s32(v);
    if (io.version >= 0x8A) {
        io.count32(p.v8a, 8);
        for (auto &v : p.v8a) io.u64(v);
    }
}

template <class IO, class P> void picture(IO &io, P &p) {
    io.s32(p.number);
    io.u8s(p.unknown2);
    io.u16(p.unknown3);
    io.u8s(p.kind);
    io.str(p.file, true);
    io.s32s(p.a);
    io.s32s(p.b);
    if (io.version >= 0x69) io.s32s(p.v69);
    if (io.version >= 0x6B) io.u8(p.v6b);
    if (io.version >= 0x72) {
        io.u8s(p.v72a);
        io.s32s(p.v72b);
    }
    if (io.version >= 0x73) {
        io.s32s(p.v73a);
        if (io.version >= 0x74) io.s32s(p.v74);
        if (io.version >= 0x75) io.s32s(p.v75);
        io.s32s(p.v73b);
    }
    if (io.version >= 0x76) io.s32s(p.v76);
    if (io.version < 0x81) return;
    io.s32s(p.v81);
    if (io.version >= 0x87) io.s32s(p.v87);
    if (io.version >= 0x89) {
        io.count32(p.v89Lists, 4);
        for (auto &l : p.v89Lists) {
            io.count32(l, 4);
            for (auto &v : l) io.s32(v);
        }
        io.s32s(p.v89);
    }
}

template <class IO, class P> void pictures(IO &io, P &p) {
    io.u16(p.count);
    if (p.count & 0x8000) {
        if constexpr (IO::reading) p.pictures.clear();
        else if (!p.pictures.empty()) throw FormatError("picture count has bit 15 set but lists pictures");
        return;
    }
    if constexpr (IO::reading) io.sized(p.pictures, p.count, 60);
    else if (p.pictures.size() != p.count) throw FormatError("picture count does not match the list");
    for (auto &pic : p.pictures) picture(io, pic);
}

template <class IO, class D> void variableDb(IO &io, D &db) {
    io.u8(db.unknown);
    io.count32(db.types, 8);
    for (auto &t : db.types) {
        io.s32(t.head);
        if (t.head <= -2) io.s32(t.idMethod);
        if (t.head <= -1) io.count32(t.fieldKinds, 4);
        else {
            if constexpr (IO::reading) io.sized(t.fieldKinds, uint32_t(t.head), 4);
            else if (t.fieldKinds.size() != size_t(t.head)) throw FormatError("type field count mismatch");
        }
        for (auto &k : t.fieldKinds) io.u32(k);
        size_t numbers = 0;
        for (auto k : t.fieldKinds) numbers += k < 2000;
        size_t strings = t.fieldKinds.size() - numbers;
        // A row of a type with no fields takes no bytes, so its count is bounded by the file's size instead.
        io.count32(t.rows, t.fieldKinds.empty() ? 1 : 4);
        for (auto &row : t.rows) {
            if constexpr (IO::reading) {
                row.numbers.resize(numbers);
                row.strings.resize(strings);
            } else if (row.numbers.size() != numbers || row.strings.size() != strings) {
                throw FormatError("database row does not match its type's fields");
            }
            for (auto &v : row.numbers) io.s32(v);
            for (auto &s : row.strings) io.str(s);
        }
    }
}

template <class IO, class S> void section7(IO &io, S &s) {
    io.u8(s.present);
    if (s.present != 1) return;
    io.count32(s.entries, 2);
    for (auto &e : s.entries) {
        io.u8(e.kind);
        if (e.kind < 0xFA) io.u8(e.small);
        else io.u32(e.value);
    }
}

template <class IO, class F> void body(IO &io, F &f) {
    io.u8(f.startMarker);
    if (f.startMarker != kMarker) throw FormatError("bad save start marker");
    io.str(f.title, true);
    io.u16(f.version);
    io.version = f.version;
    mapSection(io, f.map);
    systemSection(io, f.system);
    variablesSection(io, f.variables);
    partySection(io, f.party);
    pictures(io, f.pictures);
    variableDb(io, f.variableDb);
    section7(io, f.section7);
    io.u8(f.endMarker);
    if (f.endMarker != kMarker) throw FormatError("bad save end marker");
}

}  // namespace

void cryptSave(std::span<uint8_t> b) {
    // Three passes, each seeded by a header byte, over every byte, every second byte and every fifth byte.
    const std::pair<size_t, size_t> passes[] = {{0, 1}, {3, 2}, {9, 5}};
    if (b.size() <= kHeaderSize) return;
    for (auto [seedAt, step] : passes) {
        MsvcRand rng{b[seedAt]};
        for (size_t i = kHeaderSize; i < b.size(); i += step) b[i] ^= rng.next();
    }
}

SaveFile parseSave(std::span<const uint8_t> fileBytes) {
    if (fileBytes.size() < kHeaderSize + 1) throw FormatError("save too short");
    std::vector<uint8_t> plain(fileBytes.begin(), fileBytes.end());
    cryptSave(plain);
    SaveFile f;
    std::copy_n(plain.begin(), kHeaderSize, f.header.begin());
    uint8_t sum = 0;
    for (size_t i = kHeaderSize; i < plain.size(); i++) sum += plain[i];
    if (sum != f.header[2]) throw FormatError("save checksum mismatch");
    In in(plain, kHeaderSize);
    body(in, f);
    if (!in.atEnd()) throw FormatError("trailing bytes after save end marker at " + std::to_string(in.pos()));
    return f;
}

std::vector<uint8_t> serializeSave(const SaveFile &save) {
    Out out;
    out.bytes.assign(save.header.begin(), save.header.end());
    body(out, save);
    uint8_t sum = 0;
    for (size_t i = kHeaderSize; i < out.bytes.size(); i++) sum += out.bytes[i];
    out.bytes[2] = sum;
    cryptSave(out.bytes);
    return std::move(out.bytes);
}

}  // namespace wolf

#include "exfile.h"

#include <cstring>
#include <fstream>
#include <zlib.h>

namespace s4 {

void exUnscramble(uint8_t *d, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned b = d[i];
        int ones = __builtin_popcount(b);
        if (ones == 0 || ones == 8) continue;
        int k = 8 - ones;
        d[i] = uint8_t(ones % 2 == 0 ? (b << k) | (b >> (8 - k)) : (b >> k) | (b << (8 - k)));
    }
}

namespace {

constexpr int kMaxDepth = 64;  // nested tables and tree levels

// Reads the plain data. Every read is bounds-checked; a damaged file fails the load rather than reading past it.
struct Reader {
    const std::vector<uint8_t> &d;
    size_t p = 0;
    bool bad = false;

    uint32_t u32() {
        if (d.size() - p < 4) { bad = true; p = d.size(); return 0; }
        uint32_t v;
        std::memcpy(&v, d.data() + p, 4);
        p += 4;
        return v;
    }
    float f32() {
        uint32_t u = u32();
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }
    // A string: u32 length, then that many bytes (zero padded to 4); the text ends at the first NUL.
    std::string str() {
        uint32_t n = u32();
        if (n > d.size() - p) { bad = true; p = d.size(); return {}; }
        std::string s(reinterpret_cast<const char *>(d.data() + p), n);
        p += n;
        s.resize(std::strlen(s.c_str()));
        return s;
    }
    // A count of items at least `size` bytes each.
    uint32_t count(size_t size) {
        uint32_t n = u32();
        if (n > (d.size() - p) / size) { bad = true; p = d.size(); return 0; }
        return n;
    }
};

bool scalar(Reader &r, int type, ExValue &v) {
    v.type = type;
    switch (type) {
    case ExValue::Int: v.i = int32_t(r.u32()); return true;  // stored plain; the PC engine XORs it only in memory
    case ExValue::Float: v.f = r.f32(); return true;
    case ExValue::String: v.s = r.str(); return true;
    default: return false;
    }
}

bool header(Reader &r, std::vector<ExColumn> &cols, int depth) {
    if (depth > kMaxDepth) return false;
    uint32_t n = r.count(16);
    for (uint32_t k = 0; k < n && !r.bad; k++) {
        ExColumn c;
        c.type = int(r.u32());
        c.name = r.str();
        bool hasDefault = r.u32() == 1;
        c.key = r.u32() == 1;
        if (hasDefault) {
            ExValue unused;
            if (!scalar(r, c.type, unused)) return false;
        }
        if (c.type == ExValue::Table && !header(r, c.sub, depth + 1)) return false;
        cols.push_back(std::move(c));
    }
    return !r.bad;
}

bool rows(Reader &r, ExTable &t, const std::vector<ExColumn> &cols, int depth) {
    if (depth > kMaxDepth) return false;
    t.columns = &cols;
    uint32_t nc = r.u32();
    uint32_t nr = r.count(4);
    if (nc != cols.size() && nr > 0) return false;
    for (uint32_t row = 0; row < nr && !r.bad; row++) {
        std::vector<std::unique_ptr<ExValue>> cells;
        for (uint32_t c = 0; c < nc && !r.bad; c++) {
            auto v = std::make_unique<ExValue>();
            int type = int(r.u32());
            if (type == ExValue::Table) {
                v->type = type;
                if (!rows(r, v->table, cols[c].sub, depth + 1)) return false;
            } else if (!scalar(r, type, *v)) {
                return false;
            }
            cells.push_back(std::move(v));
        }
        t.rows.push_back(std::move(cells));
    }
    return !r.bad;
}

bool entry(Reader &r, int type, ExValue &v, int depth);

bool node(Reader &r, ExNode &n, int depth) {
    if (depth > kMaxDepth) return false;
    n.name = r.str();
    if (r.u32() == 1) {  // the node holds a value: its type, its size, then an entry
        int type = int(r.u32());
        r.u32();
        n.value = std::make_unique<ExValue>();
        if (!entry(r, type, *n.value, depth + 1)) return false;
    }
    uint32_t children = r.count(8);
    for (uint32_t k = 0; k < children && !r.bad; k++) {
        auto c = std::make_unique<ExNode>();
        if (!node(r, *c, depth + 1)) return false;
        n.children.push_back(std::move(c));
    }
    return !r.bad;
}

bool entry(Reader &r, int type, ExValue &v, int depth) {
    v.name = r.str();
    v.type = type;
    switch (type) {
    case ExValue::Int:
    case ExValue::Float:
    case ExValue::String:
        return scalar(r, type, v) && !r.bad;
    case ExValue::Table:
        return header(r, v.columns, depth) && rows(r, v.table, v.columns, depth);
    case ExValue::List: {
        uint32_t n = r.count(12);
        for (uint32_t k = 0; k < n && !r.bad; k++) {
            int itemType = int(r.u32());
            r.u32();  // the item's size
            auto item = std::make_unique<ExValue>();
            if (!scalar(r, itemType, *item)) return false;
            v.list.push_back(std::move(item));
        }
        return !r.bad;
    }
    case ExValue::Tree:
        v.tree = std::make_unique<ExNode>();
        return node(r, *v.tree, depth);
    default:
        return false;
    }
}

}  // namespace

bool ExFile::load(const std::string &path, std::string &error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "can't open " + path; return false; }
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)), {});
    auto u32at = [&](size_t at) { uint32_t v; std::memcpy(&v, raw.data() + at, 4); return v; };
    if (raw.size() < 32 || std::memcmp(raw.data(), "HEAD", 4) || std::memcmp(raw.data() + 8, "EXTF", 4)) {
        error = "not an .ex file";
        return false;
    }
    size_t at = 8 + u32at(4);
    if (at > raw.size() - 12 || std::memcmp(raw.data() + at, "DATA", 4)) { error = "no DATA chunk"; return false; }
    uint32_t csize = u32at(at + 4), usize = u32at(at + 8);
    if (csize > raw.size() - at - 12 || uint64_t(usize) > uint64_t(csize) * 1032 + 64) {
        error = "damaged DATA chunk";
        return false;
    }
    uint8_t *packed = raw.data() + at + 12;
    exUnscramble(packed, csize);
    std::vector<uint8_t> plain(usize);
    uLongf out = usize;
    if (uncompress(plain.data(), &out, packed, csize) != Z_OK || out != usize) { error = "damaged DATA chunk"; return false; }

    Reader r{plain};
    while (r.p < plain.size()) {
        int type = int(r.u32());
        uint32_t size = r.u32();
        size_t start = r.p;
        if (r.bad || size > plain.size() - start) { error = "damaged entry"; return false; }
        auto v = std::make_unique<ExValue>();
        if (!entry(r, type, *v, 0) || r.p != start + size) { error = "damaged entry " + v->name; return false; }
        byName_.emplace(v->name, v.get());
        entries_.push_back(std::move(v));
    }
    return true;
}

const ExValue *ExFile::find(const std::string &name) const {
    auto it = byName_.find(name);
    return it == byName_.end() ? nullptr : it->second;
}

}  // namespace s4

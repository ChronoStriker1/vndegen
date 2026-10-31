#include "save.h"

#include <cstring>
#include <filesystem>
#include <functional>
#include <fstream>
#include <system_error>
#include <zlib.h>

#include "sjis.h"
#include "vm.h"

namespace fs = std::filesystem;

namespace s4::save {

// ---- the "GD" container ----
// "GD", two bytes 1 1, the plain length (u32), then zlib data (level 9) XORed with the low byte of each output of a
// Mersenne Twister. The generator is seeded the old way (each state word the previous times 69069) from 0x12320F.
// Worked out from saves the SDK runtime and Test Game 46 wrote, and the SDK runtime's routine that writes the header.
// Test Game 19's VM (a 2009 Sys42VM) writes the zlib data without the XOR; see VM::SaveFormat.

namespace {

constexpr uint32_t kGDSeed = 0x12320F;

}  // namespace

void mtXor(uint8_t *p, size_t n, uint32_t seed) {
    uint32_t mt[624];
    mt[0] = seed;
    for (int i = 1; i < 624; i++) mt[i] = mt[i - 1] * 69069u;
    int idx = 624;
    for (size_t k = 0; k < n; k++) {
        if (idx >= 624) {
            for (int i = 0; i < 624; i++) {
                uint32_t y = (mt[i] & 0x80000000u) | (mt[(i + 1) % 624] & 0x7FFFFFFFu);
                mt[i] = mt[(i + 397) % 624] ^ (y >> 1) ^ ((y & 1) ? 0x9908B0DFu : 0);
            }
            idx = 0;
        }
        uint32_t y = mt[idx++];
        y ^= y >> 11;
        y ^= (y << 7) & 0x9D2C5680u;
        y ^= (y << 15) & 0xEFC60000u;
        y ^= y >> 18;
        p[k] ^= uint8_t(y);
    }
}

bool readGD(const std::string &path, std::vector<uint8_t> &plain) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
    if (d.size() < 8 || d[0] != 'G' || d[1] != 'D') return false;
    uint32_t len;
    std::memcpy(&len, d.data() + 4, 4);
    if (uint64_t(len) > uint64_t(d.size() - 8) * 1032 + 64) return false;  // more than zlib can expand to
    plain.resize(len);
    auto inflate = [&] {
        uLongf out = len;
        return uncompress(plain.data(), &out, d.data() + 8, uLong(d.size() - 8)) == Z_OK && out == len;
    };
    // Plain zlib first: XORed data never inflates cleanly, since its header bytes are scrambled.
    if (inflate()) return true;
    mtXor(d.data() + 8, d.size() - 8, kGDSeed);
    return inflate();
}

bool writeGD(const std::string &path, const std::vector<uint8_t> &plain, bool xorData) {
    if (path.empty()) return false;  // hostPath refused it: nothing, not even the temporary file, is written
    uLongf cap = compressBound(uLong(plain.size()));
    std::vector<uint8_t> d(8 + cap);
    if (compress2(d.data() + 8, &cap, plain.data(), uLong(plain.size()), 9) != Z_OK) return false;
    d.resize(8 + cap);
    d[0] = 'G'; d[1] = 'D'; d[2] = 1; d[3] = 1;
    uint32_t len = uint32_t(plain.size());
    std::memcpy(d.data() + 4, &len, 4);
    if (xorData) mtXor(d.data() + 8, cap, kGDSeed);
    // Write beside the target, then rename, so a crash never leaves half a save.
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f.write(reinterpret_cast<const char *>(d.data()), std::streamsize(d.size()))) return false;
    }
    fs::rename(tmp, path, ec);
    return !ec;
}

namespace {
// The path with each part matched case-insensitively, as on Windows, so a save the PC wrote as FOO.ASD loads when
// the game asks for foo.asd.
std::string caseless(const fs::path &base, const std::string &rel) {
    fs::path cur(base);
    for (const auto &part : fs::path(rel)) {
        std::string ps = part.string();
        if (ps.empty() || ps == ".") continue;
        fs::path exact = cur / ps;
        std::error_code ec;
        if (!fs::exists(exact, ec)) {
            std::string low = ps;
            for (char &ch : low) ch = char(std::tolower(static_cast<unsigned char>(ch)));
            for (auto &e : fs::directory_iterator(cur, ec)) {
                std::string n = e.path().filename().string();
                for (char &ch : n) ch = char(std::tolower(static_cast<unsigned char>(ch)));
                if (n == low) { exact = e.path(); break; }
            }
        }
        cur = exact;
    }
    return cur.string();
}
}  // namespace

std::string hostPath(VM &vm, const std::string &name, bool inSaveFolder) {
    std::string p = sjis::toUtf8(name);
    for (char &c : p) if (c == '\\') c = '/';
    // Game code only reaches its own folder and the save folder. An absolute path (the game folder, which
    // SystemService.GetGameFolderPath hands out) must lie in one of them, and a relative one may not climb out.
    // Anything else becomes an empty path, which every file operation fails on.
    auto inside = [](const fs::path &path, const std::string &base) {
        fs::path rel = path.lexically_normal().lexically_relative(fs::path(base).lexically_normal());
        return !rel.empty() && *rel.begin() != "..";
    };
    if (!p.empty() && p[0] == '/') {
        fs::path abs = fs::path(p).lexically_normal();
        return inside(abs, vm.rootDir) || inside(abs, vm.saveDir) ? abs.string() : std::string();
    }
    fs::path relative = fs::path(p).lexically_normal();
    if (!relative.empty() && *relative.begin() == "..") return {};
    p = relative.string();
    // A path that starts with the save folder's name, as system.GetSaveFolderName returns it, is in the save folder.
    std::string first = p.substr(0, p.find('/'));
    std::string lowFirst = first, lowSave = vm.saveFolderName;
    for (char &ch : lowFirst) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    for (char &ch : lowSave) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    if (!lowSave.empty() && lowFirst == lowSave && first.size() < p.size()) return caseless(vm.saveDir, p.substr(first.size() + 1));
    if (inSaveFolder) return caseless(vm.saveDir, p);
    return caseless(vm.rootDir, p);
}

// ---- global and group saves ----
//
// The plain text: the key, then a 56-byte header (1000, the layout version 5, 56, the number of globals in the
// program, and (offset, count) for five sections), the group name, then the sections:
//   records      u32 kind, name, u32 n, n value indexes. Record 0 (kind 1000, no name) lists the saved globals;
//                the rest are struct instances (kind 13, the struct's name) listing their members' values.
//   globals      u32 type, u32 value, name, u32 1 for each saved global.
//   strings      every string value, NUL terminated.
//   arrays       i32 rank (-1, then 0, when not allocated), each dimension's length innermost first, the number
//                of innermost rows, then each row: its length and (value, type) per element. Test Game 46's PC
//                showed the dimensions: a save with the rank where the outer length belongs loaded as 2 rows.
//   struct values u32 type, u32 value, name for each struct member.
// A string's value is its index in the string section, an array's its index in the array section and a struct's its
// record. Everything is written depth first, children before parents. Worked out from saves the SDK runtime wrote
// for test programs with every kind of global, and checked against Test Game 46's own saves.

namespace {

struct Writer {
    VM &vm;
    std::vector<std::vector<uint8_t>> records;   // record 0 first
    std::vector<uint8_t> globals, strings, arrays, structVals;
    int nStrings = 0, nArrays = 0, nStructVals = 0;

    static void u32(std::vector<uint8_t> &b, uint32_t v) { b.insert(b.end(), reinterpret_cast<uint8_t *>(&v), reinterpret_cast<uint8_t *>(&v) + 4); }
    static void cstr(std::vector<uint8_t> &b, const std::string &s) { b.insert(b.end(), s.begin(), s.end()); b.push_back(0); }

    int32_t addString(int32_t slot) {
        cstr(strings, vm.str(slot));
        return nStrings++;
    }

    // The saved value of one variable of type t holding v.
    int32_t value(const VarType &t, int32_t v) {
        switch (t.type) {
        case T_STRING: return addString(v);
        case T_STRUCT: return addStruct(v);
        default:
            if (t.type >= T_ARRAY_INT && t.type <= T_ARRAY_STRUCT) return addArray(v, t);
            if (t.type == T_ARRAY_BOOL || t.type == T_ARRAY_FUNCTYPE || t.type == 58 || t.type == 67) return addArray(v, t);
            if (t.type >= T_REF_INT && t.type <= T_REF_ARRAY_STRUCT) return -1;  // references are saved as NULL
            return v;
        }
    }

    int32_t addStruct(int32_t slot) {
        Page *p = vm.page(slot);
        if (!p || p->kind != PageKind::Struct) return -1;
        const Struct &st = vm.ain.structs[p->index];
        std::vector<int32_t> idx;
        for (size_t i = 0; i < st.members.size() && i < p->v.size(); i++) {
            const VarType &mt = st.members[i].t;
            int32_t val = value(mt, vm.page(slot)->v[i]);
            u32(structVals, uint32_t(mt.type));
            u32(structVals, uint32_t(val));
            cstr(structVals, st.members[i].name);
            idx.push_back(nStructVals++);
        }
        std::vector<uint8_t> r;
        u32(r, T_STRUCT);
        cstr(r, st.name);
        u32(r, uint32_t(idx.size()));
        for (int32_t i : idx) u32(r, uint32_t(i));
        records.push_back(std::move(r));
        return int32_t(records.size() - 1);
    }

    // Appends the innermost rows of an array of this rank, in order.
    void rows(std::vector<uint8_t> &out, int32_t slot, int rank, uint32_t &nRows) {
        Page *a = vm.page(slot);
        size_t n = a ? a->v.size() : 0;
        if (rank > 1) {
            for (size_t i = 0; i < n; i++) rows(out, vm.page(slot)->v[i], rank - 1, nRows);
            return;
        }
        nRows++;
        u32(out, uint32_t(n));
        for (size_t i = 0; i < n; i++) {
            Page &ap = *vm.page(slot);
            VarType et{ap.elemType, ap.elemStruct, 0};
            int32_t val = value(et, ap.v[i]);
            u32(out, uint32_t(val));
            u32(out, uint32_t(et.type));
        }
    }

    int32_t addArray(int32_t slot, const VarType &t) {
        Page *a = vm.page(slot);
        std::vector<uint8_t> rec;
        if (!a || a->v.empty()) {
            u32(rec, uint32_t(-1));
            u32(rec, 0);
        } else {
            int rank = std::max(1, a->rank);
            // Each dimension's length, read down the first elements, innermost first.
            std::vector<uint32_t> dims;
            for (Page *d = a; d && int(dims.size()) < rank; d = d->v.empty() ? nullptr : vm.page(d->v[0]))
                dims.insert(dims.begin(), uint32_t(d->v.size()));
            while (int(dims.size()) < rank) dims.insert(dims.begin(), 0);
            u32(rec, uint32_t(rank));
            for (uint32_t d : dims) u32(rec, d);
            std::vector<uint8_t> body;
            uint32_t nRows = 0;
            rows(body, slot, rank, nRows);
            u32(rec, nRows);
            rec.insert(rec.end(), body.begin(), body.end());
        }
        (void)t;
        arrays.insert(arrays.end(), rec.begin(), rec.end());
        return nArrays++;
    }
};

}  // namespace

int32_t globalSave(VM &vm, const std::string &key, const std::string &file, const std::string &group, int32_t *count) {
    int groupIndex = -1;
    if (!group.empty()) {
        for (size_t i = 0; i < vm.ain.objGroups.size(); i++)
            if (vm.ain.objGroups[i] == group) groupIndex = int(i);
        // A group the program doesn't declare saves no globals; the file is still written.
        if (groupIndex < 0) groupIndex = int(vm.ain.objGroups.size());
    }
    Writer w{vm};
    w.records.emplace_back();  // record 0, filled in below
    std::vector<int32_t> saved;
    Page *gp = vm.page(vm.globalPage);
    for (size_t i = 0; i < vm.ain.globals.size(); i++) {
        const Global &g = vm.ain.globals[i];
        if (groupIndex >= 0 && g.group != groupIndex) continue;
        int32_t val = w.value(g.t, gp->v[i]);
        Writer::u32(w.globals, uint32_t(g.t.type));
        Writer::u32(w.globals, uint32_t(val));
        Writer::cstr(w.globals, g.name);
        Writer::u32(w.globals, 1);
        saved.push_back(int32_t(saved.size()));
        gp = vm.page(vm.globalPage);
    }
    auto &r0 = w.records[0];
    Writer::u32(r0, 1000);
    r0.push_back(0);
    Writer::u32(r0, uint32_t(saved.size()));
    for (int32_t i : saved) Writer::u32(r0, uint32_t(i));

    std::vector<uint8_t> out;
    Writer::cstr(out, key);
    size_t keyLen = out.size();
    size_t headerAt = out.size();
    out.resize(out.size() + 56);
    Writer::cstr(out, group);
    uint32_t recordsOff = uint32_t(out.size());
    for (auto &r : w.records) out.insert(out.end(), r.begin(), r.end());
    uint32_t globalsOff = uint32_t(out.size());
    out.insert(out.end(), w.globals.begin(), w.globals.end());
    uint32_t stringsOff = uint32_t(out.size());
    out.insert(out.end(), w.strings.begin(), w.strings.end());
    uint32_t arraysOff = uint32_t(out.size());
    out.insert(out.end(), w.arrays.begin(), w.arrays.end());
    uint32_t structOff = uint32_t(out.size());
    out.insert(out.end(), w.structVals.begin(), w.structVals.end());
    // Layout 5, the one below. Test Game 19's Sys42VM always writes it; Test Game 46's Sys43VM writes it for a
    // version 5 code file and a different layout, 7, otherwise (both read from the VMs' save routines).
    uint32_t hdr[14] = {1000, 5, 56, uint32_t(vm.ain.globals.size()),
                        recordsOff, uint32_t(w.records.size()), globalsOff, uint32_t(saved.size()),
                        stringsOff, uint32_t(w.nStrings), arraysOff, uint32_t(w.nArrays), structOff, uint32_t(w.nStructVals)};
    std::memcpy(out.data() + headerAt, hdr, 56);
    (void)keyLen;
    if (count) *count = int32_t(saved.size());
    return writeGD(hostPath(vm, file, true), out, vm.saveFormat.xorData) ? 1 : 0;
}

namespace {

constexpr int32_t kMaxRank = 16;           // far above any game's arrays
constexpr size_t kMaxArrayPages = 1 << 20;  // arrays one load may make; empty dimensions cost no bytes in the file

struct Reader {
    const std::vector<uint8_t> &d;
    bool bad = false;
    uint32_t u32(size_t &i) {
        if (i + 4 > d.size()) { bad = true; return 0; }
        uint32_t v;
        std::memcpy(&v, d.data() + i, 4);
        i += 4;
        return v;
    }
    std::string cstr(size_t &i) {
        size_t j = i;
        while (j < d.size() && d[j]) j++;
        if (j >= d.size()) { bad = true; return {}; }
        std::string s(reinterpret_cast<const char *>(d.data() + i), j - i);
        i = j + 1;
        return s;
    }
};

struct Loaded {
    struct Val { int32_t type, value; std::string name; };
    struct Rec { int32_t kind; std::string name; std::vector<int32_t> idx; };
    std::vector<Rec> records;
    std::vector<Val> globals, structVals;
    std::vector<std::string> strings;
    std::vector<size_t> arrayAt;   // offset of each array record
    const std::vector<uint8_t> *data = nullptr;
};

bool parse(const std::vector<uint8_t> &d, Loaded &L, std::string &key, std::string &group) {
    Reader r{d};
    size_t i = 0;
    key = r.cstr(i);
    size_t h = i;
    uint32_t hdr[14];
    for (auto &x : hdr) x = r.u32(h);
    if (r.bad || hdr[0] != 1000) return false;
    if (hdr[1] == 7) return false;  // layout 7 (struct definitions at the end): no test game writes it
    size_t g = i + 56;
    group = r.cstr(g);
    size_t p = hdr[4];
    for (uint32_t k = 0; k < hdr[5] && !r.bad; k++) {
        Loaded::Rec rec;
        rec.kind = int32_t(r.u32(p));
        rec.name = r.cstr(p);
        uint32_t n = r.u32(p);
        for (uint32_t m = 0; m < n && !r.bad; m++) rec.idx.push_back(int32_t(r.u32(p)));
        L.records.push_back(std::move(rec));
    }
    p = hdr[6];
    for (uint32_t k = 0; k < hdr[7] && !r.bad; k++) {
        Loaded::Val v;
        v.type = int32_t(r.u32(p));
        v.value = int32_t(r.u32(p));
        v.name = r.cstr(p);
        r.u32(p);
        L.globals.push_back(std::move(v));
    }
    p = hdr[8];
    for (uint32_t k = 0; k < hdr[9] && !r.bad; k++) L.strings.push_back(r.cstr(p));
    // Array records vary in length, so walk them to find each one's start.
    p = hdr[10];
    for (uint32_t k = 0; k < hdr[11] && !r.bad; k++) {
        L.arrayAt.push_back(p);
        int32_t rank = int32_t(r.u32(p));
        if (rank < 0) { r.u32(p); continue; }
        if (rank > kMaxRank) { r.bad = true; break; }
        p += 4 * size_t(rank);  // dimensions
        uint32_t nRows = r.u32(p);
        for (uint32_t row = 0; row < nRows && !r.bad; row++) {
            p += 8 * size_t(r.u32(p));
            if (p > d.size()) r.bad = true;  // a truncated last row must fail here, before anything is applied
        }
    }
    p = hdr[12];
    for (uint32_t k = 0; k < hdr[13] && !r.bad; k++) {
        Loaded::Val v;
        v.type = int32_t(r.u32(p));
        v.value = int32_t(r.u32(p));
        v.name = r.cstr(p);
        L.structVals.push_back(std::move(v));
    }
    L.data = &d;
    return !r.bad;
}

// Walks what Applier would fill without touching the VM: a record or array that refers back to one being filled,
// or more arrays than kMaxArrayPages, fails the load before any global changes.
struct Checker {
    VM &vm;
    Loaded &L;
    std::vector<char> recordOpen = std::vector<char>(L.records.size()), arrayOpen = std::vector<char>(L.arrayAt.size());
    size_t pages = 0;
    bool failed = false;

    void value(const VarType &t, int32_t v) {
        if (failed) return;
        if (t.type == T_STRUCT) record(t.structType, v);
        else if (isArray(t.type)) array(arrayElem(t.type) == T_STRUCT ? t.structType : -1, v);
    }
    void record(int32_t structType, int32_t index) {
        if (index < 0 || size_t(index) >= L.records.size() || structType < 0 || size_t(structType) >= vm.ain.structs.size()) return;
        if (recordOpen[size_t(index)]) { failed = true; return; }
        recordOpen[size_t(index)] = 1;
        const Struct &st = vm.ain.structs[size_t(structType)];
        for (int32_t vi : L.records[size_t(index)].idx) {
            if (vi < 0 || size_t(vi) >= L.structVals.size()) continue;
            for (const Variable &m : st.members)
                if (m.name == L.structVals[size_t(vi)].name) {
                    value(m.t, L.structVals[size_t(vi)].value);
                    break;
                }
        }
        recordOpen[size_t(index)] = 0;
    }
    // elemStruct is the element struct type, or -1 when the elements aren't structs.
    void array(int32_t elemStruct, int32_t index) {
        if (index < 0 || size_t(index) >= L.arrayAt.size()) return;
        if (arrayOpen[size_t(index)]) { failed = true; return; }
        arrayOpen[size_t(index)] = 1;
        Reader r{*L.data};
        size_t p = L.arrayAt[size_t(index)];
        int32_t rank = int32_t(r.u32(p));
        if (rank > kMaxRank) { failed = true; return; }
        if (rank > 0) {
            std::vector<uint32_t> dims(static_cast<size_t>(rank), 0);
            for (auto &d : dims) d = r.u32(p);
            r.u32(p);
            // Applier makes one array per node: the outer dimensions' running products.
            uint64_t level = 1;
            for (int k = rank - 1; k >= 1 && !failed; k--) {
                level *= dims[size_t(k)];
                pages += size_t(std::min<uint64_t>(level, kMaxArrayPages + 1));
                if (level > kMaxArrayPages || pages > kMaxArrayPages) failed = true;
            }
            // The innermost rows, in order, with their struct elements.
            for (uint64_t row = 0; row < level && !failed && !r.bad; row++) {
                if (++pages > kMaxArrayPages) { failed = true; break; }
                uint32_t n = r.u32(p);
                for (uint32_t k = 0; k < n && !r.bad && !failed; k++) {
                    int32_t val = int32_t(r.u32(p));
                    r.u32(p);
                    if (elemStruct >= 0) record(elemStruct, val);
                }
            }
        }
        arrayOpen[size_t(index)] = 0;
    }
};

// Puts a saved value into a variable of the program's type, matching struct members by name.
struct Applier {
    VM &vm;
    Loaded &L;
    // A damaged save's records or arrays can refer back to one being filled, or ask for absurd numbers of arrays.
    // Either fails the load; nothing is skipped silently.
    std::vector<char> recordOpen = std::vector<char>(L.records.size()), arrayOpen = std::vector<char>(L.arrayAt.size());
    size_t pages = 0;
    bool failed = false;

    void store(int32_t &dst, const VarType &t, int32_t savedType, int32_t value) {
        if (failed) return;
        if (t.type == T_STRING) {
            if (!vm.valid(dst)) dst = vm.newString();
            vm.str(dst) = value >= 0 && size_t(value) < L.strings.size() ? L.strings[value] : std::string();
        } else if (t.type == T_STRUCT) {
            if (!vm.valid(dst)) dst = vm.newStruct(t.structType);
            if (value >= 0 && size_t(value) < L.records.size()) {
                if (recordOpen[size_t(value)]) { failed = true; return; }
                recordOpen[size_t(value)] = 1;
                fillStruct(dst, L.records[value]);
                recordOpen[size_t(value)] = 0;
            }
        } else if ((t.type >= T_ARRAY_INT && t.type <= T_ARRAY_STRUCT) || t.type == T_ARRAY_BOOL || t.type == T_ARRAY_FUNCTYPE ||
                   t.type == 58 || t.type == 67) {
            fillArray(dst, t, value);
        } else if (t.type >= T_REF_INT && t.type <= T_REF_ARRAY_STRUCT) {
            // references come back as NULL
        } else {
            (void)savedType;
            dst = value;
        }
    }

    void fillStruct(int32_t slot, const Loaded::Rec &rec) {
        Page *p = vm.page(slot);
        if (!p || p->kind != PageKind::Struct) return;
        const Struct &st = vm.ain.structs[p->index];
        for (int32_t vi : rec.idx) {
            if (vi < 0 || size_t(vi) >= L.structVals.size()) continue;
            const auto &sv = L.structVals[vi];
            for (size_t m = 0; m < st.members.size(); m++) {
                if (st.members[m].name != sv.name) continue;
                int32_t v = vm.page(slot)->v[m];
                store(v, st.members[m].t, sv.type, sv.value);
                vm.page(slot)->v[m] = v;
                break;
            }
        }
    }

    void fillArray(int32_t &dst, const VarType &t, int32_t index) {
        if (index < 0 || size_t(index) >= L.arrayAt.size()) return;
        if (arrayOpen[size_t(index)]) { failed = true; return; }
        arrayOpen[size_t(index)] = 1;
        fillArrayAt(dst, t, index);
        arrayOpen[size_t(index)] = 0;
    }

    void fillArrayAt(int32_t &dst, const VarType &t, int32_t index) {
        Reader r{*L.data};
        size_t p = L.arrayAt[index];
        int32_t rank = int32_t(r.u32(p));
        Page *old = vm.page(dst);
        int32_t elemType = old ? old->elemType : T_INT, elemStruct = old ? old->elemStruct : t.structType;
        int32_t declRank = old ? old->rank : std::max(1, t.rank);
        if (rank > kMaxRank) return;
        if (rank < 0) {
            int32_t s = vm.newArray(elemType, elemStruct, declRank, nullptr, 0);
            vm.decref(dst);
            dst = s;
            return;
        }
        std::vector<uint32_t> dims(static_cast<size_t>(rank));
        for (auto &d : dims) d = r.u32(p);
        r.u32(p);  // the number of rows, which the dimensions give
        int32_t s = node(r, p, elemType, elemStruct, rank, dims);
        vm.decref(dst);
        dst = s;
    }

    // An array of this rank: its outer length from the dimensions, its innermost rows read in order.
    int32_t node(Reader &r, size_t &p, int32_t elemType, int32_t elemStruct, int rank, const std::vector<uint32_t> &dims) {
        int32_t none[1] = {0};
        int32_t s = vm.newArray(elemType, elemStruct, rank, none, 1);
        if (++pages > kMaxArrayPages) {
            failed = true;
            return s;
        }
        if (rank > 1) {
            for (uint32_t k = 0; k < dims[size_t(rank - 1)] && !r.bad && !failed; k++) {
                int32_t v = node(r, p, elemType, elemStruct, rank - 1, dims);
                vm.page(s)->v.push_back(v);
            }
            return s;
        }
        uint32_t n = r.u32(p);
        for (uint32_t k = 0; k < n && !r.bad && !failed; k++) {
            int32_t val = int32_t(r.u32(p)), typ = int32_t(r.u32(p));
            int32_t v = elemType == T_STRING ? vm.newString() : elemType == T_STRUCT ? vm.newStruct(elemStruct) : 0;
            store(v, VarType{elemType, elemStruct, 0}, typ, val);
            vm.page(s)->v.push_back(v);
        }
        return s;
    }
};

}  // namespace

int32_t globalLoad(VM &vm, const std::string &key, const std::string &file, const std::string &group, int32_t *count) {
    if (count) *count = 0;
    std::vector<uint8_t> d;
    if (!readGD(hostPath(vm, file, true), d)) return 0;
    Loaded L;
    std::string fileKey, fileGroup;
    if (!parse(d, L, fileKey, fileGroup) || fileKey != key) return 0;
    if (!group.empty() && fileGroup != group) return 0;
    // The whole save is checked first, so a damaged one fails without changing anything; then it fills the
    // globals in place, as the PC does, so references to them stay valid and no destructor runs.
    Checker check{vm, L};
    for (auto &gv : L.globals)
        for (size_t i = 0; i < vm.ain.globals.size(); i++)
            if (vm.ain.globals[i].name == gv.name) {
                check.value(vm.ain.globals[i].t, gv.value);
                break;
            }
    if (check.failed) return 0;
    Applier a{vm, L};
    int n = 0;
    for (auto &gv : L.globals) {
        for (size_t i = 0; i < vm.ain.globals.size(); i++) {
            if (vm.ain.globals[i].name != gv.name) continue;
            int32_t v = vm.page(vm.globalPage)->v[i];
            a.store(v, vm.ain.globals[i].t, gv.type, gv.value);
            vm.page(vm.globalPage)->v[i] = v;
            n++;
            break;
        }
    }
    if (count) *count = n;
    return a.failed ? 0 : 1;
}

int32_t exists(VM &vm, const std::string &name, bool inSaveFolder) {
    std::error_code ec;
    return fs::exists(hostPath(vm, name, inSaveFolder), ec) ? 1 : 0;
}

int32_t remove(VM &vm, const std::string &name) {
    std::error_code ec;
    return fs::remove(hostPath(vm, name, true), ec) ? 1 : 0;
}

int32_t copy(VM &vm, const std::string &dst, const std::string &src) {
    std::error_code ec;
    fs::copy_file(hostPath(vm, src, true), hostPath(vm, dst, true), fs::copy_options::overwrite_existing, ec);
    return ec ? 0 : 1;
}

}  // namespace s4::save

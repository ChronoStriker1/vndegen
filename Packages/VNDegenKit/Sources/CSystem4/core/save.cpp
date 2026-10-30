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

void cipher(uint8_t *p, size_t n) {
    uint32_t mt[624];
    mt[0] = 0x12320F;
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

}  // namespace

bool readGD(const std::string &path, std::vector<uint8_t> &plain) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
    if (d.size() < 8 || d[0] != 'G' || d[1] != 'D') return false;
    uint32_t len;
    std::memcpy(&len, d.data() + 4, 4);
    plain.resize(len);
    auto inflate = [&] {
        uLongf out = len;
        return uncompress(plain.data(), &out, d.data() + 8, uLong(d.size() - 8)) == Z_OK && out == len;
    };
    // Plain zlib first: XORed data never inflates cleanly, since its header bytes are scrambled.
    if (inflate()) return true;
    cipher(d.data() + 8, d.size() - 8);
    return inflate();
}

bool writeGD(const std::string &path, const std::vector<uint8_t> &plain, bool xorData) {
    uLongf cap = compressBound(uLong(plain.size()));
    std::vector<uint8_t> d(8 + cap);
    if (compress2(d.data() + 8, &cap, plain.data(), uLong(plain.size()), 9) != Z_OK) return false;
    d.resize(8 + cap);
    d[0] = 'G'; d[1] = 'D'; d[2] = 1; d[3] = 1;
    uint32_t len = uint32_t(plain.size());
    std::memcpy(d.data() + 4, &len, 4);
    if (xorData) cipher(d.data() + 8, cap);
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
    if (!p.empty() && p[0] == '/') return p;
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
// The plain text: the key, then a 56-byte header (format 1000, the .ain version, 56, the number of globals in the
// program, and (offset, count) for five sections), the group name, then the sections:
//   records      u32 kind, name, u32 n, n value indexes. Record 0 (kind 1000, no name) lists the saved globals;
//                the rest are struct instances (kind 13, the struct's name) listing their members' values.
//   globals      u32 type, u32 value, name, u32 1 for each saved global.
//   strings      every string value, NUL terminated.
//   arrays       i32 rank (-1 when not allocated), i32 the innermost length, then the elements: a dimension is
//                its length then its elements; the first level also gives the rank first.
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

    void arrayNode(std::vector<uint8_t> &out, int32_t slot, int rank, bool top) {
        Page *a = vm.page(slot);
        size_t n = a ? a->v.size() : 0;
        if (top) u32(out, uint32_t(rank));
        u32(out, uint32_t(n));
        for (size_t i = 0; i < n; i++) {
            int32_t v = vm.page(slot)->v[i];
            if (rank > 1) {
                arrayNode(out, v, rank - 1, false);
                continue;
            }
            Page &ap = *vm.page(slot);
            VarType et{ap.elemType, ap.elemStruct, 0};
            int32_t val = value(et, v);
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
            // The second header word is the length of the innermost dimension, read down the first elements.
            Page *inner = a;
            for (int r = rank; r > 1 && inner && !inner->v.empty(); r--) inner = vm.page(inner->v[0]);
            u32(rec, uint32_t(rank));
            u32(rec, uint32_t(inner ? inner->v.size() : 0));
            arrayNode(rec, slot, rank, true);
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
    uint32_t hdr[14] = {1000, uint32_t(vm.ain.version), 56, uint32_t(vm.ain.globals.size()),
                        recordsOff, uint32_t(w.records.size()), globalsOff, uint32_t(saved.size()),
                        stringsOff, uint32_t(w.nStrings), arraysOff, uint32_t(w.nArrays), structOff, uint32_t(w.nStructVals)};
    std::memcpy(out.data() + headerAt, hdr, 56);
    (void)keyLen;
    if (count) *count = int32_t(saved.size());
    return writeGD(hostPath(vm, file, true), out, vm.saveFormat.xorData) ? 1 : 0;
}

namespace {

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
    std::function<void(size_t &, int)> skipNode = [&](size_t &q, int rank) {
        uint32_t n = r.u32(q);
        for (uint32_t k = 0; k < n && !r.bad; k++) {
            if (rank > 1) skipNode(q, rank - 1);
            else { r.u32(q); r.u32(q); }
        }
    };
    for (uint32_t k = 0; k < hdr[11] && !r.bad; k++) {
        L.arrayAt.push_back(p);
        int32_t rank = int32_t(r.u32(p));
        r.u32(p);
        if (rank < 0) continue;
        r.u32(p);  // the rank again
        skipNode(p, rank);
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

// Puts a saved value into a variable of the program's type, matching struct members by name.
struct Applier {
    VM &vm;
    Loaded &L;

    void store(int32_t &dst, const VarType &t, int32_t savedType, int32_t value) {
        if (t.type == T_STRING) {
            if (!vm.valid(dst)) dst = vm.newString();
            vm.str(dst) = value >= 0 && size_t(value) < L.strings.size() ? L.strings[value] : std::string();
        } else if (t.type == T_STRUCT) {
            if (!vm.valid(dst)) dst = vm.newStruct(t.structType);
            if (value >= 0 && size_t(value) < L.records.size()) fillStruct(dst, L.records[value]);
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
        Reader r{*L.data};
        size_t p = L.arrayAt[index];
        int32_t rank = int32_t(r.u32(p));
        r.u32(p);
        Page *old = vm.page(dst);
        int32_t elemType = old ? old->elemType : T_INT, elemStruct = old ? old->elemStruct : t.structType;
        int32_t declRank = old ? old->rank : std::max(1, t.rank);
        if (rank < 0) {
            int32_t s = vm.newArray(elemType, elemStruct, declRank, nullptr, 0);
            vm.decref(dst);
            dst = s;
            return;
        }
        r.u32(p);
        int32_t s = node(r, p, elemType, elemStruct, rank);
        vm.decref(dst);
        dst = s;
    }

    int32_t node(Reader &r, size_t &p, int32_t elemType, int32_t elemStruct, int rank) {
        uint32_t n = r.u32(p);
        int32_t dims[1] = {0};
        int32_t s = vm.newArray(elemType, elemStruct, rank, dims, 1);
        for (uint32_t k = 0; k < n && !r.bad; k++) {
            int32_t v;
            if (rank > 1) {
                v = node(r, p, elemType, elemStruct, rank - 1);
            } else {
                int32_t val = int32_t(r.u32(p)), typ = int32_t(r.u32(p));
                v = elemType == T_STRING ? vm.newString() : elemType == T_STRUCT ? vm.newStruct(elemStruct) : 0;
                store(v, VarType{elemType, elemStruct, 0}, typ, val);
            }
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
    return 1;
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

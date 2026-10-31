#include "ain.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <zlib.h>

#include "save.h"

namespace s4 {
namespace {

struct Reader {
    const std::vector<uint8_t> &d;
    size_t i = 0;
    bool bad = false;

    int32_t i32() {
        if (i + 4 > d.size()) { bad = true; i = d.size(); return 0; }
        int32_t v;
        std::memcpy(&v, d.data() + i, 4);
        i += 4;
        return v;
    }
    // A count of items at least minBytes long each. Damage that claims more than the bytes left marks the reader
    // bad and returns 0, so no count asks for a huge allocation.
    int count(size_t minBytes) {
        int n = i32();
        if (n < 0 || size_t(n) > (d.size() - i) / minBytes) { bad = true; i = d.size(); return 0; }
        return n;
    }
    std::string str() {
        size_t j = i;
        while (j < d.size() && d[j]) j++;
        if (j >= d.size()) { bad = true; i = d.size(); return {}; }
        std::string s(reinterpret_cast<const char *>(d.data() + i), j - i);
        i = j + 1;
        return s;
    }
    std::string tag() {
        if (i + 4 > d.size()) { bad = true; return {}; }
        std::string t(reinterpret_cast<const char *>(d.data() + i), 4);
        i += 4;
        return t;
    }
    VarType vtype() {
        VarType t;
        t.type = i32();
        t.structType = i32();
        t.rank = i32();
        return t;
    }
    Variable var() {
        Variable v;
        v.name = str();
        v.t = vtype();
        return v;
    }
    FuncType functype() {
        FuncType f;
        f.name = str();
        f.ret.type = i32();
        f.ret.structType = i32();
        f.argc = i32();
        int n = count(13);
        for (int k = 0; k < n && !bad; k++) f.vars.push_back(var());
        return f;
    }
};

}  // namespace

std::string Ain::load(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "cannot open " + path;
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // The SDK's compiler writes the sections XORed with the save cipher seeded 0x5D3E3 (found by searching for the
    // seed that turns the first bytes into "VERS"); the games' files are "AI2" zlib containers.
    if (raw.size() >= 4 && std::memcmp(raw.data(), "AI2\0", 4) != 0) {
        std::vector<uint8_t> plain = raw;
        save::mtXor(plain.data(), plain.size(), 0x5D3E3);
        if (std::memcmp(plain.data(), "VERS", 4) == 0) return parse(plain);
    }
    if (raw.size() < 16 || std::memcmp(raw.data(), "AI2\0", 4) != 0)
        return "not a compressed (AI2) .ain file";
    uint32_t usize, csize;
    std::memcpy(&usize, raw.data() + 8, 4);
    std::memcpy(&csize, raw.data() + 12, 4);
    if (16 + uint64_t(csize) > raw.size()) return "truncated .ain file";
    if (uint64_t(usize) > uint64_t(csize) * 1032 + 64) return "corrupt .ain file";  // beyond zlib's largest ratio
    std::vector<uint8_t> plain(usize);
    uLongf out = usize;
    if (uncompress(plain.data(), &out, raw.data() + 16, csize) != Z_OK || out != usize)
        return "corrupt .ain file";
    return parse(plain);
}

std::string Ain::parse(const std::vector<uint8_t> &plain) {
    Reader r{plain};
    std::string t, prev;
    while (r.i < plain.size() && !r.bad) {
        prev = t;
        t = r.tag();
        if (t == "VERS") {
            version = r.i32();
        } else if (t == "KEYC") {
            keycode = r.i32();
        } else if (t == "CODE") {
            int n = r.i32();
            if (n < 0 || size_t(n) > plain.size() - r.i) return "bad CODE section";
            code.assign(plain.begin() + r.i, plain.begin() + r.i + n);
            r.i += n;
        } else if (t == "FUNC") {
            int n = r.count(29);
            functions.resize(n);
            for (auto &f : functions) {
                f.address = r.i32();
                f.name = r.str();
                f.isLabel = r.i32();
                f.ret.type = r.i32();
                f.ret.structType = r.i32();
                f.argc = r.i32();
                int vc = r.i32();
                f.crc = r.i32();
                if (f.argc < 0 || vc < f.argc || size_t(vc) > (plain.size() - r.i) / 13)
                    return "bad variable counts for function " + f.name;
                for (int k = 0; k < vc && !r.bad; k++) f.vars.push_back(r.var());
                if (r.bad) break;
            }
        } else if (t == "GLOB") {
            int n = r.count(13);
            globals.resize(n);
            for (auto &g : globals) {
                g.name = r.str();
                g.t = r.vtype();
                if (version >= 5) g.group = r.i32();  // the SDK compiler's version 4 has no save groups
            }
        } else if (t == "GSET") {
            int n = r.count(9);
            globalInits.resize(n);
            for (auto &g : globalInits) {
                g.global = r.i32();
                g.type = r.i32();
                if (g.type == T_STRING) g.strValue = r.str();
                else g.intValue = r.i32();
            }
        } else if (t == "STRT") {
            int n = r.count(13);
            structs.resize(n);
            for (auto &s : structs) {
                s.name = r.str();
                s.constructor = r.i32();
                s.destructor = r.i32();
                int mc = r.count(13);
                for (int k = 0; k < mc && !r.bad; k++) s.members.push_back(r.var());
            }
        } else if (t == "MSG0") {
            int n = r.count(1);
            for (int k = 0; k < n && !r.bad; k++) messages.push_back(r.str());
        } else if (t == "MSG1") {
            // Each message is stored as length-prefixed bytes, byte j raised by 0x60 + j.
            int n = r.i32();
            r.i32();
            if (n < 0 || size_t(n) > (plain.size() - r.i) / 4) return "bad MSG1 section";
            for (int k = 0; k < n && !r.bad; k++) {
                int len = r.i32();
                if (len < 0 || r.i + len > plain.size()) return "bad MSG1 section";
                std::string s(len, '\0');
                for (int j = 0; j < len; j++) s[j] = char(plain[r.i + j] - 0x60 - j);
                r.i += len;
                messages.push_back(std::move(s));
            }
        } else if (t == "MAIN") {
            mainFunction = r.i32();
        } else if (t == "MSGF") {
            messageFunction = r.i32();
        } else if (t == "HLL0") {
            int n = r.count(5);
            libraries.resize(n);
            for (auto &l : libraries) {
                l.name = r.str();
                int fc = r.count(9);
                l.functions.resize(fc);
                for (auto &f : l.functions) {
                    f.name = r.str();
                    f.retType = r.i32();
                    int ac = r.count(5);
                    for (int k = 0; k < ac && !r.bad; k++) {
                        Variable v;
                        v.name = r.str();
                        v.t.type = r.i32();
                        f.args.push_back(v);
                    }
                }
            }
        } else if (t == "SWI0") {
            int n = r.count(12);
            switches.resize(n);
            for (auto &s : switches) {
                s.caseType = r.i32();
                s.defaultAddress = r.i32();
                int cc = r.count(8);
                for (int k = 0; k < cc && !r.bad; k++) {
                    SwitchCase c;
                    c.value = r.i32();
                    c.address = r.i32();
                    s.cases.push_back(c);
                }
            }
        } else if (t == "GVER") {
            gameVersion = r.i32();
        } else if (t == "STR0") {
            int n = r.count(1);
            for (int k = 0; k < n && !r.bad; k++) strings.push_back(r.str());
        } else if (t == "FNAM") {
            int n = r.count(1);
            for (int k = 0; k < n && !r.bad; k++) fileNames.push_back(r.str());
        } else if (t == "OJMP") {
            onJump = r.i32();
        } else if (t == "FNCT" || t == "DELG") {
            r.i32();  // section size in bytes
            int n = r.count(17);
            auto &list = t == "FNCT" ? functypes : delegates;
            for (int k = 0; k < n && !r.bad; k++) list.push_back(r.functype());
        } else if (t == "OBJG") {
            int n = r.count(1);
            for (int k = 0; k < n && !r.bad; k++) objGroups.push_back(r.str());
        } else {
        {
            char hex[16];
            std::string tag;
            for (unsigned char ch : t) { std::snprintf(hex, sizeof hex, "%02x", ch); tag += hex; }
            return "unknown .ain section " + tag + " at " + std::to_string(r.i - 4) + " after " + prev;
        }
        }
    }
    if (r.bad) return "truncated .ain section " + t;
    if (code.empty()) return "no CODE section";
    // A struct can't hold itself by value. The VM and the save encoders recurse through struct members, so a
    // damaged file with such a cycle is refused here.
    std::vector<int> state(structs.size(), 0);  // 0 unvisited, 1 on the path, 2 done
    std::function<bool(size_t)> acyclic = [&](size_t s) {
        if (state[s] == 1) return false;
        if (state[s] == 2) return true;
        state[s] = 1;
        for (const Variable &m : structs[s].members)
            if (m.t.type == T_STRUCT && m.t.structType >= 0 && size_t(m.t.structType) < structs.size() &&
                !acyclic(size_t(m.t.structType)))
                return false;
        state[s] = 2;
        return true;
    };
    for (size_t s = 0; s < structs.size(); s++)
        if (!acyclic(s)) return "struct " + structs[s].name + " contains itself";
    return {};
}

int Ain::findFunction(const std::string &name) const {
    for (size_t i = 0; i < functions.size(); i++)
        if (functions[i].name == name) return int(i);
    return -1;
}

int Ain::findStruct(const std::string &name) const {
    for (size_t i = 0; i < structs.size(); i++)
        if (structs[i].name == name) return int(i);
    return -1;
}

int Ain::findLibrary(const std::string &name) const {
    for (size_t i = 0; i < libraries.size(); i++)
        if (libraries[i].name == name) return int(i);
    return -1;
}

}  // namespace s4

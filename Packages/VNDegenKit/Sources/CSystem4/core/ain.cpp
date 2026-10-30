#include "ain.h"

#include <cstring>
#include <fstream>
#include <zlib.h>

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
        int n = i32();
        for (int k = 0; k < n && !bad; k++) f.vars.push_back(var());
        return f;
    }
};

}  // namespace

std::string Ain::load(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "cannot open " + path;
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (raw.size() < 16 || std::memcmp(raw.data(), "AI2\0", 4) != 0)
        return "not a compressed (AI2) .ain file";
    uint32_t usize, csize;
    std::memcpy(&usize, raw.data() + 8, 4);
    std::memcpy(&csize, raw.data() + 12, 4);
    if (16 + uint64_t(csize) > raw.size()) return "truncated .ain file";
    std::vector<uint8_t> plain(usize);
    uLongf out = usize;
    if (uncompress(plain.data(), &out, raw.data() + 16, csize) != Z_OK || out != usize)
        return "corrupt .ain file";
    return parse(plain);
}

std::string Ain::parse(const std::vector<uint8_t> &plain) {
    Reader r{plain};
    std::string t;
    while (r.i < plain.size() && !r.bad) {
        t = r.tag();
        if (t == "VERS") {
            version = r.i32();
        } else if (t == "KEYC") {
            keycode = r.i32();
        } else if (t == "CODE") {
            int n = r.i32();
            if (n < 0 || r.i + n > plain.size()) return "bad CODE section";
            code.assign(plain.begin() + r.i, plain.begin() + r.i + n);
            r.i += n;
        } else if (t == "FUNC") {
            int n = r.i32();
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
                for (int k = 0; k < vc && !r.bad; k++) f.vars.push_back(r.var());
                if (r.bad) break;
            }
        } else if (t == "GLOB") {
            int n = r.i32();
            globals.resize(n);
            for (auto &g : globals) {
                g.name = r.str();
                g.t = r.vtype();
                g.group = r.i32();
            }
        } else if (t == "GSET") {
            int n = r.i32();
            globalInits.resize(n);
            for (auto &g : globalInits) {
                g.global = r.i32();
                g.type = r.i32();
                if (g.type == T_STRING) g.strValue = r.str();
                else g.intValue = r.i32();
            }
        } else if (t == "STRT") {
            int n = r.i32();
            structs.resize(n);
            for (auto &s : structs) {
                s.name = r.str();
                s.constructor = r.i32();
                s.destructor = r.i32();
                int mc = r.i32();
                for (int k = 0; k < mc && !r.bad; k++) s.members.push_back(r.var());
            }
        } else if (t == "MSG0") {
            int n = r.i32();
            for (int k = 0; k < n && !r.bad; k++) messages.push_back(r.str());
        } else if (t == "MSG1") {
            // Each message is stored as length-prefixed bytes, byte j raised by 0x60 + j.
            int n = r.i32();
            r.i32();
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
            int n = r.i32();
            libraries.resize(n);
            for (auto &l : libraries) {
                l.name = r.str();
                int fc = r.i32();
                l.functions.resize(fc);
                for (auto &f : l.functions) {
                    f.name = r.str();
                    f.retType = r.i32();
                    int ac = r.i32();
                    for (int k = 0; k < ac && !r.bad; k++) {
                        Variable v;
                        v.name = r.str();
                        v.t.type = r.i32();
                        f.args.push_back(v);
                    }
                }
            }
        } else if (t == "SWI0") {
            int n = r.i32();
            switches.resize(n);
            for (auto &s : switches) {
                s.caseType = r.i32();
                s.defaultAddress = r.i32();
                int cc = r.i32();
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
            int n = r.i32();
            for (int k = 0; k < n && !r.bad; k++) strings.push_back(r.str());
        } else if (t == "FNAM") {
            int n = r.i32();
            for (int k = 0; k < n && !r.bad; k++) fileNames.push_back(r.str());
        } else if (t == "OJMP") {
            onJump = r.i32();
        } else if (t == "FNCT" || t == "DELG") {
            r.i32();  // section size in bytes
            int n = r.i32();
            auto &list = t == "FNCT" ? functypes : delegates;
            for (int k = 0; k < n && !r.bad; k++) list.push_back(r.functype());
        } else if (t == "OBJG") {
            int n = r.i32();
            for (int k = 0; k < n && !r.bad; k++) objGroups.push_back(r.str());
        } else {
            return "unknown .ain section " + t;
        }
    }
    if (r.bad) return "truncated .ain section " + t;
    if (code.empty()) return "no CODE section";
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

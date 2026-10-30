// Disassembles an .ain file and checks that every function decodes cleanly: instructions end
// exactly at the next function, and every jump lands on an instruction.
// Usage: s4dis <file.ain> [--verify] [--func name-substring] [--op number] [--lib library-name] [--struct number]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "sjis.h"
#include "ain.h"
#include "opcodes.h"

using namespace s4;

static int32_t arg(const Ain &a, uint32_t p, int k) {
    int32_t v;
    std::memcpy(&v, a.code.data() + p + 2 + 4 * k, 4);
    return v;
}

int main(int argc, char **argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: s4dis file.ain [--verify] [--func s] [--op n] [--lib name] [--struct n]\n"); return 2; }
    Ain a;
    std::string err = a.load(argv[1]);
    if (!err.empty()) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    bool verify = false;
    std::string func;
    int onlyOp = -1;
    for (int i = 2; i < argc; i++) {
        if (!std::strcmp(argv[i], "--verify")) verify = true;
        else if (!std::strcmp(argv[i], "--func") && i + 1 < argc) func = argv[++i];
        else if (!std::strcmp(argv[i], "--op") && i + 1 < argc) onlyOp = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--struct") && i + 1 < argc) {
            // A struct's members: name, type, struct type and rank.
            int n = std::atoi(argv[i + 1]);
            if (n < 0 || size_t(n) >= a.structs.size()) return 1;
            const Struct &st = a.structs[n];
            std::printf("struct %d %s\n", n, sjis::toUtf8(st.name).c_str());
            for (size_t k = 0; k < st.members.size(); k++)
                std::printf("  %zu %s type %d struct %d rank %d\n", k, sjis::toUtf8(st.members[k].name).c_str(),
                            st.members[k].t.type, st.members[k].t.structType, st.members[k].t.rank);
            return 0;
        }
        else if (!std::strcmp(argv[i], "--lib") && i + 1 < argc) {
            // A library's function signatures: return type, then each argument's name and type.
            for (auto &l : a.libraries) {
                if (l.name != argv[i + 1]) continue;
                for (auto &f : l.functions) {
                    std::printf("%d %s(", f.retType, f.name.c_str());
                    for (size_t k = 0; k < f.args.size(); k++)
                        std::printf("%s%s:%d", k ? ", " : "", sjis::toUtf8(f.args[k].name).c_str(), f.args[k].t.type);
                    std::printf(")\n");
                }
            }
            return 0;
        }
    }
    std::printf("version %d, %zu functions, %zu globals, %zu structs, %zu libraries, %zu messages\n",
                a.version, a.functions.size(), a.globals.size(), a.structs.size(), a.libraries.size(),
                a.messages.size());
    std::map<uint32_t, int> starts;  // address of FUNC marker -> function
    for (size_t i = 0; i < a.functions.size(); i++)
        if (a.functions[i].address >= 6 && a.functions[i].address <= a.code.size()) starts[a.functions[i].address - 6] = int(i);
    int bad = 0;
    std::map<int, int> useCount;
    for (auto it = starts.begin(); it != starts.end(); ++it) {
        uint32_t begin = it->first;
        auto nx = std::next(it);
        uint32_t end = nx == starts.end() ? uint32_t(a.code.size()) : nx->first;
        const Function &f = a.functions[it->second];
        bool show = !verify && (func.empty() ? onlyOp < 0 : sjis::toUtf8(f.name).find(func) != std::string::npos);
        std::set<uint32_t> insn;
        std::vector<uint32_t> targets;
        std::vector<std::string> lines;
        bool hasOp = false;
        uint32_t p = begin;
        while (p < end) {
            uint16_t op = a.code[p] | a.code[p + 1] << 8;
            const OpInfo *oi = opInfo(op);
            if (!oi) { std::printf("%s: unknown opcode %u at %u\n", sjis::toUtf8(f.name).c_str(), op, p); bad++; break; }
            insn.insert(p);
            useCount[op]++;
            if (op == onlyOp) hasOp = true;
            std::string ln = std::to_string(p) + ": " + oi->name;
            for (int k = 0; k < oi->args; k++) ln += " " + std::to_string(arg(a, p, k));
            if (op == 0x41 /* S_PUSH */ && arg(a, p, 0) >= 0 && size_t(arg(a, p, 0)) < a.strings.size())
                ln += "  ; \"" + sjis::toUtf8(a.strings[arg(a, p, 0)]) + "\"";
            if (op == 0x30 /* CALLFUNC */ || op == 0x5c /* CALLMETHOD */) {
                int fi = arg(a, p, 0);
                if (fi >= 0 && size_t(fi) < a.functions.size()) ln += "  ; " + sjis::toUtf8(a.functions[fi].name);
            }
            if (op == 0x5a /* CALLHLL */) {
                int l = arg(a, p, 0), fi = arg(a, p, 1);
                if (l >= 0 && size_t(l) < a.libraries.size() && fi >= 0 && size_t(fi) < a.libraries[l].functions.size())
                    ln += "  ; " + a.libraries[l].name + "." + a.libraries[l].functions[fi].name;
            }
            lines.push_back(ln);
            if (op == 0x2c || op == 0x2d || op == 0x2e) targets.push_back(arg(a, p, 0));
            p += 2 + 4 * oi->args;
        }
        if (p != end) {
            std::printf("%s: decoding overruns the function end (%u past %u)\n", sjis::toUtf8(f.name).c_str(), p, end);
            bad++;
        }
        for (uint32_t t : targets)
            if (!insn.count(t)) { std::printf("%s: jump to %u is not an instruction\n", sjis::toUtf8(f.name).c_str(), t); bad++; }
        if (onlyOp >= 0 && hasOp && std::getenv("S4DIS_MAXLEN") && lines.size() > size_t(std::atoi(std::getenv("S4DIS_MAXLEN")))) hasOp = false;
        if (show || (onlyOp >= 0 && hasOp)) {
            std::printf("\n== %s (args %d) ret %d/%d\n", sjis::toUtf8(f.name).c_str(), f.argc, f.ret.type, f.ret.structType);
            for (size_t k = 0; k < f.vars.size(); k++)
                std::printf("   var %zu %s type %d struct %d rank %d\n", k, sjis::toUtf8(f.vars[k].name).c_str(), f.vars[k].t.type, f.vars[k].t.structType, f.vars[k].t.rank);
            for (auto &l : lines) std::printf("  %s\n", l.c_str());
        }
    }
    if (verify) {
        std::printf("%d problems\n", bad);
        for (auto &[op, n] : useCount) std::printf("  %3d %-16s %d\n", op, kOps[op].name, n);
    }
    return bad ? 1 : 0;
}

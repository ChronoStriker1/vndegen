// CommonSystemData: named settings kept across play sessions in later games (Test Game 56), such as what the player
// has seen. The game names the file and loads it at startup; there is no save call, so each change writes the file.
// The file is VNDegen's own: a GD container (see save.h) holding the values; the PC engine's file doesn't load.
#include <cstring>
#include <map>
#include <string>

#include "game.h"
#include "save.h"

namespace s4 {
namespace {

Game &G() { return game(); }

struct Value {
    enum Type : uint8_t { Int = 1, Float = 2, String = 3, Bool = 4 } type;
    int32_t i = 0;
    float f = 0;
    std::string s;
};

std::map<std::string, Value> g_data;
std::string g_file;  // the game's path for the file

constexpr char kMagic[4] = {'V', 'C', 'S', 'D'};

void put32(std::vector<uint8_t> &d, uint32_t v) {
    uint8_t b[4];
    std::memcpy(b, &v, 4);
    d.insert(d.end(), b, b + 4);
}
void putStr(std::vector<uint8_t> &d, const std::string &s) {
    put32(d, uint32_t(s.size()));
    d.insert(d.end(), s.begin(), s.end());
}

void write() {
    if (g_file.empty()) return;
    std::vector<uint8_t> d(kMagic, kMagic + 4);
    put32(d, uint32_t(g_data.size()));
    for (const auto &[name, v] : g_data) {
        d.push_back(v.type);
        putStr(d, name);
        switch (v.type) {
        case Value::Float: { uint32_t u; std::memcpy(&u, &v.f, 4); put32(d, u); break; }
        case Value::String: putStr(d, v.s); break;
        default: put32(d, uint32_t(v.i)); break;
        }
    }
    if (!save::writeGD(save::hostPath(*G().vm, g_file, true), d, false)) G().log("CommonSystemData: can't write the file");
}

bool read(const std::vector<uint8_t> &d, std::map<std::string, Value> &out) {
    size_t p = 0;
    auto u32 = [&](uint32_t &v) {
        if (d.size() - p < 4) return false;
        std::memcpy(&v, d.data() + p, 4);
        p += 4;
        return true;
    };
    auto str = [&](std::string &s) {
        uint32_t n;
        if (!u32(n) || n > d.size() - p) return false;
        s.assign(reinterpret_cast<const char *>(d.data() + p), n);
        p += n;
        return true;
    };
    uint32_t count;
    if (d.size() < 8 || std::memcmp(d.data(), kMagic, 4) != 0) return false;
    p = 4;
    if (!u32(count)) return false;
    for (uint32_t k = 0; k < count; k++) {
        if (p >= d.size()) return false;
        Value v{Value::Type(d[p++])};
        std::string name;
        uint32_t u;
        if (!str(name)) return false;
        switch (v.type) {
        case Value::Int: case Value::Bool: if (!u32(u)) return false; v.i = int32_t(u); break;
        case Value::Float: if (!u32(u)) return false; std::memcpy(&v.f, &u, 4); break;
        case Value::String: if (!str(v.s)) return false; break;
        default: return false;
        }
        out[name] = std::move(v);
    }
    return true;
}

void set(const std::string &name, Value v) {
    g_data[name] = std::move(v);
    write();
}
const Value *get(const std::string &name, Value::Type type) {
    auto it = g_data.find(name);
    return it != g_data.end() && it->second.type == type ? &it->second : nullptr;
}

S4_HLL_RESET {
    g_data.clear();
    g_file.clear();
}

}  // namespace

S4_HLL(CommonSystemData, SetFullPathSaveFileName) { g_file = c.s(0); }
S4_HLL(CommonSystemData, LoadAtStartup) {
    std::vector<uint8_t> d;
    std::map<std::string, Value> loaded;
    std::string path = save::hostPath(*G().vm, g_file, true);
    bool ok = !path.empty() && save::readGD(path, d) && read(d, loaded);
    if (ok) g_data = std::move(loaded);
    c.retBool(ok);
}
S4_HLL(CommonSystemData, SetDataInt) { set(c.s(0), {Value::Int, c.i(1)}); c.retBool(true); }
S4_HLL(CommonSystemData, SetDataFloat) { set(c.s(0), {Value::Float, 0, c.f(1)}); c.retBool(true); }
S4_HLL(CommonSystemData, SetDataString) { set(c.s(0), {Value::String, 0, 0, c.s(1)}); c.retBool(true); }
S4_HLL(CommonSystemData, SetDataBool) { set(c.s(0), {Value::Bool, c.b(1) ? 1 : 0}); c.retBool(true); }
S4_HLL(CommonSystemData, GetDataInt) {
    const Value *v = get(c.s(0), Value::Int);
    if (v) c.ref(1) = v->i;
    c.retBool(v != nullptr);
}
S4_HLL(CommonSystemData, GetDataFloat) {
    const Value *v = get(c.s(0), Value::Float);
    if (v) c.refF(1) = v->f;
    c.retBool(v != nullptr);
}
S4_HLL(CommonSystemData, GetDataString) {
    const Value *v = get(c.s(0), Value::String);
    if (v) c.refS(1) = v->s;
    c.retBool(v != nullptr);
}
S4_HLL(CommonSystemData, GetDataBool) {
    const Value *v = get(c.s(0), Value::Bool);
    if (v) c.ref(1) = v->i;
    c.retBool(v != nullptr);
}

}  // namespace s4

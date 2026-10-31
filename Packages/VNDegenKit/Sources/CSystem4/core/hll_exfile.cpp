// MainEXFile: the game's .ex data file (exfile.h), read through handles. A handle names one value: a top-level
// entry, a list item or a table cell. Tables are addressed by column and row (A2), by the value in the key column
// and a column's name (IA2 for an int key, SA2 for a string key), or by row and a column's name (RA2). Handle 0 is
// none.
#include <filesystem>
#include <strings.h>
#include <memory>
#include <set>
#include <vector>

#include "exfile.h"
#include "game.h"
#include "sjis.h"

namespace fs = std::filesystem;

namespace s4 {
namespace {

Game &G() { return game(); }

std::unique_ptr<ExFile> g_ex;
bool g_tried = false;
std::vector<const ExValue *> g_handles{nullptr};
std::map<const ExValue *, int32_t> g_handleOf;
std::set<std::string> g_missing;

S4_HLL_RESET {
    g_ex.reset();
    g_tried = false;
    g_handles.assign(1, nullptr);
    g_handleOf.clear();
    g_missing.clear();
}

// The .ex file beside the code file, with the code file's name.
const ExFile *ex() {
    if (!g_tried) {
        g_tried = true;
        Game &g = G();
        std::string code = codeFile(g.cfg.root, g.ini);
        fs::path want = fs::path(code).replace_extension(".ex");
        std::error_code ec;
        for (auto &e : fs::directory_iterator(want.parent_path(), ec)) {
            std::string a = e.path().filename().string(), b = want.filename().string();
            if (a.size() == b.size() && strcasecmp(a.c_str(), b.c_str()) == 0) {
                auto f = std::make_unique<ExFile>();
                std::string err;
                if (f->load(e.path().string(), err)) g_ex = std::move(f);
                else g.log("MainEXFile: " + e.path().string() + ": " + err);
                break;
            }
        }
        if (!g_ex) g.log("MainEXFile: no .ex file beside " + code);
    }
    return g_ex.get();
}

// A tree path: the entry's name, then the names of the nodes down to the one wanted, joined with '.' (which can't
// be a CP932 trail byte). Null when any part is missing.
const ExNode *nodeAt(const std::string &path) {
    const ExFile *f = ex();
    if (!f) return nullptr;
    size_t dot = path.find('.');
    const ExValue *root = f->find(path.substr(0, dot));
    if (!root || root->type != ExValue::Tree || !root->tree) return nullptr;
    const ExNode *n = root->tree.get();
    while (dot != std::string::npos) {
        size_t next = path.find('.', dot + 1);
        std::string part = path.substr(dot + 1, next == std::string::npos ? std::string::npos : next - dot - 1);
        const ExNode *child = nullptr;
        for (auto &c : n->children)
            if (c->name == part) { child = c.get(); break; }
        if (!child) return nullptr;
        n = child;
        dot = next;
    }
    return n;
}

// A name: a top-level entry, or a tree path to a node that holds a value.
const ExValue *named(const std::string &name) {
    const ExFile *f = ex();
    if (!f) return nullptr;
    if (const ExValue *v = f->find(name)) return v;
    if (name.find('.') == std::string::npos) return nullptr;
    const ExNode *n = nodeAt(name);
    return n ? n->value.get() : nullptr;
}

int32_t handleFor(const ExValue *v) {
    if (!v) return 0;
    auto [it, added] = g_handleOf.emplace(v, int32_t(g_handles.size()));
    if (added) g_handles.push_back(v);
    return it->second;
}

const ExValue *at(int32_t h) { return h > 0 && size_t(h) < g_handles.size() ? g_handles[size_t(h)] : nullptr; }

const ExValue *item(const ExValue *v, int32_t i) {
    if (!v || v->type != ExValue::List || i < 0 || size_t(i) >= v->list.size()) return nullptr;
    return v->list[size_t(i)].get();
}

const ExTable *tableOf(const ExValue *v) { return v && v->type == ExValue::Table ? &v->table : nullptr; }

const ExValue *cell(const ExValue *v, int32_t col, int32_t row) {
    const ExTable *t = tableOf(v);
    if (!t || row < 0 || size_t(row) >= t->rows.size() || col < 0 || size_t(col) >= t->rows[size_t(row)].size()) return nullptr;
    return t->rows[size_t(row)][size_t(col)].get();
}

int columnNamed(const ExValue *v, const std::string &name) {
    const ExTable *t = tableOf(v);
    if (!t || !t->columns) return -1;
    for (size_t c = 0; c < t->columns->size(); c++)
        if ((*t->columns)[c].name == name) return int(c);
    return -1;
}

int keyColumn(const ExValue *v) {
    const ExTable *t = tableOf(v);
    if (!t || !t->columns) return -1;
    for (size_t c = 0; c < t->columns->size(); c++)
        if ((*t->columns)[c].key) return int(c);
    return t->columns->empty() ? -1 : 0;
}

int rowWithInt(const ExValue *v, int32_t key) {
    const ExTable *t = tableOf(v);
    int k = keyColumn(v);
    if (!t || k < 0) return -1;
    for (size_t r = 0; r < t->rows.size(); r++) {
        const ExValue *c = t->rows[r].size() > size_t(k) ? t->rows[r][size_t(k)].get() : nullptr;
        if (c && c->type == ExValue::Int && c->i == key) return int(r);
    }
    return -1;
}

int rowWithString(const ExValue *v, const std::string &key) {
    const ExTable *t = tableOf(v);
    int k = keyColumn(v);
    if (!t || k < 0) return -1;
    for (size_t r = 0; r < t->rows.size(); r++) {
        const ExValue *c = t->rows[r].size() > size_t(k) ? t->rows[r][size_t(k)].get() : nullptr;
        if (c && c->type == ExValue::String && c->s == key) return int(r);
    }
    return -1;
}

const ExValue *ia2(const ExValue *v, int32_t key, const std::string &col) { return cell(v, columnNamed(v, col), rowWithInt(v, key)); }
const ExValue *sa2(const ExValue *v, const std::string &key, const std::string &col) {
    return cell(v, columnNamed(v, col), rowWithString(v, key));
}
const ExValue *ra2(const ExValue *v, int32_t row, const std::string &col) { return cell(v, columnNamed(v, col), row); }

// Reading a value into the game's variable: false, leaving it alone, when the value is missing or of another type.
void getInt(HllCall &c, const ExValue *v, int arg) {
    bool ok = v && v->type == ExValue::Int;
    if (ok) c.ref(arg) = v->i;
    c.retBool(ok);
}
void getFloat(HllCall &c, const ExValue *v, int arg) {
    bool ok = v && v->type == ExValue::Float;
    if (ok) c.refF(arg) = v->f;
    c.retBool(ok);
}
void getString(HllCall &c, const ExValue *v, int arg) {
    bool ok = v && v->type == ExValue::String;
    if (ok) c.refS(arg) = v->s;
    c.retBool(ok);
}

}  // namespace

S4_HLL(MainEXFile, ReloadDebugEXFile) { c.retBool(true); }

S4_HLL(MainEXFile, Handle) {
    const ExValue *v = named(c.s(0));
    if (!v && ex() && g_missing.insert(c.s(0)).second) G().log("MainEXFile: no entry " + sjis::toUtf8(c.s(0)));
    c.retInt(handleFor(v));
}
S4_HLL(MainEXFile, AHandle) { c.retInt(handleFor(item(at(c.i(0)), c.i(1)))); }
S4_HLL(MainEXFile, A2Handle) { c.retInt(handleFor(cell(at(c.i(0)), c.i(1), c.i(2)))); }
S4_HLL(MainEXFile, IA2Handle) { c.retInt(handleFor(ia2(at(c.i(0)), c.i(1), c.s(2)))); }
S4_HLL(MainEXFile, SA2Handle) { c.retInt(handleFor(sa2(at(c.i(0)), c.s(1), c.s(2)))); }
S4_HLL(MainEXFile, RA2Handle) { c.retInt(handleFor(ra2(at(c.i(0)), c.i(1), c.s(2)))); }

S4_HLL(MainEXFile, Row) {
    const ExValue *v = at(c.i(0));
    c.retInt(v && v->type == ExValue::Table ? int32_t(v->table.rows.size()) : v && v->type == ExValue::List ? int32_t(v->list.size()) : 0);
}
S4_HLL(MainEXFile, Col) {
    const ExTable *t = tableOf(at(c.i(0)));
    c.retInt(t && t->columns ? int32_t(t->columns->size()) : 0);
}

S4_HLL(MainEXFile, Type) { const ExValue *v = at(c.i(0)); c.retInt(v ? v->type : 0); }
S4_HLL(MainEXFile, AType) { const ExValue *v = item(at(c.i(0)), c.i(1)); c.retInt(v ? v->type : 0); }
S4_HLL(MainEXFile, A2Type) { const ExValue *v = cell(at(c.i(0)), c.i(1), c.i(2)); c.retInt(v ? v->type : 0); }
S4_HLL(MainEXFile, IA2Type) { const ExValue *v = ia2(at(c.i(0)), c.i(1), c.s(2)); c.retInt(v ? v->type : 0); }
S4_HLL(MainEXFile, SA2Type) { const ExValue *v = sa2(at(c.i(0)), c.s(1), c.s(2)); c.retInt(v ? v->type : 0); }
S4_HLL(MainEXFile, RA2Type) { const ExValue *v = ra2(at(c.i(0)), c.i(1), c.s(2)); c.retInt(v ? v->type : 0); }

S4_HLL(MainEXFile, Exists) { c.retBool(at(c.i(0)) != nullptr); }
S4_HLL(MainEXFile, AExists) { c.retBool(item(at(c.i(0)), c.i(1)) != nullptr); }
S4_HLL(MainEXFile, A2Exists) { c.retBool(cell(at(c.i(0)), c.i(1), c.i(2)) != nullptr); }
S4_HLL(MainEXFile, IA2Exists) { c.retBool(ia2(at(c.i(0)), c.i(1), c.s(2)) != nullptr); }
S4_HLL(MainEXFile, SA2Exists) { c.retBool(sa2(at(c.i(0)), c.s(1), c.s(2)) != nullptr); }
S4_HLL(MainEXFile, RA2Exists) { c.retBool(ra2(at(c.i(0)), c.i(1), c.s(2)) != nullptr); }

S4_HLL(MainEXFile, Int) { getInt(c, at(c.i(0)), 1); }
S4_HLL(MainEXFile, Float) { getFloat(c, at(c.i(0)), 1); }
S4_HLL(MainEXFile, String) { getString(c, at(c.i(0)), 1); }
S4_HLL(MainEXFile, AInt) { getInt(c, item(at(c.i(0)), c.i(1)), 2); }
S4_HLL(MainEXFile, AFloat) { getFloat(c, item(at(c.i(0)), c.i(1)), 2); }
S4_HLL(MainEXFile, AString) { getString(c, item(at(c.i(0)), c.i(1)), 2); }
S4_HLL(MainEXFile, A2Int) { getInt(c, cell(at(c.i(0)), c.i(1), c.i(2)), 3); }
S4_HLL(MainEXFile, A2Float) { getFloat(c, cell(at(c.i(0)), c.i(1), c.i(2)), 3); }
S4_HLL(MainEXFile, A2String) { getString(c, cell(at(c.i(0)), c.i(1), c.i(2)), 3); }
S4_HLL(MainEXFile, IA2Int) { getInt(c, ia2(at(c.i(0)), c.i(1), c.s(2)), 3); }
S4_HLL(MainEXFile, IA2Float) { getFloat(c, ia2(at(c.i(0)), c.i(1), c.s(2)), 3); }
S4_HLL(MainEXFile, IA2String) { getString(c, ia2(at(c.i(0)), c.i(1), c.s(2)), 3); }
S4_HLL(MainEXFile, SA2Int) { getInt(c, sa2(at(c.i(0)), c.s(1), c.s(2)), 3); }
S4_HLL(MainEXFile, SA2Float) { getFloat(c, sa2(at(c.i(0)), c.s(1), c.s(2)), 3); }
S4_HLL(MainEXFile, SA2String) { getString(c, sa2(at(c.i(0)), c.s(1), c.s(2)), 3); }
S4_HLL(MainEXFile, RA2Int) { getInt(c, ra2(at(c.i(0)), c.i(1), c.s(2)), 3); }
S4_HLL(MainEXFile, RA2Float) { getFloat(c, ra2(at(c.i(0)), c.i(1), c.s(2)), 3); }
S4_HLL(MainEXFile, RA2String) { getString(c, ra2(at(c.i(0)), c.i(1), c.s(2)), 3); }

S4_HLL(MainEXFile, GetRowAtIntKey) { c.retInt(rowWithInt(at(c.i(0)), c.i(1))); }
S4_HLL(MainEXFile, GetRowAtStringKey) { c.retInt(rowWithString(at(c.i(0)), c.s(1))); }

}  // namespace s4

namespace s4 {
// Tree nodes: the names of a node's children (Node), and of those children that hold a value (EX).
namespace {
std::vector<const ExNode *> childrenOf(const std::string &path, bool withValue) {
    std::vector<const ExNode *> out;
    if (const ExNode *n = nodeAt(path))
        for (auto &c : n->children)
            if (!withValue || c->value) out.push_back(c.get());
    return out;
}
}  // namespace
S4_HLL(MainEXFile, GetNodeNameCount) { c.retInt(int32_t(childrenOf(c.s(0), false).size())); }
S4_HLL(MainEXFile, GetEXNameCount) { c.retInt(int32_t(childrenOf(c.s(0), true).size())); }
S4_HLL(MainEXFile, GetNodeName) {
    auto kids = childrenOf(c.s(0), false);
    int32_t i = c.i(1);
    bool ok = i >= 0 && size_t(i) < kids.size();
    if (ok) c.refS(2) = kids[size_t(i)]->name;
    c.retBool(ok);
}
S4_HLL(MainEXFile, GetEXName) {
    auto kids = childrenOf(c.s(0), true);
    int32_t i = c.i(1);
    bool ok = i >= 0 && size_t(i) < kids.size();
    if (ok) c.refS(2) = kids[size_t(i)]->name;
    c.retBool(ok);
}
}  // namespace s4

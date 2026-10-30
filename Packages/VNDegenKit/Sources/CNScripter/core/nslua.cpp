// NSLua: the Lua scripting NScripter's nslua.dll adds, as its author's notes describe it. A game's system.lua runs
// when the engine starts, before the define block. `luasub name` makes `name` a command that calls the Lua function
// NSCOM_name, which reads its arguments with NSPopInt and the like; `luacall name` asks for the callback NSCALL_name
// (animation, save, load, reset, ...). Lua calls back into the engine through the NS* functions below.
#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

#include "engine.h"

extern "C" {
#include "../third_party/lua/lauxlib.h"
#include "../third_party/lua/lua.h"
#include "../third_party/lua/lualib.h"
}

namespace vnns {

namespace {

Engine &engineOf(lua_State *L) { return *static_cast<Engine *>(lua_touserdata(L, lua_upvalueindex(1))); }

// Each NS function gets the engine as an upvalue.
#define NSFN(name) int name(lua_State *L)

// ---- arguments of a luasub command ----

NSFN(nsPopInt) {
    lua_pushinteger(L, engineOf(L).readInt());
    return 1;
}
NSFN(nsPopIntRef) {
    Engine &e = engineOf(L);
    Engine::VarRef v = e.readVar();
    if (v.kind != Engine::VarRef::Num) e.errorf("NSPopIntRef: expected a numeric variable");
    lua_pushinteger(L, v.index);
    return 1;
}
NSFN(nsPopStr) {
    std::string s = engineOf(L).readStr();
    lua_pushlstring(L, s.data(), s.size());
    return 1;
}
NSFN(nsPopStrRef) {
    Engine &e = engineOf(L);
    Engine::VarRef v = e.readVar();
    if (v.kind != Engine::VarRef::Str) e.errorf("NSPopStrRef: expected a string variable");
    lua_pushinteger(L, v.index);
    return 1;
}
NSFN(nsPopLabel) {
    std::string s = sjisLower(engineOf(L).readLabel());
    lua_pushlstring(L, s.data(), s.size());
    return 1;
}
NSFN(nsPopID) {
    std::string s = engineOf(L).readWord();
    lua_pushlstring(L, s.data(), s.size());
    return 1;
}
NSFN(nsPopComma) {
    engineOf(L).readComma();
    return 0;
}
NSFN(nsCheckComma) {
    Engine &e = engineOf(L);
    e.skipSpaces();
    lua_pushboolean(L, e.peek() == ',');
    return 1;
}

// ---- variables ----

NSFN(nsSetIntValue) {
    engineOf(L).setNumVar(int(luaL_checkinteger(L, 1)), int(luaL_checkinteger(L, 2)));
    return 0;
}
NSFN(nsSetStrValue) {
    size_t n = 0;
    const char *s = luaL_checklstring(L, 2, &n);
    engineOf(L).strVar(int(luaL_checkinteger(L, 1))).assign(s, n);
    return 0;
}
NSFN(nsGetIntValue) {
    lua_pushinteger(L, engineOf(L).getNumVar(int(luaL_checkinteger(L, 1))));
    return 1;
}
NSFN(nsGetStrValue) {
    const std::string &s = engineOf(L).strVar(int(luaL_checkinteger(L, 1)));
    lua_pushlstring(L, s.data(), s.size());
    return 1;
}

// ---- running the script ----

NSFN(nsExec) {
    engineOf(L).execString(luaL_checkstring(L, 1));
    return 0;
}
// NScripter's notes forbid jumps from a statement NSExec runs; here one would move the cursor of a statement that
// ends when NSExec returns, so it's refused.
bool insideExec(Engine &e, const char *what) {
    if (e.script().execDepth() == 0) return false;
    logf("%s inside NSExec is not allowed; ignored", what);
    return true;
}
NSFN(nsGoto) {
    Engine &e = engineOf(L);
    if (insideExec(e, "NSGoto")) return 0;
    std::string label = luaL_checkstring(L, 1);
    if (!label.empty() && label[0] == '*') label.erase(0, 1);
    e.inTextLine_ = false;
    e.jumpToLabel(label);
    return 0;
}
NSFN(nsGosub) {
    Engine &e = engineOf(L);
    if (insideExec(e, "NSGosub")) return 0;
    std::string label = luaL_checkstring(L, 1);
    if (!label.empty() && label[0] == '*') label.erase(0, 1);
    e.inTextLine_ = false;
    e.gosub(e.luaReturn_.pos, e.luaReturn_.line, e.luaReturn_.stmt, label);
    return 0;
}
NSFN(nsReturn) {
    Engine &e = engineOf(L);
    if (insideExec(e, "NSReturn")) return 0;
    e.inTextLine_ = false;
    e.returnFromSub("");
    return 0;
}
// Lua runs on the game thread, so a function that polls NSDoEvents in a loop holds the frame. The PC engine processes
// its window messages here; the runtime can't, so after a few seconds it answers true, which asks the function to
// return, as it would when the player closes the window.
constexpr auto kLuaBudget = std::chrono::seconds(3);
NSFN(nsDoEvents) {
    Engine &e = engineOf(L);
    bool over = std::chrono::steady_clock::now() - e.luaCallStart_ > kLuaBudget;
    if (over) logf("NSDoEvents: a Lua function has run for %d s; asking it to return", int(kLuaBudget.count()));
    lua_pushboolean(L, e.ended_ || over);
    return 1;
}
NSFN(nsEnd) {
    engineOf(L).requestQuit();
    return 0;
}
NSFN(nsOkBox) {
    // A modal Windows message box; the runtime has no blocking dialog, so the text goes to the log.
    logf("NSOkBox: %s", sjisToUtf8(luaL_checkstring(L, 1)).c_str());
    return 0;
}
NSFN(nsYesNoBox) {
    logf("NSYesNoBox (answered yes): %s", sjisToUtf8(luaL_checkstring(L, 1)).c_str());
    lua_pushboolean(L, 1);
    return 1;
}
NSFN(nsGetWindowSize) {
    Engine &e = engineOf(L);
    lua_pushinteger(L, e.width());
    lua_pushinteger(L, e.height());
    return 2;
}
NSFN(nsGetSkip) {
    Engine &e = engineOf(L);
    lua_pushinteger(L, e.skipMode_ ? 1 : e.autoMode_ ? 2 : 0);
    return 1;
}
NSFN(nsTimer) {
    // The PC's tick count: wall time, monotonic, advancing while a Lua function runs, so a timed loop ends.
    lua_pushinteger(L, lua_Integer(engineOf(L).wallMs()));
    return 1;
}
NSFN(nsSleep) {
    // Lua runs on the game thread, so a sleep holds the frame; the scripts that call it ask for a millisecond or two.
    int ms = std::clamp(int(luaL_checkinteger(L, 1)), 0, 10);
    if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return 0;
}
NSFN(nsLuaAnimationInterval) {
    engineOf(L).luaAnimInterval_ = std::max(1, int(luaL_checkinteger(L, 1)));
    return 0;
}
NSFN(nsLuaAnimationMode) {
    Engine &e = engineOf(L);
    e.luaAnimOn_ = lua_toboolean(L, 1) != 0;
    e.luaAnimNext_ = e.now_;
    return 0;
}
NSFN(nsGetMouse) {
    Engine &e = engineOf(L);
    bool inside = e.mouseX_ >= 0 && e.mouseY_ >= 0 && e.mouseX_ < e.width() && e.mouseY_ < e.height();
    lua_pushinteger(L, inside ? e.mouseX_ : -1);
    lua_pushinteger(L, inside ? e.mouseY_ : -1);
    return 2;
}
NSFN(nsGetClick) {
    Engine &e = engineOf(L);
    lua_pushboolean(L, e.in_.clickUp);
    lua_pushboolean(L, e.in_.rclick);
    lua_pushinteger(L, e.in_.wheel);
    lua_pushboolean(L, e.in_.click);
    lua_pushboolean(L, 0);
    return 5;
}
NSFN(nsGetKey) {
    Engine &e = engineOf(L);
    std::string name = luaL_checkstring(L, 1);
    for (char &c : name) c = char(toupper(uint8_t(c)));
    int code = -1;
    static const std::pair<const char *, int> keys[] = {
        {"ESC", VNNS_KEY_ESCAPE},   {"SPACE", VNNS_KEY_SPACE},     {"RETURN", VNNS_KEY_RETURN},
        {"ENTER", VNNS_KEY_RETURN}, {"CTRL", VNNS_KEY_CTRL},       {"UP", VNNS_KEY_UP},
        {"DOWN", VNNS_KEY_DOWN},    {"LEFT", VNNS_KEY_LEFT},       {"RIGHT", VNNS_KEY_RIGHT},
        {"PAGEUP", VNNS_KEY_PAGEUP}, {"PAGEDOWN", VNNS_KEY_PAGEDOWN}, {"SHIFT", VNNS_KEY_SHIFT}};
    for (auto &[k, v] : keys)
        if (name == k) code = v;
    if (code < 0 && name.size() == 1) code = uint8_t(name[0]);
    if (code < 0 && name.size() >= 2 && name[0] == 'F') code = VNNS_KEY_F1 + atoi(name.c_str() + 1) - 1;
    lua_pushboolean(L, code >= 0 && e.keysDown_.count(code));
    return 1;
}
NSFN(nsSystemCall) {
    Engine &e = engineOf(L);
    std::string id = sjisLower(luaL_checkstring(L, 1));
    if (id == "end" || id == "close") e.requestQuit();
    else if (id == "reset") e.resetGame();
    else if (id == "skip") e.skipMode_ = true;
    else if (id == "skipoff") e.skipMode_ = false;
    else if (id == "automode") e.autoMode_ = true;
    else if (id != "full" && id != "window") logf("NSSystemCall %s is not emulated", id.c_str());
    return 0;
}
NSFN(nsGetFileInfo) {
    // Where a file lives: its archive, offset and size. The runtime reads archives itself, so a file that exists is
    // reported by its own name as a loose file would be.
    Engine &e = engineOf(L);
    const char *name = luaL_checkstring(L, 1);
    if (!e.fs_.exists(name)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, name);
    lua_pushinteger(L, 0);
    lua_pushinteger(L, 0);
    return 3;
}
NSFN(nsExecAnimation) {
    engineOf(L).animateSprites();
    return 0;
}
NSFN(nsInvalidate) {
    engineOf(L).sceneDirty_ = true;
    return 0;
}
NSFN(nsUpdate) {
    engineOf(L).printWithEffect(1);
    return 0;
}

// ---- sprites, through the commands they stand for ----

std::string quoted(lua_State *L, int index) {
    size_t n = 0;
    const char *s = luaL_checklstring(L, index, &n);
    return "\"" + std::string(s, n) + "\"";
}
std::string num(lua_State *L, int index) {
    lua_Number v = luaL_checknumber(L, index);
    return std::to_string(long(v));
}

NSFN(nsSpLoad) {
    Engine &e = engineOf(L);
    // Loaded off screen, as the notes say; a move brings it on.
    e.execString("lsp " + num(L, 1) + "," + quoted(L, 2) + "," + std::to_string(e.width()) + "," + std::to_string(e.height()));
    return 0;
}
NSFN(nsSpMove) {
    engineOf(L).execString("amsp " + num(L, 1) + "," + num(L, 2) + "," + num(L, 3) + "," + num(L, 4));
    return 0;
}
NSFN(nsSpClear) {
    engineOf(L).execString("csp " + num(L, 1));
    return 0;
}
NSFN(nsSpVisible) {
    engineOf(L).execString("vsp " + num(L, 1) + "," + (lua_toboolean(L, 2) ? "1" : "0"));
    return 0;
}
NSFN(nsSpCell) {
    engineOf(L).execString("cell " + num(L, 1) + "," + num(L, 2));
    return 0;
}
NSFN(nsSp2Load) {
    Engine &e = engineOf(L);
    e.execString("lsp2 " + num(L, 1) + "," + quoted(L, 2) + "," + std::to_string(e.width() * 2) + "," +
                 std::to_string(e.height() * 2) + ",100,100,0");
    return 0;
}
NSFN(nsSp2Move) {
    std::string args;
    for (int i = 2; i <= 8; i++) args += (i > 2 ? "," : "") + num(L, i);
    engineOf(L).execString("amsp2 " + num(L, 1) + "," + args);
    return 0;
}
NSFN(nsSp2Clear) {
    engineOf(L).execString("csp2 " + num(L, 1));
    return 0;
}
NSFN(nsSp2Visible) {
    engineOf(L).execString("vsp2 " + num(L, 1) + "," + (lua_toboolean(L, 2) ? "1" : "0"));
    return 0;
}
NSFN(nsSpGetInfo) {
    Engine &e = engineOf(L);
    Sprite *s = e.spriteRef(int(luaL_checkinteger(L, 1)));
    bool ok = s && s->loaded && s->img.image;
    lua_pushinteger(L, ok ? s->img.cellWidth : 0);
    lua_pushinteger(L, ok ? s->img.image->h : 0);
    lua_pushinteger(L, ok ? std::max(1, s->img.cells) : 0);
    return 3;
}
NSFN(nsSpGetPos) {
    Engine &e = engineOf(L);
    Sprite *s = e.spriteRef(int(luaL_checkinteger(L, 1)));
    lua_pushinteger(L, s ? s->x : 0);
    lua_pushinteger(L, s ? s->y : 0);
    lua_pushinteger(L, s ? s->drawAlpha() : 0);
    return 3;
}
NSFN(nsSp2GetInfo) {
    Engine &e = engineOf(L);
    int n = int(luaL_checkinteger(L, 1));
    const Sprite2 *s = n >= 0 && n < kSprite2Count ? &e.scene_.sp2[size_t(n)] : nullptr;
    bool ok = s && s->loaded && s->img.image;
    lua_pushinteger(L, ok ? s->img.cellWidth : 0);
    lua_pushinteger(L, ok ? s->img.image->h : 0);
    return 2;
}
NSFN(nsSp2GetPos) {
    Engine &e = engineOf(L);
    int n = int(luaL_checkinteger(L, 1));
    const Sprite2 *s = n >= 0 && n < kSprite2Count ? &e.scene_.sp2[size_t(n)] : nullptr;
    lua_pushinteger(L, s ? s->cx : 0);
    lua_pushinteger(L, s ? s->cy : 0);
    lua_pushinteger(L, s ? s->scaleX : 0);
    lua_pushinteger(L, s ? s->scaleY : 0);
    lua_pushinteger(L, s ? s->rot : 0);
    lua_pushinteger(L, s ? (s->alpha < 0 ? 255 : s->alpha) : 0);
    lua_pushinteger(L, s ? int(s->blend) : 0);
    return 7;
}

// ---- streamed sound (NSOgg*, the dwave channels) ----

NSFN(nsOggLoad) {
    engineOf(L).execString("dwaveload " + num(L, 1) + "," + quoted(L, 2));
    return 0;
}
NSFN(nsOggPlay) {
    engineOf(L).execString(std::string(lua_toboolean(L, 2) ? "dwaveplayloop " : "dwaveplay ") + num(L, 1));
    return 0;
}
NSFN(nsOggPlay2) {
    engineOf(L).execString("dwaveplayloop " + num(L, 1));
    return 0;
}
NSFN(nsOggClose) {
    engineOf(L).execString("dwavestop " + num(L, 1));
    return 0;
}
NSFN(nsOggVolume) {
    // 0 is full and -10000 silent, as DirectSound counts.
    int v = std::clamp(int(luaL_checkinteger(L, 2)), -10000, 0);
    engineOf(L).execString("chvol " + num(L, 1) + "," + std::to_string((v + 10000) / 100));
    return 0;
}
NSFN(nsOggIsPlaying) {
    Engine &e = engineOf(L);
    int ch = int(luaL_checkinteger(L, 1));
    lua_pushboolean(L, ch >= 0 && ch < 50 && e.audio_.playing(ch));
    return 1;
}
NSFN(nsOggFade) {
    Engine &e = engineOf(L);
    int ch = int(luaL_checkinteger(L, 1));
    int ms = std::max(0, int(luaL_checkinteger(L, 4)));
    if (ch < 0 || ch >= 50) return 0;
    if (lua_toboolean(L, 5)) {
        e.audio_.stop(ch, ms);
    } else {
        int v = std::clamp(int(luaL_checkinteger(L, 3)), -10000, 0);
        e.execString("chvol " + std::to_string(ch) + "," + std::to_string((v + 10000) / 100));
    }
    return 0;
}

NSFN(nsLinear) {
    double x0 = luaL_checknumber(L, 1), y0 = luaL_checknumber(L, 2), x1 = luaL_checknumber(L, 3),
           y1 = luaL_checknumber(L, 4), x = luaL_checknumber(L, 5);
    lua_pushnumber(L, x1 == x0 ? y0 : y0 + (y1 - y0) * (x - x0) / (x1 - x0));
    return 1;
}

NSFN(nsNotEmulated) {
    lua_Debug ar;
    const char *name = lua_getstack(L, 0, &ar) && lua_getinfo(L, "n", &ar) && ar.name ? ar.name : "?";
    logf("NSLua function %s is not emulated", name);
    return 0;
}

NSFN(nlDofile) {
    Engine &e = engineOf(L);
    const char *name = luaL_checkstring(L, 1);
    // Lua errors leave through longjmp, which skips C++ destructors, so the buffer dies before anything can raise.
    int status;
    {
        Bytes data;
        if (!e.fs_.read(name, data)) status = -1;
        else status = luaL_loadbuffer(L, reinterpret_cast<const char *>(data.data()), data.size(), name);
    }
    if (status < 0) return luaL_error(L, "NL_dofile: %s not found", name);
    if (status != 0) return lua_error(L);
    lua_call(L, 0, LUA_MULTRET);
    return lua_gettop(L) - 1;
}

NSFN(luaPrint) {
    std::string line;
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        size_t len = 0;
        const char *s = lua_tolstring(L, i, &len);
        if (i > 1) line += '\t';
        line += s ? std::string(s, len) : luaL_typename(L, i);
    }
    logf("Lua: %s", sjisToUtf8(line).c_str());
    return 0;
}

const struct {
    const char *name;
    lua_CFunction fn;
} kFunctions[] = {
    {"NSPopInt", nsPopInt}, {"NSPopIntRef", nsPopIntRef}, {"NSPopStr", nsPopStr}, {"NSPopStrRef", nsPopStrRef},
    {"NSPopLabel", nsPopLabel}, {"NSPopID", nsPopID}, {"NSPopComma", nsPopComma}, {"NSCheckComma", nsCheckComma},
    {"NSSetIntValue", nsSetIntValue}, {"NSSetStrValue", nsSetStrValue}, {"NSGetIntValue", nsGetIntValue},
    {"NSGetStrValue", nsGetStrValue}, {"NSExec", nsExec}, {"NSGoto", nsGoto}, {"NSGosub", nsGosub},
    {"NSReturn", nsReturn}, {"NSDoEvents", nsDoEvents}, {"NSEnd", nsEnd}, {"NSClick", nsNotEmulated},
    {"NSOkBox", nsOkBox}, {"NSYesNoBox", nsYesNoBox}, {"NSGetWindowSize", nsGetWindowSize}, {"NSGetSkip", nsGetSkip},
    {"NSTimer", nsTimer}, {"NSSleep", nsSleep}, {"NSLuaAnimationInterval", nsLuaAnimationInterval},
    {"NSLuaAnimationMode", nsLuaAnimationMode}, {"NSGetMouse", nsGetMouse}, {"NSGetClick", nsGetClick},
    {"NSGetKey", nsGetKey}, {"NSSystemCall", nsSystemCall}, {"NSGetFileInfo", nsGetFileInfo},
    {"NSExecAnimation", nsExecAnimation}, {"NSInvalidate", nsInvalidate}, {"NSUpdate", nsUpdate},
    {"NSSpLoad", nsSpLoad}, {"NSSpMove", nsSpMove}, {"NSSpClear", nsSpClear}, {"NSSpVisible", nsSpVisible},
    {"NSSpCell", nsSpCell}, {"NSSp2Load", nsSp2Load}, {"NSSp2Move", nsSp2Move}, {"NSSp2Clear", nsSp2Clear},
    {"NSSp2Visible", nsSp2Visible}, {"NSSpGetInfo", nsSpGetInfo}, {"NSSpGetPos", nsSpGetPos},
    {"NSSp2GetInfo", nsSp2GetInfo}, {"NSSp2GetPos", nsSp2GetPos}, {"NSOggLoad", nsOggLoad}, {"NSOggPlay", nsOggPlay},
    {"NSOggPlay2", nsOggPlay2}, {"NSOggClose", nsOggClose}, {"NSOggVolume", nsOggVolume},
    {"NSOggIsPlaying", nsOggIsPlaying}, {"NSOggFade", nsOggFade}, {"NSLinear", nsLinear},
    {"NSDefSpline", nsNotEmulated}, {"NSSpline", nsNotEmulated}, {"NL_dofile", nlDofile}, {"NL_bind", nsNotEmulated},
    {"print", luaPrint},
};

// The NSD* direct-drawing functions, which no test game uses.
const char *kDrawingFunctions[] = {"NSDClear", "NSDFill", "NSDLoad", "NSDDelete", "NSDGetSize", "NSDBlt", "NSDSp",
                                   "NSDSpAdd", "NSDSp2", "NSDSp2Add", "NSDPresent", "NSDPresentRect", "NSDCopyToBg",
                                   "NSDOffMode", "NSDTransition", "NSDTransitionF", "NSDDLL", "NSDCall", "NSDSetSprite"};

// Notes when the outermost Lua call of a step began, for NSDoEvents' budget.
struct LuaCall {
    Engine &e;
    bool outermost;
    explicit LuaCall(Engine &eng) : e(eng), outermost(e.script().execDepth() == 0 && !e.inLua_) {
        if (outermost) {
            e.luaCallStart_ = std::chrono::steady_clock::now();
            e.inLua_ = true;
        }
    }
    ~LuaCall() {
        if (outermost) e.inLua_ = false;
    }
};

void report(lua_State *L, const char *what) {
    const char *msg = lua_tostring(L, -1);
    logf("Lua error in %s: %s", what, msg ? sjisToUtf8(msg).c_str() : "?");
    lua_pop(L, 1);
}

}  // namespace

void Engine::luaStart() {
    Bytes src;
    if (!fs_.read("system.lua", src)) return;
    lua_ = luaL_newstate();
    if (!lua_) return;
    lua_State *L = lua_;
    // The base, table, string and math libraries. Not io, os, package or debug: a game's script gets files only through
    // NL_dofile and the engine's commands, and no shell, process or module loading. dofile, loadfile and require read
    // the host's files, so they go too.
    for (lua_CFunction open : {luaopen_base, luaopen_table, luaopen_string, luaopen_math}) {
        lua_pushcfunction(L, open);
        lua_call(L, 0, 0);
    }
    for (const char *name : {"dofile", "loadfile", "require"}) {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }
    for (auto &f : kFunctions) {
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, f.fn, 1);
        lua_setglobal(L, f.name);
    }
    for (const char *name : kDrawingFunctions) {
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, nsNotEmulated, 1);
        lua_setglobal(L, name);
    }
    luaReturn_ = cur_;
    LuaCall call(*this);
    logf("system.lua: running it (NSLua)");
    int status = luaL_loadbuffer(L, reinterpret_cast<const char *>(src.data()), src.size(), "system.lua");
    src = Bytes();  // nothing of ours may be alive across a Lua error, which longjmps past destructors
    if (status != 0 || lua_pcall(L, 0, 0, 0) != 0) report(L, "system.lua");
}

void Engine::luaStop() {
    if (lua_) lua_close(lua_);
    lua_ = nullptr;
}

bool Engine::luaCommand(const std::string &name) {
    if (!lua_ || !luaCommands_.count(name)) return false;
    lua_State *L = lua_;
    // The statement's end: NSGosub returns there, and the cursor lands there unless the function jumped.
    Cursor start = cur_;
    skipStatement();
    Cursor end = cur_;
    cur_ = start;
    // A command run from inside another Lua call (through NSExec) must leave the outer call's return cursor alone.
    Cursor outerReturn = luaReturn_;
    luaReturn_ = end;
    LuaCall call(*this);
    int jumps = jumps_;
    lua_getglobal(L, ("NSCOM_" + name).c_str());
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        errorf("luasub %s: NSCOM_%s is not defined", name.c_str(), name.c_str());
        cur_ = end;
        luaReturn_ = outerReturn;
        return true;
    }
    if (lua_pcall(L, 0, 0, 0) != 0) report(L, ("NSCOM_" + name).c_str());
    if (jumps_ == jumps && !ended_) cur_ = end;
    luaReturn_ = outerReturn;
    return true;
}

bool Engine::luaCallback(const std::string &name, int arg, bool *result) {
    if (!lua_ || !luaCallbacks_.count(name)) return false;
    lua_State *L = lua_;
    // The notes name the save and load callbacks NSCALL_Save and NSCALL_Load; a game defines them in lowercase.
    std::string fn = "NSCALL_" + name;
    lua_getglobal(L, fn.c_str());
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        fn[7] = char(toupper(uint8_t(fn[7])));
        lua_getglobal(L, fn.c_str());
    }
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return false;
    }
    Cursor outerReturn = luaReturn_;
    luaReturn_ = cur_;
    LuaCall call(*this);
    int args = 0;
    if (arg >= 0) {
        lua_pushinteger(L, arg);
        args = 1;
    }
    bool ok = lua_pcall(L, args, 1, 0) == 0;
    if (!ok) report(L, fn.c_str());
    else {
        if (result) *result = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
    }
    luaReturn_ = outerReturn;
    return ok;
}

// Runs one statement given as text, where the script is: NSExec. Jumps aren't allowed in it, so the cursor comes
// back to where it was.
void Engine::execString(const std::string &statement) {
    Cursor saved = cur_;
    bool savedInText = inTextLine_, savedContinue = continueLine_;
    int savedJumps = jumps_;
    script_.beginExec(statement);
    cur_ = Cursor();
    inTextLine_ = false;
    skipSpaces();
    std::string word = readWord();
    if (word.empty()) logf("NSExec: no command in \"%s\"", sjisToUtf8(statement).c_str());
    else executeCommand(word);
    script_.endExec();
    cur_ = saved;
    inTextLine_ = savedInText;
    continueLine_ = savedContinue;
    jumps_ = savedJumps;
}

namespace {
using E = Engine;

void cmdLuaSub(E &e, const std::string &) {
    std::string name = e.readWord();
    if (name.empty()) return;
    if (!e.lua_) logf("luasub %s: the game has no system.lua", name.c_str());
    e.luaCommands_.insert(name);
}

void cmdLuaCall(E &e, const std::string &) {
    std::string name = e.readWord();
    if (name.empty()) return;
    static const char *known[] = {"animation", "save", "load", "reset"};
    if (std::find_if(std::begin(known), std::end(known), [&](const char *k) { return name == k; }) == std::end(known))
        logf("luacall %s is not emulated", name.c_str());
    e.luaCallbacks_.insert(name);
}

}  // namespace

void registerLuaCommands(std::unordered_map<std::string, CommandFn> &t) {
    t["luasub"] = cmdLuaSub;
    t["luacall"] = cmdLuaCall;
}

}  // namespace vnns

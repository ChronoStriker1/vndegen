// Small HLL libraries the games share: Math, MsgSkip, SystemService, OutputLog, InputString, PassRegister,
// VSFile, FileOperation, Confirm3, AliceLogo4, IbisInputEngine, DrawPluginManager, MusicSystem. Behaviour follows the
// SDK manuals where they describe a library, otherwise the names and how the games use the results.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>

#include "game.h"
#include "save.h"
#include "sjis.h"

namespace fs = std::filesystem;

namespace s4 {
namespace {

Game &G() { return game(); }
constexpr float kDeg = 3.14159265358979f / 180.0f;

// ---- Math ----
std::mt19937 g_rng(1);
S4_HLL(Math, Cos) { c.retFloat(std::cos(c.f(0) * kDeg)); }
S4_HLL(Math, Sin) { c.retFloat(std::sin(c.f(0) * kDeg)); }
S4_HLL(Math, Tan) { c.retFloat(std::tan(c.f(0) * kDeg)); }
S4_HLL(Math, Sqrt) { c.retFloat(std::sqrt(std::max(0.0f, c.f(0)))); }
S4_HLL(Math, Atan) { c.retFloat(std::atan(c.f(0)) / kDeg); }
S4_HLL(Math, Atan2) { c.retFloat(std::atan2(c.f(0), c.f(1)) / kDeg); }
S4_HLL(Math, Abs) { c.retInt(std::abs(c.i(0))); }
S4_HLL(Math, AbsF) { c.retFloat(std::fabs(c.f(0))); }
S4_HLL(Math, Pow) { c.retFloat(std::pow(c.f(0), c.f(1))); }
S4_HLL(Math, SetSeed) { g_rng.seed(uint32_t(c.i(0))); }
S4_HLL(Math, SetSeedByCurrentTime) { g_rng.seed(uint32_t(std::random_device{}())); }
S4_HLL(Math, Rand) { c.retInt(int32_t(g_rng() & 0x7FFFFFFF)); }
S4_HLL(Math, RandF) { c.retFloat(float(g_rng() & 0xFFFFFF) / float(0x1000000)); }
S4_HLL(Math, Min) { c.retInt(std::min(c.i(0), c.i(1))); }
S4_HLL(Math, MinF) { c.retFloat(std::min(c.f(0), c.f(1))); }
S4_HLL(Math, Max) { c.retInt(std::max(c.i(0), c.i(1))); }
S4_HLL(Math, MaxF) { c.retFloat(std::max(c.f(0), c.f(1))); }
S4_HLL(Math, Swap) { std::swap(c.ref(0), c.ref(1)); }
S4_HLL(Math, SwapF) { std::swap(c.ref(0), c.ref(1)); }
S4_HLL(Math, Log) { c.retFloat(std::log(c.f(0))); }
S4_HLL(Math, Log10) { c.retFloat(std::log10(c.f(0))); }
S4_HLL(Math, Ceil) { c.retInt(int32_t(std::ceil(c.f(0)))); }
S4_HLL(Math, Floor) { c.retInt(int32_t(std::floor(c.f(0)))); }
S4_HLL(Math, Round) { c.retInt(int32_t(std::lround(c.f(0)))); }
S4_HLL(Math, Clamp) { c.retInt(std::clamp(c.i(0), c.i(1), std::max(c.i(1), c.i(2)))); }
S4_HLL(Math, ClampF) { c.retFloat(std::clamp(c.f(0), c.f(1), std::max(c.f(1), c.f(2)))); }

// ---- MsgSkip: which messages the player has read, kept in the save folder ----
struct MsgSkipState {
    std::string file;
    std::vector<uint8_t> flags;
    int state = 0, enable = 1, use = 1;
} g_skip;
// The file, as Test Game 46's PC copy has it: u32 the number of messages, then one bit per message (bit n%8 of byte
// n/8, an assumption: the PC file had no bit set).
void skipSave() {
    if (g_skip.file.empty()) return;
    std::ofstream f(save::hostPath(*G().vm, g_skip.file, true), std::ios::binary);
    uint32_t n = uint32_t(G().vm->ain.messages.size());
    f.write(reinterpret_cast<const char *>(&n), 4);
    f.write(reinterpret_cast<const char *>(g_skip.flags.data()), std::streamsize(g_skip.flags.size()));
}
S4_HLL(MsgSkip, Init) {
    g_skip.file = c.s(0);
    g_skip.flags.assign((G().vm->ain.messages.size() + 7) / 8, 0);
    std::ifstream f(save::hostPath(*G().vm, g_skip.file, true), std::ios::binary);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
    if (d.size() >= 4) {
        size_t bits = d.size() - 4;
        std::copy_n(d.begin() + 4, std::min(bits, g_skip.flags.size()), g_skip.flags.begin());
    }
    c.retInt(1);
}
S4_HLL(MsgSkip, UseFlag) { g_skip.use = c.i(0); }
S4_HLL(MsgSkip, SetEnable) { g_skip.enable = c.i(0); }
S4_HLL(MsgSkip, SetState) { g_skip.state = c.i(0); }
S4_HLL(MsgSkip, GetEnable) { c.retInt(g_skip.enable); }
S4_HLL(MsgSkip, GetState) { c.retInt(g_skip.state); }
S4_HLL(MsgSkip, SetFlag) {
    size_t n = size_t(c.i(0));
    if (n / 8 < g_skip.flags.size() && !(g_skip.flags[n / 8] & (1 << (n % 8)))) {
        g_skip.flags[n / 8] |= uint8_t(1 << (n % 8));
        skipSave();
    }
}
S4_HLL(MsgSkip, GetFlag) {
    size_t n = size_t(c.i(0));
    c.retInt(n / 8 < g_skip.flags.size() && (g_skip.flags[n / 8] & (1 << (n % 8))) ? 1 : 0);
}
S4_HLL(MsgSkip, GetNumofMsg) { c.retInt(int32_t(G().vm->ain.messages.size())); }
S4_HLL(MsgSkip, GetNumofFlag) {
    int n = 0;
    for (uint8_t b : g_skip.flags) n += __builtin_popcount(b);
    c.retInt(n);
}

// ---- SystemService ----
int g_mixerVolume[8] = {80, 80, 80, 80, 80, 80, 80, 80};
bool g_mixerMute[8] = {};
S4_HLL(SystemService, GetMixerNumof) { c.retInt(4); }
S4_HLL(SystemService, GetMixerName) { c.refS(1) = ""; c.retBool(c.i(0) >= 0 && c.i(0) < 4); }
S4_HLL(SystemService, GetMixerVolume) {
    int n = c.i(0);
    if (n < 0 || n >= 8) { c.retBool(false); return; }
    c.ref(1) = g_mixerVolume[n];
    c.retBool(true);
}
S4_HLL(SystemService, GetMixerMute) {
    int n = c.i(0);
    if (n < 0 || n >= 8) { c.retBool(false); return; }
    c.ref(1) = g_mixerMute[n];
    c.retBool(true);
}
S4_HLL(SystemService, SetMixerName) { c.retBool(true); }
S4_HLL(SystemService, SetMixerVolume) {
    int n = c.i(0);
    if (n >= 0 && n < 8) {
        g_mixerVolume[n] = c.i(1);
        G().audio.setGroupVolume(n, g_mixerMute[n] ? 0 : c.i(1));
    }
    c.retBool(true);
}
S4_HLL(SystemService, SetMixerMute) {
    int n = c.i(0);
    if (n >= 0 && n < 8) {
        g_mixerMute[n] = c.b(1);
        G().audio.setGroupVolume(n, g_mixerMute[n] ? 0 : g_mixerVolume[n]);
    }
    c.retBool(true);
}
S4_HLL(SystemService, GetGameVersion) { c.retInt(G().vm->ain.gameVersion); }
S4_HLL(SystemService, GetGameName) { c.refS(0) = G().vm->gameName; }
// Test Game 56's main loops until this fills its string so that characters 0 and 16 are '=', character 8 is '/',
// and the two-digit numbers at 6 and 14 match. Its meaning is unknown; this is the plainest string of that shape.
S4_HLL(SystemService, Rance96161988) { c.refS(0) = "=0000000/0000000="; }
S4_HLL(SystemService, GetViewWidth) { c.retInt(G().screenW); }
S4_HLL(SystemService, GetViewHeight) { c.retInt(G().screenH); }
S4_HLL(SystemService, IsFullScreen) { c.retBool(true); }
S4_HLL(SystemService, ChangeNormalScreen) { c.retBool(true); }
S4_HLL(SystemService, ChangeFullScreen) { c.retBool(true); }
S4_HLL(SystemService, InitMainWindowPosAndSize) { c.retBool(true); }
S4_HLL(SystemService, MoveMouseCursorPosImmediately) { G().mouseMove(c.i(0), c.i(1)); c.retBool(true); }
S4_HLL(SystemService, SetHideMouseCursorByGame) { c.retBool(true); }
S4_HLL(SystemService, GetHideMouseCursorByGame) { c.retBool(false); }
S4_HLL(SystemService, SetUsePower2Texture) { c.retBool(true); }
S4_HLL(SystemService, GetUsePower2Texture) { c.retBool(false); }
std::map<int, int> g_windowSettings, g_cursorConfig;
S4_HLL(SystemService, SetWindowSetting) { g_windowSettings[c.i(0)] = c.i(1); c.retBool(true); }
S4_HLL(SystemService, GetWindowSetting) { c.ref(1) = g_windowSettings[c.i(0)]; c.retBool(true); }
S4_HLL(SystemService, SetMouseCursorConfig) { g_cursorConfig[c.i(0)] = c.i(1); c.retBool(true); }
S4_HLL(SystemService, GetMouseCursorConfig) { c.ref(1) = g_cursorConfig[c.i(0)]; c.retBool(true); }
S4_HLL(SystemService, RunProgram) { c.retBool(false); }
S4_HLL(SystemService, IsOpenedMutex) { c.retBool(false); }
S4_HLL(SystemService, GetGameFolderPath) { c.refS(0) = sjis::fromUtf8(G().cfg.root); }
S4_HLL(SystemService, RestrainScreensaver) {}

// ---- OutputLog: debug logs ----
S4_HLL(OutputLog, Create) { c.retInt(1); }
S4_HLL(OutputLog, Output) { G().log("game log: " + sjis::toUtf8(c.s(1))); }
S4_HLL(OutputLog, Clear) {}
S4_HLL(OutputLog, Save) { c.retInt(1); }
S4_HLL(OutputLog, EnableAutoSave) { c.retBool(true); }
S4_HLL(OutputLog, DisableAutoSave) { c.retBool(true); }
S4_HLL(MusicSystem, SetErrorOutput) {}

// ---- InputString: text entry, used for naming. Text the host types (vns4_text) arrives between Begin and End. ----
std::string g_inputResult;
S4_HLL(InputString, SetFont) {}
S4_HLL(InputString, SetPos) {}
S4_HLL(InputString, Begin) { G().takeTyped(); G().textEntry = true; }
S4_HLL(InputString, End) { G().textEntry = false; }
S4_HLL(InputString, OpenIME) {}
S4_HLL(InputString, CloseIME) {}
S4_HLL(InputString, GetResultString) { c.retString(g_inputResult + G().takeTyped()); g_inputResult.clear(); }
S4_HLL(InputString, ClearResultString) { g_inputResult.clear(); G().takeTyped(); }
S4_HLL(InputString, SetResultString) { g_inputResult = c.s(0); }

// ---- PassRegister: sets of numbers and texts kept in a file per handle ----
struct PassSet { std::string file; std::set<int32_t> numbers; std::set<std::string> texts; };
std::map<int, PassSet> g_pass;
void passWrite(PassSet &p) {
    if (p.file.empty()) return;
    std::ofstream f(save::hostPath(*G().vm, p.file, true), std::ios::binary);
    for (int32_t n : p.numbers) f << "N" << n << "\n";
    for (auto &t : p.texts) f << "T" << t << "\n";
}
S4_HLL(PassRegister, SetFileName) {
    PassSet &p = g_pass[c.i(0)];
    p.file = c.s(1);
    std::ifstream f(save::hostPath(*G().vm, p.file, true), std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        if (line.size() > 1 && line[0] == 'N') p.numbers.insert(std::atoi(line.c_str() + 1));
        else if (line.size() > 1 && line[0] == 'T') p.texts.insert(line.substr(1));
    }
    c.retBool(true);
}
S4_HLL(PassRegister, RegistNumber) { auto &p = g_pass[c.i(0)]; p.numbers.insert(c.i(1)); passWrite(p); c.retBool(true); }
S4_HLL(PassRegister, RegistText) { auto &p = g_pass[c.i(0)]; p.texts.insert(c.s(1)); passWrite(p); c.retBool(true); }
S4_HLL(PassRegister, UnregistNumber) { auto &p = g_pass[c.i(0)]; p.numbers.erase(c.i(1)); passWrite(p); c.retBool(true); }
S4_HLL(PassRegister, UnregistText) { auto &p = g_pass[c.i(0)]; p.texts.erase(c.s(1)); passWrite(p); c.retBool(true); }
S4_HLL(PassRegister, UnregistAll) { auto &p = g_pass[c.i(0)]; p.numbers.clear(); p.texts.clear(); passWrite(p); c.retBool(true); }
S4_HLL(PassRegister, ExistNumber) { c.retBool(g_pass[c.i(0)].numbers.count(c.i(1)) > 0); }
S4_HLL(PassRegister, ExistText) { c.retBool(g_pass[c.i(0)].texts.count(c.s(1)) > 0); }

// ---- VSFile: a binary file the game writes and reads value by value, in the save folder ----
// The PC's layout, from a file Test Game 19 wrote under Wine: "VSF\0", four zero bytes, then each value as a type
// byte and its data. Ints are type 1 with 4 bytes, strings type 3 ending in a NUL. Bytes (0) and floats (2, 4
// bytes) follow the order of the Write calls; no test game writes them.
std::fstream g_vs;
enum VsType : char { kVsByte = 0, kVsInt = 1, kVsFloat = 2, kVsString = 3 };
constexpr char kVsHeader[8] = {'V', 'S', 'F', 0, 0, 0, 0, 0};
S4_HLL(VSFile, OpenForWrite) {
    g_vs = std::fstream(save::hostPath(*G().vm, c.s(0), false), std::ios::out | std::ios::binary | std::ios::trunc);
    if (g_vs.is_open()) g_vs.write(kVsHeader, sizeof kVsHeader);
    c.retBool(g_vs.is_open() && bool(g_vs));
}
S4_HLL(VSFile, OpenForRead) {
    g_vs = std::fstream(save::hostPath(*G().vm, c.s(0), false), std::ios::in | std::ios::binary);
    char h[sizeof kVsHeader] = {};
    bool ok = g_vs.is_open() && g_vs.read(h, sizeof h) && std::memcmp(h, kVsHeader, 4) == 0;
    if (!ok) g_vs.close();
    c.retBool(ok);
}
S4_HLL(VSFile, Close) { g_vs.close(); c.retBool(true); }
void vsWrite(VsType t, const void *data, size_t n) {
    g_vs.put(t);
    g_vs.write(static_cast<const char *>(data), std::streamsize(n));
}
// Reads a value's type byte; false when the next value is of another type.
bool vsExpect(VsType t) { return g_vs.get() == t && bool(g_vs); }
S4_HLL(VSFile, WriteByte) { char b = char(c.i(0)); vsWrite(kVsByte, &b, 1); c.retBool(bool(g_vs)); }
S4_HLL(VSFile, WriteInt) { int32_t v = c.i(0); vsWrite(kVsInt, &v, 4); c.retBool(bool(g_vs)); }
S4_HLL(VSFile, WriteFloat) { int32_t v = c.i(0); vsWrite(kVsFloat, &v, 4); c.retBool(bool(g_vs)); }
S4_HLL(VSFile, WriteString) { const std::string &s = c.s(0); vsWrite(kVsString, s.c_str(), s.size() + 1); c.retBool(bool(g_vs)); }
S4_HLL(VSFile, ReadByte) {
    char b = 0;
    bool ok = vsExpect(kVsByte) && g_vs.read(&b, 1);
    c.ref(0) = uint8_t(b);
    c.retBool(ok);
}
S4_HLL(VSFile, ReadInt) {
    int32_t v = 0;
    bool ok = vsExpect(kVsInt) && g_vs.read(reinterpret_cast<char *>(&v), 4);
    c.ref(0) = v;
    c.retBool(ok);
}
S4_HLL(VSFile, ReadFloat) {
    int32_t v = 0;
    bool ok = vsExpect(kVsFloat) && g_vs.read(reinterpret_cast<char *>(&v), 4);
    c.ref(0) = v;
    c.retBool(ok);
}
S4_HLL(VSFile, ReadString) {
    // The string must end in its NUL within 64 KB; otherwise the file is damaged and the variable keeps its value.
    constexpr size_t kMax = 65536;
    std::string s;
    bool ok = vsExpect(kVsString);
    for (int ch; ok && (ch = g_vs.get()) != 0;) {
        if (ch == std::char_traits<char>::eof() || s.size() == kMax) ok = false;
        else s += char(ch);
    }
    if (ok) c.refS(0) = s;
    c.retBool(ok);
}

// ---- FileOperation: paths relative to the game folder ----
std::string gamePath(const std::string &name) { return save::hostPath(*G().vm, name, false); }
S4_HLL(FileOperation, ExistFile) { std::error_code ec; c.retBool(fs::is_regular_file(gamePath(c.s(0)), ec)); }
S4_HLL(FileOperation, DeleteFile) { std::error_code ec; c.retBool(fs::remove(gamePath(c.s(0)), ec)); }
S4_HLL(FileOperation, CopyFile) {
    std::error_code ec;
    fs::copy_file(gamePath(c.s(1)), gamePath(c.s(0)), fs::copy_options::overwrite_existing, ec);
    c.retBool(!ec);
}
S4_HLL(FileOperation, ExistFolder) { std::error_code ec; c.retBool(fs::is_directory(gamePath(c.s(0)), ec)); }
S4_HLL(FileOperation, CreateFolder) { std::error_code ec; c.retBool(fs::create_directories(gamePath(c.s(0)), ec) || fs::is_directory(gamePath(c.s(0)), ec)); }
S4_HLL(FileOperation, GetFileSize) {
    std::error_code ec;
    auto n = fs::file_size(gamePath(c.s(0)), ec);
    c.ref(1) = ec ? 0 : int32_t(n);
    c.retBool(!ec);
}

// ---- File: Test Game 46's save folder housekeeping. Paths start with the save folder's name. ----
S4_HLL(File, MakeDirectory) {
    std::error_code ec;
    fs::create_directories(gamePath(c.s(0)), ec);
    c.retInt(fs::is_directory(gamePath(c.s(0)), ec) ? 1 : 0);
}
S4_HLL(File, Delete) { std::error_code ec; c.retInt(fs::remove(gamePath(c.s(0)), ec) ? 1 : 0); }
// Fills a date struct (year, month, day, hour, minute, second, day of the week from Sunday 0) with the file's local
// modification time; 0 when the file is missing.
S4_HLL(File, GetTime) {
    std::error_code ec;
    auto t = fs::last_write_time(gamePath(c.s(0)), ec);
    Page *p = c.page(1);
    if (ec || !p || p->v.size() < 7) { c.retInt(0); return; }
    auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        t - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t tt = std::chrono::system_clock::to_time_t(sys);
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &tt);
#else
    localtime_r(&tt, &lt);
#endif
    int32_t vals[7] = {lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec, lt.tm_wday};
    for (int k = 0; k < 7; k++) p->v[size_t(k)] = vals[k];
    c.retInt(1);
}

// ---- Confirm3: a disc and file check at startup; the files are the game's own ----
std::fstream g_confirm;
S4_HLL(Confirm3, Init) {}
S4_HLL(Confirm3, GetDrive) { c.retInt(0); }
S4_HLL(Confirm3, File_Open) {
    g_confirm = std::fstream(gamePath(c.s(0)), std::ios::in | std::ios::out | std::ios::binary);
    c.retBool(g_confirm.is_open());
}
S4_HLL(Confirm3, File_Create) {
    g_confirm = std::fstream(gamePath(c.s(0)), std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    c.retBool(g_confirm.is_open());
}
S4_HLL(Confirm3, File_SetPos) { g_confirm.clear(); g_confirm.seekg(c.i(0)); g_confirm.seekp(c.i(0)); c.retBool(bool(g_confirm)); }
S4_HLL(Confirm3, File_ReadByte) { char b = 0; g_confirm.read(&b, 1); c.ref(0) = uint8_t(b); c.retBool(bool(g_confirm)); }
S4_HLL(Confirm3, File_ReadInt) { int32_t v = 0; g_confirm.read(reinterpret_cast<char *>(&v), 4); c.ref(0) = v; c.retBool(bool(g_confirm)); }
S4_HLL(Confirm3, File_WriteInt) { int32_t v = c.i(0); g_confirm.write(reinterpret_cast<char *>(&v), 4); c.retBool(bool(g_confirm)); }
S4_HLL(Confirm3, File_Close) { g_confirm.close(); c.retBool(true); }

// ---- AliceLogo4: the AliceSoft logo animation at startup. VNDegen skips it. ----
S4_HLL(AliceLogo4, Init) { c.retInt(1); }
S4_HLL(AliceLogo4, SetWaveNum) {}
S4_HLL(AliceLogo4, Run) {}

// ---- IbisInputEngine: joysticks. VNDegen's controllers reach the game as keys and mouse. ----
S4_HLL(IbisInputEngine, Joystick_ClearCaptureFlag) {}
S4_HLL(IbisInputEngine, Joystick_GetNumofDevice) { c.retInt(0); }
S4_HLL(IbisInputEngine, Joystick_IsKeyDown) { c.retBool(false); }
S4_HLL(IbisInputEngine, Joystick_GetAxis) { c.retFloat(0); }

// ---- DrawPluginManager: the games load drawing plugins by name; VNDegen's drawing is built in. ----
S4_HLL(DrawPluginManager, Load) { c.retInt(1); }
S4_HLL(DrawPluginManager, IsLoad) { c.retInt(1); }

S4_HLL_RESET {
    g_rng.seed(1);
    g_skip = {};
    std::fill(std::begin(g_mixerVolume), std::end(g_mixerVolume), 80);
    std::fill(std::begin(g_mixerMute), std::end(g_mixerMute), false);
    g_windowSettings.clear();
    g_cursorConfig.clear();
    g_inputResult.clear();
    g_pass.clear();
    g_vs.close();
    g_confirm.close();
}

}  // namespace
}  // namespace s4

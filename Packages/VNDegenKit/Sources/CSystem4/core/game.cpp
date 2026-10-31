#include "game.h"
#include "save.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "font.h"

namespace fs = std::filesystem;

namespace s4 {

namespace {
Game *g_game = nullptr;

// Thrown into the VM thread to unwind it when the host stops the game.
struct StopVM {};

std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

std::string lower(std::string s) {
    for (char &c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

// The file in dir whose name matches name ignoring case, or "".
std::string findCaseless(const std::string &dir, const std::string &name) {
    std::error_code ec;
    for (auto &e : fs::directory_iterator(dir, ec))
        if (lower(e.path().filename().string()) == lower(name)) return e.path().string();
    return {};
}

Game &game() { return *g_game; }

Game::Game(GameConfig c) : cfg(std::move(c)) { g_game = this; }

Game::~Game() {
    stop();
    if (logFile_) std::fclose(logFile_);
    if (g_game == this) g_game = nullptr;
}

void Game::log(const std::string &s) {
    if (logFile_) {
        std::fprintf(logFile_, "%s\n", s.c_str());
        std::fflush(logFile_);
    } else {
        std::fprintf(stderr, "%s\n", s.c_str());
    }
}

void Game::problem(const std::string &s) {
    {
        std::lock_guard<std::mutex> lock(problemMu_);
        lastProblem_ = s;
    }
    log("problem: " + s);
}

// The ini names the code file and the screen size. Both AliceStart.ini and System40.ini use "Key = value".
std::map<std::string, std::string> readIni(const std::string &root) {
    std::map<std::string, std::string> ini;
    std::string iniPath = findCaseless(root, "AliceStart.ini");
    if (iniPath.empty()) iniPath = findCaseless(root, "System40.ini");
    if (iniPath.empty()) return ini;
    std::ifstream f(iniPath, std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
        ini[k] = v;
    }
    return ini;
}

std::string codeFile(const std::string &root, const std::map<std::string, std::string> &ini) {
    auto it = ini.find("CodeName");
    std::string path = it != ini.end() ? findCaseless(root, it->second) : std::string();
    if (!path.empty()) return path;
    std::error_code ec;
    for (auto &e : fs::directory_iterator(root, ec))
        if (lower(e.path().extension().string()) == ".ain") path = e.path().string();
    return path;
}

// Later System 4 games (Test Game 56) are built on libraries this runtime doesn't have yet. Starting one would only
// show a blank screen, so it is refused with a sentence that says why.
std::string unsupportedReason(const Ain &ain) {
    static const char *const kMissing[] = {"PartsEngine", "ChipmunkSpriteEngine", "MainEXFile", "KiwiSoundEngine",
                                           "SealEngine"};
    for (auto &lib : ain.libraries)
        for (const char *m : kMissing)
            if (lib.name == m)
                return "This System 4 game is built on PartsEngine and the libraries that came with it, which VNDegen "
                       "doesn't support yet.";
    return {};
}

bool Game::start(std::string &error) {
    if (!cfg.logPath.empty()) logFile_ = std::fopen(cfg.logPath.c_str(), "a");
    HllState::resetAll();
    if (!cfg.fontsDir.empty()) setFontPackDir(cfg.fontsDir);
    ini = readIni(cfg.root);
    if (ini.count("ViewWidth")) screenW = std::atoi(ini["ViewWidth"].c_str());
    if (ini.count("ViewHeight")) screenH = std::atoi(ini["ViewHeight"].c_str());
    std::string ainPath = codeFile(cfg.root, ini);
    if (ainPath.empty()) { error = "This folder has no System 4 code file (.ain)."; return false; }
    Ain ain;
    std::string err = ain.load(ainPath);
    if (!err.empty()) { error = "The game's code file can't be read: " + err; return false; }
    // S4_FORCE starts such a game anyway, for work on the libraries it needs.
    if (std::string why = unsupportedReason(ain); !why.empty() && !std::getenv("S4_FORCE")) { error = why; return false; }

    // Archives: <stem>GA.ald for pictures, WA for sound effects, BA for music.
    std::string stem = fs::path(ainPath).stem().string();
    auto openAld = [&](const char *kind) -> std::unique_ptr<AldArchive> {
        std::string first = findCaseless(cfg.root, stem + kind + "A.ald");
        if (first.empty()) return nullptr;
        return AldArchive::open(first);
    };
    // The game's own font library, such as Rance7Font.fnl.
    {
        std::error_code ec2;
        setGameFontLibrary("");
        for (auto &e : fs::directory_iterator(cfg.root, ec2))
            if (lower(e.path().extension().string()) == ".fnl") { setGameFontLibrary(e.path().string()); break; }
    }
    cgArchive = openAld("G");
    if (std::string afa = findCaseless(cfg.root, stem + "CG.afa"); !afa.empty()) cgAfa = AfaArchive::open(afa);
    if (std::string afa = findCaseless(cfg.root, stem + "BA.afa"); !afa.empty()) bgmAfa = AfaArchive::open(afa);
    if (std::string afa = findCaseless(cfg.root, stem + "WA.afa"); !afa.empty()) waveAfa = AfaArchive::open(afa);
    waveArchive = openAld("W");
    bgmArchive = openAld("B");

    vm = std::make_unique<VM>(std::move(ain));
    vm->saveDir = cfg.saveDir;
    vm->rootDir = cfg.root;
    if (ini.count("SaveFolder")) vm->saveFolderName = ini["SaveFolder"];
    // Test Game 19's 2009 Sys42VM writes plain GD containers and version 7 resume saves (checked with a PC save
    // made under Wine). Sys43VM (Test Game 46) uses the XOR and version 8, the default.
    if (lower(ini["MainVM"]) == "sys42vm") vm->saveFormat = {false, 7};
    vm->gameName = ini.count("GameName") ? ini["GameName"] : stem;
    vm->log = [this](const std::string &s) { log(s); };
    vm->timeMs = [this] { return now(); };
    vm->peek = [this] { yieldFrame(); };
    inputLog_ = std::getenv("S4_INPUTLOG") != nullptr;
    if (std::getenv("S4_TRACE")) vm->trace = [this](const std::string &t) { log("call " + t); };
    if (std::getenv("S4_HLLMISSING"))
        for (auto &n : vm->unboundHll()) log("unbound HLL " + n);
    if (std::getenv("S4_HLLTRACE")) vm->hllTrace = [this](const std::string &t) { log("hll " + t); };
    std::error_code ec;
    fs::create_directories(cfg.saveDir, ec);
    frame_.assign(size_t(screenW) * screenH * 4, 0);
    if (!noThread) thread_ = std::thread([this] { vmMain(); });
    return true;
}

void Game::vmMain() {
    {
        std::unique_lock<std::mutex> lock(mu_);
        cv_.wait(lock, [this] { return vmTurn_ || stopping_; });
        if (stopping_) return;
    }
    try {
        vm->start();
        try {
            vm->call(vm->ain.mainFunction, {});
        } catch (const ResumeJump &) {
            // A resume load replaced the VM's state; keep running it, through any later loads.
            for (;;) {
                try {
                    vm->run(0);
                    break;
                } catch (const ResumeJump &) {
                }
            }
        }
        log("main returned");
    } catch (const VMExit &e) {
        log("game ended: " + e.reason);
        if (e.code < 0) problem(e.reason);
    } catch (const StopVM &) {
    } catch (const VMCancelled &) {
    } catch (const std::exception &e) {
        problem(std::string("runtime error: ") + e.what());
    }
    std::lock_guard<std::mutex> lock(mu_);
    ended = true;
    vmTurn_ = false;
    cv_.notify_all();
}

void Game::yieldFrame() {
    // Debugging: a function the host asked for runs here, where the game shows a frame.
    std::string call;
    {
        std::lock_guard<std::mutex> lock(mu_);
        call.swap(debugCall_);
    }
    if (!call.empty()) {
        int fn = vm->ain.findFunction(call);
        log("debug call " + std::to_string(fn));
        // Arguments are zero (false, empty); string and struct arguments aren't supported.
        if (fn > 0) vm->call(fn, std::vector<int32_t>(size_t(vm->ain.functions[fn].argc), 0));
    }
    std::unique_lock<std::mutex> lock(mu_);
    vmTurn_ = false;
    yields++;
    cv_.notify_all();
    cv_.wait(lock, [this] { return vmTurn_ || stopping_; });
    if (stopping_) throw StopVM{};
}

bool Game::step(int ms) {
    if (ended) return false;
    if (paused_) return true;
    // A step longer than 100 ms (a hitch, or the app coming back from the background) advances the clock only
    // 100 ms, so the game doesn't jump ahead. That time is lost to anything paced by the real clock, such as a
    // movie's sound, so it is logged with the mixer's timing every 10 s of game time.
    ms = std::max(0, ms);
    if (ms > 100) {
        hitches_++;
        lostMs_ += ms - 100;
        ms = 100;
    }
    clockMs_ += uint32_t(ms);
    if (clockMs_ - timingLoggedAt_ >= 10000) {
        timingLoggedAt_ = clockMs_;
        Audio::Timing t = audio.takeTiming();
        if (hitches_ || t.slow)
            log("timing: " + std::to_string(hitches_) + " steps over 100 ms lost " + std::to_string(lostMs_) +
                " ms; audio renders over half their length " + std::to_string(t.slow) + ", longest " +
                std::to_string(t.longestUs) + " us");
        hitches_ = 0;
        lostMs_ = 0;
    }
    std::unique_lock<std::mutex> lock(mu_);
    vmTurn_ = true;
    cv_.notify_all();
    // The VM runs until it shows a frame or polls. A long load still returns to the host now and then.
    cv_.wait_for(lock, std::chrono::milliseconds(250), [this] { return !vmTurn_; });
    return !ended;
}

void Game::stop() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        stopping_ = true;
        cv_.notify_all();
    }
    if (vm) vm->cancelled = true;
    if (thread_.joinable()) thread_.join();
}

void Game::key(int vk, bool down) {
    if (vk < 0 || vk >= 256) return;
    if (inputLog_)
        log("input: key " + std::to_string(vk) + (down ? " down" : " up") + " at " + std::to_string(mouseX_.load()) + "," +
            std::to_string(mouseY_.load()) + ", clock " + std::to_string(now()));
    keys_[vk] = down;
}
void Game::mouseMove(int x, int y) { mouseX_ = x; mouseY_ = y; }
void Game::wheel(int delta) { wheel_ += delta; }

void Game::present() {
    if (dumpSprites_.exchange(false)) {
        for (auto &[n, sp] : sprites.all())
            if (sp.used)
                log("sprite " + std::to_string(n) + " cg " + std::to_string(sp.cgNumber) + " at " + std::to_string(sp.x) + "," + std::to_string(sp.y) + " size " +
                    std::to_string(sp.width()) + "x" + std::to_string(sp.height()) + " z " + std::to_string(sp.z) +
                    (sp.show ? " shown" : " hidden") + " blend " + std::to_string(sp.blendRate));
    }
    std::vector<uint8_t> f(size_t(screenW) * screenH * 4);
    sprites.compose(f, screenW, screenH, 0, 0);
    std::lock_guard<std::mutex> lock(frameMu_);
    frame_.swap(f);
}

void Game::showFrame(std::vector<uint8_t> f) {
    std::lock_guard<std::mutex> lock(frameMu_);
    frame_.swap(f);
}

bool Game::takeFrame(std::vector<uint8_t> &out, int &w, int &h) {
    std::lock_guard<std::mutex> lock(frameMu_);
    out = frame_;
    w = screenW;
    h = screenH;
    return true;
}

bool Game::cgExists(int number) const { return cgArchive && cgArchive->find(number); }

bool Game::cgLoad(int number, Image &out) { return cgLoad(number, out, 0); }

bool Game::cgLoad(int number, Image &out, int depth) {
    const ArchiveEntry *e = cgArchive ? cgArchive->find(number) : nullptr;
    if (!e) return false;
    std::vector<uint8_t> d;
    if (!cgArchive->read(*e, d) || !decodeImage(d.data(), d.size(), out)) return false;
    if (out.overBase < 0) return true;
    // A picture drawn over another: its magenta pixels show the base, which may itself be drawn over another.
    Image base;
    bool haveBase = depth < 16 && cgLoad(out.overBase, base, depth + 1) && base.w == out.w && base.h == out.h;
    if (!haveBase) log("CG " + std::to_string(number) + ": base " + std::to_string(out.overBase) + " unusable");
    for (size_t i = 0; i < out.rgba.size(); i += 4) {
        uint8_t *p = &out.rgba[i];
        if (p[0] != 255 || p[1] != 0 || p[2] != 255) continue;
        if (haveBase) std::memcpy(p, &base.rgba[i], 4);
        else p[3] = 0;
    }
    out.hasAlpha = out.hasAlpha || (haveBase && base.hasAlpha) || !haveBase;
    out.overBase = -1;
    return true;
}

std::shared_ptr<const Image> Game::cgCached(int number) {
    {
        std::lock_guard<std::mutex> lock(cgMu_);
        auto it = cgCache_.find(number);
        if (it != cgCache_.end())
            if (auto p = it->second.lock()) return p;
    }
    auto img = std::make_shared<Image>();
    if (!cgLoad(number, *img)) return nullptr;
    std::lock_guard<std::mutex> lock(cgMu_);
    cgCache_[number] = img;
    cgRecent_.push_back(img);
    if (cgRecent_.size() > 64) cgRecent_.erase(cgRecent_.begin());
    return img;
}

const ArchiveEntry *Game::cgFind(const std::string &name, AfaArchive **from) const {
    static const char *const kExts[] = {".qnt", ".png", ".ajp", ".webp", ""};
    auto look = [&](AfaArchive *a) -> const ArchiveEntry * {
        if (!a) return nullptr;
        for (const char *ext : kExts)
            if (const ArchiveEntry *e = a->find(name + ext)) {
                if (from) *from = a;
                return e;
            }
        return nullptr;
    };
    if (const ArchiveEntry *e = look(cgAfa.get())) return e;
    for (auto &a : extraAfa)
        if (const ArchiveEntry *e = look(a.get())) return e;
    return nullptr;
}

bool Game::cgLoadNamed(const std::string &name, Image &out) {
    AfaArchive *a = nullptr;
    const ArchiveEntry *e = cgFind(name, &a);
    std::vector<uint8_t> d;
    if (e) return a->read(*e, d) && decodeImage(d.data(), d.size(), out);
    // A file: "<save>" and a name in the save folder (a save's thumbnail), or a path in the game folder.
    static const std::string kSave = "<save>";
    bool inSave = name.compare(0, kSave.size(), kSave) == 0;
    std::string path = vm ? save::hostPath(*vm, inSave ? name.substr(kSave.size()) : name, inSave) : std::string();
    std::error_code ec;
    if (path.empty() || !fs::is_regular_file(path, ec)) return false;
    std::ifstream f(path, std::ios::binary);
    d.assign(std::istreambuf_iterator<char>(f), {});
    return !d.empty() && decodeImage(d.data(), d.size(), out);
}

std::shared_ptr<const Image> Game::cgCachedNamed(const std::string &name) {
    {
        std::lock_guard<std::mutex> lock(cgMu_);
        auto it = cgNamedCache_.find(name);
        if (it != cgNamedCache_.end())
            if (auto p = it->second.lock()) return p;
    }
    auto img = std::make_shared<Image>();
    if (!cgLoadNamed(name, *img)) return nullptr;
    std::lock_guard<std::mutex> lock(cgMu_);
    cgNamedCache_[name] = img;
    cgRecent_.push_back(img);
    if (cgRecent_.size() > 64) cgRecent_.erase(cgRecent_.begin());
    return img;
}

void Game::cgForgetNamed(const std::string &name) {
    std::lock_guard<std::mutex> lock(cgMu_);
    cgNamedCache_.erase(name);
}

}  // namespace s4

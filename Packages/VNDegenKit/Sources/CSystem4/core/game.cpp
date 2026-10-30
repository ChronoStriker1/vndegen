#include "game.h"

#include <chrono>
#include <cstdio>
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

// The file in dir whose name matches name ignoring case, or "".
std::string findCaseless(const std::string &dir, const std::string &name) {
    std::error_code ec;
    for (auto &e : fs::directory_iterator(dir, ec))
        if (lower(e.path().filename().string()) == lower(name)) return e.path().string();
    return {};
}
}  // namespace

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
    lastProblem = s;
    log("problem: " + s);
}

bool Game::start(std::string &error) {
    if (!cfg.logPath.empty()) logFile_ = std::fopen(cfg.logPath.c_str(), "a");
    if (!cfg.fontsDir.empty()) setFontPackDir(cfg.fontsDir);
    // The ini names the code file and the screen size. Both AliceStart.ini and System40.ini use "Key = value".
    std::string iniPath = findCaseless(cfg.root, "AliceStart.ini");
    if (iniPath.empty()) iniPath = findCaseless(cfg.root, "System40.ini");
    if (!iniPath.empty()) {
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
    }
    if (ini.count("ViewWidth")) screenW = std::atoi(ini["ViewWidth"].c_str());
    if (ini.count("ViewHeight")) screenH = std::atoi(ini["ViewHeight"].c_str());
    std::string ainPath = ini.count("CodeName") ? findCaseless(cfg.root, ini["CodeName"]) : std::string();
    if (ainPath.empty()) {
        std::error_code ec;
        for (auto &e : fs::directory_iterator(cfg.root, ec))
            if (lower(e.path().extension().string()) == ".ain") ainPath = e.path().string();
    }
    if (ainPath.empty()) { error = "This folder has no System 4 code file (.ain)."; return false; }
    Ain ain;
    std::string err = ain.load(ainPath);
    if (!err.empty()) { error = "The game's code file can't be read: " + err; return false; }

    // Archives: <stem>GA.ald for pictures, WA for sound effects, BA for music.
    std::string stem = fs::path(ainPath).stem().string();
    auto openAld = [&](const char *kind) -> std::unique_ptr<AldArchive> {
        std::string first = findCaseless(cfg.root, stem + kind + "A.ald");
        if (first.empty()) return nullptr;
        return AldArchive::open(first);
    };
    cgArchive = openAld("G");
    waveArchive = openAld("W");
    bgmArchive = openAld("B");

    vm = std::make_unique<VM>(std::move(ain));
    vm->saveDir = cfg.saveDir;
    vm->gameName = ini.count("GameName") ? ini["GameName"] : stem;
    vm->log = [this](const std::string &s) { log(s); };
    vm->timeMs = [this] { return now(); };
    vm->peek = [this] { yieldFrame(); };
    std::error_code ec;
    fs::create_directories(cfg.saveDir, ec);
    frame_.assign(size_t(screenW) * screenH * 4, 0);
    thread_ = std::thread([this] { vmMain(); });
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
        vm->call(vm->ain.mainFunction, {});
        log("main returned");
    } catch (const VMExit &e) {
        log("game ended: " + e.reason);
        if (e.code < 0) problem(e.reason);
    } catch (const StopVM &) {
    } catch (const std::exception &e) {
        problem(std::string("runtime error: ") + e.what());
    }
    std::lock_guard<std::mutex> lock(mu_);
    ended = true;
    vmTurn_ = false;
    cv_.notify_all();
}

void Game::yieldFrame() {
    std::unique_lock<std::mutex> lock(mu_);
    vmTurn_ = false;
    cv_.notify_all();
    cv_.wait(lock, [this] { return vmTurn_ || stopping_; });
    if (stopping_) throw StopVM{};
}

bool Game::step(int ms) {
    if (ended) return false;
    if (paused_) return true;
    clockMs_ += uint32_t(std::max(0, ms));
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
    if (thread_.joinable()) thread_.join();
}

void Game::key(int vk, bool down) {
    if (vk >= 0 && vk < 256) keys_[vk] = down;
}
void Game::mouseMove(int x, int y) { mouseX_ = x; mouseY_ = y; }
void Game::wheel(int delta) { wheel_ += delta; }

void Game::present() {
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

bool Game::cgLoad(int number, Image &out) {
    const ArchiveEntry *e = cgArchive ? cgArchive->find(number) : nullptr;
    if (!e) return false;
    std::vector<uint8_t> d;
    return cgArchive->read(*e, d) && decodeImage(d.data(), d.size(), out);
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

}  // namespace s4

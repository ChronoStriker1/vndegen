#include "files.h"

#include "wolf_casepath.h"

#include <cstring>
#include <fstream>

namespace fs = std::filesystem;

namespace wolf {

namespace {

std::string upperAscii(std::string s) {
    for (auto &c : s)
        if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
    return s;
}

std::vector<uint8_t> readAll(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

}  // namespace

std::string normalizePath(std::string path) {
    // A file name ends at the first line break or NUL. Games pass strings whose first line is the file and whose next
    // lines hold other values, such as a sound's volume and frequency, and the PC engine uses the first line.
    if (size_t end = path.find_first_of(std::string("\r\n\0", 3)); end != std::string::npos) path.resize(end);
    while (!path.empty() && path.back() == ' ') path.pop_back();
    for (auto &c : path)
        if (c == '\\') c = '/';
    while (path.starts_with("./")) path.erase(0, 2);
    if (upperAscii(path.substr(0, 5)) == "DATA/") path.erase(0, 5);
    return path;
}

GameFiles::GameFiles(fs::path root) : root_(std::move(root)) {
    data_ = findCaseless(root_, "Data").value_or(root_ / "Data");
    if (auto whole = findCaseless(root_, "Data.wolf")) whole_ = Archive::open(whole->string());
}

// The path as it is on disk, matched without regard to case as Windows matches it (casepath, shared by the engines).
std::optional<fs::path> GameFiles::findCaseless(const fs::path &base, const std::string &rel) const {
    std::string want = (base / rel).string();
    std::string out(want.size() + 1024, '\0');
    if (casepath_resolve(want.c_str(), out.data(), out.size()) != 1) return std::nullopt;
    out.resize(std::strlen(out.c_str()));
    return fs::path(out);
}

Archive *GameFiles::archiveFor(const std::string &top) const {
    std::string key = upperAscii(top);
    auto it = folders_.find(key);
    if (it != folders_.end()) return it->second.get();
    std::unique_ptr<Archive> a;
    if (auto p = findCaseless(data_.parent_path(), data_.filename().string() + "/" + top + ".wolf"))
        a = Archive::open(p->string());
    Archive *raw = a.get();
    folders_[key] = std::move(a);
    return raw;
}

std::optional<fs::path> GameFiles::looseFile(const std::string &path) const {
    auto p = findCaseless(data_, normalizePath(path));
    if (p && fs::is_regular_file(*p)) return p;
    return std::nullopt;
}

std::optional<std::vector<uint8_t>> GameFiles::read(const std::string &raw) const {
    std::string path = normalizePath(raw);
    if (auto p = looseFile(path)) return readAll(*p);
    size_t slash = path.find('/');
    if (slash != std::string::npos) {
        if (Archive *a = archiveFor(path.substr(0, slash))) {
            if (auto d = a->read(path.substr(slash + 1))) return d;
        }
    }
    if (whole_) {
        if (auto d = whole_->read(path)) return d;
    }
    return std::nullopt;
}

bool GameFiles::exists(const std::string &raw) const {
    std::string path = normalizePath(raw);
    if (looseFile(path)) return true;
    size_t slash = path.find('/');
    if (slash != std::string::npos) {
        if (Archive *a = archiveFor(path.substr(0, slash)); a && a->contains(path.substr(slash + 1))) return true;
    }
    return whole_ && whole_->contains(path);
}

}  // namespace wolf

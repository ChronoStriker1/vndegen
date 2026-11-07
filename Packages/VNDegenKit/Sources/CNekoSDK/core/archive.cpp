#include "archive.h"

#include <dirent.h>
#include <sys/stat.h>
#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace neko {
namespace {

uint32_t rd32(const uint8_t *p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

// Lowers ASCII only, walking CP932 pairs: a trail byte can look like an upper-case letter or a backslash.
std::string lower(std::string s) {
    bool lead = false;
    for (auto &c : s) {
        auto u = static_cast<unsigned char>(c);
        if (lead) { lead = false; continue; }
        if ((u >= 0x81 && u <= 0x9F) || (u >= 0xE0 && u <= 0xFC)) { lead = true; continue; }
        if (u >= 'A' && u <= 'Z') c = char(u + 32);
    }
    return s;
}

bool hasFolder(const std::string &name) {
    bool lead = false;
    for (char c : name) {
        auto u = static_cast<unsigned char>(c);
        if (lead) { lead = false; continue; }
        if ((u >= 0x81 && u <= 0x9F) || (u >= 0xE0 && u <= 0xFC)) { lead = true; continue; }
        if (c == '\\' || c == '/') return true;
    }
    return false;
}

// A patch's pictures, in its `cg`, `sc`, `obj` and `parts` folders, the four the PC engine maps to a patch's own. The PC
// reaches them only by the patch's name, such as `sys_5\\cg\\x.bmp`, because a patch's art can be a second set under the
// base names: The Alchemist's sys_5.pak holds Masami Chie's original CGs and sprites, shown only when the player picks
// them, and Test Game 86's sys_3.pak blank sprites. Test Games 90 and 92 name their patches' new pictures that way too.
bool isArt(const std::string &path) {
    auto slash = path.find_first_of("\\/");
    if (slash == std::string::npos) return false;
    std::string folder = lower(path.substr(0, slash));
    return folder == "cg" || folder == "sc" || folder == "obj" || folder == "parts";
}

// `sys_5` is patch 5; anything else isn't a patch.
int patchNumber(const std::string &name) {
    if (name.rfind("sys_", 0) != 0 || name.size() == 4) return -1;
    int n = 0;
    for (size_t i = 4; i < name.size(); i++) {
        if (name[i] < '0' || name[i] > '9') return -1;
        n = n * 10 + (name[i] - '0');
    }
    return n;
}

}  // namespace

std::string normalizedPath(std::string path) {
    bool lead = false;
    for (auto &c : path) {
        auto u = static_cast<unsigned char>(c);
        if (lead) { lead = false; continue; }
        if ((u >= 0x81 && u <= 0x9F) || (u >= 0xE0 && u <= 0xFC)) { lead = true; continue; }
        if (c == '/') c = '\\';
    }
    return lower(std::move(path));
}

std::vector<uint8_t> unpackEntry(const uint8_t *data, size_t size) {
    std::vector<uint8_t> out;
    if (size < 8) return out;
    uint32_t key = uint32_t(size / 8 + 0x22);
    uint8_t head[4];
    for (int i = 0; i < 4; i++) {
        head[i] = data[i] ^ uint8_t(key);
        key <<= 3;
    }
    uint32_t plain = rd32(data + size - 4);
    // A file bigger than this is damage: no archive entry in the library comes near it.
    if (plain > (1u << 30)) return out;
    out.resize(plain);
    z_stream z{};
    if (inflateInit(&z) != Z_OK) return {};
    z.next_out = out.data();
    z.avail_out = plain;
    z.next_in = head;
    z.avail_in = 4;
    int status = inflate(&z, Z_NO_FLUSH);
    if (status == Z_OK) {
        z.next_in = const_cast<Bytef *>(data + 4);
        z.avail_in = uInt(size - 8);
        status = inflate(&z, Z_FINISH);
    }
    bool ok = status == Z_STREAM_END && z.total_out == plain;
    inflateEnd(&z);
    if (!ok) out.clear();
    return out;
}

void Files::add(const std::string &path, Source source) {
    auto &slot = index_[normalizedPath(path)];
    if (source.priority >= slot.priority) slot = std::move(source);
}

// Every file of a patch answers to the patch's name, `sys_5\\cfg\\msg_button.ini`, as the PC asks for some. All but its
// art also take the place of the base file of the same path, as The Alchemist's msg_button.ini does to add its art button.
// A file outside any folder in a patch archive stands in for sys.pak's.
void Files::addPatched(const std::string &patch, const std::string &path, const Source &source) {
    add(patch + "\\" + path, source);
    if (!hasFolder(path)) add("sys\\" + path, source);
    else if (!isArt(path)) add(path, source);
}

void Files::indexArchive(const std::string &file, const std::string &folder, int priority, const std::string &patch) {
    std::ifstream f(file, std::ios::binary);
    char magic[14];
    if (!f.read(magic, 14) || std::memcmp(magic, "NEKOPACK4A", 10)) return;
    uint32_t indexSize;
    std::memcpy(&indexSize, magic + 10, 4);
    // The index can't be bigger than the file, which a damaged header might claim.
    f.seekg(0, std::ios::end);
    auto fileSize = uint64_t(f.tellg());
    if (uint64_t(indexSize) + 14 > fileSize) return;
    std::vector<uint8_t> index(indexSize);
    f.seekg(14);
    if (!f.read(reinterpret_cast<char *>(index.data()), indexSize)) return;
    int archive = int(archives_.size());
    archives_.push_back(file);
    size_t pos = 0;
    while (pos + 4 <= index.size()) {
        uint32_t length = rd32(&index[pos]);
        pos += 4;
        if (length == 0 || pos + length + 8 > index.size()) break;
        int32_t key = 0;
        for (uint32_t i = 0; i < length; i++) key += int8_t(index[pos + i]);
        std::string name(reinterpret_cast<const char *>(&index[pos]), strnlen(reinterpret_cast<const char *>(&index[pos]), length));
        pos += length;
        Source source;
        source.priority = priority;
        source.archive = archive;
        source.offset = rd32(&index[pos]) ^ uint32_t(key);
        source.size = rd32(&index[pos + 4]) ^ uint32_t(key);
        pos += 8;
        if (uint64_t(source.offset) + source.size > fileSize) continue;
        // An archive in a patch folder, such as Test Game 92's sys_5/obj.pak, holds that folder's files. As in a base
        // archive, an entry with folders of its own also answers to its own name, unless the full path is art.
        if (!patch.empty()) {
            std::string qualified = folder.empty() ? name : folder + "\\" + name;
            addPatched(patch, qualified, source);
            if (!folder.empty() && hasFolder(name) && !isArt(qualified)) addPatched(patch, name, source);
            continue;
        }
        // Entries with folders of their own: `rain\\x.ogg` in `se.pak` names `se\\rain\\x.ogg`, and a script could mean
        // either, so both find it.
        if (hasFolder(name)) add(name, source);
        add(folder + "\\" + name, std::move(source));
    }
}

void Files::indexFolder(const std::string &dir, const std::string &prefix, int priority, const std::string &patch) {
    DIR *d = opendir(dir.c_str());
    if (!d) return;
    while (dirent *e = readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string full = dir + "/" + name, rel = prefix.empty() ? name : prefix + "\\" + name;
        struct stat st;
        if (stat(full.c_str(), &st)) continue;
        std::string low = lower(name);
        if (S_ISDIR(st.st_mode)) {
            indexFolder(full, rel, priority, patch);
        } else if (prefix.empty() && low.size() > 4 && low.compare(low.size() - 4, 4, ".pak") == 0) {
            indexArchive(full, low.substr(0, low.size() - 4), priority - 1, patch);
        } else {
            Source source;
            source.priority = priority;
            source.loose = full;
            addPatched(patch, rel, source);
        }
    }
    closedir(d);
}

bool Files::open(const std::string &root) {
    archives_.clear();
    index_.clear();
    DIR *d = opendir(root.c_str());
    if (!d) return false;
    struct Found { std::string name, stem; bool folder; };
    std::vector<Found> found;
    while (dirent *e = readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        struct stat st;
        if (stat((root + "/" + name).c_str(), &st)) continue;
        std::string low = lower(name);
        if (S_ISDIR(st.st_mode)) found.push_back({name, low, true});
        else if (low.size() > 4 && low.compare(low.size() - 4, 4, ".pak") == 0) found.push_back({name, low.substr(0, low.size() - 4), false});
    }
    closedir(d);
    // A patch outranks the base archives and later patches outrank earlier ones. Within a patch, its loose files outrank
    // the archives in its folder, which outrank its sys_N.pak. Priorities are spaced by three so all fit.
    for (const auto &item : found) {
        int patch = patchNumber(item.stem);
        if (item.folder) {
            if (patch >= 0) indexFolder(root + "/" + item.name, "", patch * 3 + 3, item.stem);
            continue;
        }
        if (patch >= 0) indexArchive(root + "/" + item.name, "", patch * 3 + 1, item.stem);
        else indexArchive(root + "/" + item.name, item.stem, 0);
    }
    return !archives_.empty();
}

bool Files::exists(const std::string &path) const { return index_.count(normalizedPath(path)) != 0; }

int64_t Files::storedSize(const std::string &path) const {
    auto it = index_.find(normalizedPath(path));
    if (it == index_.end()) return -1;
    if (it->second.archive >= 0) return it->second.size;
    struct stat st;
    return stat(it->second.loose.c_str(), &st) ? -1 : int64_t(st.st_size);
}

Bytes Files::read(const std::string &path) const {
    auto it = index_.find(normalizedPath(path));
    if (it == index_.end()) return nullptr;
    const Source &s = it->second;
    if (s.archive < 0) {
        std::ifstream f(s.loose, std::ios::binary);
        if (!f) return nullptr;
        return std::make_shared<const std::vector<uint8_t>>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }
    std::ifstream f(archives_[size_t(s.archive)], std::ios::binary);
    std::vector<uint8_t> packed(s.size);
    f.seekg(s.offset);
    if (!f.read(reinterpret_cast<char *>(packed.data()), s.size)) return nullptr;
    auto plain = unpackEntry(packed.data(), packed.size());
    if (plain.empty()) return nullptr;
    return std::make_shared<const std::vector<uint8_t>>(std::move(plain));
}

std::vector<std::string> Files::list(const std::string &folder) const {
    std::string prefix = normalizedPath(folder) + "\\";
    std::vector<std::string> out;
    for (const auto &[path, source] : index_)
        if (path.rfind(prefix, 0) == 0) out.push_back(path);
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace neko

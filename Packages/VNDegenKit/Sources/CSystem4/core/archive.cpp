#include "archive.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <zlib.h>

namespace fs = std::filesystem;

namespace s4 {

bool Archive::read(const ArchiveEntry &e, std::vector<uint8_t> &out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (e.file < 0 || size_t(e.file) >= paths_.size()) return false;
    FILE *f = std::fopen(paths_[e.file].c_str(), "rb");
    if (!f) return false;
    out.resize(e.size);
    bool ok = fseeko(f, off_t(e.offset), SEEK_SET) == 0 && std::fread(out.data(), 1, e.size, f) == e.size;
    std::fclose(f);
    return ok;
}

// ALD layout, from the files: sector 0 starts a table of 3-byte sector numbers (sectors are 256 bytes). The first
// entry is the sector of the map, the rest point at the files. The map has 3 bytes per resource number, from 1:
// the disk (1 for A, 2 for B, ...) and the 16-bit index of its pointer in that disk's table, or zeros when the
// number is unused. Each file starts with a header: its length, the data length, a timestamp and the name.
std::unique_ptr<AldArchive> AldArchive::open(const std::string &first) {
    auto a = std::make_unique<AldArchive>();
    fs::path p(first);
    if (p.parent_path().empty()) p = fs::path(".") / p;
    std::string stem = p.stem().string();
    if (stem.empty()) return nullptr;
    char disk0 = stem.back();
    std::string base = stem.substr(0, stem.size() - 1);
    // Disks are the files that differ only in the last letter of the stem.
    std::map<int, int> diskFile;
    std::error_code ec;
    for (auto &de : fs::directory_iterator(p.parent_path(), ec)) {
        std::string s = de.path().stem().string(), ext = de.path().extension().string();
        if (ext.size() != 4 || strcasecmp(ext.c_str(), ".ald") != 0 || s.size() != stem.size()) continue;
        if (strncasecmp(s.c_str(), base.c_str(), base.size()) != 0) continue;
        char c = char(std::toupper(s.back()));
        if (c < 'A' || c > 'Z') continue;
        diskFile[c - 'A' + 1] = int(a->paths_.size());
        a->paths_.push_back(de.path().string());
    }
    (void)disk0;
    auto readAt = [](FILE *f, uint64_t off, size_t n, std::vector<uint8_t> &out) {
        out.resize(n);
        return fseeko(f, off_t(off), SEEK_SET) == 0 && std::fread(out.data(), 1, n, f) == n;
    };
    auto t3 = [](const std::vector<uint8_t> &d, size_t i) { return uint32_t(d[i] | d[i + 1] << 8 | d[i + 2] << 16); };
    // Each disk's pointer table.
    std::map<int, std::vector<uint32_t>> ptrs;
    std::vector<uint8_t> map;
    for (auto &[disk, fi] : diskFile) {
        FILE *f = std::fopen(a->paths_[fi].c_str(), "rb");
        if (!f) continue;
        std::vector<uint8_t> head;
        if (readAt(f, 0, 3, head)) {
            uint32_t tableSectors = t3(head, 0);
            std::vector<uint8_t> table;
            if (tableSectors > 0 && readAt(f, 0, size_t(tableSectors) * 256, table)) {
                std::vector<uint32_t> v;
                for (size_t i = 0; i + 2 < table.size(); i += 3) v.push_back(t3(table, i));
                if (map.empty() && v.size() > 1 && v[1] > v[0]) readAt(f, uint64_t(v[0]) * 256, size_t(v[1] - v[0]) * 256, map);
                ptrs[disk] = std::move(v);
            }
        }
        std::fclose(f);
    }
    for (size_t k = 0; k + 2 < map.size(); k += 3) {
        int disk = map[k];
        uint32_t idx = map[k + 1] | map[k + 2] << 8;
        if (disk == 0 || !diskFile.count(disk)) continue;
        auto &pt = ptrs[disk];
        if (idx == 0 || idx >= pt.size() || pt[idx] == 0) continue;
        ArchiveEntry e;
        e.file = diskFile[disk];
        uint64_t off = uint64_t(pt[idx]) * 256;
        FILE *f = std::fopen(a->paths_[e.file].c_str(), "rb");
        if (!f) continue;
        std::vector<uint8_t> h;
        if (readAt(f, off, 16, h)) {
            uint32_t hs, size;
            std::memcpy(&hs, h.data(), 4);
            std::memcpy(&size, h.data() + 4, 4);
            std::vector<uint8_t> nm;
            if (hs >= 16 && hs < 4096 && readAt(f, off + 16, hs - 16, nm)) {
                e.name.assign(reinterpret_cast<char *>(nm.data()), strnlen(reinterpret_cast<char *>(nm.data()), nm.size()));
                e.offset = off + hs;
                e.size = size;
                a->entries_[int(k / 3) + 1] = e;
            }
        }
        std::fclose(f);
    }
    return a;
}

// AFA version 2, from the files: "AFAH", u32 28, "AlicArch", u32 2, u32 1, u32 offset of the data, then "INFO", u32 its
// size, u32 the index's plain length, u32 the entry count and the zlib index. Each entry: u32 name length, u32 padded
// length, the name, two words, u32 offset from the data start, u32 size.
std::unique_ptr<AfaArchive> AfaArchive::open(const std::string &path) {
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) return nullptr;
    uint8_t h[44];
    bool ok = std::fread(h, 1, sizeof h, f) == sizeof h && !std::memcmp(h, "AFAH", 4) && !std::memcmp(h + 8, "AlicArch", 8) &&
              !std::memcmp(h + 28, "INFO", 4);
    auto rd = [&](size_t o) { uint32_t v; std::memcpy(&v, h + o, 4); return v; };
    if (!ok) { std::fclose(f); return nullptr; }
    uint32_t dataOff = rd(24), infoSize = rd(32), plainSize = rd(36), count = rd(40);
    std::vector<uint8_t> comp(infoSize > 16 ? infoSize - 16 : 0), plain(plainSize);
    ok = std::fread(comp.data(), 1, comp.size(), f) == comp.size();
    std::fclose(f);
    uLongf out = plainSize;
    if (!ok || uncompress(plain.data(), &out, comp.data(), uLong(comp.size())) != Z_OK) return nullptr;
    auto a = std::make_unique<AfaArchive>();
    a->paths_.push_back(path);
    size_t i = 0;
    auto u32 = [&]() -> uint32_t {
        if (i + 4 > plain.size()) { i = plain.size(); return 0; }
        uint32_t v; std::memcpy(&v, plain.data() + i, 4); i += 4; return v;
    };
    for (uint32_t k = 0; k < count && i < plain.size(); k++) {
        uint32_t len = u32(), padded = u32();
        if (i + padded > plain.size()) break;
        ArchiveEntry e;
        e.file = 0;
        e.name.assign(reinterpret_cast<char *>(plain.data() + i), std::min(len, padded));
        i += padded;
        u32();
        u32();
        e.offset = uint64_t(dataOff) + u32();
        e.size = u32();
        std::string key = e.name;
        for (char &c : key) c = c == '\\' ? '/' : char(std::tolower(static_cast<unsigned char>(c)));
        a->byName_[key] = a->list_.size();
        a->list_.push_back(std::move(e));
    }
    return a;
}

const ArchiveEntry *AfaArchive::find(const std::string &name) const {
    std::string key = name;
    for (char &c : key) c = c == '\\' ? '/' : char(std::tolower(static_cast<unsigned char>(c)));
    auto it = byName_.find(key);
    return it == byName_.end() ? nullptr : &list_[it->second];
}

const ArchiveEntry *AldArchive::find(int number) const {
    auto it = entries_.find(number);
    return it == entries_.end() ? nullptr : &it->second;
}

}  // namespace s4

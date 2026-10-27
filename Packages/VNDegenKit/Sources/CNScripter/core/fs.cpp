#include "fs.h"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstring>

namespace vnns {

FileSystem::~FileSystem() {
    for (FILE *f : archives_)
        if (f) fclose(f);
}

// On-disk names are UTF-8 on Apple systems and when an archive tool converted them; other names are left as the
// Shift-JIS bytes a Windows zip stored. Both compare in Shift-JIS, as the script writes them.
static std::string diskNameToSjis(const std::string &name) { return looksUtf8(name) ? utf8ToSjis(name) : name; }

void FileSystem::indexDir(const std::string &dir, const std::string &prefix) {
    DIR *d = opendir(dir.c_str());
    if (!d) return;
    while (dirent *e = readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string full = dir + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) != 0) continue;
        std::string rel = prefix.empty() ? name : prefix + "/" + name;
        if (S_ISDIR(st.st_mode)) {
            indexDir(full, rel);
        } else {
            std::string key = normalizePath(diskNameToSjis(rel));
            loose_.emplace(key, rel);
        }
    }
    closedir(d);
}

bool FileSystem::open(const std::string &root) {
    root_ = root;
    struct stat st;
    if (stat(root.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return false;
    indexDir(root, "");
    // .ns2 archives: the highest number is searched first, and numbers may have gaps.
    std::vector<std::pair<int, std::string>> ns2;
    for (auto &[key, rel] : loose_) {
        if (key.find('\\') != std::string::npos || key.size() < 5) continue;
        if (key.compare(key.size() - 4, 4, ".ns2") != 0) continue;
        std::string stem = key.substr(0, key.size() - 4);
        if (stem.empty() || !std::all_of(stem.begin(), stem.end(), ::isdigit)) continue;
        ns2.emplace_back(atoi(stem.c_str()), rel);
    }
    std::sort(ns2.begin(), ns2.end(), [](auto &a, auto &b) { return a.first > b.first; });
    for (auto &[n, rel] : ns2) addNs2(root_ + "/" + rel);
    return true;
}

void FileSystem::enableNsa() {
    if (nsaEnabled_) return;
    nsaEnabled_ = true;
    for (int i = 0; i < 10; i++) {
        std::string base = i == 0 ? "arc" : "arc" + std::to_string(i);
        if (auto it = loose_.find(base + ".nsa"); it != loose_.end()) addNsa(root_ + "/" + it->second, false);
        if (auto it = loose_.find(base + ".sar"); it != loose_.end()) addNsa(root_ + "/" + it->second, true);
    }
}

int FileSystem::openArchive(const std::string &path) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return -1;
    archives_.push_back(f);
    return int(archives_.size() - 1);
}

// .nsa and .sar: big-endian entry count and data offset, then NUL-terminated names, each with (in .nsa only) a
// compression byte, then the offset from the data start, the stored size and (in .nsa only) the original size.
bool FileSystem::addNsa(const std::string &path, bool sar) {
    int idx = openArchive(path);
    if (idx < 0) return false;
    FILE *f = archives_[idx];
    uint8_t head[6];
    if (fread(head, 1, 6, f) != 6) return false;
    uint32_t count = be16(head), base = be32(head + 2);
    if (base < 6) return false;
    Bytes index(base - 6);
    if (fread(index.data(), 1, index.size(), f) != index.size()) return false;
    size_t p = 0;
    for (uint32_t i = 0; i < count; i++) {
        size_t end = p;
        while (end < index.size() && index[end]) end++;
        size_t need = sar ? 8 : 13;
        if (end + 1 + need > index.size()) {
            logf("archive %s: index ends early", path.c_str());
            return false;
        }
        std::string name(reinterpret_cast<const char *>(&index[p]), end - p);
        p = end + 1;
        Entry e{idx, 0, 0, 0, 0};
        if (!sar) e.compression = index[p++];
        e.offset = base + uint64_t(be32(&index[p]));
        e.size = be32(&index[p + 4]);
        p += 8;
        e.originalSize = e.size;
        if (!sar) {
            e.originalSize = be32(&index[p]);
            p += 4;
        }
        entries_.emplace(normalizePath(name), e);
    }
    return true;
}

// .ns2: a little-endian header size, then entries of a quoted name and a little-endian size, then the data in the
// same order. The header ends with one byte ('e') after the last entry.
bool FileSystem::addNs2(const std::string &path) {
    int idx = openArchive(path);
    if (idx < 0) return false;
    FILE *f = archives_[idx];
    uint8_t head[4];
    if (fread(head, 1, 4, f) != 4) return false;
    uint32_t base = le32(head);
    if (base < 5) return false;
    Bytes index(base - 4);
    if (fread(index.data(), 1, index.size(), f) != index.size()) return false;
    uint64_t offset = base;
    size_t p = 0;
    while (p < index.size() && index[p] == '"') {
        size_t end = p + 1;
        while (end < index.size() && index[end] != '"') end++;
        if (end + 5 > index.size()) break;
        std::string name(reinterpret_cast<const char *>(&index[p + 1]), end - p - 1);
        uint32_t size = le32(&index[end + 1]);
        entries_.emplace(normalizePath(name), Entry{idx, offset, size, size, 0});
        offset += size;
        p = end + 5;
    }
    return true;
}

std::vector<std::string> FileSystem::topLevelFiles() const {
    std::vector<std::string> out;
    for (auto &[key, rel] : loose_)
        if (key.find('\\') == std::string::npos) out.push_back(rel);
    return out;
}

// Files the game wrote after the folder was indexed, such as save thumbnails, are found on disk.
bool FileSystem::onDisk(std::string_view name, std::string &path) const {
    path = hostPath(name);
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool FileSystem::exists(std::string_view name) const {
    std::string key = normalizePath(name), path;
    return loose_.count(key) || entries_.count(key) || onDisk(name, path);
}

bool FileSystem::read(std::string_view name, Bytes &out) {
    std::string key = normalizePath(name);
    if (auto it = loose_.find(key); it != loose_.end()) return readFile(root_ + "/" + it->second, out);
    auto it = entries_.find(key);
    if (it == entries_.end()) {
        std::string path;
        return onDisk(name, path) && readFile(path, out);
    }
    const Entry &e = it->second;
    Bytes raw(e.size);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        FILE *f = archives_[e.archive];
        if (fseeko(f, off_t(e.offset), SEEK_SET) != 0 || fread(raw.data(), 1, raw.size(), f) != raw.size()) return false;
    }
    switch (e.compression) {
    case 0:
        out = std::move(raw);
        return true;
    case 1:
        return decodeSpb(raw.data(), raw.size(), out);
    default:
        logf("%s: archive compression %d is not supported", sjisToUtf8(name).c_str(), e.compression);
        return false;
    }
}

std::string FileSystem::hostPath(std::string_view name) const {
    std::string key = normalizePath(name);
    if (auto it = loose_.find(key); it != loose_.end()) return root_ + "/" + it->second;
    // Match each existing folder of the path without regard to case; the remainder keeps the script's spelling.
    std::string path = root_;
    std::string utf8 = sjisToUtf8(name);
    std::replace(utf8.begin(), utf8.end(), '\\', '/');
    size_t start = 0;
    while (start < utf8.size()) {
        size_t slash = utf8.find('/', start);
        std::string part = utf8.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        start = slash == std::string::npos ? utf8.size() : slash + 1;
        if (part.empty() || part == ".") continue;
        std::string match = part;
        if (DIR *d = opendir(path.c_str())) {
            std::string want = sjisLower(utf8ToSjis(part));
            while (dirent *e = readdir(d))
                if (sjisLower(diskNameToSjis(e->d_name)) == want) {
                    match = e->d_name;
                    break;
                }
            closedir(d);
        }
        path += "/" + match;
    }
    return path;
}

// SPB: big-endian width and height, then an MSB-first bit stream with the blue, green and red planes in turn. Each
// plane starts with the first pixel's 8-bit value, then codes groups of four pixels with a 3-bit code: 0 repeats the
// previous value, 1..5 give each pixel a delta of code+2 bits, 6 gives raw 8-bit values, and 7 is followed by one
// bit choosing 1- or 2-bit deltas. A delta with its low bit set adds (v >> 1) + 1, otherwise it subtracts v >> 1.
// Pixels run in serpentine order, top row first: even rows left to right, odd rows right to left.
bool decodeSpb(const uint8_t *data, size_t size, Bytes &bmp) {
    if (size < 4) return false;
    uint32_t w = be16(data), h = be16(data + 2);
    if (!w || !h) return false;
    size_t bitPos = 32, bitEnd = size * 8;
    auto bits = [&](int n) {
        uint32_t v = 0;
        for (int i = 0; i < n; i++) {
            uint32_t bit = bitPos < bitEnd ? (data[bitPos >> 3] >> (7 - (bitPos & 7))) & 1 : 0;
            v = v << 1 | bit;
            bitPos++;
        }
        return v;
    };
    uint32_t stride = (w * 3 + 3) & ~3u;
    uint32_t pixels = w * h;
    bmp.assign(54 + size_t(stride) * h, 0);
    uint8_t *hdr = bmp.data();
    auto put32 = [](uint8_t *p, uint32_t v) {
        p[0] = uint8_t(v);
        p[1] = uint8_t(v >> 8);
        p[2] = uint8_t(v >> 16);
        p[3] = uint8_t(v >> 24);
    };
    hdr[0] = 'B';
    hdr[1] = 'M';
    put32(hdr + 2, uint32_t(bmp.size()));
    put32(hdr + 10, 54);
    put32(hdr + 14, 40);
    put32(hdr + 18, w);
    put32(hdr + 22, h);
    hdr[26] = 1;
    hdr[28] = 24;
    put32(hdr + 34, uint32_t(stride * h));
    std::vector<uint8_t> plane(pixels + 4);
    for (int channel = 0; channel < 3; channel++) {
        int c = int(bits(8));
        plane[0] = uint8_t(c);
        for (uint32_t n = 1; n < pixels; n += 4) {
            uint32_t code = bits(3);
            int m = code == 7 ? int(bits(1)) + 1 : int(code) + 2;  // the delta width, once per group
            for (int k = 0; k < 4; k++) {
                if (code == 0) {
                } else if (code == 6) {
                    c = int(bits(8));
                } else {
                    uint32_t v = bits(m);
                    c = (v & 1) ? c + int(v >> 1) + 1 : c - int(v >> 1);
                }
                plane[n + k] = uint8_t(c);
            }
        }
        uint32_t i = 0;
        for (uint32_t y = 0; y < h; y++) {
            uint8_t *row = bmp.data() + 54 + size_t(h - 1 - y) * stride;  // BMP rows run bottom-up
            for (uint32_t k = 0; k < w; k++, i++) {
                uint32_t x = (y & 1) ? w - 1 - k : k;
                row[x * 3 + channel] = plane[i];
            }
        }
    }
    return true;
}

}  // namespace vnns

// Reader for DxLib archives (.wolf files), the format Wolf RPG Editor packs game data into.
#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace wolf {

class Archive {
public:
    // Opens an archive, trying each key Wolf RPG Editor 2 and 3 are known to use. Returns null when the file is not an
    // archive this reader understands, with the reason in `error`.
    static std::unique_ptr<Archive> open(const std::string &path, std::string *error = nullptr);
    ~Archive();

    // Paths are relative to the archive root, separated by '/', and matched without regard to case.
    bool contains(const std::string &path) const;
    std::optional<std::vector<uint8_t>> read(const std::string &path) const;
    std::vector<std::string> paths() const;
    int version() const { return version_; }
    // Wolf RPG Editor 3's own crypt version, kept in a version 8 header. 0 for other archives.
    uint16_t cryptVersion() const { return cryptVersion_; }

private:
    struct Entry {
        std::string name;  // original case
        uint64_t offset = 0, size = 0, packedSize = UINT64_MAX, huffmanSize = UINT64_MAX;
        uint64_t stored = 0;       // bytes the file takes in the archive
        std::vector<uint8_t> key;  // version 8: the file's own key, empty when the archive is not encrypted
    };
    struct Header8;

    Archive() = default;
    bool load(const std::vector<uint8_t> &key, std::string *error);
    bool load8(const Header8 &h, std::string_view keyString, std::string *error);
    std::optional<std::vector<uint8_t>> read8(const Entry &e, std::vector<uint8_t> stored) const;
    void walk(const std::vector<uint8_t> &head, uint64_t fileTable, uint64_t dirTable, uint64_t dir,
              const std::string &prefix, const std::string &dirKey);

    FILE *file_ = nullptr;
    uint64_t fileSize_ = 0;
    int version_ = 0;
    uint16_t cryptVersion_ = 0;
    std::vector<uint8_t> key_;  // empty when the archive is not encrypted
    std::string keyString_;     // version 8: the string each file's key is made from
    unsigned huffmanKiB_ = 0;   // version 8: how much of each file the Huffman stage covers
    bool utf8Names_ = false;
    uint64_t dataStart_ = 0;
    std::unordered_map<std::string, Entry> entries_;  // keyed by upper-case path
};

// DxLib's LZ decompression. The source starts with the decoded size, the packed size, and the escape byte. Returns
// nothing when the decoded size isn't `expected`, before allocating, so a damaged size can't ask for a large buffer.
std::vector<uint8_t> dxlibDecode(const uint8_t *src, size_t srcSize, uint64_t expected);

}  // namespace wolf

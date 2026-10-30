// Reader for DxLib archives (.wolf files), the format Wolf RPG Editor packs game data into.
#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace wolf {

class Archive {
public:
    // Opens an archive, trying each key Wolf RPG Editor 2 is known to use. Returns null when the file is not an
    // archive this reader understands, with the reason in `error`.
    static std::unique_ptr<Archive> open(const std::string &path, std::string *error = nullptr);
    ~Archive();

    // Paths are relative to the archive root, separated by '/', and matched without regard to case.
    bool contains(const std::string &path) const;
    std::optional<std::vector<uint8_t>> read(const std::string &path) const;
    std::vector<std::string> paths() const;
    int version() const { return version_; }

private:
    struct Entry {
        std::string name;  // original case
        uint64_t offset = 0, size = 0, packedSize = UINT64_MAX;
    };

    Archive() = default;
    bool load(const std::vector<uint8_t> &key, std::string *error);
    void xorAt(uint8_t *data, size_t size, uint64_t position) const;
    void walk(const std::vector<uint8_t> &head, uint64_t fileTable, uint64_t dirTable, uint64_t dir,
              const std::string &prefix);

    FILE *file_ = nullptr;
    uint64_t fileSize_ = 0;
    int version_ = 0;
    std::vector<uint8_t> key_;
    uint64_t dataStart_ = 0;
    std::unordered_map<std::string, Entry> entries_;  // keyed by upper-case path
};

// DxLib's LZ decompression. The source starts with the decoded size, the packed size, and the escape byte.
std::vector<uint8_t> dxlibDecode(const uint8_t *src, size_t srcSize);

}  // namespace wolf

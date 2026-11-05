// A game's files as the scripts name them: `bg\B300_000.bmp` means entry `B300_000.bmp` of `bg.pak`, unless a patch
// holds the same path. Lookups ignore case, because scripts and archives disagree on it, as Windows lets them.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace neko {

using Bytes = std::shared_ptr<const std::vector<uint8_t>>;

class Files {
public:
    // Indexes every archive and patch folder in the game's folder. False when it holds no NEKOPACK4 archive.
    bool open(const std::string &root);
    // The file's bytes, or null when no archive or patch has it.
    Bytes read(const std::string &path) const;
    bool exists(const std::string &path) const;
    // Paths under a folder, such as `flow`, for tools. Lower case.
    std::vector<std::string> list(const std::string &folder) const;

private:
    struct Source {
        int priority = -1;
        int archive = -1;  // index into archives_; -1 for a loose file
        std::string loose;
        uint32_t offset = 0, size = 0;
    };
    void add(const std::string &path, Source source);
    void indexArchive(const std::string &file, const std::string &folder, int priority);
    void indexFolder(const std::string &dir, const std::string &prefix, int priority);
    std::vector<std::string> archives_;
    std::unordered_map<std::string, Source> index_;
};

// Lower case with backslashes, the form the index uses.
std::string normalizedPath(std::string path);

// The container archive entries and save files share: the first 4 bytes XORed with a rolling key from the stored size,
// a zlib stream, and the unpacked size in the last 4 bytes. Empty when the data doesn't unpack.
std::vector<uint8_t> unpackEntry(const uint8_t *data, size_t size);

}  // namespace neko

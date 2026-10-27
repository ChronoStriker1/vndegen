// A game's files as NScripter sees them: loose files in the game folder, matched without regard to case, then .ns2
// archives (highest number first), then .nsa archives (arc.nsa, arc1.nsa .. arc9.nsa) once the script turns them on.
// That order was measured on the original engine; docs/nscripter-cleanroom.md has the details.
#pragma once

#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.h"

namespace vnns {

class FileSystem {
public:
    ~FileSystem();
    // Indexes the folder's loose files and its .ns2 archives. Returns false when the folder can't be read.
    bool open(const std::string &root);
    // The `nsa` command: adds arc.nsa .. arc9.nsa (and .sar archives) below the .ns2 archives.
    void enableNsa();

    // Reads a file by the name a script uses (Shift-JIS, either slash, any case). SPB-packed bitmaps come back as
    // ordinary BMP files. Thread-safe.
    bool read(std::string_view name, Bytes &out);
    bool exists(std::string_view name) const;

    // The real path of a loose file, or the path a new file of that name should get, for files the engine writes
    // (saves, logs). Existing folders are matched without regard to case.
    std::string hostPath(std::string_view name) const;
    const std::string &root() const { return root_; }
    // Loose files in the game's top folder, as paths relative to the root.
    std::vector<std::string> topLevelFiles() const;

private:
    struct Entry {
        int archive;
        uint64_t offset;
        uint32_t size;
        uint32_t originalSize;
        uint8_t compression;  // 0 stored, 1 SPB
    };
    void indexDir(const std::string &dir, const std::string &prefix);
    bool addNsa(const std::string &path, bool sar);
    bool addNs2(const std::string &path);
    int openArchive(const std::string &path);

    std::string root_;
    bool nsaEnabled_ = false;
    std::unordered_map<std::string, std::string> loose_;  // normalized name -> path relative to root_
    std::unordered_map<std::string, Entry> entries_;      // first archive to claim a name wins
    std::vector<FILE *> archives_;
    std::mutex mutex_;  // archive reads share FILE positions
};

// Decodes NScripter's SPB bitmap compression into a 24-bit BMP file.
bool decodeSpb(const uint8_t *data, size_t size, Bytes &bmp);

}  // namespace vnns

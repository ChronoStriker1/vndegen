// The game's resource archives. ALD sets (Name<Kind><Disk>.ald, one file per disk letter, the map in the first)
// hold numbered pictures, sounds and music; AFA archives hold named files. A lookup returns the file's bytes.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace s4 {

struct ArchiveEntry {
    int file = -1;        // index into the archive's open files
    uint64_t offset = 0;  // of the data
    uint32_t size = 0;
    std::string name;     // as stored, CP932
};

class Archive {
public:
    virtual ~Archive() = default;
    bool read(const ArchiveEntry &e, std::vector<uint8_t> &out);
    const std::vector<std::string> &paths() const { return paths_; }

protected:
    std::vector<std::string> paths_;
    std::mutex mutex_;
};

class AldArchive : public Archive {
public:
    // Opens every disk of the set whose first file is `first` (for example Rance02GA.ald).
    static std::unique_ptr<AldArchive> open(const std::string &first);
    const ArchiveEntry *find(int number) const;
    const std::map<int, ArchiveEntry> &entries() const { return entries_; }

private:
    std::map<int, ArchiveEntry> entries_;
};

// AFA ("AlicArch"): a zlib-compressed index of named files, version 2. Lookups ignore case and backslashes.
class AfaArchive : public Archive {
public:
    static std::unique_ptr<AfaArchive> open(const std::string &path);
    const ArchiveEntry *find(const std::string &name) const;  // CP932 name, any case
    const std::vector<ArchiveEntry> &entries() const { return list_; }

private:
    std::vector<ArchiveEntry> list_;
    std::map<std::string, size_t> byName_;  // lower-case name
};

}  // namespace s4

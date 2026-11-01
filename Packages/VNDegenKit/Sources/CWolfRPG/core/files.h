// A Wolf RPG Editor game's files: loose files under Data, and the .wolf archives that replace them.
#pragma once

#include "dxa.h"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wolf {

class GameFiles {
public:
    // `root` is the folder with Game.exe. Data is its Data folder, which may be packed as Data.wolf, or packed one
    // top-level folder at a time as Data/<Folder>.wolf.
    explicit GameFiles(std::filesystem::path root);

    // `path` is relative to Data, such as "BasicData/Game.dat" or "Picture\\Title.png". Matching ignores case, as
    // Windows does.
    std::optional<std::vector<uint8_t>> read(const std::string &path) const;
    bool exists(const std::string &path) const;
    const std::filesystem::path &root() const { return root_; }
    // The game's Data folder on disk, whatever the case of its name.
    const std::filesystem::path &dataFolder() const { return data_; }
    // A loose file on disk, if the path names one.
    std::optional<std::filesystem::path> looseFile(const std::string &path) const;
    // The names of the files in a folder on disk, relative to the game folder ("" is the game folder itself), sorted.
    // Nullopt when there is no such folder, as for one packed in an archive.
    std::optional<std::vector<std::string>> listFolder(std::string path) const;

private:
    std::optional<std::filesystem::path> findCaseless(const std::filesystem::path &base, const std::string &rel) const;
    Archive *archiveFor(const std::string &top) const;

    std::filesystem::path root_, data_;
    std::unique_ptr<Archive> whole_;
    mutable std::map<std::string, std::unique_ptr<Archive>> folders_;  // upper-case folder name to archive or null
};

// Normalises a game path: backslashes become slashes and a leading "Data/" is dropped.
std::string normalizePath(std::string path);

}  // namespace wolf

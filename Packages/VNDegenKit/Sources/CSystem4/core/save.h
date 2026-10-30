// Save files: system.GlobalSave and GroupSave write the globals (or one group of them) in the PC engine's format,
// so saves move between VNDegen and the PC game both ways. File names are relative to the game's save folder.
#pragma once
#include <cstdint>
#include <string>

namespace s4 {
class VM;
namespace save {

int32_t globalSave(VM &vm, const std::string &key, const std::string &file, const std::string &group, int32_t *count = nullptr);
int32_t globalLoad(VM &vm, const std::string &key, const std::string &file, const std::string &group, int32_t *count = nullptr);
int32_t exists(VM &vm, const std::string &name, bool inSaveFolder);
int32_t remove(VM &vm, const std::string &name);
int32_t copy(VM &vm, const std::string &dst, const std::string &src);
// The host path of a game path: CP932 to UTF-8, backslashes to slashes, matched case-insensitively.
std::string hostPath(VM &vm, const std::string &name, bool inSaveFolder);

}  // namespace save
}  // namespace s4

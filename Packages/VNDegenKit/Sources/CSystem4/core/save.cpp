#include "save.h"

#include <filesystem>
#include <system_error>

#include "sjis.h"
#include "vm.h"

namespace fs = std::filesystem;

namespace s4::save {

std::string hostPath(VM &vm, const std::string &name, bool inSaveFolder) {
    std::string p = sjis::toUtf8(name);
    for (char &c : p) if (c == '\\') c = '/';
    return inSaveFolder ? vm.saveDir + "/" + p : p;
}

int32_t globalSave(VM &vm, const std::string &, const std::string &file, const std::string &, int32_t *count) {
    if (vm.log) vm.log("save not written yet: " + sjis::toUtf8(file));
    if (count) *count = 0;
    return 0;
}

int32_t globalLoad(VM &, const std::string &, const std::string &, const std::string &, int32_t *count) {
    if (count) *count = 0;
    return 0;
}

int32_t exists(VM &vm, const std::string &name, bool inSaveFolder) {
    std::error_code ec;
    return fs::exists(hostPath(vm, name, inSaveFolder), ec) ? 1 : 0;
}

int32_t remove(VM &vm, const std::string &name) {
    std::error_code ec;
    return fs::remove(hostPath(vm, name, true), ec) ? 1 : 0;
}

int32_t copy(VM &vm, const std::string &dst, const std::string &src) {
    std::error_code ec;
    fs::copy_file(hostPath(vm, src, true), hostPath(vm, dst, true), fs::copy_options::overwrite_existing, ec);
    return ec ? 0 : 1;
}

}  // namespace s4::save

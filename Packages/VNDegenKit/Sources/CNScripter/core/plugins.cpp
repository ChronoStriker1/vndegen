// The Windows plugins test games call, emulated: NSFont.dll's gaiji (an image drawn in place of a character) and
// layer plugins (setlayer / layermessage). Other DLL calls are logged and ignored.
#include "engine.h"

namespace vnns {

struct Engine::MSpDrawItem {
    int dummy = 0;
};

void Engine::execDll(const std::string &call) {
    // "plugin.dll/function,arg,arg"
    size_t slash = call.find('/');
    std::string dll = sjisLower(call.substr(0, slash));
    std::string rest = slash == std::string::npos ? std::string() : call.substr(slash + 1);
    std::vector<std::string> args;
    size_t start = 0;
    for (size_t i = 0; i <= rest.size();) {
        if (i == rest.size() || rest[i] == ',') {
            args.push_back(rest.substr(start, i - start));
            start = ++i;
            continue;
        }
        i += sjisCharLen(rest, i);
    }
    if (dll == "nsfont.dll" && !args.empty() && sjisLower(args[0]) == "gaiji" && args.size() >= 3) {
        gaiji_.push_back({args[1], args[2]});
        return;
    }
    logf("exec_dll %s is not emulated", sjisToUtf8(call).c_str());
}

void Engine::layerMessage(int layer, const std::string &msg) {
    auto it = layers_.find(layer);
    if (it == layers_.end()) return;
    logf("layermessage %d (%s): %s is not emulated yet", layer, it->second.c_str(), sjisToUtf8(msg).c_str());
}

void Engine::drawLayers(Image &) {}

namespace {
using E = Engine;

void cmdExecDll(E &e, const std::string &) { e.execDll(e.readStr()); }

void cmdSetLayer(E &e, const std::string &) {
    int n = e.readInt();
    e.readComma();
    e.readInt();
    e.readComma();
    e.layers_[n] = sjisLower(e.readStr());
}

void cmdLayerMessage(E &e, const std::string &) {
    int n = e.readInt();
    e.readComma();
    e.layerMessage(n, e.readStr());
}

}  // namespace

void registerPluginCommands(std::unordered_map<std::string, CommandFn> &t) {
    t["exec_dll"] = cmdExecDll;
    t["setlayer"] = cmdSetLayer;
    t["layermessage"] = cmdLayerMessage;
}

}  // namespace vnns

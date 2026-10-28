// The Windows plugins test games call, emulated: NSFont.dll's gaiji (an image drawn in place of a character) and
// layer plugins (setlayer / layermessage). Other DLL calls are logged and ignored.
#include "engine.h"

#include <algorithm>
#include <cmath>

namespace vnns {

// MSpDraw.dll, a third-party layer plugin with no documentation available. One test game sends it
//   draw/<id>,<image tag>,<x>,<y>,<alpha>,<rotation>,<x scale>,<y scale>,<a>,<b>,<c>,<d>
// to pan a tall picture past the screen, and clear/all. Read as: the picture centred on (x, y), moving d pixels down
// on each of b timer ticks. The two calls it makes (y -200 moving +2, y 790 moving -2, b = 200) then keep the
// 998 x 1778 picture covering the 800 x 600 screen throughout, which supports that reading.
struct Engine::MSpDrawItem {
    int id = 0;
    LoadedImage img;
    double cx = 0, cy = 0, vx = 0, vy = 0;
    int alpha = 255, rot = 0, ticks = 0;
    double sx = 1, sy = 1;
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
    if (it->second.plugin != "mspdraw.dll") {
        logf("layermessage %d (%s) is not emulated", layer, it->second.plugin.c_str());
        return;
    }
    size_t slash = msg.find('/');
    std::string verb = sjisLower(msg.substr(0, slash));
    std::string rest = slash == std::string::npos ? std::string() : msg.substr(slash + 1);
    if (verb == "clear") {
        if (sjisLower(rest) == "all") {
            mspdraw_.clear();
        } else {
            int id = atoi(rest.c_str());
            mspdraw_.erase(std::remove_if(mspdraw_.begin(), mspdraw_.end(), [id](auto &i) { return i->id == id; }),
                           mspdraw_.end());
        }
        return;
    }
    if (verb != "draw") {
        logf("MSpDraw message %s is not emulated", sjisToUtf8(msg).c_str());
        return;
    }
    // id, then the image tag, which may itself hold commas before its ';', then numbers.
    std::vector<std::string> f;
    size_t start = 0;
    for (size_t i = 0; i <= rest.size();) {
        if (i == rest.size() || rest[i] == ',') {
            f.push_back(rest.substr(start, i - start));
            start = ++i;
            continue;
        }
        i += sjisCharLen(rest, i);
    }
    if (f.size() < 4) return;
    auto item = std::make_shared<MSpDrawItem>();
    item->id = atoi(f[0].c_str());
    // Rejoin the tag's own commas (":c/2,0,3;file") up to the field holding its ';'.
    size_t k = 1;
    std::string tag = f[k];
    if (!tag.empty() && tag[0] == ':' && tag.find(';') == std::string::npos)
        while (++k < f.size()) {
            tag += "," + f[k];
            if (f[k].find(';') != std::string::npos) break;
        }
    std::vector<double> n;
    for (size_t j = k + 1; j < f.size(); j++) n.push_back(atof(f[j].c_str()));
    n.resize(10, 0);
    if (!loadImage(tag, item->img)) return;
    item->cx = n[0];
    item->cy = n[1];
    item->alpha = int(n[2]);
    item->rot = int(n[3]);
    item->sx = n[4] ? n[4] : 1;
    item->sy = n[5] ? n[5] : 1;
    item->vx = n[6];
    item->ticks = int(n[7]);
    item->vy = n[9];
    mspdraw_.erase(std::remove_if(mspdraw_.begin(), mspdraw_.end(), [&](auto &i) { return i->id == item->id; }),
                   mspdraw_.end());
    mspdraw_.push_back(item);
}

// Runs each layer plugin's timer at the interval setlayer gave it.
void Engine::tickLayers() {
    for (auto &[n, layer] : layers_) {
        if (layer.interval <= 0 || now_ < layer.nextTick) continue;
        layer.nextTick = now_ + layer.interval;
        for (auto &item : mspdraw_)
            if (item->ticks > 0) {
                item->cx += item->vx;
                item->cy += item->vy;
                item->ticks--;
                sceneDirty_ = true;
            }
    }
}

void Engine::drawLayer(int layer, Image &out) {
    auto it = layers_.find(layer);
    if (it == layers_.end() || it->second.plugin != "mspdraw.dll") return;
    for (auto &item : mspdraw_) {
        const LoadedImage &img = item->img;
        if (!img.image) continue;
        double rad = item->rot * M_PI / 180.0;
        double m[4] = {std::cos(rad) * item->sx, std::sin(rad) * item->sy, -std::sin(rad) * item->sx,
                       std::cos(rad) * item->sy};
        blitTransformed(out, *img.image, 0, 0, img.cellWidth, img.image->h, item->cx, item->cy, m, item->alpha,
                        Blend::Normal);
    }
}

namespace {
using E = Engine;

void cmdExecDll(E &e, const std::string &) { e.execDll(e.readStr()); }

void cmdSetLayer(E &e, const std::string &) {
    int n = e.readInt();
    e.readComma();
    int interval = e.readInt();
    e.readComma();
    std::string plugin = sjisLower(e.readStr());
    e.layers_[n] = E::Layer{plugin, interval, 0};
    if (plugin != "mspdraw.dll") logf("layer plugin %s is not emulated", plugin.c_str());
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

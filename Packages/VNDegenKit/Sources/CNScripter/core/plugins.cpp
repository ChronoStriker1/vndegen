// The Windows plugins test games call, emulated: NSFont.dll's gaiji (an image drawn in place of a character) and
// layer plugins (setlayer / layermessage). Other DLL calls are logged and ignored.
#include "engine.h"

#include <algorithm>
#include <cmath>

namespace vnns {

// MSpDraw.dll, a layer plugin that moves, turns and scales sprites of its own. Its manual (version 1.10) gives the
// draw message as
//   draw/n,tag,cx,cy[,alpha,rot,zoomx,zoomy,slope,steps,dcx,dcy,dalpha,drot,dzoomx,dzoomy,dslope,acx,acy,delay]
// placing the picture by its centre; the d* values are added on each of the layer's timer ticks for `steps` ticks
// (-1 for ever), the a* values are added to dcx and dcy each tick, and `delay` ticks pass before it shows. The tag's
// mode letter may be followed by '+' or '-' for additive or subtractive blending. `set/n,name=value,...,time=count`
// changes those values and runs for count more ticks; `clear/n,n,...` and `clear/all` remove pictures. The manual
// calls slope a tilt, 0 upright and 90 flat; it's drawn as the picture's height scaled by its cosine.
struct Engine::MSpDrawItem {
    int id = 0;
    LoadedImage img;
    Blend blend = Blend::Normal;
    double cx = 0, cy = 0, alpha = 255, rot = 0, zx = 1, zy = 1, slope = 0;
    double dcx = 0, dcy = 0, dalpha = 0, drot = 0, dzx = 0, dzy = 0, dslope = 0, acx = 0, acy = 0;
    int steps = 0;  // ticks of motion left; -1 for ever
    int delay = 0;  // ticks before the picture shows
    double *field(const std::string &name) {
        static const std::pair<const char *, double MSpDrawItem::*> fields[] = {
            {"center_x", &MSpDrawItem::cx},   {"center_y", &MSpDrawItem::cy},   {"alpha", &MSpDrawItem::alpha},
            {"rot", &MSpDrawItem::rot},       {"zoom_x", &MSpDrawItem::zx},     {"zoom_y", &MSpDrawItem::zy},
            {"slope", &MSpDrawItem::slope},   {"dcenter_x", &MSpDrawItem::dcx}, {"dcenter_y", &MSpDrawItem::dcy},
            {"dalpha", &MSpDrawItem::dalpha}, {"drot", &MSpDrawItem::drot},     {"dzoom_x", &MSpDrawItem::dzx},
            {"dzoom_y", &MSpDrawItem::dzy},   {"dslope", &MSpDrawItem::dslope}, {"acenter_x", &MSpDrawItem::acx},
            {"acenter_y", &MSpDrawItem::acy}};
        for (auto &[n, m] : fields)
            if (name == n) return &(this->*m);
        return nullptr;
    }
};

namespace {
// Splits on commas, leaving two-byte characters whole.
std::vector<std::string> splitFields(const std::string &rest) {
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
    return f;
}
std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    return s.substr(i);
}
}  // namespace

void Engine::execDll(const std::string &call) {
    // "plugin.dll/function,arg,arg"
    size_t slash = call.find('/');
    std::string dll = normalizePath(call.substr(0, slash));
    dll = dll.substr(dll.rfind('\\') + 1);  // the DLL's own name, wherever the game keeps it
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
    std::vector<std::string> f = splitFields(rest);
    auto find = [&](int id) {
        return std::find_if(mspdraw_.begin(), mspdraw_.end(), [id](auto &i) { return i->id == id; });
    };
    if (verb == "clear") {
        for (const std::string &field : f) {
            std::string v = sjisLower(trim(field));
            if (v == "all") {
                mspdraw_.clear();
            } else if (auto k = find(atoi(v.c_str())); k != mspdraw_.end()) {
                mspdraw_.erase(k);
            }
        }
        sceneDirty_ = true;
        return;
    }
    if (verb == "set") {
        if (f.empty()) return;
        auto k = find(atoi(f[0].c_str()));
        if (k == mspdraw_.end()) return;
        MSpDrawItem &item = **k;
        for (size_t i = 1; i < f.size(); i++) {
            size_t eq = f[i].find('=');
            if (eq == std::string::npos) continue;
            std::string name = sjisLower(trim(f[i].substr(0, eq)));
            double value = atof(f[i].c_str() + eq + 1);
            if (name == "time") item.steps = int(value);
            else if (double *p = item.field(name)) *p = value;
            else logf("MSpDraw set: unknown value %s", name.c_str());
        }
        return;
    }
    if (verb != "draw") {
        logf("MSpDraw message %s is not emulated", sjisToUtf8(msg).c_str());
        return;
    }
    // id, then the image tag, which may itself hold commas before its ';', then numbers.
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
    // ":c+;" blends additively and ":c-;" subtractively; the tag is loaded without that sign.
    if (tag.size() > 2 && tag[0] == ':' && (tag[2] == '+' || tag[2] == '-')) {
        item->blend = tag[2] == '+' ? Blend::Add : Blend::Sub;
        tag.erase(2, 1);
    }
    double *order[] = {&item->cx,  &item->cy,     &item->alpha, &item->rot, &item->zx,  &item->zy,
                       &item->slope, nullptr,     &item->dcx,   &item->dcy, &item->dalpha, &item->drot,
                       &item->dzx, &item->dzy,    &item->dslope, &item->acx, &item->acy, nullptr};
    for (size_t j = k + 1, n = 0; j < f.size() && n < 18; j++, n++) {
        std::string v = trim(f[j]);
        if (v.empty()) continue;
        if (n == 7) item->steps = atoi(v.c_str());
        else if (n == 17) item->delay = atoi(v.c_str());
        else *order[n] = atof(v.c_str());
    }
    if (!loadImage(tag, item->img)) return;
    if (auto old = find(item->id); old != mspdraw_.end()) *old = item;
    else mspdraw_.push_back(item);
    sceneDirty_ = true;
}

// Runs each layer plugin's timer at the interval setlayer gave it.
void Engine::tickLayers() {
    for (auto &[n, layer] : layers_) {
        if (layer.interval <= 0 || now_ < layer.nextTick) continue;
        layer.nextTick = now_ + layer.interval;
        for (auto &item : mspdraw_) {
            if (item->delay > 0) {
                item->delay--;
                sceneDirty_ = true;
                continue;
            }
            if (item->steps == 0) continue;
            if (item->steps > 0) item->steps--;
            item->dcx += item->acx;
            item->dcy += item->acy;
            item->cx += item->dcx;
            item->cy += item->dcy;
            item->alpha = std::clamp(item->alpha + item->dalpha, 0.0, 255.0);
            item->rot += item->drot;
            item->zx += item->dzx;
            item->zy += item->dzy;
            item->slope += item->dslope;
            sceneDirty_ = true;
        }
    }
}

void Engine::drawLayer(int layer, Image &out) {
    auto it = layers_.find(layer);
    if (it == layers_.end() || it->second.plugin != "mspdraw.dll") return;
    for (auto &item : mspdraw_) {
        const LoadedImage &img = item->img;
        if (!img.image || item->delay > 0 || item->alpha <= 0) continue;
        // Anticlockwise on screen, as lsp2's rotation is drawn.
        double rad = item->rot * M_PI / 180.0;
        double sy = item->zy * std::fabs(std::cos(item->slope * M_PI / 180.0));
        if (sy < 0.001 || item->zx <= 0) continue;
        double m[4] = {std::cos(rad) * item->zx, std::sin(rad) * sy, -std::sin(rad) * item->zx, std::cos(rad) * sy};
        blitTransformed(out, *img.image, 0, 0, img.cellWidth, img.image->h, item->cx, item->cy, m,
                        int(std::lround(item->alpha)), item->blend);
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
    std::string plugin = normalizePath(e.readStr());
    plugin = plugin.substr(plugin.rfind('\\') + 1);  // the DLL's own name, wherever the game keeps it
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

#include "tjsCommHead.h"
#include "game_menu.h"

#include "TVPScript.h"
#include "TVPStorage.h"
#include "TextStream.h"
#include "menubar_scan.h"
#include "tjsNativeMenuItem.h"
#include "vnkk.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>

static int menuBarMode = VNKK_MENU_BAR_AUTOMATIC;
static int menuBarShown = -1;

void VNKKSetMenuBarMode(int mode)
{
    menuBarMode = mode;
    menuBarShown = -1;
}

int VNKKMenuBarShown()
{
    return menuBarShown;
}

// ".ks" for a KAG scenario, ".tjs" for TJS, or nullptr for any other file.
static const char *scriptExtension(const std::string &name)
{
    for (const char *suffix : {".ks", ".tjs"}) {
        size_t n = strlen(suffix);
        if (name.size() < n)
            continue;
        bool same = true;
        for (size_t i = 0; i < n && same; i++) {
            char c = name[name.size() - n + i];
            same = (c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c) == suffix[i];
        }
        if (same)
            return suffix;
    }
    return nullptr;
}

// Reads every script the game can load by name, through the archives' ciphers and the engine's text decoding, and
// looks for a test of whether the menu bar is hidden. A name in several search paths, such as a script a patch
// archive replaces, is read only where the engine's own lookup finds it. The game has added its search paths by the
// time it asks.
static bool scriptsTestForHiddenMenuBar()
{
    Uint64 start = SDL_GetTicks();
    int scanned = 0;
    std::vector<ttstr> storages;
    try {
        storages = TVPListAutoPathStorages();
    } catch (...) {
        SDL_Log("vnkk: could not list the game's scripts");
    }
    std::set<std::string> names, placed;
    for (const ttstr &storage : storages) {
        ttstr name = TVPExtractStorageName(storage);
        const char *extension = scriptExtension(name.AsStdString());
        if (!extension || !names.insert(name.AsLowerCase().AsStdString()).second)
            continue;
        ttstr path;
        ttstr text;
        try {
            path = TVPGetPlacedPath(name);
            if (path.IsEmpty() || !placed.insert(path.AsStdString()).second)
                continue;
            iTJSTextReadStream *stream = TVPCreateTextStreamForRead(path, TJS_N(""));
            try {
                stream->Read(text, 0);
            } catch (...) {
                stream->Destruct();
                throw;
            }
            stream->Destruct();
        } catch (...) {
            SDL_Log("vnkk: could not read %s to check its menu bar test", name.AsStdString().c_str());
            continue;
        }
        scanned++;
        if (auto hit = VNKKFindHiddenMenuBarTest(text.AsStdString(), !strcmp(extension, ".ks"))) {
            SDL_Log("vnkk: the menu bar reports hidden: %s line %zu tests for it (%s): %s", path.AsStdString().c_str(),
                    hit->line, hit->rule, hit->excerpt.c_str());
            SDL_Log("vnkk: checked %d scripts for a menu bar test in %llu ms", scanned,
                    (unsigned long long)(SDL_GetTicks() - start));
            return true;
        }
    }
    SDL_Log("vnkk: the menu bar reports shown: none of the game's %d scripts tests for a hidden one (%llu ms)",
            scanned, (unsigned long long)(SDL_GetTicks() - start));
    return false;
}

static bool menuBarShownHook()
{
    if (menuBarShown < 0) {
        switch (menuBarMode) {
        case VNKK_MENU_BAR_HIDDEN:
            SDL_Log("vnkk: the menu bar reports hidden, as the player chose");
            menuBarShown = 0;
            break;
        case VNKK_MENU_BAR_SHOWN:
            SDL_Log("vnkk: the menu bar reports shown, as the player chose");
            menuBarShown = 1;
            break;
        default:
            menuBarShown = !scriptsTestForHiddenMenuBar();
            break;
        }
    }
    return menuBarShown;
}

void VNKKInstallMenuBarHook()
{
    menuBarShown = -1;
    TVPSetMenuBarShownHook(menuBarShownHook);
}

// Items VNDegen's own menu covers or that can't work here, by the names KAG gives them on the main window: quitting,
// windowed and full screen, the help file and About window, the font dialog, which the engine lacks, and the debug
// menu. A game's own item stored under one of these names, as Tawawa's Quit Game is, is left out too.
static const tjs_char *const skippedItems =
    TJS_N("(function { var w = Window.mainWindow, r = []; if (w === null || !(isvalid w)) return r;"
          " var n = ['exitMenuItem', 'windowedMenuItem', 'fullScreenMenuItem', 'helpMenu', 'helpIndexMenuItem',"
          " 'helpAboutMenuItem', 'chChangeFontMenuItem', 'debugMenu'];"
          " for (var i = 0; i < n.count; i++) { var o = typeof w[n[i]] == 'undefined' ? void : w[n[i]];"
          " if (typeof o == 'Object' && o !== null) r.add(o); } return r; })()");

static tTJSNI_MenuItem *mainMenu(tTJSVariant &holder)
{
    TVPExecuteExpression(TJS_N("Window.mainWindow !== null && isvalid Window.mainWindow ? Window.mainWindow.menu : void"),
                         &holder);
    if (holder.Type() != tvtObject || !holder.AsObjectNoAddRef())
        return nullptr;
    return tTJSNI_BaseMenuItem::CastFromVariant(holder);
}

static std::set<iTJSDispatch2 *> skipped(tTJSVariant &holder)
{
    std::set<iTJSDispatch2 *> objects;
    TVPExecuteExpression(skippedItems, &holder);
    iTJSDispatch2 *array = holder.Type() == tvtObject ? holder.AsObjectNoAddRef() : nullptr;
    if (!array)
        return objects;
    tTJSVariant count;
    array->PropGet(0, TJS_N("count"), nullptr, &count, array);
    for (tjs_int i = 0; i < (tjs_int)count.AsInteger(); i++) {
        tTJSVariant item;
        array->PropGetByNum(0, i, &item, array);
        if (item.Type() == tvtObject && item.AsObjectNoAddRef())
            objects.insert(item.AsObjectNoAddRef());
    }
    return objects;
}

static void appendString(std::string &out, const std::string &text)
{
    out += '"';
    for (unsigned char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char escaped[8];
                snprintf(escaped, sizeof escaped, "\\u%04x", c);
                out += escaped;
            } else {
                out += (char)c;
            }
        }
    }
    out += '"';
}

// The items under `parent` that show, each with its children, as a JSON array. An item's id is the path of indexes
// from the menu bar down, such as "0.3", in the game's own lists, so hidden items keep the ids of the rest stable.
static void appendItems(std::string &out, const tTJSNI_MenuItem *parent, const std::string &path, bool enabled,
                        const std::set<iTJSDispatch2 *> &skip)
{
    out += '[';
    bool first = true;
    const auto &children = parent->GetChildren();
    for (size_t i = 0; i < children.size(); i++) {
        auto *item = static_cast<tTJSNI_MenuItem *>(children[i]);
        if (!item || !item->GetVisible() || skip.count(item->GetOwnerNoAddRef()))
            continue;
        ttstr caption;
        item->GetCaption(caption);
        std::string id = path.empty() ? std::to_string(i) : path + "." + std::to_string(i);
        bool itemEnabled = enabled && item->GetEnabled();
        if (!first)
            out += ',';
        first = false;
        out += "{\"id\":";
        appendString(out, id);
        out += ",\"title\":";
        appendString(out, caption.AsStdString());
        out += ",\"checked\":";
        out += item->GetChecked() ? "true" : "false";
        out += ",\"radio\":";
        out += item->GetRadio() ? "true" : "false";
        out += ",\"enabled\":";
        out += itemEnabled ? "true" : "false";
        out += ",\"items\":";
        appendItems(out, item, id, itemEnabled, skip);
        out += '}';
    }
    out += ']';
}

std::string VNKKMenuJSON()
{
    try {
        tTJSVariant menuHolder, skipHolder;
        tTJSNI_MenuItem *menu = mainMenu(menuHolder);
        if (!menu)
            return std::string();
        std::string out;
        appendItems(out, menu, std::string(), true, skipped(skipHolder));
        return out;
    } catch (...) {
        SDL_Log("vnkk: could not read the game's menu");
        return std::string();
    }
}

bool VNKKMenuClick(const char *id)
{
    try {
        tTJSVariant holder;
        tTJSNI_MenuItem *item = mainMenu(holder);
        for (const char *p = id; item && *p;) {
            char *end;
            unsigned long index = strtoul(p, &end, 10);
            if (end == p)
                return false;
            const auto &children = item->GetChildren();
            item = index < children.size() ? static_cast<tTJSNI_MenuItem *>(children[index]) : nullptr;
            p = *end == '.' ? end + 1 : end;
        }
        if (!item)
            return false;
        ttstr caption;
        item->GetCaption(caption);
        SDL_Log("vnkk: choosing the game's menu item %s, %s", id, caption.AsStdString().c_str());
        // As Windows does for a menu click, the click waits in the input queue for the game's next frame.
        TVPPostInputEvent(new tTVPOnMenuItemClickInputEvent(item));
        return true;
    } catch (...) {
        SDL_Log("vnkk: could not choose the game's menu item %s", id);
        return false;
    }
}

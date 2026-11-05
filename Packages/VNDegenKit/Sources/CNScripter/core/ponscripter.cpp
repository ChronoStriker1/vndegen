// Ponscripter's additions to NScripter, as the Umineko When They Cry games use them. Ponscripter games have a UTF-8
// script (0.utf) holding the text in two languages, each line marked with langjp or langen. Everything here was
// settled as a black box, with lab scripts run on the games' own engine under Wine; docs/nscripter-cleanroom.md lists
// what each lab showed.
#include "engine.h"

namespace vnns {

namespace {
using E = Engine;

// langen, langjp, langall: the language of the text that follows, until the next of them. The rest of the line is
// usually that text (langen^Hello^@).
void cmdLang(E &e, const std::string &name) {
    e.textLang_ = name == "langen" ? 0 : name == "langjp" ? 1 : -1;
    e.continueLine_ = true;
}

void cmdShowLang(E &e, const std::string &name) { e.shownLang_ = name == "showlangjp" ? 1 : 0; }

// getreadlang: the language of the text being read: -1 for all, 0 English, 1 Japanese. The games' textgosub routine
// skips the waits of the language that isn't shown with it.
void cmdGetReadLang(E &e, const std::string &) { e.setVar(e.readVar(), e.textLang_); }

// language english|japanese, in the define block. It changed neither the language shown nor the text's layout in any
// lab, so it's read and ignored.
void cmdLanguage(E &e, const std::string &) { e.readStr(); }

// h_mapfont n,"file": face n is the game's font file.
void cmdMapFont(E &e, const std::string &) {
    int n = e.readInt();
    e.readComma();
    e.fontFiles_[n] = e.readStr();
}

// h_rendering hinting, positioning: how glyphs are rasterized. The games ask for no hinting and whole-pixel positions,
// which is how the runtime draws them, so the settings are read and kept no further.
void cmdRendering(E &e, const std::string &) {
    e.readStr();
    if (e.readComma()) e.readStr();
}

// steamsetachieve "name": a Steam achievement, which VNDegen has no place for.
void cmdSteamAchievement(E &e, const std::string &) { logf("Steam achievement %s", e.readStr().c_str()); }
}  // namespace

const std::unordered_map<std::string, CommandFn> &ponscripterCommandTable() {
    static const std::unordered_map<std::string, CommandFn> table = {
        {"langen", cmdLang},          {"langjp", cmdLang},       {"langall", cmdLang},
        {"showlangen", cmdShowLang},  {"showlangjp", cmdShowLang}, {"getreadlang", cmdGetReadLang},
        {"language", cmdLanguage},    {"h_mapfont", cmdMapFont}, {"h_rendering", cmdRendering},
        {"steamsetachieve", cmdSteamAchievement},
    };
    return table;
}

}  // namespace vnns

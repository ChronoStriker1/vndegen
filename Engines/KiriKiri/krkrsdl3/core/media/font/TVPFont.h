#pragma once
#include "tjs.h"
#include "tjsHashSearch.h"
#include "TVPFontstruc.h"

void TVPGetAllFontList(std::vector<ttstr>& list);
void TVPInitFontNames();
int TVPEnumFontsProc(const ttstr& FontPath);
const ttstr& TVPGetDefaultFontName();
// VNDegen: the font that draws characters the requested font lacks. It is the default font unless the default is a
// Windows font substitute, whose coverage is narrower.
const ttstr& TVPGetGlyphFallbackFontName();
// VNDegen: drops the Windows font substitutes registered so far and registers those of the table vnfont_init last read,
// for a host that changed the installed fonts between games.
void TVPRefreshFontSubstitutes();
// VNDegen: whether `fontname` is registered as a Windows font substitute rather than a font file the game or engine has.
bool TVPIsFontSubstitute(const ttstr& fontname);
tTJSBinaryStream* TVPCreateFontStream(const ttstr& fontname);

struct TVPFontNamePathInfo
{
    ttstr Path;
    std::function<tTJSBinaryStream*(TVPFontNamePathInfo*)> Getter;
    int Index;
    // VNDegen: true for a Windows font substitute. A font file registered under the same name replaces the whole
    // entry, so it is never mistaken for one.
    bool Substitute = false;
};
TVPFontNamePathInfo* TVPFindFont(const ttstr& name);

//---------------------------------------------------------------------------
// font enumeration and existence check
//---------------------------------------------------------------------------
class tTVPttstrHash
{
public:
    static tjs_uint32 Make(const ttstr& val);
};
extern tTJSHashTable<ttstr, TVPFontNamePathInfo, tTVPttstrHash> TVPFontNames;

bool TVPFontExists(const ttstr& name);
ttstr TVPGetBeingFont(ttstr fonts);
void TVPCreateDefaultFont();
const tTVPFont& TVPGetDefaultFont();

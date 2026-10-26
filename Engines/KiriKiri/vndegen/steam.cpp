// Steam builds link krkrsteam.dll, steam_api.dll, and DrawDeviceForSteam.dll. VNDegen runs them
// without Steam: achievements, stats, cloud files, and screenshots do nothing, Steam.getLanguage
// reports the system language, and the draw device is the engine's own.
#include "ncbind/ncbind.hpp"

#include <SDL3/SDL_locale.h>

#include <cstring>
#include <string>

// Steam's API language name for the player's preferred system language.
static std::string SteamLanguage()
{
    static const struct
    {
        const char* code;
        const char* name;
    } languages[] = {
        {"ar", "arabic"},    {"bg", "bulgarian"}, {"cs", "czech"},     {"da", "danish"},
        {"de", "german"},    {"el", "greek"},     {"en", "english"},   {"fi", "finnish"},
        {"fr", "french"},    {"hu", "hungarian"}, {"id", "indonesian"}, {"it", "italian"},
        {"ja", "japanese"},  {"ko", "koreana"},   {"nb", "norwegian"}, {"nl", "dutch"},
        {"no", "norwegian"}, {"pl", "polish"},    {"ro", "romanian"},  {"ru", "russian"},
        {"sv", "swedish"},   {"th", "thai"},      {"tr", "turkish"},   {"uk", "ukrainian"},
        {"vi", "vietnamese"}};
    std::string name = "english";
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    if (locales && count > 0 && locales[0]->language)
    {
        const char* language = locales[0]->language;
        const char* country = locales[0]->country ? locales[0]->country : "";
        if (!strcmp(language, "zh"))
            name = !strcmp(country, "TW") || !strcmp(country, "HK") || !strcmp(country, "MO")
                       ? "tchinese"
                       : "schinese";
        else if (!strcmp(language, "pt"))
            name = !strcmp(country, "BR") ? "brazilian" : "portuguese";
        else if (!strcmp(language, "es"))
            name = !strcmp(country, "ES") || !*country ? "spanish" : "latam";
        else
            for (const auto& entry : languages)
                if (!strcmp(language, entry.code))
                    name = entry.name;
    }
    SDL_free(locales);
    return name;
}

// The members krkrsteam.dll registers on its Steam class, as TJS that games call on the class.
// They follow DummySteam.tjs in wamsoft's plugin source, the stand-in it ships for running without
// Steam.
static void RegisterSteam()
{
    std::string script = R"(class Steam {
        property initialized { getter { return true; } }
        property achievementsCount { getter { return 0; } }
        property cloudEnabled { getter { return false; } setter(value) {} }
        property cloudFileCount { getter { return 0; } }
        property steamID { getter { return ""; } }
        property accountID { getter { return 0; } }
        property loggedOn { getter { return false; } }
        property personaName { getter { return ""; } }
        property appID { getter { return 0; } }
        function RestartAppIfNecessary() { return false; }
        function requestInitialize() { return true; }
        function getLanguage() { return ")" + SteamLanguage() + R"("; }
        function getAchievement() {}
        function setAchievement() { return true; }
        function clearAchievement() { return true; }
        function getCloudQuota() { return %[total: 0, available: 0]; }
        function getCloudFileInfo() {}
        function deleteCloudFile() { return false; }
        function copyCloudFile() { return false; }
        function triggerScreenshot() {}
        function hookScreenshots() {}
        function writeScreenshot() { return false; }
        function isBroadcasting() { return false; }
        function hookBroadcasting() {}
        function isIsSubscribedApp() { return false; }
        function isDlcInstalled() { return false; }
        function getDLCCount() { return 0; }
        function getDLCData() {}
    })";
    TVPExecuteScript(ttstr(script));
}

#define NCB_MODULE_NAME TJS_N("krkrsteam.dll")
NCB_POST_REGIST_CALLBACK(RegisterSteam);
#undef NCB_MODULE_NAME

static void RegisterDrawDevice()
{
    TVPExecuteScript(TJS_N("global.DrawDeviceForSteam = Window.BasicDrawDevice;"));
}

#define NCB_MODULE_NAME TJS_N("DrawDeviceForSteam.dll")
NCB_POST_REGIST_CALLBACK(RegisterDrawDevice);

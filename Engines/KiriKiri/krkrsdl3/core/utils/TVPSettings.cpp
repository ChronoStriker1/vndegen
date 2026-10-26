#include "tjsCommHead.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include "TVPSettings.h"

#include "TVPWindow.h"

#include "TVPSystem.h"
#include "TVPStorage.h"
#include "TVPDebug.h"
#include "TVPMsg.h"
#include "Platform.h"
#include "PlatformView.h"
#include "PlatformFile.h"

#include "TVPCompositor.h"
#include "tjsLex.h"

TVPGlobalSettings TVPSettings;
static std::vector<ttstr> TVPProgramArguments;
static void TVPDumpOptions()
{
    std::vector<ttstr>::const_iterator i;
    ttstr options("(info) Specified option :");
    if (TVPProgramArguments.size())
    {
        for (i = TVPProgramArguments.begin(); i != TVPProgramArguments.end(); i++)
        {
            options += TJS_N(" ");
            options += *i;
        }
    }
    else
    {
        options += (const tjs_char*)TVPNone;
    }
    TVPAddImportantLog(options);
}
static ttstr TVPParseCommandLineOne(const ttstr& i)
{
    // value is specified
    const tjs_char *p, *o;
    p = o = i.c_str();
    p = TJS_strchr(p, '=');

    if (p == NULL)
    {
        return i + TJS_N("=yes");
    }

    p++;

    ttstr optname(o, (int)(p - o));
    // A quoted value is a TJS string literal, as .cf files write them.
    if (*p == TJS_N('\'') || *p == TJS_N('"'))
    {
        try
        {
            tTJSVariant value;
            if (TJSParseString(value, &p))
                return optname + ttstr(value);
        }
        catch (...)
        {
        }
    }
    return optname + p;
}
static void TVPInitProgramArguments(int tvp_argc, char* tvp_argv[])
{
    // find options from self executable image
    bool argument_stopped = false;
    int file_argument_count = 0;
    for (tjs_int i = 1; i < tvp_argc; i++)
    {
        if (argument_stopped)
        {
            ttstr arg_name_and_value =
                TJS_N("-arg") + ttstr(file_argument_count) + TJS_N("=") + ttstr(tvp_argv[i]);
            file_argument_count++;
            TVPProgramArguments.push_back(arg_name_and_value);
        }
        else
        {
            if (tvp_argv[i][0] == TJS_N('-'))
            {
                if (tvp_argv[i][1] == TJS_N('-') && tvp_argv[i][2] == 0)
                {
                    // argument stopper
                    argument_stopped = true;
                }
                else
                {
                    ttstr value(tvp_argv[i]);
                    if (!TJS_strchr(value.c_str(), TJS_N('=')))
                        value += TJS_N("=yes");
                    TVPProgramArguments.push_back(TVPParseCommandLineOne(value));
                }
            }
        }
    }
}
static void TVPDetectRender()
{
    // SDL 渲染驱动列表（仅信息展示）
    TVPListAllRenderBackend();
    // 已注册且平台可用的渲染后端（见 core/render/backend/RenderBackend.h）
    std::vector<std::string> backends = krkrsdl3::TVPListRenderBackends();
    auto hasBackend = [&backends](const char* name)
    { return std::find(backends.begin(), backends.end(), name) != backends.end(); };
    // 选择编译后端
    tTJSVariant opt;
    TVPSettings.renderer = "software"; // 软渲染保底
    if (TVPGetCommandLine(TJS_N("-render"), &opt))
    {
        ttstr str(opt);
        if (str == TJS_N("opengl") || str == TJS_N("gl") || str == TJS_N("gpu"))
        {
            if (hasBackend("opengl"))
                TVPSettings.renderer = "opengl";
            else
                TVPAddImportantLog(ttstr(TJS_N("Renderer 'opengl' is not available, using '")) +
                                   ttstr(TVPSettings.renderer) + TJS_N("'"));
        }
        else if (str == TJS_N("vulkan") || str == TJS_N("vk"))
        {
            if (hasBackend("vulkan"))
                TVPSettings.renderer = "vulkan";
            else
                TVPAddImportantLog(ttstr(TJS_N("Renderer 'vulkan' is not available, using '")) +
                                   ttstr(TVPSettings.renderer) + TJS_N("'"));
        }
        else if (str == TJS_N("metal") || str == TJS_N("mtl"))
        {
            if (hasBackend("metal"))
                TVPSettings.renderer = "metal";
            else
                TVPAddImportantLog(ttstr(TJS_N("Renderer 'metal' is not available, using '")) +
                                   ttstr(TVPSettings.renderer) + TJS_N("'"));
        }
        else if (str == TJS_N("d3d11") || str == TJS_N("d3d"))
        {
            if (hasBackend("d3d11"))
                TVPSettings.renderer = "d3d11";
            else
                TVPAddImportantLog(ttstr(TJS_N("Renderer 'd3d11' is not available, using '")) +
                                   ttstr(TVPSettings.renderer) + TJS_N("'"));
        }
        else if (str == TJS_N("d3d12"))
        {
            if (hasBackend("d3d12"))
                TVPSettings.renderer = "d3d12";
            else
                TVPAddImportantLog(ttstr(TJS_N("Renderer 'd3d12' is not available, using '")) +
                                   ttstr(TVPSettings.renderer) + TJS_N("'"));
        }
        else if (str == TJS_N("d3d9"))
        {
            if (hasBackend("d3d9"))
                TVPSettings.renderer = "d3d9";
            else
                TVPAddImportantLog(ttstr(TJS_N("Renderer 'd3d9' is not available, using '")) +
                                   ttstr(TVPSettings.renderer) + TJS_N("'"));
        }
        else if (str == TJS_N("software") || str == TJS_N("sw"))
        {
            TVPSettings.renderer = "software";
        }
        else
        {
            TVPAddImportantLog(ttstr(TJS_N("Unknown renderer '")) + str +
                               TJS_N("', using default '") + ttstr(TVPSettings.renderer) +
                               TJS_N("'"));
        }
    }
    else
    // 未指定时自动选择
    // 默认选择（无需做任何区分，编译时已确认平台）
    // 优先判定平台相关后端，再判定通用后端
    {
        // Apple系列
        if (hasBackend("metal"))
            TVPSettings.renderer = "metal";
        // Windows系列
        else if (hasBackend("d3d11"))
            TVPSettings.renderer = "d3d11";
        else if (hasBackend("d3d12"))
            TVPSettings.renderer = "d3d12";
        else if (hasBackend("d3d9"))
            TVPSettings.renderer = "d3d9";
        // 通用系列
        else if (hasBackend("opengl"))
            TVPSettings.renderer = "opengl";
        else if (hasBackend("vulkan"))
            TVPSettings.renderer = "vulkan";
        else
            TVPSettings.renderer = "software";
    }
    TVPAddImportantLog(ttstr("Selected Render: ") + TVPSettings.renderer);

    // 初始窗口尺寸：-window=WxH（默认 1280x720）
    TVPSettings.window_width = 1280;
    TVPSettings.window_height = 720;
    if (TVPGetCommandLine(TJS_N("-window"), &opt))
    {
        ttstr str(opt);
        const tjs_char* p = str.c_str();
        int w = 0, h = 0;
        while (*p >= '0' && *p <= '9')
        {
            w = w * 10 + (*p - '0');
            p++;
        }
        if (*p == 'x' || *p == 'X')
        {
            p++;
            while (*p >= '0' && *p <= '9')
            {
                h = h * 10 + (*p - '0');
                p++;
            }
        }
        if (w > 0 && h > 0)
        {
            TVPSettings.window_width = w;
            TVPSettings.window_height = h;
            TVPAddImportantLog(ttstr("Window size: ") + ttstr(w) + TJS_N("x") + ttstr(h));
        }
        else
        {
            TVPAddImportantLog(ttstr("Invalid -window value '") + str +
                               TJS_N("', using default 1280x720"));
        }
    }

    // 垂直同步：-vsync=0/1（默认开启）
    TVPSettings.vsync = 1;
    if (TVPGetCommandLine(TJS_N("-vsync"), &opt))
    {
        ttstr str(opt);
        if (str == TJS_N("0"))
            TVPSettings.vsync = 0;
        else if (str == TJS_N("1"))
            TVPSettings.vsync = 1;
        else
            TVPAddImportantLog(ttstr(TJS_N("Invalid -vsync value '")) + str + TJS_N("', using 1"));
    }

    // 画面比例：-stretch=0/1（默认 0=保持比例 letterbox；1=拉伸铺满窗口）
    krkrsdl3::TVPSetStretchMode(false);
    if (TVPGetCommandLine(TJS_N("-stretch"), &opt))
    {
        ttstr str(opt);
        if (str == TJS_N("1"))
            krkrsdl3::TVPSetStretchMode(true);
        else if (str != TJS_N("0"))
            TVPAddImportantLog(ttstr(TJS_N("Invalid -stretch value '")) + str + TJS_N("', using 0"));
    }
}

// System.exeName is the game's .exe on Windows, and games look for files beside it, such as
// <exe>.cf. A folder can hold tools too, so the .exe that has a .cf wins, then a lone .exe.
static ttstr TVPFindGameExe(const ttstr& dir)
{
    auto stem = [](std::string name, const char* ext)
    {
        for (char& c : name)
            if (c >= 'A' && c <= 'Z')
                c += 'a' - 'A';
        size_t length = strlen(ext);
        bool matches = name.size() > length && !name.compare(name.size() - length, length, ext);
        return matches ? name.substr(0, name.size() - length) : std::string();
    };
    std::vector<std::string> exes;
    std::vector<std::string> configs;
    TVPGetLocalFileListAt(dir,
                          [&](const ttstr&, tTVPLocalFileInfo* s)
                          {
                              // The lister's name is lowercased for the auto path table. The
                              // file is opened by path, which is case sensitive on iOS.
                              std::string file = s->NativeName;
                              if (!(s->Mode & S_IFREG))
                                  return;
                              if (!stem(file, ".exe").empty())
                                  exes.push_back(file);
                              else if (!stem(file, ".cf").empty())
                                  configs.push_back(stem(file, ".cf"));
                          });
    std::sort(exes.begin(), exes.end());
    for (const std::string& exe : exes)
        if (std::find(configs.begin(), configs.end(), stem(exe, ".exe")) != configs.end())
            return dir + ttstr(exe);
    return exes.size() == 1 ? dir + ttstr(exes.front()) : ttstr();
}

// A folder named by a command-line option, as a native path ending in a slash. Without the option, `fallback`.
static ttstr TVPNativeFolderOption(const tjs_char* name, const ttstr& fallback)
{
    tTJSVariant value;
    if (!TVPGetCommandLine(name, &value))
        return fallback;
    ttstr folder(value);
    if (folder.GetLastChar() != TJS_N('/'))
        folder += TJS_N("/");
    return folder;
}

// Options a game's .cf may not set: VNDegen chooses the renderer, the window, the game folder,
// and the folders that stand in for AppData and Documents, and a game may not name a program
// to run. -forcelog opens krkr.console.log while options are still being read, which aborts the
// engine, and VNDegen keeps its own log.
static bool TVPConfigOptionIgnored(const ttstr& option)
{
    static const tjs_char* const ignored[] = {
        TJS_N("-render="),       TJS_N("-drawthread="), TJS_N("-window="),   TJS_N("-vsync="),
        TJS_N("-stretch="),      TJS_N("-smoothzoom="), TJS_N("-gameroot="), TJS_N("-appdatadir="),
        TJS_N("-personaldir="),  TJS_N("-exceptionexe="), TJS_N("-exceptionarg="), TJS_N("-forcelog=")};
    for (const tjs_char* name : ignored)
        if (option.StartsWith(name))
            return true;
    return false;
}

// A game's <exe>.cf holds options as on Windows: a NAME="VALUE" line each, the value a TJS
// string, and comments starting with ';'. They rank below the command line.
static void TVPReadConfigFileOptions(const ttstr& exe)
{
    std::string path = exe.AsStdString();
    path.replace(path.size() - 4, 4, ".cf");
    std::ifstream file(path);
    if (!file)
        return;
    ttstr applied, ignored;
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == ';')
            continue;
        ttstr option = TVPParseCommandLineOne(ttstr("-" + line));
        if (TVPConfigOptionIgnored(option))
        {
            ignored += ttstr(TJS_N(" ")) + option;
            continue;
        }
        applied += ttstr(TJS_N(" ")) + option;
        TVPProgramArguments.push_back(option);
    }
    TVPAddImportantLog(ttstr(TJS_N("(info) Options from the .cf file :")) + applied);
    if (!ignored.IsEmpty())
        TVPAddImportantLog(ttstr(TJS_N("(info) Ignored options from the .cf file :")) + ignored);
}

// -datapath moves savedata/, as on Windows. Its macros name the game folder and the folders that
// stand in for AppData and Documents. $(vistapath) is AppData, as on Windows Vista and later, and
// Saved Games is Documents here. A relative path is in the game folder. Returns an empty string
// for a path outside those folders.
static ttstr TVPResolveDataPath(ttstr path, const ttstr& appData, const ttstr& personal)
{
    const ttstr& root = TVPNativeProjectDir;
    const std::pair<const tjs_char*, const ttstr*> macros[] = {
        {TJS_N("$(exepath)"), &root},       {TJS_N("$(personalpath)"), &personal},
        {TJS_N("$(appdatapath)"), &appData}, {TJS_N("$(vistapath)"), &appData},
        {TJS_N("$(savedgamespath)"), &personal}};
    for (const auto& [macro, folder] : macros)
        path.Replace(macro, folder->SubString(0, folder->GetLen() - 1));
    path.Replace(TJS_N("\\"), TJS_N("/"));
    if (!path.StartsWith(TJS_N("/")))
        path = root + path;
    if (path.GetLastChar() != TJS_N('/'))
        path += TJS_N("/");
    if (path.IndexOf(ttstr(TJS_N("/../"))) >= 0 || path.IndexOf(TJS_N(':')) >= 0)
        return ttstr();
    for (const ttstr* folder : {&root, &appData, &personal})
        if (path.StartsWith(*folder))
            return path;
    return ttstr();
}

bool TVPParseArguments(int argc, char* argv[])
{
    // exeName
    TVPNativeExeName = argv[0];
    TVPNativeExeDir = TVPExtractStoragePath(TVPNativeExeName);
    TVPAddImportantLog(TVPFormatMessage("(info) Exe path : %1", TVPNativeExeName));
    // args
    TVPInitProgramArguments(argc, argv);
    TVPDumpOptions();

    // datas
    TVPNativeProjectData = argv[1];
    if (TVPCheckExistentLocalFolder(TVPNativeProjectData))
    {
        // Storage names inside a folder are its path with the file name appended.
        if (TVPNativeProjectData.GetLastChar() != TJS_N('/'))
            TVPNativeProjectData += TJS_N("/");
    }
    else if (!TVPCheckExistentLocalFile(TVPNativeProjectData))
    {
        TVPAddImportantLog(TVPFormatMessage("%1 not found.", TVPNativeProjectData));
        return false;
    }
    // -gameroot names the folder that holds the game's executable. It becomes
    // System.exePath and the base for savedata/, as on Windows, wherever the
    // project data itself lives.
    tTJSVariant gameRoot;
    if (TVPGetCommandLine(TJS_N("-gameroot"), &gameRoot))
    {
        TVPNativeProjectDir = ttstr(gameRoot);
        if (TVPNativeProjectDir.GetLastChar() != TJS_N('/'))
            TVPNativeProjectDir += TJS_N("/");
        TVPNativeGameExe = TVPFindGameExe(TVPNativeProjectDir);
        if (!TVPNativeGameExe.IsEmpty())
            TVPReadConfigFileOptions(TVPNativeGameExe);
    }
    else if (TVPCheckExistentLocalFolder(TVPNativeProjectData))
    {
        TVPNativeProjectDir = TVPNativeProjectData;
    }
    else
    {
        TVPNativeProjectDir = TVPExtractStoragePath(TVPNativeProjectData);
    }
    // TVPNormalizeStorageName->file://
    TVPProjectData = TVPNormalizeStorageName(TVPNativeProjectData);
    TVPProjectDir = TVPNormalizeStorageName(TVPNativeProjectDir);
    TVPAddImportantLog(TVPFormatMessage("(info) Game path : %1", TVPProjectData));
    // System.appDataPath and System.personalPath are the AppData and Documents folders on Windows, where some games
    // keep their saves. -appdatadir and -personaldir name the folders that stand in for them.
    ttstr nativeAppData = TVPNativeFolderOption(TJS_N("-appdatadir"), TVPNativeProjectDir);
    ttstr nativePersonal = TVPNativeFolderOption(TJS_N("-personaldir"), TVPNativeProjectDir);
    TVPAppDataDir = TVPNormalizeStorageName(nativeAppData);
    TVPPersonalDir = TVPNormalizeStorageName(nativePersonal);

    // savedata path
    TVPDataPath = TVPProjectDir + TJS_N("savedata/");
    tTJSVariant dataPathOption;
    if (TVPGetCommandLine(TJS_N("-datapath"), &dataPathOption) && !ttstr(dataPathOption).IsEmpty())
    {
        ttstr dataPath = TVPResolveDataPath(dataPathOption, nativeAppData, nativePersonal);
        if (dataPath.IsEmpty())
            TVPAddImportantLog(TVPFormatMessage("(info) Ignoring -datapath=%1, which leaves the game's folders",
                                                ttstr(dataPathOption)));
        else
            TVPDataPath = TVPNormalizeStorageName(dataPath);
    }
    TVPNativeDataPath = TVPGetLocallyAccessibleName(TVPDataPath);
    TVPAddImportantLog(TVPFormatMessage("(info) Savedata path : %1", TVPDataPath));

    // set log output directory
    TVPSetLogLocation(TVPNativeDataPath);

    // check CPU type
    TVPDetectCPU();

    // check render
    TVPDetectRender();

    // 其他设置
    TVPSettings.ogl_accurate_render = false;
    TVPSettings.default_font = "";
    TVPSettings.force_default_font = false;
    TVPSettings.software_draw_thread = 0;

    return true;
}
void TVPClearAllArguments()
{
    TVPProgramArguments.clear();
    TVPNativeExeName.Clear();
    TVPNativeExeDir.Clear();
    TVPNativeProjectData.Clear();
    TVPNativeProjectDir.Clear();
    TVPNativeGameExe.Clear();
    TVPNativeDataPath.Clear();
    TVPProjectData.Clear();
    TVPProjectDir.Clear();
    TVPDataPath.Clear();
    TVPAppDataDir.Clear();
    TVPPersonalDir.Clear();
}
bool TVPGetCommandLine(const tjs_char* name, tTJSVariant* value)
{
    tjs_int namelen = (tjs_int)TJS_strlen(name);
    std::vector<ttstr>::const_iterator i;
    for (i = TVPProgramArguments.begin(); i != TVPProgramArguments.end(); i++)
    {
        if (!TJS_strncmp(i->c_str(), name, namelen))
        {
            if (i->c_str()[namelen] == TJS_N('='))
            {
                // value is specified
                const tjs_char* p = i->c_str() + namelen + 1;
                if (value)
                    *value = p;
                return true;
            }
            else if (i->c_str()[namelen] == 0)
            {
                // value is not specified
                if (value)
                    *value = TJS_N("yes");
                return true;
            }
        }
    }
    return false;
}
void TVPSetCommandLine(const tjs_char* name, const ttstr& value)
{
    tjs_int namelen = (tjs_int)TJS_strlen(name);
    std::vector<ttstr>::iterator i;
    for (i = TVPProgramArguments.begin(); i != TVPProgramArguments.end(); i++)
    {
        if (!TJS_strncmp(i->c_str(), name, namelen))
        {
            if (i->c_str()[namelen] == TJS_N('=') || i->c_str()[namelen] == 0)
            {
                // value found
                *i = ttstr(i->c_str(), namelen) + TJS_N("=") + value;
                return;
            }
        }
    }

    // value not found; insert argument into front
    TVPProgramArguments.insert(TVPProgramArguments.begin(), ttstr(name) + TJS_N("=") + value);
}
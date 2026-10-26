#include "tjsCommHead.h"

#include <algorithm>
#include <string>
#include <vector>

#include "tjsError.h"
#include "tjsDebug.h"

#include "TVPApplication.h"
#include "TVPSystem.h"
#include "TVPDebug.h"
#include "TVPMsg.h"
#include "movie_player.h"
#include "TVPScript.h"
#include "TVPPlugin.h"

#include "Exception.h"
#include "SystemControl.h"
#include "TVPWaveManager.h"
#include "GraphicsLoadThread.h"
#include "Platform.h"
#include "PlatformView.h"
#include "RenderManager.h"
#include "WindowManager.h"
#include "TVPEvent.h"
#include "TVPCompositor.h"

#include "TVPStorage.h"
#include "TVPColor.h"
#include "TVPFont.h"
#include "TVPTimer.h"

tTVPApplication* Application = NULL;
static tTJSCriticalSection _NoMemCallBackCS;
static void* _reservedMem = malloc(1024 * 1024 * 4); // 4M reserved mem

ttstr TVPGetErrorDialogTitle()
{
    const ttstr& title = TVPGetWindowTitle();
    if (title.IsEmpty())
    {
        return TVPGetPackageVersionString() + " Error";
    }
    else
    {
        return ttstr(TVPGetPackageVersionString()) + " " + title;
    }
}

void TVPOnError();
void TVPLockSoundMixer();
void TVPUnlockSoundMixer();

static bool _warnLowMem = true;
void TVPCheckMemory()
{
#if defined(_DEBUG)
    if (_warnLowMem)
    {
        tjs_int freeMem = TVPGetSystemFreeMemory();
        if (freeMem < 24)
        {
            char buf[256];
            sprintf(buf,
                    "Insufficient memory (%dMB available)\nYou can diable this notice in global "
                    "preference.",
                    freeMem);
            const char* btn = "OK";
            TVPShowSimpleMessageBox(buf, "No Memory Warning", 1, &btn);
            _warnLowMem = false;
        }
    }
#endif
}

tTVPApplication::tTVPApplication()
  : tarminate_(false),
    image_load_thread_(NULL),
    has_map_report_process_(false)
{
}
tTVPApplication::~tTVPApplication()
{
    delete image_load_thread_;
}

bool tTVPApplication::StartApplication()
{
    TVPTerminated = false;
    TVPTerminateOnWindowClose = true;
    TVPTerminateOnNoWindowStartup = true;
    TVPTerminateCode = 0;

    // try starting the program!
    try
    {
        TVPResetTimerSystem();
        TVPInitScriptEngine();
        TVPInitFontNames();

        // banner
        TVPAddImportantLog(
            TVPFormatMessage(TVPProgramStartedOn, TVPGetOSName(), TVPGetPlatformName()));

        // TVPInitializeBaseSystems
        TVPInitializeBaseSystems();
        
        image_load_thread_ = new tTVPAsyncImageLoader();

        TVPLoadPluigins(); // load plugin module *.tpm
        TVPSystemInit();

        TVPSetWindowTitle(TVPKirikiri.operator const tjs_char*());
        TVPSystemControl = new tTVPSystemControl();

        // start image load thread
        image_load_thread_->Resume();

        TVPInitializeStartupScript();
    }
    catch (const EAbort&)
    {
        // nothing to do
    }
    // A game whose startup fails must not take the host process down with it.
    catch (const eTJSScriptError& e)
    {
        TVPAddImportantLog(ttstr(TVPScriptExceptionRaised) + TJS_N("\n") + e.GetMessage());
        if (e.GetTrace().GetLen())
            TVPAddImportantLog(ttstr(TJS_N("trace : ")) + e.GetTrace());
        return false;
    }
    catch (const eTJS& e)
    {
        TVPAddImportantLog(ttstr(TVPScriptExceptionRaised) + TJS_N("\n") + e.GetMessage());
        return false;
    }

    return true;
}

bool tTVPApplication::Run()
{
    try
    {
        if (TVPTerminated)
            return false;
        ProcessMessages();
        if (TVPSystemControl)
            TVPSystemControl->SystemWatchTimerTimer();
    }
    catch (const EAbort&)
    {
        // nothing to do
    }
    iTVPTexture2D::RecycleProcess();
    return true;
}

void tTVPApplication::ProcessMessages()
{
    // Messages run one at a time off the queue, so a handler that cancels a later message of this
    // batch, as deleting a VideoOverlay does, stops it. Messages posted meanwhile wait for the next call.
    tjs_uint64 end;
    {
        tTJSCSH cs(m_msgQueueLock);
        end = m_nextMsgSeq;
    }
    for (;;)
    {
        tMsg func;
        {
            tTJSCSH cs(m_msgQueueLock);
            if (m_lstUserMsg.empty() || m_lstUserMsg.front().seq >= end)
                break;
            func = std::move(m_lstUserMsg.front().func);
            m_lstUserMsg.pop_front();
        }
        func();
    }
    TVPTimer::ProgressAllTimer();
}

void tTVPApplication::Terminate()
{
    tarminate_ = true;
    TVPTerminated = true;
}

void tTVPApplication::PostUserMessage(const std::function<void()>& func, void* host, int msg)
{
    tTJSCSH cs(m_msgQueueLock);
    m_lstUserMsg.push_back({host, msg, func, m_nextMsgSeq++});
}

void tTVPApplication::CancelUserMessages(void* host, int msg)
{
    tTJSCSH cs(m_msgQueueLock);
    for (auto it = m_lstUserMsg.begin(); it != m_lstUserMsg.end();)
    {
        if (it->host == host && (!msg || it->msg == msg))
            it = m_lstUserMsg.erase(it);
        else
            ++it;
    }
}

void tTVPApplication::OnExit()
{
    TVPSystemUninit();
    TVPUnloadPlugins();

    delete image_load_thread_;
    image_load_thread_ = NULL;
    TVPDeliverCompactEvent(TVP_COMPACT_LEVEL_MAX);

    if (TVPSystemControl)
        delete TVPSystemControl;
    TVPSystemControl = NULL;
    TVPUninitScriptEngine();
    VNKKDestroyMoviePlayers();
    TVPClearAllAutoPath();
    TVPClearAllWindows();
    krkrsdl3::TVPClearAllTexture();
    iTVPTexture2D::RecycleProcess();
}

void tTVPApplication::LoadImageRequest(class iTJSDispatch2* owner,
                                       class tTJSNI_Bitmap* bmp,
                                       const ttstr& name)
{
    if (image_load_thread_)
    {
        image_load_thread_->LoadRequest(owner, bmp, name);
    }
}

void tTVPApplication::RegisterActiveEvent(
    void* host, const std::function<void(void*, eTVPActiveEvent)>& func /*empty = unregister*/)
{
    if (func)
        m_activeEvents.emplace(host, func);
    else
        m_activeEvents.erase(host);
}

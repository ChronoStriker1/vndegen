#include "tjsCommHead.h"

#include "krmovie.h"
#include "krnull.h"
#include "TVPDebug.h"

// VNDegen has no video player yet. Every movie, in every mode, is reported and
// replaced by a player that ends at once.
static void CreateUnsupportedVideo(iTVPVideoCallback* callbackwin,
                                   const tjs_char* streamname,
                                   iTVPVideoOverlay** out)
{
    TVPAddImportantLog(ttstr(TJS_N("video not supported, skipping ")) + streamname);
    *out = new NullVideoPlayer(callbackwin);
}

void GetVideoOverlayObject(iTVPVideoCallback* callbackwin,
                           tTJSBinaryStream* stream,
                           const tjs_char* streamname,
                           const tjs_char* type,
                           uint64_t size,
                           iTVPVideoOverlay** out)
{
    CreateUnsupportedVideo(callbackwin, streamname, out);
}

void GetVideoLayerObject(iTVPVideoCallback* callbackwin,
                         tTJSBinaryStream* stream,
                         const tjs_char* streamname,
                         const tjs_char* type,
                         uint64_t size,
                         iTVPVideoOverlay** out)
{
    CreateUnsupportedVideo(callbackwin, streamname, out);
}

void GetMixingVideoOverlayObject(iTVPVideoCallback* callbackwin,
                                 tTJSBinaryStream* stream,
                                 const tjs_char* streamname,
                                 const tjs_char* type,
                                 uint64_t size,
                                 iTVPVideoOverlay** out)
{
    CreateUnsupportedVideo(callbackwin, streamname, out);
}

void GetMFVideoOverlayObject(iTVPVideoCallback* callbackwin,
                             tTJSBinaryStream* stream,
                             const tjs_char* streamname,
                             const tjs_char* type,
                             uint64_t size,
                             iTVPVideoOverlay** out)
{
    CreateUnsupportedVideo(callbackwin, streamname, out);
}

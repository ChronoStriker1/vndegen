#include "tjsCommHead.h"

#include "krmovie.h"
#include "krnull.h"
#include "movie_player.h"

// Every mode plays through VNDegen's FFmpeg player. A movie it cannot decode is replaced by a
// player that ends at once, so a game waiting on it continues.
static void CreatePlayer(iTVPVideoCallback* callbackwin,
                         tTJSBinaryStream* stream,
                         const tjs_char* streamname,
                         MovieMode mode,
                         iTVPVideoOverlay** out)
{
    *out = VNKKCreateMoviePlayer(callbackwin, stream, streamname, mode);
    if (!*out)
        *out = new NullVideoPlayer(callbackwin);
}

void GetVideoOverlayObject(iTVPVideoCallback* callbackwin,
                           tTJSBinaryStream* stream,
                           const tjs_char* streamname,
                           const tjs_char* type,
                           uint64_t size,
                           iTVPVideoOverlay** out)
{
    CreatePlayer(callbackwin, stream, streamname, MovieMode::Overlay, out);
}

void GetVideoLayerObject(iTVPVideoCallback* callbackwin,
                         tTJSBinaryStream* stream,
                         const tjs_char* streamname,
                         const tjs_char* type,
                         uint64_t size,
                         iTVPVideoOverlay** out)
{
    CreatePlayer(callbackwin, stream, streamname, MovieMode::Layer, out);
}

void GetMixingVideoOverlayObject(iTVPVideoCallback* callbackwin,
                                 tTJSBinaryStream* stream,
                                 const tjs_char* streamname,
                                 const tjs_char* type,
                                 uint64_t size,
                                 iTVPVideoOverlay** out)
{
    CreatePlayer(callbackwin, stream, streamname, MovieMode::Mixer, out);
}

void GetMFVideoOverlayObject(iTVPVideoCallback* callbackwin,
                             tTJSBinaryStream* stream,
                             const tjs_char* streamname,
                             const tjs_char* type,
                             uint64_t size,
                             iTVPVideoOverlay** out)
{
    CreatePlayer(callbackwin, stream, streamname, MovieMode::MediaFoundation, out);
}

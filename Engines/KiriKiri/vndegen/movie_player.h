// The engine's movie player, behind krmovie.h's iTVPVideoOverlay. See movie_player.cpp.
#ifndef VNKK_MOVIE_PLAYER_H
#define VNKK_MOVIE_PLAYER_H

#include "krmovie.h"

// How a movie reaches the screen, one per krmovie.h factory function.
enum class MovieMode {
    Overlay,         // drawn over the window, in its rectangle
    Layer,           // drawn into two bitmaps a layer shows, with EC_UPDATE per frame
    Mixer,           // drawn as Overlay; the mixing bitmap is ignored
    MediaFoundation, // drawn as Overlay, reporting status through WM_STATE_CHANGE
};

// Creates a player that owns stream, or returns null when the movie cannot be decoded. The stream
// is deleted either way.
iTVPVideoOverlay *VNKKCreateMoviePlayer(iTVPVideoCallback *callback, tTJSBinaryStream *stream,
                                        const ttstr &name, MovieMode mode);

#endif

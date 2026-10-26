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

// Stops every player's decoder and deletes its stream before engine shutdown deletes plugins and the
// script engine, which a decoder's archive reads use: a game's xp3filter.tjs decrypts them, and the
// stream holds the context its content filter returned. The players stay alive for
// their VideoOverlays to release, and movies opened from then on play as the null player.
void VNKKStopMovieWorkers();

// Deletes the players still alive once the script engine is gone. Engine shutdown deletes leftover
// script objects without invalidating them, so a VideoOverlay held there never releases its player,
// whose decoder would go on using a sound buffer SDL is about to destroy.
void VNKKDestroyMoviePlayers();

#endif

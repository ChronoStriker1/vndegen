// The movie decoder the Linux tools lend the runtime: vnffmovie, the app's decoder, built here against the system's
// FFmpeg when CMake finds it. Without it the tools pass none, and games run with their movies left out.
#pragma once
#include "vnneko.h"

#ifdef NEKO_HOST_MOVIES
#include "vnffmovie.h"

// vnffmovie's functions under the runtime's names: the same layouts, with the movie as a void pointer.
static_assert(sizeof(vnffmovie_info) == sizeof(VNNekoMovieInfo), "the movie info layouts must match");
inline const VNNekoMovies *hostMovies() {
    static const VNNekoMovies movies = {
        reinterpret_cast<decltype(VNNekoMovies::open)>(vnffmovie_open),
        reinterpret_cast<decltype(VNNekoMovies::frame)>(vnffmovie_frame),
        reinterpret_cast<decltype(VNNekoMovies::audio)>(vnffmovie_audio),
        reinterpret_cast<decltype(VNNekoMovies::seek)>(vnffmovie_seek),
        reinterpret_cast<decltype(VNNekoMovies::close)>(vnffmovie_close),
    };
    return &movies;
}
#else
inline const VNNekoMovies *hostMovies() { return nullptr; }
#endif

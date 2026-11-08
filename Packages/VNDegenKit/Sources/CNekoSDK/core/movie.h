// A movie on a sprite, through the decoder the host lends the runtime (VNNekoMovies). Its frames become the sprite's
// picture and its sound streams into the mix.
#pragma once
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "archive.h"
#include "image.h"
#include "vnneko.h"

namespace neko {

class Movie {
public:
    // Null, with the reason in `error`, when the decoder can't play it. A file in an archive is read into memory, and
    // a loose one, the usual place for a movie, is read from disk as it plays.
    static std::shared_ptr<Movie> open(const VNNekoMovies &api, const Files &files, const std::string &path, bool loop, std::string &error);
    ~Movie();
    Movie(const Movie &) = delete;
    Movie &operator=(const Movie &) = delete;

    // The picture due `seconds` into the movie, a new one only when it changed, and whether the movie has ended.
    ImageRef frame(double seconds, bool &ended);
    bool hasSound() const { return info_.audioRate > 0; }
    // Sound as 44.1 kHz stereo, for the mix. Called on the audio thread.
    int sound(float *out, int frames);

private:
    Movie() = default;
    static int64_t read(void *user, uint64_t offset, void *buffer, size_t length);
    VNNekoMovies api_{};
    void *movie_ = nullptr;
    VNNekoMovieInfo info_{};
    FILE *file_ = nullptr;
    Bytes bytes_;
    uint64_t size_ = 0;
    ImageRef current_;
    std::vector<float> in_;
    double resampleAt_ = 0;  // where 44.1 kHz output is in the movie's own samples
};

}  // namespace neko

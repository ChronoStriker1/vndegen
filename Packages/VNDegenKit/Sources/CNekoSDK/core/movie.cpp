#include "movie.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace neko {

int64_t Movie::read(void *user, uint64_t offset, void *buffer, size_t length) {
    auto *m = static_cast<Movie *>(user);
    if (offset >= m->size_) return 0;
    length = size_t(std::min<uint64_t>(length, m->size_ - offset));
    if (m->bytes_) {
        std::memcpy(buffer, m->bytes_->data() + offset, length);
        return int64_t(length);
    }
    if (std::fseek(m->file_, long(offset), SEEK_SET)) return -1;
    return int64_t(std::fread(buffer, 1, length, m->file_));
}

std::shared_ptr<Movie> Movie::open(const VNNekoMovies &api, const Files &files, const std::string &path, bool loop, std::string &error) {
    std::shared_ptr<Movie> m(new Movie);
    m->api_ = api;
    if (std::string loose = files.loosePath(path); !loose.empty()) {
        m->file_ = std::fopen(loose.c_str(), "rb");
        if (m->file_ && !std::fseek(m->file_, 0, SEEK_END)) m->size_ = uint64_t(std::ftell(m->file_));
    } else if ((m->bytes_ = files.read(path))) {
        m->size_ = m->bytes_->size();
    }
    if (!m->size_) {
        error = "can't read it";
        return nullptr;
    }
    char why[256] = "";
    m->movie_ = api.open(read, m.get(), m->size_, loop, &m->info_, why, sizeof why);
    if (!m->movie_) {
        error = why;
        return nullptr;
    }
    return m;
}

Movie::~Movie() {
    if (movie_) api_.close(movie_);
    if (file_) std::fclose(file_);
}

ImageRef Movie::frame(double seconds, bool &ended) {
    int changed = 0, done = 0;
    const uint8_t *pixels = api_.frame(movie_, seconds, &changed, &done);
    ended = done != 0;
    if (pixels && (changed || !current_)) {
        auto image = std::make_shared<Image>();
        image->w = info_.width;
        image->h = info_.height;
        image->rgba.assign(pixels, pixels + size_t(info_.width) * size_t(info_.height) * 4);
        current_ = image;
    }
    return current_;
}

// The movie's own rate and channels to 44.1 kHz stereo, by linear steps between its samples.
int Movie::sound(float *out, int frames) {
    int channels = info_.audioChannels;
    if (info_.audioRate == 44100 && channels == 2) return int(api_.audio(movie_, out, size_t(frames)));
    double step = double(info_.audioRate) / 44100;
    size_t need = size_t(std::ceil(resampleAt_ + frames * step)) + 2;
    size_t have = in_.size() / size_t(channels);
    if (have < need) {
        in_.resize(need * size_t(channels));
        have += api_.audio(movie_, in_.data() + have * size_t(channels), need - have);
        in_.resize(have * size_t(channels));
    }
    int written = 0;
    for (; written < frames; written++) {
        auto at = size_t(resampleAt_);
        if (at + 1 >= have) break;
        float k = float(resampleAt_ - double(at));
        for (int c = 0; c < 2; c++) {
            int from = std::min(c, channels - 1);
            float a = in_[at * size_t(channels) + size_t(from)], b = in_[(at + 1) * size_t(channels) + size_t(from)];
            out[written * 2 + c] = a + (b - a) * k;
        }
        resampleAt_ += step;
    }
    auto used = size_t(resampleAt_);
    in_.erase(in_.begin(), in_.begin() + std::ptrdiff_t(std::min(used, have) * size_t(channels)));
    resampleAt_ -= double(std::min(used, have));
    return written;
}

}  // namespace neko

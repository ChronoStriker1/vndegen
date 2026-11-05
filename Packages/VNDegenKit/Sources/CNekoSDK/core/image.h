// Pictures: 24-bit BMP with a separate .alp of 8-bit alpha, or PNG. Always RGBA with straight alpha.
#pragma once
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <list>
#include <mutex>
#include <thread>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "archive.h"

namespace neko {

struct Image {
    int w = 0, h = 0;
    bool opaque = true;
    std::vector<uint8_t> rgba;
    size_t bytes() const { return rgba.size(); }
};
using ImageRef = std::shared_ptr<const Image>;

// A BMP from anywhere, such as a save's thumbnail. Null when it can't be read.
ImageRef decodeBmpBytes(const std::vector<uint8_t> &data);
// A 24-bit BMP of an RGBA frame, shrunk to w by h, as the PC game writes a save's thumbnail.
std::vector<uint8_t> encodeThumbnail(const uint8_t *rgba, int frameW, int frameH, int w, int h);

// Decoded pictures, most recently used kept. Scripts load the same backgrounds and bars over and over.
class Images {
public:
    explicit Images(const Files &files) : files_(files) {}
    // Null when the file is missing or isn't a picture this runtime reads.
    ImageRef get(const std::string &path);
    // Decodes these ahead of need on a thread of its own, so `get` finds them ready. Scripts load pictures one node at a time
    // on the main thread, and a 1080p picture takes tens of ms to unpack.
    // Each is a path and a blur radius, 0 for none.
    void prefetch(const std::vector<std::pair<std::string, int>> &wanted);
    ~Images();
    // The same picture, blurred by this radius in pixels. Kept like any other, since scripts hold a blur for a whole scene.
    ImageRef blurred(const std::string &path, int radius);

private:
    void work();
    ImageRef decode(const std::string &path) const;
    ImageRef remember(const std::string &key, ImageRef image);
    const Files &files_;
    std::list<std::pair<std::string, ImageRef>> recent_;
    std::unordered_map<std::string, std::list<std::pair<std::string, ImageRef>>::iterator> byPath_;
    size_t held_ = 0;
    // Guards the cache and the queue. `get` waits on `done_` for a picture the thread is already decoding.
    std::mutex mu_;
    std::condition_variable wake_, done_;
    std::deque<std::pair<std::string, int>> queue_;
    std::string decoding_;
    bool stopping_ = false;
    std::thread worker_;
};

}  // namespace neko

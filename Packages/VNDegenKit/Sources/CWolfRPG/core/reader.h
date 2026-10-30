// Little-endian reader over a byte buffer, for Wolf RPG Editor's data files.
#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace wolf {

struct FormatError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Reader {
public:
    explicit Reader(std::span<const uint8_t> data) : data_(data) {}

    size_t pos() const { return pos_; }
    size_t size() const { return data_.size(); }
    bool atEnd() const { return pos_ >= data_.size(); }
    void seek(size_t pos) { pos_ = pos; }
    void skip(size_t n) { need(n); pos_ += n; }

    uint8_t u8() { need(1); return data_[pos_++]; }
    uint16_t u16() { need(2); uint16_t v = data_[pos_] | data_[pos_ + 1] << 8; pos_ += 2; return v; }
    uint32_t u32() {
        need(4);
        uint32_t v = uint32_t(data_[pos_]) | uint32_t(data_[pos_ + 1]) << 8 | uint32_t(data_[pos_ + 2]) << 16 |
                     uint32_t(data_[pos_ + 3]) << 24;
        pos_ += 4;
        return v;
    }
    int32_t s32() { return int32_t(u32()); }

    std::span<const uint8_t> bytes(size_t n) {
        need(n);
        auto s = data_.subspan(pos_, n);
        pos_ += n;
        return s;
    }

    // A length-prefixed string. The length counts the terminating NUL. Wolf reads strings as C strings, so the text
    // ends at the first NUL; editors sometimes leave old bytes after it. The bytes stay in the game's encoding, which
    // the caller converts.
    std::string rawString() {
        uint32_t len = u32();
        auto b = bytes(len);
        size_t n = 0;
        while (n < len && b[n] != 0) n++;
        return std::string(reinterpret_cast<const char *>(b.data()), n);
    }

    void expect(std::initializer_list<uint8_t> magic, const char *what) {
        for (uint8_t m : magic) {
            if (u8() != m) throw FormatError(std::string("bad magic in ") + what + " at " + std::to_string(pos_ - 1));
        }
    }

private:
    void need(size_t n) const {
        if (pos_ + n > data_.size()) throw FormatError("read past end at " + std::to_string(pos_));
    }

    std::span<const uint8_t> data_;
    size_t pos_ = 0;
};

}  // namespace wolf

#include "savefile.h"

#include <cassert>
#include <cstdio>
#include <string>

using wolf::Encoding;

static void roundTrip(const std::string &source, const std::string &bytes, Encoding enc) {
    auto written = wolf::pictureTextBytes(source, enc);
    assert(written && *written == bytes);
    auto read = wolf::pictureText(bytes, enc);
    assert(read && *read == source);
}

int main() {
    roundTrip("\\f[15]<R>1000\n\\ay[32]100\n\\f[13]\\ay[62]0 / 1000",
              std::string("\x01\x05\x03\x0e\x1d\x01\x01\x02\x15" "1000\n\x01\x05\x1e\x5c\x36\x01" "100\n"
                          "\x01\x05\x03\x0e\x1b\x01\x01\x05\x1e\x5d\x14\x01" "0 / 1000"),
              Encoding::UTF8);
    roundTrip("\\f[16]Mon", std::string("\x01\x05\x03\x0e\x1e\x01" "Mon"), Encoding::UTF8);
    roundTrip("\\E\\f[48]0", std::string("\x01\x02\x05\x01\x05\x03\x0e\x3e\x01" "0"), Encoding::ShiftJIS);
    roundTrip("\\f[55]1", std::string("\x01\x05\x03\x0e\x45\x01" "1"), Encoding::ShiftJIS);
    assert(!wolf::pictureTextBytes("\\c[2]red", Encoding::UTF8));
    assert(!wolf::pictureTextBytes("<C>centred", Encoding::UTF8));
    assert(!wolf::pictureText(std::string("\x01\x02\x07" "x"), Encoding::UTF8));
    std::puts("picture text ok");
}

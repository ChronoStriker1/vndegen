#include "dxa.h"

#include "data.h"
#include "reader.h"

#include <algorithm>
#include <array>
#include <span>
#include <string_view>

namespace wolf {

namespace {

// Wolf RPG Editor 2's key strings: up to 2.02a, 2.10, and 2.20 to 2.24.
constexpr std::array<std::string_view, 3> keyStrings = {
    "\x0F\x53\xE1\x3E\x04\x37\x12\x17\x60\x0F\x53\xE1",
    "\x4C\xD9\x2A\xB7\x28\x9B\xAC\x07\x3E\x77\xEC\x4C",
    "\x38\x50\x40\x28\x72\x4F\x21\x70\x3B\x73\x35\x38",
};

uint8_t swapHalves(uint8_t b) { return uint8_t(b >> 4 | b << 4); }
uint8_t rotateRight(uint8_t b, int n) { return uint8_t(b >> n | b << (8 - n)); }

std::vector<uint8_t> makeKey(std::string_view s) {
    std::vector<uint8_t> k(12);
    for (size_t i = 0; i < k.size(); i++) k[i] = uint8_t(s[i % s.size()]);
    k[0] = ~k[0];
    k[1] = swapHalves(k[1]);
    k[2] ^= 0x8A;
    k[3] = ~swapHalves(k[3]);
    k[4] = ~k[4];
    k[5] ^= 0xAC;
    k[6] = ~k[6];
    k[7] = ~rotateRight(k[7], 3);
    k[8] = rotateRight(k[8], 5);
    k[9] ^= 0x7F;
    k[10] = swapHalves(k[10]) ^ 0xD6;
    k[11] ^= 0xCC;
    return k;
}

std::string upperAscii(std::string s) {
    for (auto &c : s)
        if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
    return s;
}

std::string pathKey(const std::string &path) {
    std::string s = upperAscii(path);
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

bool readAt(FILE *f, uint64_t pos, uint8_t *buf, size_t n) {
    if (fseeko(f, off_t(pos), SEEK_SET) != 0) return false;
    return fread(buf, 1, n, f) == n;
}

// The decrypted tables, read with fields of the archive's address size (4 or 8 bytes).
struct Tables {
    std::span<const uint8_t> data;
    size_t width;

    uint64_t field(uint64_t pos, size_t n) const {
        if (pos > data.size() || n > data.size() - pos)
            throw FormatError("table read past end at " + std::to_string(pos));
        uint64_t v = 0;
        for (size_t i = 0; i < n; i++) v |= uint64_t(data[pos + i]) << (8 * i);
        return v;
    }
    uint64_t addr(uint64_t pos) const { return field(pos, width); }
    // All one bits in an address-sized field, which the archive uses for "none".
    uint64_t none() const { return width == 8 ? UINT64_MAX : UINT32_MAX; }
};

// Name, attributes, three 8-byte timestamps, address, size, and from version 2 on the packed size.
size_t fileEntrySize(int version, size_t width) {
    size_t n = width * 2 + 24 + width * 2;
    return version >= 2 ? n + width : n;
}

}  // namespace

std::unique_ptr<Archive> Archive::open(const std::string &path, std::string *error) {
    auto fail = [&](std::string why) -> std::unique_ptr<Archive> {
        if (error) *error = std::move(why);
        return nullptr;
    };
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return fail("cannot open file");
    std::unique_ptr<Archive> a(new Archive);
    a->file_ = f;
    if (fseeko(f, 0, SEEK_END) != 0) return fail("cannot seek");
    off_t size = ftello(f);
    if (size < 0) return fail("cannot get file size");
    a->fileSize_ = uint64_t(size);

    std::string why = "not a DxLib archive, or encrypted with an unknown key";
    for (auto s : keyStrings) {
        a->entries_.clear();
        std::string err;
        if (a->load(makeKey(s), &err)) return a;
        // A key that decrypts the magic is the right one, so its error is the real reason.
        if (!err.empty()) why = err;
    }
    return fail(why);
}

Archive::~Archive() {
    if (file_) fclose(file_);
}

void Archive::xorAt(uint8_t *data, size_t size, uint64_t position) const {
    size_t k = size_t(position % key_.size());
    for (size_t i = 0; i < size; i++) {
        data[i] ^= key_[k];
        if (++k == key_.size()) k = 0;
    }
}

// Leaves `error` empty when the key does not decrypt the magic, so the caller tries the next key.
bool Archive::load(const std::vector<uint8_t> &key, std::string *error) {
    key_ = key;
    uint8_t h[48] = {};
    size_t got = size_t(std::min<uint64_t>(fileSize_, sizeof(h)));
    if (got < 4 || !readAt(file_, 0, h, got)) return false;
    xorAt(h, got, 0);
    if (h[0] != 'D' || h[1] != 'X') return false;

    int version = h[2] | h[3] << 8;
    if (version < 1 || version > 6) {
        *error = "unsupported DxLib archive version " + std::to_string(version);
        return false;
    }
    size_t width = version >= 6 ? 8 : 4;
    size_t headerSize = 8 + width * 4 + (version >= 6 ? 8 : version >= 4 ? 4 : 0);
    if (got < headerSize) {
        *error = "truncated header";
        return false;
    }
    Tables hdr{std::span<const uint8_t>(h, headerSize), width};
    uint64_t tableSize = hdr.field(4, 4);
    uint64_t dataStart = hdr.addr(8);
    uint64_t tableStart = hdr.addr(8 + width);
    uint64_t fileTable = hdr.addr(8 + width * 2);
    uint64_t dirTable = hdr.addr(8 + width * 3);
    if (tableStart > fileSize_ || tableSize > fileSize_ - tableStart || dataStart > fileSize_ ||
        fileTable > dirTable || dirTable > tableSize) {
        *error = "header points outside the file";
        return false;
    }
    std::vector<uint8_t> head(tableSize);
    if (!readAt(file_, tableStart, head.data(), head.size())) {
        *error = "cannot read tables";
        return false;
    }
    xorAt(head.data(), head.size(), version >= 5 ? 0 : tableStart);

    version_ = version;
    dataStart_ = dataStart;
    try {
        walk(head, fileTable, dirTable, 0, "");
    } catch (const FormatError &e) {
        *error = e.what();
        return false;
    }
    return true;
}

void Archive::walk(const std::vector<uint8_t> &head, uint64_t fileTable, uint64_t dirTable, uint64_t dir,
                   const std::string &prefix) {
    size_t width = version_ >= 6 ? 8 : 4;
    Tables t{head, width};
    size_t entrySize = fileEntrySize(version_, width);
    uint64_t names = fileTable;  // the name table ends where the file table starts
    uint64_t files = head.size() - fileTable;
    uint64_t dirs = head.size() - dirTable;

    if (dir > dirs || width * 4 > dirs - dir) throw FormatError("directory record outside the table");
    uint64_t count = t.addr(dirTable + dir + width * 2);
    uint64_t first = t.addr(dirTable + dir + width * 3);
    if (first > files || count > (files - first) / entrySize) throw FormatError("directory entries outside the table");

    for (uint64_t i = 0; i < count; i++) {
        uint64_t e = first + i * entrySize;
        uint64_t p = fileTable + e;
        uint64_t nameAt = t.addr(p);
        uint64_t attributes = t.addr(p + width);
        uint64_t address = t.addr(p + width * 2 + 24);
        uint64_t size = t.addr(p + width * 3 + 24);
        uint64_t packed = version_ >= 2 ? t.addr(p + width * 4 + 24) : t.none();

        // A name entry is its length in 4-byte units, a checksum, the upper-case name, then the name as written.
        // Both forms fill the same number of units.
        if (nameAt > names || 4 > names - nameAt) throw FormatError("name outside the table");
        uint64_t units = t.field(nameAt, 2);
        if (units * 8 > names - nameAt - 4) throw FormatError("name outside the table");
        auto raw = t.data.subspan(nameAt + 4 + units * 4, units * 4);
        std::string name(raw.begin(), std::find(raw.begin(), raw.end(), uint8_t(0)));
        std::string path = prefix + toUtf8(name, Encoding::ShiftJIS);

        if (attributes & 0x10) {
            // Requiring the record to point back at this entry and at this directory means each record is reached
            // once, so a malformed archive cannot loop. The root's record is never a child.
            if (address == 0 || address > dirs || width * 4 > dirs - address ||
                t.addr(dirTable + address) != e || t.addr(dirTable + address + width) != dir)
                throw FormatError("bad directory record for " + path);
            // Games nest a few folders deep; a deeper chain is damage and would only use up the stack.
            if (std::count(path.begin(), path.end(), '/') >= 64) throw FormatError("folders nest too deep at " + path);
            walk(head, fileTable, dirTable, address, path + "/");
            continue;
        }
        Entry entry;
        entry.name = path;
        entry.offset = dataStart_ + address;
        entry.size = size;
        entry.packedSize = packed == t.none() ? UINT64_MAX : packed;
        uint64_t stored = entry.packedSize == UINT64_MAX ? size : packed;
        if (address > fileSize_ || dataStart_ > fileSize_ - address || stored > fileSize_ - entry.offset)
            throw FormatError("data outside the file for " + path);
        entries_.emplace(pathKey(path), std::move(entry));
    }
}

bool Archive::contains(const std::string &path) const { return entries_.contains(pathKey(path)); }

std::optional<std::vector<uint8_t>> Archive::read(const std::string &path) const {
    auto it = entries_.find(pathKey(path));
    if (it == entries_.end()) return std::nullopt;
    const Entry &e = it->second;
    bool packed = e.packedSize != UINT64_MAX;
    std::vector<uint8_t> stored(packed ? e.packedSize : e.size);
    if (!readAt(file_, e.offset, stored.data(), stored.size())) return std::nullopt;
    // Versions 5 and 6 key each file's bytes from its unpacked size rather than its place in the archive.
    xorAt(stored.data(), stored.size(), version_ >= 5 ? e.size : e.offset);
    if (!packed) return stored;
    auto out = dxlibDecode(stored.data(), stored.size());
    if (out.size() != e.size) return std::nullopt;
    return out;
}

std::vector<std::string> Archive::paths() const {
    std::vector<std::string> out;
    out.reserve(entries_.size());
    for (auto &[key, e] : entries_) out.push_back(e.name);
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<uint8_t> dxlibDecode(const uint8_t *src, size_t srcSize) {
    if (srcSize < 9) return {};
    uint64_t size = uint32_t(src[0] | src[1] << 8 | src[2] << 16 | uint32_t(src[3]) << 24);
    size_t end = uint32_t(src[4] | src[5] << 8 | src[6] << 16 | uint32_t(src[7]) << 24);
    uint8_t escape = src[8];
    if (end < 9 || end > srcSize) return {};
    // A back-reference copies at most 8195 bytes from 4 bytes of input, so a larger size means a corrupt header.
    if (size > uint64_t(end - 9) * 2049) return {};

    std::vector<uint8_t> out;
    out.reserve(size);
    size_t i = 9;
    while (i < end) {
        uint8_t b = src[i++];
        if (b != escape) {
            out.push_back(b);
        } else {
            if (i >= end) return {};
            uint8_t c = src[i++];
            if (c == escape) {
                out.push_back(escape);
            } else {
                if (c > escape) c--;
                size_t length = c >> 3;
                if (c & 4) {
                    if (i >= end) return {};
                    length += size_t(src[i++]) << 5;
                }
                length += 4;
                size_t bytes = (c & 3) + 1;
                if (bytes > 3 || bytes > end - i) return {};
                size_t distance = 0;
                for (size_t k = 0; k < bytes; k++) distance |= size_t(src[i++]) << (8 * k);
                distance++;
                if (distance > out.size() || length > size - out.size()) return {};
                size_t from = out.size() - distance;
                for (size_t k = 0; k < length; k++) out.push_back(out[from + k]);
            }
        }
        if (out.size() > size) return {};
    }
    if (out.size() != size) return {};
    return out;
}

}  // namespace wolf

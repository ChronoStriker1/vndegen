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

// Wolf RPG Editor 3's archive key strings, by the crypt version the editor writes into the header (3.00, 3.14, 3.31 and
// 3.50), then Wolf RPG Editor 2.225's, whose crypt version is 0. From UberWolf's table of Wolf RPG Editor archive keys.
// Only the 3.14 key has been checked against a game's archives.
struct Wolf3Key {
    uint16_t cryptVersion;
    std::string_view key;
};
constexpr std::array<Wolf3Key, 5> wolf3Keys = {{
    {0x12C, "\x0F\x53\xE1\x3E\x8E\xB5\x41\x91\x52\x16\x55\xAE\x34\xC9\x8F\x79\x59\x2F\x59\x6B\x95\x19\x9B\x1B"
            "\x35\x9A\x2F\xDE\xC9\x7C\x12\x96\xC3\x14\xB5\x0F\x53\xE1\x3E\x8E"},
    {0x13A, "\x31\xF9\x01\x36\xA3\xE3\x8D\x3C\x7B\xC3\x7D\x25\xAD\x63\x28\x19\x1B\xF7\x8E\x6C\xC4\xE5\xE2\x76"
            "\x82\xEA\x4F\xED\x61\xDA\xE0\x44\x5B\xB6\x46\x3B\x06\xD5\xCE\xB6\x78\x58\xD0\x7C\x82"},
    {0x14B, "\xCA\x08\x4C\x5D\x17\x0D\xDA\xA1\xD7\x27\xC8\x41\x54\x38\x82\x32\x54\xB7\xF9\x46\x8E\x13\x6B\xCA"
            "\xD0\x5C\x95\x95\xE2\xDC\x03\x53\x60\x9B\x4A\x38\x17\xF3\x69\x59\xA4\xC7\x9A\x43\x63\xE6\x54\xAF"
            "\xDB\xBB\x43\x58"},
    {0x15E, "\xD2\x84\xCE\x28\xCE\x88\x82\xE4\x2A\x18\x2E\x4C\x06\xB4\xEA\x84\x06\xB8\xC6\x88\x5A\xA0\x9E\x7C"
            "\x56\x40\xBA\x34\x52\xCC\xC6\x7C\x2E\x14\x12\x68\xFE\x5C\x76\x94\x86\x78\x8E\x4C\xBE\x88\x66\x9C"
            "\x1E\xE0\x8E\x6C"},
    {0, "WLFRPrO!p(;s5((8P@((UFWlu$#5(="},
}};

// DxLib's key string for a version 8 archive made without one.
constexpr std::string_view defaultKey8 = "DXBDXARC";

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

// A version 8 key: the CRC-32 of the string's bytes at even positions, then the low three bytes of the CRC-32 of those
// at odd positions. A string shorter than 4 bytes is lengthened with the default key first.
std::vector<uint8_t> makeKey8(std::string s) {
    if (s.size() < 4) s += defaultKey8;
    uint32_t crc[2] = {UINT32_MAX, UINT32_MAX};
    for (size_t i = 0; i < s.size(); i++) {
        uint32_t &c = crc[i % 2];
        c ^= uint8_t(s[i]);
        for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1)));
    }
    uint32_t even = ~crc[0], odd = ~crc[1];
    return {uint8_t(even), uint8_t(even >> 8), uint8_t(even >> 16), uint8_t(even >> 24),
            uint8_t(odd), uint8_t(odd >> 8), uint8_t(odd >> 16)};
}

// XORs a stream whose first byte is at `position`. An empty key leaves the bytes as they are.
void xorStream(uint8_t *data, size_t size, const std::vector<uint8_t> &key, uint64_t position) {
    if (key.empty()) return;
    size_t k = size_t(position % key.size());
    for (size_t i = 0; i < size; i++) {
        data[i] ^= key[k];
        if (++k == key.size()) k = 0;
    }
}

// DxLib's Huffman decoding. The header is read most significant bit first: the decoded size and the coded size, each
// as a 6-bit width less one and then that many bits, and 256 byte weights, each as a delta from the previous one. The
// coded data starts at the next byte and is read least significant bit first. `expected` is the size the caller
// needs, or UINT64_MAX for any.
std::optional<std::vector<uint8_t>> huffmanDecode(std::span<const uint8_t> src, uint64_t expected) {
    uint64_t pos = 0;
    bool past = false;
    auto bits = [&](unsigned n) {
        uint64_t v = 0;
        for (unsigned i = 0; i < n && !past; i++, pos++) {
            if (pos / 8 >= src.size()) past = true;
            else v = v << 1 | (src[pos / 8] >> (7 - pos % 8) & 1);
        }
        return v;
    };
    uint64_t size = bits(unsigned(bits(6)) + 1);
    bits(unsigned(bits(6)) + 1);  // the coded size, which decoding does not need

    // Weights fit in 16 bits in a good stream; a wider type keeps a damaged one from overflowing.
    std::array<int64_t, 511> weight{};
    int64_t w = 0;
    for (int i = 0; i < 256; i++) {
        unsigned c = unsigned(bits(3));
        bool minus = bits(1);
        int64_t magnitude = int64_t(bits(2 * (c + 1)));
        w = i == 0 ? magnitude : minus ? w - magnitude : w + magnitude;
        weight[i] = w;
    }
    if (past) return std::nullopt;
    size_t start = size_t((pos + 7) / 8);
    // Every byte takes at least one bit, which bounds the size a damaged header can claim.
    if ((expected != UINT64_MAX && size != expected) || size > uint64_t(src.size() - start) * 8) return std::nullopt;

    // The tree must come out exactly as the encoder built it: each inner node joins the two parentless nodes of lowest
    // weight, ties going to the lower node number, the first taken as its 0 child. All 256 byte values take part, even
    // those of weight 0.
    std::array<std::array<uint16_t, 2>, 255> children;
    std::vector<uint16_t> open(256);
    for (uint16_t i = 0; i < 256; i++) open[i] = i;
    auto takeLowest = [&] {
        size_t best = 0;
        for (size_t i = 1; i < open.size(); i++) {
            uint16_t a = open[i], b = open[best];
            if (weight[a] < weight[b] || (weight[a] == weight[b] && a < b)) best = i;
        }
        uint16_t node = open[best];
        open.erase(open.begin() + ptrdiff_t(best));
        return node;
    };
    for (uint16_t node = 256; node < 511; node++) {
        uint16_t zero = takeLowest(), one = takeLowest();
        weight[node] = weight[zero] + weight[one];
        children[node - 256] = {zero, one};
        open.push_back(node);
    }

    std::vector<uint8_t> out(size);
    uint64_t bit = uint64_t(start) * 8, end = uint64_t(src.size()) * 8;
    for (auto &o : out) {
        unsigned node = 510;
        while (node >= 256) {
            if (bit >= end) return std::nullopt;
            node = children[node - 256][src[bit / 8] >> (bit % 8) & 1];
            bit++;
        }
        o = uint8_t(node);
    }
    return out;
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

// Name, attributes, three 8-byte timestamps, address, size, from version 2 on the packed size, and from version 8 on
// the Huffman-coded size.
size_t fileEntrySize(int version, size_t width) {
    size_t n = width * 2 + 24 + width * 2;
    return version >= 8 ? n + width * 2 : version >= 2 ? n + width : n;
}

}  // namespace

struct Archive::Header8 {
    uint64_t tableSize, dataStart, tableStart, fileTable, dirTable;
    uint32_t codePage, flags;  // flags bit 0: nothing is encrypted; bit 1: the tables are not compressed
    uint8_t huffmanKiB;
};

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

    // A version 8 header is never encrypted, so its magic and version show in the clear.
    uint8_t h[64] = {};
    size_t got = size_t(std::min<uint64_t>(a->fileSize_, sizeof(h)));
    if (got >= 4 && readAt(f, 0, h, got) && h[0] == 'D' && h[1] == 'X' && h[2] == 8 && h[3] == 0) {
        if (got < sizeof(h)) return fail("truncated header");
        Tables t{std::span<const uint8_t>(h), 8};
        Header8 hd{t.field(4, 4), t.addr(0x08), t.addr(0x10), t.addr(0x18), t.addr(0x20),
                   uint32_t(t.field(0x28, 4)), uint32_t(t.field(0x2C, 4)), h[0x30]};
        uint64_t end = a->fileSize_;
        if (hd.tableStart > end || hd.dataStart > end || ((hd.flags & 2) && hd.tableSize > end - hd.tableStart) ||
            hd.fileTable > hd.dirTable || hd.dirTable > hd.tableSize)
            return fail("header points outside the file");
        // Wolf RPG Editor 3 keeps its crypt version in the upper half of the flags, which DxLib does not use.
        a->cryptVersion_ = uint16_t(hd.flags >> 16);
        std::string err;
        if (hd.flags & 1) return a->load8(hd, "", &err) ? std::move(a) : fail(err);
        // The header names the editor's crypt version, so its key goes first; the rest are tried in case a game
        // changed the field.
        std::vector<std::string_view> tries;
        for (auto &k : wolf3Keys)
            if (k.cryptVersion == a->cryptVersion_) tries.push_back(k.key);
        for (auto &k : wolf3Keys)
            if (k.cryptVersion != a->cryptVersion_) tries.push_back(k.key);
        tries.push_back(defaultKey8);
        for (auto s : tries) {
            a->entries_.clear();
            if (a->load8(hd, s, &err)) return a;
        }
        char crypt[8];
        std::snprintf(crypt, sizeof(crypt), "0x%X", unsigned(a->cryptVersion_));
        return fail(std::string("version 8 archive with crypt version ") + crypt +
                    ", encrypted with an unknown key or damaged");
    }

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

// Leaves `error` empty when the key does not decrypt the magic, so the caller tries the next key.
bool Archive::load(const std::vector<uint8_t> &key, std::string *error) {
    key_ = key;
    uint8_t h[48] = {};
    size_t got = size_t(std::min<uint64_t>(fileSize_, sizeof(h)));
    if (got < 4 || !readAt(file_, 0, h, got)) return false;
    xorStream(h, got, key_, 0);
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
    xorStream(head.data(), head.size(), key_, version >= 5 ? 0 : tableStart);

    version_ = version;
    dataStart_ = dataStart;
    try {
        walk(head, fileTable, dirTable, 0, "", "");
    } catch (const FormatError &e) {
        *error = e.what();
        return false;
    }
    return true;
}

// A wrong key shows as tables that do not decode or do not hold together, which leaves the reason in `error`.
bool Archive::load8(const Header8 &h, std::string_view keyString, std::string *error) {
    keyString_ = std::string(keyString.substr(0, 63));
    key_ = h.flags & 1 ? std::vector<uint8_t>() : makeKey8(keyString_);
    // Compressed tables run to the end of the file; nothing records their stored length.
    bool compressed = !(h.flags & 2);
    std::vector<uint8_t> head(compressed ? fileSize_ - h.tableStart : h.tableSize);
    if (!readAt(file_, h.tableStart, head.data(), head.size())) {
        *error = "cannot read tables";
        return false;
    }
    xorStream(head.data(), head.size(), key_, 0);
    if (compressed) {
        auto coded = huffmanDecode(head, UINT64_MAX);
        // Checking the LZ header's size first keeps a wrong key from asking for a large buffer.
        if (!coded || coded->size() < 4 || Tables{*coded, 4}.field(0, 4) != h.tableSize ||
            (head = dxlibDecode(coded->data(), coded->size())).size() != h.tableSize) {
            *error = "cannot decode tables";
            return false;
        }
    }

    version_ = 8;
    dataStart_ = h.dataStart;
    huffmanKiB_ = h.huffmanKiB;
    // Version 8 names use the header's code page. Wolf RPG Editor writes 932, Shift-JIS; 65001 is UTF-8.
    utf8Names_ = h.codePage == 65001;
    try {
        walk(head, h.fileTable, h.dirTable, 0, "", "");
    } catch (const FormatError &e) {
        *error = e.what();
        return false;
    }
    return true;
}

// `dirKey` is the upper-case names of the folders holding this one, innermost first, as a version 8 file key needs.
void Archive::walk(const std::vector<uint8_t> &head, uint64_t fileTable, uint64_t dirTable, uint64_t dir,
                   const std::string &prefix, const std::string &dirKey) {
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
        uint64_t huffman = version_ >= 8 ? t.addr(p + width * 5 + 24) : t.none();

        // A name entry is its length in 4-byte units, a checksum, the upper-case name, then the name as written.
        // Both forms fill the same number of units.
        if (nameAt > names || 4 > names - nameAt) throw FormatError("name outside the table");
        uint64_t units = t.field(nameAt, 2);
        if (units * 8 > names - nameAt - 4) throw FormatError("name outside the table");
        auto rawUpper = t.data.subspan(nameAt + 4, units * 4);
        auto raw = t.data.subspan(nameAt + 4 + units * 4, units * 4);
        std::string upper(rawUpper.begin(), std::find(rawUpper.begin(), rawUpper.end(), uint8_t(0)));
        std::string name(raw.begin(), std::find(raw.begin(), raw.end(), uint8_t(0)));
        std::string path = prefix + toUtf8(name, utf8Names_ ? Encoding::UTF8 : Encoding::ShiftJIS);

        if (attributes & 0x10) {
            // Requiring the record to point back at this entry and at this directory means each record is reached
            // once, so a malformed archive cannot loop. The root's record is never a child.
            if (address == 0 || address > dirs || width * 4 > dirs - address ||
                t.addr(dirTable + address) != e || t.addr(dirTable + address + width) != dir)
                throw FormatError("bad directory record for " + path);
            // Games nest a few folders deep; a deeper chain is damage and would only use up the stack.
            if (std::count(path.begin(), path.end(), '/') >= 64) throw FormatError("folders nest too deep at " + path);
            walk(head, fileTable, dirTable, address, path + "/", upper + dirKey);
            continue;
        }
        Entry entry;
        entry.name = path;
        entry.offset = dataStart_ + address;
        entry.size = size;
        entry.packedSize = packed == t.none() ? UINT64_MAX : packed;
        entry.huffmanSize = huffman == t.none() ? UINT64_MAX : huffman;
        uint64_t length = entry.packedSize == UINT64_MAX ? size : packed;
        entry.stored = length;
        if (entry.huffmanSize != UINT64_MAX) {
            // Unless the Huffman stage covers the whole file, the middle of the data follows the coded bytes as is.
            uint64_t ends = uint64_t(huffmanKiB_) * 2048;
            bool whole = huffmanKiB_ == 255 || length <= ends;
            entry.stored = whole ? huffman
                           : huffman > fileSize_ || length - ends > fileSize_ ? UINT64_MAX
                                                                             : huffman + (length - ends);
        }
        if (address > fileSize_ || dataStart_ > fileSize_ - address || entry.stored > fileSize_ - entry.offset)
            throw FormatError("data outside the file for " + path);
        // A version 8 file's key comes from the archive's key string, the file's upper-case name, and the names of
        // the folders holding it, innermost first.
        if (version_ >= 8 && !key_.empty()) entry.key = makeKey8(keyString_ + upper + dirKey);
        entries_.emplace(pathKey(path), std::move(entry));
    }
}

bool Archive::contains(const std::string &path) const { return entries_.contains(pathKey(path)); }

std::optional<std::vector<uint8_t>> Archive::read(const std::string &path) const {
    auto it = entries_.find(pathKey(path));
    if (it == entries_.end()) return std::nullopt;
    const Entry &e = it->second;
    std::vector<uint8_t> stored(e.stored);
    if (!readAt(file_, e.offset, stored.data(), stored.size())) return std::nullopt;
    if (version_ >= 8) return read8(e, std::move(stored));
    // Versions 5 and 6 key each file's bytes from its unpacked size rather than its place in the archive.
    xorStream(stored.data(), stored.size(), key_, version_ >= 5 ? e.size : e.offset);
    if (e.packedSize == UINT64_MAX) return stored;
    auto out = dxlibDecode(stored.data(), stored.size());
    if (out.size() != e.size) return std::nullopt;
    return out;
}

// The encoder applies LZ, then Huffman, then XOR, so they are undone in the reverse order.
std::optional<std::vector<uint8_t>> Archive::read8(const Entry &e, std::vector<uint8_t> stored) const {
    xorStream(stored.data(), stored.size(), e.key, e.size);
    bool packed = e.packedSize != UINT64_MAX;
    uint64_t length = packed ? e.packedSize : e.size;
    std::vector<uint8_t> data;
    if (e.huffmanSize == UINT64_MAX) {
        data = std::move(stored);
    } else if (uint64_t k = uint64_t(huffmanKiB_) * 1024; huffmanKiB_ == 255 || length <= 2 * k) {
        auto whole = huffmanDecode(stored, length);
        if (!whole) return std::nullopt;
        data = std::move(*whole);
    } else {
        // The coded bytes decode to the first k bytes and then the last k bytes; the middle follows them as is.
        auto ends = huffmanDecode(std::span<const uint8_t>(stored).first(e.huffmanSize), 2 * k);
        if (!ends) return std::nullopt;
        data.reserve(length);
        data.insert(data.end(), ends->begin(), ends->begin() + ptrdiff_t(k));
        data.insert(data.end(), stored.begin() + ptrdiff_t(e.huffmanSize), stored.end());
        data.insert(data.end(), ends->begin() + ptrdiff_t(k), ends->end());
    }
    if (!packed) return data;
    auto out = dxlibDecode(data.data(), data.size());
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

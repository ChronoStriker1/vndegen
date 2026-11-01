// KiriKiriCipherTest: checks cipher detection and decryption through the engine's archive code.
//   KiriKiriCipherTest                 writes synthetic archives to a temporary folder and checks them
//   KiriKiriCipherTest --bench <xp3>   times reading every entry of an archive with and without its cipher
#include "tjsCommHead.h"
#include "TVPStorage.h"
#include "XP3Archive.h"
#include "ciphers.h"
#include "vnkk.h"

#include <malloc/malloc.h>
#include <zlib.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <new>
#include <signal.h>
#include <string>
#include <unistd.h>
#include <vector>

using Bytes = std::vector<uint8_t>;
using Encrypt = std::function<void(uint32_t hash, Bytes &data)>;

static int failures;

// Every operator new in the process goes through these, so a test can see the most memory held at once.
static std::atomic<int64_t> liveBytes{0}, peakBytes{0};

static void track(int64_t delta)
{
    int64_t now = liveBytes += delta;
    int64_t peak = peakBytes.load();
    while (now > peak && !peakBytes.compare_exchange_weak(peak, now)) {
    }
}

void *operator new(size_t size)
{
    void *p = malloc(size ? size : 1);
    if (!p)
        throw std::bad_alloc();
    track((int64_t)malloc_size(p));
    return p;
}

void *operator new[](size_t size) { return operator new(size); }

void operator delete(void *p) noexcept
{
    if (p) {
        track(-(int64_t)malloc_size(p));
        free(p);
    }
}

void operator delete[](void *p) noexcept { operator delete(p); }
void operator delete(void *p, size_t) noexcept { operator delete(p); }
void operator delete[](void *p, size_t) noexcept { operator delete(p); }

// The most memory held at once while run() runs, beyond what was held before it.
template <typename Run> static int64_t peakDuring(Run run)
{
    int64_t before = liveBytes;
    peakBytes = before;
    run();
    return peakBytes - before;
}

static void check(bool ok, const std::string &what)
{
    printf("%s: %s\n", ok ? "pass" : "FAIL", what.c_str());
    if (!ok)
        failures++;
}

static uint32_t adler(const Bytes &data)
{
    return (uint32_t)adler32(adler32(0, nullptr, 0), data.data(), (uInt)data.size());
}

static void put(Bytes &out, uint64_t value, int size)
{
    for (int i = 0; i < size; i++)
        out.push_back((uint8_t)(value >> (8 * i)));
}

static void chunk(Bytes &out, const char *name, const Bytes &body)
{
    out.insert(out.end(), name, name + 4);
    put(out, body.size(), 8);
    out.insert(out.end(), body.begin(), body.end());
}

struct Entry {
    std::string name;
    Bytes plain;
    bool compressed;
    uint64_t unstored = 0; // listed in the index beyond what the segments hold
};

struct Segment {
    bool compressed;
    uint64_t start, orgSize, arcSize;
};

// The header, 11 bytes of signature and the index offset, comes before an archive's contents.
static const uint64_t headerSize = 19;

static Bytes fileChunk(const std::string &name, uint64_t orgSize, uint64_t arcSize,
                       const std::vector<Segment> &segments, uint32_t checksum)
{
    Bytes info, segm, adlr, body, out;
    put(info, 0, 4);
    put(info, orgSize, 8);
    put(info, arcSize, 8);
    put(info, name.size(), 2);
    for (char c : name)
        put(info, (uint8_t)c, 2);
    for (const Segment &segment : segments) {
        put(segm, segment.compressed ? 1 : 0, 4);
        put(segm, segment.start, 8);
        put(segm, segment.orgSize, 8);
        put(segm, segment.arcSize, 8);
    }
    put(adlr, checksum, 4);
    chunk(body, "info", info);
    chunk(body, "segm", segm);
    chunk(body, "adlr", adlr);
    chunk(out, "File", body);
    return out;
}

// Writes an XP3 archive of contents, at headerSize, and an uncompressed index.
static void writeXP3(const std::string &path, const Bytes &contents, const Bytes &index)
{
    Bytes file = {'X', 'P', '3', 0x0d, 0x0a, 0x20, 0x0a, 0x1a, 0x8b, 0x67, 0x01};
    put(file, headerSize + contents.size(), 8);
    file.insert(file.end(), contents.begin(), contents.end());
    file.push_back(0);
    put(file, index.size(), 8);
    file.insert(file.end(), index.begin(), index.end());
    FILE *out = fopen(path.c_str(), "wb");
    fwrite(file.data(), 1, file.size(), out);
    fclose(out);
}

// Writes an XP3 archive of entries, each encrypted with encrypt and stored under hash(plain).
static void writeArchive(const std::string &path, const std::vector<Entry> &entries,
                         const Encrypt &encrypt, const std::function<uint32_t(const Bytes &)> &hash)
{
    Bytes contents, index;
    for (const Entry &entry : entries) {
        uint32_t checksum = hash(entry.plain);
        Bytes stored = entry.plain;
        encrypt(checksum, stored);
        if (entry.compressed) {
            uLongf size = compressBound(stored.size());
            Bytes packed(size);
            compress(packed.data(), &size, stored.data(), stored.size());
            packed.resize(size);
            stored = packed;
        }
        uint64_t start = headerSize + contents.size();
        contents.insert(contents.end(), stored.begin(), stored.end());
        Bytes file = fileChunk(entry.name, entry.plain.size() + entry.unstored, stored.size(),
                               {{entry.compressed, start, entry.plain.size(), stored.size()}}, checksum);
        index.insert(index.end(), file.begin(), file.end());
    }
    writeXP3(path, contents, index);
}

static Bytes pattern(size_t size, uint32_t seed)
{
    Bytes data(size);
    for (size_t i = 0; i < size; i++) {
        seed = seed * 1103515245 + 12345;
        data[i] = (uint8_t)(seed >> 16);
    }
    return data;
}

static std::vector<Entry> sampleEntries()
{
    // Sizes cross every band boundary of bandedXorAdd, and a few entries are too small to cover them.
    std::vector<Entry> entries;
    const size_t sizes[] = {40, 130, 300, 600, 1000, 2500, 4096, 9000, 20000, 70000};
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++)
        entries.push_back({"file" + std::to_string(i) + ".bin", pattern(sizes[i], (uint32_t)i + 1), i % 2 == 0});
    entries.push_back({"empty.txt", {}, false});
    return entries;
}

static ttstr storageName(const std::string &path)
{
    return TVPNormalizeStorageName(ttstr(path));
}

static tTVPXP3Archive *openArchive(const std::string &path)
{
    return dynamic_cast<tTVPXP3Archive *>(TVPOpenArchive(storageName(path), true));
}

// Opens an archive whose index may be malformed. Null when the engine rejects it.
static tTVPXP3Archive *tryOpenArchive(const std::string &path)
{
    try {
        return openArchive(path);
    } catch (...) {
        return nullptr;
    }
}

// Reads an entry in odd-sized pieces, so decryption runs at offsets that start inside each band.
static Bytes readEntry(tTVPXP3Archive *archive, tjs_uint index)
{
    tTJSBinaryStream *stream = archive->CreateStreamByIndex(index);
    Bytes data(stream->GetSize());
    size_t done = 0;
    while (done < data.size()) {
        tjs_uint piece = (tjs_uint)std::min<size_t>(97, data.size() - done);
        stream->ReadBuffer(data.data() + done, piece);
        done += piece;
    }
    delete stream;
    return data;
}

static bool readsPlain(tTVPXP3Archive *archive, const std::vector<Entry> &entries)
{
    std::map<std::string, const Bytes *> expected;
    for (const Entry &entry : entries)
        expected[entry.name] = &entry.plain;
    if (archive->GetCount() != entries.size())
        return false;
    for (tjs_uint i = 0; i < archive->GetCount(); i++) {
        auto found = expected.find(archive->GetName(i).AsStdString());
        if (found == expected.end() || readEntry(archive, i) != *found->second)
            return false;
    }
    return true;
}

static std::string cipherName(tTVPXP3Archive *archive)
{
    return archive->Cipher ? archive->Cipher->name : "plain";
}

static void encryptXor(uint32_t hash, Bytes &data)
{
    for (uint8_t &b : data)
        b ^= (uint8_t)hash;
}

static void encryptRing(uint32_t hash, Bytes &data)
{
    uint32_t x = (hash ^ 0x2f91de55) & 0x7fffffff;
    x |= x << 31;
    uint8_t key[32];
    for (uint8_t &k : key) {
        k = x;
        x = x >> 8 | (x & ~1u) << 23;
    }
    for (size_t i = 0; i < data.size(); i++)
        data[i] ^= key[i & 31];
}

static void encryptBanded(uint32_t hash, Bytes &data)
{
    uint8_t k = (uint8_t)hash;
    for (size_t p = 0; p < data.size(); p++) {
        if (p < 0x7b)
            data[p] ^= (uint8_t)(k * 0x15);
        else if (p < 0xf6)
            data[p] -= (uint8_t)(k * 0xe0);
        else if (p < 0x171)
            data[p] ^= (uint8_t)(k * 0x2b);
        else
            data[p] -= (uint8_t)(k * 0xca);
    }
}

// An archive file held in memory: blobs at their offsets, and filler bytes everywhere else, so an
// entry can be far larger than the test's memory. It counts the bytes read from it.
class SyntheticArchive : public tTJSBinaryStream {
public:
    uint64_t Add(const Bytes &data)
    {
        blobs[size] = data;
        size += data.size();
        return size - data.size();
    }
    uint64_t Reserve(uint64_t count)
    {
        size += count;
        return size - count;
    }
    tjs_uint64 Seek(tjs_int64 offset, tjs_int whence) override
    {
        position = (whence == TJS_BS_SEEK_SET ? 0 : whence == TJS_BS_SEEK_CUR ? position : size) + offset;
        return position;
    }
    tjs_uint Read(void *buffer, tjs_uint count) override
    {
        uint64_t n = position < size ? std::min<uint64_t>(count, size - position) : 0;
        uint8_t *out = (uint8_t *)buffer;
        memset(out, 0x5a, n);
        for (const auto &[start, data] : blobs) {
            uint64_t from = std::max(start, position), to = std::min(start + data.size(), position + n);
            if (from < to)
                memcpy(out + (from - position), data.data() + (from - start), to - from);
        }
        position += n;
        bytesRead += n;
        return (tjs_uint)n;
    }
    tjs_uint Write(const void *, tjs_uint) override { return 0; }
    bool Flush() override { return true; }
    tjs_uint64 GetSize() override { return size; }
    uint64_t bytesRead = 0;

private:
    std::map<uint64_t, Bytes> blobs;
    uint64_t size = 0, position = 0;
};

static tTVPXP3Archive::tArchiveItem indexItem(const std::string &name, uint32_t hash, uint64_t start,
                                              uint64_t orgSize, uint64_t arcSize, bool compressed)
{
    tTVPXP3Archive::tArchiveItem item;
    item.Name = ttstr(name.c_str());
    item.FileHash = hash;
    item.OrgSize = orgSize;
    item.ArcSize = arcSize;
    item.Segments.push_back({start, 0, orgSize, arcSize, compressed});
    return item;
}

// Detection reads and decompresses a bounded number of bytes, however large an archive's entries
// say they are. The large entries here come after three small XOR-encrypted ones, which are enough.
static void checkDetectionWork()
{
    auto archiveWith = [](SyntheticArchive &archive, std::vector<tTVPXP3Archive::tArchiveItem> &items) {
        uint64_t smallBytes = 0;
        for (uint32_t i = 0; i < 3; i++) {
            Bytes plain = pattern(1000 + i * 100, i + 200), stored = plain;
            uint32_t hash = adler(plain);
            encryptXor(hash, stored);
            items.push_back(indexItem("small" + std::to_string(i), hash, archive.Add(stored), plain.size(), plain.size(), false));
            smallBytes += plain.size();
        }
        return smallBytes;
    };
    auto detect = [](SyntheticArchive &archive, const std::vector<tTVPXP3Archive::tArchiveItem> &items) {
        auto start = std::chrono::steady_clock::now();
        const VNKKCipher *cipher = VNKKDetectCipher(ttstr("file://./synthetic.xp3"), items, &archive);
        printf("detection read %llu bytes in %.0f ms\n", (unsigned long long)archive.bytesRead,
               std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
        return std::string(cipher ? cipher->name : "plain");
    };

    {
        SyntheticArchive archive;
        std::vector<tTVPXP3Archive::tArchiveItem> items;
        uint64_t smallBytes = archiveWith(archive, items);
        for (uint32_t i = 0; i < 2; i++)
            items.push_back(indexItem("stored" + std::to_string(i), i + 1, archive.Reserve(256 << 20), 256 << 20, 256 << 20, false));
        check(detect(archive, items) == "xorByChecksum", "an archive with 256 MiB entries is detected from its small ones");
        check(archive.bytesRead == smallBytes, "detection reads none of the 256 MiB entries (" +
                                                   std::to_string(archive.bytesRead) + " bytes read)");
    }
    {
        SyntheticArchive archive;
        std::vector<tTVPXP3Archive::tArchiveItem> items;
        uint64_t smallBytes = archiveWith(archive, items);
        Bytes zeros(64 << 20);
        uLongf packedSize = compressBound(zeros.size());
        Bytes packed(packedSize);
        compress(packed.data(), &packedSize, zeros.data(), zeros.size());
        packed.resize(packedSize);
        uint32_t hash = adler(zeros);
        zeros = Bytes();
        for (uint32_t i = 0; i < 2; i++)
            items.push_back(indexItem("packed" + std::to_string(i), hash + i, archive.Add(packed), 64 << 20, packed.size(), true));
        check(detect(archive, items) == "xorByChecksum", "an archive with entries that expand to 64 MiB is detected from its small ones");
        check(archive.bytesRead == smallBytes, "detection decompresses none of the 64 MiB entries (" +
                                                   std::to_string(archive.bytesRead) + " bytes read)");
    }
    {
        SyntheticArchive archive;
        std::vector<tTVPXP3Archive::tArchiveItem> items;
        for (uint32_t i = 0; i < 2; i++)
            items.push_back(indexItem("movie" + std::to_string(i), i + 1, archive.Reserve(256 << 20), 256 << 20, 256 << 20, false));
        check(detect(archive, items) == "plain", "an archive of only 256 MiB entries opens as plain");
        check(archive.bytesRead == 0, "without reading them (" + std::to_string(archive.bytesRead) + " bytes read)");
    }
}

static int runTests()
{
    std::filesystem::path dir = std::filesystem::temp_directory_path() / ("vnkk-cipher-test-" + std::to_string(getpid()));
    std::filesystem::create_directories(dir);
    std::vector<Entry> entries = sampleEntries();
    auto path = [&](const char *name) { return (dir / name).string(); };
    auto none = [](uint32_t, Bytes &) {};

    writeArchive(path("plain.xp3"), entries, none, adler);
    writeArchive(path("xor.xp3"), entries, encryptXor, adler);
    writeArchive(path("inverted.xp3"), entries, [](uint32_t hash, Bytes &data) { encryptXor(~(hash + 1), data); }, adler);
    writeArchive(path("ring.xp3"), entries, encryptRing, adler);
    writeArchive(path("banded.xp3"), entries, encryptBanded, adler);
    // Protected archives add a warning text file whose checksum is made up.
    std::vector<Entry> withDecoy = entries;
    const std::string warning = "Warning: extracting this archive is prohibited.";
    withDecoy.push_back({"warning.txt", Bytes(warning.begin(), warning.end()), false});
    writeArchive(path("decoy.xp3"), withDecoy, encryptXor, [&](const Bytes &plain) {
        return plain == withDecoy.back().plain ? 0x5eed5eedu : adler(plain);
    });
    std::vector<Entry> large;
    for (uint32_t i = 0; i < 4; i++)
        large.push_back({"large" + std::to_string(i) + ".ogg", pattern(300000 + i * 1000, i + 50), i == 0});
    writeArchive(path("large.xp3"), large, encryptXor, adler);
    writeArchive(path("placeholder.xp3"), entries, none, [](const Bytes &) { return 0x23333333u; });
    // XOR by the checksum's second byte, a scheme the table lacks.
    writeArchive(path("unknown.xp3"), entries, [](uint32_t hash, Bytes &data) { encryptXor(hash >> 8, data); }, adler);

    tTVPXP3Archive *plain = openArchive(path("plain.xp3"));
    check(plain && cipherName(plain) == "plain", "a plain archive is detected as plain");
    check(plain && readsPlain(plain, entries), "a plain archive reads back unchanged");

    // Both stay open together, as a game's archives do, and each keeps its own scheme.
    tTVPXP3Archive *xorArchive = openArchive(path("xor.xp3"));
    tTVPXP3Archive *banded = openArchive(path("banded.xp3"));
    check(xorArchive && cipherName(xorArchive) == "xorByChecksum", "an XOR archive is detected as xorByChecksum");
    check(banded && cipherName(banded) == "bandedXorAdd", "a banded archive is detected as bandedXorAdd");
    check(xorArchive && readsPlain(xorArchive, entries), "an XOR archive decrypts to its plain contents");
    check(banded && readsPlain(banded, entries), "a banded archive decrypts to its plain contents");

    tTVPXP3Archive *inverted = openArchive(path("inverted.xp3"));
    check(inverted && cipherName(inverted) == "xorByInvertedChecksum", "an inverted XOR archive is detected");
    check(inverted && readsPlain(inverted, entries), "an inverted XOR archive decrypts to its plain contents");

    tTVPXP3Archive *ring = openArchive(path("ring.xp3"));
    check(ring && cipherName(ring) == "xorByChecksumRing", "a key ring archive is detected");
    check(ring && readsPlain(ring, entries), "a key ring archive decrypts to its plain contents");

    tTVPXP3Archive *decoy = openArchive(path("decoy.xp3"));
    check(decoy && cipherName(decoy) == "xorByChecksum", "an archive with a made-up checksum on one entry is still detected");

    tTVPXP3Archive *largeArchive = openArchive(path("large.xp3"));
    check(largeArchive && cipherName(largeArchive) == "xorByChecksum", "an archive of only large entries is detected");
    check(largeArchive && readsPlain(largeArchive, large), "an archive of only large entries decrypts");

    std::vector<Entry> withLong = entries;
    withLong.push_back({"long.bin", pattern(3000, 99), false, 100});
    writeArchive(path("long.xp3"), withLong, encryptXor, adler);
    tTVPXP3Archive *longArchive = openArchive(path("long.xp3"));
    check(longArchive && cipherName(longArchive) == "xorByChecksum",
          "an archive listing a file longer than its segments is still detected");

    tTVPXP3Archive *placeholder = openArchive(path("placeholder.xp3"));
    check(placeholder && cipherName(placeholder) == "plain", "an archive with one checksum on every entry is read as plain");
    check(placeholder && readsPlain(placeholder, entries), "an archive with one checksum on every entry reads back unchanged");
    check(vnkk_problem() == nullptr, "no problem is recorded for supported archives");

    tTVPXP3Archive *unknown = openArchive(path("unknown.xp3"));
    check(unknown && cipherName(unknown) == "plain", "an unknown scheme opens without decryption");
    check(vnkk_problem() != nullptr, "an unknown scheme records a problem for the player");
    if (vnkk_problem())
        printf("problem: %s\n", vnkk_problem());

    // A game's own xp3filter.tjs installs a filter, which replaces the native cipher.
    TVPSetXP3ArchiveExtractionFilter([](tTVPXP3ExtractionFilterInfo *info, tTJSVariant *) {
        uint8_t *buffer = (uint8_t *)info->Buffer;
        for (tjs_uint i = 0; i < info->BufferSize; i++)
            buffer[i] ^= (uint8_t)info->FileHash;
    });
    check(xorArchive && readsPlain(xorArchive, entries), "an extraction filter replaces the native cipher");
    TVPSetXP3ArchiveExtractionFilter(nullptr);

    // Index fields come straight from the file, so their sizes can be anything.
    std::vector<Entry> huge;
    for (uint32_t i = 0; i < 2; i++)
        huge.push_back({"movie" + std::to_string(i) + ".mpg", pattern(8 << 20, i + 70), false});
    writeArchive(path("huge.xp3"), huge, encryptXor, adler);
    huge.clear();
    tTVPXP3Archive *hugeArchive = nullptr;
    int64_t held = peakDuring([&] { hugeArchive = openArchive(path("huge.xp3")); });
    check(hugeArchive && cipherName(hugeArchive) == "xorByChecksum", "an archive of 8 MiB entries is detected");
    check(held < 1 << 20, "detecting it holds under 1 MiB at once (" + std::to_string(held >> 10) + " KiB)");

    writeXP3(path("packed.xp3"), pattern(4096, 8),
             fileChunk("packed.bin", 100, 64 << 20, {{true, headerSize, 100, 64 << 20}}, 1));
    tTVPXP3Archive *packed = nullptr;
    held = peakDuring([&] { packed = tryOpenArchive(path("packed.xp3")); });
    check(packed && cipherName(packed) == "plain", "an archive whose compressed size runs past its end opens");
    check(held < 1 << 20, "the compressed size is not allocated (" + std::to_string(held >> 10) + " KiB held)");

    writeXP3(path("overflow.xp3"), pattern(4096, 7),
             fileChunk("overflow.bin", 1, 1, {{false, headerSize, 1, 1}, {false, headerSize, UINT64_MAX, UINT64_MAX}}, 1));
    tTVPXP3Archive *overflow = tryOpenArchive(path("overflow.xp3"));
    check(overflow && cipherName(overflow) == "plain",
          "segment sizes that overflow are rejected without writing past the sample");

    Bytes chunkSize = {'F', 'i', 'l', 'e'};
    put(chunkSize, UINT64_MAX - 11, 8);
    writeXP3(path("chunksize.xp3"), {}, chunkSize);
    signal(SIGALRM, [](int) {
        const char message[] = "FAIL: opening an archive whose chunk size overflows did not finish in 10 s\n";
        write(STDOUT_FILENO, message, sizeof message - 1);
        _exit(1);
    });
    alarm(10);
    tTVPXP3Archive *badChunk = tryOpenArchive(path("chunksize.xp3"));
    alarm(0);
    check(!badChunk, "an index chunk whose size overflows is rejected");

    checkDetectionWork();

    for (tTVPXP3Archive *archive :
         {plain, xorArchive, banded, decoy, largeArchive, longArchive, placeholder, unknown, overflow, hugeArchive, packed})
        if (archive)
            archive->Release();
    std::filesystem::remove_all(dir);
    printf("%s\n", failures ? "cipher test failed" : "cipher test passed");
    return failures ? 1 : 0;
}

static double readAll(tTVPXP3Archive *archive, uint64_t &bytes)
{
    Bytes buffer;
    bytes = 0;
    auto start = std::chrono::steady_clock::now();
    for (tjs_uint i = 0; i < archive->GetCount(); i++) {
        tTJSBinaryStream *stream = archive->CreateStreamByIndex(i);
        buffer.resize(stream->GetSize());
        stream->ReadBuffer(buffer.data(), (tjs_uint)buffer.size());
        bytes += buffer.size();
        delete stream;
    }
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

static int bench(const char *path)
{
    tTVPXP3Archive *archive = openArchive(path);
    if (!archive)
        return 1;
    const VNKKCipher *cipher = archive->Cipher;
    printf("%u entries, %s\n", archive->GetCount(), cipher ? cipher->name : "plain");
    uint64_t bytes = 0;
    readAll(archive, bytes); // warms the file cache
    for (int round = 0; round < 3; round++) {
        archive->Cipher = nullptr;
        double without = readAll(archive, bytes);
        archive->Cipher = cipher;
        double with = readAll(archive, bytes);
        printf("round %d: %.1f MB, without cipher %.0f ms, with cipher %.0f ms\n", round + 1,
               bytes / 1e6, without, with);
    }
    archive->Release();
    return 0;
}

int main(int argc, char *argv[])
{
    // Each result reaches the log even if a later check crashes.
    setvbuf(stdout, nullptr, _IOLBF, 0);
    if (argc == 3 && !strcmp(argv[1], "--bench"))
        return bench(argv[2]);
    return runTests();
}

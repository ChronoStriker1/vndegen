// KiriKiriCipherTest: checks cipher detection and decryption through the engine's archive code.
//   KiriKiriCipherTest                 writes synthetic archives to a temporary folder and checks them
//   KiriKiriCipherTest --bench <xp3>   times reading every entry of an archive with and without its cipher
#include "tjsCommHead.h"
#include "TVPStorage.h"
#include "XP3Archive.h"
#include "ciphers.h"
#include "vnkk.h"

#include <zlib.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

using Bytes = std::vector<uint8_t>;
using Encrypt = std::function<void(uint32_t hash, Bytes &data)>;

static int failures;

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
};

// Writes an XP3 archive of entries, each encrypted with encrypt and stored under hash(plain).
static void writeArchive(const std::string &path, const std::vector<Entry> &entries,
                         const Encrypt &encrypt, const std::function<uint32_t(const Bytes &)> &hash)
{
    Bytes file = {'X', 'P', '3', 0x0d, 0x0a, 0x20, 0x0a, 0x1a, 0x8b, 0x67, 0x01};
    put(file, 0, 8);
    Bytes index;
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
        uint64_t start = file.size();
        file.insert(file.end(), stored.begin(), stored.end());

        Bytes info, segm, adlr, body;
        put(info, 0, 4);
        put(info, entry.plain.size(), 8);
        put(info, stored.size(), 8);
        put(info, entry.name.size(), 2);
        for (char c : entry.name)
            put(info, (uint8_t)c, 2);
        put(segm, entry.compressed ? 1 : 0, 4);
        put(segm, start, 8);
        put(segm, entry.plain.size(), 8);
        put(segm, stored.size(), 8);
        put(adlr, checksum, 4);
        chunk(body, "info", info);
        chunk(body, "segm", segm);
        chunk(body, "adlr", adlr);
        chunk(index, "File", body);
    }
    uint64_t indexOffset = file.size();
    file.push_back(0);
    put(file, index.size(), 8);
    file.insert(file.end(), index.begin(), index.end());
    for (int i = 0; i < 8; i++)
        file[11 + i] = (uint8_t)(indexOffset >> (8 * i));
    FILE *out = fopen(path.c_str(), "wb");
    fwrite(file.data(), 1, file.size(), out);
    fclose(out);
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

static int runTests()
{
    std::filesystem::path dir = std::filesystem::temp_directory_path() / ("vnkk-cipher-test-" + std::to_string(getpid()));
    std::filesystem::create_directories(dir);
    std::vector<Entry> entries = sampleEntries();
    auto path = [&](const char *name) { return (dir / name).string(); };
    auto none = [](uint32_t, Bytes &) {};

    writeArchive(path("plain.xp3"), entries, none, adler);
    writeArchive(path("xor.xp3"), entries, encryptXor, adler);
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

    tTVPXP3Archive *decoy = openArchive(path("decoy.xp3"));
    check(decoy && cipherName(decoy) == "xorByChecksum", "an archive with a made-up checksum on one entry is still detected");

    tTVPXP3Archive *largeArchive = openArchive(path("large.xp3"));
    check(largeArchive && cipherName(largeArchive) == "xorByChecksum", "an archive of only large entries is detected");
    check(largeArchive && readsPlain(largeArchive, large), "an archive of only large entries decrypts");

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

    for (tTVPXP3Archive *archive : {plain, xorArchive, banded, decoy, largeArchive, placeholder, unknown})
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
    if (argc == 3 && !strcmp(argv[1], "--bench"))
        return bench(argv[2]);
    return runTests();
}

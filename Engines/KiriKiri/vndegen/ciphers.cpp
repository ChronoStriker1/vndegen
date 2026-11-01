#include "tjsCommHead.h"
#include "ciphers.h"
#include "TVPMsg.h"

#include <SDL3/SDL_log.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <string>

// k is the low byte of the file's adler32 in every scheme seen so far.

static void xorByChecksum(uint32_t hash, uint64_t, uint8_t *buffer, size_t size)
{
    const uint8_t k = hash;
    for (size_t i = 0; i < size; i++)
        buffer[i] ^= k;
}

// The cipher of a 61 KB .tpm plugin such as xp3dec.tpm: Marshmallow Imouto Succubus, Agent Shion, Kagachi-sama.
static void xorByInvertedChecksum(uint32_t hash, uint64_t, uint8_t *buffer, size_t size)
{
    const uint8_t k = ~(hash + 1);
    for (size_t i = 0; i < size; i++)
        buffer[i] ^= k;
}

// The decoder.tpm plugin's cipher, in four bands by position within the file. Each band is its own
// loop, so the compiler can vectorize the last one, which holds nearly every byte.
static void bandedXorAdd(uint32_t hash, uint64_t offset, uint8_t *buffer, size_t size)
{
    const uint8_t k = hash;
    const uint64_t end = offset + size;
    auto band = [&](uint64_t from, uint64_t to, auto apply) {
        from = std::max(from, offset);
        to = std::min(to, end);
        for (uint64_t p = from; p < to; p++)
            apply(buffer[p - offset]);
    };
    band(0, 0x7b, [x = uint8_t(k * 0x15)](uint8_t &b) { b ^= x; });
    band(0x7b, 0xf6, [a = uint8_t(k * 0xe0)](uint8_t &b) { b += a; });
    band(0xf6, 0x171, [x = uint8_t(k * 0x2b)](uint8_t &b) { b ^= x; });
    band(0x171, UINT64_MAX, [a = uint8_t(k * 0xca)](uint8_t &b) { b += a; });
}

// Compiled into the .exe of Cosplaying in Costalia: a 32-byte key made from the checksum and repeated through the file.
static void xorByChecksumRing(uint32_t hash, uint64_t offset, uint8_t *buffer, size_t size)
{
    uint32_t x = (hash ^ 0x2f91de55) & 0x7fffffff;
    x |= x << 31;
    uint8_t key[32];
    for (uint8_t &k : key) {
        k = x;
        x = x >> 8 | (x & ~1u) << 23;
    }
    for (size_t i = 0; i < size; i++)
        buffer[i] ^= key[(offset + i) & 31];
}

static const VNKKCipher ciphers[] = {
    {"xorByChecksum", xorByChecksum},
    {"bandedXorAdd", bandedXorAdd},
    {"xorByInvertedChecksum", xorByInvertedChecksum},
    {"xorByChecksumRing", xorByChecksumRing},
};

static const size_t maxSamples = 8;
// Reading stops early past this many bytes, so an archive of large files opens fast.
static const uint64_t sampleBudget = 4 * 1024 * 1024;
// Detection reads and decompresses at most this many bytes per archive. An entry that does not fit
// in what is left is skipped before any of it is read.
static const uint64_t workLimit = 32 * 1024 * 1024;
// Samples at least this long cover every band of bandedXorAdd, so the schemes cannot agree on them.
static const uint64_t preferredSampleSize = 0x200;
// Samples are read, decompressed, and checksummed this many bytes at a time, so detection holds a
// few of these however large the entries are.
static const size_t pieceSize = 64 * 1024;

struct Sample {
    uint32_t hash;
    // The adler32 of the entry's contents without decryption, then decrypted with each scheme.
    std::array<uint32_t, 1 + std::size(ciphers)> checksums;
};

// Runs inflate over a compressed segment, handing each decompressed piece to add.
template <typename Add>
static void inflateSegment(tTJSBinaryStream *st, const tTVPXP3ArchiveSegment &segment, Add add)
{
    z_stream z{};
    if (inflateInit(&z) != Z_OK)
        TVPThrowExceptionMessage(TJS_N("vnkk: could not decompress a sample"));
    std::unique_ptr<z_stream, int (*)(z_stream *)> end(&z, inflateEnd);
    std::vector<uint8_t> packed(pieceSize), plain(pieceSize);
    uint64_t packedLeft = segment.ArcSize, plainLeft = segment.OrgSize;
    for (int result = Z_OK; result != Z_STREAM_END;) {
        if (z.avail_in == 0 && packedLeft > 0) {
            size_t size = std::min<uint64_t>(packedLeft, pieceSize);
            st->ReadBuffer(packed.data(), (tjs_uint)size);
            packedLeft -= size;
            z.next_in = packed.data();
            z.avail_in = (uInt)size;
        }
        z.next_out = plain.data();
        z.avail_out = (uInt)pieceSize;
        // Z_BUF_ERROR here means the compressed data ended early.
        result = inflate(&z, Z_NO_FLUSH);
        size_t produced = pieceSize - z.avail_out;
        if ((result != Z_OK && result != Z_STREAM_END) || produced > plainLeft)
            TVPThrowExceptionMessage(TJS_N("vnkk: could not decompress a sample"));
        add(plain.data(), produced);
        plainLeft -= produced;
    }
    if (plainLeft)
        TVPThrowExceptionMessage(TJS_N("vnkk: could not decompress a sample"));
}

static Sample readSample(const tTVPXP3Archive::tArchiveItem &item, tTJSBinaryStream *st)
{
    Sample sample{item.FileHash, {}};
    sample.checksums.fill(adler32(0, nullptr, 0));
    uint64_t position = 0;
    std::vector<uint8_t> decrypted(pieceSize);
    auto add = [&](const uint8_t *data, size_t size) {
        sample.checksums[0] = adler32(sample.checksums[0], data, (uInt)size);
        for (size_t i = 0; i < std::size(ciphers); i++) {
            std::copy(data, data + size, decrypted.begin());
            ciphers[i].decrypt(item.FileHash, position, decrypted.data(), size);
            sample.checksums[i + 1] = adler32(sample.checksums[i + 1], decrypted.data(), (uInt)size);
        }
        position += size;
    };

    const uint64_t archiveSize = st->GetSize();
    std::vector<uint8_t> piece(pieceSize);
    for (const tTVPXP3ArchiveSegment &segment : item.Segments) {
        uint64_t stored = segment.IsCompressed ? segment.ArcSize : segment.OrgSize;
        if (segment.OrgSize > item.OrgSize - position || segment.Start > archiveSize ||
            stored > archiveSize - segment.Start)
            TVPThrowExceptionMessage(TJS_N("vnkk: segment outside its file or archive"));
        st->SetPosition(segment.Start);
        if (segment.IsCompressed) {
            inflateSegment(st, segment, add);
            continue;
        }
        for (uint64_t left = stored; left > 0;) {
            size_t size = std::min<uint64_t>(left, pieceSize);
            st->ReadBuffer(piece.data(), (tjs_uint)size);
            add(piece.data(), size);
            left -= size;
        }
    }
    // Some archives list files longer than their segments. The engine's reads end with the last
    // segment, so the sample does too.
    return sample;
}

// Takes the work of sampling item from budget: the bytes its segments store, plus the bytes the
// compressed ones expand to. Sampling reads no more than that, since reads stop at each segment's
// stored size and decompression at its original size. Returns false, taking nothing, when item
// does not fit.
static bool chargeSample(const tTVPXP3Archive::tArchiveItem &item, uint64_t &budget)
{
    uint64_t left = budget;
    for (const tTVPXP3ArchiveSegment &segment : item.Segments) {
        uint64_t stored = segment.IsCompressed ? segment.ArcSize : segment.OrgSize;
        uint64_t expanded = segment.IsCompressed ? segment.OrgSize : 0;
        if (stored > left || expanded > left - stored)
            return false;
        left -= stored + expanded;
    }
    budget = left;
    return true;
}

// Index 0 counts the samples that verify without decryption, then one per scheme.
static size_t countVerified(const std::vector<Sample> &samples, size_t index)
{
    size_t verified = 0;
    for (const Sample &sample : samples)
        verified += sample.checksums[index] == sample.hash;
    return verified;
}

const VNKKCipher *VNKKDetectCipher(const ttstr &archiveName,
                                   const std::vector<tTVPXP3Archive::tArchiveItem> &items,
                                   tTJSBinaryStream *st)
{
    std::string name = TVPExtractStorageName(archiveName).AsStdString();
    try {
        // The smallest entries with distinct checksums, preferring ones that cover every band.
        std::vector<const tTVPXP3Archive::tArchiveItem *> candidates;
        for (const tTVPXP3Archive::tArchiveItem &item : items)
            if (item.OrgSize > 0 && !item.Segments.empty())
                candidates.push_back(&item);
        std::sort(candidates.begin(), candidates.end(), [](auto a, auto b) {
            bool aShort = a->OrgSize < preferredSampleSize, bShort = b->OrgSize < preferredSampleSize;
            return aShort != bShort ? bShort : a->OrgSize < b->OrgSize;
        });
        std::vector<Sample> samples;
        uint64_t sampledBytes = 0, budget = workLimit;
        for (const tTVPXP3Archive::tArchiveItem *item : candidates) {
            if (samples.size() == maxSamples || (samples.size() >= 2 && sampledBytes >= sampleBudget))
                break;
            bool seen = false;
            for (const Sample &sample : samples)
                seen = seen || sample.hash == item->FileHash;
            if (seen || !chargeSample(*item, budget))
                continue;
            samples.push_back(readSample(*item, st));
            sampledBytes += item->OrgSize;
        }
        if (samples.empty()) {
            if (!candidates.empty())
                SDL_Log("vnkk: the entries of %s are too large to check for encryption", name.c_str());
            return nullptr;
        }
        // Some archives hold a warning text file under a made-up checksum, so a scheme needs only
        // half of the samples. Plain data wins a tie.
        const VNKKCipher *best = nullptr;
        size_t bestCount = countVerified(samples, 0);
        for (size_t i = 0; i < std::size(ciphers); i++) {
            size_t count = countVerified(samples, i + 1);
            if (count > bestCount)
                best = &ciphers[i], bestCount = count;
        }
        if (bestCount * 2 >= samples.size()) {
            if (best)
                SDL_Log("vnkk: %s is encrypted with %s", name.c_str(), best->name);
            return best;
        }
        bool sameChecksum = true;
        for (const tTVPXP3Archive::tArchiveItem &item : items)
            sameChecksum = sameChecksum && item.FileHash == items.front().FileHash;
        // Such archives store a placeholder in place of real checksums, and their data is plain.
        if (sameChecksum && items.size() > 1)
            return nullptr;
    } catch (...) {
        SDL_Log("vnkk: could not check %s for encryption", name.c_str());
        return nullptr;
    }
    SDL_Log("vnkk: unknown encryption in %s", name.c_str());
    vnkk_record_problem("This game's archives use an encryption VNDegen does not support yet.");
    return nullptr;
}

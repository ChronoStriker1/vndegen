#include "tjsCommHead.h"
#include "ciphers.h"
#include "TVPMsg.h"

#include <SDL3/SDL_log.h>
#include <zlib.h>

#include <algorithm>
#include <string>

// k is the low byte of the file's adler32 in every scheme seen so far.

static void xorByChecksum(uint32_t hash, uint64_t, uint8_t *buffer, size_t size)
{
    const uint8_t k = hash;
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

static const VNKKCipher ciphers[] = {
    {"xorByChecksum", xorByChecksum},
    {"bandedXorAdd", bandedXorAdd},
};

static const size_t maxSamples = 8;
// Reading stops early past this many bytes, so an archive of large files opens fast.
static const uint64_t sampleBudget = 4 * 1024 * 1024;
// Samples at least this long cover every band of bandedXorAdd, so the schemes cannot agree on them.
static const uint64_t preferredSampleSize = 0x200;

struct Sample {
    uint32_t hash;
    std::vector<uint8_t> data; // decompressed, still encrypted
};

static Sample readSample(const tTVPXP3Archive::tArchiveItem &item, tTJSBinaryStream *st)
{
    Sample sample{item.FileHash, std::vector<uint8_t>(item.OrgSize)};
    std::vector<uint8_t> packed;
    for (const tTVPXP3ArchiveSegment &segment : item.Segments) {
        if (segment.Offset + segment.OrgSize > item.OrgSize)
            TVPThrowExceptionMessage(TJS_N("vnkk: segment outside its file"));
        uint8_t *out = sample.data.data() + segment.Offset;
        st->SetPosition(segment.Start);
        if (!segment.IsCompressed) {
            st->ReadBuffer(out, (tjs_uint)segment.OrgSize);
            continue;
        }
        packed.resize(segment.ArcSize);
        st->ReadBuffer(packed.data(), (tjs_uint)packed.size());
        uLongf length = segment.OrgSize;
        if (uncompress(out, &length, packed.data(), packed.size()) != Z_OK || length != segment.OrgSize)
            TVPThrowExceptionMessage(TJS_N("vnkk: could not decompress a sample"));
    }
    return sample;
}

static size_t countVerified(const std::vector<Sample> &samples, const VNKKCipher *cipher)
{
    size_t verified = 0;
    std::vector<uint8_t> plain;
    for (const Sample &sample : samples) {
        plain = sample.data;
        if (cipher)
            cipher->decrypt(sample.hash, 0, plain.data(), plain.size());
        verified += adler32(adler32(0, nullptr, 0), plain.data(), (uInt)plain.size()) == sample.hash;
    }
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
        uint64_t sampledBytes = 0;
        for (const tTVPXP3Archive::tArchiveItem *item : candidates) {
            if (samples.size() == maxSamples || (samples.size() >= 2 && sampledBytes >= sampleBudget))
                break;
            bool seen = false;
            for (const Sample &sample : samples)
                seen = seen || sample.hash == item->FileHash;
            if (seen)
                continue;
            samples.push_back(readSample(*item, st));
            sampledBytes += item->OrgSize;
        }
        if (samples.empty())
            return nullptr;
        // Some archives hold a warning text file under a made-up checksum, so a scheme needs only
        // half of the samples. Plain data wins a tie.
        const VNKKCipher *best = nullptr;
        size_t bestCount = countVerified(samples, nullptr);
        for (const VNKKCipher &cipher : ciphers) {
            size_t count = countVerified(samples, &cipher);
            if (count > bestCount)
                best = &cipher, bestCount = count;
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

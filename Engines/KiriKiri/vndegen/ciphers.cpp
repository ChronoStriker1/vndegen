#include "tjsCommHead.h"
#include "ciphers.h"
#include "TVPMsg.h"

#include <SDL3/SDL_log.h>
#include <zlib.h>

#include <string>

// k is the low byte of the file's adler32 in every scheme seen so far.

static void xorByChecksum(uint32_t hash, uint64_t, uint8_t *buffer, size_t size)
{
    const uint8_t k = hash;
    for (size_t i = 0; i < size; i++)
        buffer[i] ^= k;
}

// The decoder.tpm plugin's cipher, in four bands by position within the file.
static void bandedXorAdd(uint32_t hash, uint64_t offset, uint8_t *buffer, size_t size)
{
    const uint8_t k = hash;
    const uint8_t xor1 = k * 0x15, add2 = k * 0xe0, xor3 = k * 0x2b, add4 = k * 0xca;
    for (size_t i = 0; i < size; i++) {
        const uint64_t p = offset + i;
        if (p < 0x7b)
            buffer[i] ^= xor1;
        else if (p < 0xf6)
            buffer[i] += add2;
        else if (p < 0x171)
            buffer[i] ^= xor3;
        else
            buffer[i] += add4;
    }
}

static const VNKKCipher ciphers[] = {
    {"xorByChecksum", xorByChecksum},
    {"bandedXorAdd", bandedXorAdd},
};

static const size_t maxSamples = 8;
static const uint64_t maxSampleSize = 64 * 1024;
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

static bool verifies(const std::vector<Sample> &samples, const VNKKCipher *cipher)
{
    std::vector<uint8_t> plain;
    for (const Sample &sample : samples) {
        plain = sample.data;
        if (cipher)
            cipher->decrypt(sample.hash, 0, plain.data(), plain.size());
        if (adler32(adler32(0, nullptr, 0), plain.data(), (uInt)plain.size()) != sample.hash)
            return false;
    }
    return true;
}

const VNKKCipher *VNKKDetectCipher(const ttstr &archiveName,
                                   const std::vector<tTVPXP3Archive::tArchiveItem> &items,
                                   tTJSBinaryStream *st)
{
    std::string name = TVPExtractStorageName(archiveName).AsStdString();
    try {
        std::vector<Sample> samples;
        auto take = [&](uint64_t minSize) {
            for (const tTVPXP3Archive::tArchiveItem &item : items) {
                if (samples.size() == maxSamples)
                    return;
                if (item.OrgSize < minSize || item.OrgSize > maxSampleSize || item.Segments.empty())
                    continue;
                bool seen = false;
                for (const Sample &sample : samples)
                    seen = seen || sample.hash == item.FileHash;
                if (!seen)
                    samples.push_back(readSample(item, st));
            }
        };
        take(preferredSampleSize);
        take(1);
        if (samples.empty() || verifies(samples, nullptr))
            return nullptr;
        for (const VNKKCipher &cipher : ciphers) {
            if (verifies(samples, &cipher)) {
                SDL_Log("vnkk: %s is encrypted with %s", name.c_str(), cipher.name);
                return &cipher;
            }
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

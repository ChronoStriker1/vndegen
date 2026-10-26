// Native decryption of encrypted XP3 archives. Each archive's scheme is detected when it opens,
// from the adler32 checksums its index stores for the plain contents of every entry.
#ifndef VNKK_CIPHERS_H
#define VNKK_CIPHERS_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "XP3Archive.h"

struct VNKKCipher {
    const char *name;
    // Decrypts size bytes in place. offset is the position of buffer[0] within the file, and hash
    // is the file's adler32 from the archive index.
    void (*decrypt)(uint32_t hash, uint64_t offset, uint8_t *buffer, size_t size);
};

// Returns the scheme the archive's entries are encrypted with, or NULL when they are plain or use a
// scheme the table lacks. An unknown scheme is logged and recorded as a problem. st reads the
// archive file and items are its index.
const VNKKCipher *VNKKDetectCipher(const ttstr &archiveName,
                                   const std::vector<tTVPXP3Archive::tArchiveItem> &items,
                                   tTJSBinaryStream *st);

// Records a problem for vnkk_problem. Defined in vnkk.mm. sentence must be a string literal.
void vnkk_record_problem(const char *sentence);

#endif

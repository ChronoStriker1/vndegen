#include "archive.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *name;   // as stored
    char *folded; // for lookups
    uint64_t offset;
    uint32_t size;
    uint32_t key; // the key the entry's data starts with
} Entry;

struct RGSSArchive {
    FILE *file;
    Entry *entries;
    size_t count;
    // Open addressing on the folded names, sized to a power of two at least twice the count.
    long *slots;
    size_t slotMask;
};

void rgss_fold_path(const char *path, char *out, size_t len) {
    size_t i = 0;
    for (; path[i] && i + 1 < len; i++) {
        char c = path[i];
        if (c == '\\') c = '/';
        else if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        out[i] = c;
    }
    out[i] = 0;
}

static uint64_t hash_string(const char *s) {
    uint64_t h = 1469598103934665603ull;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 1099511628211ull;
    return h;
}

static uint32_t read_u32(FILE *file, bool *ok) {
    uint8_t b[4];
    if (fread(b, 1, 4, file) != 4) {
        *ok = false;
        return 0;
    }
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

static bool add_entry(RGSSArchive *archive, size_t *capacity, Entry entry) {
    if (archive->count == *capacity) {
        *capacity = *capacity ? *capacity * 2 : 256;
        Entry *grown = realloc(archive->entries, *capacity * sizeof(Entry));
        if (!grown) return false;
        archive->entries = grown;
    }
    archive->entries[archive->count++] = entry;
    return true;
}

// Version 1 (XP and VX): the key starts at 0xDEADCAFE and steps as key * 7 + 3 after every field it decrypts, the
// name one byte per step. Each entry's data follows its header and starts with the key the header ended on.
static bool read_index_v1(RGSSArchive *archive, long fileSize) {
    uint32_t key = 0xDEADCAFEu;
    size_t capacity = 0;
    bool ok = true;
    while (ftell(archive->file) < fileSize) {
        uint32_t nameLength = read_u32(archive->file, &ok) ^ key;
        key = key * 7 + 3;
        if (!ok || nameLength == 0 || nameLength > 4096) return false;
        char *name = malloc(nameLength + 1);
        if (!name || fread(name, 1, nameLength, archive->file) != nameLength) {
            free(name);
            return false;
        }
        for (uint32_t i = 0; i < nameLength; i++) {
            name[i] = (char)((uint8_t)name[i] ^ (key & 0xFF));
            key = key * 7 + 3;
        }
        name[nameLength] = 0;
        uint32_t size = read_u32(archive->file, &ok) ^ key;
        key = key * 7 + 3;
        long offset = ftell(archive->file);
        if (!ok || offset + (long)size > fileSize) {
            free(name);
            return false;
        }
        Entry entry = {.name = name, .offset = (uint64_t)offset, .size = size, .key = key};
        if (!add_entry(archive, &capacity, entry) || fseek(archive->file, (long)size, SEEK_CUR) != 0) {
            free(name);
            return false;
        }
    }
    return true;
}

// Version 3 (VX Ace): a seed after the header gives the index key, seed * 9 + 3. Each index record is the offset,
// size, data key and name length XORed with that key, then the name XORed with the key's bytes in turn. An offset of
// zero ends the index.
static bool read_index_v3(RGSSArchive *archive, long fileSize) {
    bool ok = true;
    uint32_t key = read_u32(archive->file, &ok) * 9 + 3;
    size_t capacity = 0;
    while (ok) {
        uint32_t offset = read_u32(archive->file, &ok) ^ key;
        uint32_t size = read_u32(archive->file, &ok) ^ key;
        uint32_t dataKey = read_u32(archive->file, &ok) ^ key;
        uint32_t nameLength = read_u32(archive->file, &ok) ^ key;
        if (!ok) return false;
        if (offset == 0) return true;
        if (nameLength == 0 || nameLength > 4096 || (long)offset + (long)size > fileSize) return false;
        char *name = malloc(nameLength + 1);
        if (!name || fread(name, 1, nameLength, archive->file) != nameLength) {
            free(name);
            return false;
        }
        for (uint32_t i = 0; i < nameLength; i++) name[i] = (char)((uint8_t)name[i] ^ (uint8_t)(key >> (8 * (i % 4))));
        name[nameLength] = 0;
        Entry entry = {.name = name, .offset = offset, .size = size, .key = dataKey};
        if (!add_entry(archive, &capacity, entry)) {
            free(name);
            return false;
        }
    }
    return false;
}

static bool build_lookup(RGSSArchive *archive) {
    size_t slots = 16;
    while (slots < archive->count * 2) slots *= 2;
    archive->slots = malloc(slots * sizeof(long));
    if (!archive->slots) return false;
    for (size_t i = 0; i < slots; i++) archive->slots[i] = -1;
    archive->slotMask = slots - 1;
    for (size_t i = 0; i < archive->count; i++) {
        Entry *entry = &archive->entries[i];
        size_t length = strlen(entry->name) + 1;
        entry->folded = malloc(length);
        if (!entry->folded) return false;
        rgss_fold_path(entry->name, entry->folded, length);
        size_t slot = hash_string(entry->folded) & archive->slotMask;
        while (archive->slots[slot] >= 0) slot = (slot + 1) & archive->slotMask;
        archive->slots[slot] = (long)i;
    }
    return true;
}

RGSSArchive *rgss_archive_open(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    uint8_t header[8];
    if (fread(header, 1, 8, file) != 8 || memcmp(header, "RGSSAD\0", 7) != 0 || (header[7] != 1 && header[7] != 3)) {
        fclose(file);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 8, SEEK_SET);
    RGSSArchive *archive = calloc(1, sizeof(RGSSArchive));
    if (!archive) {
        fclose(file);
        return NULL;
    }
    archive->file = file;
    bool ok = header[7] == 1 ? read_index_v1(archive, fileSize) : read_index_v3(archive, fileSize);
    if (!ok || !build_lookup(archive)) {
        rgss_archive_close(archive);
        return NULL;
    }
    return archive;
}

void rgss_archive_close(RGSSArchive *archive) {
    if (!archive) return;
    for (size_t i = 0; i < archive->count; i++) {
        free(archive->entries[i].name);
        free(archive->entries[i].folded);
    }
    free(archive->entries);
    free(archive->slots);
    if (archive->file) fclose(archive->file);
    free(archive);
}

long rgss_archive_find(const RGSSArchive *archive, const char *path) {
    char folded[1024];
    rgss_fold_path(path, folded, sizeof folded);
    size_t slot = hash_string(folded) & archive->slotMask;
    for (long index; (index = archive->slots[slot]) >= 0; slot = (slot + 1) & archive->slotMask) {
        if (strcmp(archive->entries[index].folded, folded) == 0) return index;
    }
    return -1;
}

// Both versions decrypt data four bytes at a time with the entry's key, stepping it as key * 7 + 3 after each word.
bool rgss_archive_read(RGSSArchive *archive, long index, uint8_t **data, size_t *size) {
    const Entry *entry = &archive->entries[index];
    uint8_t *buffer = malloc(entry->size ? entry->size : 1);
    if (!buffer) return false;
    if (fseek(archive->file, (long)entry->offset, SEEK_SET) != 0 ||
        fread(buffer, 1, entry->size, archive->file) != entry->size) {
        free(buffer);
        return false;
    }
    uint32_t key = entry->key;
    for (uint32_t i = 0; i < entry->size; i += 4) {
        for (uint32_t j = 0; j < 4 && i + j < entry->size; j++) buffer[i + j] ^= (uint8_t)(key >> (8 * j));
        key = key * 7 + 3;
    }
    *data = buffer;
    *size = entry->size;
    return true;
}

size_t rgss_archive_count(const RGSSArchive *archive) { return archive->count; }
const char *rgss_archive_name(const RGSSArchive *archive, long entry) { return archive->entries[entry].name; }
size_t rgss_archive_size(const RGSSArchive *archive, long entry) { return archive->entries[entry].size; }

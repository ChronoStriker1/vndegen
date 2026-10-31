// The .ex data files later System 4 games (Test Game 56) keep their tables and settings in, which the MainEXFile
// library reads. docs/system4-cleanroom.md has where the layout came from.
//
// The file: "HEAD", u32 12, "EXTF", u32 1, u32 entry count, then "DATA", u32 compressed size, u32 plain size and the
// compressed bytes. Each compressed byte is rotated by its count of one bits (left when the count is even, right
// when odd, by 8 minus the count), and the result is zlib data. The plain data is the entries, each a u32 type, a
// u32 size and the value.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace s4 {

struct ExValue;

// A table's column: its type, name (CP932) and, for a column of tables, the nested columns.
struct ExColumn {
    int type = 0;
    std::string name;
    bool key = false;
    std::vector<ExColumn> sub;
};

struct ExTable {
    const std::vector<ExColumn> *columns = nullptr;  // the header (owned by the top-level value or a parent column)
    std::vector<std::vector<std::unique_ptr<ExValue>>> rows;
};

struct ExNode {
    std::string name;
    std::unique_ptr<ExValue> value;  // may be null
    std::vector<std::unique_ptr<ExNode>> children;
};

struct ExValue {
    enum Type { Int = 1, Float = 2, String = 3, Table = 4, List = 5, Tree = 6 };
    int type = 0;
    std::string name;  // for entries; empty for cells and list items
    int32_t i = 0;
    float f = 0;
    std::string s;
    std::vector<ExColumn> columns;  // a top-level table's header
    ExTable table;
    std::vector<std::unique_ptr<ExValue>> list;
    std::unique_ptr<ExNode> tree;
};

class ExFile {
public:
    // Reads and decodes the file. False, with a reason, when it isn't a readable .ex file.
    bool load(const std::string &path, std::string &error);
    const ExValue *find(const std::string &name) const;
    const std::vector<std::unique_ptr<ExValue>> &entries() const { return entries_; }

private:
    std::vector<std::unique_ptr<ExValue>> entries_;
    std::map<std::string, const ExValue *> byName_;
};

// Decodes the DATA chunk's bytes in place (the bit-count rotation). Exposed for tests.
void exUnscramble(uint8_t *data, size_t n);

}  // namespace s4

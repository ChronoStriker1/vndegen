#include "script.h"

#include <cstring>

namespace neko {
namespace {

const char kMagic[] = "NEKOSDK_ADVSCRIPT2";  // with its NUL

uint32_t rd32(const uint8_t *p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

}  // namespace

bool parseScript(const uint8_t *data, size_t size, Script &out, size_t *trailing) {
    out.nodes.clear();
    out.byId.clear();
    if (size < sizeof kMagic + 4 || std::memcmp(data, kMagic, sizeof kMagic)) return false;
    size_t pos = sizeof kMagic;
    uint32_t count = rd32(data + pos);
    pos += 4;
    // A node's fixed part: id, colour tag, editor position, opcode, 32 parameters, next, then 64 bytes whose first int is
    // a branch's other target.
    constexpr size_t kFixed = 16 + 128 + 4 + 64;
    if (count > size / kFixed) return false;
    out.nodes.resize(count);
    for (uint32_t n = 0; n < count; n++) {
        if (pos + kFixed > size) return false;
        Node &node = out.nodes[n];
        node.id = int(rd32(data + pos));
        node.opcode = int(rd32(data + pos + 12));
        std::memcpy(node.p.data(), data + pos + 16, 128);
        node.next = int(rd32(data + pos + 144));
        node.otherwise = int(rd32(data + pos + 148));
        pos += kFixed;
        for (auto &text : node.s) {
            if (pos + 4 > size) return false;
            size_t length = rd32(data + pos);
            pos += 4;
            if (length > size - pos) return false;
            size_t end = pos + length;
            // A fan translation of Test Game 94 breaks these lengths both ways: some are one byte short, and some text
            // has NULs inside. Trust a length unless its span doesn't end in a NUL, then read on to the next NUL.
            if (length && data[end - 1] != 0) {
                auto nul = static_cast<const uint8_t *>(std::memchr(data + end, 0, size - end));
                if (!nul) return false;
                end = size_t(nul - data) + 1;
            }
            text.reserve(end - pos);
            for (size_t i = pos; i < end; i++)
                if (data[i]) text.push_back(char(data[i]));
            pos = end;
        }
        out.byId[node.id] = int(n);
    }
    if (trailing) *trailing = size - pos;
    return true;
}

}  // namespace neko

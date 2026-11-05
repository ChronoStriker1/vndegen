// NEKOSDK_ADVSCRIPT2 scripts: scene, flow and text-group files, each a list of nodes run by following `next`.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace neko {

struct Node {
    int id = 0;
    int opcode = 0;
    int next = 0;
    std::array<int32_t, 32> p{};
    std::array<std::string, 33> s;  // CP932, without NULs
};

struct Script {
    std::string name;
    std::vector<Node> nodes;
    std::unordered_map<int, int> byId;
    const Node *node(int id) const {
        auto it = byId.find(id);
        return it == byId.end() ? nullptr : &nodes[size_t(it->second)];
    }
    const Node *first() const { return nodes.empty() ? nullptr : &nodes[0]; }
};

// False when the bytes aren't a whole script. `trailing` receives how many bytes follow the last node.
bool parseScript(const uint8_t *data, size_t size, Script &out, size_t *trailing = nullptr);

}  // namespace neko

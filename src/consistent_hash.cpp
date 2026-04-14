#include "consistent_hash.h"
#include <sstream>
#include <cstdint>
#include <stdexcept>

uint32_t ConsistentHash::fnv1a(const std::string& s) const {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

ConsistentHash::ConsistentHash(int vnodes) : vnodes_(vnodes) {}

std::string ConsistentHash::lookup(const std::string& key) const {
    if (ring_.empty()) return "";

    uint32_t h  = fnv1a(key) % 1000;

    auto it = ring_.lower_bound(h);

    if (it == ring_.end()) it = ring_.begin();

    return it->second;
}

std::string ConsistentHash::assignKey(const std::string& key) {
    std::string node = lookup(key);
    if (!node.empty() && nodes_.count(node))
        nodes_[node].keys.insert(key);
    return node;
}

void ConsistentHash::rebalance() {
    std::vector<std::string> allKeys;
    for (auto& [name, info] : nodes_) {
        for (const auto& k : info.keys) allKeys.push_back(k);
        info.keys.clear();
    }
    for (const auto& k : allKeys) assignKey(k);
}
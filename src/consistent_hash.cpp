#include "consistent_hash.h"
#include <sstream>
#include <cstdint>
#include <stdexcept>
using namespace std;


uint32_t ConsistentHash::fnv1a(const std::string& s) const {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

//  Constructor 

ConsistentHash::ConsistentHash(int vnodes) : vnodes_(vnodes) {}

//  addNode 

void ConsistentHash::addNode(const std::string& name, const std::string& color) {
    if (nodes_.count(name))
        throw std::invalid_argument("Node already exists: " + name);

    nodes_[name] = NodeInfo{ name, color, {} };

    for (int i = 0; i < vnodes_; ++i) {
        std::string vkey = name + "#vn" + std::to_string(i);
        uint32_t    pos  = fnv1a(vkey) % 1000;
        ring_[pos]       = name;             
    }

    rebalance();
}

//  removeNode 

void ConsistentHash::removeNode(const std::string& name) {
    if (!nodes_.count(name))
        throw std::invalid_argument("Node not found: " + name);

    // Collect keys owned by this node before removing it
    std::vector<std::string> orphans(
        nodes_.at(name).keys.begin(),
        nodes_.at(name).keys.end()
    );

    nodes_.erase(name);

    // Remove all vnode positions belonging to this node
    for (auto it = ring_.begin(); it != ring_.end(); ) {
        if (it->second == name) it = ring_.erase(it);
        else                    ++it;
    }

    // Reassign orphaned keys to their new responsible nodes
    for (const auto& key : orphans)
        assignKey(key);
}

//  lookup 

std::string ConsistentHash::lookup(const std::string& key) const {
    if (ring_.empty()) return "";

    uint32_t h  = fnv1a(key) % 1000;

    auto it = ring_.lower_bound(h);

    if (it == ring_.end()) it = ring_.begin();

    return it->second;
}

//  assignKey 

std::string ConsistentHash::assignKey(const std::string& key) {
    std::string node = lookup(key);
    if (!node.empty() && nodes_.count(node))
        nodes_[node].keys.insert(key);
    return node;
}

void ConsistentHash::setVNodes(int n) {
    if (n < 1) return;
    vnodes_ = n;
    
    ring_.clear();

    for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
        const std::string& name = it->first;
        for (int i = 0; i < vnodes_; ++i) {
            uint32_t pos = fnv1a(name + "#vn" + std::to_string(i));
            ring_[pos] = name;
        }
    }
    rebalance();
}

//  rebalance 

void ConsistentHash::rebalance() {
    std::vector<std::string> allKeys;
    for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
        for (auto kit = it->second.keys.begin(); kit != it->second.keys.end(); ++kit)
            allKeys.push_back(*kit);
        it->second.keys.clear();
    }
    for (size_t i = 0; i < allKeys.size(); ++i) assignKey(allKeys[i]);
}
